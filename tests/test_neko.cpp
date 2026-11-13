// SPDX-License-Identifier: GPL-3.0-or-later
// neko-cafe's rules, driven frame by frame: the A button, the cook, tips, the day and the results.
#include "game/NekoCafe.h"
#include "game/NekoCafeGame.h"

#include <QSignalSpy>
#include <QtTest>

using namespace hop;
using C = NekoCafe;

namespace {
// stand the maid beside a customer (inside the reach box) and press A for one frame
void pressA(C &g) { g.frame(C::A); g.frame(0); }
void standBeside(C &g, int idx) { const auto &c = g.customer(idx); g.debugSetMaid(c.x - 4, c.y); }
void standAtCounter(C &g) { g.debugSetMaid(40, 48); }
void runFrames(C &g, int n) { for (int i = 0; i < n; ++i) g.frame(0); }

// one order from start to serve; `patienceLeft` is the share of patience (percent) the customer has when served
struct Served { int happy; int tips; int bills; int upset; };
Served oneOrder(quint32 seed, int patienceLeft)
{
    C g(seed);
    const int i = g.debugSpawn(8, C::Patient, 1000);
    standBeside(g, i);
    pressA(g);                               // take the order
    standAtCounter(g);
    pressA(g);                               // hand the slip to the cook
    for (int f = 0; f < 600 && !g.holdsFood(); ++f) { standAtCounter(g); g.frame(0); }   // he cooks, the maid picks it up
    for (int f = 0; f < 40; ++f) g.frame(0);                                              // her happy hop
    g.debugCustomer(i).patience = g.debugCustomer(i).patienceMax * patienceLeft / 100;
    standBeside(g, i);
    pressA(g);                               // serve
    return {g.happy(), g.tips(), g.bills(), g.upset()};
}
}

class TestNeko : public QObject {
    Q_OBJECT
private slots:
    void routesAvoidEveryTable()
    {
        // every customer route is made of straight lines and no customer's feet cross a solid tile on the way to the chair
        for (int s = 0; s < C::kSeats; ++s) {
            QVERIFY2(C::seatRow(s) > 0 && C::seatCol(s) > 0, "seat inside the room");
            for (int p = 0; p + 1 < C::kPathLen; ++p) {
                const int x0 = C::routeX(s, p), y0 = C::routeY(s, p), x1 = C::routeX(s, p + 1), y1 = C::routeY(s, p + 1);
                QVERIFY2(x0 == x1 || y0 == y1, qPrintable(QStringLiteral("seat %1 leg %2 is not straight").arg(s).arg(p)));
                if (p + 2 == C::kPathLen) continue;   // the last step is onto the chair
                const int n = qMax(qAbs(x1 - x0), qAbs(y1 - y0));
                for (int k = 0; k <= n; ++k) {
                    const int x = x0 + (x1 > x0 ? k : x1 < x0 ? -k : 0), y = y0 + (y1 > y0 ? k : y1 < y0 ? -k : 0);
                    QVERIFY2(!C::solidAt(x + 4, y + 15), qPrintable(QStringLiteral("seat %1 leg %2 walks into something at %3,%4").arg(s).arg(p).arg(x).arg(y)));
                }
            }
        }
    }

    void takingAnOrderThenTheCookThenServing()
    {
        C g(7);
        const int i = g.debugSpawn(8, C::Patient, 1000);
        QVERIFY(!g.holdsTicket());
        standBeside(g, i);
        pressA(g);
        QVERIFY(g.holdsTicket());
        QVERIFY(g.customer(i).taken);
        standAtCounter(g);
        QVERIFY(g.maidAtCounter());
        pressA(g);
        QVERIFY(!g.holdsTicket());
        QCOMPARE(g.cookState(), int(C::CookCooking));
        for (int f = 0; f < 600 && !g.holdsFood(); ++f) { standAtCounter(g); g.frame(0); }
        QVERIFY2(g.holdsFood(), "the cook finished and she picked it up");
        QVERIFY(g.exclaim());
        QCOMPARE(g.cookState(), int(C::CookIdle));
        QVERIFY(g.maidJump() <= 0);
    }

    void aSlipCannotBeHandedInAwayFromTheCounterOrWhileTheCookIsBusy()
    {
        C g(3);
        const int a = g.debugSpawn(8, C::Patient, 1000);
        standBeside(g, a);
        pressA(g);
        QVERIFY(g.holdsTicket());
        g.debugSetMaid(100, 100);
        pressA(g);
        QVERIFY2(g.holdsTicket(), "not at the counter: the slip stays with her");
        standAtCounter(g);
        pressA(g);
        QCOMPARE(g.cookState(), int(C::CookCooking));
        const int b = g.debugSpawn(9, C::Patient, 1000);
        standBeside(g, b);
        pressA(g);
        QVERIFY(g.holdsTicket());
        standAtCounter(g);
        pressA(g);
        QVERIFY2(g.holdsTicket(), "the cook is busy: she keeps the slip");
    }

    void aWrongCoffeeSendsThatCustomerAwayUpsetAndTheMaidKeepsTheCoffee()
    {
        C g(5);
        const int a = g.debugSpawn(8, C::Patient, 5000);
        const int b = g.debugSpawn(9, C::Patient, 5000);
        standBeside(g, a); pressA(g);
        standAtCounter(g); pressA(g);
        for (int f = 0; f < 600 && !g.holdsFood(); ++f) { standAtCounter(g); g.frame(0); }
        QVERIFY(g.holdsFood());
        runFrames(g, 40);                        // her happy hop: she cannot act during it
        standBeside(g, b); pressA(g);            // b has not ordered yet: this takes their order, never a wrong serve
        QVERIFY2(g.customer(b).taken, "pressing A beside someone who has not ordered takes their order");
        QCOMPARE(g.upset(), 0);
        standBeside(g, b); pressA(g);            // now b has ordered and she carries a's coffee: wrong
        QCOMPARE(g.customer(b).state, int(C::Reacting));
        QCOMPARE(g.customer(b).mood, int(C::MoodUpset));
        QVERIFY2(g.holdsFood(), "she keeps the coffee");
        QCOMPARE(g.upset(), 1);
    }

    void tipsFollowThePatienceThatWasLeft()
    {
        int fastHappy = 0, okHappy = 0, slowHappy = 0, fastTips = 0, okTips = 0;
        for (quint32 seed = 1; seed <= 300; ++seed) {
            const Served f = oneOrder(seed, 80), o = oneOrder(seed, 25), s = oneOrder(seed, 5);
            fastHappy += f.happy; okHappy += o.happy; slowHappy += s.happy;
            if (f.happy) { QCOMPARE(f.tips, C::kTipFast); QCOMPARE(f.bills, C::kBill); fastTips += f.tips; }
            if (o.happy) { QCOMPARE(o.tips, C::kTipOk); QCOMPARE(o.bills, C::kBill); okTips += o.tips; }
            QCOMPARE(s.bills, 0);                // too slow: no bill, no tip, upset
            QCOMPARE(s.tips, 0);
            QCOMPARE(s.upset, 1);
        }
        QCOMPARE(slowHappy, 0);
        // one in ten is grumpy whatever you do (so about 270 of 300)
        QVERIFY2(fastHappy >= 245 && fastHappy <= 295, qPrintable(QString::number(fastHappy)));
        QVERIFY2(okHappy >= 245 && okHappy <= 295, qPrintable(QString::number(okHappy)));
        QVERIFY(fastTips > okTips);
    }

    void aWaitingCustomerGivesUpWhenPatienceRunsOut()
    {
        C g(11);
        const int i = g.debugSpawn(8, C::Hungry, 60);
        runFrames(g, 59);
        QCOMPARE(g.customer(i).state, int(C::Waiting));
        runFrames(g, 2);
        QCOMPARE(g.customer(i).state, int(C::Leaving));
        QCOMPARE(g.customer(i).mood, int(C::MoodUpset));
        QCOMPARE(g.upset(), 1);
    }

    void aLeavingCustomersOrderIsDropped()
    {
        C g(2);
        const int i = g.debugSpawn(8, C::Hungry, 120);
        standBeside(g, i); pressA(g);
        QVERIFY(g.holdsTicket());
        for (int f = 0; f < 200; ++f) { g.debugSetMaid(100, 100); g.frame(0); }
        QVERIFY2(!g.holdsTicket(), "the customer left, so the slip in her pocket is thrown away");
    }

    void theDayEndsAfterClosingWithNothingNewAfterLastCall()
    {
        for (quint32 seed = 1; seed <= 12; ++seed) {
            C g(seed);
            bool ended = false;
            for (int f = 0; f < C::kDayFrames + 6000; ++f) {
                g.frame(0);
                QVERIFY(g.activeCustomers() <= C::kMaxCustomers);
                if (g.shiftFrames() < C::kStep1) QVERIFY(g.activeCustomers() <= 2);
                if (g.shiftFrames() >= C::kDayFrames - C::kLastCall + 700)
                    for (int k = 0; k < C::kMaxCustomers; ++k) QVERIFY(g.customer(k).state != C::Entering);
                QSet<int> seats;
                for (int k = 0; k < C::kMaxCustomers; ++k)
                    if (g.customer(k).state != C::None) { QVERIFY2(!seats.contains(g.customer(k).seat), "two customers in one seat"); seats.insert(g.customer(k).seat); }
                if (g.phase() == C::Results) { ended = true; break; }
            }
            QVERIFY2(ended, qPrintable(QStringLiteral("seed %1: the day did not end").arg(seed)));
            QCOMPARE(g.served(), 0);                  // nobody was served
            QCOMPARE(g.bills() + g.tips(), 0);
            QVERIFY(g.upset() > 0);
            QCOMPARE(g.grade(), 'D');
            QCOMPARE(g.activeCustomers(), 0);
        }
    }

    void aTirelessMaidPlaysAWholeDay()
    {
        for (quint32 seed = 21; seed <= 26; ++seed) {
            C g(seed);
            int guard = 0;
            while (g.phase() != C::Results && guard++ < C::kDayFrames + 9000) {
                // the bot: carry coffee to its customer; else slip to the counter; else take the next order; teleports instead of walking
                int keys = 0;
                if (g.holdsFood()) {
                    const int idx = g.debugFood();
                    if (g.customer(idx).state == C::Waiting || g.customer(idx).state == C::Ordering) { standBeside(g, idx); keys = C::A; }
                } else if (g.holdsTicket()) {
                    standAtCounter(g); keys = C::A;
                } else if (g.cookState() == C::CookHanding) {
                    standAtCounter(g);
                } else {
                    for (int k = 0; k < C::kMaxCustomers; ++k) {
                        const auto &c = g.customer(k);
                        if ((c.state == C::Ordering || c.state == C::Waiting) && !c.taken) { standBeside(g, k); keys = C::A; break; }
                    }
                }
                g.frame(keys);
                g.frame(0);
                // the books always balance
                QCOMPARE(g.bills(), g.happy() * C::kBill);
                QVERIFY(g.tips() >= 0 && g.tips() <= g.happy() * C::kTipFast);
                QVERIFY(g.served() >= g.happy());
            }
            QCOMPARE(g.phase(), int(C::Results));
            QVERIFY2(g.served() >= 5, qPrintable(QStringLiteral("seed %1 served only %2").arg(seed).arg(g.served())));
            QVERIFY(g.happy() > 0);
            QVERIFY(g.profit() > 0);
            QVERIFY(g.happinessPct() >= 0 && g.happinessPct() <= 100);
        }
    }

    void theSameSeedPlaysTheSameDay()
    {
        C a(99), b(99), c(100);
        int diff = 0;
        for (int f = 0; f < 6000; ++f) {
            a.frame(0); b.frame(0); c.frame(0);
            QCOMPARE(a.activeCustomers(), b.activeCustomers());
            for (int k = 0; k < C::kMaxCustomers; ++k) { QCOMPARE(a.customer(k).x, b.customer(k).x); QCOMPARE(a.customer(k).state, b.customer(k).state); }
            if (a.activeCustomers() != c.activeCustomers() || a.customer(0).seat != c.customer(0).seat) ++diff;
        }
        QVERIFY2(diff > 0, "another seed makes another day");
    }

    void theMaidIsStoppedByWallsAndTables()
    {
        C g;
        for (int f = 0; f < 600; ++f) g.frame(C::Left);
        QVERIFY(g.maidX() >= 0);
        QVERIFY(!C::solidAt(g.maidX() + 3, g.maidY() + 15));
        for (int f = 0; f < 600; ++f) g.frame(C::Up);
        QVERIFY(g.maidY() >= 0);
        QVERIFY(!C::solidAt(g.maidX() + 7, g.maidY() + 15));
    }

    void anEmptyDayIsGradeD() { QCOMPARE(NekoCafe().grade(), 'D'); }

    void theGameObjectRunsTheClockAndReportsTheEnd()
    {
        NekoCafeGame game;
        QSignalSpy done(&game, &NekoCafeGame::finished);
        QVERIFY(!game.started());
        game.start(5);
        QVERIFY(game.started());
        game.setKey(QStringLiteral("right"), true);
        game.advance(1000);
        QVERIFY(game.maidX() > C::kMaidStartX);
        game.releaseKeys();
        game.setPaused(true);
        const int x = game.maidX();
        game.advance(1000);
        QCOMPARE(game.maidX(), x);                    // paused: nothing moves
        game.setPaused(false);
        for (int i = 0; i < 6000 && game.phase() == 0; ++i) game.advance(100);
        QCOMPARE(game.phase(), 1);
        QCOMPARE(done.count(), 1);                    // reported once
        game.advance(5000);
        QCOMPARE(done.count(), 1);
        QVERIFY(game.resultsReady());
        game.again();
        QCOMPARE(game.phase(), 0);
        QCOMPARE(game.day(), 2);
        game.quit();
        QVERIFY(!game.started());
    }
};

QTEST_GUILESS_MAIN(TestNeko)
#include "test_neko.moc"
