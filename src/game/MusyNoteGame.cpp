// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/MusyNoteGame.h"

#include <QRandomGenerator>

namespace hop {

MusyNoteGame::MusyNoteGame(QObject *parent) : QObject(parent)
{
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &MusyNoteGame::step);
    m_clock.start();
}

void MusyNoteGame::start(int artCount)
{
    m_art = qMax(1, artCount);
    m_g.reset(QRandomGenerator::global()->generate());
    m_g.setArtCount(m_art);
    m_started = true;
    m_paused = false;
    m_wasDead = false;
    m_autoUsed = m_autoplay;          // a run is only scored if the song never flew it
    m_pulse = false;
    m_ids.clear();
    m_last = m_clock.elapsed();
    m_lastBeat = m_last;
    ++m_frame;
    m_timer.start();
    emit stateChanged();
    emit pausedChanged();
    emit wallsChanged();
    emit frameChanged();
}

void MusyNoteGame::setPaused(bool p)
{
    if (p == m_paused) return;
    m_paused = p;
    m_last = m_clock.elapsed();
    emit pausedChanged();
}

void MusyNoteGame::setAutoplay(bool on)
{
    if (on == m_autoplay) return;
    m_autoplay = on;
    if (on && m_started && m_g.state() != MusyNote::Dead) { m_autoUsed = true; emit stateChanged(); }   // from now on this run keeps no score
    emit autoplayChanged();
}

void MusyNoteGame::flap()
{
    if (!m_started || m_paused || m_autoplay) return;       // the song has the controls while it plays for you
    if (m_g.state() == MusyNote::Dead) return;
    const bool begin = m_g.state() == MusyNote::Ready;
    m_g.flap();
    ++m_frame;
    if (begin) emit stateChanged();
    emit frameChanged();
}

void MusyNoteGame::onBeat(double strength)
{
    if (!m_started || m_paused || !m_playing) return;
    m_lastBeat = m_clock.elapsed();
    if (m_pulse) { m_pulse = false; emit pulseChanged(); }
    m_g.beat(strength);
    ++m_beatFlash;
    emit beatFlashChanged();
}

const MusyNote::Wall *MusyNoteGame::find(int id) const
{
    for (const MusyNote::Wall &w : m_g.walls()) if (w.id == id) return &w;
    return nullptr;
}
double MusyNoteGame::wallX(int id) const { const auto *w = find(id); return w ? w->x : -10; }
double MusyNoteGame::wallGapTop(int id) const { const auto *w = find(id); return w ? w->gapTop : 0; }
double MusyNoteGame::wallGapBottom(int id) const { const auto *w = find(id); return w ? w->gapBottom : 0; }
QVariantList MusyNoteGame::wallTopArts(int id) const { QVariantList o; if (const auto *w = find(id)) for (int a : w->topArts) o.append(a); return o; }
QVariantList MusyNoteGame::wallBottomArts(int id) const { QVariantList o; if (const auto *w = find(id)) for (int a : w->bottomArts) o.append(a); return o; }

// The autopilot (the song plays for you): aim for the middle of the next gap.
void MusyNoteGame::pilot()
{
    double target = MusyNote::kH / 2;
    for (const MusyNote::Wall &w : m_g.walls())
        if (w.x + MusyNote::kWallW > MusyNote::kNoteX - MusyNote::kNoteR) { target = (w.gapTop + w.gapBottom) / 2; break; }
    if (m_g.state() == MusyNote::Ready) { m_g.flap(); emit stateChanged(); return; }   // it starts the run itself
    if (m_g.noteY() > target + 0.45 && m_g.noteVy() > -1.0) m_g.flap();
}

void MusyNoteGame::tickOnce(double dt, qint64 now)
{
    // no beat for a while (a quiet or beatless song): the game keeps its own pulse so there are still walls
    const double gap = 60000.0 / qBound(60.0, m_bpm, 180.0);
    if (m_g.state() == MusyNote::Playing && now - m_lastBeat > qMax(1400.0, gap * 2.0)) {
        if (!m_pulse) { m_pulse = true; emit pulseChanged(); }
        m_lastBeat = now;
        m_g.beat(0.5);
        ++m_beatFlash;
        emit beatFlashChanged();
    }
    if (m_autoplay) pilot();
    const auto before = m_g.walls();
    m_g.tick(dt, m_bpm);
    // the list of wall ids changes only when one appears or leaves
    QVariantList ids;
    for (const MusyNote::Wall &w : m_g.walls()) ids.append(w.id);
    if (ids != m_ids) { m_ids = ids; emit wallsChanged(); }
    ++m_frame;
    emit frameChanged();
    if (m_g.state() == MusyNote::Dead && !m_wasDead) {
        m_wasDead = true;
        emit stateChanged();
        emit finished(m_g.score(), m_g.passed(), !m_autoUsed);
    }
}

void MusyNoteGame::step()
{
    const qint64 now = m_clock.elapsed();
    const double dt = qMin(0.05, double(now - m_last) / 1000.0);
    m_last = now;
    if (!m_started || m_paused) return;
    if (!m_playing && m_g.state() != MusyNote::Ready) return;   // the music stopped: the game waits for it
    tickOnce(dt, now);
}

void MusyNoteGame::advance(int ticks, double dt)
{
    qint64 now = m_clock.elapsed();
    for (int i = 0; i < ticks; ++i) { now += qint64(dt * 1000); tickOnce(dt, now); }
}

} // namespace hop
