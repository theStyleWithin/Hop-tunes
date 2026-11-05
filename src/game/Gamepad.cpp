// SPDX-License-Identifier: GPL-3.0-or-later
#include "game/Gamepad.h"

#include <QDir>
#include <QFile>
#if defined(Q_OS_ANDROID)
#  include <QJniEnvironment>
#  include <QJniObject>
#endif
#include <QLibrary>

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
#  include <cerrno>
#  include <cstdio>
#  include <fcntl.h>
#  include <linux/joystick.h>
#  include <sys/ioctl.h>
#  include <unistd.h>
#endif

namespace hop {

namespace {
const char *const kDirs[] = {"left", "right", "up", "down"};

// Xbox-style layout as the Linux joystick API numbers it
const char *buttonName(int b)
{
    static const char *const names[] = {"a", "b", "x", "y", "lb", "rb", "back", "start"};
    return b >= 0 && b < 8 ? names[b] : nullptr;
}
}

Gamepad *Gamepad::s_instance = nullptr;

Gamepad::Gamepad(QObject *parent) : QObject(parent)
{
    s_instance = this;
    m_scan.setInterval(2000);
    connect(&m_scan, &QTimer::timeout, this, &Gamepad::scan);
    m_repeat.setInterval(70);
    connect(&m_repeat, &QTimer::timeout, this, &Gamepad::repeatTick);
}

Gamepad::~Gamepad() { setListening(false); if (s_instance == this) s_instance = nullptr; }

void Gamepad::setListening(bool on)
{
    if (on == m_listening) return;
    m_listening = on;
#if defined(Q_OS_ANDROID)
    androidSetListening(on);   // tell the activity whether a game wants the controller to itself
#endif
    if (on) { scan(); m_scan.start(); }
    else {
        m_scan.stop();
        m_repeat.stop();
        m_held.clear();
        m_notifier.reset();
#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)
        if (m_fd >= 0) ::close(m_fd);
#endif
        m_fd = -1;
        setConnected(false);
    }
}

void Gamepad::setConnected(bool c, const QString &name)
{
    if (c == m_connected && name == m_name) return;
    m_connected = c;
    m_name = c ? name : QString();
    emit connectedChanged();
}

void Gamepad::hold(const QString &action, bool down)
{
    if (action == QLatin1String("left") || action == QLatin1String("right") || action == QLatin1String("down")) {
        if (down) { m_held = action; m_repeatPhase = 0; m_repeat.setInterval(180); m_repeat.start(); }
        else if (m_held == action) { m_held.clear(); m_repeat.stop(); }
    }
    if (down) emit pressed(action);
}

void Gamepad::repeatTick()
{
    if (m_held.isEmpty()) { m_repeat.stop(); return; }
    m_repeat.setInterval(70);
    emit pressed(m_held);
}

void Gamepad::feedAction(const QString &action, bool down) { hold(action, down); }

void Gamepad::setDevice(const QString &name)
{
    if (name.isEmpty()) { m_held.clear(); m_repeat.stop(); setConnected(false); }
    else setConnected(true, name);
}

void Gamepad::logEvent(const QString &text)
{
    m_events.append(text);
    while (m_events.size() > 20) m_events.removeFirst();
    emit eventsChanged();
}

#if defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID)

void Gamepad::scan()
{
    if (m_fd >= 0) return;   // already have one; a pulled plug shows up as a read error
    for (const QString &n : QDir(QStringLiteral("/dev/input")).entryList({QStringLiteral("js*")}, QDir::System | QDir::NoDotAndDotDot, QDir::Name)) {
        const QByteArray path = QFile::encodeName(QStringLiteral("/dev/input/") + n);
        const int fd = ::open(path.constData(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;
        char name[128] = {0};
        if (ioctl(fd, JSIOCGNAME(sizeof(name)), name) < 0) std::snprintf(name, sizeof(name), "game controller");
        m_fd = fd;
        for (bool &d : m_axisDir) d = false;
        m_notifier = std::make_unique<QSocketNotifier>(fd, QSocketNotifier::Read);
        connect(m_notifier.get(), &QSocketNotifier::activated, this, &Gamepad::readLinux);
        setConnected(true, QString::fromLocal8Bit(name));
        return;
    }
}

void Gamepad::readLinux()
{
    struct js_event e;
    while (m_fd >= 0) {
        const ssize_t n = ::read(m_fd, &e, sizeof(e));
        if (n != sizeof(e)) {
            if (n < 0 && errno != EAGAIN) {   // unplugged
                m_notifier.reset();
                ::close(m_fd);
                m_fd = -1;
                m_held.clear();
                m_repeat.stop();
                setConnected(false);
            }
            return;
        }
        e.type &= ~JS_EVENT_INIT;
        if (e.type == JS_EVENT_BUTTON) {
            if (const char *b = buttonName(e.number)) hold(QLatin1String(b), e.value != 0);
        } else if (e.type == JS_EVENT_AXIS) {
            // d-pad is axes 6/7 on Xbox-style pads; the left stick is axes 0/1
            const bool x = e.number == 0 || e.number == 6, y = e.number == 1 || e.number == 7;
            if (x) { const bool l = e.value < -16000, r = e.value > 16000;
                     if (l != m_axisDir[0]) { m_axisDir[0] = l; hold(QLatin1String(kDirs[0]), l); }
                     if (r != m_axisDir[1]) { m_axisDir[1] = r; hold(QLatin1String(kDirs[1]), r); } }
            if (y) { const bool u = e.value < -16000, d = e.value > 16000;
                     if (u != m_axisDir[2]) { m_axisDir[2] = u; hold(QLatin1String(kDirs[2]), u); }
                     if (d != m_axisDir[3]) { m_axisDir[3] = d; hold(QLatin1String(kDirs[3]), d); } }
        }
    }
}

void Gamepad::pollWindows() {}

#elif defined(Q_OS_WIN)

namespace {
struct XState { quint32 packet; quint16 buttons; quint8 lt, rt; qint16 lx, ly, rx, ry; };
using XGetState = quint32 (__stdcall *)(quint32, XState *);
XGetState xinput()
{
    static XGetState fn = [] {
        for (const char *dll : {"xinput1_4", "xinput1_3", "xinput9_1_0"}) {
            QLibrary lib(QString::fromLatin1(dll));
            if (lib.load()) if (auto f = reinterpret_cast<XGetState>(lib.resolve("XInputGetState"))) return f;
        }
        return XGetState(nullptr);
    }();
    return fn;
}
}

void Gamepad::scan()
{
    if (!m_scan.isActive()) return;
    if (!xinput()) return;
    // poll at 60 Hz while a pad is there, every 2 s while looking
    pollWindows();
}

void Gamepad::pollWindows()
{
    XGetState get = xinput();
    if (!get) return;
    XState st{};
    if (get(0, &st) != 0) {   // not connected
        if (m_connected) { m_held.clear(); m_repeat.stop(); setConnected(false); m_winButtons = 0; }
        m_scan.setInterval(2000);
        return;
    }
    if (!m_connected) setConnected(true, QStringLiteral("Xbox controller"));
    m_scan.setInterval(16);
    struct Map { quint16 mask; const char *name; };
    static const Map map[] = {{0x0001, "up"}, {0x0002, "down"}, {0x0004, "left"}, {0x0008, "right"}, {0x0010, "start"}, {0x0020, "back"},
                              {0x0100, "lb"}, {0x0200, "rb"}, {0x1000, "a"}, {0x2000, "b"}, {0x4000, "x"}, {0x8000, "y"}};
    quint16 b = st.buttons;
    if (st.lx < -16000) b |= 0x0004; if (st.lx > 16000) b |= 0x0008;
    if (st.ly > 16000) b |= 0x0001; if (st.ly < -16000) b |= 0x0002;
    for (const Map &m : map) {
        const bool now = b & m.mask, was = m_winButtons & m.mask;
        if (now != was) hold(QLatin1String(m.name), now);
    }
    m_winButtons = b;
}

#elif defined(Q_OS_ANDROID)

// Android: nothing to scan, the activity (android/src/.../HopActivity.java) reports the controller and its buttons.
void Gamepad::scan() {}
void Gamepad::pollWindows() {}

#else

void Gamepad::scan() {}
void Gamepad::pollWindows() {}

#endif

#if !(defined(Q_OS_LINUX) && !defined(Q_OS_ANDROID))
void Gamepad::readLinux() {}
#endif

#if defined(Q_OS_ANDROID)

void Gamepad::androidSetListening(bool on)
{
    QJniObject::callStaticMethod<void>("io/github/thestylewithin/hoptunes/HopActivity", "setListening", "(Z)V", jboolean(on));
}

namespace {
// These run on the Android UI thread; everything is handed over to the Qt thread.
void jniAction(JNIEnv *env, jclass, jstring action, jboolean down)
{
    const QString a = QJniObject(action).toString();
    Q_UNUSED(env);
    if (Gamepad *g = Gamepad::instance())
        QMetaObject::invokeMethod(g, [g, a, down] { g->feedAction(a, down); }, Qt::QueuedConnection);
}
void jniRaw(JNIEnv *, jclass, jstring text)
{
    const QString t = QJniObject(text).toString();
    if (Gamepad *g = Gamepad::instance())
        QMetaObject::invokeMethod(g, [g, t] { g->logEvent(t); }, Qt::QueuedConnection);
}
void jniDevice(JNIEnv *, jclass, jstring name)
{
    const QString n = QJniObject(name).toString();
    if (Gamepad *g = Gamepad::instance())
        QMetaObject::invokeMethod(g, [g, n] { g->logEvent(n.isEmpty() ? QStringLiteral("controller: none") : QStringLiteral("controller: ") + n); g->setDevice(n); },
                                  Qt::QueuedConnection);
}
}

// Called once from main(): connects HopActivity's native methods to the functions above and asks for the current controller.
void Gamepad::registerAndroid()
{
    QJniEnvironment env;
    const JNINativeMethod methods[] = {
        {"nativeAction", "(Ljava/lang/String;Z)V", reinterpret_cast<void *>(jniAction)},
        {"nativeRaw", "(Ljava/lang/String;)V", reinterpret_cast<void *>(jniRaw)},
        {"nativeDevice", "(Ljava/lang/String;)V", reinterpret_cast<void *>(jniDevice)},
    };
    env.registerNativeMethods("io/github/thestylewithin/hoptunes/HopActivity", methods, 3);
    const QJniObject name = QJniObject::callStaticObjectMethod("io/github/thestylewithin/hoptunes/HopActivity", "controllerName", "()Ljava/lang/String;");
    if (name.isValid()) { const QString n = name.toString(); if (!n.isEmpty()) setDevice(n); }
}

#endif

} // namespace hop
