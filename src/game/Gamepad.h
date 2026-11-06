// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QSocketNotifier>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <memory>

namespace hop {

// A game controller as plain actions, for the games in Now playing. Linux reads the kernel's joystick device
// (/dev/input/js*, no extra library), Windows asks XInput. Both give the same action names, so the screen does not
// care which one it is: "left" "right" "up" "down" (d-pad or left stick), "a" "b" "x" "y", "lb" "rb", "start" "back".
// Holding left, right or down repeats like a key. Controllers that do not follow the Xbox layout still work for the
// d-pad and the first four buttons.
class Gamepad : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString name READ name NOTIFY connectedChanged)
    Q_PROPERTY(QStringList recentEvents READ recentEvents NOTIFY eventsChanged)   // the last 20 raw controller events, for diagnostics
public:
    explicit Gamepad(QObject *parent = nullptr);
    ~Gamepad() override;

    bool connected() const { return m_connected; }
    QString name() const { return m_name; }
    // Only look for a controller while something wants it (Now playing's games); nothing is opened otherwise.
    Q_INVOKABLE void setListening(bool on);

    QStringList recentEvents() const { return m_events; }

    // for tests and for the keyboard-free paths: behaves as if the controller did this
    void feedAction(const QString &action, bool down);
    // A controller was plugged in or removed (name "" = none). Android learns this from its activity; also for tests.
    void setDevice(const QString &name);
    void logEvent(const QString &text);
    // Android: the activity (HopActivity.java) reports controller events here; the one Gamepad that is listening gets them.
    static Gamepad *instance() { return s_instance; }
#if defined(Q_OS_ANDROID)
    void registerAndroid();
    static void androidSetListening(bool on);
#endif

signals:
    void eventsChanged();
    void connectedChanged();
    void pressed(const QString &action);
    void released(const QString &action);   // for games that hold a button (pinball flippers)

private:
    void scan();
    void readLinux();
    void pollWindows();
    void hold(const QString &action, bool down);
    void repeatTick();
    void setConnected(bool c, const QString &name = {});

    static Gamepad *s_instance;
    QStringList m_events;
    bool m_connected = false, m_listening = false;
    QString m_name;
    QTimer m_scan, m_repeat;
    QString m_held;          // the direction being held, for auto-repeat
    int m_repeatPhase = 0;
    int m_fd = -1;
    std::unique_ptr<QSocketNotifier> m_notifier;
    bool m_axisDir[4] = {false, false, false, false};   // left, right, up, down from the stick / d-pad axes
    quint16 m_winButtons = 0;
};

} // namespace hop
