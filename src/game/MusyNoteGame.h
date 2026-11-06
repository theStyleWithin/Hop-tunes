// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "game/MusyNote.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantList>

namespace hop {

// Musy Note for the screen: MusyNote's rules plus a 60 Hz clock, the music and the input. MusyNote.qml only draws
// what this exposes. Tap, key and controller all come in through flap().
class MusyNoteGame : public QObject {
    Q_OBJECT
    Q_PROPERTY(int state READ state NOTIFY stateChanged)          // 0 ready, 1 playing, 2 over
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(bool autoplay READ autoplay WRITE setAutoplay NOTIFY autoplayChanged)   // the song flies the note: no score is kept
    Q_PROPERTY(bool scored READ scored NOTIFY stateChanged)       // this run counts for the best list (never true after autoplay)
    Q_PROPERTY(int frame READ frame NOTIFY frameChanged)          // bumps every tick: bindings that read the note or a wall depend on it
    Q_PROPERTY(double noteY READ noteY NOTIFY frameChanged)
    Q_PROPERTY(double noteVy READ noteVy NOTIFY frameChanged)
    Q_PROPERTY(QVariantList wallIds READ wallIds NOTIFY wallsChanged)   // changes only when a wall appears or goes
    Q_PROPERTY(int score READ score NOTIFY frameChanged)
    Q_PROPERTY(int level READ level NOTIFY frameChanged)
    Q_PROPERTY(int beatFlash READ beatFlash NOTIFY beatFlashChanged)
    Q_PROPERTY(bool pulse READ pulse NOTIFY pulseChanged)         // the game keeps its own pulse (no beats heard)
    Q_PROPERTY(double worldW READ worldW CONSTANT)
    Q_PROPERTY(double worldH READ worldH CONSTANT)
    Q_PROPERTY(double noteX READ noteX CONSTANT)
    Q_PROPERTY(double noteR READ noteR CONSTANT)
    Q_PROPERTY(double wallW READ wallW CONSTANT)
public:
    explicit MusyNoteGame(QObject *parent = nullptr);

    int state() const { return int(m_g.state()); }
    bool paused() const { return m_paused; }
    void setPaused(bool p);
    bool autoplay() const { return m_autoplay; }
    void setAutoplay(bool on);
    bool scored() const { return !m_autoUsed; }
    int frame() const { return m_frame; }
    double noteY() const { return m_g.noteY(); }
    double noteVy() const { return m_g.noteVy(); }
    QVariantList wallIds() const { return m_ids; }
    int score() const { return m_g.score(); }
    int level() const { return m_g.level(); }
    int beatFlash() const { return m_beatFlash; }
    bool pulse() const { return m_pulse; }
    static double worldW() { return MusyNote::kW; }
    static double worldH() { return MusyNote::kH; }
    static double noteX() { return MusyNote::kNoteX; }
    static double noteR() { return MusyNote::kNoteR; }
    static double wallW() { return MusyNote::kWallW; }

    Q_INVOKABLE void start(int artCount);
    Q_INVOKABLE void flap();
    // one wall by id: x, gapTop, gapBottom and a cover number for each block of its top and bottom part
    Q_INVOKABLE double wallX(int id) const;
    Q_INVOKABLE double wallGapTop(int id) const;
    Q_INVOKABLE double wallGapBottom(int id) const;
    Q_INVOKABLE QVariantList wallTopArts(int id) const;
    Q_INVOKABLE QVariantList wallBottomArts(int id) const;

    // the music
    Q_INVOKABLE void setMusic(double bpm, double pitch01) { m_bpm = bpm; m_g.setPitch(pitch01); }
    Q_INVOKABLE void onBeat(double strength);
    Q_INVOKABLE void setPlaying(bool playing) { m_playing = playing; }

    // for tests: run n ticks of dt seconds without the timer
    Q_INVOKABLE void advance(int ticks, double dt);

signals:
    void stateChanged();
    void pausedChanged();
    void autoplayChanged();
    void frameChanged();
    void wallsChanged();
    void beatFlashChanged();
    void pulseChanged();
    void finished(int score, int passed, bool scored);   // the note crashed; `scored` is false after autoplay

private:
    void step();
    void tickOnce(double dt, qint64 nowMs);
    void pilot();
    const MusyNote::Wall *find(int id) const;

    MusyNote m_g;
    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_last = 0, m_lastBeat = 0;
    QVariantList m_ids;
    bool m_started = false, m_paused = false, m_playing = false, m_pulse = false, m_wasDead = false;
    bool m_autoplay = false, m_autoUsed = false;
    double m_bpm = 110;
    int m_frame = 0, m_beatFlash = 0, m_art = 1;
};

} // namespace hop
