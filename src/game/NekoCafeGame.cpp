// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/NekoCafeGame.h"

#include <QDateTime>
#include <QVariantMap>

namespace hop {

NekoCafeGame::NekoCafeGame(QObject *parent) : QObject(parent)
{
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &NekoCafeGame::tick);
}

QVariantList NekoCafeGame::customers() const
{
    QVariantList out;
    for (int i = 0; i < NekoCafe::kMaxCustomers; ++i) {
        const NekoCafe::Customer &c = m_g.customer(i);
        if (c.state == NekoCafe::None) continue;
        QVariantMap m;
        m[QStringLiteral("id")] = i;
        m[QStringLiteral("x")] = c.x;
        m[QStringLiteral("y")] = c.y;
        m[QStringLiteral("state")] = c.state;
        m[QStringLiteral("dir")] = c.dir;
        m[QStringLiteral("step")] = c.step;
        m[QStringLiteral("activity")] = c.activity;
        m[QStringLiteral("taken")] = c.taken;
        m[QStringLiteral("mood")] = c.mood;
        m[QStringLiteral("bubbleX")] = NekoCafe::bubbleLeft(c.seat) * 8;
        m[QStringLiteral("bubbleY")] = NekoCafe::bubbleTop(c.seat) * 8;
        out << m;
    }
    return out;
}

void NekoCafeGame::setPaused(bool p)
{
    if (m_paused == p) return;
    m_paused = p;
    if (p) m_keys = 0;
    m_clock.invalidate();
    emit stateChanged();
}

void NekoCafeGame::start(quint32 seed)
{
    m_g = NekoCafe(seed ? seed : quint32(QDateTime::currentMSecsSinceEpoch() & 0x7fffffff) | 1u);
    m_started = true; m_paused = false; m_sentFinished = false; m_keys = 0; m_acc = 0;
    m_clock.restart();
    m_timer.start();
    ++m_frame;
    emit stateChanged();
    emit frameChanged();
}

void NekoCafeGame::again()
{
    if (m_g.phase() != NekoCafe::Results || !m_g.resultsReady()) return;
    m_g.newDay(quint32(QDateTime::currentMSecsSinceEpoch() & 0x7fffffff) | 1u);
    m_sentFinished = false; m_keys = 0; m_acc = 0;
    m_clock.restart();
    ++m_frame;
    emit stateChanged();
    emit frameChanged();
}

void NekoCafeGame::quit()
{
    m_timer.stop();
    m_started = false; m_paused = false; m_keys = 0;
    emit stateChanged();
}

void NekoCafeGame::setKey(const QString &key, bool down)
{
    int bit = 0;
    if (key == QLatin1String("left")) bit = NekoCafe::Left;
    else if (key == QLatin1String("right")) bit = NekoCafe::Right;
    else if (key == QLatin1String("up")) bit = NekoCafe::Up;
    else if (key == QLatin1String("down")) bit = NekoCafe::Down;
    else if (key == QLatin1String("a")) bit = NekoCafe::A;
    if (!bit) return;
    if (down) m_keys |= bit; else m_keys &= ~bit;
}

void NekoCafeGame::advance(int ms)
{
    if (!m_started || m_paused) return;
    m_acc += ms * 60.0 / 1000.0;
    int steps = 0;
    while (m_acc >= 1.0 && steps < 600) {
        m_acc -= 1.0;
        ++steps;
        m_g.frame(m_keys);
        if (m_g.phase() == NekoCafe::Results && !m_sentFinished) {
            m_sentFinished = true;
            emit finished(m_g.profit(), m_g.served(), m_g.happy());
        }
    }
    if (m_acc > 5) m_acc = 0;
    if (steps > 0) { ++m_frame; emit frameChanged(); }
}

void NekoCafeGame::tick()
{
    if (!m_clock.isValid()) { m_clock.start(); return; }
    const qint64 ms = m_clock.restart();
    advance(int(qMin<qint64>(ms, 100)));   // a stall (the window was dragged) never makes the cafe jump ahead
}

} // namespace hop
