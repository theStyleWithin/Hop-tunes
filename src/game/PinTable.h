// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QColor>
#include <QImage>
#include <QLineF>
#include <QPointF>
#include <QVector>
#include <QtGlobal>

namespace hop {

// The rules of Cover Pinball: a pinball table whose bumpers sit where the album cover is brightest. No Qt Quick and no
// audio in here (deterministic, unit-tested); the screen (CoverPinball.qml), the beat and the controller plug in from outside.
// Everything here is our own small design: one ball, round bumpers, two flippers and a plunger lane; the physics is a
// plain circle-against-segment solver in fixed sub-steps.
//
// World: 10 units wide, 17 tall, y grows downward, gravity pulls toward +y. The plunger lane is the narrow strip on the right.
//  * Each bumper pays 100; a hit within 0.12 s of a beat pays double; a bumper that is pulsing (a beat just landed) kicks harder.
//  * Lighting every bumper at least once pays 500 and starts the round again.
//  * Three balls. A ball that drains takes one; the lane waits for the next launch.
//  * The bass of the song nudges gravity a little (heavier on loud bass), never enough to make the table unplayable.
class PinTable {
public:
    static constexpr double kW = 10.0, kH = 17.0;
    static constexpr double kBallR = 0.28;
    static constexpr double kLaneX = 9.15;                    // the lane lies between this and the right wall
    static constexpr int kBalls = 3;
    static constexpr double kFlipLen = 1.6, kFlipR = 0.2, kFlipPy = 14.6, kLeftPx = 2.6, kRightPx = 6.55;   // the flippers (pivot and size)
    enum State { Ready, Playing, Over };

    struct Bumper {
        double x = 0, y = 0, r = 0.6;
        QColor color;                 // taken from the cover where the bumper sits
        double pulse = 0;             // 1 right after a beat or a hit, fading out
        bool lit = false;             // hit at least once this round
    };

    // The bumpers a cover gives: the brightest, most colourful places of the picture, spaced apart; the same
    // picture always gives the same table. A null image gives a plain default layout.
    static QVector<Bumper> layoutFromImage(const QImage &cover);

    explicit PinTable(quint32 seed = 1) { reset(QVector<Bumper>(), seed); }
    void reset(const QVector<Bumper> &layout, quint32 seed);

    // ---- input ----
    void launch(double power01);                // in Ready: the ball leaves the lane
    void flip(bool left, bool up);              // hold a flipper up / let it down

    // ---- music ----
    void beat(double strength);                 // every bumper pulses; a hit in the next 0.12 s is "on the beat"
    void setBass(double b01) { m_bass = qBound(0.0, b01, 1.0); }
    void tick(double dt);                       // fixed sub-steps inside

    // ---- reading ----
    State state() const { return m_state; }
    int score() const { return m_score; }
    int ballsLeft() const { return m_balls; }
    int hits() const { return m_hits; }                       // counts up on every bumper hit
    bool lastHitOnBeat() const { return m_lastOnBeat; }
    int lastBumper() const { return m_lastBumper; }
    int rounds() const { return m_rounds; }                   // all bumpers lit this many times
    QPointF ball() const { return m_p; }
    QPointF ballVelocity() const { return m_v; }
    bool ballInLane() const { return m_p.x() > kLaneX && m_p.y() > 9.0; }
    const QVector<Bumper> &bumpers() const { return m_bumpers; }
    QVector<QLineF> wallLines() const { QVector<QLineF> o; for (const Segment &g : m_walls) o.append(QLineF(g.a, g.b)); return o; }
    double flipperAngle(bool left) const { return left ? m_fl.angle : m_fr.angle; }
    double time() const { return m_time; }
    // for tests: put the ball somewhere with a speed, in play
    void placeBall(QPointF p, QPointF v) { m_p = p; m_v = v; m_state = Playing; }
    // for the autopilot
    bool ballNearFlipper(bool left) const;

private:
    struct Flipper { double px = 0, py = 0, angle = 0, speed = 0; bool up = false; double side = 1; };
    struct Segment { QPointF a, b; double bounce; };
    void step(double dt);
    void collideSegments();
    void collideBumpers();
    void collideFlipper(const Flipper &f);
    void bounceOff(const QPointF &n, double pen, double bounce, const QPointF &surf);
    void parkBall();
    void lose();
    static QPointF flipperTip(const Flipper &f);

    QVector<Segment> m_walls;
    QVector<Bumper> m_bumpers;
    Flipper m_fl, m_fr;
    QPointF m_p, m_v;
    State m_state = Ready;
    int m_score = 0, m_balls = kBalls, m_hits = 0, m_lastBumper = -1, m_rounds = 0;
    bool m_lastOnBeat = false;
    double m_time = 0, m_lastBeat = -10, m_bass = 0.5, m_slow = 0;
    quint32 m_seed = 1;
};

} // namespace hop
