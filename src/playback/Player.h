// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library/Track.h"
#include "library/TrackListModel.h"

#include <QElapsedTimer>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QTimer>

#include <atomic>

typedef struct _GstElement GstElement;
typedef struct _GstBus GstBus;

namespace hop {

// Music playback on GStreamer playbin3 (spec §5).
// - Gapless: when a track is about to end, playbin's "about-to-finish" hands it the next
//   URI, so tracks join sample-accurately.
// - Crossfade (0–12 s, off by default): a second playbin starts the next song and the two
//   are faded with an equal-power curve. Never inside a continuous album, which stays gapless.
// - ReplayGain: GStreamer's rgvolume (+ rglimiter) applies the gain tags in the files.
// - Repeat off / all / one, shuffle, play next, add to queue, reordering.
class Player : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool playing READ isPlaying NOTIFY playingChanged)
    Q_PROPERTY(bool hasTrack READ hasTrack NOTIFY currentChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentChanged)
    Q_PROPERTY(int queueLength READ queueLength NOTIFY queueChanged)
    Q_PROPERTY(hop::TrackListModel *queueModel READ queueModel CONSTANT)
    Q_PROPERTY(qint64 currentTrackId READ currentTrackId NOTIFY currentChanged)
    Q_PROPERTY(QString title READ title NOTIFY currentChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY currentChanged)
    Q_PROPERTY(QString album READ album NOTIFY currentChanged)
    Q_PROPERTY(int year READ year NOTIFY currentChanged)
    Q_PROPERTY(int trackNo READ trackNo NOTIFY currentChanged)
    Q_PROPERTY(QString artUrl READ artUrl NOTIFY currentChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(int repeatMode READ repeatMode WRITE setRepeatMode NOTIFY repeatModeChanged)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY shuffleChanged)
    Q_PROPERTY(int crossfadeMs READ crossfadeMs WRITE setCrossfadeMs NOTIFY crossfadeChanged)
    Q_PROPERTY(bool fading READ fading NOTIFY fadingChanged)
    Q_PROPERTY(int replayGainMode READ replayGainMode WRITE setReplayGainMode NOTIFY replayGainChanged)
    Q_PROPERTY(double replayGainPreamp READ replayGainPreamp WRITE setReplayGainPreamp NOTIFY replayGainChanged)
    Q_PROPERTY(bool replayGainAvailable READ replayGainAvailable CONSTANT)

public:
    enum Repeat { RepeatOff = 0, RepeatAll = 1, RepeatOne = 2 };
    enum Gain { GainOff = 0, GainTrack = 1, GainAlbum = 2 };
    Q_ENUM(Repeat)
    Q_ENUM(Gain)

    explicit Player(QObject *parent = nullptr);
    ~Player() override;

    bool isPlaying() const { return m_playing; }
    bool hasTrack() const { return m_index >= 0 && m_index < m_queue.size(); }
    int currentIndex() const { return m_index; }
    int queueLength() const { return int(m_queue.size()); }
    TrackListModel *queueModel() { return &m_queueModel; }
    qint64 currentTrackId() const { return hasTrack() ? m_queue[m_index].id : 0; }
    QString title() const;
    QString artist() const;
    QString album() const;
    int year() const { return hasTrack() ? m_queue[m_index].year : 0; }
    int trackNo() const { return hasTrack() ? m_queue[m_index].trackNo : 0; }
    QString artUrl() const;
    const Track *currentTrack() const { return hasTrack() ? &m_queue[m_index] : nullptr; }
    qint64 position() const { return m_position; }
    qint64 duration() const { return m_duration; }
    double volume() const { return m_volume; }
    void setVolume(double v);
    QString error() const { return m_error; }

    int repeatMode() const { return m_repeat.load(); }
    void setRepeatMode(int mode);
    bool shuffle() const { return m_shuffle; }
    void setShuffle(bool on);
    int crossfadeMs() const { return m_crossfadeMs.load(); }
    void setCrossfadeMs(int ms);       // 0 = off, max 12 000
    bool fading() const { return m_outBin != nullptr; }
    int replayGainMode() const { return m_rgMode; }
    void setReplayGainMode(int mode);  // applies from the next track
    double replayGainPreamp() const { return m_rgPreamp; }
    void setReplayGainPreamp(double db);
    bool replayGainAvailable() const;

    // Replaces the queue and starts playing at `start` (with shuffle on, `start` plays
    // first and the rest is shuffled).
    void setQueue(const QList<Track> &tracks, int start);
    // Plays one track from positionMs (resuming an audiobook or podcast). The seek happens once
    // the pipeline has prerolled, because a seek before that is silently dropped.
    void playFrom(const Track &track, qint64 positionMs);
    QList<Track> queue() const { return m_queue; }
    Q_INVOKABLE QString queueTitle(int i) const
    {
        return (i >= 0 && i < m_queue.size()) ? m_queue[i].displayTitle() : QString();
    }
    // Adds to the end of the queue (starts playing if nothing is queued).
    void enqueue(const QList<Track> &tracks);
    // Inserts right after the current track (after the one already lined up for gapless).
    void playNext(const QList<Track> &tracks);
    Q_INVOKABLE bool moveQueueItem(int from, int to);
    Q_INVOKABLE bool removeFromQueue(int index);      // not the current track
    Q_INVOKABLE void cycleRepeat() { setRepeatMode((repeatMode() + 1) % 3); }
    Q_INVOKABLE void playQueueIndex(int index) { startAt(index); }

    // Two songs that should join without a fade: consecutive tracks of one album.
    static bool continuesAlbum(const Track &a, const Track &b);

public slots:
    void play();
    void pause();
    void toggle();
    void stop();
    void next();
    void previous();          // restarts the track if more than 3 s in
    void seek(qint64 ms);

signals:
    void playingChanged();
    void currentChanged();
    void trackStarted();             // a track began playing (also a repeat of the same one)
    void queueChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void errorChanged();
    void seeked(qint64 ms);          // explicit seek (not normal progress)
    void repeatModeChanged();
    void shuffleChanged();
    void crossfadeChanged();
    void fadingChanged();
    void replayGainChanged();

private:
    static void onAboutToFinish(GstElement *playbin, void *self);
    GstElement *makeBin();
    GstElement *makeGainFilter() const;
    void applyGainFilter(GstElement *bin);
    int nextIndexLocked(bool userSkip) const;
    bool wantsFadeLocked(int from, int to) const;
    void startAt(int index);
    void startFade(int next);
    void fadeStep();
    void finishFade();
    void pollBus();
    void updatePosition();
    void setPlaying(bool p);
    void setError(const QString &e);
    void syncQueueModel();

    QByteArray m_factory;
    GstElement *m_playbin = nullptr;
    GstBus *m_bus = nullptr;
    GstElement *m_outBin = nullptr;  // the song fading out during a crossfade
    GstBus *m_outBus = nullptr;
    QTimer m_busTimer;
    QTimer m_posTimer;
    QTimer m_fadeTimer;
    QElapsedTimer m_fadeClock;
    qint64 m_fadeLen = 0;

    QList<Track> m_queue;
    QList<Track> m_unshuffled;       // original order while shuffle is on
    TrackListModel m_queueModel;
    int m_index = -1;
    mutable QMutex m_gaplessMutex;   // guards queue, index and bins against the streaming thread
    int m_pendingIndex = -1;         // set in about-to-finish, applied at STREAM_START

    std::atomic<bool> m_switching{false};   // a state change is in progress: about-to-finish keeps out
    std::atomic<int> m_repeat{RepeatOff};
    std::atomic<int> m_crossfadeMs{0};
    bool m_shuffle = false;
    int m_rgMode = GainOff;
    double m_rgPreamp = 0.0;

    bool m_playing = false;
    qint64 m_startSeekMs = 0;   // pending resume position for playFrom()
    qint64 m_position = 0;
    qint64 m_duration = 0;
    double m_volume = 1.0;
    QString m_error;
};

} // namespace hop
