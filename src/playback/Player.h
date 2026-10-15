// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library/Track.h"

#include <QList>
#include <QMutex>
#include <QObject>
#include <QTimer>

typedef struct _GstElement GstElement;
typedef struct _GstBus GstBus;

namespace hop {

// Music playback on GStreamer playbin3 (spec §5). Gapless: when the current track is
// about to end, playbin's "about-to-finish" hands it the next URI, so tracks join
// sample-accurately with no gap.
class Player : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool playing READ isPlaying NOTIFY playingChanged)
    Q_PROPERTY(bool hasTrack READ hasTrack NOTIFY currentChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentChanged)
    Q_PROPERTY(int queueLength READ queueLength NOTIFY queueChanged)
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

public:
    explicit Player(QObject *parent = nullptr);
    ~Player() override;

    bool isPlaying() const { return m_playing; }
    bool hasTrack() const { return m_index >= 0 && m_index < m_queue.size(); }
    int currentIndex() const { return m_index; }
    int queueLength() const { return m_queue.size(); }
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

    // Replaces the queue and starts playing at `start`.
    void setQueue(const QList<Track> &tracks, int start);
    QList<Track> queue() const { return m_queue; }
    Q_INVOKABLE QString queueTitle(int i) const
    {
        return (i >= 0 && i < m_queue.size()) ? m_queue[i].displayTitle() : QString();
    }

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
    void queueChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void errorChanged();
    void seeked(qint64 ms);          // explicit seek (not normal progress)

private:
    static void onAboutToFinish(GstElement *playbin, void *self);
    void startAt(int index);
    void pollBus();
    void updatePosition();
    void setPlaying(bool p);
    void setError(const QString &e);

    GstElement *m_playbin = nullptr;
    GstBus *m_bus = nullptr;
    QTimer m_busTimer;
    QTimer m_posTimer;

    QList<Track> m_queue;
    int m_index = -1;
    mutable QMutex m_gaplessMutex;   // guards m_pendingIndex and queue reads from the streaming thread
    int m_pendingIndex = -1;         // set in about-to-finish, applied at STREAM_START

    bool m_playing = false;
    qint64 m_position = 0;
    qint64 m_duration = 0;
    double m_volume = 1.0;
    QString m_error;
};

} // namespace hop
