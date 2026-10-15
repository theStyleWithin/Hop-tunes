// SPDX-License-Identifier: GPL-3.0-or-later
// End-to-end profile flows through the real QML UI (offscreen): first run, PIN curtain,
// switching, guest mode, removal, restart behaviour.
#include "App.h"

#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

#include <gst/gst.h>

#include <memory>

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

class TestUi : public QObject {
    Q_OBJECT
    QTemporaryDir m_home;
    std::unique_ptr<App> m_app;
    std::unique_ptr<QQmlApplicationEngine> m_engine;

    QQuickWindow *window() { return qobject_cast<QQuickWindow *>(m_engine->rootObjects().constFirst()); }
    // Visual-tree search: Repeater delegates are not QObject children of their parent.
    static QQuickItem *find(QQuickItem *root, const QString &name)
    {
        if (root->objectName() == name) return root;
        for (QQuickItem *c : root->childItems())
            if (QQuickItem *hit = find(c, name)) return hit;
        return nullptr;
    }
    QQuickItem *item(const char *name)
    {
        auto *i = find(window()->contentItem(), QLatin1String(name));
        if (!i) qWarning("no item %s", name);
        return i;
    }
    bool click(const char *name)
    {
        QQuickItem *i = item(name);
        if (!i || !i->isVisible() || !i->isEnabled()) { qWarning("cannot click %s", name); return false; }
        const QPointF c = i->mapToScene(QPointF(i->width() / 2, i->height() / 2));
        QTest::mouseClick(window(), Qt::LeftButton, Qt::NoModifier, c.toPoint());
        return true;
    }
    void type(const char *name, const QString &text)
    {
        QQuickItem *i = item(name);
        QVERIFY(i);
        i->forceActiveFocus();
        i->setProperty("text", QString());
        for (const QChar c : text) QTest::keyClick(window(), c.toLatin1());
    }
    // HOPTUNES_UI_SHOTS=<dir> saves design renders of each screen while the test runs.
    void shot(const char *name)
    {
        const QString dir = qEnvironmentVariable("HOPTUNES_UI_SHOTS");
        if (dir.isEmpty()) return;
        QTest::qWait(300);
        window()->grabWindow().save(dir + QLatin1Char('/') + QLatin1String(name) + QStringLiteral(".png"));
    }
    QString gateMode() { return item("profileGate")->property("mode").toString(); }

    void start()
    {
        m_engine.reset();
        m_app.reset(new App);
        QVERIFY(m_app->init());
        m_engine.reset(new QQmlApplicationEngine);
        m_engine->rootContext()->setContextProperty(QStringLiteral("hop"), m_app.get());
        m_engine->rootContext()->setContextProperty(QStringLiteral("initialView"), QString());
        m_engine->load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
        QVERIFY(!m_engine->rootObjects().isEmpty());
        QVERIFY(QTest::qWaitForWindowExposed(window()));
    }

private slots:
    void initTestCase()
    {
        qputenv("HOME", m_home.path().toLocal8Bit());
        for (const char *v : {"XDG_DATA_HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME", "HOPTUNES_PROFILE"}) qunsetenv(v);
        qputenv("HOPTUNES_AUDIO_SINK", "fakesink sync=true");
        QDir().mkpath(m_home.filePath(QStringLiteral("Music/A/B")));
        QVERIFY(writeWav(m_home.filePath(QStringLiteral("Music/A/B/01 Song.wav")), 3000));
        QQuickStyle::setStyle(QStringLiteral("Basic"));
    }

    void cleanupTestCase() { m_engine.reset(); m_app.reset(); }

    void firstRunCreatesProfileWithPin()
    {
        start();
        QVERIFY(!m_app->profileReady());
        QCOMPARE(gateMode(), QStringLiteral("create"));
        QCOMPARE(m_app->suggestedFolder(), QFileInfo(m_home.filePath(QStringLiteral("Music"))).canonicalFilePath());

        type("profileName", QStringLiteral("Jo"));
        QVERIFY(click("usePin"));
        type("pin1", QStringLiteral("2468"));
        type("pin2", QStringLiteral("2467"));
        QVERIFY(click("createButton"));
        QVERIFY(!m_app->profileReady());             // mismatched PINs are refused
        type("pin2", QStringLiteral("2468"));
        shot("profile-create");
        QVERIFY(click("createButton"));
        QTRY_VERIFY(m_app->profileReady());
        QCOMPARE(m_app->profile().value("name").toString(), QStringLiteral("Jo"));
        QVERIFY(m_app->profile().value("hasPin").toBool());
        QTRY_COMPARE_WITH_TIMEOUT(m_app->songs()->count(), 1, 10000);   // ~/Music was adopted
        QVERIFY(item("profileChip")->isVisible());
    }

    void switchShowsPinCurtain()
    {
        m_app->playSongs(0);
        QTRY_VERIFY(m_app->player()->isPlaying());
        QVERIFY(click("profileChip"));
        QTRY_VERIFY(!m_app->profileReady());
        QVERIFY(!m_app->player()->hasTrack());       // playback stops on switch
        QCOMPARE(m_app->songs()->count(), 0);        // nothing from the last profile shows
        QCOMPARE(gateMode(), QStringLiteral("pick"));

        QVERIFY(click("tile_Jo"));
        QCOMPARE(gateMode(), QStringLiteral("pin"));
        type("pinEntry", QStringLiteral("1111"));
        QTest::keyClick(window(), Qt::Key_Return);
        QVERIFY(!m_app->profileReady());
        QCOMPARE(item("pinError")->property("text").toString(), QStringLiteral("wrong PIN"));
        shot("profile-pin");
        type("pinEntry", QStringLiteral("2468"));
        QVERIFY(click("unlock"));
        QTRY_VERIFY(m_app->profileReady());
    }

    void pinLockout()
    {
        const QString id = m_app->profile().value("id").toString();
        m_app->leaveProfile();
        for (int i = 0; i < 5; ++i) QCOMPARE(m_app->openProfile(id, QStringLiteral("0000")), QStringLiteral("wrong PIN"));
        QVERIFY(m_app->openProfile(id, QStringLiteral("2468")).startsWith(QStringLiteral("too many tries")));
        QVERIFY(!m_app->profileReady());
    }

    void guestKeepsNothing()
    {
        QCOMPARE(gateMode(), QStringLiteral("pick"));
        const QStringList before = QDir(m_home.filePath(QStringLiteral(".local/share/hop-tunes/profiles"))).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
        QVERIFY(click("guestButton"));
        QTRY_VERIFY(m_app->profileReady());
        QVERIFY(m_app->guest());
        QTRY_COMPARE_WITH_TIMEOUT(m_app->songs()->count(), 1, 10000);
        m_app->player()->setVolume(0.3);
        m_app->leaveProfile();
        const QStringList after = QDir(m_home.filePath(QStringLiteral(".local/share/hop-tunes/profiles"))).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
        QCOMPARE(after, before);
        QCOMPARE(m_app->profileStore()->count(), 1);
    }

    void restartWithPinProfileDoesNotAutoOpen()
    {
        start();
        QVERIFY(!m_app->profileReady());
        QCOMPARE(gateMode(), QStringLiteral("pick"));
    }

    void secondProfileHasItsOwnFolders()
    {
        QVERIFY(click("addProfile"));
        QCOMPARE(gateMode(), QStringLiteral("create"));
        type("profileName", QStringLiteral("jo"));
        QVERIFY(click("createButton"));
        QVERIFY(!m_app->profileReady());             // names are unique, ignoring case
        type("profileName", QStringLiteral("Max"));
        QVERIFY(click("createButton"));
        QTRY_VERIFY(m_app->profileReady());
        m_app->setProfileAccent(QStringLiteral("#ff8a4c"));
        {   // a non-square picture becomes a 256 px square avatar
            QImage pic(480, 320, QImage::Format_RGB32);
            pic.fill(QColor("#2b2b3a"));
            QPainter p(&pic);
            p.setRenderHint(QPainter::Antialiasing);
            p.setBrush(QColor("#ff8a4c")); p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(240, 190), 90, 90);
            p.setBrush(QColor("#f2c94c"));
            p.drawRect(80, 250, 320, 70);
            p.end();
            const QString f = m_home.filePath(QStringLiteral("me.jpg"));
            QVERIFY(pic.save(f));
            QCOMPARE(m_app->setProfileAvatar(QUrl::fromLocalFile(f)), QString());
            QCOMPARE(m_app->setProfileAvatar(QUrl::fromLocalFile(m_home.filePath(QStringLiteral("Music/A/B/01 Song.wav")))).isEmpty(), false);
            const QImage saved(m_app->profileStore()->avatarPath(m_app->profile().value("id").toString()));
            QCOMPARE(saved.size(), QSize(256, 256));
            QVERIFY(!m_app->profile().value("avatar").toString().isEmpty());
        }
        window()->setProperty("view", QStringLiteral("settings"));
        shot("profile-settings");
        window()->setProperty("view", QStringLiteral("collection"));
        m_app->leaveProfile();
        shot("profile-picker");
        QCOMPARE(m_app->openProfile(m_app->profileStore()->profiles().last().id), QString());
        m_app->removeFolder(m_app->folders().value(0));
        QCOMPARE(m_app->songs()->count(), 0);
        m_app->player()->setVolume(0.4);
        m_app->leaveProfile();

        // Jo still sees the music; Max's settings were saved.
        const QString jo = m_app->profileStore()->profiles().first().id;
        QCOMPARE(m_app->openProfile(jo, QStringLiteral("2468")), QString());
        QTRY_COMPARE_WITH_TIMEOUT(m_app->songs()->count(), 1, 10000);
        m_app->leaveProfile();
        const QString max = m_app->profileStore()->profiles().last().id;
        QCOMPARE(m_app->openProfile(max), QString());
        QVERIFY(m_app->folders().isEmpty());
        QCOMPARE(m_app->player()->volume(), 0.4);
    }

    void removeNeverTouchesMusic()
    {
        const QString max = m_app->profile().value("id").toString();
        const QString dir = m_app->profileStore()->dirFor(max);
        QVERIFY(QDir(dir).exists());
        QCOMPARE(m_app->removeProfile(), QString());
        QVERIFY(!m_app->profileReady());
        QVERIFY(!QDir(dir).exists());
        QCOMPARE(m_app->profileStore()->count(), 1);
        QVERIFY(QFile::exists(m_home.filePath(QStringLiteral("Music/A/B/01 Song.wav"))));
    }
};

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    gst_init(&argc, &argv);
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("hop-tunes"));
    QGuiApplication::setOrganizationName(QStringLiteral("hop-tunes"));
    for (const char *f : {":/fonts/Jost-300-Light.ttf", ":/fonts/Jost-400-Book.ttf", ":/fonts/Jost-500-Medium.ttf"})
        QFontDatabase::addApplicationFont(QString::fromLatin1(f));
    QFont::insertSubstitutions(QStringLiteral("Jost"), {QStringLiteral("Jost*"), QStringLiteral("DejaVu Sans")});
    TestUi t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_ui.moc"
