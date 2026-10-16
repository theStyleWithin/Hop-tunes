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
// NOTE: this class could not be visually verified on real GPU hardware or against a real
// display in the sandbox this was written in — only that it builds and that the
// decode/position/subtitle logic (MpvCore, exercised headless with vo=null) is correct.
// The render-thread glue below follows mpv's documented render API usage exactly, but
// treat it as unverified until it's been seen actually painting a frame on screen.
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
    void setResumePosition(qint64 ms) { m_pendingResumeMs = ms; }

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
