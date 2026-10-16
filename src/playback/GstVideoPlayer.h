// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

class QQuickItem;
typedef struct _GstElement GstElement;
typedef struct _GstBus GstBus;

namespace hop {

// Video playback on GStreamer's playbin3 — the fallback engine for builds without libmpv
// (currently: Flatpak, see CMakeLists.txt's HOPTUNES_HAVE_MPV and that manifest's
// comment). Renders through gst-plugins-good's Qt6 QML sink (qml6glsink, package
// gstreamer1.0-qt6) and gst-plugins-base's glsinkbin (package gstreamer1.0-gl) into a
// GstGLQt6VideoItem the QML side creates (see GstVideoSurfaceView.qml) — both are
// ordinary GStreamer plugins, already the kind of dependency this app has everywhere, so
// nothing new needs packaging for Flatpak the way libmpv would. Verified end to end in
// the sandbox this was built in, including real on-screen (software GL) rendering — see
// the commit message for how, which is more than MpvVideoItem's render path could claim.
class GstVideoPlayer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
    Q_PROPERTY(bool idle READ idle NOTIFY idleChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(QString hwdecActive READ hwdecActive NOTIFY hwdecActiveChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QVariantList subtitleTracks READ subtitleTracks NOTIFY subtitleTracksChanged)

public:
    explicit GstVideoPlayer(QObject *parent = nullptr);
    ~GstVideoPlayer() override;
    GstVideoPlayer(const GstVideoPlayer &) = delete;
    GstVideoPlayer &operator=(const GstVideoPlayer &) = delete;

    // True if the plugins this engine needs (qml6glsink, glsinkbin) are installed.
    static bool available();

    // Called once by GstVideoSurfaceView.qml when its GstGLQt6VideoItem is ready; builds
    // the pipeline the first time, no-ops after (one surface per player, like MpvVideoItem).
    Q_INVOKABLE void attachSurface(QQuickItem *item);
    Q_INVOKABLE bool openSource(const QString &path, qint64 startPositionMs = 0);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void seek(qint64 ms);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void setSubtitleTrack(int id);   // -1 disables subtitles

    bool paused() const { return m_paused; }
    bool idle() const { return m_idle; }
    qint64 position() const { return m_position; }
    qint64 duration() const { return m_duration; }
    int volume() const { return m_volume; }
    void setVolume(int percent);
    QString hwdecActive() const { return m_hwdecActive; }
    QString error() const { return m_error; }
    QVariantList subtitleTracks() const { return m_subtitleTracks; }

signals:
    void pausedChanged();
    void idleChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void hwdecActiveChanged();
    void errorChanged();
    void subtitleTracksChanged();
    void fileLoaded();
    void endOfFile();

private:
    void pollBus();
    void updatePosition();
    void refreshSubtitleTracks();
    void detectHwdec();
    void setError(const QString &e);
    void setPaused(bool p);

    GstElement *m_pipeline = nullptr;
    GstBus *m_bus = nullptr;
    QQuickItem *m_surface = nullptr;
    QTimer m_busTimer;
    QTimer m_posTimer;

    bool m_paused = true;
    bool m_idle = true;
    qint64 m_position = 0;
    qint64 m_duration = 0;
    qint64 m_pendingSeekMs = 0;
    int m_volume = 100;
    QString m_hwdecActive;
    QString m_error;
    QVariantList m_subtitleTracks;
};

} // namespace hop
