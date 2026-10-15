// SPDX-License-Identifier: GPL-3.0-or-later
// MPRIS test on a private D-Bus session bus: drives Hop-tunes the way media keys,
// desktop widgets and playerctl do.
#include "platform/Mpris.h"
#include "playback/Player.h"

#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

// GIO has a struct member called `signals`, which Qt defines as a macro.
#pragma push_macro("signals")
#undef signals
#include <gio/gio.h>
#pragma pop_macro("signals")
#include <gst/gst.h>

using namespace hop;

namespace {

bool writeWav(const QString &path, int ms)
{
    const quint32 rate = 8000, bytes = rate * 2 * ms / 1000;
    QByteArray h;
    auto u32 = [&h](quint32 v) { char b[4]; qToLittleEndian(v, b); h.append(b, 4); };
    auto u16 = [&h](quint16 v) { char b[2]; qToLittleEndian(v, b); h.append(b, 2); };
    h.append("RIFF"); u32(36 + bytes); h.append("WAVE"); h.append("fmt "); u32(16); u16(1); u16(1);
    u32(rate); u32(rate * 2); u16(2); u16(16); h.append("data"); u32(bytes);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(h);
    f.write(QByteArray(int(bytes), '\0'));
    return true;
}

} // namespace

class TestMpris : public QObject {
    Q_OBJECT
    QProcess m_bus;
    GDBusConnection *m_client = nullptr;

    GVariant *prop(const char *iface, const char *name)
    {
        GVariant *r = g_dbus_connection_call_sync(m_client, "org.mpris.MediaPlayer2.hoptunes", "/org/mpris/MediaPlayer2",
                                                  "org.freedesktop.DBus.Properties", "Get", g_variant_new("(ss)", iface, name),
                                                  G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, nullptr);
        if (!r) return nullptr;
        GVariant *v = nullptr;
        g_variant_get(r, "(v)", &v);
        g_variant_unref(r);
        return v;
    }
    QString status()
    {
        GVariant *v = prop("org.mpris.MediaPlayer2.Player", "PlaybackStatus");
        const QString s = v ? QString::fromUtf8(g_variant_get_string(v, nullptr)) : QString();
        if (v) g_variant_unref(v);
        return s;
    }
    bool call(const char *method, GVariant *args = nullptr)
    {
        GVariant *r = g_dbus_connection_call_sync(m_client, "org.mpris.MediaPlayer2.hoptunes", "/org/mpris/MediaPlayer2",
                                                  "org.mpris.MediaPlayer2.Player", method, args, nullptr,
                                                  G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, nullptr);
        if (r) g_variant_unref(r);
        return r != nullptr;
    }

private slots:
    void initTestCase()
    {
        if (qEnvironmentVariableIsEmpty("HOPTUNES_AUDIO_SINK")) qputenv("HOPTUNES_AUDIO_SINK", "fakesink sync=true");
        gst_init(nullptr, nullptr);
        if (!gst_element_factory_find("wavparse")) QSKIP("GStreamer wavparse not installed");
        const QString daemon = QStandardPaths::findExecutable(QStringLiteral("dbus-daemon"));
        if (daemon.isEmpty()) QSKIP("dbus-daemon not installed");
        m_bus.start(daemon, {QStringLiteral("--session"), QStringLiteral("--nofork"), QStringLiteral("--print-address")});
        QVERIFY(m_bus.waitForReadyRead(5000));
        const QByteArray addr = m_bus.readLine().trimmed();
        QVERIFY(!addr.isEmpty());
        qputenv("DBUS_SESSION_BUS_ADDRESS", addr);
        m_client = g_dbus_connection_new_for_address_sync(addr.constData(),
            GDBusConnectionFlags(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION),
            nullptr, nullptr, nullptr);
        QVERIFY(m_client);
    }

    void cleanupTestCase()
    {
        if (m_client) g_object_unref(m_client);
        m_bus.kill();
        m_bus.waitForFinished();
    }

    void controlsPlayback()
    {
        QTemporaryDir dir;
        QVERIFY(writeWav(dir.filePath("a.wav"), 20000));
        QVERIFY(writeWav(dir.filePath("b.wav"), 20000));
        Track a; a.id = 1; a.path = dir.filePath("a.wav"); a.title = "First Song"; a.artist = "Test Artist"; a.durationMs = 20000;
        Track b = a; b.id = 2; b.path = dir.filePath("b.wav"); b.title = "Second Song";

        Player player;
        Mpris mpris(&player);
        mpris.start();
        QTRY_VERIFY_WITH_TIMEOUT(mpris.isRegistered(), 5000);

        GVariant *id = prop("org.mpris.MediaPlayer2", "Identity");
        QVERIFY(id);
        QCOMPARE(QString::fromUtf8(g_variant_get_string(id, nullptr)), QStringLiteral("Hop-tunes"));
        g_variant_unref(id);
        QCOMPARE(status(), QStringLiteral("Stopped"));

        player.setQueue({a, b}, 0);
        QTRY_COMPARE_WITH_TIMEOUT(status(), QStringLiteral("Playing"), 5000);
        GVariant *md = prop("org.mpris.MediaPlayer2.Player", "Metadata");
        QVERIFY(md);
        const char *title = nullptr;
        QVERIFY(g_variant_lookup(md, "xesam:title", "&s", &title));
        QCOMPARE(QString::fromUtf8(title), QStringLiteral("First Song"));
        gint64 len = 0;
        QVERIFY(g_variant_lookup(md, "mpris:length", "x", &len));
        QCOMPARE(len, gint64(20000) * 1000);
        g_variant_unref(md);

        QVERIFY(call("PlayPause"));                                   // the media key
        QTRY_VERIFY_WITH_TIMEOUT(!player.isPlaying(), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(status(), QStringLiteral("Paused"), 5000);

        QVERIFY(call("Next"));
        QTRY_COMPARE_WITH_TIMEOUT(player.currentIndex(), 1, 5000);

        // Volume is writable from desktop widgets.
        GVariant *r = g_dbus_connection_call_sync(m_client, "org.mpris.MediaPlayer2.hoptunes", "/org/mpris/MediaPlayer2",
            "org.freedesktop.DBus.Properties", "Set",
            g_variant_new("(ssv)", "org.mpris.MediaPlayer2.Player", "Volume", g_variant_new_double(0.5)),
            nullptr, G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, nullptr);
        QVERIFY(r);
        g_variant_unref(r);
        QTRY_COMPARE_WITH_TIMEOUT(player.volume(), 0.5, 5000);

        QSignalSpy seeked(&player, &Player::seeked);
        QVERIFY(call("SetPosition", g_variant_new("(ox)", "/io/github/thestylewithin/hoptunes/track/2", gint64(5'000'000))));
        QTRY_COMPARE_WITH_TIMEOUT(seeked.count(), 1, 5000);
        QCOMPARE(seeked.first().first().toLongLong(), qint64(5000));
    }
};

QTEST_GUILESS_MAIN(TestMpris)
#include "test_mpris.moc"
