// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/PinballGame.h"

#include <QImage>
#include <QRandomGenerator>
#include <QUrl>

namespace hop {

PinballGame::PinballGame(QObject *parent) : QObject(parent)
{
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &PinballGame::step);
    m_clock.start();
}

const PinTable::Bumper &PinballGame::at(int i) const
{
    static const PinTable::Bumper none;
    const auto &b = m_t.bumpers();
    return i >= 0 && i < b.size() ? b[i] : none;
}

void PinballGame::start(const QString &coverUrl)
{
    QImage cover;
    if (!coverUrl.isEmpty()) {
        const QUrl u(coverUrl);
        cover.load(u.isLocalFile() ? u.toLocalFile() : coverUrl);
    }
    m_t.reset(PinTable::layoutFromImage(cover), QRandomGenerator::global()->generate());
    m_started = true;
    m_paused = false;
    m_wasOver = false;
    m_autoUsed = m_autoplay;          // a game is only scored if the song never played it
    m_pulse = false;
    m_autoT = 0;
    m_last = m_clock.elapsed();
    m_lastBeat = m_last;
    ++m_frame;
    m_timer.start();
    emit layoutChanged();
    emit stateChanged();
    emit pausedChanged();
    emit frameChanged();
}

void PinballGame::setPaused(bool p)
{
    if (p == m_paused) return;
    m_paused = p;
    m_last = m_clock.elapsed();
    emit pausedChanged();
}

void PinballGame::setAutoplay(bool on)
{
    if (on == m_autoplay) return;
    m_autoplay = on;
    if (on && m_started && m_t.state() != PinTable::Over) { m_autoUsed = true; emit stateChanged(); }   // this game keeps no score from now on
    if (!on) { m_t.flip(true, false); m_t.flip(false, false); }
    emit autoplayChanged();
}

void PinballGame::launch()
{
    if (!m_started || m_paused || m_autoplay) return;
    const bool begin = m_t.state() == PinTable::Ready;
    m_t.launch(0.5);
    if (begin && m_t.state() == PinTable::Playing) emit stateChanged();
}

void PinballGame::flip(bool left, bool up)
{
    if (!m_started || m_paused || m_autoplay) return;       // the song has the flippers while it plays for you
    m_t.flip(left, up);
}

void PinballGame::onBeat(double strength)
{
    if (!m_started || m_paused || !m_playing) return;
    m_lastBeat = m_clock.elapsed();
    if (m_pulse) { m_pulse = false; emit pulseChanged(); }
    m_t.beat(strength);
    ++m_beatFlash;
    emit beatFlashChanged();
}

// The autopilot (the song plays for you): launches by itself and swings a flipper when the ball is over it, letting go again.
void PinballGame::pilot(double dt)
{
    m_autoT += dt;
    if (m_t.state() == PinTable::Ready) { m_t.launch(0.5); emit stateChanged(); return; }
    const bool swing = std::fmod(m_autoT, 0.30) < 0.15;
    m_t.flip(true, swing && m_t.ballNearFlipper(true));
    m_t.flip(false, swing && m_t.ballNearFlipper(false));
}

void PinballGame::tickOnce(double dt, qint64 now)
{
    // no beat for a while (a quiet or beatless song): the table keeps its own pulse so the bumpers still move
    const double gap = 60000.0 / qBound(60.0, m_bpm, 180.0);
    if (m_t.state() == PinTable::Playing && now - m_lastBeat > qMax(1400.0, gap * 2.0)) {
        if (!m_pulse) { m_pulse = true; emit pulseChanged(); }
        m_lastBeat = now;
        m_t.beat(0.5);
        ++m_beatFlash;
        emit beatFlashChanged();
    }
    const auto before = m_t.state();
    if (m_autoplay) pilot(dt);
    m_t.tick(dt);
    ++m_frame;
    emit frameChanged();
    if (m_t.state() != before) emit stateChanged();
    if (m_t.state() == PinTable::Over && !m_wasOver) {
        m_wasOver = true;
        emit stateChanged();
        emit finished(m_t.score(), m_t.rounds(), !m_autoUsed);
    }
}

void PinballGame::step()
{
    const qint64 now = m_clock.elapsed();
    const double dt = qMin(0.05, double(now - m_last) / 1000.0);
    m_last = now;
    if (!m_started || m_paused) return;
    if (!m_playing && m_t.state() != PinTable::Ready) return;   // the music stopped: the game waits for it
    tickOnce(dt, now);
}

void PinballGame::advance(int ticks, double dt)
{
    qint64 now = m_clock.elapsed();
    for (int i = 0; i < ticks; ++i) { now += qint64(dt * 1000); tickOnce(dt, now); }
}

} // namespace hop
