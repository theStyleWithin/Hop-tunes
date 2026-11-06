// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QPointF>
#include <QVector>
#include <QtGlobal>

namespace hop {

// The rules of Musy Note: a flappy-bird-style game where a music note flies through gaps in walls built from album
// covers, and the song makes the course. No Qt Quick and no audio in here (deterministic, unit-tested); the screen
// (MusyNote.qml), the beat (BeatTracker) and the controller plug in from outside.
//
// World: 16 units wide, 9 tall (landscape). The note sits at x = 3.0, gravity pulls it down, a flap kicks it up.
//  * A wall is made at every few beats, never closer than 1.4 s to the last one, and is placed so that it reaches the note
//    exactly ON a beat, so the music's pulse is the rhythm you fly to.
//  * Where the gap sits follows the pitch of the music at that moment (high sound, high gap), but never more than
//    2 units away from the last gap, so every course can be flown. The gap shrinks as the level goes up.
//  * Flying through the middle third of the gap scores 2, the rest of it 1. Touching a wall or the floor ends the game.
class MusyNote {
public:
    static constexpr double kW = 16.0, kH = 9.0;
    static constexpr double kNoteX = 3.0, kNoteR = 0.34;
    static constexpr double kWallW = 1.3;
    static constexpr double kMinSpacing = 1.4;     // seconds between two walls reaching the note
    enum State { Ready, Playing, Dead };

    struct Wall {
        double x = 0;                 // left edge
        double gapTop = 0, gapBottom = 0;
        bool passed = false;
        int id = 0;
        QVector<int> topArts, bottomArts;   // a cover number for every block of the wall (top part, bottom part)
    };

    explicit MusyNote(quint32 seed = 1) { reset(seed); }
    void reset(quint32 seed);
    void setArtCount(int n) { m_artCount = qMax(1, n); }

    // ---- input ----
    void flap();                          // the first flap also starts the game

    // ---- music ----
    void setPitch(double t01) { m_pitch = qBound(0.0, t01, 1.0); }   // 0 = low sound, 1 = high (band centroid)
    void beat(double strength);                                       // a beat landed (spawns a wall when the spacing allows)
    // advance time; bpm gives the scroll speed
    void tick(double dt, double bpm);

    // ---- reading ----
    State state() const { return m_state; }
    double noteY() const { return m_y; }
    double noteVy() const { return m_vy; }
    const QVector<Wall> &walls() const { return m_walls; }
    int score() const { return m_score; }
    int level() const { return 1 + m_passed / 8; }
    int passed() const { return m_passed; }
    double speed() const { return m_speed; }
    double gapSize() const;
    int flaps() const { return m_flaps; }

    // for tests: spawn a wall now at a given gap centre
    void debugSpawn(double centre);
    static constexpr double kMaxGapStep = 2.0;

private:
    double scrollSpeed(double bpm) const;
    quint32 rnd();
    void spawn(double arrivalSec);
    void collide();

    State m_state = Ready;
    double m_y = kH / 2, m_vy = 0;
    double m_pitch = 0.5;
    double m_speed = 4.0, m_bpm = 110, m_time = 0, m_lastArrival = -100, m_lastGap = kH / 2;
    double m_lastBeatTime = 0;
    QVector<Wall> m_walls;
    int m_score = 0, m_passed = 0, m_flaps = 0, m_nextId = 1, m_artCount = 1;
    quint32 m_seed = 1;
};

} // namespace hop
