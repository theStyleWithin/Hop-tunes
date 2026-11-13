// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/NekoCafe.h"

namespace hop {

namespace {
// The floor plan, 20x18 tiles (# wall, W window, k kitchen, S stove, C counter, R register, . floor, c chair, T table, P plant, D door).
const char *const kPlan[18] = {
    "####################", "###W###W###W###W####", "#kSSkkkkk#........P#", "#kkkkkkkk#.........#",
    "#kkkkkkkk#.........#", "#kkkkkkkk#.........#", "#CCCCRCkk..........#", "#..................#",
    "#.cTc...cTc...cTc..#", "#..................#", "#..................#", "#.cTc...cTc...cTc..#",
    "#..................#", "#..................#", "#.cTc...cTc...cTc..#", "#..................#",
    "#P................P#", "#########DD#########",
};
// For each seat: its chair tile and the route from the door to it (x, y of the sprite's top-left, six points).
const int kSeatCol[18] = {2, 4, 8, 10, 14, 16, 2, 4, 8, 10, 14, 16, 2, 4, 8, 10, 14, 16};
const int kSeatRow[18] = {8, 8, 8, 8, 8, 8, 11, 11, 11, 11, 11, 11, 14, 14, 14, 14, 14, 14};
const unsigned char kPath[18 * 12] = {
    72, 128, 72, 120, 48, 120, 48, 64, 16, 64, 16, 56,   72, 128, 72, 120, 48, 120, 48, 64, 32, 64, 32, 56,
    72, 128, 72, 120, 48, 120, 48, 64, 64, 64, 64, 56,   72, 128, 72, 120, 96, 120, 96, 64, 80, 64, 80, 56,
    72, 128, 72, 120, 96, 120, 96, 64, 112, 64, 112, 56, 72, 128, 72, 120, 96, 120, 96, 64, 128, 64, 128, 56,
    72, 128, 72, 120, 48, 120, 48, 88, 16, 88, 16, 80,   72, 128, 72, 120, 48, 120, 48, 88, 32, 88, 32, 80,
    72, 128, 72, 120, 48, 120, 48, 88, 64, 88, 64, 80,   72, 128, 72, 120, 96, 120, 96, 88, 80, 88, 80, 80,
    72, 128, 72, 120, 96, 120, 96, 88, 112, 88, 112, 80, 72, 128, 72, 120, 96, 120, 96, 88, 128, 88, 128, 80,
    72, 128, 72, 120, 48, 120, 48, 112, 16, 112, 16, 104, 72, 128, 72, 120, 48, 120, 48, 112, 32, 112, 32, 104,
    72, 128, 72, 120, 48, 120, 48, 112, 64, 112, 64, 104, 72, 128, 72, 120, 96, 120, 96, 112, 80, 112, 80, 104,
    72, 128, 72, 120, 96, 120, 96, 112, 112, 112, 112, 104, 72, 128, 72, 120, 96, 120, 96, 112, 128, 112, 128, 104,
};
const int kJumpDy[NekoCafe::kJumpFrames] = {0, -1, -3, -4, -5, -6, -7, -8, -9, -9, -10, -10, -11, -11, -11,
                                            -11, -11, -11, -10, -10, -9, -9, -8, -7, -6, -5, -4, -3, -1, 0};
// the maid's collision box (her feet and skirt), relative to the top-left of her 16x16 sprite
constexpr int kHbLeft = 3, kHbMid = 7, kHbRight = 12, kHbTop = 10, kHbBottom = 15;
constexpr int kWalkFrames = 8, kCustMoveWalkTicks = 4, kPhoneFrames = 40, kPaperFrames = 90;
} // namespace

bool NekoCafe::solidAt(int x, int y)
{
    if (x < 0 || y < 0) return true;
    const int tx = x >> 3, ty = y >> 3;
    if (tx >= 20 || ty >= 18) return true;
    switch (kPlan[ty][tx]) {
    case '#': case 'W': case 'C': case 'R': case 'T': case 'c': case 'P': case 'S': return true;
    default: return false;
    }
}

int NekoCafe::seatCol(int s) { return kSeatCol[s]; }
int NekoCafe::seatRow(int s) { return kSeatRow[s]; }
int NekoCafe::routeX(int seat, int i) { return kPath[seat * 12 + i * 2]; }
int NekoCafe::routeY(int seat, int i) { return kPath[seat * 12 + i * 2 + 1]; }
int NekoCafe::bubbleLeft(int seat)
{
    int left = kSeatCol[seat] >= 3 ? kSeatCol[seat] - 2 : 1;
    if (left > 20 - 1 - 5) left = 20 - 1 - 5;
    return left;
}
int NekoCafe::bubbleTop(int seat) { return kSeatRow[seat] - 3; }

quint32 NekoCafe::rnd(quint32 n)
{
    m_rng ^= m_rng << 13; m_rng ^= m_rng >> 17; m_rng ^= m_rng << 5;   // xorshift32
    return (m_rng >> 8) % n;
}

void NekoCafe::newDay(quint32 seed)
{
    m_rng = seed ? seed : 1;
    for (int i = 0; i < 4; ++i) rnd(2);
    m_day = qMin(m_day + 1, 99);
    m_phase = Playing; m_shift = 0; m_spawn = kFirstCustomer; m_resultsWait = 0; m_toggle = false; m_prevKeys = 0;
    m_mx = kMaidStartX; m_my = kMaidStartY; m_mdir = DirDown; m_mstep = 0; m_stepTimer = 0; m_jump = 0; m_excl = 0;
    m_ticket = m_food = kNone;
    m_cx = kCookHomeX; m_cstate = CookIdle; m_cvx = 1; m_cstep = 0; m_ccust = kNone;
    for (Customer &c : m_cust) c = Customer();
    for (bool &b : m_seatTaken) b = false;
    m_served = m_happy = m_upset = m_bills = m_tips = 0;
}

int NekoCafe::maidJump() const { return m_jump > 0 ? kJumpDy[kJumpFrames - m_jump] : 0; }
int NekoCafe::activeCustomers() const
{
    int n = 0;
    for (const Customer &c : m_cust) if (c.state != None) ++n;
    return n;
}
int NekoCafe::happinessPct() const
{
    const int total = m_happy + m_upset;
    return total ? m_happy * 100 / total : 0;
}
char NekoCafe::grade() const
{
    const int p = happinessPct();
    return p >= 80 ? 'A' : p >= 60 ? 'B' : p >= 40 ? 'C' : 'D';
}

bool NekoCafe::blocked(int x, int y) const
{
    return solidAt(x + kHbLeft, y + kHbTop) || solidAt(x + kHbMid, y + kHbTop) || solidAt(x + kHbRight, y + kHbTop)
        || solidAt(x + kHbLeft, y + kHbBottom) || solidAt(x + kHbMid, y + kHbBottom) || solidAt(x + kHbRight, y + kHbBottom);
}

bool NekoCafe::maidAtCounter() const
{
    const int mid = m_mx + kHbMid;
    return m_my >= kCounterMinY && m_my <= kCounterMaxY && mid >= kCounterMinX && mid <= kCounterMaxX;
}

// ---- the cook: idle -> cooking (paces) -> returning to his spot -> handing the order over -> idle
void NekoCafe::cookWalk()
{
    if (++m_canim >= kWalkFrames) { m_canim = 0; m_cstep ^= 1; }
    if (++m_cmove >= 2) { m_cmove = 0; m_cx += m_cvx; }
}

void NekoCafe::updateCook()
{
    switch (m_cstate) {
    case CookCooking:
        cookWalk();
        if (m_cx >= kCookRightX) m_cvx = -1;
        if (m_cx <= kCookLeftX) m_cvx = 1;
        if (++m_ctimer >= kCookCooking) m_cstate = CookReturning;
        break;
    case CookReturning:
        m_cvx = m_cx < kCookHomeX ? 1 : -1;
        if (m_cx == kCookHomeX) { m_cstate = CookHanding; m_chand = 0; }
        else cookWalk();
        break;
    case CookHanding:   // he holds the order out until the maid takes it; if the customer left he puts it away
        if (m_ccust == kNone && ++m_chand >= kCookHand) { m_cstate = CookIdle; m_cstep = 0; }
        break;
    default: break;
    }
}

// ---- customers
int NekoCafe::maxCustomersNow() const { return m_shift < kStep1 ? 2 : m_shift < kStep2 ? 3 : kMaxCustomers; }
int NekoCafe::nextSpawnDelay()
{
    if (m_shift < kStep1) return 1200 + int(rnd(600));
    if (m_shift < kStep2) return 900 + int(rnd(600));
    return 600 + int(rnd(600));
}
int NekoCafe::pickFreeSeat()
{
    int s = int(rnd(kSeats));
    for (int i = 0; i < kSeats; ++i) {
        if (!m_seatTaken[s]) return s;
        if (++s >= kSeats) s = 0;
    }
    return kNone;
}

void NekoCafe::spawnCustomer()
{
    int i = 0;
    while (i < kMaxCustomers && m_cust[i].state != None) ++i;
    if (i == kMaxCustomers) return;
    const int s = pickFreeSeat();
    if (s == kNone) return;
    Customer &c = m_cust[i];
    c = Customer();
    m_seatTaken[s] = true;
    c.state = Entering; c.seat = s; c.wp = 1;
    c.x = routeX(s, 0); c.y = routeY(s, 0); c.dir = DirUp;
    c.type = int(rnd(100)) < kPatientPct ? Patient : Hungry;
    c.activity = c.type == Patient ? ActPhone + int(rnd(2)) : ActNone;
    c.order = int(rnd(kCoffeeKinds));
    c.patience = c.type == Patient ? kPatientMin + int(rnd(kPatientRange)) : kHungryMin + int(rnd(kHungryRange));
    c.patienceMax = c.patience;
}

int NekoCafe::debugSpawn(int seat, int type, int patience)
{
    int i = 0;
    while (i < kMaxCustomers && m_cust[i].state != None) ++i;
    if (i == kMaxCustomers) return -1;
    Customer &c = m_cust[i];
    c = Customer();
    m_seatTaken[seat] = true;
    c.seat = seat; c.wp = kPathLen - 1; c.x = routeX(seat, kPathLen - 1); c.y = routeY(seat, kPathLen - 1);
    c.type = type; c.activity = type == Patient ? ActPhone : ActNone;
    c.patienceMax = c.patience = patience > 0 ? patience : (type == Patient ? kPatientMin : kHungryMin);
    c.state = Waiting;
    return i;
}

void NekoCafe::updateSpawning()
{
    if (m_shift >= kDayFrames - kLastCall) return;   // last call: nobody new walks in
    if (m_spawn > 0) { --m_spawn; return; }
    if (activeCustomers() < maxCustomersNow()) { spawnCustomer(); m_spawn = nextSpawnDelay(); }
    else m_spawn = 60;
}

void NekoCafe::startOrder(Customer &c)
{
    c.state = Ordering; c.bubbleTimer = kBubbleFrames; c.step = 0;
}

void NekoCafe::idleAnimate(Customer &c)
{
    if (c.activity == ActNone) return;
    const int limit = c.activity == ActPhone ? kPhoneFrames : kPaperFrames;
    if (++c.anim >= limit) { c.anim = 0; c.step ^= 1; }
}

void NekoCafe::walkCustomer(Customer &c)
{
    const int tx = routeX(c.seat, c.wp), ty = routeY(c.seat, c.wp);
    if (c.x != tx) { if (c.x < tx) { ++c.x; c.dir = DirRight; } else { --c.x; c.dir = DirLeft; } }
    else if (c.y != ty) { if (c.y < ty) { ++c.y; c.dir = DirDown; } else { --c.y; c.dir = DirUp; } }
    if (++c.anim >= kCustMoveWalkTicks) { c.anim = 0; c.step ^= 1; }
    if (c.x != tx || c.y != ty) return;

    if (c.state == Entering) {
        if (c.wp + 1 >= kPathLen) {   // at the chair: a hungry customer orders at once, a patient one settles in first
            c.dir = DirDown; c.step = 0; c.anim = int(rnd(30));
            if (c.type == Hungry) startOrder(c);
            else { c.state = Settling; c.settleTimer = kSettleMin + int(rnd(kSettleRange)); }
        } else ++c.wp;
    } else {   // Leaving
        if (c.wp == 0) { m_seatTaken[c.seat] = false; c.state = None; }
        else --c.wp;
    }
}

void NekoCafe::forget(int i)
{
    if (m_ccust == i) m_ccust = kNone;
    if (m_ticket == i) m_ticket = kNone;
    if (m_food == i) m_food = kNone;
}

void NekoCafe::startLeaving(Customer &c)
{
    c.state = Leaving; c.wp = kPathLen - 2; c.step = 0; c.anim = 0;
}

void NekoCafe::leaveUpset(int i)
{
    Customer &c = m_cust[i];
    c.mood = MoodUpset;
    ++m_upset;
    forget(i);
    startLeaving(c);
}

void NekoCafe::react(Customer &c, int mood)
{
    c.state = Reacting; c.mood = mood; c.reactTimer = kReactFrames; c.step = 0;
    if (mood == MoodUpset) ++m_upset;
}

// How happy a customer is depends on how much of their patience was left: a big tip, a small one, then nothing at all (and no
// bill). A few leave upset whatever you do.
void NekoCafe::serveRight(int i)
{
    Customer &c = m_cust[i];
    bool happy = false;
    int tip = 0;
    m_food = kNone;
    const int pct = int(qint64(c.patience) * 100 / c.patienceMax);
    if (pct >= kTipFastPct) { happy = true; tip = kTipFast; }
    else if (pct >= kTipOkPct) { happy = true; tip = kTipOk; }
    if (happy && int(rnd(100)) < kGrumpyPct) { happy = false; tip = 0; }
    ++m_served;
    if (happy) { ++m_happy; m_bills += kBill; m_tips += tip; react(c, MoodHappy); }
    else react(c, MoodUpset);
}

void NekoCafe::serveWrong(int i)
{
    forget(i);
    react(m_cust[i], MoodUpset);
}

int NekoCafe::reach(const Customer &c) const
{
    int dx = (m_mx + 8) - (c.x + 4), dy = (m_my + 8) - (c.y + 8);
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    if (dx > kReachX || dy > kReachY) return 255;
    return dx + dy;
}

// A: 1. carrying a coffee and its customer is within reach: serve them. 2. at the counter with a slip and the cook free: hand it
// in. 3. beside a waiting customer: take their order (if none was taken and no slip is carried), or, if they already ordered and
// the maid carries someone else's coffee, they get it by mistake and leave upset.
void NekoCafe::aButton()
{
    if (m_food != kNone && reach(m_cust[m_food]) != 255) { serveRight(m_food); return; }
    if (m_ticket != kNone && maidAtCounter() && m_cstate == CookIdle) {
        m_ccust = m_ticket; m_cstate = CookCooking; m_cvx = 1; m_ctimer = 0; m_cmove = 0; m_canim = 0;
        m_ticket = kNone;
        return;
    }
    int best = kNone, bestD = 255;
    for (int i = 0; i < kMaxCustomers; ++i) {
        if (!waiting(m_cust[i])) continue;
        const int d = reach(m_cust[i]);
        if (d < bestD) { bestD = d; best = i; }
    }
    if (best == kNone) return;
    if (!m_cust[best].taken) {
        if (m_ticket == kNone) { m_cust[best].taken = true; m_ticket = best; }
    } else if (m_food != kNone) serveWrong(best);
}

void NekoCafe::updatePickup()
{
    if (m_cstate == CookHanding && m_ccust != kNone && m_food == kNone && m_jump == 0 && maidAtCounter()) {
        m_food = m_ccust; m_ccust = kNone; m_cstate = CookIdle; m_cstep = 0;
        m_jump = kJumpFrames; m_excl = kExclFrames;
    }
}

void NekoCafe::updateCustomers()
{
    const bool closing = m_shift >= kDayFrames + kCloseGrace;
    for (int i = 0; i < kMaxCustomers; ++i) {
        Customer &c = m_cust[i];
        if (closing && (c.state == Settling || waiting(c))) { leaveUpset(i); continue; }   // anyone still waiting gives up
        switch (c.state) {
        case Entering: case Leaving:
            if (m_toggle) walkCustomer(c);   // one pixel every other frame
            break;
        case Settling:
            idleAnimate(c);
            if (--c.settleTimer == 0) startOrder(c);
            break;
        case Ordering:
            if (--c.bubbleTimer == 0) { c.state = Waiting; c.step = 0; }
            if (c.patience > 0) --c.patience;
            if (c.patience == 0) leaveUpset(i);
            break;
        case Waiting:
            idleAnimate(c);
            if (c.patience > 0) --c.patience;
            if (c.patience == 0) leaveUpset(i);
            break;
        case Reacting:
            if (--c.reactTimer == 0) startLeaving(c);
            break;
        default: break;
        }
    }
}

void NekoCafe::showResults()
{
    m_phase = Results;
    m_resultsWait = kResultsWait;
}

void NekoCafe::frame(int keys)
{
    const int pressed = keys & ~m_prevKeys;
    m_prevKeys = keys;
    if (m_phase == Results) {
        if (m_resultsWait > 0) --m_resultsWait;
        return;
    }
    if (m_shift < 65535) ++m_shift;
    m_toggle = !m_toggle;
    if (m_jump > 0) --m_jump;
    if (m_excl > 0) --m_excl;

    bool moved = false;   // one pixel per frame, one axis at a time so she slides along walls; no moving while she hops
    if (m_jump == 0) {
        if (keys & Left)  { m_mdir = DirLeft;  if (!blocked(m_mx - 1, m_my)) { --m_mx; moved = true; } }
        if (keys & Right) { m_mdir = DirRight; if (!blocked(m_mx + 1, m_my)) { ++m_mx; moved = true; } }
        if (keys & Up)    { m_mdir = DirUp;    if (!blocked(m_mx, m_my - 1)) { --m_my; moved = true; } }
        if (keys & Down)  { m_mdir = DirDown;  if (!blocked(m_mx, m_my + 1)) { ++m_my; moved = true; } }
        if (pressed & A) aButton();
    }
    if (moved) { if (++m_stepTimer >= kWalkFrames) { m_stepTimer = 0; m_mstep ^= 1; } }
    else { m_stepTimer = 0; m_mstep = 0; }

    updateCook();
    updatePickup();
    updateSpawning();
    updateCustomers();
    if (m_shift >= kDayFrames && activeCustomers() == 0) showResults();   // the clock ran out and the last customer left
}

} // namespace hop
