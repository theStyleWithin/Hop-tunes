// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/PinTable.h"

#include <QtMath>
#include <algorithm>

namespace hop {

namespace {
constexpr double kStep = 1.0 / 240.0;
constexpr double kGravity = 11.0;
constexpr double kMaxSpeed = 26.0;
constexpr double kFlipLen = PinTable::kFlipLen, kFlipR = PinTable::kFlipR, kFlipRest = 0.49, kFlipUp = -0.49, kFlipSpeed = 20.0;
constexpr double kBeatWindow = 0.12;
constexpr double kLaneStartY = 16.2;
inline double dot(const QPointF &a, const QPointF &b) { return a.x() * b.x() + a.y() * b.y(); }
}

QPointF PinTable::flipperTip(const Flipper &f)
{
    return QPointF(f.px + f.side * kFlipLen * qCos(f.angle), f.py + kFlipLen * qSin(f.angle));
}

// ---- the table from a cover ----------------------------------------------------------------------------

QVector<PinTable::Bumper> PinTable::layoutFromImage(const QImage &cover)
{
    // places a bumper may sit: clear of the walls, the lane, the top corners and the flippers
    const double x0 = 1.3, x1 = 8.0, y0 = 2.7, y1 = 10.4, minGap = 2.3;
    QVector<Bumper> out;
    auto add = [&](double x, double y, double r, QColor c) { Bumper b; b.x = x; b.y = y; b.r = r; b.color = c; out.append(b); };
    static const double defs[6][3] = {{3.2, 4.2, 0.7}, {6.2, 4.2, 0.7}, {4.7, 6.6, 0.75}, {2.4, 8.2, 0.55}, {7.0, 8.2, 0.55}, {4.7, 9.9, 0.55}};
    if (!cover.isNull()) {
        // the cover fills the table (centre-cropped to its shape); one cell is half a unit
        const int gw = 20, gh = 34;
        QImage g = cover.convertToFormat(QImage::Format_RGB32).scaled(gw, gh, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        g = g.copy((g.width() - gw) / 2, (g.height() - gh) / 2, gw, gh);
        struct Cell { double score; int i, j; QColor c; };
        QVector<Cell> cells;
        for (int j = 0; j < gh; ++j)
            for (int i = 0; i < gw; ++i) {
                const double x = (i + 0.5) * kW / gw, y = (j + 0.5) * kH / gh;
                if (x < x0 || x > x1 || y < y0 || y > y1) continue;
                const QColor c = g.pixelColor(i, j);
                cells.append({0.6 * c.valueF() + 0.4 * c.saturationF() + 0.0001 * (1.0 - double(j * gw + i) / (gw * gh)), i, j, c});
            }
        std::stable_sort(cells.begin(), cells.end(), [](const Cell &a, const Cell &b) { return a.score > b.score; });
        double sum = 0;
        for (const Cell &c : cells) sum += c.score;
        const double mean = cells.isEmpty() ? 0 : sum / cells.size();
        for (const Cell &c : cells) {
            if (out.size() >= 7) break;
            const double x = (c.i + 0.5) * kW / gw, y = (c.j + 0.5) * kH / gh;
            bool clear = true;
            for (const Bumper &b : out) if (qHypot(b.x - x, b.y - y) < minGap) { clear = false; break; }
            if (!clear) continue;
            QColor col = QColor::fromHsvF(qMax(0.0, c.c.hsvHueF()), qBound(0.45, c.c.saturationF() + 0.2, 1.0), 0.95);
            add(x, y, c.score >= mean ? 0.75 : 0.55, col);
        }
    }
    for (int k = 0; out.size() < 5 && k < 6; ++k) {      // a flat or tiny picture: fill up from the default table
        bool clear = true;
        for (const Bumper &b : out) if (qHypot(b.x - defs[k][0], b.y - defs[k][1]) < minGap) { clear = false; break; }
        if (clear) add(defs[k][0], defs[k][1], defs[k][2], QColor::fromHsvF(k / 6.0, 0.6, 0.95));
    }
    return out;
}

// ---- the table ---------------------------------------------------------------------------------------

void PinTable::reset(const QVector<Bumper> &layout, quint32 seed)
{
    m_seed = seed ? seed : 1;
    m_bumpers = layout.isEmpty() ? layoutFromImage(QImage()) : layout;
    for (Bumper &b : m_bumpers) { b.pulse = 0; b.lit = false; }
    m_walls.clear();
    const double wb = 0.45, sb = 0.55;
    auto seg = [&](double ax, double ay, double bx, double by, double bounce) { m_walls.append({QPointF(ax, ay), QPointF(bx, by), bounce}); };
    seg(0, 17.5, 0, 2.5, wb);            // left wall
    seg(0, 2.5, 2.6, 0, wb);             // top left corner
    seg(2.6, 0, 7.4, 0, wb);             // top
    seg(7.4, 0, kW, 3.0, wb);            // top right corner: the plunger lane throws the ball into the table from here
    seg(kW, 3.0, kW, 17.5, wb);          // right wall
    seg(kLaneX, 3.9, kLaneX, 16.6, wb);  // the lane wall
    seg(kLaneX, 16.6, kW, 16.6, wb);     // the lane floor
    seg(0, 12.4, 2.5, 14.45, sb);        // the slopes down to the flippers
    seg(kLaneX, 12.4, 6.65, 14.45, sb);
    m_fl = Flipper{kLeftPx, kFlipPy, kFlipRest, 0, false, 1};
    m_fr = Flipper{kRightPx, kFlipPy, kFlipRest, 0, false, -1};
    m_score = 0; m_balls = kBalls; m_hits = 0; m_lastBumper = -1; m_rounds = 0; m_lastOnBeat = false;
    m_time = 0; m_lastBeat = -10; m_slow = 0;
    parkBall();
    m_state = Ready;
}

void PinTable::parkBall()
{
    m_p = QPointF((kLaneX + kW) / 2, kLaneStartY);
    m_v = QPointF(0, 0);
    m_slow = 0;
}

void PinTable::launch(double power01)
{
    if (m_state == Over) return;
    if (m_state == Playing && !(ballInLane() && m_v.y() > -2.0)) return;
    m_p.setX((kLaneX + kW) / 2);
    m_v = QPointF(0, -(18.6 + 3.0 * qBound(0.0, power01, 1.0)));
    m_state = Playing;
}

void PinTable::flip(bool left, bool up) { (left ? m_fl : m_fr).up = up; }

void PinTable::beat(double strength)
{
    if (m_state == Over) return;
    m_lastBeat = m_time;
    for (Bumper &b : m_bumpers) b.pulse = qMax(b.pulse, qBound(0.4, strength, 1.0));
}

bool PinTable::ballNearFlipper(bool left) const
{
    const Flipper &f = left ? m_fl : m_fr;
    const double dx = (m_p.x() - f.px) * f.side;
    return m_state == Playing && m_p.y() > f.py - 2.2 && m_p.y() < f.py + 0.6 && dx > -0.3 && dx < kFlipLen + 0.5;
}

void PinTable::tick(double dt)
{
    if (m_state == Over) return;
    double left = qMin(dt, 0.05);
    while (left > 1e-9) { const double h = qMin(kStep, left); step(h); left -= h; }
}

void PinTable::bounceOff(const QPointF &n, double pen, double bounce, const QPointF &surf)
{
    m_p += n * pen;
    const QPointF rel(m_v.x() - surf.x(), m_v.y() - surf.y());
    const double vn = dot(rel, n);
    if (vn < 0) m_v -= n * ((1.0 + bounce) * vn);
}

void PinTable::step(double dt)
{
    m_time += dt;
    for (Bumper &b : m_bumpers) b.pulse = qMax(0.0, b.pulse - dt * 4.0);
    for (Flipper *f : {&m_fl, &m_fr}) {
        const double target = f->up ? kFlipUp : kFlipRest;
        const double d = target - f->angle;
        const double move = qBound(-kFlipSpeed * dt, d, kFlipSpeed * dt);
        f->angle += move;
        f->speed = move / dt;
    }
    if (m_state == Ready) return;                                  // the ball waits in the lane
    const double g = kGravity * (0.9 + 0.3 * m_bass);
    m_v.ry() += g * dt;
    const double sp = qHypot(m_v.x(), m_v.y());
    if (sp > kMaxSpeed) m_v *= kMaxSpeed / sp;
    m_p += m_v * dt;
    collideSegments();
    collideBumpers();
    collideFlipper(m_fl);
    collideFlipper(m_fr);
    // a ball that has stopped on something it should not rest on gets a little push, so the game never stalls
    if (qHypot(m_v.x(), m_v.y()) < 0.25 && !ballInLane()) m_slow += dt; else m_slow = 0;
    if (m_slow > 2.5) {
        m_seed = m_seed * 1664525u + 1013904223u;
        m_v.rx() += ((m_seed >> 16) & 1) ? 1.8 : -1.8;
        m_v.ry() -= 1.0;
        m_slow = 0;
    }
    // back in the lane and nearly still: it waits for the next launch, no ball is lost
    if (ballInLane() && qHypot(m_v.x(), m_v.y()) < 0.6 && m_p.y() > 15.5) { parkBall(); m_state = Ready; }
    if (m_p.y() > kH + 0.6) lose();
    if (m_p.x() < -1 || m_p.x() > kW + 1 || m_p.y() < -1) { m_p = QPointF(kW / 2, 6); m_v = QPointF(0, 0); }   // safety net, never reached in normal play
}

void PinTable::lose()
{
    --m_balls;
    if (m_balls <= 0) { m_balls = 0; m_state = Over; return; }
    parkBall();
    m_state = Ready;
}

void PinTable::collideSegments()
{
    for (const Segment &s : m_walls) {
        const QPointF ab = s.b - s.a;
        const double len2 = dot(ab, ab);
        const double t = len2 > 0 ? qBound(0.0, dot(m_p - s.a, ab) / len2, 1.0) : 0.0;
        const QPointF c = s.a + ab * t;
        QPointF d = m_p - c;
        const double dist = qHypot(d.x(), d.y());
        if (dist >= kBallR || dist < 1e-9) continue;
        bounceOff(d / dist, kBallR - dist, s.bounce, QPointF(0, 0));
    }
}

void PinTable::collideBumpers()
{
    for (int i = 0; i < m_bumpers.size(); ++i) {
        Bumper &b = m_bumpers[i];
        QPointF d = m_p - QPointF(b.x, b.y);
        const double dist = qHypot(d.x(), d.y());
        const double reach = b.r + kBallR;
        if (dist >= reach || dist < 1e-9) continue;
        const QPointF n = d / dist;
        m_p += n * (reach - dist);
        const double vn = dot(m_v, n);
        const double kick = 5.5 + 3.5 * b.pulse;                    // a bumper that just pulsed throws harder
        if (vn < kick) m_v += n * (kick - vn);
        const bool onBeat = (m_time - m_lastBeat) <= kBeatWindow;
        m_score += onBeat ? 200 : 100;
        m_lastOnBeat = onBeat;
        m_lastBumper = i;
        ++m_hits;
        b.pulse = 1.0;
        b.lit = true;
        bool all = true;
        for (const Bumper &o : m_bumpers) if (!o.lit) { all = false; break; }
        if (all) { m_score += 500; ++m_rounds; for (Bumper &o : m_bumpers) o.lit = false; }
    }
}

void PinTable::collideFlipper(const Flipper &f)
{
    const QPointF a(f.px, f.py), b = flipperTip(f);
    const QPointF ab = b - a;
    const double t = qBound(0.0, dot(m_p - a, ab) / dot(ab, ab), 1.0);
    const QPointF c = a + ab * t;
    const QPointF d = m_p - c;
    const double dist = qHypot(d.x(), d.y());
    const double reach = kBallR + kFlipR;
    if (dist >= reach || dist < 1e-9) return;
    // the speed of that point of the flipper: along the swing, growing toward the tip
    const double along = t * kFlipLen;
    const QPointF surf(-f.side * qSin(f.angle) * f.speed * along, qCos(f.angle) * f.speed * along);
    bounceOff(d / dist, reach - dist, 0.45, surf);
}

} // namespace hop
