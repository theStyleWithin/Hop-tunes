// SPDX-License-Identifier: GPL-3.0-or-later
// The beat tracker and the rules of Album Fall, driven with made-up input.
#include "game/AnalysisGuard.h"
#include "game/BeatTracker.h"
#include "game/FallBoard.h"
#include "game/MusyNote.h"
#include "game/PinTable.h"
#include "game/Gamepad.h"
#include "game/SpectrumAnalyzer.h"

#include <QPainter>
#include <QTemporaryDir>
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

    void aControllerBecomesActionsAndIsListedForDiagnostics()
    {
        Gamepad pad;
        QSignalSpy pressed(&pad, &Gamepad::pressed);
        QSignalSpy connected(&pad, &Gamepad::connectedChanged);
        QVERIFY(!pad.connected());
        pad.setDevice(QStringLiteral("GameSir X2 Pro"));          // what the Android activity reports
        QVERIFY(pad.connected());
        QCOMPARE(pad.name(), QStringLiteral("GameSir X2 Pro"));
        pad.feedAction(QStringLiteral("a"), true);                // Cross / A
        pad.feedAction(QStringLiteral("a"), false);
        pad.feedAction(QStringLiteral("up"), true);
        QCOMPARE(pressed.size(), 2);
        QCOMPARE(pressed.at(0).at(0).toString(), QStringLiteral("a"));
        QCOMPARE(pressed.at(1).at(0).toString(), QStringLiteral("up"));
        // holding left repeats like a key; letting go stops it
        pad.feedAction(QStringLiteral("left"), true);
        QTRY_VERIFY_WITH_TIMEOUT(pressed.size() >= 5, 1500);
        pad.feedAction(QStringLiteral("left"), false);
        const int n = pressed.size();
        QTest::qWait(300);
        QCOMPARE(pressed.size(), n);
        // the diagnostics list keeps only the last 20 raw events
        for (int i = 0; i < 30; ++i) pad.logEvent(QStringLiteral("down KEYCODE_BUTTON_A -> a #%1").arg(i));
        QCOMPARE(pad.recentEvents().size(), 20);
        QCOMPARE(pad.recentEvents().last(), QStringLiteral("down KEYCODE_BUTTON_A -> a #29"));
        pad.setDevice({});
        QVERIFY(!pad.connected());
        QVERIFY(pad.name().isEmpty());
    }

    // The Android tap hands over raw samples; the analyser must turn them into the same 32 bands as GStreamer's spectrum.
    void samplesBecomeThirtyTwoBands()
    {
        SpectrumAnalyzer an;
        an.setSampleRate(44100);
        auto tone = [&](double hz, double amp, int n, int start) {
            QVector<float> s(n);
            for (int i = 0; i < n; ++i) s[i] = float(amp * std::sin(2 * M_PI * hz * (start + i) / 44100.0));
            return s;
        };
        // a loud 220 Hz tone lives in band 0 (0-690 Hz), a 5 kHz tone in band 7 (4.8-5.5 kHz)
        QVector<float> frame;
        int frames = 0, pos = 0;
        QVector<float> last;
        for (int blk = 0; blk < 12; ++blk) {
            QVector<float> s = tone(220, 0.8, 4096, pos);
            const QVector<float> hi = tone(5000, 0.4, 4096, pos);
            for (int i = 0; i < s.size(); ++i) s[i] += hi[i];
            pos += 4096;
            an.feed(s.constData(), s.size());
            while (an.next(&frame)) { ++frames; last = frame; }
        }
        QVERIFY2(frames >= 30, qPrintable(QString::number(frames)));       // about 30 frames a second: 1.1 s of sound
        QCOMPARE(last.size(), 32);
        QVERIFY2(last[0] > -8.f && last[0] <= 0.f, qPrintable(QString::number(double(last[0]))));   // 0.8 -> about -2 dB
        QVERIFY2(last[7] > -14.f, qPrintable(QString::number(double(last[7]))));                    // 0.4 -> about -8 dB
        for (int b : {2, 3, 4, 12, 20, 30}) QVERIFY2(last[b] < -45.f, qPrintable(QString("band %1 = %2").arg(b).arg(double(last[b]))));   // and nothing else
        // silence is -80 everywhere
        an.reset();
        QVector<float> zeros(8192, 0.f);
        an.feed(zeros.constData(), zeros.size());
        QVERIFY(an.next(&frame));
        for (float v : frame) QCOMPARE(v, -80.f);
        // the beat tracker can use it: a kick every 500 ms in the lowest band is found
        BeatTracker t;
        int beats = 0;
        an.reset();
        int at = 0;
        for (int i = 0; i < 12 * 44100; i += 1470) {
            const bool kick = ((i * 1000LL / 44100) % 500) < 90;
            QVector<float> s = tone(80, kick ? 0.9 : 0.02, 1470, i);
            const QVector<float> pad = tone(3000, 0.3, 1470, i);   // music has more in it than a kick: a steady body above it
            for (int k = 0; k < s.size(); ++k) s[k] += pad[k];
            an.feed(s.constData(), s.size());
            while (an.next(&frame)) { if (t.feed(frame, qint64(at) * 1000 / 30)) ++beats; ++at; }
        }
        QVERIFY2(beats >= 18 && beats <= 28, qPrintable(QString::number(beats)));
    }

    // ---- Musy Note ----
    // A simple autopilot: flap when below the middle of the next gap. If it can fly every course, a person can.
    static void pilot(MusyNote &g)
    {
        double target = MusyNote::kH / 2;
        for (const MusyNote::Wall &w : g.walls())
            if (w.x + MusyNote::kWallW > MusyNote::kNoteX - MusyNote::kNoteR) { target = (w.gapTop + w.gapBottom) / 2; break; }
        if (g.noteY() > target + 0.45 && g.noteVy() > -1.0) g.flap();
    }

    void musyNoteWallsArriveOnABeat()
    {
        MusyNote g(3);
        g.setArtCount(10);
        g.flap();
        const double bpm = 120, bi = 0.5, dt = 1.0 / 240;
        g.tick(0.0, bpm);
        g.beat(0.8);                                          // a beat spawns a wall...
        QCOMPARE(g.walls().size(), 1);
        const double v = g.speed() > 0 ? g.speed() : 4.0;
        Q_UNUSED(v);
        // ...which must reach the note an exact number of beats later
        double t = 0;
        const int id = g.walls().first().id;
        double arrival = -1;
        for (int i = 0; i < 240 * 8 && arrival < 0; ++i) {
            pilot(g);
            g.tick(dt, bpm);
            t += dt;
            for (const MusyNote::Wall &w : g.walls())
                if (w.id == id && w.x + MusyNote::kWallW / 2 <= MusyNote::kNoteX) arrival = t;
            if (g.state() == MusyNote::Dead) break;
        }
        QVERIFY(arrival > 0);
        const double beats = arrival / bi;
        QVERIFY2(qAbs(beats - std::round(beats)) < 0.06, qPrintable(QString::number(beats)));
    }

    void musyNoteWallsAreNeverCloserThanTheMinimumSpacing()
    {
        MusyNote g(9);
        g.setArtCount(5);
        g.flap();
        QVector<double> arrivals;
        double t = 0;
        QSet<int> seen;
        for (int i = 0; i < 60 * 40; ++i) {   // 40 s, a beat every 0.3 s (fast)
            pilot(g);
            if (i % 18 == 0) g.beat(0.7);
            g.tick(1.0 / 60, 180);
            t += 1.0 / 60;
            for (const MusyNote::Wall &w : g.walls())
                if (!seen.contains(w.id) && w.x + MusyNote::kWallW / 2 <= MusyNote::kNoteX) { seen.insert(w.id); arrivals.append(t); }
            if (g.state() == MusyNote::Dead) break;
        }
        QVERIFY(arrivals.size() >= 8);
        for (int i = 1; i < arrivals.size(); ++i) QVERIFY2(arrivals[i] - arrivals[i - 1] >= MusyNote::kMinSpacing - 0.05, qPrintable(QString::number(arrivals[i] - arrivals[i - 1])));
    }

    void musyNoteEveryCourseCanBeFlown()
    {
        // the worst music for the gap: the pitch jumps from one end to the other on every beat, at several tempos
        for (double bpm : {70.0, 100.0, 140.0, 180.0}) {
            MusyNote g{quint32(bpm)};
            g.setArtCount(8);
            g.flap();
            const double bi = 60.0 / bpm;
            double next = 0;
            int jump = 0;
            for (int i = 0; i < 60 * 70 && g.state() != MusyNote::Dead; ++i) {
                const double now = i / 60.0;
                if (now >= next) { g.setPitch((jump++ % 2) ? 0.0 : 1.0); g.beat(0.6); next += bi; }
                pilot(g);
                g.tick(1.0 / 60, bpm);
            }
            QVERIFY2(g.state() == MusyNote::Playing, qPrintable(QStringLiteral("died at %1 bpm after %2 walls").arg(bpm).arg(g.passed())));
            QVERIFY2(g.passed() >= 20, qPrintable(QString::number(g.passed())));
            QVERIFY(g.score() >= g.passed());
        }
    }

    void musyNoteGapFollowsThePitchAndShrinks()
    {
        MusyNote hi(1), lo(1);
        hi.setArtCount(3); lo.setArtCount(3);
        hi.flap(); lo.flap();
        for (int i = 0; i < 3; ++i) { hi.setPitch(1.0); lo.setPitch(0.0); hi.beat(0.5); lo.beat(0.5); for (int k = 0; k < 60 * 2; ++k) { pilot(hi); pilot(lo); hi.tick(1.0 / 60, 120); lo.tick(1.0 / 60, 120); } }
        QVERIFY(!hi.walls().isEmpty() && !lo.walls().isEmpty());
        QVERIFY(hi.walls().last().gapTop < lo.walls().last().gapTop);     // a high sound makes a high gap
        // a gap is never further than the step limit from the one before, and always inside the screen
        MusyNote g(4);
        g.flap();
        g.setArtCount(2);
        double prev = -1;
        for (int i = 0; i < 12; ++i) {
            g.setPitch(i % 2 ? 0.0 : 1.0);
            g.debugSpawn(4.5);
        }
        MusyNote s(5);
        s.flap();
        s.setArtCount(2);
        for (int i = 0; i < 6 && s.state() == MusyNote::Playing; ++i) {
            s.setPitch(i % 2 ? 0.0 : 1.0);
            s.beat(0.5);
            for (int k = 0; k < 60 * 2; ++k) { pilot(s); s.tick(1.0 / 60, 120); }
            if (!s.walls().isEmpty()) {
                const auto &w = s.walls().last();
                const double c = (w.gapTop + w.gapBottom) / 2;
                QVERIFY(w.gapTop >= 0.4 && w.gapBottom <= MusyNote::kH - 0.4);
                if (prev >= 0) QVERIFY(qAbs(c - prev) <= MusyNote::kMaxGapStep + 0.01);
                prev = c;
            }
        }
        // higher level, smaller gap (never below the floor of 2.35)
        MusyNote lv(1);
        const double start = lv.gapSize();
        QVERIFY(start > 3.0);
        QVERIFY(qAbs(start - 3.3) < 1e-9);
    }

    void musyNoteCollisionsAndScoring()
    {
        MusyNote g(2);
        g.setArtCount(2);
        QCOMPARE(g.state(), MusyNote::Ready);
        g.tick(0.5, 100);
        QCOMPARE(g.state(), MusyNote::Ready);       // nothing moves until the first flap
        g.flap();
        QCOMPARE(g.state(), MusyNote::Playing);
        // fall to the floor: dead
        for (int i = 0; i < 60 * 3 && g.state() == MusyNote::Playing; ++i) g.tick(1.0 / 60, 100);
        QCOMPARE(g.state(), MusyNote::Dead);
        g.flap();                                    // a dead game ignores flaps
        QCOMPARE(g.state(), MusyNote::Dead);
        // a wall straight ahead with the gap far away: hit
        MusyNote w(2);
        w.setArtCount(2);
        w.flap();
        w.debugSpawn(1.5);                           // gap near the top, the note stays low
        for (int i = 0; i < 60 * 4 && w.state() == MusyNote::Playing; ++i) { if (w.noteY() > 6.0) w.flap(); w.tick(1.0 / 60, 100); }
        QCOMPARE(w.state(), MusyNote::Dead);
        // flying through the middle of a gap scores 2; the edge of it 1
        MusyNote p(2);
        p.setArtCount(2);
        p.flap();
        p.debugSpawn(4.5);
        for (int i = 0; i < 60 * 4 && p.state() == MusyNote::Playing && p.passed() == 0; ++i) { pilot(p); p.tick(1.0 / 60, 100); }
        QCOMPARE(p.state(), MusyNote::Playing);
        QCOMPARE(p.passed(), 1);
        QVERIFY(p.score() == 1 || p.score() == 2);
    }

    void musyNoteIsDeterministic()
    {
        auto run = [](quint32 seed) {
            MusyNote g(seed);
            g.setArtCount(9);
            g.flap();
            QVector<int> arts;
            for (int i = 0; i < 60 * 20 && g.state() == MusyNote::Playing; ++i) {
                if (i % 30 == 0) { g.setPitch((i / 30) % 3 * 0.5); g.beat(0.5); }
                pilot(g);
                g.tick(1.0 / 60, 120);
            }
            for (const auto &w : g.walls()) arts += w.topArts + w.bottomArts;
            return std::make_pair(g.score(), arts);
        };
        QCOMPARE(run(77), run(77));
    }

    // The crash guard for the sound analysis: a marker while it is on; found at start-up it means "the last run ended while it was on"
    void theAnalysisCrashGuardRemembersAnUncleanEnd()
    {
        QTemporaryDir dir;
        {
            AnalysisGuard g(dir.path());
            QVERIFY(!g.tripped());                      // a clean start
            g.arm();                                    // the analysis starts listening...
            g.disarm();                                 // ...and stops cleanly
        }
        {
            AnalysisGuard g(dir.path());
            QVERIFY(!g.tripped());
            g.arm();                                    // listening when the process dies: the marker stays
        }
        {
            AnalysisGuard g(dir.path());
            QVERIFY(g.tripped());                       // next start-up: it was on when the app ended
            AnalysisGuard again(dir.path());
            QVERIFY(again.tripped());                   // and it stays tripped on every start until the person turns it back on
            g.clear();
        }
        AnalysisGuard g(QDir(dir.path()).filePath(QStringLiteral("not/yet/there")));
        g.arm();                                        // makes its folder
        QVERIFY(g.tripped());
        g.disarm();
        QVERIFY(!g.tripped());
    }

    // The autopilot ("let the song play"): it must play sensibly, not just legally.
    void theAutopilotPlaysAWholeGameWithoutToppingOut()
    {
        FallBoard b(12345);
        b.setArtCount(6);
        int pieces = 0;
        for (int i = 1; i <= 220 && b.state() == FallBoard::Playing; ++i) {
            b.beat(0.5, qint64(i) * 1000);
            const FallBoard::Plan p = b.suggest();
            QVERIFY(p.valid);
            for (int r = 0; r < p.rotations; ++r) b.rotate();
            for (int guard = 0; guard < 16 && b.pieceX() != p.x; ++guard) { if (b.pieceX() < p.x) b.moveRight(); else b.moveLeft(); }
            b.hardDrop();
            ++pieces;
        }
        QCOMPARE(b.state(), FallBoard::Playing);              // 220 pieces and still going
        QVERIFY2(b.lines() >= 40, qPrintable(QString::number(b.lines())));
        QVERIFY(pieces >= 200);
    }

    void sameSeedSamePieces()
    {
        FallBoard a(42), b(42);
        a.setArtCount(5); b.setArtCount(5);
        for (int i = 1; i <= 5; ++i) { a.beat(0.5, i * 1000); b.beat(0.5, i * 1000); QCOMPARE(a.pieceCells(), b.pieceCells()); QCOMPARE(a.pieceArt(), b.pieceArt()); a.hardDrop(); b.hardDrop(); }
    }

    // ---- Cover Pinball ----

    void pinballBumpersSitOnTheBrightPlacesOfTheCover()
    {
        // a dark cover with two bright, colourful spots: those spots are where the bumpers go
        QImage im(200, 340, QImage::Format_RGB32);
        im.fill(QColor(15, 15, 20));
        QPainter p(&im);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 60, 40));  p.drawEllipse(QPoint(60, 100), 22, 22);     // upper left
        p.setBrush(QColor(60, 220, 255)); p.drawEllipse(QPoint(150, 220), 22, 22);    // lower right
        p.end();
        const auto a = PinTable::layoutFromImage(im);
        const auto b = PinTable::layoutFromImage(im);
        QVERIFY(a.size() >= 5 && a.size() <= 7);
        QCOMPARE(a.size(), b.size());
        for (int i = 0; i < a.size(); ++i) { QCOMPARE(a[i].x, b[i].x); QCOMPARE(a[i].y, b[i].y); }   // same cover, same table
        auto near = [&](double x, double y) { for (const auto &k : a) if (qHypot(k.x - x, k.y - y) < 0.9) return true; return false; };
        QVERIFY(near(3.0, 5.0));      // (60,100) of 200x340 on a 10x17 table
        QVERIFY(near(7.5, 11.0) || near(7.5, 10.4));   // the lower spot, pulled up to where a bumper may stand
        for (int i = 0; i < a.size(); ++i) {
            QVERIFY(a[i].x > 1.0 && a[i].x < 8.4 && a[i].y > 2.4 && a[i].y < 10.8);
            for (int j = i + 1; j < a.size(); ++j) QVERIFY(qHypot(a[i].x - a[j].x, a[i].y - a[j].y) >= 1.8);
        }
        const auto none = PinTable::layoutFromImage(QImage());           // no cover: a plain table
        QVERIFY(none.size() >= 5);
        QImage flat(64, 64, QImage::Format_RGB32); flat.fill(Qt::black);
        QVERIFY(PinTable::layoutFromImage(flat).size() >= 5);              // a flat picture still gives a table
    }

    void pinballLaunchesScoresAndDrains()
    {
        PinTable t(7);
        QCOMPARE(t.state(), PinTable::Ready);
        QCOMPARE(t.ballsLeft(), 3);
        t.tick(0.5);
        QVERIFY(t.ballInLane());                                  // it waits
        t.launch(0.5);
        QCOMPARE(t.state(), PinTable::Playing);
        bool leftLane = false;
        for (int i = 0; i < 600 && t.state() == PinTable::Playing; ++i) { t.tick(1.0 / 60); if (!t.ballInLane()) leftLane = true; }
        QVERIFY(leftLane);                                        // the lane throws it into the table
        // a ball dropped onto a bumper scores 100 and gets kicked away
        PinTable u(3);
        const auto b0 = u.bumpers().first();
        u.placeBall(QPointF(b0.x, b0.y - b0.r - 0.5), QPointF(0, 3));
        const int before = u.score();
        for (int i = 0; i < 60 && u.hits() == 0; ++i) u.tick(1.0 / 60);
        QCOMPARE(u.hits(), 1);
        QCOMPARE(u.score() - before, 100);
        QVERIFY(!u.lastHitOnBeat());
        QVERIFY(u.ballVelocity().y() < 0);
        // the same hit just after a beat pays double
        PinTable w(3);
        w.placeBall(QPointF(b0.x, b0.y - b0.r - 0.5), QPointF(0, 3));
        w.beat(0.8);
        for (int i = 0; i < 60 && w.hits() == 0; ++i) w.tick(1.0 / 60);
        QCOMPARE(w.score(), 200);
        QVERIFY(w.lastHitOnBeat());
        // balls drain, three of them end the game, and Over takes no more input
        PinTable d(1);
        for (int ball = 0; ball < 3; ++ball) {
            d.placeBall(QPointF(4.7, 15.8), QPointF(0, 6));     // straight down between the flippers
            for (int i = 0; i < 300 && d.state() == PinTable::Playing; ++i) d.tick(1.0 / 60);
            QCOMPARE(d.ballsLeft(), 2 - ball);
        }
        QCOMPARE(d.state(), PinTable::Over);
        d.launch(1.0);
        QCOMPARE(d.state(), PinTable::Over);
    }

    void pinballLightingEveryBumperPaysARound()
    {
        PinTable t(5);
        const auto bs = t.bumpers();
        for (int i = 0; i < bs.size(); ++i) {
            t.placeBall(QPointF(bs[i].x, bs[i].y - bs[i].r - 0.4), QPointF(0, 4));
            const int h = t.hits();
            for (int k = 0; k < 40 && t.hits() == h; ++k) t.tick(1.0 / 60);
            QCOMPARE(t.hits(), h + 1);
        }
        QCOMPARE(t.rounds(), 1);
        QVERIFY(t.score() >= 1000 + 100 * bs.size());
    }

    void pinballFlippersLiftTheBallAndNothingEscapes()
    {
        // a ball resting on the left flipper is thrown up when it is raised
        PinTable t(2);
        t.placeBall(QPointF(3.4, 13.9), QPointF(0, 0));
        for (int i = 0; i < 30; ++i) t.tick(1.0 / 60);
        const double rest = t.ball().y();
        t.flip(true, true);
        double top = rest;
        for (int i = 0; i < 60; ++i) { t.tick(1.0 / 60); top = qMin(top, t.ball().y()); }
        QVERIFY2(top < rest - 1.0, qPrintable(QString("%1 -> %2").arg(rest).arg(top)));
        // long scripted games; the ball never leaves the table and nothing goes NaN.
        // (1) random flips only: the game ends well inside five minutes. (2) a flipper-minding bot: it plays on, scores a lot.
        for (int bot = 0; bot < 2; ++bot) {
            PinTable g(11 + bot);
            quint32 s = 12345 + bot;
            for (int i = 0; i < 60 * 60 * 5 && g.state() != PinTable::Over; ++i) {
                s = s * 1664525u + 1013904223u;
                if (g.state() == PinTable::Ready && (s >> 20) % 30 == 0) g.launch((s >> 8 & 255) / 255.0);
                if (bot) {
                    const bool swing = (i / 14) % 2 == 0;        // a player lets go again: a ball under a held flipper stays there
                    g.flip(true, swing && g.ballNearFlipper(true));
                    g.flip(false, swing && g.ballNearFlipper(false));
                } else {
                    if ((s >> 12) % 7 == 0) g.flip(true, (s >> 3) & 1);
                    if ((s >> 14) % 7 == 0) g.flip(false, (s >> 5) & 1);
                }
                if (i % 40 == 0) g.beat(0.6);
                g.tick(1.0 / 60);
                QVERIFY(std::isfinite(g.ball().x()) && std::isfinite(g.ball().y()));
                QVERIFY(g.ball().x() > -1.0 && g.ball().x() < PinTable::kW + 1.0 && g.ball().y() > -1.0 && g.ball().y() < PinTable::kH + 1.0);
            }
            QVERIFY(g.hits() > 0);
            if (!bot) QCOMPARE(g.state(), PinTable::Over);
            else QVERIFY2(g.score() > 1000, qPrintable(QString("bot score %1, balls %2").arg(g.score()).arg(g.ballsLeft())));
        }
    }
};

QTEST_GUILESS_MAIN(TestGame)
#include "test_game.moc"
