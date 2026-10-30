// SPDX-License-Identifier: GPL-3.0-or-later
#include "playback/GstVideoPlayer.h"

#include <QFileInfo>
#include <QUrl>
#include <QVariantMap>

#include <gst/gst.h>

namespace hop {

namespace {

// GstPlayFlags lives in a plugin header; these are its stable values (see Player.cpp).
constexpr guint kFlagVideo = 1u << 0;
constexpr guint kFlagAudio = 1u << 1;
constexpr guint kFlagText = 1u << 2;
constexpr guint kFlagSoftVolume = 1u << 4;

// Classic playbin, not playbin3: a single video has no gapless hand-off to speed up (the
// one thing playbin3 is for in Player.cpp), and playbin3's stream-selection API differs
// from the n-text/current-text/get-text-tags properties used below for subtitle tracks
// (confirmed by testing — GstPlayBin3 doesn't have those properties at all).
QByteArray playbinFactoryName() { return QByteArrayLiteral("playbin"); }

// A same-basename .srt/.ass/.ssa/.vtt next to the video, if any (playbin has no
// auto-sidecar detection of its own — unlike mpv, which does this for us).
QString sidecarSubtitle(const QString &videoPath)
{
    const QFileInfo fi(videoPath);
    const QString base = fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QLatin1Char('.');
    for (const char *ext : {"srt", "ass", "ssa", "vtt"}) {
        const QString candidate = base + QLatin1String(ext);
        if (QFileInfo::exists(candidate)) return candidate;
    }
    return {};
}

// Best-effort: GStreamer auto-plugs a hardware decoder when one is ranked and installed
// (vaapi, nvcodec, v4l2, msdk, d3d11…); there is no single "hwdec" knob to ask for like
// mpv's, so this walks the live pipeline afterwards and looks for one by name.
void findHwDecoder(GstBin *bin, QString &out)
{
    GstIterator *it = gst_bin_iterate_recurse(bin);
    GValue v = G_VALUE_INIT;
    while (gst_iterator_next(it, &v) == GST_ITERATOR_OK) {
        auto *el = GST_ELEMENT(g_value_get_object(&v));
        if (GstElementFactory *f = gst_element_get_factory(el)) {
            const QString name = QString::fromUtf8(GST_OBJECT_NAME(f));
            for (const char *hint : {"vaapi", "nvdec", "nvh264", "nvh265", "v4l2sl", "msdk", "d3d11"}) {
                if (name.contains(QLatin1String(hint), Qt::CaseInsensitive)) { out = name; break; }
            }
        }
        g_value_unset(&v);
        if (!out.isEmpty()) break;
    }
    gst_iterator_free(it);
}

} // namespace

GstVideoPlayer::GstVideoPlayer(QObject *parent) : QObject(parent)
{
    if (!gst_is_initialized()) gst_init(nullptr, nullptr);
    m_busTimer.setInterval(30);
    connect(&m_busTimer, &QTimer::timeout, this, &GstVideoPlayer::pollBus);
    m_posTimer.setInterval(250);
    connect(&m_posTimer, &QTimer::timeout, this, &GstVideoPlayer::updatePosition);
}

GstVideoPlayer::~GstVideoPlayer()
{
    if (m_pipeline) {
        gst_element_set_state(m_pipeline, GST_STATE_NULL);
        if (m_bus) gst_object_unref(m_bus);
        gst_object_unref(m_pipeline);
    }
}

bool GstVideoPlayer::available()
{
    GstElementFactory *sink = gst_element_factory_find("qml6glsink");
    GstElementFactory *bin = gst_element_factory_find("glsinkbin");
    const bool ok = sink && bin;
    if (sink) gst_object_unref(sink);
    if (bin) gst_object_unref(bin);
    return ok;
}

void GstVideoPlayer::setError(const QString &e)
{
    if (m_error == e) return;
    m_error = e;
    emit errorChanged();
}

void GstVideoPlayer::setPaused(bool p)
{
    if (m_paused == p) return;
    m_paused = p;
    emit pausedChanged();
}

void GstVideoPlayer::setVolume(int percent)
{
    m_volume = qBound(0, percent, 150);
    if (m_pipeline) g_object_set(m_pipeline, "volume", double(m_volume) / 100.0, nullptr);
    emit volumeChanged();
}

void GstVideoPlayer::attachSurface(QQuickItem *item)
{
    if (m_pipeline) return;   // one surface per player; already built
    m_surface = item;

    const QByteArray factory = playbinFactoryName();
    m_pipeline = gst_element_factory_make(factory.constData(), "hop-video-pipeline");
    if (!m_pipeline) { setError(QStringLiteral("GStreamer playbin is missing")); return; }
    gst_object_ref_sink(m_pipeline);
    g_object_set(m_pipeline, "flags", kFlagVideo | kFlagAudio | kFlagText | kFlagSoftVolume,
                 "volume", double(m_volume) / 100.0, nullptr);

    // Headless (tests): HOPTUNES_VIDEO_SINK=fakesink skips the real GL sink chain, same
    // idea as HOPTUNES_AUDIO_SINK in Player.cpp.
    const QByteArray forcedSink = qgetenv("HOPTUNES_VIDEO_SINK");
    if (!forcedSink.isEmpty()) {
        GError *err = nullptr;
        GstElement *sink = gst_parse_launch(forcedSink.constData(), &err);
        if (err) g_error_free(err);
        if (sink) g_object_set(m_pipeline, "video-sink", sink, nullptr);
    } else if (m_surface) {
        GstElement *glsink = gst_element_factory_make("qml6glsink", nullptr);
        GstElement *sinkbin = gst_element_factory_make("glsinkbin", nullptr);
        if (glsink && sinkbin) {
            g_object_set(glsink, "widget", static_cast<void *>(m_surface), nullptr);
            g_object_set(sinkbin, "sink", glsink, nullptr);
            g_object_set(m_pipeline, "video-sink", sinkbin, nullptr);
        } else {
            setError(QStringLiteral("gstreamer1.0-qt6 / gstreamer1.0-gl not installed"));
        }
    } else {
        setError(QStringLiteral("attachSurface() called with no surface and no HOPTUNES_VIDEO_SINK override"));
    }

    m_bus = gst_element_get_bus(m_pipeline);
    m_busTimer.start();
}

bool GstVideoPlayer::openSource(const QString &path, qint64 startPositionMs)
{
    if (!m_pipeline) return false;
    gst_element_set_state(m_pipeline, GST_STATE_NULL);

    const QByteArray uri = QUrl::fromLocalFile(path).toString().toUtf8();
    g_object_set(m_pipeline, "uri", uri.constData(), nullptr);

    const QString sub = sidecarSubtitle(path);
    if (!sub.isEmpty()) {
        const QByteArray subUri = QUrl::fromLocalFile(sub).toString().toUtf8();
        g_object_set(m_pipeline, "suburi", subUri.constData(), nullptr);
    }

    m_pendingSeekMs = startPositionMs;
    m_idle = false;
    m_duration = 0;
    emit durationChanged();
    const GstStateChangeReturn rc = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    if (rc == GST_STATE_CHANGE_FAILURE) { setError(QStringLiteral("could not start playback")); return false; }
    setPaused(false);
    m_posTimer.start();
    return true;
}

void GstVideoPlayer::play()
{
    if (!m_pipeline) return;
    gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
    setPaused(false);
    m_posTimer.start();
}

void GstVideoPlayer::pause()
{
    if (!m_pipeline) return;
    gst_element_set_state(m_pipeline, GST_STATE_PAUSED);
    setPaused(true);
}

void GstVideoPlayer::togglePause() { paused() ? play() : pause(); }

void GstVideoPlayer::seek(qint64 ms)
{
    if (!m_pipeline) return;
    gst_element_seek_simple(m_pipeline, GST_FORMAT_TIME,
                            GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                            qMax<qint64>(0, ms) * GST_MSECOND);
}

void GstVideoPlayer::stop()
{
    if (!m_pipeline) return;
    gst_element_set_state(m_pipeline, GST_STATE_NULL);
    m_posTimer.stop();
    m_idle = true;
    emit idleChanged();
}

void GstVideoPlayer::setSubtitleTrack(int id)
{
    if (!m_pipeline) return;
    if (id < 0) {
        guint flags = 0;
        g_object_get(m_pipeline, "flags", &flags, nullptr);
        g_object_set(m_pipeline, "flags", flags & ~kFlagText, nullptr);
    } else {
        guint flags = 0;
        g_object_get(m_pipeline, "flags", &flags, nullptr);
        g_object_set(m_pipeline, "flags", flags | kFlagText, "current-text", id, nullptr);
    }
}

void GstVideoPlayer::updatePosition()
{
    if (!m_pipeline) return;
    // playbin finishes linking a suburi's text stream slightly after the main stream's
    // ASYNC_DONE (confirmed by testing), so keep checking here too until it shows up.
    if (m_subtitleTracks.isEmpty()) refreshSubtitleTracks();
    gint64 pos = 0;
    if (gst_element_query_position(m_pipeline, GST_FORMAT_TIME, &pos)) {
        const qint64 ms = pos / 1000000;
        if (ms != m_position) { m_position = ms; emit positionChanged(); }
    }
    if (m_duration <= 0) {
        gint64 dur = 0;
        if (gst_element_query_duration(m_pipeline, GST_FORMAT_TIME, &dur) && dur > 0) {
            m_duration = dur / 1000000;
            emit durationChanged();
        }
    }
}

void GstVideoPlayer::refreshSubtitleTracks()
{
    if (!m_pipeline) return;
    gint n = 0;
    g_object_get(m_pipeline, "n-text", &n, nullptr);
    gint current = -1;
    g_object_get(m_pipeline, "current-text", &current, nullptr);
    QVariantList tracks;
    for (gint i = 0; i < n; ++i) {
        QVariantMap t;
        t["id"] = i;
        t["type"] = QStringLiteral("sub");
        t["selected"] = (i == current);
        GstTagList *tags = nullptr;
        g_signal_emit_by_name(m_pipeline, "get-text-tags", i, &tags);
        if (tags) {
            gchar *lang = nullptr;
            if (gst_tag_list_get_string(tags, GST_TAG_LANGUAGE_CODE, &lang) && lang) {
                t["lang"] = QString::fromUtf8(lang);
                g_free(lang);
            }
            gst_tag_list_unref(tags);
        }
        tracks.append(t);
    }
    m_subtitleTracks = tracks;
    emit subtitleTracksChanged();
}

void GstVideoPlayer::detectHwdec()
{
    if (!m_pipeline) return;
    QString found;
    findHwDecoder(GST_BIN(m_pipeline), found);
    const QString next = found.isEmpty() ? QStringLiteral("no") : found;
    if (next != m_hwdecActive) { m_hwdecActive = next; emit hwdecActiveChanged(); }
}

void GstVideoPlayer::pollBus()
{
    if (!m_bus) return;
    GstMessage *msg;
    while ((msg = gst_bus_pop(m_bus)) != nullptr) {
        switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ASYNC_DONE:
            if (m_pendingSeekMs > 0) { seek(m_pendingSeekMs); m_pendingSeekMs = 0; }
            refreshSubtitleTracks();
            detectHwdec();
            emit fileLoaded();
            break;
        case GST_MESSAGE_EOS:
            m_idle = true;
            emit idleChanged();
            emit endOfFile();
            break;
        case GST_MESSAGE_ERROR: {
            GError *err = nullptr;
            gchar *dbg = nullptr;
            gst_message_parse_error(msg, &err, &dbg);
            setError(err ? QString::fromUtf8(err->message) : QStringLiteral("playback error"));
            if (err) g_error_free(err);
            g_free(dbg);
            break;
        }
        default:
            break;
        }
        gst_message_unref(msg);
    }
}

} // namespace hop
