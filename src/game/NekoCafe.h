// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QVector>
#include <QtGlobal>

namespace hop {

// The rules of neko-cafe, a top-down cafe sim: you are a neko maid taking orders, fetching coffee from the cook and serving it.
// Ported from the Game Boy original, frame for frame: the screen is 160x144 pixels and one frame() is 1/60 s. No Qt Quick and
// no clock in here (deterministic for a seed, unit-tested); the screen (NekoCafeApp.qml) and the 60 Hz clock (NekoCafeGame) plug in.
//
// The loop: a customer walks in, sits, and orders; press A beside them to take the order, A at the counter to hand the slip to the
// cook, wait while he cooks, stand at the counter to pick the coffee up, then A beside the right customer. The faster you serve,
// the happier they are: a happy customer pays the bill and tips, an upset one pays nothing. The day lasts 5 minutes.
class NekoCafe {
public:
    // ---- the numbers (frames are 1/60 s, money is in cents)
    static constexpr int kW = 160, kH = 144;
    static constexpr int kDayFrames = 18000, kLastCall = 1800, kCloseGrace = 900, kResultsWait = 90;
    static constexpr int kBill = 795, kTipFastPct = 40, kTipFast = 200, kTipOkPct = 15, kTipOk = 100, kGrumpyPct = 10;
    static constexpr int kReachX = 12, kReachY = 18;
    static constexpr int kJumpFrames = 30, kExclFrames = 45, kReactFrames = 100;
    static constexpr int kMaxCustomers = 6, kSeats = 18, kPathLen = 6, kCoffeeKinds = 4;
    static constexpr int kPatientPct = 50, kPatientMin = 2400, kPatientRange = 1200, kHungryMin = 700, kHungryRange = 400;
    static constexpr int kSettleMin = 180, kSettleRange = 180, kBubbleFrames = 150;
    static constexpr int kStep1 = 3600, kStep2 = 10800, kFirstCustomer = 180;
    static constexpr int kCookCooking = 180, kCookHand = 40, kCookHomeX = 32, kCookY = 32, kCookLeftX = 12, kCookRightX = 52;
    static constexpr int kMaidStartX = 44, kMaidStartY = 48;
    static constexpr int kCounterMinX = 8, kCounterMaxX = 71, kCounterMinY = 46, kCounterMaxY = 52;

    // keys, as a bit mask
    enum Key { Left = 1, Right = 2, Up = 4, Down = 8, A = 16 };
    enum Dir { DirDown, DirUp, DirRight, DirLeft };
    enum CookState { CookIdle, CookCooking, CookReturning, CookHanding };
    enum CustState { None, Entering, Settling, Ordering, Waiting, Reacting, Leaving };
    enum Mood { MoodNone, MoodHappy, MoodUpset };
    enum Phase { Playing, Results };
    enum Type { Patient, Hungry };
    enum Activity { ActNone, ActPhone, ActPaper };
    static constexpr int kNone = 255;

    struct Customer {
        int state = None;
        int seat = 0, wp = 0;
        int x = 0, y = 0;                 // top-left of the 8x16 sprite
        int dir = DirDown, step = 0, anim = 0;
        int type = Patient, activity = ActNone, order = 0;
        int bubbleTimer = 0, settleTimer = 0;
        int patience = 0, patienceMax = 1;
        bool taken = false;
        int mood = MoodNone, reactTimer = 0;
    };

    explicit NekoCafe(quint32 seed = 1) { newDay(seed); }
    void newDay(quint32 seed);
    void frame(int keys);                 // one 1/60 s step

    // ---- what the screen shows
    int phase() const { return m_phase; }
    int shiftFrames() const { return m_shift; }
    int dayNumber() const { return m_day; }
    int maidX() const { return m_mx; }
    int maidY() const { return m_my; }
    int maidDir() const { return m_mdir; }
    int maidStep() const { return m_mstep; }
    int maidJump() const;                 // the hop's vertical offset (0 or negative pixels)
    bool exclaim() const { return m_excl > 0; }
    bool holdsFood() const { return m_food != kNone; }
    bool holdsTicket() const { return m_ticket != kNone; }
    int cookX() const { return m_cx; }
    int cookState() const { return m_cstate; }
    int cookStep() const { return m_cstep; }
    int cookVx() const { return m_cvx; }
    const Customer &customer(int i) const { return m_cust[i]; }
    int activeCustomers() const;
    bool resultsReady() const { return m_phase == Results && m_resultsWait == 0; }
    // the day's tally
    int served() const { return m_served; }
    int happy() const { return m_happy; }
    int upset() const { return m_upset; }
    int bills() const { return m_bills; }
    int tips() const { return m_tips; }
    int profit() const { return m_bills + m_tips; }
    int happinessPct() const;
    char grade() const;

    // ---- the floor plan and the seats
    static bool solidAt(int x, int y);
    static int seatCol(int s);
    static int seatRow(int s);
    static int routeX(int seat, int i);
    static int routeY(int seat, int i);
    static int bubbleLeft(int seat);      // tile column and row of a customer's order bubble (5x2 tiles)
    static int bubbleTop(int seat);

    // ---- for tests
    int debugSpawn(int seat, int type, int patience = -1);   // seat a customer at once (index), skipping the walk in
    void debugSetMaid(int x, int y) { m_mx = x; m_my = y; }
    void debugSetShift(int f) { m_shift = f; }
    Customer &debugCustomer(int i) { return m_cust[i]; }
    int debugTicket() const { return m_ticket; }
    int debugFood() const { return m_food; }
    int debugCook() const { return m_ccust; }
    bool maidAtCounter() const;

private:
    quint32 rnd(quint32 n);
    bool blocked(int x, int y) const;
    void aButton();
    void updateCook();
    void cookWalk();
    void updatePickup();
    void updateSpawning();
    void updateCustomers();
    void spawnCustomer();
    void startOrder(Customer &c);
    void walkCustomer(Customer &c);
    void idleAnimate(Customer &c);
    void forget(int i);
    void startLeaving(Customer &c);
    void leaveUpset(int i);
    void react(Customer &c, int mood);
    void serveRight(int i);
    void serveWrong(int i);
    int reach(const Customer &c) const;
    int maxCustomersNow() const;
    int nextSpawnDelay();
    int pickFreeSeat();
    void showResults();
    static bool waiting(const Customer &c) { return c.state == Ordering || c.state == Waiting; }

    quint32 m_rng = 1;
    int m_phase = Playing, m_day = 0, m_shift = 0, m_spawn = 0, m_resultsWait = 0;
    bool m_toggle = false;
    int m_mx = 0, m_my = 0, m_mdir = DirDown, m_mstep = 0, m_stepTimer = 0, m_jump = 0, m_excl = 0;
    int m_cx = 0, m_cstate = CookIdle, m_cvx = 1, m_cstep = 0, m_canim = 0, m_cmove = 0, m_chand = 0, m_ctimer = 0, m_ccust = kNone;
    int m_ticket = kNone, m_food = kNone;
    Customer m_cust[kMaxCustomers];
    bool m_seatTaken[kSeats] = {};
    int m_prevKeys = 0;
    int m_served = 0, m_happy = 0, m_upset = 0, m_bills = 0, m_tips = 0;
};

} // namespace hop
