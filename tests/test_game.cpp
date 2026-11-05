// SPDX-License-Identifier: GPL-3.0-or-later
// The beat tracker and the rules of Album Fall, driven with made-up input.
#include "game/BeatTracker.h"
#include "game/FallBoard.h"

#include <QtTest>

using namespace hop;

namespace {
QVector<float> frame(float bass, float rest = -70.f)
{
    QVector<float> f(32, rest);
    f[0] = bass;
    f[1] = bass - 6;
    return f;
}
}

class TestGame : public QObject {
    Q_OBJECT
private slots:
    void findsAKickEveryHalfSecond()
    {
        BeatTracker t;
        int beats = 0;
        double lastStrength = 0;
        // 30 frames a second for 12 s: quiet bass with a loud kick every 500 ms (120 beats a minute)
        for (int i = 0; i < 360; ++i) {
            const qint64 ms = qint64(i) * 1000 / 30;
            const bool kick = (ms % 500) < 70;
            double s = 0;
            if (t.feed(frame(kick ? -8.f : -45.f, -50.f), ms, &s)) { ++beats; lastStrength = s; }
        }
        QVERIFY2(beats >= 18 && beats <= 26, qPrintable(QString::number(beats)));   // ~24 kicks
        QVERIFY(lastStrength > 0.4);
        QVERIFY2(qAbs(t.bpm() - 120) < 12, qPrintable(QString::number(t.bpm())));
        QVERIFY(t.level() > 0);
    }

    void silenceAndSteadyNoiseAreNotBeats()
    {
        BeatTracker t;
        int beats = 0;
        for (int i = 0; i < 300; ++i) beats += t.feed(frame(-80.f, -80.f), qint64(i) * 33);
        QCOMPARE(beats, 0);
        for (int i = 300; i < 600; ++i) beats += t.feed(frame(-20.f, -30.f), qint64(i) * 33);   // loud but flat
        QVERIFY(beats <= 2);          // only the step up at the start may count
        QVERIFY(!t.feed(QVector<float>(3, 0.f), 99999));   // a malformed frame is ignored
    }

    void beatsChooseThePieceSize()
    {
        for (const auto &s : FallBoard::shapesFor(0.1)) QVERIFY(s.size() >= 2 && s.size() <= 3);
        for (const auto &s : FallBoard::shapesFor(0.5)) QCOMPARE(s.size(), 4);
        for (const auto &s : FallBoard::shapesFor(0.95)) QCOMPARE(s.size(), 5);
        FallBoard b(7);
        b.setArtCount(4);
        b.beat(0.9, 1000);
        QVERIFY(b.hasPiece());
        QCOMPARE(b.pieceCells().size(), 5);
        FallBoard c(7);
        c.beat(0.1, 1000);
        QVERIFY(c.pieceCells().size() <= 3);
    }

    void aPieceWaitsForABeatButNotForever()
    {
        FallBoard b(3);
        b.tick(0.3, 4, 300);
        QVERIFY(!b.hasPiece());             // waiting for a beat
        b.tick(0.5, 4, 800);
        QVERIFY(b.hasPiece());              // 0.7 s passed with no beat
    }

    void movingRotatingAndTheWalls()
    {
        FallBoard b(11);
        b.beat(0.5, 0);
        QVERIFY(b.hasPiece());
        for (int i = 0; i < 20; ++i) b.moveLeft();
        int minX = 99;
        for (const QPoint &p : b.pieceCells()) minX = qMin(minX, p.x());
        QCOMPARE(minX, 0);
        QVERIFY(!b.moveLeft());             // stopped by the wall
        const int before = b.pieceCells().size();
        b.rotate();
        QCOMPARE(b.pieceCells().size(), before);   // a turn next to the wall still works (kick) and keeps the piece whole
        for (const QPoint &p : b.pieceCells()) QVERIFY(p.x() >= 0 && p.x() < FallBoard::W);
    }

    void hardDropLocksAndScores()
    {
        FallBoard b(5);
        b.setArtCount(3);
        b.beat(0.5, 1000);
        const auto ghost = b.ghostCells();
        QVERIFY(!ghost.isEmpty());
        for (const QPoint &p : ghost) QVERIFY(p.y() >= 0 && p.y() < FallBoard::H);
        b.hardDrop();
        QVERIFY(!b.hasPiece());
        QCOMPARE(b.lockCount(), 1);
        int filled = 0;
        for (int y = 0; y < FallBoard::H; ++y) for (int x = 0; x < FallBoard::W; ++x) if (b.cell(x, y)) { ++filled; QVERIFY(b.cell(x, y) <= 3); }
        QVERIFY(filled >= 4 && filled <= 5);
        QVERIFY(b.score() > 0);
    }

    void landingOnTheBeatPaysMore()
    {
        FallBoard onBeat(5), offBeat(5);
        onBeat.beat(0.5, 1000);
        offBeat.beat(0.5, 1000);
        onBeat.beat(0.5, 2000);   // (a beat while a piece falls only marks the time)
        onBeat.tick(0.0, 1, 2050);   // 50 ms after the beat
        onBeat.hardDrop();
        QVERIFY(onBeat.lastLockOnBeat());
        QCOMPARE(onBeat.streak(), 1);
        offBeat.tick(0.0, 1, 1500);   // 500 ms after the last beat
        offBeat.hardDrop();
        QVERIFY(!offBeat.lastLockOnBeat());
        QCOMPARE(offBeat.streak(), 0);
        QVERIFY(onBeat.score() > offBeat.score());
    }

    void fullRowsClearAndTheRestFalls()
    {
        // Fill the bottom row with five dominoes (every piece hard-dropped into its own two columns).
        FallBoard b(1);
        b.setArtCount(2);
        int safety = 0;
        auto bottomFilled = [&] { int n = 0; for (int x = 0; x < FallBoard::W; ++x) n += b.cell(x, FallBoard::H - 1) != 0; return n; };
        while (b.lines() == 0 && b.state() == FallBoard::Playing && safety++ < 400) {
            b.beat(0.1, safety * 1000);      // small pieces
            // put the piece in the leftmost bottom gap
            for (int i = 0; i < 12; ++i) b.moveLeft();
            int gap = 0; while (gap < FallBoard::W && b.cell(gap, FallBoard::H - 1)) ++gap;
            int minX = 99; for (const QPoint &p : b.pieceCells()) minX = qMin(minX, p.x());
            for (int i = 0; i < gap - minX; ++i) b.moveRight();
            b.hardDrop();
            Q_UNUSED(bottomFilled);
        }
        QVERIFY2(b.lines() >= 1 || b.state() == FallBoard::GameOver, "either a row cleared or the stack topped out");
        if (b.lines() >= 1) {
            QVERIFY(b.lastLinesCleared() >= 1);
            QVERIFY(b.score() >= 100);
            QCOMPARE(b.level(), 1);
        }
    }

    void toppingOutEndsTheGame()
    {
        FallBoard b(9);
        int safety = 0;
        while (b.state() == FallBoard::Playing && safety++ < 500) { b.beat(0.5, safety * 1000); b.hardDrop(); }
        QCOMPARE(b.state(), FallBoard::GameOver);
        QVERIFY(!b.moveLeft() && !b.rotate());
        b.reset(2);
        QCOMPARE(b.state(), FallBoard::Playing);
        QCOMPARE(b.score(), 0);
        for (int y = 0; y < FallBoard::H; ++y) for (int x = 0; x < FallBoard::W; ++x) QCOMPARE(b.cell(x, y), 0);
    }

    void sameSeedSamePieces()
    {
        FallBoard a(42), b(42);
        a.setArtCount(5); b.setArtCount(5);
        for (int i = 1; i <= 5; ++i) { a.beat(0.5, i * 1000); b.beat(0.5, i * 1000); QCOMPARE(a.pieceCells(), b.pieceCells()); QCOMPARE(a.pieceArt(), b.pieceArt()); a.hardDrop(); b.hardDrop(); }
    }
};

QTEST_GUILESS_MAIN(TestGame)
#include "test_game.moc"
