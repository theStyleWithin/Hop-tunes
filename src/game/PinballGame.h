// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "game/PinTable.h"

#include <QColor>
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantList>

namespace hop {

// Cover Pinball for the screen: PinTable's rules plus a 60 Hz clock, the music and the input. CoverPinball.qml only
// draws what this exposes. Keys, controller and the autopilot all come in through flip() and launch().
class PinballGame : public QObject {
    Q_OBJECT
    Q_PROPERTY(int state READ state NOTIFY stateChanged)          // 0 ready (ball in the lane), 1 playing, 2 over
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(bool autoplay READ autoplay WRITE setAutoplay NOTIFY autoplayChanged)   // the song plays the flippers: no score is kept
    Q_PROPERTY(bool scored READ scored NOTIFY stateChanged)       // this game counts for the best list (never true after autoplay)
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)     // a game is on (not over)
    Q_PROPERTY(int frame READ frame NOTIFY frameChanged)          // bumps every tick: bindings that read the ball depend on it
    Q_PROPERTY(double ballX READ ballX NOTIFY frameChanged)
    Q_PROPERTY(double ballY READ ballY NOTIFY frameChanged)
    Q_PROPERTY(double leftAngle READ leftAngle NOTIFY frameChanged)
    Q_PROPERTY(double rightAngle READ rightAngle NOTIFY frameChanged)
    Q_PROPERTY(int score READ score NOTIFY frameChanged)
    Q_PROPERTY(int ballsLeft READ ballsLeft NOTIFY frameChanged)
    Q_PROPERTY(int rounds READ rounds NOTIFY frameChanged)
    Q_PROPERTY(int hits READ hits NOTIFY frameChanged)
    Q_PROPERTY(bool lastHitOnBeat READ lastHitOnBeat NOTIFY frameChanged)
    Q_PROPERTY(int beatFlash READ beatFlash NOTIFY beatFlashChanged)
    Q_PROPERTY(bool pulse READ pulse NOTIFY pulseChanged)         // the game keeps its own pulse (no beats heard)
    Q_PROPERTY(int bumperCount READ bumperCount NOTIFY layoutChanged)
    Q_PROPERTY(double worldW READ worldW CONSTANT)
    Q_PROPERTY(double worldH READ worldH CONSTANT)
    Q_PROPERTY(double ballR READ ballR CONSTANT)
    Q_PROPERTY(double laneX READ laneX CONSTANT)
    Q_PROPERTY(double flipLen READ flipLen CONSTANT)
    Q_PROPERTY(double flipR READ flipR CONSTANT)
    Q_PROPERTY(double flipY READ flipY CONSTANT)
    Q_PROPERTY(double leftPivotX READ leftPivotX CONSTANT)
    Q_PROPERTY(double rightPivotX READ rightPivotX CONSTANT)
    Q_PROPERTY(QVariantList walls READ walls CONSTANT)     // [ax, ay, bx, by, ...] of every wall
public:
    explicit PinballGame(QObject *parent = nullptr);

    int state() const { return int(m_t.state()); }
    bool paused() const { return m_paused; }
    void setPaused(bool p);
    bool autoplay() const { return m_autoplay; }
    void setAutoplay(bool on);
    bool scored() const { return !m_autoUsed; }
    bool running() const { return m_started && m_t.state() != PinTable::Over; }
    int frame() const { return m_frame; }
    double ballX() const { return m_t.ball().x(); }
    double ballY() const { return m_t.ball().y(); }
    double leftAngle() const { return m_t.flipperAngle(true); }
    double rightAngle() const { return m_t.flipperAngle(false); }
    int score() const { return m_t.score(); }
    int ballsLeft() const { return m_t.ballsLeft(); }
    int rounds() const { return m_t.rounds(); }
    int hits() const { return m_t.hits(); }
    bool lastHitOnBeat() const { return m_t.lastHitOnBeat(); }
    int beatFlash() const { return m_beatFlash; }
    bool pulse() const { return m_pulse; }
    int bumperCount() const { return int(m_t.bumpers().size()); }
    static double worldW() { return PinTable::kW; }
    static double worldH() { return PinTable::kH; }
    static double ballR() { return PinTable::kBallR; }
    static double laneX() { return PinTable::kLaneX; }
    static double flipLen() { return PinTable::kFlipLen; }
    static double flipR() { return PinTable::kFlipR; }
    static double flipY() { return PinTable::kFlipPy; }
    static double leftPivotX() { return PinTable::kLeftPx; }
    static double rightPivotX() { return PinTable::kRightPx; }
    QVariantList walls() const { QVariantList o; for (const QLineF &l : m_t.wallLines()) o << l.x1() << l.y1() << l.x2() << l.y2(); return o; }

    // a fresh game on the table the cover gives (a file URL or path; empty or unreadable gives the plain table)
    Q_INVOKABLE void start(const QString &coverUrl);
    Q_INVOKABLE void launch();
    Q_INVOKABLE void flip(bool left, bool up);
    // one bumper by number: where it stands, how big, its colour, whether it is lit and how strongly it pulses now
    Q_INVOKABLE double bumperX(int i) const { return at(i).x; }
    Q_INVOKABLE double bumperY(int i) const { return at(i).y; }
    Q_INVOKABLE double bumperR(int i) const { return at(i).r; }
    Q_INVOKABLE QColor bumperColor(int i) const { return at(i).color; }
    Q_INVOKABLE bool bumperLit(int i) const { return at(i).lit; }
    Q_INVOKABLE double bumperPulse(int i) const { return at(i).pulse; }

    // the music
    Q_INVOKABLE void setMusic(double bpm, double bass01) { m_bpm = bpm; m_t.setBass(bass01); }
    Q_INVOKABLE void onBeat(double strength);
    Q_INVOKABLE void setPlaying(bool playing) { m_playing = playing; }

    // for tests: run n ticks of dt seconds without the timer
    Q_INVOKABLE void advance(int ticks, double dt);

signals:
    void stateChanged();
    void pausedChanged();
    void autoplayChanged();
    void frameChanged();
    void beatFlashChanged();
    void pulseChanged();
    void layoutChanged();
    void finished(int score, int rounds, bool scored);   // the last ball drained; `scored` is false after autoplay

private:
    void step();
    void tickOnce(double dt, qint64 nowMs);
    void pilot(double dt);
    const PinTable::Bumper &at(int i) const;

    PinTable m_t;
    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_last = 0, m_lastBeat = 0;
    bool m_started = false, m_paused = false, m_playing = false, m_pulse = false, m_wasOver = false;
    bool m_autoplay = false, m_autoUsed = false;
    double m_bpm = 110, m_autoT = 0;
    int m_frame = 0, m_beatFlash = 0;
};

} // namespace hop
