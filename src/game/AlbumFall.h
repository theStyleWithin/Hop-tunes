// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "game/FallBoard.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantList>

namespace hop {

// Album Fall for the screen: FallBoard's rules plus a clock, the beat and the input actions. AlbumFall.qml only
// draws what it exposes. Keyboard and gamepad both come in through act().
class AlbumFall : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)       // a game is on (not over)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(bool gameOver READ gameOver NOTIFY changed)
    Q_PROPERTY(int score READ score NOTIFY changed)
    Q_PROPERTY(int lines READ lines NOTIFY changed)
    Q_PROPERTY(int level READ level NOTIFY changed)
    Q_PROPERTY(int streak READ streak NOTIFY changed)
    Q_PROPERTY(int rev READ rev NOTIFY changed)                        // bumps whenever the picture changes
    Q_PROPERTY(QVariantList piece READ piece NOTIFY changed)           // [x, y, x, y, ...] of the falling piece
    Q_PROPERTY(QVariantList ghost READ ghost NOTIFY changed)
    Q_PROPERTY(QVariantList next READ next NOTIFY changed)             // the next piece, around (0,0)
    Q_PROPERTY(QVariantList pieceArts READ pieceArts NOTIFY changed)   // one cover number per block of the piece
    Q_PROPERTY(QVariantList nextArts READ nextArts NOTIFY changed)
    Q_PROPERTY(int beatFlash READ beatFlash NOTIFY beatFlashChanged)   // counts up on every beat (pulses the screen)
    Q_PROPERTY(int landed READ landed NOTIFY changed)                  // counts up on every landing
    Q_PROPERTY(bool landedOnBeat READ landedOnBeat NOTIFY changed)
    Q_PROPERTY(int cleared READ cleared NOTIFY changed)                // counts up on every line clear
    Q_PROPERTY(int lastCleared READ lastCleared NOTIFY changed)
    Q_PROPERTY(bool pulse READ pulse NOTIFY pulseChanged)              // true while the own metronome stands in for a quiet song
    Q_PROPERTY(double speed READ speed NOTIFY changed)                 // rows a second right now
public:
    explicit AlbumFall(QObject *parent = nullptr);

    bool running() const { return m_started && m_board.state() == FallBoard::Playing; }
    bool paused() const { return m_paused; }
    void setPaused(bool p);
    bool gameOver() const { return m_started && m_board.state() == FallBoard::GameOver; }
    int score() const { return m_board.score(); }
    int lines() const { return m_board.lines(); }
    int level() const { return m_board.level(); }
    int streak() const { return m_board.streak(); }
    int rev() const { return m_rev; }
    QVariantList piece() const { return flat(m_board.pieceCells()); }
    QVariantList ghost() const { return flat(m_board.ghostCells()); }
    QVariantList next() const { return flat(m_board.nextCells()); }
    QVariantList pieceArts() const { QVariantList o; for (int v : m_board.pieceArts()) o.append(v); return o; }
    QVariantList nextArts() const { QVariantList o; for (int v : m_board.nextArts()) o.append(v); return o; }
    int beatFlash() const { return m_beatFlash; }
    int landed() const { return m_board.lockCount(); }
    bool landedOnBeat() const { return m_board.lastLockOnBeat(); }
    int cleared() const { return m_board.clearCount(); }
    int lastCleared() const { return m_board.lastLinesCleared(); }
    bool pulse() const { return m_pulse; }
    double speed() const { return m_speed; }

    Q_INVOKABLE int cell(int x, int y) const { return m_board.cell(x, y); }   // 0 empty, else cover number + 1
    Q_INVOKABLE void start(int artCount);                  // a fresh game
    // "left" "right" "down" "rotate" "rotateBack" "drop" -- ignored while paused or over
    Q_INVOKABLE void act(const QString &action);

    // The music: tempo (beats a minute) and bass level set the speed; a beat picks the next piece.
    void setMusic(double bpm, double bass) { m_bpm = bpm; m_bass = bass; }
    Q_INVOKABLE void onBeat(double strength);
    Q_INVOKABLE void setTempo(double bpm, double bass) { setMusic(bpm, bass); }
    // true while the song is playing; the game freezes when it is not
    Q_INVOKABLE void setPlaying(bool playing) { m_playing = playing; }
    // when the analysis is not there (or the song is too quiet to find beats) the game keeps its own pulse
    Q_INVOKABLE void setAnalysis(bool available) { m_analysis = available; }

signals:
    void runningChanged();
    void pausedChanged();
    void changed();
    void beatFlashChanged();
    void pulseChanged();
    void finished(int score, int lines);     // the stack topped out

private:
    void step();
    static QVariantList flat(const QVector<QPoint> &cells);

    FallBoard m_board;
    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_last = 0, m_lastBeat = 0;
    bool m_started = false, m_paused = false, m_playing = false, m_analysis = false, m_pulse = false, m_wasOver = false;
    double m_bpm = 110, m_bass = 0, m_speed = 3;
    int m_rev = 0, m_beatFlash = 0, m_art = 1;
};

} // namespace hop
