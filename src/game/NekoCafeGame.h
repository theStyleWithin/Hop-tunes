// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "game/NekoCafe.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantList>

namespace hop {

// neko-cafe for the screen: NekoCafe's rules plus a 60 Hz clock and the input. NekoCafeApp.qml only draws what this exposes.
class NekoCafeGame : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool started READ started NOTIFY stateChanged)       // false on the title screen
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY stateChanged)
    Q_PROPERTY(bool open READ isOpen WRITE setOpen NOTIFY stateChanged)   // the game's screen is up: Space is its A button, not play/pause
    Q_PROPERTY(int phase READ phase NOTIFY frameChanged)            // 0 playing, 1 results
    Q_PROPERTY(bool resultsReady READ resultsReady NOTIFY frameChanged)
    Q_PROPERTY(int frame READ frame NOTIFY frameChanged)            // bumps every step: bindings that read the scene depend on it
    Q_PROPERTY(int maidX READ maidX NOTIFY frameChanged)
    Q_PROPERTY(int maidY READ maidY NOTIFY frameChanged)            // already includes the happy hop
    Q_PROPERTY(int maidDir READ maidDir NOTIFY frameChanged)
    Q_PROPERTY(int maidStep READ maidStep NOTIFY frameChanged)
    Q_PROPERTY(bool exclaim READ exclaim NOTIFY frameChanged)
    Q_PROPERTY(bool holdsFood READ holdsFood NOTIFY frameChanged)
    Q_PROPERTY(bool holdsTicket READ holdsTicket NOTIFY frameChanged)
    Q_PROPERTY(int cookX READ cookX NOTIFY frameChanged)
    Q_PROPERTY(int cookState READ cookState NOTIFY frameChanged)
    Q_PROPERTY(int cookStep READ cookStep NOTIFY frameChanged)
    Q_PROPERTY(int cookVx READ cookVx NOTIFY frameChanged)
    Q_PROPERTY(QVariantList customers READ customers NOTIFY frameChanged)
    Q_PROPERTY(double dayFraction READ dayFraction NOTIFY frameChanged)
    Q_PROPERTY(int day READ day NOTIFY stateChanged)
    Q_PROPERTY(int served READ served NOTIFY frameChanged)
    Q_PROPERTY(int happy READ happy NOTIFY frameChanged)
    Q_PROPERTY(int upset READ upset NOTIFY frameChanged)
    Q_PROPERTY(int bills READ bills NOTIFY frameChanged)
    Q_PROPERTY(int tips READ tips NOTIFY frameChanged)
    Q_PROPERTY(int happinessPct READ happinessPct NOTIFY frameChanged)
    Q_PROPERTY(QString grade READ grade NOTIFY frameChanged)
public:
    explicit NekoCafeGame(QObject *parent = nullptr);

    bool started() const { return m_started; }
    bool paused() const { return m_paused; }
    void setPaused(bool p);
    bool isOpen() const { return m_open; }
    void setOpen(bool o) { if (m_open != o) { m_open = o; emit stateChanged(); } }
    int phase() const { return m_g.phase(); }
    bool resultsReady() const { return m_g.resultsReady(); }
    int frame() const { return m_frame; }
    int maidX() const { return m_g.maidX(); }
    int maidY() const { return m_g.maidY() + m_g.maidJump(); }
    int maidDir() const { return m_g.maidDir(); }
    int maidStep() const { return m_g.maidStep(); }
    bool exclaim() const { return m_g.exclaim(); }
    bool holdsFood() const { return m_g.holdsFood(); }
    bool holdsTicket() const { return m_g.holdsTicket(); }
    int cookX() const { return m_g.cookX(); }
    int cookState() const { return m_g.cookState(); }
    int cookStep() const { return m_g.cookStep(); }
    int cookVx() const { return m_g.cookVx(); }
    QVariantList customers() const;
    double dayFraction() const { return qMin(1.0, double(m_g.shiftFrames()) / NekoCafe::kDayFrames); }
    int day() const { return m_g.dayNumber(); }
    int served() const { return m_g.served(); }
    int happy() const { return m_g.happy(); }
    int upset() const { return m_g.upset(); }
    int bills() const { return m_g.bills(); }
    int tips() const { return m_g.tips(); }
    int happinessPct() const { return m_g.happinessPct(); }
    QString grade() const { return QString(QChar(m_g.grade())); }
    NekoCafe &rules() { return m_g; }

    // "start" begins the first day from the title screen; "again" the next day from the results
    Q_INVOKABLE void start(quint32 seed = 0);
    Q_INVOKABLE void again();
    Q_INVOKABLE void quit();                                  // back to the title screen and stop the clock
    // keys: "left" "right" "up" "down" "a"
    Q_INVOKABLE void setKey(const QString &key, bool down);
    Q_INVOKABLE void releaseKeys() { m_keys = 0; }
    Q_INVOKABLE void advance(int ms);                         // the clock, also used by tests to play without waiting

signals:
    void frameChanged();
    void stateChanged();
    void finished(int profit, int served, int happy);          // the day ended (a full day: it only ends after closing time)

private:
    void tick();

    NekoCafe m_g;
    QTimer m_timer;
    QElapsedTimer m_clock;
    double m_acc = 0;
    int m_keys = 0, m_frame = 0;
    bool m_started = false, m_paused = false, m_sentFinished = false, m_open = false;
};

} // namespace hop
