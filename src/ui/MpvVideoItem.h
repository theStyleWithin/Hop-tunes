// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "playback/MpvCore.h"

#include <QQuickFramebufferObject>
#include <QVariantList>

struct mpv_render_context;

namespace hop {

// The QML-visible video surface (spec §5b): a QQuickFramebufferObject that owns an
// MpvCore and drives libmpv's OpenGL render API into the item's FBO. hwdec is requested
// as "auto-safe" (hardware decode with a safe software fallback) as soon as the mpv core
// exists — before init(), as the spec's performance targets ask for.
//
// NOTE: verified end to end against a real (software/llvmpipe) OpenGL context under Xvfb —
// opening a real video through the real App + QML engine actually decodes and paints frames
// into the item's FBO (confirmed by grabbing the window and sampling real pixel variation,
// not a solid colour). That caught a real bug: setResumePosition() was missing Q_INVOKABLE,
// so QML's call to it threw and silently aborted onSurfaceLoaded() before the video ever
// loaded — the actual cause of video never playing, fixed below. Still not confirmed against
// real GPU hardware/hwdec (this sandbox has none), so hwdec-specific issues could remain;
// treat the render-thread glue as verified for the software-decode path only.
class MpvVideoItem : public QQuickFramebufferObject {
    Q_OBJECT
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
    Q_PROPERTY(bool idle READ idle NOTIFY idleChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(QString hwdecActive READ hwdecActive NOTIFY hwdecActiveChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QVariantList subtitleTracks READ subtitleTracks NOTIFY subtitleTracksChanged)

public:
    explicit MpvVideoItem(QQuickItem *parent = nullptr);
    ~MpvVideoItem() override;

    Renderer *createRenderer() const override;

    QString source() const { return m_source; }
    void setSource(const QString &path);   // local file path; "" stops and unloads
    bool paused() const { return m_core.paused(); }
    bool idle() const { return m_core.idle(); }
    qint64 position() const { return m_core.position(); }
    qint64 duration() const { return m_core.duration(); }
    int volume() const { return m_core.volume(); }
    QString hwdecActive() const { return m_core.hwdecActive(); }
    QString error() const { return m_core.error(); }
    QVariantList subtitleTracks() const { return m_core.subtitleTracks(); }
    void setVolume(int percent) { m_core.setVolume(percent); emit volumeChanged(); }

    // Resumes from this position (spec §5b "Resume") the next time setSource() loads a file.
    // Must be Q_INVOKABLE: VideoPlayerView.qml calls this on the surface from QML, and a
    // plain C++ method (as this was) is invisible to QML's meta-object system — calling it
    // threw "is not a function", aborting onSurfaceLoaded() before it ever set item.source
    // or called item.play(). That's the actual root cause of video "silently" never playing
    // on the libmpv path: no error ever reached the UI because it was a QML console warning,
    // not something routed through surface.error.
    Q_INVOKABLE void setResumePosition(qint64 ms) { m_pendingResumeMs = ms; }

    Q_INVOKABLE void play() { m_core.play(); }
    Q_INVOKABLE void pause() { m_core.pause(); }
    Q_INVOKABLE void togglePause() { m_core.togglePause(); }
    Q_INVOKABLE void seek(qint64 ms) { m_core.seek(ms); }
    Q_INVOKABLE void stop() { m_core.stop(); }
    Q_INVOKABLE void addSubtitleFile(const QString &path) { m_core.addSubtitleFile(path); }
    Q_INVOKABLE void setSubtitleTrack(int id) { m_core.setSubtitleTrack(id); }

    // Used only by the Renderer (render thread) to reach the mpv handle.
    MpvCore *core() { return &m_core; }

signals:
    void sourceChanged();
    void pausedChanged();
    void idleChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void hwdecActiveChanged();
    void errorChanged();
    void subtitleTracksChanged();

private:
    class FboRenderer;

    MpvCore m_core;
    QString m_source;
    qint64 m_pendingResumeMs = 0;
};

// One-time registration for both the real app and the in-process QML UI tests
// (main.cpp and test_ui.cpp each call this before loading any QML).
void registerVideoQmlTypes();

} // namespace hop
