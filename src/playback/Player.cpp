// SPDX-License-Identifier: GPL-3.0-or-later
#include "playback/Player.h"

#include <QMutexLocker>
#include <QUrl>
#include <QtGlobal>

#include <gst/gst.h>

namespace hop {

namespace {
// GstPlayFlags lives in a plugin header; these are its stable values.
constexpr guint kFlagAudio = 1u << 1;
constexpr guint kFlagSoftVolume = 1u << 4;

QByteArray toUri(const QString &path)
{
    GError *err = nullptr;
    gchar *uri = gst_filename_to_uri(path.toUtf8().constData(), &err);
    if (err) g_error_free(err);
    QByteArray out(uri ? uri : "");
    g_free(uri);
    return out;
}
} // namespace

Player::Player(QObject *parent) : QObject(parent)
{
    if (!gst_is_initialized()) gst_init(nullptr, nullptr);

    // playbin3's gapless hand-off is unreliable before GStreamer 1.22 ("Internal data stream
    // error" at the end of a queue), so older runtimes use the long-proven playbin.
    // HOPTUNES_PLAYBIN=playbin|playbin3 overrides (used by tests).
    guint major = 0, minor = 0, micro = 0, nano = 0;
    gst_version(&major, &minor, &micro, &nano);
    QByteArray factory = (major > 1 || minor >= 22) ? QByteArrayLiteral("playbin3") : QByteArrayLiteral("playbin");
    const QByteArray forced = qgetenv("HOPTUNES_PLAYBIN");
    if (forced == "playbin" || forced == "playbin3") factory = forced;
    m_playbin = gst_element_factory_make(factory.constData(), "hop-player");
    if (!m_playbin) m_playbin = gst_element_factory_make("playbin", "hop-player");
    if (!m_playbin) {
        setError(QStringLiteral("GStreamer playbin is missing: install gst-plugins-base"));
        return;
    }
    gst_object_ref_sink(m_playbin);

    // Music only: never open a video window for embedded cover-art streams.
    g_object_set(m_playbin, "flags", kFlagAudio | kFlagSoftVolume, nullptr);

    // Headless runs (tests, CI, screenshots) can force a sink, e.g.
    // HOPTUNES_AUDIO_SINK=fakesink  or  HOPTUNES_AUDIO_SINK="fakesink sync=true" (real-time)
    const QByteArray sinkDesc = qgetenv("HOPTUNES_AUDIO_SINK");
    if (!sinkDesc.isEmpty()) {
        GError *err = nullptr;
        GstElement *sink = gst_parse_launch(sinkDesc.constData(), &err);
        if (err) g_error_free(err);
        if (sink) g_object_set(m_playbin, "audio-sink", sink, nullptr);
    }

    g_signal_connect(m_playbin, "about-to-finish", G_CALLBACK(&Player::onAboutToFinish), this);

    m_bus = gst_element_get_bus(m_playbin);
    m_busTimer.setInterval(30);
    connect(&m_busTimer, &QTimer::timeout, this, &Player::pollBus);
    m_busTimer.start();

    m_posTimer.setInterval(250);
    connect(&m_posTimer, &QTimer::timeout, this, &Player::updatePosition);
}

Player::~Player()
{
    if (m_playbin) {
        gst_element_set_state(m_playbin, GST_STATE_NULL);
        g_signal_handlers_disconnect_by_data(m_playbin, this);
        gst_object_unref(m_playbin);
    }
    if (m_bus) gst_object_unref(m_bus);
}

QString Player::title() const  { return hasTrack() ? m_queue[m_index].displayTitle() : QString(); }
QString Player::artist() const { return hasTrack() ? m_queue[m_index].displayArtist() : QString(); }
QString Player::artUrl() const
{
    return (hasTrack() && !m_queue[m_index].art.isEmpty()) ? QUrl::fromLocalFile(m_queue[m_index].art).toString() : QString();
}
QString Player::album() const  { return hasTrack() ? m_queue[m_index].displayAlbum() : QString(); }

void Player::setVolume(double v)
{
    v = qBound(0.0, v, 1.0);
    if (qFuzzyCompare(v, m_volume)) return;
    m_volume = v;
    if (m_playbin) g_object_set(m_playbin, "volume", v, nullptr);
    emit volumeChanged();
}

void Player::setQueue(const QList<Track> &tracks, int start)
{
    {
        QMutexLocker lock(&m_gaplessMutex);
        m_queue = tracks;
        m_pendingIndex = -1;
    }
    emit queueChanged();
    if (tracks.isEmpty()) { stop(); return; }
    startAt(qBound(0, start, int(tracks.size()) - 1));
}

void Player::startAt(int index)
{
    if (!m_playbin || index < 0 || index >= m_queue.size()) return;
    gst_element_set_state(m_playbin, GST_STATE_READY);
    {
        QMutexLocker lock(&m_gaplessMutex);
        m_index = index;
        m_pendingIndex = -1;
    }
    g_object_set(m_playbin, "uri", toUri(m_queue[index].path).constData(), nullptr);
    m_position = 0;
    m_duration = m_queue[index].durationMs;
    emit positionChanged();
    emit durationChanged();
    emit currentChanged();
    setError({});
    play();
}

void Player::play()
{
    if (!m_playbin || !hasTrack()) return;
    if (gst_element_set_state(m_playbin, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        setError(QStringLiteral("could not play %1").arg(m_queue[m_index].path));
        return;
    }
    setPlaying(true);
}

void Player::pause()
{
    if (!m_playbin) return;
    gst_element_set_state(m_playbin, GST_STATE_PAUSED);
    setPlaying(false);
}

void Player::toggle() { m_playing ? pause() : play(); }

void Player::stop()
{
    if (m_playbin) gst_element_set_state(m_playbin, GST_STATE_NULL);
    setPlaying(false);
    m_position = 0;
    emit positionChanged();
}

void Player::next()
{
    if (m_index + 1 < m_queue.size()) startAt(m_index + 1);
    else stop();
}

void Player::previous()
{
    if (m_position > 3000 || m_index <= 0) { seek(0); return; }
    startAt(m_index - 1);
}

void Player::seek(qint64 ms)
{
    if (!m_playbin || !hasTrack()) return;
    ms = qMax<qint64>(0, ms);
    gst_element_seek_simple(m_playbin, GST_FORMAT_TIME,
                            GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), ms * GST_MSECOND);
    m_position = ms;
    emit positionChanged();
}

// Runs on a GStreamer streaming thread, shortly before the current track ends.
void Player::onAboutToFinish(GstElement *playbin, void *self)
{
    auto *p = static_cast<Player *>(self);
    QByteArray uri;
    {
        QMutexLocker lock(&p->m_gaplessMutex);
        const int next = p->m_index + 1;
        if (next >= p->m_queue.size()) return;     // end of queue: let EOS happen
        p->m_pendingIndex = next;
        uri = toUri(p->m_queue[next].path);
    }
    g_object_set(playbin, "uri", uri.constData(), nullptr);
}

void Player::pollBus()
{
    if (!m_bus) return;
    while (GstMessage *msg = gst_bus_pop(m_bus)) {
        switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_STREAM_START: {
            int pending;
            {
                QMutexLocker lock(&m_gaplessMutex);
                pending = m_pendingIndex;
                if (pending >= 0) { m_index = pending; m_pendingIndex = -1; }
            }
            if (pending >= 0) {
                m_position = 0;
                m_duration = m_queue[m_index].durationMs;
                emit positionChanged();
                emit durationChanged();
                emit currentChanged();
            }
            break;
        }
        case GST_MESSAGE_EOS:
            gst_element_set_state(m_playbin, GST_STATE_READY);
            setPlaying(false);
            m_position = m_duration;
            emit positionChanged();
            break;
        case GST_MESSAGE_ERROR: {
            GError *err = nullptr;
            gchar *dbg = nullptr;
            gst_message_parse_error(msg, &err, &dbg);
            const QString text = QString::fromUtf8(err ? err->message : "playback error");
            if (err) g_error_free(err);
            g_free(dbg);
            setError(text);
            // Skip a broken file instead of stopping the whole queue.
            if (m_index + 1 < m_queue.size()) startAt(m_index + 1);
            else stop();
            break;
        }
        case GST_MESSAGE_DURATION_CHANGED:
            updatePosition();
            break;
        default:
            break;
        }
        gst_message_unref(msg);
    }
}

void Player::updatePosition()
{
    if (!m_playbin) return;
    gint64 pos = 0, dur = 0;
    if (gst_element_query_position(m_playbin, GST_FORMAT_TIME, &pos)) {
        const qint64 ms = pos / GST_MSECOND;
        if (ms != m_position) { m_position = ms; emit positionChanged(); }
    }
    if (gst_element_query_duration(m_playbin, GST_FORMAT_TIME, &dur) && dur > 0) {
        const qint64 ms = dur / GST_MSECOND;
        if (ms != m_duration) { m_duration = ms; emit durationChanged(); }
    }
}

void Player::setPlaying(bool p)
{
    if (p) m_posTimer.start(); else m_posTimer.stop();
    if (p == m_playing) return;
    m_playing = p;
    emit playingChanged();
}

void Player::setError(const QString &e)
{
    if (e == m_error) return;
    m_error = e;
    emit errorChanged();
}

} // namespace hop
