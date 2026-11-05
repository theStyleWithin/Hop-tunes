// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/MusyNote.h"

#include <algorithm>
#include <cmath>

namespace hop {

namespace {
constexpr double kGravity = 24.0;     // units / s^2
constexpr double kFlapImpulse = -7.8; // units / s
constexpr double kMaxFall = 11.0;
}

void MusyNote::reset(quint32 seed)
{
    m_state = Ready;
    m_y = kH / 2; m_vy = 0;
    m_pitch = 0.5;
    m_speed = 4.0; m_bpm = 110; m_time = 0; m_lastArrival = -100; m_lastGap = kH / 2;
    m_lastBeatTime = 0;
    m_walls.clear();
    m_score = m_passed = m_flaps = 0;
    m_nextId = 1;
    m_seed = seed ? seed : 1;
}

quint32 MusyNote::rnd()
{
    m_seed ^= m_seed << 13; m_seed ^= m_seed >> 17; m_seed ^= m_seed << 5;   // xorshift32
    return m_seed;
}

double MusyNote::gapSize() const { return qMax(2.35, 3.3 - 0.12 * (level() - 1)); }

double MusyNote::scrollSpeed(double bpm) const
{
    // the tempo gives the pace (slow songs are calm), each level adds a little
    return qBound(3.4, 3.4 + (bpm - 80.0) * 0.022, 5.6) + 0.12 * (level() - 1);
}

void MusyNote::flap()
{
    if (m_state == Dead) return;
    if (m_state == Ready) m_state = Playing;
    m_vy = kFlapImpulse;
    ++m_flaps;
}

void MusyNote::beat(double)
{
    if (m_state != Playing) return;
    m_lastBeatTime = m_time;
    const double bi = 60.0 / qBound(60.0, m_bpm, 180.0);
    const double v = scrollSpeed(m_bpm);
    // the first beat-multiple that puts the new wall just off the right edge: it arrives at the note on a beat
    const double travelOffscreen = (kW - kNoteX + 0.2) / v;
    const double k = std::ceil(travelOffscreen / bi);
    const double arrival = m_time + k * bi;
    if (arrival - m_lastArrival < kMinSpacing) return;      // too soon after the last wall: wait for a later beat
    spawn(arrival);
}

void MusyNote::spawn(double arrivalSec)
{
    const double v = scrollSpeed(m_bpm);
    const double half = gapSize() / 2.0;
    const double lo = half + 0.5, hi = kH - half - 0.5;
    // pitch -> gap centre (high sound = high gap), then limited to a step the player can fly
    double centre = hi - (hi - lo) * m_pitch;
    centre = qBound(m_lastGap - kMaxGapStep, centre, m_lastGap + kMaxGapStep);
    centre = qBound(lo, centre, hi);
    Wall w;
    w.id = m_nextId++;
    w.x = kNoteX + v * (arrivalSec - m_time) - kWallW / 2.0;   // its middle reaches the note at arrivalSec
    w.gapTop = centre - half;
    w.gapBottom = centre + half;
    const int topCells = int(std::ceil(w.gapTop)), bottomCells = int(std::ceil(kH - w.gapBottom));
    for (int i = 0; i < topCells; ++i) w.topArts.append(int(rnd() % quint32(m_artCount)));
    for (int i = 0; i < bottomCells; ++i) w.bottomArts.append(int(rnd() % quint32(m_artCount)));
    m_walls.append(w);
    m_lastArrival = arrivalSec;
    m_lastGap = centre;
}

void MusyNote::debugSpawn(double centre)
{
    const double half = gapSize() / 2.0;
    Wall w;
    w.id = m_nextId++;
    w.x = kW;
    w.gapTop = centre - half;
    w.gapBottom = centre + half;
    m_walls.append(w);
    m_lastGap = centre;
}

void MusyNote::collide()
{
    if (m_y + kNoteR >= kH) { m_state = Dead; m_y = kH - kNoteR; return; }   // the floor
    if (m_y - kNoteR < 0) { m_y = kNoteR; if (m_vy < 0) m_vy = 0; }            // the ceiling only stops you
    for (const Wall &w : m_walls) {
        const double l = w.x, r = w.x + kWallW;
        if (kNoteX + kNoteR < l || kNoteX - kNoteR > r) continue;
        // the note is a circle: it hits the wall if its centre is within a radius of a solid part
        auto hit = [&](double top, double bottom) {
            const double cx = qBound(l, kNoteX, r), cy = qBound(top, m_y, bottom);
            const double dx = kNoteX - cx, dy = m_y - cy;
            return dx * dx + dy * dy < kNoteR * kNoteR;
        };
        if (hit(-1.0, w.gapTop) || hit(w.gapBottom, kH + 1.0)) { m_state = Dead; return; }
    }
}

void MusyNote::tick(double dt, double bpm)
{
    m_bpm = qBound(60.0, bpm, 180.0);
    if (m_state == Ready) {   // the note bobs on the spot until the first flap
        m_y = kH / 2 + 0.25 * std::sin(m_time * 3.0);
        m_time += dt;
        return;
    }
    if (m_state != Playing) return;
    m_time += dt;
    m_speed = scrollSpeed(m_bpm);
    m_vy = qMin(kMaxFall, m_vy + kGravity * dt);
    m_y += m_vy * dt;
    for (Wall &w : m_walls) {
        w.x -= m_speed * dt;
        if (!w.passed && w.x + kWallW < kNoteX - kNoteR) {
            w.passed = true;
            ++m_passed;
            // through the middle third of the gap pays double
            const double mid = (w.gapTop + w.gapBottom) / 2.0, third = (w.gapBottom - w.gapTop) / 6.0;
            m_score += qAbs(m_y - mid) <= third ? 2 : 1;
        }
    }
    while (!m_walls.isEmpty() && m_walls.first().x + kWallW < -1.0) m_walls.removeFirst();
    collide();
}

} // namespace hop
