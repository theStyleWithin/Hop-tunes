// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPoint>
#include <QVector>
#include <QtGlobal>

namespace hop {

// The rules of Album Fall: a falling-blocks game where the music decides the pieces. No Qt Quick and no audio in
// here, so it is deterministic and unit-tested; the screen (AlbumFall.qml) and the beat (BeatTracker) are plugged in
// from outside.
//
//  * The board is 10 wide, 20 tall. Every block carries its own random album cover; a cell holds
//    cover number + 1 (0 = empty).
//  * A beat (beat()) with a strength 0..1 picks the next piece: a soft beat gives a small piece (2-3 blocks), a normal
//    one a four-block piece, a heavy one a five-block piece.
//  * After a piece lands, the next one waits for a beat (or 0.7 s at most) before it starts to fall.
//  * Locking a piece within 120 ms of a beat is "on the beat": a bonus, and a streak that multiplies line clears.
class FallBoard {
public:
    static constexpr int W = 10;
    static constexpr int H = 20;
    enum State { Playing, GameOver };

    explicit FallBoard(quint32 seed = 1) { reset(seed); }
    void reset(quint32 seed);
    void setArtCount(int n) { m_artCount = qMax(1, n); }

    // ---- input ----
    bool moveLeft() { return move(-1); }
    bool moveRight() { return move(1); }
    bool rotate(bool clockwise = true);
    bool softDrop();                 // one row down; false (and locks) if it could not
    void hardDrop();                 // straight down and lock

    // ---- time and rhythm ----
    void beat(double strength, qint64 nowMs);          // a beat landed
    void tick(double dtSec, double rowsPerSec, qint64 nowMs);   // advance time; gravity is rows per second

    // ---- reading the board ----
    State state() const { return m_state; }
    int cell(int x, int y) const { return m_grid[y * W + x]; }
    bool hasPiece() const { return m_active; }
    QVector<QPoint> pieceCells() const;      // absolute positions of the falling piece
    QVector<QPoint> ghostCells() const;      // where it would land
    // every block of a piece carries its own random cover (index 0..artCount-1), like a box of chocolates
    QVector<int> pieceArts() const { return m_active ? m_piece.arts : QVector<int>(); }
    QVector<QPoint> nextCells() const { return m_next.cells; }   // shape only, around (0,0)
    QVector<int> nextArts() const { return m_next.arts; }
    int score() const { return m_score; }
    int lines() const { return m_lines; }
    int level() const { return 1 + m_lines / 10; }
    int streak() const { return m_streak; }          // locked on the beat in a row
    int pieces() const { return m_pieces; }
    // counters QML watches to flash effects
    int lockCount() const { return m_lockCount; }
    bool lastLockOnBeat() const { return m_lastLockOnBeat; }
    int lastLinesCleared() const { return m_lastCleared; }
    int clearCount() const { return m_clearCount; }

    static constexpr int kOnBeatMs = 120;
    static QVector<QVector<QPoint>> shapesFor(double strength);   // the pool a beat of this strength draws from

private:
    struct Piece { QVector<QPoint> cells; QVector<int> arts; int x = 0, y = 0; };
    bool fits(const Piece &p, int dx, int dy) const;
    bool move(int dx);
    void lock(qint64 nowMs);
    void spawn();
    Piece makePiece(double strength);
    quint32 rnd();
    void clearLines();

    int m_grid[W * H];
    State m_state = Playing;
    Piece m_piece, m_next;
    bool m_active = false;
    bool m_waiting = false;       // a piece landed: the next one starts on a beat
    double m_waited = 0;
    double m_fall = 0;            // gravity accumulator, in rows
    double m_lastStrength = 0.5;
    qint64 m_lastBeatMs = -100000;
    qint64 m_now = 0;
    int m_artCount = 1;
    int m_score = 0, m_lines = 0, m_streak = 0, m_pieces = 0;
    int m_lockCount = 0, m_clearCount = 0, m_lastCleared = 0;
    bool m_lastLockOnBeat = false;
    quint32 m_seed = 1;
};

} // namespace hop
