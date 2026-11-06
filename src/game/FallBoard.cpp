// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/FallBoard.h"

#include <algorithm>
#include <cstring>

namespace hop {

using Shape = QVector<QPoint>;

static const QVector<Shape> &smallShapes()
{
    static const QVector<Shape> s = {
        {{0, 0}, {1, 0}},                    // domino
        {{-1, 0}, {0, 0}, {1, 0}},           // bar of three
        {{0, 0}, {1, 0}, {0, 1}},            // corner
    };
    return s;
}

static const QVector<Shape> &normalShapes()
{
    static const QVector<Shape> s = {
        {{-1, 0}, {0, 0}, {1, 0}, {2, 0}},   // I
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}},    // O
        {{-1, 0}, {0, 0}, {1, 0}, {0, 1}},   // T
        {{-1, 0}, {0, 0}, {1, 0}, {1, 1}},   // L
        {{-1, 0}, {0, 0}, {1, 0}, {-1, 1}},  // J
        {{0, 0}, {1, 0}, {-1, 1}, {0, 1}},   // S
        {{-1, 0}, {0, 0}, {0, 1}, {1, 1}},   // Z
    };
    return s;
}

static const QVector<Shape> &bigShapes()
{
    static const QVector<Shape> s = {
        {{0, -1}, {-1, 0}, {0, 0}, {1, 0}, {0, 1}},     // plus
        {{0, 0}, {1, 0}, {0, 1}, {1, 1}, {0, 2}},       // P
        {{-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}},     // U
        {{-1, 0}, {0, 0}, {1, 0}, {2, 0}, {2, 1}},      // long L
        {{-1, 0}, {0, 0}, {1, 0}, {0, 1}, {0, 2}},      // big T
    };
    return s;
}

QVector<Shape> FallBoard::shapesFor(double strength)
{
    if (strength < 0.35) return smallShapes();
    if (strength < 0.75) return normalShapes();
    return bigShapes();
}

void FallBoard::reset(quint32 seed)
{
    std::memset(m_grid, 0, sizeof(m_grid));
    m_state = Playing;
    m_active = false;
    m_waiting = true;
    m_waited = 0;
    m_fall = 0;
    m_lastStrength = 0.5;
    m_lastBeatMs = -100000;
    m_score = m_lines = m_streak = m_pieces = 0;
    m_lockCount = m_clearCount = m_lastCleared = 0;
    m_lastLockOnBeat = false;
    m_seed = seed ? seed : 1;
    m_next = makePiece(0.5);
}

quint32 FallBoard::rnd()
{
    m_seed ^= m_seed << 13; m_seed ^= m_seed >> 17; m_seed ^= m_seed << 5;   // xorshift32
    return m_seed;
}

FallBoard::Piece FallBoard::makePiece(double strength)
{
    const QVector<Shape> pool = shapesFor(strength);
    Piece p;
    p.cells = pool[int(rnd() % quint32(pool.size()))];
    p.art = int(rnd() % quint32(m_artCount));
    for (int turns = int(rnd() % 4); turns > 0; --turns)
        for (QPoint &c : p.cells) c = QPoint(-c.y(), c.x());
    return p;
}

bool FallBoard::fits(const Piece &p, int dx, int dy) const
{
    for (const QPoint &c : p.cells) {
        const int x = p.x + c.x() + dx, y = p.y + c.y() + dy;
        if (x < 0 || x >= W || y >= H) return false;
        if (y >= 0 && m_grid[y * W + x]) return false;
    }
    return true;
}

void FallBoard::spawn()
{
    m_piece = m_next;
    int minX = 99, maxX = -99, minY = 99;
    for (const QPoint &c : m_piece.cells) { minX = qMin(minX, c.x()); maxX = qMax(maxX, c.x()); minY = qMin(minY, c.y()); }
    m_piece.x = (W - (maxX - minX + 1)) / 2 - minX;
    m_piece.y = -minY;
    m_next = makePiece(m_lastStrength);   // the next one is drawn from the latest beat
    m_waiting = false;
    m_fall = 0;
    ++m_pieces;
    if (!fits(m_piece, 0, 0)) { m_state = GameOver; m_active = false; return; }
    m_active = true;
}

bool FallBoard::move(int dx)
{
    if (m_state != Playing || !m_active || !fits(m_piece, dx, 0)) return false;
    m_piece.x += dx;
    return true;
}

bool FallBoard::rotate(bool cw)
{
    if (m_state != Playing || !m_active) return false;
    Piece r = m_piece;
    for (QPoint &c : r.cells) c = cw ? QPoint(-c.y(), c.x()) : QPoint(c.y(), -c.x());
    for (int kick : {0, -1, 1, -2, 2}) {   // a little wall kick, so a turn next to the wall still works
        r.x = m_piece.x + kick;
        if (fits(r, 0, 0)) { m_piece = r; return true; }
    }
    return false;
}

bool FallBoard::softDrop()
{
    if (m_state != Playing || !m_active) return false;
    if (fits(m_piece, 0, 1)) { ++m_piece.y; ++m_score; return true; }
    lock(m_now);
    return false;
}

void FallBoard::hardDrop()
{
    if (m_state != Playing || !m_active) return;
    int rows = 0;
    while (fits(m_piece, 0, 1)) { ++m_piece.y; ++rows; }
    m_score += rows * 2;
    lock(m_now);
}

void FallBoard::lock(qint64 nowMs)
{
    for (const QPoint &c : m_piece.cells) {
        const int x = m_piece.x + c.x(), y = m_piece.y + c.y();
        if (y < 0) { m_state = GameOver; m_active = false; return; }
        m_grid[y * W + x] = m_piece.art + 1;
    }
    m_active = false;
    ++m_lockCount;
    m_lastLockOnBeat = qAbs(nowMs - m_lastBeatMs) <= kOnBeatMs;
    if (m_lastLockOnBeat) { ++m_streak; m_score += 50 * level(); } else m_streak = 0;
    clearLines();
    m_waiting = true;
    m_waited = 0;
}

void FallBoard::clearLines()
{
    int cleared = 0;
    for (int y = H - 1; y >= 0; --y) {
        bool full = true;
        for (int x = 0; x < W && full; ++x) full = m_grid[y * W + x] != 0;
        if (!full) continue;
        for (int yy = y; yy > 0; --yy) std::memcpy(&m_grid[yy * W], &m_grid[(yy - 1) * W], sizeof(int) * W);
        std::memset(m_grid, 0, sizeof(int) * W);
        ++cleared;
        ++y;   // the row that fell into this place must be checked again
    }
    m_lastCleared = cleared;
    if (cleared == 0) return;
    static const int points[] = {0, 100, 300, 500, 800};
    // a streak of on-the-beat landings multiplies the clear: x1, then +25 % for each, up to x2
    const double mult = 1.0 + qMin(4, m_streak) * 0.25;
    m_score += int(points[qMin(cleared, 4)] * level() * mult);
    m_lines += cleared;
    ++m_clearCount;
}

void FallBoard::beat(double strength, qint64 nowMs)
{
    m_lastBeatMs = nowMs;
    m_now = nowMs;
    m_lastStrength = qBound(0.0, strength, 1.0);
    if (m_state != Playing) return;
    if (!m_active && m_waiting) {
        // the next piece was drawn from the previous beat; this beat decides its weight
        m_next = makePiece(m_lastStrength);
        spawn();
    }
}

void FallBoard::tick(double dt, double rowsPerSec, qint64 nowMs)
{
    m_now = nowMs;
    if (m_state != Playing) return;
    if (!m_active) {
        if (m_waiting) {
            m_waited += dt;
            if (m_waited >= 0.7) spawn();   // no beat came: do not leave the player staring at an empty board
        }
        return;
    }
    m_fall += dt * rowsPerSec * (1.0 + 0.06 * (level() - 1));
    while (m_fall >= 1.0 && m_active) {
        m_fall -= 1.0;
        if (fits(m_piece, 0, 1)) ++m_piece.y;
        else { lock(nowMs); break; }
    }
}

FallBoard::Plan FallBoard::suggest() const
{
    Plan best;
    if (!m_active || m_state != Playing) return best;
    double bestScore = -1e18;
    for (int rot = 0; rot < 4; ++rot) {
        Piece p = m_piece;
        for (int r = 0; r < rot; ++r) for (QPoint &c : p.cells) c = QPoint(-c.y(), c.x());
        for (int x = -4; x < W + 4; ++x) {
            p.x = x;
            p.y = m_piece.y;
            if (!fits(p, 0, 0)) continue;
            while (fits(p, 0, 1)) ++p.y;
            int g[W * H];
            std::memcpy(g, m_grid, sizeof(g));
            bool above = false;
            for (const QPoint &c : p.cells) {
                const int cx = p.x + c.x(), cy = p.y + c.y();
                if (cy < 0) { above = true; continue; }
                g[cy * W + cx] = 1;
            }
            if (above) continue;                       // would top out
            int lines = 0;
            for (int y = 0; y < H; ++y) { bool full = true; for (int xx = 0; xx < W && full; ++xx) full = g[y * W + xx] != 0; lines += full; }
            int heights[W] = {0}, holes = 0, total = 0, bump = 0;
            for (int xx = 0; xx < W; ++xx) {
                int y = 0;
                while (y < H && !g[y * W + xx]) ++y;
                heights[xx] = H - y;
                total += heights[xx];
                for (int yy = y; yy < H; ++yy) if (!g[yy * W + xx]) ++holes;
            }
            for (int xx = 0; xx + 1 < W; ++xx) bump += qAbs(heights[xx] - heights[xx + 1]);
            const double score = -0.51 * total + 0.76 * lines - 0.36 * holes - 0.18 * bump;
            if (score > bestScore) { bestScore = score; best.valid = true; best.rotations = rot; best.x = x; }
        }
    }
    return best;
}

QVector<QPoint> FallBoard::pieceCells() const
{
    QVector<QPoint> out;
    if (!m_active) return out;
    for (const QPoint &c : m_piece.cells) out.append(QPoint(m_piece.x + c.x(), m_piece.y + c.y()));
    return out;
}

QVector<QPoint> FallBoard::ghostCells() const
{
    QVector<QPoint> out;
    if (!m_active) return out;
    int drop = 0;
    while (fits(m_piece, 0, drop + 1)) ++drop;
    for (const QPoint &c : m_piece.cells) out.append(QPoint(m_piece.x + c.x(), m_piece.y + c.y() + drop));
    return out;
}

} // namespace hop
