// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/AlbumFall.h"

#include <QDateTime>
#include <QRandomGenerator>

namespace hop {

AlbumFall::AlbumFall(QObject *parent) : QObject(parent)
{
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &AlbumFall::step);
    m_clock.start();
}

QVariantList AlbumFall::flat(const QVector<QPoint> &cells)
{
    QVariantList out;
    for (const QPoint &p : cells) { out.append(p.x()); out.append(p.y()); }
    return out;
}

void AlbumFall::start(int artCount)
{
    m_art = qMax(1, artCount);
    m_board.setArtCount(m_art);
    m_board.reset(QRandomGenerator::global()->generate());
    m_started = true;
    m_paused = false;
    m_wasOver = false;
    m_last = m_clock.elapsed();
    m_lastBeat = m_last;
    ++m_rev;
    m_timer.start();
    emit runningChanged();
    emit pausedChanged();
    emit changed();
}

void AlbumFall::setPaused(bool p)
{
    if (p == m_paused) return;
    m_paused = p;
    m_last = m_clock.elapsed();
    emit pausedChanged();
}

void AlbumFall::onBeat(double strength)
{
    if (!m_started || m_paused || !m_playing) return;
    m_lastBeat = m_clock.elapsed();
    if (m_pulse) { m_pulse = false; emit pulseChanged(); }
    m_board.beat(strength, m_lastBeat);
    ++m_beatFlash;
    ++m_rev;
    emit beatFlashChanged();
    emit changed();
}

void AlbumFall::act(const QString &a)
{
    if (!m_started || m_paused || m_board.state() != FallBoard::Playing) return;
    bool moved = false;
    if (a == QLatin1String("left")) moved = m_board.moveLeft();
    else if (a == QLatin1String("right")) moved = m_board.moveRight();
    else if (a == QLatin1String("down")) { m_board.softDrop(); moved = true; }
    else if (a == QLatin1String("rotate")) moved = m_board.rotate(true);
    else if (a == QLatin1String("rotateBack")) moved = m_board.rotate(false);
    else if (a == QLatin1String("drop")) { m_board.hardDrop(); moved = true; }
    if (!moved) return;
    ++m_rev;
    emit changed();
    if (m_board.state() == FallBoard::GameOver && !m_wasOver) { m_wasOver = true; emit runningChanged(); emit finished(m_board.score(), m_board.lines()); }
}

void AlbumFall::step()
{
    const qint64 now = m_clock.elapsed();
    const double dt = qMin(0.1, double(now - m_last) / 1000.0);
    m_last = now;
    if (!m_started || m_paused || !m_playing || m_board.state() != FallBoard::Playing) return;

    // the song stands in with its own pulse when no beat has come for a while (quiet or beatless music, no analysis)
    const double gap = 60000.0 / qBound(60.0, m_bpm, 180.0);
    if (now - m_lastBeat > qMax(1400.0, gap * 2.0)) {
        if (!m_pulse) { m_pulse = true; emit pulseChanged(); }
        m_lastBeat = now;
        m_board.beat(0.5, now);
        ++m_beatFlash;
        emit beatFlashChanged();
    }
    // bass sets the speed: the tempo gives the pace, the low end leans on it
    m_speed = qBound(1.8, m_bpm / 60.0 * 2.2, 8.0) * (0.85 + 0.3 * qBound(0.0, m_bass, 1.0));
    const int clearsBefore = m_board.clearCount(), landedBefore = m_board.lockCount();
    const bool hadPiece = m_board.hasPiece();
    const QVector<QPoint> before = m_board.pieceCells();
    m_board.tick(dt, m_speed, now);
    if (m_board.pieceCells() != before || m_board.hasPiece() != hadPiece || m_board.clearCount() != clearsBefore || m_board.lockCount() != landedBefore) {
        ++m_rev;
        emit changed();
    }
    if (m_board.state() == FallBoard::GameOver && !m_wasOver) { m_wasOver = true; emit runningChanged(); emit changed(); emit finished(m_board.score(), m_board.lines()); }
}

} // namespace hop
