// SPDX-License-Identifier: GPL-3.0-or-later
#include "playback/Player.h"

#include <QMutexLocker>
#include <QRandomGenerator>
#include <QUrl>
#include <QtGlobal>

#include <algorithm>
#include <cmath>

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

void releaseBin(GstElement *&bin, GstBus *&bus, void *owner)
{
    if (bin) {
        gst_element_set_state(bin, GST_STATE_NULL);
        g_signal_handlers_disconnect_by_data(bin, owner);
        gst_object_unref(bin);
        bin = nullptr;
    }
    if (bus) { gst_object_unref(bus); bus = nullptr; }
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
    m_factory = (major > 1 || minor >= 22) ? QByteArrayLiteral("playbin3") : QByteArrayLiteral("playbin");
    const QByteArray forced = qgetenv("HOPTUNES_PLAYBIN");
    if (forced == "playbin" || forced == "playbin3") m_factory = forced;
    if (GstElementFactory *f = gst_element_factory_find(m_factory.constData())) gst_object_unref(f);
    else m_factory = QByteArrayLiteral("playbin");

    m_playbin = makeBin();
    if (!m_playbin) {
        setError(QStringLiteral("GStreamer playbin is missing: install gst-plugins-base"));
        return;
    }
    m_bus = gst_element_get_bus(m_playbin);
    m_busTimer.setInterval(30);
    connect(&m_busTimer, &QTimer::timeout, this, &Player::pollBus);
    m_busTimer.start();

    m_posTimer.setInterval(250);
    connect(&m_posTimer, &QTimer::timeout, this, &Player::updatePosition);
    m_fadeTimer.setInterval(40);
    connect(&m_fadeTimer, &QTimer::timeout, this, &Player::fadeStep);
}

Player::~Player()
{
    m_fadeTimer.stop();
    releaseBin(m_outBin, m_outBus, this);
    releaseBin(m_playbin, m_bus, this);
}

GstElement *Player::makeBin()
{
    GstElement *bin = gst_element_factory_make(m_factory.constData(), nullptr);
    if (!bin) return nullptr;
    gst_object_ref_sink(bin);
    // Music only: never open a video window for embedded cover-art streams.
    g_object_set(bin, "flags", kFlagAudio | kFlagSoftVolume, "volume", m_volume, nullptr);
    // Headless runs (tests, CI, screenshots) can force a sink, e.g.
    // HOPTUNES_AUDIO_SINK=fakesink  or  HOPTUNES_AUDIO_SINK="fakesink sync=true" (real-time)
    const QByteArray sinkDesc = qgetenv("HOPTUNES_AUDIO_SINK");
    if (!sinkDesc.isEmpty()) {
        GError *err = nullptr;
        GstElement *sink = gst_parse_launch(sinkDesc.constData(), &err);
        if (err) g_error_free(err);
        if (sink) g_object_set(bin, "audio-sink", sink, nullptr);
    }
    applyGainFilter(bin);
    g_signal_connect(bin, "about-to-finish", G_CALLBACK(&Player::onAboutToFinish), this);
    return bin;
}

// ---- ReplayGain -----------------------------------------------------------------------

bool Player::replayGainAvailable() const
{
    GstElementFactory *f = gst_element_factory_find("rgvolume");
    if (f) gst_object_unref(f);
    return f != nullptr;
}

GstElement *Player::makeGainFilter() const
{
    if (m_rgMode == GainOff || !replayGainAvailable()) return nullptr;
    GError *err = nullptr;
    // rglimiter only exists alongside rgvolume (same plugin); it keeps boosted peaks from clipping.
    GstElement *f = gst_parse_bin_from_description("rgvolume name=rg ! rglimiter", TRUE, &err);
    if (err) { g_error_free(err); return nullptr; }
    if (GstElement *rg = gst_bin_get_by_name(GST_BIN(f), "rg")) {
        // Songs without gain tags get a small cut, so they sit near tagged ones (~89 dB target).
        g_object_set(rg, "album-mode", m_rgMode == GainAlbum, "pre-amp", m_rgPreamp, "fallback-gain", -6.0, nullptr);
        gst_object_unref(rg);
    }
    return f;
}

void Player::applyGainFilter(GstElement *bin)
{
    // playbin only accepts a new audio-filter when it is not running.
    GstState state = GST_STATE_NULL;
    gst_element_get_state(bin, &state, nullptr, 0);
    if (state > GST_STATE_READY) return;
    g_object_set(bin, "audio-filter", makeGainFilter(), nullptr);
}

void Player::setReplayGainMode(int mode)
{
    mode = qBound(0, mode, 2);
    if (mode == m_rgMode) return;
    m_rgMode = mode;
    if (m_playbin && !m_playing) applyGainFilter(m_playbin);   // otherwise from the next track
    emit replayGainChanged();
}

void Player::setReplayGainPreamp(double db)
{
    db = qBound(-15.0, db, 15.0);
    if (qFuzzyCompare(db + 100.0, m_rgPreamp + 100.0)) return;
    m_rgPreamp = db;
    if (m_playbin && !m_playing) applyGainFilter(m_playbin);
    emit replayGainChanged();
}

// ---- basic properties ------------------------------------------------------------------

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
    if (m_playbin && !m_outBin) g_object_set(m_playbin, "volume", v, nullptr);   // a fade uses it on the next step
    emit volumeChanged();
}

void Player::setRepeatMode(int mode)
{
    mode = qBound(0, mode, 2);
    if (mode == m_repeat.load()) return;
    m_repeat.store(mode);
    emit repeatModeChanged();
}

void Player::setCrossfadeMs(int ms)
{
    ms = qBound(0, ms, 12000);
    if (ms == m_crossfadeMs.load()) return;
    m_crossfadeMs.store(ms);
    emit crossfadeChanged();
}

void Player::syncQueueModel() { m_queueModel.setTracks(m_queue); }

// ---- queue ---------------------------------------------------------------------------

bool Player::continuesAlbum(const Track &a, const Track &b)
{
    if (a.album.isEmpty() || a.album != b.album) return false;
    const QString aa = a.albumArtist.isEmpty() ? a.artist : a.albumArtist;
    const QString ba = b.albumArtist.isEmpty() ? b.artist : b.albumArtist;
    if (aa != ba) return false;
    if (b.disc == a.disc) return b.trackNo == a.trackNo + 1;
    return b.disc == a.disc + 1 && b.trackNo == 1;
}

int Player::nextIndexLocked(bool userSkip) const
{
    if (m_queue.isEmpty()) return -1;
    if (!userSkip && m_repeat.load() == RepeatOne) return m_index;
    if (m_index + 1 < m_queue.size()) return m_index + 1;
    return m_repeat.load() == RepeatAll ? 0 : -1;
}

bool Player::wantsFadeLocked(int from, int to) const
{
    if (m_crossfadeMs.load() <= 0 || to < 0 || from < 0 || to == from) return false;
    if (from >= m_queue.size() || to >= m_queue.size()) return false;
    return !continuesAlbum(m_queue[from], m_queue[to]);
}

void Player::setQueue(const QList<Track> &tracks, int start)
{
    finishFade();
    QList<Track> q = tracks;
    int first = qBound(0, start, qMax(0, int(tracks.size()) - 1));
    if (m_shuffle && !tracks.isEmpty()) {
        m_unshuffled = tracks;
        const Track lead = q.takeAt(first);
        std::shuffle(q.begin(), q.end(), *QRandomGenerator::global());
        q.prepend(lead);
        first = 0;
    } else {
        m_unshuffled.clear();
    }
    {
        QMutexLocker lock(&m_gaplessMutex);
        m_queue = q;
        m_pendingIndex = -1;
    }
    syncQueueModel();
    emit queueChanged();
    if (q.isEmpty()) { stop(); m_index = -1; emit currentChanged(); return; }   // also used to clear on profile switch
    startAt(first);
}

void Player::enqueue(const QList<Track> &tracks)
{
    if (tracks.isEmpty()) return;
    if (!hasTrack()) { setQueue(tracks, 0); return; }
    {
        QMutexLocker lock(&m_gaplessMutex);
        m_queue.append(tracks);
    }
    if (m_shuffle) m_unshuffled.append(tracks);
    syncQueueModel();
    emit queueChanged();
}

void Player::playNext(const QList<Track> &tracks)
{
    if (tracks.isEmpty()) return;
    if (!hasTrack()) { setQueue(tracks, 0); return; }
    {
        QMutexLocker lock(&m_gaplessMutex);
        const int at = (m_pendingIndex >= 0 ? m_pendingIndex : m_index) + 1;
        for (int i = 0; i < tracks.size(); ++i) m_queue.insert(at + i, tracks[i]);
    }
    if (m_shuffle) m_unshuffled.append(tracks);
    syncQueueModel();
    emit queueChanged();
}

bool Player::moveQueueItem(int from, int to)
{
    {
        QMutexLocker lock(&m_gaplessMutex);
        const int n = int(m_queue.size());
        if (from < 0 || to < 0 || from >= n || to >= n || from == to) return false;
        m_queue.move(from, to);
        auto shift = [from, to](int i) {
            if (i < 0) return i;
            if (i == from) return to;
            if (from < i && to >= i) return i - 1;
            if (from > i && to <= i) return i + 1;
            return i;
        };
        m_index = shift(m_index);
        m_pendingIndex = shift(m_pendingIndex);
    }
    syncQueueModel();
    emit queueChanged();
    emit currentChanged();   // "track n of m" may have changed; the song itself has not
    return true;
}

bool Player::removeFromQueue(int index)
{
    {
        QMutexLocker lock(&m_gaplessMutex);
        if (index < 0 || index >= m_queue.size() || index == m_index || index == m_pendingIndex) return false;
        m_queue.removeAt(index);
        if (index < m_index) --m_index;
        if (m_pendingIndex > index) --m_pendingIndex;
    }
    syncQueueModel();
    emit queueChanged();
    emit currentChanged();
    return true;
}

void Player::setShuffle(bool on)
{
    if (on == m_shuffle) return;
    m_shuffle = on;
    if (hasTrack()) {
        QMutexLocker lock(&m_gaplessMutex);
        if (on) {
            // Keep what is playing (and what is already lined up), shuffle the rest.
            m_unshuffled = m_queue;
            const int keep = (m_pendingIndex >= 0 ? m_pendingIndex : m_index) + 1;
            if (keep < m_queue.size()) std::shuffle(m_queue.begin() + keep, m_queue.end(), *QRandomGenerator::global());
        } else if (!m_unshuffled.isEmpty()) {
            auto find = [this](int i) {
                if (i < 0 || i >= m_queue.size()) return i;
                for (int k = 0; k < m_unshuffled.size(); ++k)
                    if (m_unshuffled[k].path == m_queue[i].path) return k;
                return i;
            };
            const int idx = find(m_index), pend = find(m_pendingIndex);
            m_queue = m_unshuffled;
            m_unshuffled.clear();
            m_index = idx;
            m_pendingIndex = pend;
        }
    }
    syncQueueModel();
    emit queueChanged();
    emit currentChanged();
    emit shuffleChanged();
}

// ---- transport -------------------------------------------------------------------------

void Player::playFrom(const Track &track, qint64 positionMs)
{
    setQueue({track}, 0);
    m_startSeekMs = positionMs > 0 ? positionMs : 0;   // after setQueue: startAt() must not clear it
}

void Player::startAt(int index)
{
    finishFade();
    if (!m_playbin || index < 0 || index >= m_queue.size()) return;
    m_switching = true;
    gst_element_set_state(m_playbin, GST_STATE_READY);
    m_switching = false;
    {
        QMutexLocker lock(&m_gaplessMutex);
        m_index = index;
        m_pendingIndex = -1;
    }
    applyGainFilter(m_playbin);
    g_object_set(m_playbin, "uri", toUri(m_queue[index].path).constData(), "volume", m_volume, nullptr);
    m_position = 0;
    m_duration = m_queue[index].durationMs;
    emit positionChanged();
    emit durationChanged();
    emit currentChanged();
    emit trackStarted();
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
    finishFade();
    gst_element_set_state(m_playbin, GST_STATE_PAUSED);
    setPlaying(false);
}

void Player::toggle() { m_playing ? pause() : play(); }

void Player::stop()
{
    finishFade();
    m_switching = true;
    if (m_playbin) gst_element_set_state(m_playbin, GST_STATE_NULL);
    m_switching = false;
    setPlaying(false);
    m_position = 0;
    emit positionChanged();
}

void Player::next()
{
    int n;
    {
        QMutexLocker lock(&m_gaplessMutex);
        n = nextIndexLocked(true);
    }
    if (n >= 0) startAt(n);
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
    finishFade();
    ms = qMax<qint64>(0, ms);
    gst_element_seek_simple(m_playbin, GST_FORMAT_TIME,
                            GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT), ms * GST_MSECOND);
    m_position = ms;
    emit positionChanged();
    emit seeked(ms);
}

// Runs on a GStreamer streaming thread, shortly before the current track ends.
void Player::onAboutToFinish(GstElement *playbin, void *self)
{
    auto *p = static_cast<Player *>(self);
    // Touching the pipeline while the UI thread is tearing it down can deadlock inside
    // GStreamer (seen with playbin3 1.24 when skipping right at a gapless hand-off).
    if (p->m_switching) return;
    QByteArray uri;
    {
        QMutexLocker lock(&p->m_gaplessMutex);
        if (playbin != p->m_playbin) return;            // the song fading out: it just ends
        const int next = p->nextIndexLocked(false);
        if (next < 0) return;                           // end of queue: let EOS happen
        if (p->wantsFadeLocked(p->m_index, next)) return;   // crossfade (or EOS) takes over
        p->m_pendingIndex = next;
        uri = toUri(p->m_queue[next].path);
    }
    g_object_set(playbin, "uri", uri.constData(), nullptr);
}

// ---- crossfade ---------------------------------------------------------------------------

void Player::startFade(int next)
{
    GstElement *in = makeBin();
    if (!in) return;
    g_object_set(in, "uri", toUri(m_queue[next].path).constData(), "volume", 0.0, nullptr);
    {
        QMutexLocker lock(&m_gaplessMutex);
        m_outBin = m_playbin;
        m_outBus = m_bus;
        m_playbin = in;
        m_bus = gst_element_get_bus(in);
        m_index = next;
        m_pendingIndex = -1;
    }
    m_fadeLen = std::clamp<qint64>(std::min<qint64>(m_crossfadeMs.load(), m_duration - m_position), 500, 12000);
    m_fadeClock.start();
    gst_element_set_state(m_playbin, GST_STATE_PLAYING);
    m_position = 0;
    m_duration = m_queue[next].durationMs;
    emit positionChanged();
    emit durationChanged();
    emit currentChanged();
    emit trackStarted();
    emit fadingChanged();
    m_fadeTimer.start();
}

void Player::fadeStep()
{
    if (!m_outBin) { m_fadeTimer.stop(); return; }
    const double t = qBound(0.0, double(m_fadeClock.elapsed()) / double(qMax<qint64>(1, m_fadeLen)), 1.0);
    // Equal-power curve: the overall loudness stays even through the fade.
    g_object_set(m_playbin, "volume", m_volume * std::sin(t * M_PI / 2), nullptr);
    g_object_set(m_outBin, "volume", m_volume * std::cos(t * M_PI / 2), nullptr);
    if (t >= 1.0) finishFade();
}

void Player::finishFade()
{
    m_fadeTimer.stop();
    if (!m_outBin) return;
    {
        QMutexLocker lock(&m_gaplessMutex);
        releaseBin(m_outBin, m_outBus, this);
    }
    if (m_playbin) g_object_set(m_playbin, "volume", m_volume, nullptr);
    emit fadingChanged();
}

// ---- bus + position ----------------------------------------------------------------------

void Player::pollBus()
{
    // The fading-out song only matters for its end.
    while (m_outBus) {
        GstMessage *msg = gst_bus_pop(m_outBus);
        if (!msg) break;
        const auto type = GST_MESSAGE_TYPE(msg);
        gst_message_unref(msg);
        if (type == GST_MESSAGE_EOS || type == GST_MESSAGE_ERROR) finishFade();
    }
    if (!m_bus) return;
    // Bounded per tick, so a flood of messages (a tiny file on repeat, a broken stream)
    // can never starve the UI thread.
    for (int budget = 64; budget > 0; --budget) {
        GstMessage *msg = gst_bus_pop(m_bus);
        if (!msg) break;
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
                emit trackStarted();
            }
            break;
        }
        case GST_MESSAGE_EOS: {
            // Reached when nothing was lined up gaplessly: the end of the queue, or a
            // crossfade that could not start in time (e.g. after seeking to the very end).
            int next;
            {
                QMutexLocker lock(&m_gaplessMutex);
                next = nextIndexLocked(false);
            }
            if (next >= 0) {
                startAt(next);
            } else {
                gst_element_set_state(m_playbin, GST_STATE_READY);
                setPlaying(false);
                m_position = m_duration;
                emit positionChanged();
            }
            break;
        }
        case GST_MESSAGE_ERROR: {
            GError *err = nullptr;
            gchar *dbg = nullptr;
            gst_message_parse_error(msg, &err, &dbg);
            const QString text = QString::fromUtf8(err ? err->message : "playback error");
            if (err) g_error_free(err);
            g_free(dbg);
            // Skip a broken file instead of stopping the whole queue. Drop the rest of its
            // messages first (one bad file can post several errors), and keep the message
            // visible after moving on.
            gst_message_unref(msg);
            gst_bus_set_flushing(m_bus, TRUE);
            gst_bus_set_flushing(m_bus, FALSE);
            if (m_index + 1 < m_queue.size()) startAt(m_index + 1);
            else stop();
            setError(text);
            return;
        }
        case GST_MESSAGE_ASYNC_DONE:
            if (m_startSeekMs > 0 && GST_MESSAGE_SRC(msg) == GST_OBJECT(m_playbin)) {
                const qint64 ms = m_startSeekMs;
                m_startSeekMs = 0;
                seek(ms);
            }
            break;
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
    // Start a crossfade when the song is that close to its end.
    if (m_playing && !m_outBin && m_duration > 0 && m_position > 0) {
        int next = -1;
        bool fade = false;
        {
            QMutexLocker lock(&m_gaplessMutex);
            next = nextIndexLocked(false);
            fade = m_pendingIndex < 0 && wantsFadeLocked(m_index, next);
        }
        if (fade && m_duration - m_position <= m_crossfadeMs.load()) startFade(next);
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
