// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

#include <cstdint>

struct mpv_handle;
struct mpv_event;

namespace hop {

// A thin RAII wrapper around libmpv's client API (spec §5b: video engine). Deliberately
// has no rendering code at all — that lives in MpvVideoItem, which owns one of these and
// adds the OpenGL render context on top. Kept separate so the player logic (load, play,
// pause, seek, subtitles, resume) is unit-testable headless (vo=null), independent of
// Qt Quick's scenegraph, which this sandbox cannot exercise visually.
class MpvCore : public QObject {
    Q_OBJECT
public:
    explicit MpvCore(QObject *parent = nullptr);
    ~MpvCore() override;
    MpvCore(const MpvCore &) = delete;
    MpvCore &operator=(const MpvCore &) = delete;

    // Must be called before init(); e.g. ("vo","null")/("ao","null") for headless tests,
    // ("hwdec","auto-safe") always (spec §5b performance targets).
    void setOption(const QString &name, const QString &value);
    bool init();   // mpv_create + mpv_initialize; false + error() set on failure
    mpv_handle *handle() const { return m_mpv; }

    bool open(const QString &path, qint64 startPositionMs = 0);
    void play();
    void pause();
    void togglePause();
    void seek(qint64 ms);     // absolute, clamped to [0, duration]
    void stop();
    void setVolume(int percent);   // 0-100
    void addSubtitleFile(const QString &path);   // "sub-add", select it
    void setSubtitleTrack(int id);                // mpv "sid"; -1 disables subtitles

    bool paused() const { return m_paused; }
    bool idle() const { return m_idle; }
    qint64 position() const { return m_positionMs; }
    qint64 duration() const { return m_durationMs; }
    int volume() const { return m_volume; }
    QString hwdecActive() const { return m_hwdecCurrent; }
    QString error() const { return m_error; }
    // Each entry: {id, lang, title, selected}.
    QVariantList subtitleTracks() const { return m_subtitleTracks; }

signals:
    void positionChanged();
    void durationChanged();
    void pausedChanged();
    void fileLoaded();
    void endOfFile();
    void subtitleTracksChanged();
    void errorChanged();

private:
    Q_INVOKABLE void processEvents();   // invoked (queued) from mpv's wakeup callback
    void handleEvent(const mpv_event *ev);
    void refreshTrackList();
    void setError(const QString &e);

    mpv_handle *m_mpv = nullptr;
    bool m_paused = true;
    bool m_idle = true;
    qint64 m_positionMs = 0;
    qint64 m_durationMs = 0;
    qint64 m_pendingSeekMs = 0;
    int m_volume = 100;
    QString m_hwdecCurrent;
    QString m_error;
    QVariantList m_subtitleTracks;

    static void wakeupTrampoline(void *ctx);
};

} // namespace hop
