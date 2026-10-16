// SPDX-License-Identifier: GPL-3.0-or-later
#include "playback/MpvCore.h"

#ifdef HOPTUNES_HAVE_MPV

#include <QMetaObject>
#include <QUrl>
#include <QVariantMap>

#include <clocale>

#include <mpv/client.h>

namespace hop {

namespace {

// libmpv requires the process's LC_NUMERIC category to be "C": mpv_create() parses its
// own option values (e.g. "1.0") assuming '.' as the decimal separator, and silently
// returns null if the active locale could break that (see mpv/client.h's own docs on
// this). Qt/GLib/GStreamer startup code commonly calls setlocale(LC_ALL, "") — pulling in
// whatever locale the desktop is configured with — well before any MpvCore exists, so this
// must be reasserted right before every mpv_create() call here, not just once at startup.
// Reproduced on a real desktop locale (Steam Deck running CachyOS Handheld Edition); CI's
// headless runners default to the C locale already, which is why this never showed up
// there.
void ensureMpvLocale() { std::setlocale(LC_NUMERIC, "C"); }

QString nodeString(const mpv_node &n)
{
    return n.format == MPV_FORMAT_STRING && n.u.string ? QString::fromUtf8(n.u.string) : QString();
}

// Reads one entry of mpv's "track-list" (MPV_FORMAT_NODE_MAP) into a QVariantMap.
QVariantMap trackMap(const mpv_node &entry)
{
    QVariantMap m;
    if (entry.format != MPV_FORMAT_NODE_MAP || !entry.u.list) return m;
    const mpv_node_list *list = entry.u.list;
    for (int i = 0; i < list->num; ++i) {
        const QString key = QString::fromUtf8(list->keys[i]);
        const mpv_node &v = list->values[i];
        if (key == QLatin1String("id") && v.format == MPV_FORMAT_INT64) m["id"] = qint64(v.u.int64);
        else if (key == QLatin1String("type")) m["type"] = nodeString(v);
        else if (key == QLatin1String("lang")) m["lang"] = nodeString(v);
        else if (key == QLatin1String("title")) m["title"] = nodeString(v);
        else if (key == QLatin1String("selected") && v.format == MPV_FORMAT_FLAG) m["selected"] = bool(v.u.flag);
        else if (key == QLatin1String("external") && v.format == MPV_FORMAT_FLAG) m["external"] = bool(v.u.flag);
    }
    return m;
}

} // namespace

void MpvCore::wakeupTrampoline(void *ctx)
{
    QMetaObject::invokeMethod(static_cast<MpvCore *>(ctx), "processEvents", Qt::QueuedConnection);
}

MpvCore::MpvCore(QObject *parent) : QObject(parent) {}

MpvCore::~MpvCore()
{
    if (m_mpv) mpv_terminate_destroy(m_mpv);
}

void MpvCore::setOption(const QString &name, const QString &value)
{
    if (!m_mpv) { ensureMpvLocale(); m_mpv = mpv_create(); }
    if (!m_mpv) { setError(QStringLiteral("mpv_create failed")); return; }
    mpv_set_option_string(m_mpv, name.toUtf8().constData(), value.toUtf8().constData());
}

bool MpvCore::init()
{
    if (!m_mpv) { ensureMpvLocale(); m_mpv = mpv_create(); }
    if (!m_mpv) { setError(QStringLiteral("mpv_create failed")); return false; }
    // "keep-open" so a finished file stays loaded (we report end-of-file ourselves and
    // decide what happens next; the UI layer owns "what's next", like Player does for audio).
    mpv_set_option_string(m_mpv, "keep-open", "yes");
    mpv_set_option_string(m_mpv, "video-timeout", "20");
    const int rc = mpv_initialize(m_mpv);
    if (rc < 0) {
        setError(QString::fromUtf8(mpv_error_string(rc)));
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        return false;
    }
    mpv_set_wakeup_callback(m_mpv, &MpvCore::wakeupTrampoline, this);
    mpv_observe_property(m_mpv, 1, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 2, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, 3, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 4, "core-idle", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, 5, "hwdec-current", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, 6, "track-list", MPV_FORMAT_NODE);
    return true;
}

bool MpvCore::open(const QString &path, qint64 startPositionMs)
{
    if (!m_mpv) return false;
    const QByteArray url = QUrl::fromLocalFile(path).toString().toUtf8();
    const char *args[] = {"loadfile", url.constData(), "replace", nullptr};
    const int rc = mpv_command(m_mpv, args);
    if (rc < 0) { setError(QString::fromUtf8(mpv_error_string(rc))); return false; }
    if (startPositionMs > 0) {
        // Applied once the file has actually loaded (see MPV_EVENT_FILE_LOADED below);
        // mpv rejects a seek before there is anything to seek in.
        m_pendingSeekMs = startPositionMs;
    }
    return true;
}

void MpvCore::play() { if (m_mpv) mpv_set_property_string(m_mpv, "pause", "no"); }
void MpvCore::pause() { if (m_mpv) mpv_set_property_string(m_mpv, "pause", "yes"); }
void MpvCore::togglePause() { paused() ? play() : pause(); }

void MpvCore::seek(qint64 ms)
{
    if (!m_mpv) return;
    const double secs = double(qBound<qint64>(0, ms, m_durationMs > 0 ? m_durationMs : ms)) / 1000.0;
    const QByteArray v = QByteArray::number(secs, 'f', 3);
    const char *args[] = {"seek", v.constData(), "absolute", nullptr};
    mpv_command(m_mpv, args);
}

void MpvCore::stop()
{
    if (!m_mpv) return;
    const char *args[] = {"stop", nullptr};
    mpv_command(m_mpv, args);
}

void MpvCore::setVolume(int percent)
{
    m_volume = qBound(0, percent, 100);
    if (m_mpv) mpv_set_property_string(m_mpv, "volume", QByteArray::number(m_volume).constData());
}

void MpvCore::addSubtitleFile(const QString &path)
{
    if (!m_mpv) return;
    const QByteArray p = path.toUtf8();
    const char *args[] = {"sub-add", p.constData(), "select", nullptr};
    mpv_command(m_mpv, args);
}

void MpvCore::setSubtitleTrack(int id)
{
    if (!m_mpv) return;
    mpv_set_property_string(m_mpv, "sid", id < 0 ? "no" : QByteArray::number(id).constData());
}

void MpvCore::setError(const QString &e)
{
    if (m_error == e) return;
    m_error = e;
    emit errorChanged();
}

void MpvCore::processEvents()
{
    if (!m_mpv) return;
    while (true) {
        mpv_event *ev = mpv_wait_event(m_mpv, 0.0);
        if (!ev || ev->event_id == MPV_EVENT_NONE) break;
        handleEvent(ev);
    }
}

void MpvCore::handleEvent(const mpv_event *ev)
{
    switch (ev->event_id) {
    case MPV_EVENT_FILE_LOADED:
        m_idle = false;
        if (m_pendingSeekMs > 0) { seek(m_pendingSeekMs); m_pendingSeekMs = 0; }
        emit fileLoaded();
        break;
    case MPV_EVENT_END_FILE:
        m_idle = true;
        emit endOfFile();
        break;
    case MPV_EVENT_SHUTDOWN:
        m_mpv = nullptr;   // core is gone; nothing else may touch the handle
        break;
    case MPV_EVENT_PROPERTY_CHANGE: {
        const auto *prop = static_cast<const mpv_event_property *>(ev->data);
        if (!prop || !prop->name) break;
        const QString name = QString::fromUtf8(prop->name);
        if (name == QLatin1String("time-pos")) {
            const qint64 ms = prop->format == MPV_FORMAT_DOUBLE
                ? qint64(*static_cast<double *>(prop->data) * 1000.0) : m_positionMs;
            if (ms != m_positionMs) { m_positionMs = ms; emit positionChanged(); }
        } else if (name == QLatin1String("duration")) {
            const qint64 ms = prop->format == MPV_FORMAT_DOUBLE
                ? qint64(*static_cast<double *>(prop->data) * 1000.0) : m_durationMs;
            if (ms != m_durationMs) { m_durationMs = ms; emit durationChanged(); }
        } else if (name == QLatin1String("pause")) {
            const bool p = prop->format == MPV_FORMAT_FLAG && *static_cast<int *>(prop->data) != 0;
            if (p != m_paused) { m_paused = p; emit pausedChanged(); }
        } else if (name == QLatin1String("hwdec-current")) {
            const QString v = prop->format == MPV_FORMAT_STRING && prop->data
                ? QString::fromUtf8(*static_cast<char **>(prop->data)) : QString();
            m_hwdecCurrent = v;
        } else if (name == QLatin1String("track-list")) {
            refreshTrackList();
        }
        break;
    }
    default:
        break;
    }
}

void MpvCore::refreshTrackList()
{
    if (!m_mpv) return;
    mpv_node root;
    if (mpv_get_property(m_mpv, "track-list", MPV_FORMAT_NODE, &root) < 0) return;
    QVariantList subs;
    if (root.format == MPV_FORMAT_NODE_ARRAY && root.u.list) {
        for (int i = 0; i < root.u.list->num; ++i) {
            const QVariantMap t = trackMap(root.u.list->values[i]);
            if (t.value("type").toString() == QLatin1String("sub")) subs.append(t);
        }
    }
    mpv_free_node_contents(&root);
    m_subtitleTracks = subs;
    emit subtitleTracksChanged();
}

} // namespace hop

#else   // !HOPTUNES_HAVE_MPV

// No libmpv.pc at configure time (see CMakeLists.txt and the Flatpak manifest's comment —
// not yet packaged there). Everything below is a no-op; the surface never plays anything
// and reports a clear error instead of a silent black rectangle.
namespace hop {

MpvCore::MpvCore(QObject *parent) : QObject(parent) {}
MpvCore::~MpvCore() = default;

void MpvCore::setOption(const QString &, const QString &) {}

bool MpvCore::init()
{
    setError(QStringLiteral("video playback is not available in this build (no libmpv)"));
    return false;
}

bool MpvCore::open(const QString &, qint64) { return false; }
void MpvCore::play() {}
void MpvCore::pause() {}
void MpvCore::togglePause() {}
void MpvCore::seek(qint64) {}
void MpvCore::stop() {}
void MpvCore::setVolume(int percent) { m_volume = qBound(0, percent, 100); }
void MpvCore::addSubtitleFile(const QString &) {}
void MpvCore::setSubtitleTrack(int) {}
void MpvCore::processEvents() {}
void MpvCore::handleEvent(const mpv_event *) {}
void MpvCore::refreshTrackList() {}
void MpvCore::wakeupTrampoline(void *) {}

void MpvCore::setError(const QString &e)
{
    if (m_error == e) return;
    m_error = e;
    emit errorChanged();
}

} // namespace hop

#endif   // HOPTUNES_HAVE_MPV
