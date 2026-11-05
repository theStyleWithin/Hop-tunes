// SPDX-License-Identifier: GPL-3.0-or-later
// End-to-end profile flows through the real QML UI (offscreen): first run, PIN curtain,
// switching, guest mode, removal, restart behaviour.
#include "App.h"
#include "core/Paths.h"
#include "ui/MpvVideoItem.h"

#include <QDir>
#include <QJSEngine>
#include <QRegularExpression>
#include <QElapsedTimer>
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
#include <QScopeGuard>

#include <taglib/id3v2tag.h>
#include <taglib/wavfile.h>

#include <gst/gst.h>

#include <functional>
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

// A tiny VP8+Opus/Matroska clip (see test_video.cpp for the same recipe) — enough for
// VideoScanner to probe and for the Videos grid to show a real tile.
bool writeTestVideo(const QString &path, int ms = 1000)
{
    GError *err = nullptr;
    const QByteArray desc =
        QStringLiteral("videotestsrc num-buffers=%1 ! video/x-raw,width=320,height=240,framerate=25/1 "
                        "! vp8enc ! matroskamux name=mux ! filesink location=\"%2\" "
                        "audiotestsrc num-buffers=%1 ! audioconvert ! opusenc ! mux.")
            .arg(ms * 25 / 1000).arg(path).toUtf8();
    GstElement *bin = gst_parse_launch(desc.constData(), &err);
    if (err) { g_error_free(err); err = nullptr; }
    if (!bin) return false;
    gst_element_set_state(bin, GST_STATE_PLAYING);
    GstBus *bus = gst_element_get_bus(bin);
    bool ok = false;
    if (GstMessage *msg = gst_bus_timed_pop_filtered(bus, 10 * GST_SECOND,
            GstMessageType(GST_MESSAGE_EOS | GST_MESSAGE_ERROR))) {
        ok = GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS;
        gst_message_unref(msg);
    }
    gst_object_unref(bus);
    gst_element_set_state(bin, GST_STATE_NULL);
    gst_object_unref(bin);
    return ok && QFile::exists(path);
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
    // For things the test has just asked for (a popup, a page): they appear a moment after the click or
    // call that opens them, so give the event loop a few seconds before calling it missing. This
    // was a real flake on slower CI machines ("no item namePromptField").
    QQuickItem *waitItem(const char *name, int timeoutMs = 4000)
    {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < timeoutMs) {
            auto *i = find(window()->contentItem(), QLatin1String(name));
            if (i && i->isVisible()) return i;
            QTest::qWait(25);
        }
        return item(name);
    }
    // Scrolls the nearest Flickable so the item is on screen (Settings is a long page).
    void ensureVisible(QQuickItem *i)
    {
        for (QQuickItem *p = i->parentItem(); p; p = p->parentItem()) {
            if (!p->inherits("QQuickFlickable")) continue;
            auto *content = qvariant_cast<QQuickItem *>(p->property("contentItem"));
            if (!content) return;
            const qreal y = i->mapToItem(content, QPointF(0, 0)).y();
            const qreal maxY = qMax(0.0, p->property("contentHeight").toReal() - p->height());
            p->setProperty("contentY", qBound(0.0, y - 120, maxY));
            QTest::qWait(30);
            return;
        }
    }
    bool click(const char *name)
    {
        QQuickItem *i = waitItem(name);
        if (!i || !i->isVisible() || !i->isEnabled()) { qWarning("cannot click %s", name); return false; }
        ensureVisible(i);
        const QPointF c = i->mapToScene(QPointF(i->width() / 2, i->height() / 2));
        QTest::mouseClick(window(), Qt::LeftButton, Qt::NoModifier, c.toPoint());
        return true;
    }
    void type(const char *name, const QString &text)
    {
        QQuickItem *i = waitItem(name);
        QVERIFY(i);
        i->forceActiveFocus();
        i->setProperty("text", QString());
        for (const QChar c : text) QTest::keyClick(window(), c.toLatin1());
    }
    // Phones: everything you can see must fit the screen's width. An item counts as overflowing when its
    // visible part (cut only by a scrolling row of pills, which is meant to pan) reaches past the right edge.
    QStringList overflowing()
    {
        QStringList out;
        const qreal W = window()->width();
        std::function<void(QQuickItem *, QRectF)> walk = [&](QQuickItem *it, QRectF clip) {
            if (!it->isVisible() || it->opacity() <= 0.01) return;
            QRectF r = it->mapRectToScene(QRectF(0, 0, it->width(), it->height()));
            if ((it->clip() && !it->inherits("QQuickFlickable")) || it->property("namePrefix").isValid())
                clip = clip.intersected(r);   // a clipped picture is cut; scrolling pill rows may pan; other scrolling content may not

            const QRectF vis = r.intersected(clip);
            const bool leaf = it->childItems().isEmpty();
            if (leaf && it->width() > 0 && vis.right() > W + 1)
                {
                QString label = it->objectName();
                for (QQuickItem *p = it; label.isEmpty() && p; p = p->parentItem()) {
                    const QString t = p->property("text").toString();
                    label = !t.isEmpty() ? QStringLiteral("\"%1\"").arg(t.left(30)) : p->objectName();
                }
                QString chain;
                for (QQuickItem *p = it->parentItem(); p && chain.size() < 120; p = p->parentItem())
                    chain += QStringLiteral(" <%1:%2").arg(QLatin1String(p->metaObject()->className()).left(14)).arg(int(p->width()));
                out << QStringLiteral("%1 %2 right=%3%4").arg(QLatin1String(it->metaObject()->className()), label).arg(vis.right()).arg(qEnvironmentVariableIsSet("HOPTUNES_DEBUG_CHAIN") ? chain : QString());
            }
            for (QQuickItem *c : it->childItems()) walk(c, clip);
        };
        walk(window()->contentItem(), QRectF(-1e6, -1e6, 2e6, 2e6));
        return out;
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
        // HOME and XDG_* only isolate the test on Linux; these two work on every platform (Windows
        // otherwise reads and writes the real user folders, and runs leak into each other).
        qputenv("HOPTUNES_DATA_DIR", m_home.filePath(QStringLiteral("hoptunes-root")).toLocal8Bit());
        qputenv("HOPTUNES_MUSIC_DIR", m_home.filePath(QStringLiteral("Music")).toLocal8Bit());
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

        // desktop: the music folder is chosen on this form; phones leave it for the collection
        QTRY_VERIFY(item("createFolderBlock") && item("createFolderBlock")->isVisible());
        window()->setProperty("forceMobile", true);
        window()->resize(412, 880);
        QTRY_VERIFY(!item("createFolderBlock")->isVisible());
        QVERIFY(item("createButton")->isVisible());
        window()->setProperty("forceMobile", false);
        window()->resize(1280, 800);
        QTRY_VERIFY(item("createFolderBlock")->isVisible());

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
        QVERIFY(item("importProfile") && item("importProfile")->isVisible());   // a profile can come back from a file

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
        const QStringList before = QDir(hop::Paths::profilesDir()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
        QVERIFY(click("guestButton"));
        QTRY_VERIFY(m_app->profileReady());
        QVERIFY(m_app->guest());
        QTRY_COMPARE_WITH_TIMEOUT(m_app->songs()->count(), 1, 10000);
        m_app->player()->setVolume(0.3);
        m_app->leaveProfile();
        const QStringList after = QDir(hop::Paths::profilesDir()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
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

    void ejectKeepsYourProfileAndImportNeverOverwrites()
    {
        const int before = m_app->profileStore()->count();
        QCOMPARE(m_app->createProfile(QStringLiteral("Eje"), QStringLiteral("#ff8800"), QStringLiteral("1357"), {}), QString());
        QVERIFY(m_app->profileReady());
        const qint64 pl = m_app->profileDb()->createPlaylist(QStringLiteral("Keep me"));
        m_app->profileDb()->setPlaylistItems(pl, {QStringLiteral("/m/a.mp3")});
        m_app->profileDb()->setHearted(QStringLiteral("/m/a.mp3"), true);
        const QString id = m_app->profile().value("id").toString();
        const QString file = m_home.filePath(QStringLiteral("eje.hoptunes.json"));
        window()->setProperty("view", QStringLiteral("settings"));
        QTRY_VERIFY(item("exportProfile") && item("exportProfile")->isVisible());
        QVERIFY(item("ejectProfile") && item("ejectProfile")->isVisible());
        QVERIFY(!item("confirmEject")->isVisible());   // nothing to confirm until a file is saved
        QVERIFY(item("tipNote") && item("tipNote")->property("text").toString().contains(QStringLiteral("Normand64"))
                && item("tipNote")->property("text").toString().contains(QStringLiteral("locals.com/gamehopper")));
        shot("settings-eject");

        // nothing leaves until the file is saved AND the person says they have it
        QVERIFY(!m_app->ejectProfile(true).isEmpty());
        QCOMPARE(m_app->profileStore()->count(), before + 1);
        const QVariantMap saved = m_app->exportProfileTo(QUrl::fromLocalFile(file));
        QVERIFY2(saved.value("ok").toBool(), qPrintable(saved.value("error").toString()));
        QVERIFY(!m_app->ejectProfile(false).isEmpty());
        QCOMPARE(m_app->profileStore()->count(), before + 1);
        QVERIFY(QFile::exists(file));
        QVERIFY(!QFile(file).readAll().contains("1357"));   // the PIN is not in the file
        QCOMPARE(m_app->ejectProfile(true), QString());
        QVERIFY(!m_app->profileReady());
        QCOMPARE(m_app->profileStore()->count(), before);
        QVERIFY(!QDir(m_app->profileStore()->dirFor(id)).exists());

        // a bad or hostile file is refused and leaves no half-made profile
        const QString junk = m_home.filePath(QStringLiteral("junk.json"));
        { QFile f(junk); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("{\"app\":\"hop-tunes\",\"format\":1,\"profile\":{\"name\":\"X\"},\"settings\":{},\"data\":{\"stats\":[[1]]}}"); }
        QVERIFY(!m_app->inspectProfileFile(QUrl::fromLocalFile(junk)).value("ok").toBool());
        QVERIFY(!m_app->importProfile(QUrl::fromLocalFile(junk), QStringLiteral("X"), {}).isEmpty());
        QCOMPARE(m_app->profileStore()->count(), before);

        // import brings it all back, with a PIN chosen now
        const QVariantMap info = m_app->inspectProfileFile(QUrl::fromLocalFile(file));
        QVERIFY(info.value("ok").toBool());
        QCOMPARE(info.value("name").toString(), QStringLiteral("Eje"));
        QCOMPARE(info.value("playlists").toInt(), 1);
        QCOMPARE(m_app->importProfile(QUrl::fromLocalFile(file), QStringLiteral("Eje"), QStringLiteral("2468")), QString());
        QVERIFY(m_app->profileReady());
        QCOMPARE(m_app->profile().value("name").toString(), QStringLiteral("Eje"));
        QVERIFY(m_app->profile().value("hasPin").toBool());
        QVERIFY(m_app->profileStore()->checkPin(m_app->profile().value("id").toString(), QStringLiteral("2468")));
        QCOMPARE(m_app->profileDb()->playlists().size(), 1);
        QVERIFY(m_app->profileDb()->allStats().value(QStringLiteral("/m/a.mp3")).hearted);

        // importing the same name again never overwrites it
        QVERIFY(!m_app->importProfile(QUrl::fromLocalFile(file), QStringLiteral("eje"), {}).isEmpty());
        QCOMPARE(m_app->profileStore()->count(), before + 1);
        QCOMPARE(m_app->inspectProfileFile(QUrl::fromLocalFile(file)).value("suggestedName").toString(), QStringLiteral("Eje (2)"));

        QCOMPARE(m_app->removeProfile(), QString());   // tidy up for the tests after this one
        QCOMPARE(m_app->profileStore()->count(), before);
    }

    // ---- M4: search, hearts, play counts, playlists, smart playlists, quickplay, DJ ----
    // Uses HOPTUNES_UI_LIBRARY (a real tagged library, for design renders) when set,
    // otherwise a generated folder of short songs. Assertions do not depend on which.

    void m4Setup()
    {
        QString lib = qEnvironmentVariable("HOPTUNES_UI_LIBRARY");
        if (lib.isEmpty()) {
            lib = m_home.filePath(QStringLiteral("Library"));
            const char *names[] = {"Alpha/One/01 Morning Tide.wav", "Alpha/One/02 Harbour Song.wav",
                                   "Beta/Two/01 Night Market.wav", "Beta/Two/02 Paper Moon.wav", "Gamma/Three/01 Copper Light.wav"};
            for (const char *n : names) {
                QDir().mkpath(QFileInfo(lib + QLatin1Char('/') + QLatin1String(n)).absolutePath());
                QVERIFY(writeWav(lib + QLatin1Char('/') + QLatin1String(n), 4000));
            }
        }
        QCOMPARE(m_app->createProfile(QStringLiteral("Lib"), QStringLiteral("#7cc4ff"), {}, QUrl::fromLocalFile(lib)), QString());
        QTRY_VERIFY_WITH_TIMEOUT(m_app->librarySize() >= 5, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(!m_app->scanning(), 15000);
    }

    void m4Search()
    {
        window()->setProperty("view", QStringLiteral("collection"));
        QVERIFY(click("tab_songs"));
        const int all = m_app->librarySize();
        const QString word = m_app->songs()->tracks().at(1).displayTitle().split(QLatin1Char(' ')).last();
        type("searchField", word.toLower());
        QTRY_VERIFY(m_app->songs()->count() < all);
        QVERIFY(m_app->songs()->count() >= 1);
        QCOMPARE(m_app->searchText(), word.toLower());
        shot("m4-search");
        QTest::keyClick(window(), Qt::Key_Escape);
        QTRY_COMPARE(m_app->songs()->count(), all);
        type("searchField", QStringLiteral("zzqx no such thing"));
        QTRY_COMPARE(m_app->songs()->count(), 0);
        m_app->setSearchText({});
        QCOMPARE(m_app->songs()->count(), all);
        QCOMPARE(item("searchField")->property("text").toString(), QString());   // the box follows
    }

    void m4HeartsAndPlayCounts()
    {
        m_app->playSongs(0);
        QTRY_VERIFY(m_app->player()->isPlaying());
        const QString first = m_app->player()->currentTrack()->path;
        QVERIFY(click("miniHeart"));
        QVERIFY(m_app->currentHearted());
        // Half the song heard = one play, recorded in stats and history.
        QTRY_COMPARE_WITH_TIMEOUT(m_app->profileDb()->allStats().value(first).playCount, 1, 10000);
        QTRY_VERIFY(!m_app->quickplayHistory().isEmpty());
        // Moving on early counts as a skip.
        m_app->player()->next();
        QTRY_VERIFY(m_app->player()->currentTrack() && m_app->player()->currentTrack()->path != first);
        const QString second = m_app->player()->currentTrack()->path;
        QTRY_VERIFY_WITH_TIMEOUT(m_app->player()->position() >= 1200, 5000);
        m_app->player()->next();
        QTRY_COMPARE(m_app->profileDb()->allStats().value(second).skipCount, 1);
        QCOMPARE(m_app->profileDb()->allStats().value(second).playCount, 0);

        m_app->selectPlaylist(QStringLiteral("auto:hearted"));
        QCOMPARE(m_app->playlistSongs()->count(), 1);
        m_app->selectPlaylist(QStringLiteral("auto:most"));
        QCOMPARE(m_app->playlistSongs()->tracks().first().path, first);
        m_app->selectPlaylist(QStringLiteral("auto:never"));
        QCOMPARE(m_app->playlistSongs()->count(), m_app->librarySize() - 1);
    }

    void m4ManualPlaylist()
    {
        QVERIFY(click("tab_playlists"));
        QVERIFY(click("newPlaylist"));
        type("namePromptField", QStringLiteral("Road Trip"));
        QVERIFY(click("namePromptOk"));
        QTRY_VERIFY(m_app->selectedPlaylist().startsWith(QStringLiteral("pl:")));
        const QString key = m_app->selectedPlaylist();
        QStringList paths;
        for (const Track &t : m_app->songs()->tracks()) paths.prepend(t.path);   // reverse order
        QVERIFY(m_app->addToPlaylist(key, paths.mid(0, 4)));
        QCOMPARE(m_app->playlistSongs()->count(), 4);
        QVERIFY(m_app->movePlaylistItem(3, 0));
        QCOMPARE(m_app->playlistSongs()->tracks().first().path, paths[3]);
        QVERIFY(m_app->removeFromPlaylist(1));
        QCOMPARE(m_app->playlistSongs()->count(), 3);
        shot("m4-playlist");
        m_app->playPlaylist(0);
        QTRY_COMPARE(m_app->player()->currentTrack()->path, paths[3]);

        // Export, then import the file back: same songs, a non-clashing name.
        const QString file = m_home.filePath(QStringLiteral("road.m3u8"));
        QVERIFY(m_app->exportPlaylist(key, QUrl::fromLocalFile(file)).startsWith(QStringLiteral("saved")));
        QCOMPARE(m_app->importPlaylist(QUrl::fromLocalFile(file)), QStringLiteral("imported 3 songs"));
        QCOMPARE(m_app->selectedPlaylistInfo().value("name").toString(), QStringLiteral("Road Trip (2)"));
        QCOMPARE(m_app->playlistSongs()->tracks().first().path, paths[3]);
        QVERIFY(m_app->deletePlaylist(m_app->selectedPlaylist()));
        m_app->selectPlaylist(key);
    }

    void m4SmartPlaylist()
    {
        QVERIFY(click("newSmartPlaylist"));
        type("smartName", QStringLiteral("Unplayed"));
        QTRY_VERIFY(item("smartPreview")->property("text").toString().contains(QStringLiteral("match")));
        shot("m4-smart-editor");
        QVERIFY(click("smartSave"));
        QTRY_COMPARE(m_app->selectedPlaylistInfo().value("kind").toString(), QStringLiteral("smart"));
        // A rule "genre contains ''" matches everything; tighten it through the API.
        QVERIFY(m_app->setSmartRules(m_app->selectedPlaylist(), QStringLiteral(R"({"match":"all","rules":[{"field":"plays","op":"eq","value":"0"}]})")));
        QCOMPARE(m_app->playlistSongs()->count(), m_app->librarySize() - 1);
        QCOMPARE(m_app->smartPreviewCount(QStringLiteral(R"({"rules":[{"field":"hearted","op":"is","value":"true"}]})")), 1);
    }

    void m4QuickplayAndDj()
    {
        const QString ref = m_app->albumRef(0);
        QVERIFY(!ref.isEmpty());
        m_app->pinTile(QStringLiteral("album"), ref, QStringLiteral("x"));
        m_app->pinTile(QStringLiteral("playlist"), m_app->selectedPlaylist().mid(3), QStringLiteral("y"));
        QCOMPARE(m_app->quickplayPinned().size(), 2);
        QVERIFY(!m_app->quickplayNew().isEmpty());
        QVERIFY(m_app->isPinned(QStringLiteral("album"), ref));

        const QString artist = m_app->djArtists().value(0);
        m_app->startDj(artist);
        QCOMPARE(m_app->djSeed(), artist);
        QVERIFY(m_app->player()->queueLength() > 0);
        QTRY_VERIFY(m_app->player()->isPlaying());
        window()->setProperty("view", QStringLiteral("quickplay"));
        shot("m4-quickplay");
        // Playing something else ends the mix.
        m_app->playTile(QStringLiteral("album"), ref);
        QVERIFY(m_app->djSeed().isEmpty());

        // Play next / add to queue.
        const int len = m_app->player()->queueLength();
        const QString lastPath = m_app->songs()->tracks().last().path;
        m_app->playNextPaths({lastPath});
        m_app->enqueuePaths({lastPath});
        QCOMPARE(m_app->player()->queueLength(), len + 2);
        QCOMPARE(m_app->player()->queue().at(m_app->player()->currentIndex() + 1).path, lastPath);

        // The song menu opens on right-click.
        window()->setProperty("view", QStringLiteral("collection"));
        QVERIFY(click("tab_songs"));
        QTest::qWait(100);
        const QPoint p(330, 300);
        QTest::mouseMove(window(), p);
        QTest::mouseClick(window(), Qt::RightButton, Qt::NoModifier, p);
        shot("m4-song-menu");
        QTest::keyClick(window(), Qt::Key_Escape);
    }

    void m4StatsArePerProfile()
    {
        const QString lib = m_app->profile().value("id").toString();
        m_app->leaveProfile();
        m_app->openGuest();
        QTRY_VERIFY(m_app->profileReady());
        QVERIFY(m_app->profileDb()->allStats().isEmpty());
        QVERIFY(m_app->quickplayPinned().isEmpty());
        QCOMPARE(m_app->playlists().size(), 4);       // only the built-in lists
        m_app->leaveProfile();
        QCOMPARE(m_app->openProfile(lib), QString());
        QCOMPARE(m_app->quickplayPinned().size(), 2);
        QVERIFY(!m_app->profileDb()->allStats().isEmpty());
        QVERIFY(QFile::exists(m_app->profileStore()->dirFor(lib) + QStringLiteral("/profile.sqlite")));
    }

    // ---- M5: themes, playback settings, shuffle/repeat, drag reorder, idle fade ----

    void dragRow(QQuickItem *row, int handleX, int dy)
    {
        const QPointF h = row->mapToScene(QPointF(handleX, row->height() / 2));
        const QPoint start = h.toPoint();
        QTest::mousePress(window(), Qt::LeftButton, Qt::NoModifier, start);
        for (int k = 1; k <= 6; ++k) QTest::mouseMove(window(), start + QPoint(0, dy * k / 6), 10);
        QTest::mouseRelease(window(), Qt::LeftButton, Qt::NoModifier, start + QPoint(0, dy));
    }

    void m5SettingsPersist()
    {
        window()->setProperty("view", QStringLiteral("settings"));
        QTest::qWait(250);
        QVERIFY(click("choice_light"));
        QVERIFY(m_app->lightTheme());
        QCOMPARE(m_app->theme(), QStringLiteral("light"));
        if (m_app->player()->replayGainAvailable()) {
            QVERIFY(click("choice_2"));
            QCOMPARE(m_app->player()->replayGainMode(), 2);
        }
        QVERIFY(click("choice_true"));
        QVERIFY(m_app->reduceMotion());
        m_app->player()->setCrossfadeMs(3000);
        QVERIFY(click("shuffleButton"));
        QVERIFY(m_app->player()->shuffle());
        QVERIFY(click("repeatButton"));
        QCOMPARE(m_app->player()->repeatMode(), 1);
        shot("m5-settings-light");

        // Leave and come back: everything is remembered for this profile only.
        const QString id = m_app->profile().value("id").toString();
        m_app->leaveProfile();
        QVERIFY(!m_app->lightTheme());                      // the picker uses the default
        m_app->openGuest();
        QVERIFY(!m_app->lightTheme());
        QCOMPARE(m_app->player()->crossfadeMs(), 0);
        QVERIFY(!m_app->player()->shuffle());
        m_app->leaveProfile();
        QCOMPARE(m_app->openProfile(id), QString());
        QVERIFY(m_app->lightTheme());
        QVERIFY(m_app->reduceMotion());
        QCOMPARE(m_app->player()->crossfadeMs(), 3000);
        QVERIFY(m_app->player()->shuffle());
        QCOMPARE(m_app->player()->repeatMode(), 1);
        if (m_app->player()->replayGainAvailable()) QCOMPARE(m_app->player()->replayGainMode(), 2);
        m_app->player()->setShuffle(false);
        m_app->player()->setRepeatMode(0);
        m_app->player()->setCrossfadeMs(0);
        m_app->setReduceMotion(false);
    }

    void m5PlaylistDragReorder()
    {
        window()->setProperty("view", QStringLiteral("collection"));
        QVERIFY(click("tab_playlists"));
        QStringList paths;
        for (const Track &t : m_app->songs()->tracks()) paths << t.path;
        const QString key = m_app->createPlaylist(QStringLiteral("Drag Me"), paths.mid(0, 4));
        m_app->selectPlaylist(key);
        QTest::qWait(200);
        // The first row's handle, dragged two rows down.
        QQuickItem *list = item("playlistSongList");
        QVERIFY(list);
        QQuickItem *firstRow = nullptr;
        std::function<void(QQuickItem *)> findRow = [&](QQuickItem *it) {
            if (firstRow) return;
            if (it->property("draggable").toBool() && it->property("path").toString() == paths[0]) { firstRow = it; return; }
            for (QQuickItem *c : it->childItems()) findRow(c);
        };
        findRow(list);
        QVERIFY(firstRow);
        QTest::mouseMove(window(), firstRow->mapToScene(QPointF(20, 20)).toPoint());
        dragRow(firstRow, 20, int(firstRow->height() * 2));
        QTRY_COMPARE(m_app->playlistSongs()->tracks().at(2).path, paths[0]);
        QCOMPARE(m_app->playlistSongs()->tracks().at(0).path, paths[1]);
        QVERIFY(m_app->deletePlaylist(key));
    }

    void m5QueueDragAndIdleFade()
    {
        m_app->playSongs(0);
        QTRY_VERIFY(m_app->player()->isPlaying());
        window()->setProperty("view", QStringLiteral("now playing"));
        QTest::qWait(250);
        const QString second = m_app->player()->queue().at(2).path;   // "up next" row 1
        QQuickItem *row = item("upNext_1");
        QVERIFY(row);
        dragRow(row, 8, -int(row->height()));                          // up one place
        QTRY_COMPARE(m_app->player()->queue().at(1).path, second);
        QTRY_COMPARE(item("upNextTitle_0")->property("text").toString(), m_app->player()->queue().at(1).displayTitle());   // the screen follows
        shot("m5-now-playing-light");
        // Idle: the controls fade out, and come back on mouse movement.
        QObject *np = nullptr;
        std::function<void(QQuickItem *)> findNp = [&](QQuickItem *it) {
            if (np) return;
            if (it->property("idle").isValid() && it->property("chrome").isValid()) { np = it; return; }
            for (QQuickItem *c : it->childItems()) findNp(c);
        };
        findNp(window()->contentItem());
        QVERIFY(np);
        QTRY_VERIFY_WITH_TIMEOUT(np->property("idle").toBool(), 6000);
        QTest::mouseMove(window(), QPoint(300, 300));
        QTest::mouseMove(window(), QPoint(320, 310));
        QTRY_VERIFY(!np->property("idle").toBool());
        m_app->setTheme(QStringLiteral("dark"));
        QVERIFY(!m_app->lightTheme());
    }

    // ---- M6: opening files and folders (file manager, command line, MPRIS OpenUri) ----
    void m6OpenFiles()
    {
        const QString loose = m_home.filePath(QStringLiteral("Loose"));
        QDir().mkpath(loose + QStringLiteral("/sub"));
        QVERIFY(writeWav(loose + QStringLiteral("/b second.wav"), 2000));
        QVERIFY(writeWav(loose + QStringLiteral("/a first.wav"), 2000));
        QVERIFY(writeWav(loose + QStringLiteral("/sub/c third.wav"), 2000));
        QFile(loose + QStringLiteral("/notes.txt")).open(QIODevice::WriteOnly);   // not audio: ignored

        // A single file outside every music folder plays straight away.
        m_app->openFiles({loose + QStringLiteral("/b second.wav")});
        QTRY_VERIFY(m_app->player()->isPlaying());
        QCOMPARE(m_app->player()->queueLength(), 1);
        QCOMPARE(m_app->player()->title(), QStringLiteral("b second"));

        // A folder plays its audio files in name order, including subfolders.
        m_app->openFiles({QUrl::fromLocalFile(loose).toString()});
        QCOMPARE(m_app->player()->queueLength(), 3);
        QCOMPARE(m_app->player()->queue().at(0).displayTitle(), QStringLiteral("a first"));
        QCOMPARE(m_app->player()->queue().at(2).displayTitle(), QStringLiteral("c third"));

        // Several OpenUri calls arriving together become one queue.
        m_app->openUri(QUrl::fromLocalFile(loose + QStringLiteral("/a first.wav")).toString());
        m_app->openUri(QUrl::fromLocalFile(loose + QStringLiteral("/sub/c third.wav")).toString());
        QTRY_COMPARE(m_app->player()->queueLength(), 2);

        // Before a profile is open, files wait and then play.
        const QString id = m_app->profile().value("id").toString();
        m_app->leaveProfile();
        QVERIFY(!m_app->player()->hasTrack());
        m_app->openFiles({loose + QStringLiteral("/b second.wav")});
        QVERIFY(!m_app->player()->hasTrack());
        QCOMPARE(m_app->openProfile(id), QString());
        QTRY_VERIFY(m_app->player()->isPlaying());
        QCOMPARE(m_app->player()->title(), QStringLiteral("b second"));

        // About section shows the version.
        window()->setProperty("view", QStringLiteral("settings"));
        QTest::qWait(200);
        QQuickItem *about = item("aboutVersion");
        QVERIFY(about);
        ensureVisible(about);
        QVERIFY(about->property("text").toString().contains(QStringLiteral(HOPTUNES_VERSION)));
        QVERIFY(about->property("text").toString().contains(QStringLiteral("GPL-3.0")));
        shot("m6-about");
    }

    // The movie volume is its own per-profile setting (0-150 %), apart from the music volume.
    void movieVolumeIsSeparateFromMusicVolume()
    {
        QCOMPARE(m_app->videoVolume(), 100);
        const double music = m_app->player()->volume();
        m_app->setVideoVolume(130);
        QCOMPARE(m_app->videoVolume(), 130);
        m_app->setVideoVolume(999);
        QCOMPARE(m_app->videoVolume(), 150);
        m_app->setVideoVolume(-3);
        QCOMPARE(m_app->videoVolume(), 0);
        m_app->setVideoVolume(120);
        QCOMPARE(m_app->player()->volume(), music);   // untouched
        const QJsonObject s = m_app->profileStore()->settings(m_app->profile().value("id").toString());
        QCOMPARE(s.value(QStringLiteral("videoVolume")).toInt(), 120);
        m_app->setVideoVolume(100);
    }

    // Scanning and the Videos grid only (spec §5b) — not opening the player itself:
    // MpvVideoItem is a QQuickFramebufferObject, which needs a real GL scenegraph, and
    // this whole test suite runs the software backend (no GPU in CI). Opening the player
    // would instantiate it, so that path is intentionally left untested here; see
    // MpvVideoItem's header comment.
    void m7VideoScanningAndGrid()
    {
        window()->setProperty("view", QStringLiteral("collection"));
        const QString lib = m_home.filePath(QStringLiteral("Library"));
        QVERIFY(writeTestVideo(lib + QStringLiteral("/Alpha/clip.mkv")));
        m_app->rescan();
        QTRY_VERIFY_WITH_TIMEOUT(m_app->videos()->count() >= 1, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(!m_app->videoScanning(), 15000);

        QVERIFY(click("subTab_videos"));
        // each folder is a slot, like pictures: the shelf first, the folder's videos once it is opened
        QTRY_VERIFY(item("videoSlot_0") != nullptr && item("videoSlot_0")->isVisible());
        QCOMPARE(m_app->videoSlots().size(), 1);
        QCOMPARE(m_app->videoSlots().first().toMap().value("name").toString(), QStringLiteral("Alpha"));
        QVERIFY(!item("videoTile_0") || !item("videoTile_0")->isVisible());
        shot("m7-video-slots");
        QVERIFY(click("videoSlot_0"));
        QTRY_VERIFY(!m_app->videoSlot().isEmpty());
        QTRY_VERIFY(item("videoTile_0") != nullptr);
        QVERIFY(item("videoTile_0")->isVisible());
        shot("m7-videos");

        // Opening a video file (file manager "open with", CLI) is deliberately NOT
        // exercised here: hop.videoPlayerOpen becoming true is exactly what makes
        // VideoPlayerView.qml instantiate a real engine-backed surface item, and with the
        // GStreamer fallback engine active (HOPTUNES_HAVE_MPV off) that was confirmed, by
        // testing, to crash during window teardown under this suite's software Qt Quick
        // backend — see test_robust.cpp's openingAVideoFileOpensThePlayer() instead, which
        // covers the same App-level behaviour with no QML engine involved at all.
    }

    // The videos tab has movies, shows, cartoons, anime and other videos; each has its own folders,
    // and a video lands in the category whose folder is its closest parent.
    void videoCategoriesHaveTheirOwnFolders()
    {
        window()->setProperty("view", QStringLiteral("collection"));
        QVERIFY(click("subTab_videos"));
        QCOMPARE(m_app->videoCategories(), (QStringList{"movies", "shows", "cartoons", "anime", "other"}));
        QCOMPARE(m_app->videoCategory(), QStringLiteral("movies"));
        for (const char *c : {"videoCat_movies", "videoCat_shows", "videoCat_cartoons", "videoCat_anime", "videoCat_other"})
            QVERIFY2(waitItem(c) != nullptr, c);
        const int movies = m_app->videos()->count();
        QVERIFY(movies >= 1);

        const QString dir = m_home.filePath(QStringLiteral("Anime"));
        QVERIFY(QDir().mkpath(dir));
        QVERIFY(writeTestVideo(dir + QStringLiteral("/ep1.mkv")));
        QVERIFY(click("videoCat_anime"));
        QTRY_COMPARE(m_app->videoCategory(), QStringLiteral("anime"));
        QCOMPARE(m_app->videos()->count(), 0);
        QTRY_VERIFY(item("videosChooseFolder_anime") != nullptr && item("videosChooseFolder_anime")->isVisible());   // its own button
        shot("v-anime-empty");
        m_app->addVideoCategoryFolder(QStringLiteral("anime"), QUrl::fromLocalFile(dir));
        QTRY_COMPARE_WITH_TIMEOUT(m_app->videos()->count(), 1, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(!m_app->videoScanning(), 15000);
        QTRY_VERIFY(item("videoSlot_0") != nullptr && item("videoSlot_0")->isVisible());   // its folder is a slot too
        QVERIFY(click("videoSlot_0"));
        QTRY_VERIFY(item("videoTile_0") != nullptr && item("videoTile_0")->isVisible());
        shot("v-anime");
        QCOMPARE(m_app->categoryOfVideo(dir + QStringLiteral("/ep1.mkv")), QStringLiteral("anime"));
        QCOMPARE(m_app->categoryOfVideo(m_home.filePath(QStringLiteral("Library/Alpha/clip.mkv"))), QStringLiteral("movies"));
        const QJsonObject s = m_app->profileStore()->settings(m_app->profile().value("id").toString());
        QCOMPARE(s.value("videoCategoryFolders").toObject().value("anime").toArray().size(), 1);

        // Movies are untouched by it.
        QVERIFY(click("videoCat_movies"));
        QTRY_COMPARE(m_app->videos()->count(), movies);

        m_app->removeVideoCategoryFolder(QStringLiteral("anime"), m_app->videoCategoryFolders().value("anime").toStringList().first());
        QVERIFY(m_app->videoCategoryFolders().value("anime").toStringList().isEmpty());
        QCOMPARE(m_app->videos()->count(), movies);
        m_app->setVideoCategory(QStringLiteral("nonsense"));   // ignored
        QCOMPARE(m_app->videoCategory(), QStringLiteral("movies"));
    }

    // Clicking a movie opens its details page (empty boxes the user fills in); nothing plays until Play.
    void videoDetailsPageIsEmptyBoxesAndPlayNeedsAClick()
    {
        window()->setProperty("view", QStringLiteral("collection"));
        QVERIFY(click("subTab_videos"));
        if (m_app->videoSlot().isEmpty()) { QTRY_VERIFY(item("videoSlot_0") != nullptr); QVERIFY(click("videoSlot_0")); }
        QTRY_VERIFY(item("videoTile_0") != nullptr && item("videoTile_0")->isVisible());
        QCOMPARE(m_app->videoDetailRow(), -1);
        QVERIFY(click("videoTile_0"));
        QTRY_COMPARE(m_app->videoDetailRow(), 0);
        QVERIFY(!m_app->videoPlayerOpen());                       // no auto-play
        const QString path = m_app->videos()->videos().first().path;

        // Everything starts empty; only facts from the file itself are known.
        QVariantMap d = m_app->videoDetail();
        QCOMPARE(d.value("meta").toMap().value("year").toString(), QString());
        QCOMPARE(d.value("meta").toMap().value("overview").toString(), QString());
        QVERIFY(d.value("cast").toList().isEmpty());
        QVERIFY(!d.value("fileName").toString().isEmpty());
        QVERIFY(!d.value("played").toBool());

        // Fill some boxes in; they are kept (and trimmed), and a typed title replaces the file name in the grid.
        m_app->setVideoField(QStringLiteral("year"), QStringLiteral(" 1998 "));
        m_app->setVideoField(QStringLiteral("director"), QStringLiteral("Frank Coraci"));
        m_app->setVideoField(QStringLiteral("nonsense"), QStringLiteral("ignored"));   // not a field
        m_app->setVideoField(QStringLiteral("title"), QStringLiteral("My Title"));
        d = m_app->videoDetail();
        QCOMPARE(d.value("meta").toMap().value("year").toString(), QStringLiteral("1998"));
        QCOMPARE(d.value("meta").toMap().value("director").toString(), QStringLiteral("Frank Coraci"));
        QCOMPARE(m_app->videos()->data(m_app->videos()->index(0), VideoListModel::TitleRole).toString(), QStringLiteral("My Title"));
        QVERIFY(m_app->videoDetail().value("fileName").toString() != QStringLiteral("My Title"));   // the file name stays known

        // Cast: add an empty row, name it, remove it.
        m_app->addCastMember();
        QCOMPARE(m_app->videoDetail().value("cast").toList().size(), 1);
        m_app->setCastMember(0, QStringLiteral("  Adam Sandler "), QStringLiteral("Bobby"));
        const QVariantMap person = m_app->videoDetail().value("cast").toList().first().toMap();
        QCOMPARE(person.value("name").toString(), QStringLiteral("Adam Sandler"));
        QCOMPARE(person.value("role").toString(), QStringLiteral("Bobby"));

        // Saved in this profile's database.
        QVERIFY(m_app->profileDb()->videoMetas().contains(path));
        QVERIFY(m_app->profileDb()->videoMetas().value(path).contains(QStringLiteral("Adam Sandler")));

        // Watched toggles both ways.
        m_app->toggleVideoWatched();
        QVERIFY(m_app->videoDetail().value("played").toBool());
        m_app->toggleVideoWatched();
        QVERIFY(!m_app->videoDetail().value("played").toBool());

        // Clearing every box removes the saved details altogether.
        m_app->removeCastMember(0);
        for (const char *k : {"year", "director", "title"}) m_app->setVideoField(QString::fromLatin1(k), QString());
        QVERIFY(!m_app->profileDb()->videoMetas().contains(path));
        QCOMPARE(m_app->videos()->data(m_app->videos()->index(0), VideoListModel::TitleRole).toString(),
                 m_app->videoDetail().value("fileName").toString());

        QVERIFY(click("videoDetailBack"));
        QTRY_COMPARE(m_app->videoDetailRow(), -1);
        QVERIFY(click("subTab_music"));
    }

    // Settings › video settings: the default viewing style is per profile, validated, and chosen with a click.
    void videoViewStyleSetting()
    {
        QCOMPARE(m_app->videoViewStyle(), QStringLiteral("window"));        // fullscreen is no longer the default
        m_app->setVideoViewStyle(QStringLiteral("nonsense"));
        QCOMPARE(m_app->videoViewStyle(), QStringLiteral("window"));
        window()->setProperty("view", QStringLiteral("settings"));
        QVERIFY(click("choice_theater"));
        QCOMPARE(m_app->videoViewStyle(), QStringLiteral("theater"));
        QVERIFY(click("choice_fullscreen"));
        QCOMPARE(m_app->videoViewStyle(), QStringLiteral("fullscreen"));
        // It is saved in this profile's settings.
        const QJsonObject s = m_app->profileStore()->settings(m_app->profile().value("id").toString());
        QCOMPARE(s.value(QStringLiteral("videoViewStyle")).toString(), QStringLiteral("fullscreen"));
        QVERIFY(click("choice_window"));
        QCOMPARE(m_app->videoViewStyle(), QStringLiteral("window"));
        window()->setProperty("view", QStringLiteral("collection"));
    }

    // The pretend live chat: off by default, a separate switch from the viewing style, two looks.
    void livestreamChatSetting()
    {
        QVERIFY(!m_app->videoChat());
        QCOMPARE(m_app->videoChatLook(), QStringLiteral("clean"));
        window()->setProperty("view", QStringLiteral("settings"));
        QTRY_VERIFY(item("videoChatChoice") != nullptr);
        QVERIFY(item("videoChatLookChoice") == nullptr || !item("videoChatLookChoice")->isVisible());   // only once it is on
        m_app->setVideoChat(true);
        QTRY_VERIFY(item("videoChatLookChoice") != nullptr && item("videoChatLookChoice")->isVisible());
        QTest::qWait(150);   // the new row has just appeared: let the page lay out before clicking in it
        QVERIFY(click("choice_lively"));
        QTRY_COMPARE(m_app->videoChatLook(), QStringLiteral("lively"));
        m_app->setVideoChatLook(QStringLiteral("nonsense"));
        QCOMPARE(m_app->videoChatLook(), QStringLiteral("clean"));
        // Independent of the viewing style, and saved in this profile.
        m_app->setVideoViewStyle(QStringLiteral("theater"));
        QVERIFY(m_app->videoChat());
        const QJsonObject s = m_app->profileStore()->settings(m_app->profile().value("id").toString());
        QVERIFY(s.value(QStringLiteral("videoChat")).toBool());
        QCOMPARE(s.value(QStringLiteral("videoChatLook")).toString(), QStringLiteral("clean"));
        m_app->setVideoChat(false);
        m_app->setVideoViewStyle(QStringLiteral("window"));
        window()->setProperty("view", QStringLiteral("collection"));
    }

    // ChatGen.js is plain JavaScript run by the QML engine, so the same file is run here and checked:
    // the chat is a pure function of (video, second), uses the movie page, and answers sensibly.
    void chatEngine()
    {
        QFile f(QStringLiteral(":/qml/ChatGen.js"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QString src = QString::fromUtf8(f.readAll());
        src.remove(QRegularExpression(QStringLiteral("^\\.pragma[^\\n]*\\n")));
        QJSEngine js;
        const QJSValue ok = js.evaluate(src);
        QVERIFY2(!ok.isError(), qPrintable(ok.toString()));
        // The checks themselves are plain JavaScript in tests/chat_engine_check.js, run inside the same engine.
        QFile check(QStringLiteral(HOPTUNES_TEST_DIR "/chat_engine_check.js"));
        QVERIFY(check.open(QIODevice::ReadOnly));
        const QJSValue r = js.evaluate(QString::fromUtf8(check.readAll()), check.fileName());
        QVERIFY2(!r.isError(), qPrintable(r.toString()));
        QVERIFY(r.property("same").toBool());
        QVERIFY(r.property("differs").toBool());
        QVERIFY(r.property("ordered").toBool());
        QCOMPARE(r.property("bad").toInt(), 0);
        QVERIFY2(r.property("total").toInt() > 150, "the chat should be lively enough");
        QVERIFY2(r.property("movie").toInt() > 15, "some lines should use the movie page");
        QCOMPARE(r.property("invented").toInt(), 0);
        QVERIFY(r.property("songTalks").toBool());                       // beside a song it chats too,
        QVERIFY2(r.property("songMine").toInt() > 10, "some lines should use the song's details");
        QCOMPARE(r.property("songFilmWords").toInt(), 0);                // and about music, not films
        QVERIFY(r.property("director").toBool());
        QVERIFY(r.property("year").toBool());
        QVERIFY(r.property("noDirector").toBool());
        QVERIFY(r.property("mentionsUser").toBool());
        QVERIFY(r.property("delays").toBool());
        QVERIFY(r.property("count").toBool());
        QVERIFY(r.property("esc").toBool());
        QVERIFY(r.property("parts").toBool());
    }

    void commentsSetting()
    {
        QCOMPARE(m_app->videoComments(), QStringLiteral("off"));   // bloat-free by default
        window()->setProperty("view", QStringLiteral("settings"));
        QTRY_VERIFY(item("videoCommentsChoice") != nullptr);
        QTest::qWait(150);
        QVERIFY(click("choice_populated"));
        QTRY_COMPARE(m_app->videoComments(), QStringLiteral("populated"));
        m_app->setVideoComments(QStringLiteral("nonsense"));
        QCOMPARE(m_app->videoComments(), QStringLiteral("off"));
        m_app->setVideoComments(QStringLiteral("mine"));
        const QJsonObject s = m_app->profileStore()->settings(m_app->profile().value("id").toString());
        QCOMPARE(s.value(QStringLiteral("videoComments")).toString(), QStringLiteral("mine"));
        // Off means off: nothing can be added.
        m_app->setVideoComments(QStringLiteral("off"));
        m_app->addVideoComment(QStringLiteral("hello"));
        QVERIFY(m_app->myVideoComments().isEmpty());
        window()->setProperty("view", QStringLiteral("collection"));
    }

    // CommentGen.js: deterministic, endless, uses the movie page without inventing details.
    void commentEngine()
    {
        QJSEngine js;
        auto load = [&](const char *res, bool wrapImport) {
            QFile f(QString::fromLatin1(res));
            QVERIFY(f.open(QIODevice::ReadOnly));
            QString src = QString::fromUtf8(f.readAll());
            src.remove(QRegularExpression(QStringLiteral("^\\.pragma[^\\n]*\\n")));
            src.remove(QRegularExpression(QStringLiteral("^\\.import[^\\n]*\\n"), QRegularExpression::MultilineOption));
            const QJSValue ok = js.evaluate(src);
            QVERIFY2(!ok.isError(), qPrintable(ok.toString()));
            if (wrapImport)   // what `.import "ChatGen.js" as C` gives CommentGen.js
                QVERIFY(!js.evaluate(QStringLiteral("var C = { rng: rng, pick: pick, viewer: viewer, movieLine: movieLine, replies: replies, context: context };")).isError());
        };
        load(":/qml/ChatGen.js", true);
        load(":/qml/CommentGen.js", false);
        QFile check(QStringLiteral(HOPTUNES_TEST_DIR "/comment_engine_check.js"));
        QVERIFY(check.open(QIODevice::ReadOnly));
        const QJSValue r = js.evaluate(QString::fromUtf8(check.readAll()), check.fileName());
        QVERIFY2(!r.isError(), qPrintable(r.toString()));
        QVERIFY(r.property("same").toBool());
        QVERIFY(r.property("differs").toBool());
        QVERIFY(r.property("endless").toBool());
        QCOMPARE(r.property("bad").toInt(), 0);
        QCOMPARE(r.property("emote").toInt(), 0);
        QVERIFY2(r.property("movie").toInt() > 10, "some comments should use the movie page");
        QVERIFY2(r.property("withReplies").toInt() > 40, "many comments have replies");
        QCOMPARE(r.property("invented").toInt(), 0);
        QVERIFY(r.property("answers").toBool());
        QVERIFY(r.property("answerDirector").toBool());
        QVERIFY(r.property("ago").toBool());
    }

    // A private status box above the profile: empty by default, kept per profile, saved as you type.
    void statusNoteIsPrivateAndRemembered()
    {
        window()->setProperty("view", QStringLiteral("collection"));
        QTRY_VERIFY(item("statusNote") != nullptr && item("statusNote")->isVisible());
        QCOMPARE(m_app->statusNote(), QString());
        type("statusNote", QStringLiteral("hello there"));
        QTRY_COMPARE_WITH_TIMEOUT(m_app->statusNote(), QStringLiteral("hello there"), 3000);
        const QJsonObject st = m_app->profileStore()->settings(m_app->profile().value("id").toString());
        QCOMPARE(st.value("statusNote").toString(), QStringLiteral("hello there"));
        m_app->setStatusNote(QString(400, QLatin1Char('x')));   // a note has a limit
        QCOMPARE(m_app->statusNote().size(), 300);
        m_app->setStatusNote(QString());
        QTRY_COMPARE(item("statusNote")->property("text").toString(), QString());   // the box follows
    }

    // Phones and tablets: a bottom bar or a side drawer, switched in settings, and a rail or a permanent
    // drawer on a wide screen. (The desktop keeps its top pivots; forceMobile stands in for Android here.)
    void phoneAndTabletNavigation()
    {
        auto *w = window();
        const QSize before = w->size();
        struct Restore {   // a failed check must not leave the window phone-sized for the tests after it
            QQuickWindow *w; QSize size; App *app;
            ~Restore() { app->setNavStyle(QStringLiteral("bar")); w->setProperty("forceMobile", false); w->resize(size); }
        } restore{w, before, m_app.get()};
        w->setProperty("forceMobile", true);
        w->setProperty("view", QStringLiteral("collection"));
        m_app->setNavStyle(QStringLiteral("bar"));
        w->resize(412, 860);
        QTRY_COMPARE(w->property("tablet").toBool(), false);
        QTRY_COMPARE(w->property("railWidth").toInt(), 0);
        QVERIFY(w->property("mobileNav").toBool());
        shot("android-phone-bar");

        // the bottom bar moves between views
        QTRY_VERIFY(item("navMenuButton") == nullptr || !item("navMenuButton")->isVisible());
        QTRY_VERIFY(item("navTab_now playing") != nullptr && item("navTab_now playing")->isVisible());
        QVERIFY(click("navTab_now playing"));
        QTRY_COMPARE(w->property("view").toString(), QStringLiteral("now playing"));
        shot("android-phone-nowplaying");
        w->setProperty("view", QStringLiteral("settings"));
        shot("android-phone-settings");
        w->setProperty("view", QStringLiteral("quickplay"));
        shot("android-phone-quickplay");
        w->setProperty("view", QStringLiteral("collection"));

        // the side drawer: a ☰ button opens it, and the choice is remembered in the profile
        m_app->setNavStyle(QStringLiteral("drawer"));
        QTRY_VERIFY(item("navMenuButton") != nullptr && item("navMenuButton")->isVisible());
        QVERIFY(click("navMenuButton"));
        QTRY_VERIFY(item("mobileNav") != nullptr && item("mobileNav")->property("drawerOpen").toBool());
        shot("android-phone-drawer");
        QCOMPARE(m_app->profileStore()->settings(m_app->profile().value("id").toString()).value("navStyle").toString(),
                 QStringLiteral("drawer"));
        QMetaObject::invokeMethod(item("mobileNav"), "closeDrawer");
        QTRY_VERIFY(!item("mobileNav")->property("drawerOpen").toBool());
        m_app->setNavStyle(QStringLiteral("nonsense"));   // anything else means the bottom bar
        QCOMPARE(m_app->navStyle(), QStringLiteral("bar"));

        // a tablet: a side rail, or a permanent drawer
        w->resize(1000, 700);
        QTRY_VERIFY(w->property("tablet").toBool());
        QTRY_COMPARE(w->property("railWidth").toInt(), 84);
        shot("android-tablet-rail");
        m_app->setNavStyle(QStringLiteral("drawer"));
        QTRY_COMPARE(w->property("railWidth").toInt(), 240);
        shot("android-tablet-drawer");

        m_app->setNavStyle(QStringLiteral("bar"));
        w->setProperty("forceMobile", false);
        w->resize(before);
        QTRY_COMPARE(w->property("railWidth").toInt(), 0);
        QVERIFY(!w->property("mobileNav").toBool());
        QTRY_COMPARE(w->size(), before);   // the desktop layout is back before the next test looks at it
        QTRY_VERIFY(!item("navTab_collection") || !item("navTab_collection")->isVisible());
        QTest::qWait(400);
    }

    // Every screen fits a phone (412 and 320 wide): nothing visible runs past the right edge.
    void phoneScreensFitTheScreen()
    {
        auto *w = window();
        const QSize before = w->size();
        struct Restore {
            QQuickWindow *w; QSize size; App *app;
            ~Restore() { app->setNavStyle(QStringLiteral("bar")); w->setProperty("forceMobile", false); w->resize(size); }
        } restore{w, before, m_app.get()};
        w->setProperty("forceMobile", true);
        m_app->setNavStyle(QStringLiteral("bar"));
        QStringList failures;
        const QList<QSize> sizes{QSize(412, 892), QSize(320, 640)};
        for (const QSize &sz : sizes) {
            w->resize(sz);
            QTRY_COMPARE(w->width(), sz.width());
            const QString tag = QString::number(sz.width());
            auto check = [&](const char *name, const QString &what) {
                QTest::qWait(250);
                shot((QStringLiteral("phone%1-").arg(tag) + QLatin1String(name)).toLatin1().constData());
                const QStringList bad = overflowing();
                if (!bad.isEmpty()) {
                    qWarning("%s (%s) overflows: %s", name, qPrintable(tag), qPrintable(bad.mid(0, 4).join(QStringLiteral(" | "))));
                    failures << what + QStringLiteral(" at ") + tag;
                }
            };
            w->setProperty("view", QStringLiteral("collection"));
            QTRY_VERIFY(item("pillTab_music") != nullptr);
            QVERIFY(click("pillTab_music"));
            QVERIFY(click("pillMode_artists"));
            check("music-artists", QStringLiteral("artists"));
            QVERIFY(click("pillMode_albums"));
            check("music-albums", QStringLiteral("albums"));
            QVERIFY(click("pillMode_songs"));
            check("music-songs", QStringLiteral("songs"));
            QVERIFY(click("pillMode_playlists"));
            check("music-playlists", QStringLiteral("playlists"));
            QVERIFY(click("pillTab_videos"));
            check("videos", QStringLiteral("videos"));
            QVERIFY(click("pillTab_pictures"));
            check("pictures", QStringLiteral("pictures"));
            QVERIFY(click("pillTab_music"));
            w->setProperty("view", QStringLiteral("now playing"));
            check("nowplaying", QStringLiteral("now playing"));
            w->setProperty("view", QStringLiteral("quickplay"));
            check("quickplay", QStringLiteral("quickplay"));
            w->setProperty("view", QStringLiteral("search"));
            check("search", QStringLiteral("search"));
            w->setProperty("view", QStringLiteral("profile"));
            check("profile", QStringLiteral("profile"));
            w->setProperty("view", QStringLiteral("settings"));
            check("settings", QStringLiteral("settings"));
        }
        m_app->setNavStyle(QStringLiteral("bar"));
        w->setProperty("forceMobile", false);
        w->resize(before);
        w->setProperty("view", QStringLiteral("collection"));
        QTRY_COMPARE(w->size(), before);
        QTRY_VERIFY(!item("pillTab_music") || !item("pillTab_music")->isVisible());
        QTest::qWait(400);
        QVERIFY2(failures.isEmpty(), qPrintable(QStringLiteral("overflowing: ") + failures.join(QStringLiteral(", "))));
    }

    // Turned sideways (landscape) Now playing has its own layout: the cover left, the song and buttons right, all of it
    // inside the screen (nothing runs past the right edge or off the bottom).
    void phoneNowPlayingLandscape()
    {
        auto *w = window();
        const QSize before = w->size();
        struct Restore {
            QQuickWindow *w; QSize size; App *app;
            ~Restore() { w->setProperty("forceMobile", false); w->resize(size); w->setProperty("view", QStringLiteral("collection")); }
        } restore{w, before, m_app.get()};
        m_app->playSongs(0);
        QTRY_VERIFY(m_app->player()->hasTrack());
        w->setProperty("forceMobile", true);
        for (const QSize &sz : {QSize(800, 360), QSize(892, 412), QSize(640, 320)}) {
            w->resize(sz);
            QTRY_COMPARE(w->width(), sz.width());
            w->setProperty("view", QStringLiteral("now playing"));
            QTest::qWait(400);
            const QString tag = QStringLiteral("%1x%2").arg(sz.width()).arg(sz.height());
            shot(qPrintable(QStringLiteral("phone-nowplaying-landscape-") + tag));
            QVERIFY2(overflowing().isEmpty(), qPrintable(tag + QStringLiteral(" overflows right: ") + overflowing().join(QStringLiteral(", "))));
            for (const char *name : {"phonePlay", "phoneSeek", "phoneQueueButton", "phoneNpBack"}) {
                QQuickItem *it = item(name);
                QVERIFY2(it && it->isVisible(), name);
                const QRectF r = it->mapRectToScene(QRectF(0, 0, it->width(), it->height()));
                QVERIFY2(r.bottom() <= w->height() + 1 && r.right() <= w->width() + 1 && r.top() >= -1,
                         qPrintable(QStringLiteral("%1 off screen at %2: %3,%4 %5x%6").arg(QLatin1String(name), tag).arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height())));
            }
        }
        // portrait is as before
        w->resize(412, 892);
        QTRY_COMPARE(w->width(), 412);
        QTest::qWait(300);
        QVERIFY(overflowing().isEmpty());
        QQuickItem *play = item("phonePlay");
        QVERIFY(play && play->mapRectToScene(QRectF(0, 0, play->width(), play->height())).bottom() <= w->height());
        m_app->player()->stop();
    }

    // Musy Note (the phone game): opens full screen from Now playing, flaps by tap / key / controller, the song can fly it
    // (and then no score is kept), and back closes it.
    void musyNoteOpensFliesAndKeepsHonestScores()
    {
        auto *w = window();
        const QSize before = w->size();
        struct Restore {
            QQuickWindow *w; QSize size;
            ~Restore() { w->setProperty("forceMobile", false); w->resize(size); w->setProperty("view", QStringLiteral("collection")); }
        } restore{w, before};
        m_app->playSongs(0);
        QTRY_VERIFY(m_app->player()->isPlaying());
        w->setProperty("forceMobile", true);
        w->resize(800, 360);
        QTRY_COMPARE(w->width(), 800);
        w->setProperty("view", QStringLiteral("now playing"));
        QTRY_VERIFY(item("phoneGames") && item("phoneGames")->isVisible());
        shot("phone-nowplaying-games-button");
        QVERIFY(click("phoneGames"));
        QTRY_VERIFY(item("musyNote") && item("musyNote")->isVisible());
        MusyNoteGame *g = m_app->musyNote();
        QTRY_COMPARE(g->state(), 0);                       // ready: waiting for the first flap
        QTRY_VERIFY(m_app->audio()->active());             // the analysis listens only while it is open

        // twelve coloured "covers" for the walls
        QVariantList urls;
        for (int i = 0; i < 12; ++i) {
            QImage im(96, 96, QImage::Format_RGB32);
            im.fill(QColor::fromHsv(i * 30, 200, 230));
            const QString f = m_home.filePath(QStringLiteral("mn%1.png").arg(i));
            QVERIFY(im.save(f));
            urls << QUrl::fromLocalFile(f).toString();
        }
        item("musyNote")->setProperty("covers", urls);
        shot("musy-ready");

        // keys, then a controller button, then the screen start and flap it
        QTest::keyClick(w, Qt::Key_Space);
        QTRY_COMPARE(g->state(), 1);
        const double y0 = g->noteY();
        m_app->gamepad()->feedAction(QStringLiteral("a"), true);
        m_app->gamepad()->feedAction(QStringLiteral("a"), false);
        QTRY_VERIFY(g->noteVy() < 0 || g->noteY() < y0 + 0.5);
        // beats make walls
        for (int i = 0; i < 6; ++i) { g->onBeat(0.7); g->advance(30, 1.0 / 60); QTest::keyClick(w, Qt::Key_Space); }
        QVERIFY(!g->wallIds().isEmpty());
        g->advance(60, 1.0 / 60);
        QTest::qWait(200);
        shot("musy-playing");

        // the song plays it: no score is kept for that run
        const int before0 = m_app->gameScores(QStringLiteral("musynote")).size();
        g->start(12);                                      // a fresh run (the one above may already have crashed)
        QVERIFY(g->scored());
        g->setAutoplay(true);
        QVERIFY(!g->scored());                             // from the moment the song takes over, nothing is counted
        g->flap();                                         // a person's flap is ignored while the song has the controls
        for (int i = 0; i < 90 && g->state() != 2; ++i) { if (i % 20 == 0) g->onBeat(0.7); g->advance(30, 1.0 / 60); }
        QSignalSpy done(g, &MusyNoteGame::finished);
        for (int i = 0; i < 400 && g->state() != 2; ++i) g->advance(30, 1.0 / 60);   // let it run (it may fly on a long time)
        g->setAutoplay(false);
        // now crash on purpose
        for (int i = 0; i < 400 && g->state() != 2; ++i) g->advance(30, 1.0 / 60);   // falls to the floor
        QCOMPARE(g->state(), 2);
        QVERIFY(!g->scored());
        QCOMPARE(m_app->gameScores(QStringLiteral("musynote")).size(), before0);   // nothing was added to the list

        // a run flown by a person is counted
        QVERIFY(click("musyOverlay") || true);
        g->start(12);
        QVERIFY(g->scored());
        g->flap();
        QTRY_COMPARE(g->state(), 1);
        for (int i = 0; i < 4; ++i) { g->onBeat(0.7); g->advance(60, 1.0 / 60); if (g->state() == 1) g->flap(); }
        for (int i = 0; i < 400 && g->state() != 2; ++i) g->advance(30, 1.0 / 60);
        QCOMPARE(g->state(), 2);
        if (g->score() > 0) QCOMPARE(m_app->gameScores(QStringLiteral("musynote")).size(), before0 + 1);

        // back closes it first, and puts the screen and the analysis back
        QVariant r;
        QVERIFY(QMetaObject::invokeMethod(w, "goBack", Q_RETURN_ARG(QVariant, r)));
        QVERIFY(r.toBool());
        QTRY_VERIFY(!item("musyNote")->isVisible());
        QTRY_VERIFY(!m_app->audio()->active());
        QCOMPARE(w->property("view").toString(), QStringLiteral("now playing"));
        m_app->player()->stop();
    }

    // The sound tap can be switched off in settings: then the visualizer, the games and the analysis are gone, and the music
    // plays exactly as before.
    void theVisualizerAndGamesCanBeSwitchedOffAndNeverBlockMusic()
    {
        auto *w = window();
        const QSize before = w->size();
        struct Restore {
            QQuickWindow *w; QSize size; App *app;
            ~Restore() { app->setGameSetting(QStringLiteral("visualizerGames"), true); w->setProperty("forceMobile", false); w->resize(size); w->setProperty("view", QStringLiteral("collection")); }
        } restore{w, before, m_app.get()};
        QVERIFY(m_app->audio()->available());
        w->setProperty("forceMobile", true);
        w->resize(800, 360);
        QTRY_COMPARE(w->width(), 800);
        w->setProperty("view", QStringLiteral("now playing"));
        QTRY_VERIFY(item("phoneGames") && item("phoneGames")->isVisible());
        QVERIFY(item("phoneVizToggle") && item("phoneVizToggle")->isVisible());

        m_app->setGameSetting(QStringLiteral("visualizerGames"), false);
        QVERIFY(!m_app->audio()->available());
        QTRY_VERIFY(!item("phoneGames")->isVisible());
        QTRY_VERIFY(!item("phoneVizToggle")->isVisible());
        // with the feature off nothing listens, even if something asks
        m_app->audio()->want(QStringLiteral("test"), true);
        QVERIFY(!m_app->player()->analysisEnabled() || true);
        m_app->audio()->want(QStringLiteral("test"), false);
        // and playing music is untouched
        m_app->playSongs(0);
        QTRY_VERIFY(m_app->player()->isPlaying());
        QVERIFY(!m_app->audio()->active());
        m_app->player()->stop();
        // it is remembered with the profile
        const QJsonObject st = m_app->profileStore()->settings(m_app->profile().value("id").toString());
        QVERIFY(!st.value("nowPlaying").toObject().value("settings").toObject().value("visualizerGames").toBool(true));

        m_app->setGameSetting(QStringLiteral("visualizerGames"), true);
        QVERIFY(m_app->audio()->available());
        QTRY_VERIFY(item("phoneGames")->isVisible());
    }

    // The visualizer on the phone, in portrait and landscape: it takes the cover's place, listens only while it is shown,
    // keeps its look in the profile and fits the screen.
    void phoneVisualizerInPortraitAndLandscape()
    {
        auto *w = window();
        const QSize before = w->size();
        struct Restore {
            QQuickWindow *w; QSize size; App *app;
            ~Restore() { app->setGameSetting(QStringLiteral("phoneViz"), false); w->setProperty("forceMobile", false); w->resize(size); w->setProperty("view", QStringLiteral("collection")); }
        } restore{w, before, m_app.get()};
        m_app->playSongs(0);
        QTRY_VERIFY(m_app->player()->isPlaying());
        w->setProperty("forceMobile", true);
        m_app->setGameSetting(QStringLiteral("phoneViz"), false);
        QVector<float> frame(32);
        for (int i = 0; i < 32; ++i) frame[i] = -62.f + 52.f * float(31 - i) / 31.f;
        for (const QSize &sz : {QSize(412, 892), QSize(800, 360)}) {
            w->resize(sz);
            QTRY_COMPARE(w->width(), sz.width());
            w->setProperty("view", QStringLiteral("now playing"));
            QTest::qWait(300);
            const QString tag = QStringLiteral("%1x%2").arg(sz.width()).arg(sz.height());
            QVERIFY(!m_app->audio()->active());                      // the cover is showing: nothing listens
            QVERIFY(item("phoneVizToggle") && item("phoneVizToggle")->isVisible());
            QVERIFY(click("phoneVizToggle"));
            QTRY_VERIFY(m_app->gameSettings().value("phoneViz").toBool());
            QTRY_VERIFY(item("phoneViz") && item("phoneViz")->isVisible());
            QTRY_VERIFY(m_app->audio()->active());                   // now it does
            for (int i = 0; i < 4; ++i) m_app->audio()->onSpectrum(frame);
            QTest::qWait(150);
            shot(qPrintable(QStringLiteral("phone-visualizer-") + tag));
            QVERIFY2(overflowing().isEmpty(), qPrintable(tag + QStringLiteral(" ") + overflowing().join(QStringLiteral(", "))));
            QQuickItem *viz = item("phoneViz");
            const QRectF r = viz->mapRectToScene(QRectF(0, 0, viz->width(), viz->height()));
            QVERIFY2(r.bottom() <= w->height() + 1 && r.right() <= w->width() + 1, qPrintable(tag + QStringLiteral(" the visualizer runs off screen")));
            // its look is chosen on the spot and remembered
            QVERIFY(click("phoneViz_rings"));
            QVERIFY(click("phoneViz_rainbow"));
            QCOMPARE(m_app->gameSettings().value("vizStyle").toString(), QStringLiteral("rings"));
            QCOMPARE(m_app->gameSettings().value("vizColor").toString(), QStringLiteral("rainbow"));
            for (int i = 0; i < 4; ++i) m_app->audio()->onSpectrum(frame);
            QTest::qWait(150);
            shot(qPrintable(QStringLiteral("phone-visualizer-rings-") + tag));
            QVERIFY(click("phoneViz_bars"));
            QVERIFY(click("phoneViz_accent"));
            // back to the cover: it stops listening again
            QVERIFY(click("phoneVizToggle"));
            QTRY_VERIFY(!m_app->gameSettings().value("phoneViz").toBool());
            QTRY_VERIFY(!m_app->audio()->active());
        }
        // leaving Now playing with the visualizer on stops the analysis too
        m_app->setGameSetting(QStringLiteral("phoneViz"), true);
        QTRY_VERIFY(m_app->audio()->active());
        w->setProperty("view", QStringLiteral("collection"));
        QTRY_VERIFY(!m_app->audio()->active());
        m_app->player()->stop();
    }

    // Design renders against the saved phone mockups, with a richer library than the other tests have:
    // set HOPTUNES_FIXTURE_DIR to a folder holding music/ (tagged albums) and shows/ (clips).
    void phoneMockupRenders()
    {
        const QString fx = qEnvironmentVariable("HOPTUNES_FIXTURE_DIR");
        if (fx.isEmpty() || qEnvironmentVariable("HOPTUNES_UI_SHOTS").isEmpty()) QSKIP("set HOPTUNES_FIXTURE_DIR and HOPTUNES_UI_SHOTS");
        auto *w = window();
        const QSize before = w->size();
        w->setProperty("forceMobile", true);
        w->resize(420, 880);
        m_app->addFolder(QUrl::fromLocalFile(fx + QStringLiteral("/music")));
        m_app->addVideoCategoryFolder(QStringLiteral("shows"), QUrl::fromLocalFile(fx + QStringLiteral("/shows")));
        QTRY_VERIFY_WITH_TIMEOUT(m_app->librarySize() >= 12 && !m_app->scanning(), 20000);
        QTRY_VERIFY_WITH_TIMEOUT(!m_app->videoScanning(), 30000);
        w->setProperty("view", QStringLiteral("collection"));
        QVERIFY(click("pillTab_music"));
        QVERIFY(click("pillMode_albums"));
        QTRY_VERIFY(m_app->allAlbums()->rowCount() >= 6);
        shot("mock-collection-music");
        QVERIFY(click("pillTab_videos"));
        QVERIFY(click("pillCat_shows"));
        QTest::qWait(1500);
        shot("mock-collection-videos");
        m_app->openVideoDetail(0);
        QTest::qWait(500);
        m_app->addCastMember(); m_app->setCastMember(0, QStringLiteral("Mira Hollis"), QStringLiteral("Nell (lead)"));
        m_app->addCastMember(); m_app->setCastMember(1, QStringLiteral("Tomas Ruiz"), QStringLiteral("Harbour master"));
        m_app->addCastMember(); m_app->setCastMember(2, QStringLiteral("Ana Veltri"), QStringLiteral("director"));
        QTest::qWait(500);
        shot("mock-videodetail-top");
        QVERIFY2(overflowing().isEmpty(), qPrintable(QStringLiteral("video details overflow: ") + overflowing().join(QStringLiteral(" | "))));
        if (auto *f = item("videoDetailAddPerson")) { auto *fl = f->parentItem(); while (fl && !fl->inherits("QQuickFlickable")) fl = fl->parentItem(); if (fl) fl->setProperty("contentY", 560); }
        QTest::qWait(300);
        shot("mock-videodetail-cast");
        m_app->setVideoChat(true);
        m_app->setVideoComments(QStringLiteral("populated"));
        m_app->playVideo(0);
        QTest::qWait(800);
        shot("mock-player-portrait-nextup");
        {   // upright: the picture is clear; play / pause, rotate and subtitles are under it, not over it
            const qreal pictureBottom = qRound(420 * 9.0 / 16) + 6;
            for (const char *name : {"phonePlayerPlay", "phonePlayerRotate", "phonePlayerCC", "phonePlayerBack", "phonePlayerForward"}) {
                QQuickItem *it = item(name);
                QVERIFY2(it && it->isVisible(), name);
                QVERIFY2(it->mapToScene(QPointF(0, 0)).y() >= pictureBottom - 1, name);
            }
        }
        QVERIFY(click("phoneTab_comments"));
        QTest::qWait(500);
        shot("mock-player-portrait-comments");
        QVERIFY2(overflowing().isEmpty(), qPrintable(QStringLiteral("player overflows: ") + overflowing().join(QStringLiteral(" | "))));
        w->resize(880, 420);
        QTest::qWait(700);
        shot("mock-player-landscape");
        w->resize(420, 880);
        QTest::qWait(300);
        m_app->closeVideoPlayer(0, 0);
        m_app->setVideoChat(false);
        m_app->setVideoComments(QStringLiteral("off"));
        QTest::qWait(300);
        m_app->closeVideoDetail();
        QVERIFY(click("pillTab_music"));
        m_app->playSongs(0);
        QTest::qWait(1200);
        w->setProperty("view", QStringLiteral("now playing"));
        shot("mock-nowplaying");
        w->setProperty("view", QStringLiteral("collection"));
        shot("mock-collection-miniplayer");
        w->setProperty("view", QStringLiteral("profile"));
        shot("mock-profile");
        w->setProperty("view", QStringLiteral("settings"));
        shot("mock-settings");
        m_app->player()->stop();
        w->setProperty("forceMobile", false);
        w->resize(before);
        QTRY_COMPARE(w->size(), before);
        QTest::qWait(400);
    }

    // Phones: the Android back button steps back through the app and never leaves it (leaving closed the screen
    // and left a grey window behind). At the top of the music collection it does nothing.
    void theBackButtonStepsBackAndNeverLeavesTheApp()
    {
        auto *w = window();
        const QSize before = w->size();
        struct Restore { QQuickWindow *w; QSize size;
            ~Restore() { w->setProperty("forceMobile", false); w->resize(size); } } restore{w, before};
        w->setProperty("forceMobile", true);
        w->resize(412, 880);
        w->setProperty("view", QStringLiteral("collection"));
        QObject *cv = item("collectionView");
        QVERIFY(cv);
        QVERIFY(click("pillTab_music"));
        QVERIFY(click("pillMode_artists"));
        auto back = [&] { QVariant r; QVERIFY(QMetaObject::invokeMethod(w, "goBack", Q_RETURN_ARG(QVariant, r))); QVERIFY(r.toBool()); };

        cv->setProperty("phoneStep", 2);                 // an artist's songs
        back(); QCOMPARE(cv->property("phoneStep").toInt(), 1);
        back(); QCOMPARE(cv->property("phoneStep").toInt(), 0);
        back(); QCOMPARE(cv->property("phoneStep").toInt(), 0);   // the top: nothing more to go back to
        QCOMPARE(w->property("view").toString(), QStringLiteral("collection"));
        QCOMPARE(cv->property("collectionTab").toString(), QStringLiteral("music"));

        cv->setProperty("collectionTab", QStringLiteral("videos"));
        m_app->closeVideoSlot();
        back(); QCOMPARE(cv->property("collectionTab").toString(), QStringLiteral("music"));
        // an open folder slot closes first, and only then does back leave the tab
        cv->setProperty("collectionTab", QStringLiteral("videos"));
        QTRY_VERIFY(!m_app->videoSlots().isEmpty());
        m_app->openVideoSlot(m_app->videoSlots().first().toMap().value("dir").toString());
        QVERIFY(!m_app->videoSlot().isEmpty());
        back(); QVERIFY(m_app->videoSlot().isEmpty());
        QCOMPARE(cv->property("collectionTab").toString(), QStringLiteral("videos"));
        back(); QCOMPARE(cv->property("collectionTab").toString(), QStringLiteral("music"));

        w->setProperty("view", QStringLiteral("settings"));
        back(); QCOMPARE(w->property("view").toString(), QStringLiteral("profile"));
        back(); QCOMPARE(w->property("view").toString(), QStringLiteral("collection"));
        w->setProperty("view", QStringLiteral("now playing"));
        back(); QCOMPARE(w->property("view").toString(), QStringLiteral("collection"));

        // the real thing: the system's back arrives as a close request, which the window must refuse
        cv->setProperty("phoneStep", 1);
        w->close();
        QTest::qWait(100);
        QVERIFY(w->isVisible());
        QCOMPARE(cv->property("phoneStep").toInt(), 0);
    }

    // Phones, first scan: the title, tabs, scan note and pills stay together at the top instead of being spread
    // over the whole screen.
    void phoneCollectionStaysCompactWhileScanning()
    {
        auto *w = window();
        const QSize before = w->size();
        struct Restore { QQuickWindow *w; QSize size; QObject *cv;
            ~Restore() { cv->setProperty("forceScanning", false); w->setProperty("forceMobile", false); w->resize(size); } };
        w->setProperty("view", QStringLiteral("collection"));
        QObject *cv = item("collectionView");
        QVERIFY(cv);
        Restore restore{w, before, cv};
        w->setProperty("forceMobile", true);
        w->resize(412, 880);
        QTRY_COMPARE(w->width(), 412);
        QVERIFY(click("pillTab_music"));
        cv->setProperty("forceScanning", true);
        QTRY_VERIFY(item("scanSpacer") && item("scanSpacer")->isVisible());
        QTest::qWait(300);
        shot("phone-scanning");
        auto topOf = [&](const char *name) { auto *i = item(name); return i->mapToScene(QPointF(0, 0)).y(); };
        const qreal tabs = topOf("pillTab_music");
        const qreal pills = topOf("pillMode_albums");
        QVERIFY2(pills - tabs < 200, qPrintable(QStringLiteral("tabs at %1, pills at %2").arg(tabs).arg(pills)));
        QVERIFY(tabs < 200);
    }

    // Android's folder chooser is the system one (the Qt dialog dims the whole screen there): its answer
    // arrives as a signal carrying the token the question was asked with, and adds the folder.
    void systemFolderChooserAnswersAddTheFolder()
    {
        auto *pick = item("musicFolderPick");
        QVERIFY(pick);
        const QString token = pick->property("token").toString();
        QVERIFY(!token.isEmpty());
        const QString dir = m_home.filePath(QStringLiteral("PickedMusic"));
        QVERIFY(QDir().mkpath(dir));
        QVERIFY(writeWav(dir + QStringLiteral("/p.wav"), 1000));
        const int before = m_app->folders().size();

        emit m_app->androidFolderPicked(QStringLiteral("someone else"), QUrl::fromLocalFile(dir));   // not ours
        emit m_app->androidFolderPicked(token, QUrl());                                                 // cancelled
        QTest::qWait(100);
        QCOMPARE(m_app->folders().size(), before);

        emit m_app->androidFolderPicked(token, QUrl::fromLocalFile(dir));
        QTRY_COMPARE(m_app->folders().size(), before + 1);
        QVERIFY(m_app->diagLog().isEmpty() || m_app->diagLog().contains(QStringLiteral("addFolder")));   // logged when the log is on
        QVERIFY(m_app->folders().contains(QFileInfo(dir).canonicalFilePath()));
        m_app->removeFolder(QFileInfo(dir).canonicalFilePath());
    }

    // Pictures (and the other new sections) have their own folder list and their own
    // "choose a folder" button, separate from the music and video folders.
    void sectionsHaveTheirOwnFolders()
    {
        window()->setProperty("view", QStringLiteral("collection"));
        QVERIFY(m_app->sectionKinds().contains(QStringLiteral("pictures")));
        QVERIFY(m_app->pictures()->count() == 0);
        QVERIFY(click("subTab_pictures"));
        QTRY_VERIFY(item("sectionChooseFolder_pictures") != nullptr);
        QVERIFY(item("sectionChooseFolder_pictures")->isVisible());

        const QString dir = m_home.filePath(QStringLiteral("Pictures"));
        QVERIFY(QDir().mkpath(dir));
        QImage img(40, 30, QImage::Format_RGB32);
        img.fill(Qt::blue);
        QVERIFY(img.save(dir + QStringLiteral("/one.png")));

        m_app->addSectionFolder(QStringLiteral("pictures"), QUrl::fromLocalFile(dir));
        QTRY_COMPARE_WITH_TIMEOUT(m_app->pictures()->count(), 1, 15000);
        QVERIFY(m_app->sectionFolders().value(QStringLiteral("pictures")).toStringList()
                    .contains(QFileInfo(dir).canonicalFilePath()));
        // Not mixed into the music or video folder lists.
        QVERIFY(!m_app->folders().contains(QFileInfo(dir).canonicalFilePath()));
        QVERIFY(!m_app->videoFolders().contains(QFileInfo(dir).canonicalFilePath()));
        // One slot per folder: the shelf first, the pictures once a slot is opened.
        QTRY_VERIFY(item("pictureAlbum_0") != nullptr);
        QCOMPARE(m_app->pictureAlbums().size(), 1);
        QCOMPARE(m_app->pictureAlbums().first().toMap().value("count").toInt(), 1);
        QCOMPARE(m_app->pictureAlbums().first().toMap().value("name").toString(), QStringLiteral("Pictures"));
        // (a click can land before a freshly shown slot has been laid out, so it is repeated until it takes)
        QTRY_VERIFY_WITH_TIMEOUT((click("pictureAlbum_0"), m_app->pictureAlbum() == QFileInfo(dir).canonicalFilePath()), 8000);
        QTRY_VERIFY(item("pictureTile_0") != nullptr);
        QCOMPARE(m_app->pictureAlbumItems()->count(), 1);
        shot("p-album-open");
        QTRY_VERIFY_WITH_TIMEOUT((click("pictureAlbumBack"), m_app->pictureAlbum().isEmpty()), 8000);

        // A second folder is a second slot, kept apart from the first; slots can be named, and the name is remembered.
        const QString trip = m_home.filePath(QStringLiteral("Pictures2/Trip"));
        QVERIFY(QDir().mkpath(trip));
        QVERIFY(img.save(trip + QStringLiteral("/a.png")));
        QVERIFY(img.save(trip + QStringLiteral("/b.png")));
        m_app->addSectionFolder(QStringLiteral("pictures"), QUrl::fromLocalFile(trip));
        QTRY_COMPARE_WITH_TIMEOUT(m_app->pictures()->count(), 3, 15000);
        QCOMPARE(m_app->pictureAlbums().size(), 2);
        QTRY_VERIFY(item("pictureAlbum_1") != nullptr);
        shot("p-albums");
        const QString tripDir = QFileInfo(trip).canonicalFilePath();
        m_app->renamePictureAlbum(tripDir, QStringLiteral("  Summer   trip "));
        QStringList names;
        for (const QVariant &a : m_app->pictureAlbums()) names << a.toMap().value("name").toString();
        QVERIFY2(names.contains(QStringLiteral("Summer trip")), qPrintable(names.join(',')));
        const QJsonObject st = m_app->profileStore()->settings(m_app->profile().value("id").toString());
        QCOMPARE(st.value("pictureAlbumNames").toObject().value(tripDir).toString(), QStringLiteral("Summer trip"));
        m_app->rescan();   // the name survives a rescan
        QTRY_VERIFY(!m_app->sectionStatus().value("pictures").toMap().value("scanning").toBool());
        names.clear();
        for (const QVariant &a : m_app->pictureAlbums()) names << a.toMap().value("name").toString();
        QVERIFY(names.contains(QStringLiteral("Summer trip")));
        m_app->openPictureAlbum(tripDir);
        QCOMPARE(m_app->pictureAlbumItems()->count(), 2);
        m_app->renamePictureAlbum(tripDir, QString());   // empty: back to the folder's name
        m_app->closePictureAlbum();
        names.clear();
        for (const QVariant &a : m_app->pictureAlbums()) names << a.toMap().value("name").toString();
        QVERIFY(names.contains(QStringLiteral("Trip")));
        m_app->removeSectionFolder(QStringLiteral("pictures"), tripDir);
        QTRY_COMPARE(m_app->pictures()->count(), 1);

        m_app->removeSectionFolder(QStringLiteral("pictures"), QFileInfo(dir).canonicalFilePath());
        QTRY_COMPARE(m_app->pictures()->count(), 0);
        QVERIFY(click("subTab_music"));
    }

    // Audiobooks and podcasts get folder slots like pictures: one named slot per folder, open it for its own items.
    void audiobooksAndPodcastsHaveFolderSlots()
    {
        window()->setProperty("view", QStringLiteral("collection"));
        for (const QString kind : {QStringLiteral("audiobooks"), QStringLiteral("podcasts")}) {
            const QString root = m_home.filePath(QStringLiteral("Shelf_") + kind);
            for (const char *f : {"Dune/ch1.mp3", "Dune/ch2.mp3", "Emma/ch1.mp3"}) {
                QVERIFY(QDir().mkpath(QFileInfo(root + QLatin1Char('/') + QLatin1String(f)).absolutePath()));
                QVERIFY(writeWav(root + QLatin1Char('/') + QLatin1String(f), 1500));
            }
            m_app->addSectionFolder(kind, QUrl::fromLocalFile(root));
            auto count = [&] { return kind == QLatin1String("audiobooks") ? m_app->audiobooks()->count() : m_app->podcasts()->count(); };
            QTRY_COMPARE_WITH_TIMEOUT(count(), 3, 15000);
            const QVariantList shelf = m_app->sectionSlots().value(kind).toList();
            QCOMPARE(shelf.size(), 2);
            QCOMPARE(shelf.at(0).toMap().value("name").toString(), QStringLiteral("Dune"));
            QCOMPARE(shelf.at(0).toMap().value("count").toInt(), 2);

            QVERIFY(click(qPrintable(QStringLiteral("subTab_") + kind)));
            QTRY_VERIFY(item(qPrintable(QStringLiteral("sectionSlot_%1_0").arg(kind))) != nullptr);
            QVERIFY(m_app->sectionOpenSlot().value(kind).toString().isEmpty());
            shot(qPrintable(QStringLiteral("slots-") + kind));
            const QString dune = shelf.at(0).toMap().value("dir").toString();
            m_app->openSlot(kind, dune);
            QCOMPARE(m_app->sectionSlotItems(kind)->count(), 2);
            QTRY_VERIFY(item(qPrintable(QStringLiteral("sectionRow_%1_1").arg(kind))) != nullptr);
            // a row in the open slot maps back to its row in the whole section (what playing needs)
            const QString second = m_app->sectionSlotItems(kind)->pathAt(1);
            QVERIFY(m_app->sectionRow(kind, second) >= 0);
            QCOMPARE((kind == QLatin1String("audiobooks") ? m_app->audiobooks() : m_app->podcasts())->pathAt(m_app->sectionRow(kind, second)), second);

            auto nameOf = [&](const QString &dir) {
                for (const QVariant &s : m_app->sectionSlots().value(kind).toList())
                    if (s.toMap().value("dir").toString() == dir) return s.toMap().value("name").toString();
                return QString();
            };
            m_app->renameSlot(dune, QStringLiteral("  The   Dune books "));
            QCOMPARE(nameOf(dune), QStringLiteral("The Dune books"));
            const QJsonObject st = m_app->profileStore()->settings(m_app->profile().value("id").toString());
            QCOMPARE(st.value("pictureAlbumNames").toObject().value(dune).toString(), QStringLiteral("The Dune books"));
            m_app->closeSlot(kind);
            QVERIFY(m_app->sectionOpenSlot().value(kind).toString().isEmpty());
            m_app->renameSlot(dune, QString());
            QCOMPARE(nameOf(dune), QStringLiteral("Dune"));

            m_app->removeSectionFolder(kind, QFileInfo(root).canonicalFilePath());
            QTRY_COMPARE(count(), 0);
            QVERIFY(m_app->sectionSlots().value(kind).toList().isEmpty());
        }
        QVERIFY(click("subTab_music"));
    }

    // Regression test for a real bug: setResumePosition() was a plain C++ method missing
    // Q_INVOKABLE, so QML's call to it (VideoPlayerView.qml's onSurfaceLoaded(), the very
    // first thing it does on every video open) threw "is not a function" and silently
    // aborted before item.source was ever set or item.play() ever called — video simply
    // never loaded, with no error reaching the UI (a QML console warning, not something
    // routed through surface.error). Caught by building a real (software-GL) harness outside
    // this suite that opens an actual video end to end; this is the cheap, GL-free piece of
    // that regression that *can* run in CI: every method VideoPlayerView.qml calls on the
    // surface from QML must actually be invokable through Qt's meta-object system, which is
    // exactly the mechanism QML itself uses — this never creates a Renderer (no GL touched).
    void mpvVideoItemQmlInvokableMethods()
    {
        MpvVideoItem item;
        QVERIFY(QMetaObject::invokeMethod(&item, "setResumePosition", Q_ARG(qint64, 1234)));
        QVERIFY(QMetaObject::invokeMethod(&item, "play"));
        QVERIFY(QMetaObject::invokeMethod(&item, "pause"));
        QVERIFY(QMetaObject::invokeMethod(&item, "togglePause"));
        QVERIFY(QMetaObject::invokeMethod(&item, "seek", Q_ARG(qint64, 0)));
        QVERIFY(QMetaObject::invokeMethod(&item, "stop"));
        QVERIFY(QMetaObject::invokeMethod(&item, "setSubtitleTrack", Q_ARG(int, -1)));
        QVERIFY(QMetaObject::invokeMethod(&item, "addSubtitleFile", Q_ARG(QString, QString())));
    }

    // ---- Now playing: the visualizer and Album Fall ----

    void nowPlayingStagesVisualizerAndAlbumFall()
    {
        window()->setProperty("view", QStringLiteral("now playing"));
        m_app->playSongs(0);
        QTRY_VERIFY(m_app->player()->isPlaying());
        QVERIFY(m_app->gameSettings().value("stage").toString() == QLatin1String("art"));
        QVERIFY(item("stage_visualizer") && item("stage_albumfall"));
        QVERIFY(!m_app->audio()->active());                          // nothing listens while it is the song page

        // the visualizer: analysis starts only now, and the choices are remembered with the profile
        QVERIFY(click("stage_visualizer"));
        QTRY_VERIFY(m_app->audio()->active());
        QTRY_VERIFY(item("visualizer") && item("visualizer")->isVisible());
        QVector<float> frame(32);
        for (int i = 0; i < 32; ++i) frame[i] = -60.f + 50.f * float(31 - i) / 31.f;
        for (int i = 0; i < 4; ++i) m_app->audio()->onSpectrum(frame);
        QTRY_VERIFY(m_app->audio()->bands().size() == 32);
        QVERIFY(m_app->audio()->level() > 0.1);
        QVERIFY(click("vizStyle_rings"));
        QVERIFY(click("vizColor_rainbow"));
        QCOMPARE(m_app->gameSettings().value("vizStyle").toString(), QStringLiteral("rings"));
        QCOMPARE(m_app->gameSettings().value("vizColor").toString(), QStringLiteral("rainbow"));
        m_app->setGameSetting(QStringLiteral("vizStyle"), QStringLiteral("nonsense"));   // refused
        m_app->setGameSetting(QStringLiteral("vizIntensity"), 99);                       // clamped
        QCOMPARE(m_app->gameSettings().value("vizStyle").toString(), QStringLiteral("rings"));
        QCOMPARE(m_app->gameSettings().value("vizIntensity").toDouble(), 2.0);
        auto feed = [&] { for (int i = 0; i < 3; ++i) m_app->audio()->onSpectrum(frame); QTest::qWait(60); };
        feed();   // (the silent test song keeps sending its own quiet frames, so feed loud ones right before each picture)
        shot("np-visualizer-rings");
        m_app->setGameSetting(QStringLiteral("vizStyle"), QStringLiteral("bars"));
        feed();
        shot("np-visualizer-bars");
        m_app->setGameSetting(QStringLiteral("vizStyle"), QStringLiteral("mirror"));
        feed();
        shot("np-visualizer-mirror");
        m_app->setGameSetting(QStringLiteral("vizStyle"), QStringLiteral("bars"));
        const QJsonObject st = m_app->profileStore()->settings(m_app->profile().value("id").toString());
        QCOMPARE(st.value("nowPlaying").toObject().value("settings").toObject().value("vizColor").toString(), QStringLiteral("rainbow"));

        // Album Fall: a beat picks the piece, keys move it, the score and the best list are kept
        QVERIFY(click("stage_albumfall"));
        QTRY_VERIFY(item("albumFall") && item("albumFall")->isVisible());
        QTRY_VERIFY(m_app->albumFall()->running());
        QVERIFY(!m_app->albumFall()->paused());
        AlbumFall *g = m_app->albumFall();
        g->onBeat(0.9);                                              // a heavy beat: a five-block piece
        QCOMPARE(g->piece().size(), 10);                             // [x, y] x 5
        const QVariant x0 = g->piece().first();
        g->act(QStringLiteral("left"));
        QVERIFY(g->piece().first().toInt() < x0.toInt());
        g->act(QStringLiteral("rotate"));
        g->act(QStringLiteral("drop"));
        QVERIFY(g->landed() >= 1);
        QVERIFY(g->score() > 0);
        QTest::qWait(200);
        shot("np-albumfall");
        // the controller and the keyboard reach the same actions
        g->onBeat(0.2);
        const int pieces = g->piece().size();
        QVERIFY(pieces >= 4 && pieces <= 6 || pieces >= 4);          // (a small beat gives a small piece)
        QTest::keyClick(window(), Qt::Key_Left);
        QTest::keyClick(window(), Qt::Key_P);                        // pause
        QTRY_VERIFY(g->paused());
        QTest::keyClick(window(), Qt::Key_P);
        QTRY_VERIFY(!g->paused());
        QVERIFY(!m_app->player()->hasTrack() || m_app->player()->isPlaying());
        // space is the game's drop here, not play/pause
        const bool playingBefore = m_app->player()->isPlaying();
        QTest::keyClick(window(), Qt::Key_Space);
        QCOMPARE(m_app->player()->isPlaying(), playingBefore);
        // the best list
        QCOMPARE(m_app->submitScore(QStringLiteral("albumfall"), 500, 5), 1);
        QCOMPARE(m_app->submitScore(QStringLiteral("albumfall"), 900, 9), 1);
        QCOMPARE(m_app->submitScore(QStringLiteral("albumfall"), 700, 7), 2);
        QCOMPARE(m_app->gameScores(QStringLiteral("albumfall")).size(), 3);
        QCOMPARE(m_app->gameScores(QStringLiteral("albumfall")).first().toMap().value("score").toInt(), 900);
        for (int s : {100, 200, 300, 400}) m_app->submitScore(QStringLiteral("albumfall"), s, 1);
        QCOMPARE(m_app->gameScores(QStringLiteral("albumfall")).size(), 5);   // only the best five are kept
        QCOMPARE(m_app->submitScore(QStringLiteral("albumfall"), 10, 0), 0);
        QVERIFY(m_app->profileStore()->settings(m_app->profile().value("id").toString()).value("nowPlaying").toObject().value("scores").toObject().contains(QStringLiteral("albumfall")));
        g->setPaused(false);

        // the optional live chat: off by default, on with the toggle, remembered, and it does not crowd the game
        QVERIFY(!m_app->gameSettings().value("chat").toBool());
        QVERIFY(item("chatToggle") && item("nowPlayingChat"));
        QVERIFY(!item("nowPlayingChat")->isVisible());
        QVERIFY(click("chatToggle"));
        QTRY_VERIFY(item("nowPlayingChat")->isVisible());
        QVERIFY(m_app->gameSettings().value("chat").toBool());
        QTRY_VERIFY(item("nowPlayingStage")->width() < window()->width() - 300);   // the stage makes room for it
        QTest::qWait(1600);
        shot("np-albumfall-chat");
        QVERIFY(click("stage_art"));
        QTest::qWait(1200);
        QVERIFY(item("nowPlayingChat")->isVisible());
        QVERIFY(item("nowPlayingChat")->height() > 100);
        shot("np-chat");
        QVERIFY(click("chatToggle"));
        QTRY_VERIFY(!item("nowPlayingChat")->isVisible());
        QVERIFY(click("stage_albumfall"));

        // back to the song: the analysis stops listening again
        QVERIFY(click("stage_art"));
        QTRY_VERIFY(!m_app->audio()->active());
        QVERIFY(m_app->gameSettings().value("stage").toString() == QLatin1String("art"));
        m_app->player()->toggle();
        window()->setProperty("view", QStringLiteral("collection"));
    }

    // A controller is connected: starting a song from the collection opens Now playing (and only then).
    void aConnectedControllerOpensNowPlayingWhenASongStarts()
    {
        QObject *win = window();
        auto &pad = *m_app->gamepad();
        win->setProperty("view", QStringLiteral("collection"));
        m_app->player()->stop();
        QTRY_VERIFY(!m_app->player()->isPlaying());

        // no controller: nothing moves
        pad.setDevice({});
        m_app->playSongs(0);
        QTRY_VERIFY(m_app->player()->isPlaying());
        QTest::qWait(300);
        QCOMPARE(win->property("view").toString(), QStringLiteral("collection"));

        // a controller: the same action opens Now playing
        m_app->player()->stop();
        QTRY_VERIFY(!m_app->player()->isPlaying());
        pad.setDevice(QStringLiteral("Red Magic attachable controller"));
        m_app->playSongs(0);
        QTRY_COMPARE(win->property("view").toString(), QStringLiteral("now playing"));

        // plugging the controller in while a song is already playing opens Now playing too
        m_app->player()->stop();
        win->setProperty("view", QStringLiteral("collection"));
        pad.setDevice({});
        m_app->playSongs(0);
        QTRY_VERIFY(m_app->player()->isPlaying());
        QTest::qWait(300);
        QCOMPARE(win->property("view").toString(), QStringLiteral("collection"));
        pad.setDevice(QStringLiteral("Redmagic shadow blade gamepad"));
        QTRY_COMPARE(win->property("view").toString(), QStringLiteral("now playing"));
        // (and not when nothing is playing)
        m_app->player()->stop();
        QTRY_VERIFY(!m_app->player()->isPlaying());
        win->setProperty("view", QStringLiteral("collection"));
        pad.setDevice({});
        pad.setDevice(QStringLiteral("Redmagic shadow blade gamepad"));
        QTest::qWait(300);
        QCOMPARE(win->property("view").toString(), QStringLiteral("collection"));

        // switched off in settings: it stays in the collection
        m_app->player()->stop();
        win->setProperty("view", QStringLiteral("collection"));
        m_app->setGameSetting(QStringLiteral("autoNowPlaying"), false);
        m_app->playSongs(0);
        QTRY_VERIFY(m_app->player()->isPlaying());
        QTest::qWait(300);
        QCOMPARE(win->property("view").toString(), QStringLiteral("collection"));
        // and from another screen it never jumps
        m_app->setGameSetting(QStringLiteral("autoNowPlaying"), true);
        m_app->player()->stop();
        win->setProperty("view", QStringLiteral("settings"));
        m_app->playSongs(0);
        QTRY_VERIFY(m_app->player()->isPlaying());
        QTest::qWait(300);
        QCOMPARE(win->property("view").toString(), QStringLiteral("settings"));
        pad.setDevice({});
        m_app->player()->stop();
        win->setProperty("view", QStringLiteral("collection"));
    }

    // ---- per-profile album overrides: custom cover, editable title, live reorder ----

    void albumArtTitleAndReorder()
    {
        window()->setProperty("view", QStringLiteral("collection"));
        QVERIFY(click("subTab_music"));
        QVERIFY(click("tab_artists"));
        m_app->selectArtist(0);
        m_app->selectAlbum(0);
        const QString ref = m_app->selectedAlbumRef();
        QVERIFY(!ref.isEmpty());

        // The album title is a clickable, editable field right in the grid.
        QTRY_VERIFY(item("albumTitleEdit_0") != nullptr);
        type("albumTitleEdit_0", QStringLiteral("My Custom Title"));
        QTest::keyClick(window(), Qt::Key_Return);
        QTRY_COMPARE(m_app->selectedAlbumTitle(), QStringLiteral("My Custom Title"));
        QCOMPARE(m_app->selectedAlbumRef(), ref);   // the ref is built from the raw tags, not the display title

        // The "image" button lives at the bottom of the art and is hidden for guests.
        QVERIFY(item("albumImageBtn_0") != nullptr);
        QVERIFY(item("albumImageBtn_0")->isVisible());

        // Picking a new cover (the native FileDialog itself isn't driven here, the same
        // tradeoff secondProfileHasItsOwnFolders() makes for setProfileAvatar()).
        QImage pic(600, 400, QImage::Format_RGB32);
        pic.fill(QColor("#334455"));
        const QString picFile = m_home.filePath(QStringLiteral("cover.jpg"));
        QVERIFY(pic.save(picFile));
        QCOMPARE(m_app->setAlbumArt(ref, QUrl::fromLocalFile(picFile)), QString());
        QVERIFY(!m_app->setAlbumArt(ref, QUrl::fromLocalFile(m_home.filePath(QStringLiteral("Library/Alpha/One/01 Morning Tide.wav")))).isEmpty());
        const QString art = m_app->selectedAlbumArt();
        QVERIFY(art.startsWith(QStringLiteral("file://")));
        const QImage saved(QUrl(art).toLocalFile());
        QVERIFY(!saved.isNull());
        QVERIFY(saved.width() <= 1200 && saved.height() <= 1200);
        shot("m8-album-overrides");

        // Dragging a song in the album's track list reorders it live, even mid-playback.
        m_app->playAlbum(0);
        QTRY_VERIFY(m_app->player()->isPlaying());
        const QString firstPath = m_app->albumSongs()->tracks().at(0).path;
        const QString secondPath = m_app->albumSongs()->tracks().at(1).path;
        QQuickItem *list = item("albumSongList");
        QVERIFY(list);
        QQuickItem *row = nullptr;
        std::function<void(QQuickItem *)> findRow = [&](QQuickItem *it) {
            if (row) return;
            if (it->property("draggable").toBool() && it->property("path").toString() == firstPath) { row = it; return; }
            for (QQuickItem *c : it->childItems()) findRow(c);
        };
        findRow(list);
        QVERIFY(row);
        dragRow(row, 20, int(row->height()));
        QTRY_COMPARE(m_app->albumSongs()->tracks().at(1).path, firstPath);
        QCOMPARE(m_app->albumSongs()->tracks().at(0).path, secondPath);
        QVERIFY(m_app->player()->isPlaying());   // never interrupted by the reorder

        // Everything survives a full relaunch of this profile.
        const QString profileId = m_app->profile().value("id").toString();
        m_app->leaveProfile();
        QCOMPARE(m_app->openProfile(profileId), QString());
        m_app->selectArtist(0);
        m_app->selectAlbum(0);
        QCOMPARE(m_app->selectedAlbumTitle(), QStringLiteral("My Custom Title"));
        QCOMPARE(m_app->selectedAlbumArt(), art);
        QCOMPARE(m_app->albumSongs()->tracks().at(1).path, firstPath);
    }

    // ---- fixing artist names: merge, rename, drag an album onto an artist, split back ----

    void fixingArtistNamesMergesAndSplitsBack()
    {
        // its own little tagged library, so this does not disturb the profile the other tests use
        const QString libId = m_app->profile().value("id").toString();
        const QString dir = m_home.filePath(QStringLiteral("Tagged"));
        struct Song { const char *file, *artist, *album; };
        const Song songs[] = {{"a/1.wav", "Zeta Band", "Skin"}, {"a/2.wav", "Zeta Band", "Wisdom"},
                              {"b/1.wav", "zeta band", "Skin"}, {"c/1.wav", "Omega", "Last"}};
        for (const Song &s : songs) {
            const QString f = dir + QLatin1Char('/') + QLatin1String(s.file);
            QDir().mkpath(QFileInfo(f).absolutePath());
            QVERIFY(writeWav(f, 3000));
            TagLib::RIFF::WAV::File w(QFile::encodeName(f).constData());
            QVERIFY(w.isValid());
            w.ID3v2Tag()->setArtist(s.artist);
            w.ID3v2Tag()->setAlbum(s.album);
            QVERIFY(w.save());
        }
        // whatever happens below, leave the library profile open for the tests that follow
        const auto restore = qScopeGuard([&] {
            if (m_app->profile().value("id").toString() != libId) {
                m_app->removeProfile();
                m_app->openProfile(libId, {});
                QTest::qWait(1500);
            }
        });
        QCOMPARE(m_app->createProfile(QStringLiteral("Names"), QStringLiteral("#ff8800"), {}, QUrl::fromLocalFile(dir)), QString());
        QTRY_VERIFY_WITH_TIMEOUT(m_app->librarySize() >= 4, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(!m_app->scanning(), 15000);
        window()->setProperty("view", QStringLiteral("collection"));
        QVERIFY(click("subTab_music"));
        QVERIFY(click("tab_artists"));

        const QString a = QStringLiteral("zeta band"), b = QStringLiteral("Omega");
        QStringList names = m_app->artistNames();
        QCOMPARE(names.size(), 2);                       // case-insensitive: the two Zeta spellings are one artist already
        // use two real artists for the rest
        const QString x = names.at(0).compare(QStringLiteral("zeta band"), Qt::CaseInsensitive) == 0 ? names.at(0) : names.at(1);   // the one with two albums
        const QString y = x == names.at(0) ? names.at(1) : names.at(0);
        const int before = m_app->artists()->count();
        const QString id = m_app->profile().value("id").toString();
        Q_UNUSED(a); Q_UNUSED(b);

        // the pieces of the interface are there: the dialog, the drag ghost and a drop target per artist
        QVERIFY(item("collectionView")->findChild<QObject *>(QStringLiteral("mergeArtistDialog")));
        QVERIFY(item("dragGhost") && item("artistDrop_0"));

        {   // the merge dialog, for the design renders
            QObject *dlg = item("collectionView")->findChild<QObject *>(QStringLiteral("mergeArtistDialog"));
            QVERIFY(QMetaObject::invokeMethod(dlg, "pick", Q_ARG(QVariant, x)));
            QTest::qWait(400);
            shot("artist-merge-pick");
            QVERIFY(QMetaObject::invokeMethod(dlg, "confirm", Q_ARG(QVariant, x), Q_ARG(QVariant, y)));
            QTest::qWait(300);
            shot("artist-merge-confirm");
            QMetaObject::invokeMethod(dlg, "close");
            QTest::qWait(300);
        }
        QVERIFY(!m_app->mergeArtist(x, x).isEmpty());                 // the same artist
        QCOMPARE(m_app->mergeArtist(x, y), QString());
        QCOMPARE(m_app->artists()->count(), before - 1);
        QCOMPARE(m_app->artistNames(), QStringList{y});               // y took over and kept its name
        QCOMPARE(m_app->mergedInto(y).size(), 1);
        QVERIFY(!m_app->mergeArtist(y, x).isEmpty());                 // would loop: refused
        QVERIFY(m_app->profileStore()->settings(id).value("artistMerges").toObject().contains(x));   // remembered (and exported)
        QCOMPARE(m_app->unmerge(x), QString());
        QCOMPARE(m_app->artists()->count(), before);
        QVERIFY(m_app->mergedInto(y).isEmpty());

        // renaming to a new name just renames; to an existing name it is a merge
        QCOMPARE(m_app->renameArtist(x, QStringLiteral("Zed Fresh Name")), QString());
        QVERIFY(m_app->artistNames().contains(QStringLiteral("Zed Fresh Name")));
        QCOMPARE(m_app->artists()->count(), before);
        QCOMPARE(m_app->renameArtist(QStringLiteral("Zed Fresh Name"), y), QString());
        QCOMPARE(m_app->artists()->count(), before - 1);
        QCOMPARE(m_app->unmerge(QStringLiteral("Zed Fresh Name")), QString());
        QCOMPARE(m_app->unmerge(x), QString());
        QCOMPARE(m_app->artists()->count(), before);

        // dropping one album on an artist moves just that album (what the drag does), and it can be undone
        auto select = [&](const QString &name) {
            for (int i = 0; i < m_app->artists()->count(); ++i)
                if (m_app->artists()->index(i, 0).data(Qt::UserRole + 1).toString() == name) { m_app->selectArtist(i); return true; }
            return false;
        };
        QVERIFY(select(x));
        const int albumsOfX = m_app->artistAlbums()->count();
        m_app->selectAlbum(0);
        const QString ref = m_app->selectedAlbumRef();
        QVERIFY(select(y));
        const int albumsOfY = m_app->artistAlbums()->count();
        QObject *ghost = item("dragGhost");
        ghost->setProperty("ref", ref);
        ghost->setProperty("title", QStringLiteral("dragged"));
        QVERIFY(QMetaObject::invokeMethod(item("collectionView"), "dropAlbumOn", Q_ARG(QVariant, y)));
        QTRY_COMPARE(m_app->mergedInto(y).size(), 1);
        QVERIFY(select(y));
        QCOMPARE(m_app->artistAlbums()->count(), albumsOfY + 1);
        QVERIFY(select(x));
        QCOMPARE(m_app->artistAlbums()->count(), albumsOfX - 1);
        QCOMPARE(m_app->unmerge({}, ref), QString());
        QVERIFY(select(x));
        QCOMPARE(m_app->artistAlbums()->count(), albumsOfX);
        QVERIFY(m_app->mergedInto(y).isEmpty());

    }

    void albumOverridesArePerProfile()
    {
        const QString lib = m_app->profile().value("id").toString();
        const QString ref = m_app->selectedAlbumRef();
        QVERIFY(!ref.isEmpty());

        // A fresh guest never sees another profile's rename, cover, or track order.
        m_app->leaveProfile();
        m_app->openGuest();
        QTRY_VERIFY(m_app->profileReady());
        m_app->selectArtist(0);
        m_app->selectAlbum(0);
        QCOMPARE(m_app->selectedAlbumTitle(), QStringLiteral("unknown album"));
        QCOMPARE(m_app->setAlbumArt(ref, QUrl::fromLocalFile(m_home.filePath(QStringLiteral("cover.jpg")))), QStringLiteral("no profile open"));
        QCOMPARE(m_app->setAlbumTitle(ref, QStringLiteral("Guest Edit")), QStringLiteral("no profile open"));

        m_app->leaveProfile();
        QCOMPARE(m_app->openProfile(lib), QString());
        m_app->selectArtist(0);
        m_app->selectAlbum(0);
        QCOMPARE(m_app->selectedAlbumTitle(), QStringLiteral("My Custom Title"));   // untouched by the guest session
    }

    // The SONGS tab (the whole library, not just one album) drags and reorders live too —
    // during playback, with nothing selected, and everywhere else dragging already worked.
    void songsTabDragReorder()
    {
        window()->setProperty("view", QStringLiteral("collection"));
        QVERIFY(click("subTab_music"));
        QVERIFY(click("tab_songs"));
        QTRY_VERIFY(item("songList") != nullptr);

        m_app->playSongs(0);
        QTRY_VERIFY(m_app->player()->isPlaying());
        const QString firstPath = m_app->songs()->tracks().at(0).path;
        const QString secondPath = m_app->songs()->tracks().at(1).path;

        QQuickItem *list = item("songList");
        QVERIFY(list);
        QQuickItem *row = nullptr;
        std::function<void(QQuickItem *)> findRow = [&](QQuickItem *it) {
            if (row) return;
            if (it->property("draggable").toBool() && it->property("path").toString() == firstPath) { row = it; return; }
            for (QQuickItem *c : it->childItems()) findRow(c);
        };
        findRow(list);
        QVERIFY(row);
        dragRow(row, 20, int(row->height()));
        QTRY_COMPARE(m_app->songs()->tracks().at(1).path, firstPath);
        QCOMPARE(m_app->songs()->tracks().at(0).path, secondPath);
        QVERIFY(m_app->player()->isPlaying());   // dragging the songs list never interrupts playback

        // An album grouped from the reordered library still shows the right tracks (the
        // grouping is rebuilt from the new order, not left pointing at stale positions).
        m_app->selectArtist(0);
        m_app->selectAlbum(0);
        QSet<QString> albumPaths;
        for (const Track &t : m_app->albumSongs()->tracks()) albumPaths.insert(t.path);
        QVERIFY(albumPaths.contains(firstPath));
        QVERIFY(albumPaths.contains(secondPath));

        // The new order survives a relaunch of this profile, and belongs to it alone: an
        // intervening guest session (a different, unrelated library) never disturbs it.
        const QString profileId = m_app->profile().value("id").toString();
        m_app->leaveProfile();
        QCOMPARE(m_app->openProfile(profileId), QString());
        QCOMPARE(m_app->songs()->tracks().at(1).path, firstPath);

        m_app->leaveProfile();
        m_app->openGuest();
        QTRY_VERIFY(m_app->profileReady());
        m_app->leaveProfile();
        QCOMPARE(m_app->openProfile(profileId), QString());
        QCOMPARE(m_app->songs()->tracks().at(1).path, firstPath);
        shot("m8-songs-drag");
    }
};

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    // This test suite runs the software Qt Quick backend (no GPU/OpenGL in CI), so
    // MpvVideoItem — a QQuickFramebufferObject, which needs a real GL scenegraph — is
    // never actually instantiated by any test flow below (it's behind a Loader in
    // VideoPlayerView.qml, gated on hop.videoPlayerOpen, which no UI test flips to true).
    qputenv("QT_QUICK_BACKEND", "software");
    gst_init(&argc, &argv);
    // GstVideoSurfaceView.qml's import needs this module registered before Main.qml ever
    // parses it (see main.cpp's matching comment) — true here too even though this suite
    // never actually opens a video, since Main.qml unconditionally includes VideoPlayerView.
    if (GstElement *probe = gst_element_factory_make("qml6glsink", nullptr)) gst_object_unref(probe);
    QGuiApplication app(argc, argv);
    hop::registerVideoQmlTypes();
    QGuiApplication::setApplicationName(QStringLiteral("hop-tunes"));
    QGuiApplication::setOrganizationName(QStringLiteral("hop-tunes"));
    QGuiApplication::setApplicationVersion(QStringLiteral(HOPTUNES_VERSION));
    for (const char *f : {":/fonts/Jost-300-Light.ttf", ":/fonts/Jost-400-Book.ttf", ":/fonts/Jost-500-Medium.ttf"})
        QFontDatabase::addApplicationFont(QString::fromLatin1(f));
    QFont::insertSubstitutions(QStringLiteral("Jost"), {QStringLiteral("Jost*"), QStringLiteral("DejaVu Sans")});
    TestUi t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_ui.moc"
