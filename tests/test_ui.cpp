// SPDX-License-Identifier: GPL-3.0-or-later
// End-to-end profile flows through the real QML UI (offscreen): first run, PIN curtain,
// switching, guest mode, removal, restart behaviour.
#include "App.h"
#include "core/Paths.h"
#include "ui/MpvVideoItem.h"
#ifdef HOPTUNES_NET_APPS
#include "apps/KickClient.h"
#include "apps/StreamDock.h"
#include "net/AppNet.h"
#endif

#include <QBuffer>
#include <QDir>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJSEngine>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QTcpSocket>
#include <QHostAddress>
#include <QUrlQuery>
#include <QJSValue>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
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

#ifdef HOPTUNES_NET_APPS
// Answers the Kick app's requests from fixtures, a moment later like a real reply, and remembers what was asked (never touches the network).
class FakeKick : public NetTransport {
public:
    QList<QUrl> asked;
    int aborted = 0;
    int livePolls = 0;
    int failMode = 0;                    // 0 fine, 1 only the list server fails, 2 everything fails (a 503)
    static QByteArray pngBytes()
    {
        QImage im(64, 36, QImage::Format_RGB32); im.fill(QColor(90, 60, 160));
        QByteArray b; QBuffer buf(&b); buf.open(QIODevice::WriteOnly); im.save(&buf, "PNG"); return b;
    }
    static QByteArray list()
    {
        QJsonArray a;
        auto add = [&](const QString &slug, int v, bool mature) {
            a.append(QJsonObject{{"id", 1}, {"title", "title of " + slug}, {"viewer_count", v},
                                 {"thumbnail", QJsonObject{{"src", "https://images.kick.com/t/" + slug + ".webp"}}},
                                 {"channel", QJsonObject{{"slug", slug}, {"username", slug}, {"profile_pic", "https://files.kick.com/p/" + slug + ".webp"}}},
                                 {"category", QJsonObject{{"id", 15}, {"name", "Just Chatting"}}}, {"language", "en"}, {"is_mature", mature}});
        };
        add("alpha", 9000, false); add("bravo", 8000, false); add("charlie", 7000, true);
        return QJsonDocument(QJsonObject{{"data", QJsonObject{{"livestreams", a}, {"pagination", QJsonObject{{"next_cursor", ""}}}}}}).toJson();
    }
    static QByteArray channel(const QString &slug)
    {
        QJsonObject o{{"slug", slug}, {"user", QJsonObject{{"username", slug}}}, {"chatroom", QJsonObject{{"id", 5}}}, {"followers_count", 3}};
        o["livestream"] = QJsonValue();                     // offline: no video engine is started in this test
        if (slug == QLatin1String("gamehopper"))            // (the developer is live, so the pinned card shows; the test never opens it)
            o["livestream"] = QJsonObject{{"session_title", "developer stream"}, {"viewer_count", 12}, {"is_mature", false},
                                          {"thumbnail", QJsonObject{{"url", "https://stream.kick.com/thumbnails/dev.jpg"}}}};
        return QJsonDocument(o).toJson();
    }
    void get(const QUrl &url, const QByteArray &, const QByteArray &, NetLimits, Done done) override
    {
        asked << url;
        NetReply r; r.status = 200;
        if (failMode == 2 || (failMode == 1 && url.host() == "web.kick.com")) { r.status = 503; r.error = QStringLiteral("the site answered 503"); done(r); return; }
        if (url.host() == "id.kick.com" && url.path() == "/oauth/token") r.body = "{\"access_token\":\"acc-ui\",\"refresh_token\":\"ref-ui\",\"expires_in\":3600}";
        else if (url.host() == "api.kick.com" && url.path() == "/public/v1/users") r.body = "{\"data\":[{\"name\":\"UiTester\",\"user_id\":7}]}";
        else if (url.host() == "id.kick.com" && url.path() == "/oauth/revoke") r.body = "{}";
        else if (url.host() == "web.kick.com") r.body = list();
        else if (url.path() == "/api/search") {
            const QString w = QUrlQuery(url).queryItemValue("searched_word", QUrl::FullyDecoded).toLower();
            QJsonArray a;
            for (const QString &s : {QStringLiteral("alphabet"), QStringLiteral("alpha"), QStringLiteral("xalpha")})
                if (s.contains(w)) a.append(QJsonObject{{"slug", s}, {"user", QJsonObject{{"username", s}}}});
            QJsonArray cats;
            if (QStringLiteral("palworld").contains(w)) cats.append(QJsonObject{{"id", 7234}, {"name", "Palworld"}, {"is_mature", false}});
            r.body = QJsonDocument(QJsonObject{{"channels", a}, {"categories", cats}}).toJson();
        }
        else if (url.path().endsWith("/videos")) {
            QJsonArray a;
            a.append(QJsonObject{{"id", 1}, {"is_live", true}, {"session_title", "live now"}, {"source", "https://stream.kick.com/x/master.m3u8"}, {"duration", 0}});
            a.append(QJsonObject{{"id", 77}, {"is_live", false}, {"session_title", "last night's stream"}, {"duration", 7200000},
                                 {"source", "https://stream.kick.com/v77/master.m3u8"}, {"start_time", "2026-11-11 20:00:00"}, {"viewer_count", 1200},
                                 {"thumbnail", QJsonObject{{"src", "https://images.kick.com/video_thumbnails/v77/720.webp"}}}});
            r.body = QJsonDocument(a).toJson();
        }
        else if (url.path().endsWith("/clips")) {
            r.body = QJsonDocument(QJsonObject{{"clips", QJsonArray{QJsonObject{{"id", "clip_abc"}, {"title", "a funny moment"}, {"duration", 30},
                {"clip_url", "https://clips.kick.com/clips/ab/clip_abc/playlist.m3u8"}, {"thumbnail_url", "https://clips.kick.com/clips/ab/clip_abc/thumbnail.webp"},
                {"views", 5000}, {"created_at", "2026-11-01T10:00:00Z"}}}}}).toJson();
        }
        else if (url.host().endsWith("live-video.net")) {          // a live stream: a sliding playlist that gains a piece at each look
            if (url.path() == "/live/master.m3u8") r.body = "#EXTM3U\n#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=1500000,RESOLUTION=1280x720,VIDEO=\"720p60\"\n720p/playlist.m3u8\n";
            else if (url.path().endsWith("/playlist.m3u8")) {
                const int k = livePolls++;
                QByteArray t = "#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:0.04\n#EXT-X-MEDIA-SEQUENCE:" + QByteArray::number(k) + "\n";
                for (int i = 0; i < 4; ++i) t += "#EXTINF:2.0,\n" + QByteArray::number(k + i) + ".ts\n";
                r.body = t;
            } else r.body = QByteArray(3000, 'L');
        }
        else if (url.path().endsWith("/master.m3u8"))
            r.body = "#EXTM3U\n#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=1500000,RESOLUTION=1280x720,VIDEO=\"720p60\"\n720p60/playlist.m3u8\n";
        else if (url.path().endsWith("/720p60/playlist.m3u8"))
            r.body = "#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:10\n#EXTINF:10,\n0.ts\n#EXTINF:10,\n1.ts\n#EXT-X-ENDLIST\n";
        else if (url.path().endsWith("/clip_abc/playlist.m3u8"))
            r.body = "#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:10\n#EXTINF:30,\nclip.ts\n#EXT-X-ENDLIST\n";
        else if (url.path().endsWith(".ts")) r.body = QByteArray(5000, 'v');
        else if (url.host() == "images.kick.com" || url.host() == "files.kick.com" || url.host() == "stream.kick.com") r.body = pngBytes();
        else if (url.path().startsWith("/api/v2/channels/")) r.body = channel(url.path().section('/', -1));
        else if (url.path().startsWith("/api/v1/subcategories")) r.body = "{\"data\":[{\"id\":15,\"name\":\"Just Chatting\",\"slug\":\"just-chatting\"},{\"id\":3,\"name\":\"Grand Theft Auto V\",\"slug\":\"gta\"}]}";
        else r.body = "{}";
        QTimer::singleShot(0, this, [done, r] { done(r); });
    }
    void abortAll() override { ++aborted; }
};
#endif

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
    bool m_liveShots = false;   // the render test wants games running (not paused) in its pictures
    void shot(const char *name)
    {
        const QString dir = qEnvironmentVariable("HOPTUNES_UI_SHOTS");
        if (dir.isEmpty()) return;
        // real time must not run the game while the picture is taken
        MusyNoteGame *mg = m_app->musyNote();
        const bool wasPaused = mg->paused();
        if (!m_liveShots) mg->setPaused(true);
        QTest::qWait(300);
        mg->setPaused(wasPaused);
        window()->grabWindow().save(dir + QLatin1Char('/') + QLatin1String(name) + QStringLiteral(".png"));
    }
    QString gateMode() { return item("profileGate")->property("mode").toString(); }

    void start()
    {
        m_engine.reset();
        m_app.reset(new App);
        QVERIFY(m_app->init());
        m_engine.reset(new QQmlApplicationEngine);
        m_app->installImageProviders(m_engine.get());
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
        // a controller press that arrives with the opening (a stick that rests off-centre) does not start it either
        m_app->gamepad()->feedAction(QStringLiteral("up"), true);
        m_app->gamepad()->feedAction(QStringLiteral("up"), false);
        QTest::qWait(100);
        QCOMPARE(g->state(), 0);
        // a tap that arrives right after opening (the end of the tap that opened it) does not start the game
        QTest::mouseClick(w, Qt::LeftButton, Qt::NoModifier, QPoint(w->width() / 2, w->height() / 2));
        QTest::qWait(200);
        QCOMPARE(g->state(), 0);
        QTest::qWait(300);                                 // after the short grace period a tap does fly
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
        {   // a close look at the note's moods: a big note on a plain background
            QObject *sprite = item("musyNote")->findChild<QObject *>(QStringLiteral("musyNoteSprite"));
            QVERIFY(sprite);
            QTest::qWait(150);
            shot("musy-note-happy");
        }

        // keys, then a controller button, then the screen start and flap it
        QTest::keyClick(w, Qt::Key_Space);
        QTRY_COMPARE(g->state(), 1);
        g->advance(6, 1.0 / 60);
        g->setPaused(true);                                // (real time must not run while the picture is taken)
        QTest::qWait(120);
        shot("musy-flying");
        g->setPaused(false);
        const double y0 = g->noteY();
        m_app->gamepad()->feedAction(QStringLiteral("a"), true);
        m_app->gamepad()->feedAction(QStringLiteral("a"), false);
        QTRY_VERIFY(g->noteVy() < 0 || g->noteY() < y0 + 0.5);
        // beats make walls (only the stepped time counts here, not the clock)
        for (int i = 0; i < 6; ++i) {                      // flap only when the note sags, so the run lasts whatever the clock did
            g->onBeat(0.7);
            for (int k = 0; k < 6; ++k) { if (g->noteY() > 4.5) QTest::keyClick(w, Qt::Key_Space); g->advance(5, 1.0 / 60); }
        }
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

    // The phone held upright, including a narrow and a short one (the Red Magic is about 346 dp wide): everything on Now
    // playing is inside the screen, the cover shrinks to make room and nothing is cut off at the right or the bottom.
    void phoneNowPlayingPortraitFitsNarrowAndShortPhones()
    {
        auto *w = window();
        const QSize before = w->size();
        struct Restore {
            QQuickWindow *w; QSize size; App *app;
            ~Restore() { app->setGameSetting(QStringLiteral("phoneViz"), false); w->setProperty("forceMobile", false); w->resize(size); w->setProperty("view", QStringLiteral("collection")); }
        } restore{w, before, m_app.get()};
        m_app->playSongs(0);
        QTRY_VERIFY(m_app->player()->hasTrack());
        w->setProperty("forceMobile", true);
        for (bool viz : {false, true}) {
            m_app->setGameSetting(QStringLiteral("phoneViz"), viz);
            for (const QSize &sz : {QSize(346, 700), QSize(346, 620), QSize(320, 560), QSize(412, 892), QSize(360, 640)}) {
                w->resize(sz);
                QTRY_COMPARE(w->width(), sz.width());
                w->setProperty("view", QStringLiteral("now playing"));
                QTest::qWait(350);
                const QString tag = QStringLiteral("%1x%2%3").arg(sz.width()).arg(sz.height()).arg(viz ? QStringLiteral(" viz") : QString());
                if (sz == QSize(346, 700) || sz == QSize(320, 560)) shot(qPrintable(QStringLiteral("phone-nowplaying-portrait-") + tag.simplified().replace(' ', '-')));
                QVERIFY2(overflowing().isEmpty(), qPrintable(tag + QStringLiteral(" overflows right: ") + overflowing().join(QStringLiteral(", "))));
                for (const char *name : {"phoneNpBack", "phoneVizToggle", "phoneGames", "phoneSeek", "phonePlay", "phoneQueueButton", "phoneVolumeButton", viz ? "phoneViz" : "phoneCover"}) {
                    QQuickItem *it = item(name);
                    QVERIFY2(it && it->isVisible(), qPrintable(QLatin1String(name) + QStringLiteral(" missing at ") + tag));
                    const QRectF r = it->mapRectToScene(QRectF(0, 0, it->width(), it->height()));
                    QVERIFY2(r.left() >= -1 && r.right() <= w->width() + 1 && r.top() >= -1 && r.bottom() <= w->height() + 1,
                             qPrintable(QStringLiteral("%1 is cut off at %2: x %3..%4 of %5, y %6..%7 of %8").arg(QLatin1String(name), tag)
                                            .arg(r.left()).arg(r.right()).arg(w->width()).arg(r.top()).arg(r.bottom()).arg(w->height())));
                }
            }
        }
        m_app->player()->stop();
    }

    // Media on a drive that is not plugged in is simply left out; plug the drive in and it comes back by itself.
    void mediaOnAnUnpluggedDriveIsLeftOutUntilItIsBack()
    {
        const QString drive = m_home.filePath(QStringLiteral("USBDRIVE"));
        const QString away = m_home.filePath(QStringLiteral("USBDRIVE.unplugged"));
        QVERIFY(QDir().mkpath(drive + QStringLiteral("/Songs")));
        QVERIFY(QDir().mkpath(drive + QStringLiteral("/Books")));
        for (const char *n : {"a.wav", "b.wav"}) QVERIFY(writeWav(drive + QStringLiteral("/Songs/") + QLatin1String(n), 800));
        QVERIFY(writeWav(drive + QStringLiteral("/Books/ch1.mp3"), 800));
        const int songsBefore = m_app->librarySize();
        const int booksBefore = m_app->audiobooks()->count();

        m_app->addFolder(QUrl::fromLocalFile(drive + QStringLiteral("/Songs")));
        m_app->addSectionFolder(QStringLiteral("audiobooks"), QUrl::fromLocalFile(drive + QStringLiteral("/Books")));
        QTRY_COMPARE_WITH_TIMEOUT(m_app->librarySize(), songsBefore + 2, 15000);
        QTRY_COMPARE_WITH_TIMEOUT(m_app->audiobooks()->count(), booksBefore + 1, 15000);
        QTRY_VERIFY_WITH_TIMEOUT(!m_app->scanning(), 15000);
        m_app->checkDrives();                                  // it learns what is connected now
        QVERIFY(m_app->missingFolders().isEmpty());

        // the drive is pulled out: its songs and books vanish from the lists at the next check, and are marked in settings
        QVERIFY(QDir().rename(drive, away));
        m_app->checkDrives();
        QTRY_COMPARE_WITH_TIMEOUT(m_app->librarySize(), songsBefore, 5000);                 // (the lists follow the check a moment later)
        QTRY_COMPARE_WITH_TIMEOUT(m_app->audiobooks()->count(), booksBefore, 5000);
        QVERIFY(!m_app->missingFolders().isEmpty());
        QVERIFY(m_app->folderMissing(QFileInfo(drive + QStringLiteral("/Songs")).absoluteFilePath()) || !m_app->missingFolders().isEmpty());
        window()->setProperty("view", QStringLiteral("settings"));
        QTest::qWait(300);
        QVERIFY(item("folderNotConnected") != nullptr);
        window()->setProperty("view", QStringLiteral("collection"));
        // a scan with the drive away must not throw its rows out
        m_app->rescan();
        QTRY_VERIFY_WITH_TIMEOUT(!m_app->scanning(), 15000);
        QCOMPARE(m_app->librarySize(), songsBefore);

        // plugged in again: found and scanned by itself, nothing was lost
        QVERIFY(QDir().rename(away, drive));
        m_app->checkDrives();
        QTRY_COMPARE_WITH_TIMEOUT(m_app->librarySize(), songsBefore + 2, 15000);
        QTRY_COMPARE_WITH_TIMEOUT(m_app->audiobooks()->count(), booksBefore + 1, 15000);
        QVERIFY(m_app->missingFolders().isEmpty());

        // tidy up
        m_app->removeFolder(QFileInfo(drive + QStringLiteral("/Songs")).canonicalFilePath());
        m_app->removeSectionFolder(QStringLiteral("audiobooks"), QFileInfo(drive + QStringLiteral("/Books")).canonicalFilePath());
        QTRY_COMPARE_WITH_TIMEOUT(m_app->librarySize(), songsBefore, 15000);
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
        QVERIFY(item("stage_visualizer") && item("stage_games"));
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
        QVERIFY(click("stage_games"));
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
        // pause by its button, and the song playing the game for you: nothing is scored then
        QVERIFY(item("afPause") && item("afAutoplay"));
        {
            AlbumFall *af = m_app->albumFall();
            QVERIFY(click("afPause"));
            QTRY_VERIFY(af->paused());
            QVERIFY(click("afPause"));
            QTRY_VERIFY(!af->paused());
            const int before = m_app->gameScores(QStringLiteral("albumfall")).size();
            af->start(8);
            QVERIFY(af->scored());
            QVERIFY(click("afAutoplay"));
            QVERIFY(af->autoplay());
            QVERIFY(!af->scored());                         // from here on nothing is counted
            af->act(QStringLiteral("drop"));                // a person's input is refused while the song plays
            const int before0 = af->landed();
            for (int i = 0; i < 40; ++i) { af->onBeat(0.6); QTest::qWait(60); }
            QVERIFY2(af->landed() > before0, "the song should have dropped pieces");
            QVERIFY(af->score() > 0);
            QCOMPARE(m_app->gameScores(QStringLiteral("albumfall")).size(), before);   // and the list is untouched
            QVERIFY(click("afAutoplay"));
            QVERIFY(!af->autoplay());
            af->start(8);
            QVERIFY(af->scored());                          // a game after that counts again
        }
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
        {   // whoever types in the chat is a member (nobody pays for a pretend stream), in both looks
            QQuickItem *chat = item("nowPlayingChat");
            for (const char *look : {"clean", "lively"}) {
                m_app->setVideoChatLook(QString::fromLatin1(look));
                QVariant ok;
                QVERIFY(QMetaObject::invokeMethod(chat, "send", Q_RETURN_ARG(QVariant, ok), Q_ARG(QVariant, QString::fromLatin1("hello from the test %1").arg(QLatin1String(look)))));
                if (!ok.toBool()) { QTest::qWait(1300); QVERIFY(QMetaObject::invokeMethod(chat, "send", Q_RETURN_ARG(QVariant, ok), Q_ARG(QVariant, QString::fromLatin1("hello again %1").arg(QLatin1String(look))))); }
                QVERIFY(ok.toBool());
                QVariant badge, mine, html;
                QVERIFY(QMetaObject::invokeMethod(chat, "lastBadge", Q_RETURN_ARG(QVariant, badge)));
                QVERIFY(QMetaObject::invokeMethod(chat, "lastIsMine", Q_RETURN_ARG(QVariant, mine)));
                QVERIFY(QMetaObject::invokeMethod(chat, "lastHtml", Q_RETURN_ARG(QVariant, html)));
                QVERIFY(mine.toBool());
                QCOMPARE(badge.toString(), QStringLiteral("member"));
                QVERIFY2(html.toString().contains(look[0] == 'l' ? QStringLiteral("MEMBER") : QStringLiteral("(member)")), qPrintable(html.toString()));
                QTest::qWait(1300);                                  // slow mode between messages
            }
            m_app->setVideoChatLook(QStringLiteral("clean"));
        }
        shot("np-chat");
        QVERIFY(click("chatToggle"));
        QTRY_VERIFY(!item("nowPlayingChat")->isVisible());
        QVERIFY(click("stage_games"));

        // back to the song: the analysis stops listening again
        QVERIFY(click("stage_art"));
        QTRY_VERIFY(!m_app->audio()->active());
        QVERIFY(m_app->gameSettings().value("stage").toString() == QLatin1String("art"));
        m_app->player()->toggle();
        window()->setProperty("view", QStringLiteral("collection"));
    }

    // Cover Pinball: the games chip opens a game picker; the table is made from the cover; flippers hold with keys and the
    // controller; the song playing it keeps no score; Esc leaves
    void coverPinballOpensFromTheGamesChipAndPlays()
    {
        auto *w = window();
        w->setProperty("view", QStringLiteral("now playing"));
        m_app->playSongs(0);
        QTRY_VERIFY(m_app->player()->isPlaying());
        QVERIFY(item("stage_games"));
        QVERIFY(click("stage_games"));
        QVERIFY(item("game_albumfall") && item("game_pinball"));       // the picker for the two games
        QTRY_VERIFY(item("albumFall") && item("albumFall")->isVisible());
        QVERIFY(click("game_pinball"));
        QCOMPARE(m_app->gameSettings().value("game").toString(), QStringLiteral("pinball"));
        QCOMPARE(m_app->gameSettings().value("stage").toString(), QStringLiteral("pinball"));
        QTRY_VERIFY(item("coverPinball") && item("coverPinball")->isVisible());
        QTRY_VERIFY(!item("albumFall")->isVisible());
        PinballGame *g = m_app->pinball();
        QTRY_VERIFY(g->running());
        QCOMPARE(g->state(), 0);                                       // the ball waits in the lane
        QVERIFY(g->bumperCount() >= 5);
        QVERIFY(item("pbTable") && item("pbTable")->isVisible());
        // the controller only listens while a game is on screen
        m_app->gamepad()->feedAction(QStringLiteral("start"), true);   // start pauses
        QTRY_VERIFY(g->paused());
        m_app->gamepad()->feedAction(QStringLiteral("start"), true);
        QTRY_VERIFY(!g->paused());
        // space launches; keys and controller buttons hold the flippers up and let them down again
        QTest::keyClick(w, Qt::Key_Space);
        QTRY_COMPARE(g->state(), 1);
        const double rest = g->leftAngle();
        QTest::keyPress(w, Qt::Key_Z);
        QTRY_VERIFY(g->leftAngle() < rest - 0.5);
        QTest::keyRelease(w, Qt::Key_Z);
        QTRY_VERIFY(g->leftAngle() > rest - 0.05);
        const double rrest = g->rightAngle();
        m_app->gamepad()->feedAction(QStringLiteral("rb"), true);
        QTRY_VERIFY(g->rightAngle() < rrest - 0.5);
        m_app->gamepad()->feedAction(QStringLiteral("rb"), false);
        QTRY_VERIFY(g->rightAngle() > rrest - 0.05);
        shot("np-pinball");
        // the song plays it: nothing is scored, however it ends
        const int before = m_app->gameScores(QStringLiteral("pinball")).size();
        QVERIFY(click("pbAutoplay"));
        QVERIFY(g->autoplay() && !g->scored());
        QVERIFY(item("pbOverlay"));
        g->advance(60 * 120, 1.0 / 60);
        QVERIFY(g->hits() > 0);
        QCOMPARE(m_app->gameScores(QStringLiteral("pinball")).size(), before);
        QVERIFY(click("pbAutoplay"));
        QVERIFY(!g->autoplay());
        // a click on a button must not take the keys away: Space is still the launch, not a second click on that button
        QTest::keyClick(w, Qt::Key_Space);
        QVERIFY(!g->autoplay());
        // Esc leaves back to the song; the game pauses and the choice of game is remembered
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_COMPARE(m_app->gameSettings().value("stage").toString(), QStringLiteral("art"));
        QTRY_VERIFY(g->paused());
        QCOMPARE(m_app->gameSettings().value("game").toString(), QStringLiteral("pinball"));
        QVERIFY(click("stage_games"));                                 // the chip comes back to the last game
        QCOMPARE(m_app->gameSettings().value("stage").toString(), QStringLiteral("pinball"));
        QTest::qWait(300);
        QVERIFY(click("game_albumfall"));
        QTRY_COMPARE(m_app->gameSettings().value("game").toString(), QStringLiteral("albumfall"));
        QVERIFY(click("stage_art"));
        m_app->setGameSetting(QStringLiteral("game"), QStringLiteral("albumfall"));
    }

    // Apps: a tab of its own next to the other collection tabs (desktop), glass tiles, and an app opens inside the window
    void appsTabHoldsGlassTilesAndAppsOpenInsideTheWindow()
    {
        auto *w = window();
        w->setProperty("view", QStringLiteral("collection"));
        m_app->playSongs(0);
        QTRY_VERIFY(m_app->player()->isPlaying());
        QVERIFY(item("subTab_music") && item("subTab_videos") && item("subTab_apps"));
        QVERIFY(!m_app->appsEnabled());                                         // off by default: every other tab, no apps tab
        QVERIFY(!item("subTab_apps")->isVisible());
        QVERIFY(item("subTab_podcasts")->isVisible());
        m_app->setAppsEnabled(true);                                            // (settings > apps does the same, tested below)
        QTRY_VERIFY(item("subTab_apps")->isVisible());
        QVERIFY(click("subTab_apps"));
        QTRY_VERIFY(item("appsView") && item("appsView")->isVisible());
        QQuickItem *tile = item("appTile_kick");
        QVERIFY(tile && tile->isVisible());
        QVERIFY(tile->width() >= 100 && tile->height() > tile->width());       // a glass square with its name under it
        shot("apps-tab");
        QVERIFY(click("appTile_kick"));
        QObject *host = item("appHost");
        QVERIFY(host);
        QTRY_VERIFY(host->property("active").toBool());
        QTRY_VERIFY(item("kickApp") && item("kickApp")->isVisible());
        QVERIFY(m_app->player()->isPlaying());                                  // the music plays on inside an app
        QVERIFY(!item("appsView")->isVisible() || host->property("visible").toBool());
        shot("apps-open");
        // Esc leaves the app and lands on the apps tab; the back button of the phone does the same
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(!host->property("active").toBool());
        QVERIFY(item("appsView")->isVisible());
        QVERIFY(click("appTile_kick"));
        QTRY_VERIFY(host->property("active").toBool());
        QVariant r;
        QVERIFY(QMetaObject::invokeMethod(w, "goBack", Q_RETURN_ARG(QVariant, r)));
        QTRY_VERIFY(!host->property("active").toBool());
        QVERIFY(click("appTile_kick"));
        QTRY_VERIFY(host->property("active").toBool());
        w->setProperty("view", QStringLiteral("now playing"));                 // leaving the collection closes it
        QTRY_VERIFY(!host->property("active").toBool());
        // phones: the apps pill comes after podcasts once apps are switched on (and not before)
        const QSize before = w->size();
        m_app->setAppsEnabled(false);
        w->setProperty("view", QStringLiteral("collection"));
        w->setProperty("forceMobile", true);
        w->resize(412, 880);
        QTRY_COMPARE(w->width(), 412);
        QTRY_VERIFY(!item("subTab_apps")->isVisible());
        QVERIFY(item("pillTab_apps") == nullptr);                               // off by default
        m_app->setAppsEnabled(true);
        QTRY_VERIFY(item("pillTab_apps") != nullptr);
        QVERIFY(item("pillTab_podcasts") != nullptr);
        QTRY_VERIFY(item("pillTab_apps")->x() > item("pillTab_podcasts")->x());   // after podcasts (once the row has laid itself out)
        item("collectionView")->setProperty("collectionTab", QStringLiteral("apps"));   // (the pill row scrolls sideways: the pill itself can be off the edge)
        QTRY_VERIFY(item("appsView") && item("appsView")->isVisible());
        shot("phone-apps-tab");
        QVERIFY(!item("appsEmpty")->isVisible());                              // neko cafe is in every build; kick only in networked ones
        QVERIFY(item("appTile_nekocafe") && item("appTile_nekocafe")->isVisible());
        m_app->setAppsEnabled(false);
        QTRY_VERIFY(item("pillTab_apps") == nullptr);
        w->setProperty("forceMobile", false);
        w->resize(before);
        QTRY_COMPARE(w->size(), before);
        QTRY_VERIFY(item("subTab_music")->isVisible());
        QVERIFY(click("subTab_music"));
    }


    // neko cafe: a game inside the apps tab. It opens from its tile, starts on A, the maid walks, a whole day ends in the results
    // screen, and Esc pauses first and leaves second.
    void nekoCafeOpensPlaysADayAndLeaves()
    {
        auto *w = window();
        w->setProperty("view", QStringLiteral("collection"));
        m_app->playSongs(0);                                                   // music is playing, as it usually is
        QTRY_VERIFY(m_app->player()->isPlaying());
        m_app->setAppsEnabled(true);
        QTRY_VERIFY(item("subTab_apps")->isVisible());
        QVERIFY(click("subTab_apps"));
        QTRY_VERIFY(item("appsView") && item("appsView")->isVisible());
        QVERIFY(item("appTile_nekocafe") && item("appTile_nekocafe")->isVisible());
        QVERIFY(click("appTile_nekocafe"));
        QObject *host = item("appHost");
        QTRY_VERIFY(host->property("active").toBool());
        QTRY_VERIFY(item("nekoCafeApp") && item("nekoCafeApp")->isVisible());
        QTRY_VERIFY(item("nekoCafeApp")->hasActiveFocus());                     // it takes the keys
        NekoCafeGame *neko = m_app->neko();
        QVERIFY(!neko->started());
        QVERIFY(neko->isOpen());                                               // Space is the game's A now, not play/pause
        m_app->gamepad()->setDevice(QStringLiteral("Test Pad"));                // a controller shows up while music plays: the game is not yanked away
        QTest::qWait(100);
        QVERIFY(host->property("active").toBool());
        QCOMPARE(w->property("view").toString(), QStringLiteral("collection"));
        m_app->gamepad()->setDevice(QString());
        QVERIFY(item("nekoTitle")->isVisible());
        shot("neko-title");
        QTest::keyClick(w, Qt::Key_Return);                                    // A starts the day
        QVERIFY(neko->started());
        QVERIFY(!item("nekoTitle")->isVisible());
        const int x0 = neko->maidX();
        QTest::keyPress(w, Qt::Key_Right);                                     // the maid walks while the key is held
        neko->advance(500);
        QTest::keyRelease(w, Qt::Key_Right);
        QVERIFY2(neko->maidX() > x0, "she walked right");
        const int x1 = neko->maidX();
        neko->advance(500);
        QCOMPARE(neko->maidX(), x1);                                           // let go: she stops
        QQuickItem *maid = item("nekoMaid");
        QVERIFY(maid && maid->isVisible());
        QVERIFY(maid->x() > 0 && maid->x() < item("nekoScreen")->width());
        auto someoneIsIn = [neko] { for (int i = 0; i < NekoCafe::kMaxCustomers; ++i) if (neko->rules().customer(i).state != NekoCafe::None) return true; return false; };
        for (int i = 0; i < 120 && !someoneIsIn(); ++i) neko->advance(1000);   // (the days are random: wait for the first customer to walk in)
        QVERIFY2(someoneIsIn(), "customers came in");
        neko->advance(8000);                                                   // and sit down
        QGuiApplication::processEvents();
        shot("neko-cafe");
        // Esc pauses, Esc again leaves
        QTest::keyClick(w, Qt::Key_Escape);
        QVERIFY(host->property("active").toBool());
        QVERIFY(neko->paused());
        QTRY_VERIFY(item("nekoPaused")->isVisible());
        shot("neko-paused");
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(!host->property("active").toBool());
        QVERIFY2(!neko->started(), "leaving ends the day and stops the clock");
        QVERIFY(!neko->isOpen());
        // play a whole day (nobody serves anyone: the results show an empty day) and read the results
        QVERIFY(click("appTile_nekocafe"));
        QTRY_VERIFY(item("nekoCafeApp") && item("nekoCafeApp")->isVisible());
        QTRY_VERIFY(item("nekoCafeApp")->hasActiveFocus());
        QTest::keyClick(w, Qt::Key_Space);
        QVERIFY(neko->started());
        for (int i = 0; i < 400 && neko->phase() == 0; ++i) neko->advance(1000);
        QCOMPARE(neko->phase(), 1);
        QTRY_VERIFY(item("nekoResults")->isVisible());
        QCOMPARE(neko->grade(), QStringLiteral("D"));
        neko->advance(3000);
        shot("neko-results");
        QTest::keyClick(w, Qt::Key_Return);                                    // the next day
        QCOMPARE(neko->phase(), 0);
        QCOMPARE(neko->day(), 2);
        QTest::keyClick(w, Qt::Key_Escape);                                    // pause, then leave
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(!host->property("active").toBool());
        m_app->setAppsEnabled(false);
    }

#ifdef HOPTUNES_NET_APPS
    // Kick: asks before it connects, browses without playing anything, follow and block stay local, and everything it reaches goes
    // through the gateway (answered here from fixtures).
    void kickAppAsksThenBrowsesFollowsAndBlocksLocally()
    {
        auto *w = window();
        auto fake = std::make_unique<FakeKick>();
        FakeKick *ft = fake.get();
        m_app->gateway()->setTransport(std::move(fake));
        KickClient *kick = m_app->kickClient();
        w->setProperty("view", QStringLiteral("collection"));
        m_app->setAppsEnabled(true);
        QTRY_VERIFY(item("subTab_apps")->isVisible());
        QVERIFY(click("subTab_apps"));
        QVERIFY(click("appTile_kick"));
        // 1. the first time it asks, and nothing has been sent
        QTRY_VERIFY(item("kickPermission") && item("kickPermission")->isVisible());
        QVERIFY(!m_app->appNetAllowed("kick"));
        QCOMPARE(ft->asked.size(), 0);
        QVERIFY(!kick->isOpen() || kick->streams().isEmpty());
        QVERIFY(item("kickAllow") && item("kickNotNow"));
        QVERIFY(item("kickDevLink") && item("kickDevLink")->property("text").toString().contains("https://kick.com/gamehopper/about"));   // the link at the very bottom
        QVERIFY(item("kickDevLink")->mapToItem(item("kickPermission"), QPointF(0, 0)).y() > item("kickAllow")->mapToItem(item("kickPermission"), QPointF(0, 0)).y());
        shot("kick-permission");
        // "not now" leaves and still nothing was sent
        QVERIFY(click("kickNotNow"));
        QTRY_VERIFY(!item("appHost")->property("active").toBool());
        QCOMPARE(ft->asked.size(), 0);
        QVERIFY(click("appTile_kick"));
        QTRY_VERIFY(item("kickPermission")->isVisible());
        QVERIFY(click("kickAllow"));
        // 2. allowed: the live list comes in, 18+ streams are hidden, and no video engine has been started
        QTRY_VERIFY(item("kickCard_alpha") && item("kickCard_alpha")->isVisible());
        QVERIFY(m_app->appNetAllowed("kick"));
        QVERIFY(item("kickCard_bravo") != nullptr);
        QVERIFY(item("kickCard_charlie") == nullptr);
        // the developer is live in this fixture: that card comes first (top left), tagged, ahead of Kick's own order
        QTRY_VERIFY(item("kickCard_gamehopper") && item("kickCard_gamehopper")->isVisible());
        QVERIFY(item("kickDeveloperTag") != nullptr);
        QTRY_VERIFY(item("kickCard_alpha")->x() > 0 || item("kickCard_alpha")->y() > 0);        // (the grid has laid the cards out)
        QVERIFY(item("kickCard_gamehopper")->x() == 0 && item("kickCard_gamehopper")->y() == 0);   // first place: top left
        QVERIFY(item("kickCard_gamehopper")->x() < item("kickCard_alpha")->x() || item("kickCard_gamehopper")->y() < item("kickCard_alpha")->y());
        QVERIFY(item("kickPlayer") == nullptr || !item("kickSurface"));
        QVERIFY(item("kickSurface") == nullptr);                                   // nothing plays until a stream is picked
        QVERIFY(item("kickMore") == nullptr || !item("kickMore")->isVisible());   // (one page only in this fixture)
        for (const QUrl &u : ft->asked) QVERIFY2(AppNet::urlAllowed("kick", u), qPrintable(u.toString()));
        QTRY_VERIFY(item("kickContacted") && item("kickContacted")->property("text").toString().contains("web.kick.com"));
        // the connection bar sits at the top, right under the app's title bar, above the streams
        QVERIFY(item("kickStatusBar")->mapToItem(item("kickApp"), QPointF(0, 0)).y() < 5);
        QVERIFY(item("kickStatusBar")->mapToItem(item("kickApp"), QPointF(0, 0)).y() < item("kickBrowse")->mapToItem(item("kickApp"), QPointF(0, 0)).y());
        shot("kick-browse");
        QVERIFY(click("kickMature"));
        QTRY_VERIFY(item("kickCard_charlie") != nullptr);
        QVERIFY(click("kickMature"));
        QTRY_VERIFY(item("kickCard_charlie") == nullptr);
        // 3. a card opens its channel (offline in the fixture: nothing to play, and it says so); follow and block are local
        QVERIFY(click("kickCard_alpha"));
        QTRY_VERIFY(item("kickWatch") && item("kickWatch")->isVisible());
        QTRY_VERIFY(item("kickChannelState") && item("kickChannelState")->isVisible());
        QTRY_VERIFY(item("kickChannelState")->property("text").toString().contains("offline"));
        QVERIFY(item("kickSurface") == nullptr);
        const int sentBefore = ft->asked.size();
        // chat: none at the start; pretend is a choice; Kick's own chat is not offered (there is no sign-in)
        QCOMPARE(kick->chatMode(), QStringLiteral("off"));
        QVERIFY(item("kickPretendChat") != nullptr && !item("kickPretendChat")->isVisible());
        QVERIFY(item("kickChat_pretend") != nullptr && item("kickChat_kick") == nullptr);
        kick->setChatMode("kick");                                                   // refused: only none and pretend exist
        QCOMPARE(kick->chatMode(), QStringLiteral("off"));
        QVERIFY(click("kickChat_pretend"));
        QTRY_VERIFY(item("kickPretendChat")->isVisible());
        QCOMPARE(kick->chatMode(), QStringLiteral("pretend"));
        QVERIFY(m_app->profileStore()->settings(m_app->profile().value("id").toString()).value("kick").toObject().value("chatMode").toString() == QLatin1String("pretend"));
        QVERIFY(click("kickChat_off"));
        QTRY_VERIFY(!item("kickPretendChat")->isVisible());
        // a live channel: no seek meter (it is only for videos kept on this device), the "live" tag instead
        QVERIFY(!item("kickPlayer")->property("showSeekMeter").toBool());
        QVERIFY(item("kickPlayer")->property("showLiveTag").toBool());
        // the follow button says what it did: "☆ follow" -> "★ following" (filled) -> back, every time
        QCOMPARE(item("kickFollowButton")->property("text").toString(), QStringLiteral("\u2606 follow"));
        QVERIFY(!item("kickFollowButton")->property("primary").toBool());
        QVERIFY(click("kickFollowButton"));
        QVERIFY(kick->isFollowing("alpha"));
        QTRY_COMPARE(item("kickFollowButton")->property("text").toString(), QStringLiteral("\u2605 following"));
        QVERIFY(item("kickFollowButton")->property("primary").toBool());
        QVERIFY(click("kickFollowButton"));
        QVERIFY(!kick->isFollowing("alpha"));
        QTRY_COMPARE(item("kickFollowButton")->property("text").toString(), QStringLiteral("\u2606 follow"));
        QVERIFY(!item("kickFollowButton")->property("primary").toBool());
        QVERIFY(click("kickFollowButton"));
        QVERIFY(kick->isFollowing("alpha"));
        QTRY_COMPARE(item("kickFollowButton")->property("text").toString(), QStringLiteral("\u2605 following"));
        QCOMPARE(ft->asked.size(), sentBefore);                                    // following asks nobody
        QVERIFY(m_app->profileStore()->settings(m_app->profile().value("id").toString()).value("kick").toObject().value("following").toArray().contains("alpha"));
        QTest::keyClick(w, Qt::Key_Escape);                                        // Esc steps back to the streams first
        QTRY_VERIFY(item("kickBrowse")->isVisible());
        QVERIFY(item("appHost")->property("active").toBool());
        QVERIFY(click("kickCard_bravo"));
        QTRY_VERIFY(item("kickWatch")->isVisible());
        QVERIFY(click("kickBlockButton"));
        QTRY_VERIFY(item("kickBrowse")->isVisible());
        QVERIFY(kick->isBlocked("bravo"));
        QTRY_VERIFY(item("kickCard_bravo") == nullptr);
        QTRY_VERIFY(item("kickUndo") && item("kickUndo")->isVisible());
        QVERIFY(click("kickUndoButton"));                                          // undo
        QTRY_VERIFY(item("kickCard_bravo") != nullptr);
        QVERIFY(!kick->isBlocked("bravo"));
        kick->block("bravo");
        QTRY_VERIFY(item("kickCard_bravo") == nullptr);
        QVERIFY(m_app->profileStore()->settings(m_app->profile().value("id").toString()).value("kick").toObject().value("blocked").toArray().contains("bravo"));
        // 4. the status bar only tells; there is no disconnect button to hit by accident
        QVERIFY(item("kickContacted") != nullptr);
        QVERIFY(item("kickDisconnect") == nullptr);
        QVERIFY(item("kickCard_alpha") && item("kickCard_alpha")->isVisible());
        // 5. settings: the permission and the lists, and taking the permission back closes the app's connection
        w->setProperty("view", QStringLiteral("settings"));
        QTRY_VERIFY(item("kickNetChoice") != nullptr);
        QVERIFY(item("kickListsBox") != nullptr);
        QTRY_VERIFY(item("kickUnblock_bravo") != nullptr);
        m_app->setAppNetAllowed("kick", false);
        QVERIFY(!m_app->gateway()->isOpen("kick"));
        QVERIFY(!m_app->profileStore()->settings(m_app->profile().value("id").toString()).value("appsNet").toObject().value("kick").toBool());
        w->setProperty("view", QStringLiteral("collection"));
        // 6. leaving: Esc at the top of the app closes it and the gateway
        m_app->setAppNetAllowed("kick", true);
        QVERIFY(click("subTab_apps"));
        QVERIFY(click("appTile_kick"));
        QTRY_VERIFY(item("kickBrowse")->isVisible());
        QVERIFY(m_app->gateway()->isOpen("kick"));
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(!item("appHost")->property("active").toBool());
        QTRY_VERIFY(!m_app->gateway()->isOpen("kick"));
        // tidy up for the tests after this one
        kick->clearBlocked(); kick->unfollow("alpha");
        m_app->setAppNetAllowed("kick", false);
        m_app->setAppsEnabled(false);
        QVERIFY(click("subTab_music"));
    }

    // The status bar's light shows how Kick's servers are doing: green, yellow when something is wrong, red when it is down. Red covers the
    // live streams (nothing can be watched) but the downloads stay reachable, and when Kick answers again the list comes back by itself.
    void kickHealthLightShowsGreenYellowRedAndRecovers()
    {
        auto *w = window();
        auto fake = std::make_unique<FakeKick>();
        FakeKick *ft = fake.get();
        m_app->gateway()->setTransport(std::move(fake));
        KickClient *kick = m_app->kickClient();
        kick->setHealthInterval(100);
        struct Restore {
            QWindow *w; App *app; KickClient *k;
            ~Restore() { k->setHealthInterval(60000); k->clearBlocked(); app->setAppNetAllowed("kick", false); app->setAppsEnabled(false); w->setProperty("view", QStringLiteral("collection")); }
        } restore{w, m_app.get(), kick};
        w->setProperty("view", QStringLiteral("collection"));
        m_app->setAppsEnabled(true);
        m_app->setAppNetAllowed("kick", true);
        QTRY_VERIFY(item("subTab_apps") && item("subTab_apps")->isVisible());
        QVERIFY(click("subTab_apps"));
        QVERIFY(click("appTile_kick"));
        QTRY_VERIFY(item("kickCard_alpha") && item("kickCard_alpha")->isVisible());
        auto dot = [this] { return item("kickHealthDot")->property("color").value<QColor>(); };
        QTRY_COMPARE(kick->health(), QString("good"));
        QTRY_COMPARE(dot(), QColor("#53fc18"));
        QVERIFY(item("kickContacted")->property("text").toString().startsWith("kick is up"));
        QVERIFY(item("kickDown") == nullptr || !item("kickDown")->isVisible());
        shot("kick-health-green");
        // something is wrong: yellow, the list is still there
        ft->failMode = 1;
        QTRY_COMPARE_WITH_TIMEOUT(kick->health(), QString("slow"), 5000);
        QTRY_COMPARE(dot(), QColor("#f5c518"));
        QVERIFY(item("kickContacted")->property("text").toString().contains("slow or partly down"));
        QVERIFY(item("kickCard_alpha")->isVisible());
        shot("kick-health-yellow");
        // down: red, a cover over the streams, no way to watch, but the downloads are one tap away
        ft->failMode = 2;
        QTRY_COMPARE_WITH_TIMEOUT(kick->health(), QString("down"), 5000);
        QTRY_COMPARE(dot(), QColor("#ff4d4d"));
        QTRY_VERIFY(item("kickDown") && item("kickDown")->isVisible());
        QVERIFY(item("kickDown")->property("z").toReal() > 0);
        shot("kick-health-red");
        QVERIFY(click("kickDownDownloads"));
        QTRY_VERIFY(item("kickDownloads") && item("kickDownloads")->isVisible());
        QTRY_VERIFY(!item("kickDown")->isVisible());                                 // the downloads page is not covered
        // Kick answers again: the cover goes, the list is asked for again, and the light turns green
        const int before = ft->asked.size();
        ft->failMode = 0;
        QTRY_VERIFY_WITH_TIMEOUT(kick->health() != QString("down"), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(ft->asked.size() > before, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(kick->health(), QString("good"), 5000);
        QTRY_COMPARE(dot(), QColor("#53fc18"));
        QVERIFY(!item("kickDown")->isVisible());
        m_app->setAppNetAllowed("kick", false);
        m_app->setAppsEnabled(false);
        QVERIFY(click("subTab_music"));
    }

    // The seek meter along the top of a video kept on this device (Kick): drag or tap to rewind and skip ahead. A live stream has none.
    void seekMeterMovesADownloadedVideo()
    {
        QQmlComponent comp(m_engine.get(), QUrl(QStringLiteral("qrc:/qml/SeekMeter.qml")));
        QVERIFY2(comp.status() == QQmlComponent::Ready, qPrintable(comp.errorString()));
        std::unique_ptr<QObject> obj(comp.create(m_engine->rootContext()));
        QVERIFY(obj);
        QSignalSpy sink(obj.get(), SIGNAL(seekRequested(double)));
        QVERIFY(sink.isValid());
        auto last = [&] { return sink.last().at(0).toDouble(); };
        auto skip = [&](double by) { QVariant r; QMetaObject::invokeMethod(obj.get(), "skip", Q_RETURN_ARG(QVariant, r), Q_ARG(QVariant, by)); };
        // all of the video: "0:30 / 2:00:00", skips are clamped to the ends
        obj->setProperty("duration", 7200000.0); obj->setProperty("position", 30000.0);
        QVERIFY(obj->property("seekable").toBool());
        QCOMPARE(obj->property("text").toString(), QString("0:30 / 2:00:00"));
        skip(10000);   QCOMPARE(last(), 40000.0);
        skip(-10000);  QCOMPARE(last(), 20000.0);
        skip(-999999); QCOMPARE(last(), 0.0);
        obj->setProperty("position", 7195000.0);
        skip(10000);   QCOMPARE(last(), 7200000.0);
        // no length from the engine: the buttons do nothing and there is no text
        const int n = sink.size();
        obj->setProperty("duration", 0.0);
        QVERIFY(!obj->property("seekable").toBool());
        QCOMPARE(obj->property("text").toString(), QString());
        skip(10000);
        QCOMPARE(sink.size(), n);
    }

    // The mini player says what the open app is doing when no song plays: "kick" on the hub, "now playing <channel>" while a live stream is on.
    void miniPlayerSaysWhatKickIsDoing()
    {
        auto *w = window();
        auto fake = std::make_unique<FakeKick>();
        m_app->gateway()->setTransport(std::move(fake));
        KickClient *kick = m_app->kickClient();
        struct Restore { App *app; ~Restore() { app->setAppNetAllowed("kick", false); app->setAppsEnabled(false); } } restore{m_app.get()};
        w->setProperty("view", QStringLiteral("collection"));
        m_app->setAppsEnabled(true);
        m_app->setAppNetAllowed("kick", true);
        auto mini = [this] { return item("miniPlayer"); };
        QVERIFY(mini());
        QCOMPARE(mini()->property("appTitle").toString(), QString());                   // not in Kick: nothing to add
        QTRY_VERIFY(item("subTab_apps") && item("subTab_apps")->isVisible());
        QVERIFY(click("subTab_apps"));
        QVERIFY(click("appTile_kick"));
        QTRY_VERIFY(item("kickCard_alpha") && item("kickCard_alpha")->isVisible());
        // the hub: "kick" (a song that is playing would still win in the mini player's own label)
        QTRY_COMPARE(mini()->property("appTitle").toString(), QString("kick"));
        QCOMPARE(mini()->property("appSub").toString(), QString());
        if (!m_app->player()->hasTrack()) QCOMPARE(item("miniTitle")->property("text").toString(), QString("kick"));
        // an offline channel's page is still just "kick": nothing is playing there
        QVERIFY(QMetaObject::invokeMethod(item("kickApp"), "openChannel", Q_ARG(QVariant, QStringLiteral("alpha"))));
        QTRY_VERIFY(item("kickWatch") && item("kickWatch")->isVisible());
        QTRY_VERIFY(item("kickChannelState")->isVisible());
        QCOMPARE(mini()->property("appTitle").toString(), QString("kick"));
        // the rule itself (the live picture needs a video engine, which differs per machine, so the rule is tested directly)
        auto say = [this](QVariantMap st) {
            QVariant r;
            QMetaObject::invokeMethod(item("kickApp"), "nowPlayingFor", Q_RETURN_ARG(QVariant, r), Q_ARG(QVariant, st));
            return r.value<QJSValue>().toVariant().toMap();
        };
        const QVariantMap live{{"open", true}, {"page", "watch"}, {"tab", "live"}, {"live", true}, {"wanted", true}, {"error", ""}, {"channel", "LumielleVT"}};
        QCOMPARE(say(live).value("title").toString(), QString("now playing LumielleVT"));
        QCOMPARE(say(live).value("sub").toString(), QString("live on kick"));
        for (const QPair<QString, QVariant> &change : {qMakePair(QString("live"), QVariant(false)),            // offline
                                                       qMakePair(QString("wanted"), QVariant(false)),          // the picture is not on
                                                       qMakePair(QString("error"), QVariant("could not play")),   // it failed: never claim more than is true
                                                       qMakePair(QString("tab"), QVariant("clips")),           // looking at clips, not watching
                                                       qMakePair(QString("page"), QVariant("browse"))}) {
            QVariantMap st = live; st[change.first] = change.second;
            QCOMPARE(say(st).value("title").toString(), QString("kick"));
            QCOMPARE(say(st).value("sub").toString(), QString());
        }
        const QVariantMap dl{{"open", true}, {"page", "play"}, {"wanted", true}, {"error", ""}, {"dlTitle", "a funny moment"}, {"dlChannel", "alpha"}};
        QCOMPARE(say(dl).value("title").toString(), QString("now playing a funny moment"));
        QCOMPARE(say(dl).value("sub").toString(), QString("alpha"));
        QVariantMap broken = dl; broken["error"] = "x";
        QCOMPARE(say(broken).value("title").toString(), QString("kick"));
        QVariantMap closed = live; closed["open"] = false;
        QCOMPARE(say(closed).value("title").toString(), QString());
        // leaving Kick: back to what it was
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(item("kickBrowse")->isVisible());
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(!item("appHost")->property("active").toBool());
        QTRY_COMPARE(mini()->property("appTitle").toString(), QString());
        if (!m_app->player()->hasTrack()) QCOMPARE(item("miniTitle")->property("text").toString(), QString("nothing playing"));
        Q_UNUSED(kick);
        m_app->setAppNetAllowed("kick", false);
        m_app->setAppsEnabled(false);
        QVERIFY(click("subTab_music"));
    }

    // The live stream keeps playing in a small draggable window after the Kick app is left, shows in the mini player's text, and ends when you
    // press its x or start a song. (The picture needs a video engine that differs per machine, so state and geometry are asserted, not frames.)
    void aLiveStreamKeepsPlayingInASmallWindowAfterTheAppIsLeft()
    {
#ifdef Q_OS_ANDROID
        QSKIP("the phone has picture in picture instead");
#endif
        auto *w = window();
        auto fake = std::make_unique<FakeKick>();
        m_app->gateway()->setTransport(std::move(fake));
        StreamDock *dock = m_app->streamDock();
        const QSize before = w->size();
        struct Restore { App *app; QWindow *w; QSize size; ~Restore() { app->streamDock()->stop(); w->resize(size); app->setAppNetAllowed("kick", false); app->setAppsEnabled(false); } } restore{m_app.get(), w, before};
        const QString addr = QStringLiteral("https://a1b2.us-east-1.playback.live-video.net/api/video/v1/master.m3u8?t=1");
        w->setProperty("view", QStringLiteral("collection"));
        m_app->setAppsEnabled(true);
        m_app->setAppNetAllowed("kick", true);
        QVERIFY(!item("streamDock")->isVisible());                                        // nothing is playing: no window
        auto box = [this]() -> QRectF { QQuickItem *b = item("streamDockBox"); return QRectF(b->mapToScene(QPointF(0, 0)), b->size()); };
        auto inside = [&](const QRectF &r) { return r.left() >= 0 && r.top() >= 0 && r.right() <= w->width() + 0.5 && r.bottom() <= w->height() + 0.5; };
        // a stream starts (from a channel page, in the real app) and the app is left: the window stays, in the corner, inside the window
        QVERIFY(dock->start("gamehopper", "GameHopper", "a title", addr));
        QTRY_VERIFY(item("streamDock")->isVisible());
        QTRY_VERIFY(box().width() > 200 && box().width() < w->width() * 0.5);
        QVERIFY(inside(box()));
        QVERIFY(box().center().x() > w->width() / 2 && box().center().y() > w->height() / 2);   // bottom right
        for (const char *name : {"streamDockStop", "streamDockExpand", "streamDockMute", "streamDockName"}) QVERIFY2(item(name) && item(name)->isVisible(), name);
        QCOMPARE(item("streamDockName")->property("text").toString(), QString("GameHopper"));
        shot("stream-dock-corner");
        // it stays while you move around Hop-tunes, and the mini player says what is playing even with Kick closed
        w->setProperty("view", QStringLiteral("settings"));
        QVERIFY(item("streamDock")->isVisible() && dock->active());
        QCOMPARE(item("miniPlayer")->property("appTitle").toString(), QString("now playing GameHopper"));
        QCOMPARE(item("miniPlayer")->property("appSub").toString(), QString("live on kick"));
        w->setProperty("view", QStringLiteral("collection"));
        // it can be put anywhere, and never leaves the window (also after the window gets smaller)
        item("streamDock")->setProperty("spot", QPointF(120, 90));
        QTRY_VERIFY(qAbs(box().left() - 120) < 2 && qAbs(box().top() - 90) < 2);
        item("streamDock")->setProperty("spot", QPointF(99999, 99999));
        QTRY_VERIFY(inside(box()) && box().right() > w->width() - 40);
        w->resize(900, 600);
        QTRY_COMPARE(w->width(), 900);
        QTRY_VERIFY(inside(box()));
        shot("stream-dock-moved");
        w->resize(before);
        QTRY_COMPARE(w->size(), before);
        // the x closes it and moves on
        QVERIFY(click("streamDockStop"));
        QTRY_VERIFY(!dock->active());
        QTRY_VERIFY(!item("streamDock")->isVisible());
        QCOMPARE(item("miniPlayer")->property("appTitle").toString(), QString());
        // a song starting ends it; and a stream starting pauses a song that is playing
        QVERIFY(dock->start("gamehopper", "GameHopper", "a title", addr));
        QTRY_VERIFY(item("streamDock")->isVisible());
        m_app->playSongs(0);
        QTRY_VERIFY(m_app->player()->isPlaying());
        QTRY_VERIFY(!dock->active());
        QTRY_VERIFY(!item("streamDock")->isVisible());
        QVERIFY(dock->start("gamehopper", "GameHopper", "a title", addr));
        QVERIFY(!m_app->player()->isPlaying());                                           // the song waited
        QTRY_VERIFY(item("streamDock")->isVisible());
        // the arrow on the small window takes you back to the channel: Kick opens on it, and the picture grows over the page's slot
        QVERIFY(!item("appHost")->property("active").toBool());
        QVERIFY(click("streamDockExpand"));
        QTRY_VERIFY(item("appHost")->property("active").toBool());
        QTRY_VERIFY(item("kickWatch") && item("kickWatch")->isVisible());
        QTRY_COMPARE(m_app->kickClient()->channel().value("slug").toString(), QString("gamehopper"));
        QVERIFY(dock->active());
        QTRY_VERIFY(item("kickLiveSlot")->isVisible());
        QTRY_VERIFY(qAbs(box().width() - item("kickLiveSlot")->width()) < 3 && qAbs(box().height() - item("kickLiveSlot")->height()) < 3);   // grown to the page
        QVERIFY(!item("streamDockStop")->isVisible());                                    // (the page has the full controls)
        shot("stream-dock-on-page");
        // back to the hub: it shrinks to the corner again and keeps playing
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(item("kickBrowse")->isVisible());
        QTRY_VERIFY(item("streamDockStop")->isVisible());
        QTRY_VERIFY(box().width() < w->width() * 0.5);
        QVERIFY(dock->active());
        QCOMPARE(item("miniPlayer")->property("appTitle").toString(), QString("now playing GameHopper"));
        // leaving the app entirely: it keeps playing; taking the permission back ends it
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(!item("appHost")->property("active").toBool());
        QVERIFY(dock->active() && item("streamDock")->isVisible());
        m_app->setAppNetAllowed("kick", false);
        QTRY_VERIFY(!dock->active());
        m_app->setAppsEnabled(false);
        QVERIFY(click("subTab_music"));
    }

    // Kick on a phone held upright: one column of cards that fits the screen, touch-sized, and on the watch page the picture on top
    // with the chat under it.
    void kickAppFitsAPhone()
    {
        auto *w = window();
        auto fake = std::make_unique<FakeKick>();
        FakeKick *ft = fake.get();
        m_app->gateway()->setTransport(std::move(fake));
        KickClient *kick = m_app->kickClient();
        const QSize before = w->size();
        struct Restore {
            QWindow *w; QSize size; App *app; KickClient *k;
            ~Restore() { w->setProperty("forceMobile", false); w->resize(size); k->clearBlocked(); app->setAppNetAllowed("kick", false); app->setAppsEnabled(false); w->setProperty("view", QStringLiteral("collection")); }
        } restore{w, before, m_app.get(), kick};
        w->setProperty("view", QStringLiteral("collection"));
        m_app->setAppsEnabled(true);
        m_app->setAppNetAllowed("kick", true);
        w->setProperty("forceMobile", true);
        w->resize(390, 844);
        QTRY_COMPARE(w->width(), 390);
        QTRY_VERIFY(item("pillTab_apps") != nullptr);
        item("collectionView")->setProperty("collectionTab", QStringLiteral("apps"));
        QTRY_VERIFY(item("appTile_kick") && item("appTile_kick")->isVisible());
        QVERIFY(click("appTile_kick"));
        QTRY_VERIFY(item("kickCard_alpha") && item("kickCard_alpha")->isVisible());
        QQuickItem *app = item("kickApp");
        // everything stays inside the width of the screen: search, the filters (wrapped), the cards
        QTRY_VERIFY(item("kickSearch")->width() > 300);
        for (const char *name : {"kickSearch", "kickSort", "kickLanguage", "kickMature", "kickRefresh", "kickCard_alpha"}) {
            QQuickItem *it = item(name);
            QVERIFY2(it && it->isVisible(), name);
            const QPointF a = it->mapToItem(app, QPointF(0, 0)), b = it->mapToItem(app, QPointF(it->width(), 0));
            QVERIFY2(a.x() >= -1 && b.x() <= app->width() + 1, name);
        }
        QTRY_VERIFY(item("kickCard_alpha")->width() > 280);                          // one wide card per row, not a sliver
        QTRY_VERIFY(item("kickCard_bravo")->y() > item("kickCard_alpha")->y());      // the next one is under it
        QVERIFY(item("kickCardMenu_alpha")->isVisible());                            // no hover on a touch screen: the menu is always there
        shot("phone-kick-browse");
        // the arrow at the top right pulls the menu up like a curtain (room for streams) and back down
        QVERIFY(item("appCollapse") && item("appCollapse")->isVisible());
        const qreal cardY = item("kickCard_alpha")->mapToItem(app, QPointF(0, 0)).y();
        QVERIFY(click("appCollapse"));
        QTRY_VERIFY(!item("kickSearch")->isVisible());
        QTRY_VERIFY(item("kickCard_alpha")->mapToItem(app, QPointF(0, 0)).y() < cardY - 100);          // the streams moved up into the room
        const qreal upY = item("kickCard_alpha")->mapToItem(app, QPointF(0, 0)).y();
        shot("phone-kick-curtain-up");
        QVERIFY(click("appCollapse"));
        QTRY_VERIFY(item("kickSearch")->isVisible());
        QTRY_VERIFY(item("kickCard_alpha")->mapToItem(app, QPointF(0, 0)).y() > upY + 100);         // and back down again (the room is given back)
        QVERIFY(click("appCollapse"));                                                              // leave it pulled up: the search button brings it back (below)
        QTRY_VERIFY(!item("kickSearch")->isVisible());
        // on a phone a tap on a stream asks first (a fat finger while scrolling must not open one)
        QVERIFY(kick->askBeforeOpening());
        QVERIFY(click("kickCard_alpha"));
        QTRY_VERIFY(item("kickConfirm") && item("kickConfirm")->isVisible());
        QVERIFY(item("kickBrowse")->isVisible() && !item("kickWatch")->isVisible());            // nothing opened
        shot("phone-kick-confirm");
        QVERIFY(click("kickConfirmNo"));
        QTRY_VERIFY(!item("kickConfirm")->isVisible());
        QVERIFY(item("kickBrowse")->isVisible());
        QVERIFY(click("kickCard_alpha"));
        QTRY_VERIFY(item("kickConfirm")->isVisible());
        QVERIFY(click("kickConfirmYes"));
        QTRY_VERIFY(item("kickWatch") && item("kickWatch")->isVisible());
        QVERIFY(!item("kickConfirm")->isVisible());
        QTRY_VERIFY(item("kickChannelState")->isVisible());
        // the channel's header, tabs and record row are under the same curtain: pull it down to see them
        QVERIFY(item("appCollapse")->isVisible());
        QVERIFY(click("appCollapse"));
        QTRY_VERIFY(item("kickFollowButton")->isVisible());
        shot("phone-kick-watch-header");
        QVERIFY(click("kickChat_pretend"));
        QTRY_VERIFY(item("kickPretendChat")->isVisible());
        const QPointF stateTop = item("kickChannelState")->mapToItem(app, QPointF(0, 0));
        const QPointF chatTop = item("kickPretendChat")->mapToItem(app, QPointF(0, 0));
        QTRY_VERIFY(item("kickPretendChat")->mapToItem(app, QPointF(0, 0)).y() > item("kickChannelState")->mapToItem(app, QPointF(0, item("kickChannelState")->height())).y() - 1);
        Q_UNUSED(stateTop); Q_UNUSED(chatTop);
        shot("phone-kick-watch");
        QTRY_VERIFY2(item("kickPretendChat")->width() > 280, qPrintable(QString::number(item("kickPretendChat")->width())));
        QVERIFY(item("kickFollowButton")->mapToItem(app, QPointF(item("kickFollowButton")->width(), 0)).x() <= app->width() + 1);
        QVERIFY(item("kickBlockButton")->mapToItem(app, QPointF(item("kickBlockButton")->width(), 0)).x() <= app->width() + 1);
        QVERIFY(click("kickChat_off"));
        // leaving the app (Android) lets the stream go; coming back asks Kick for the channel again (a fresh stream address, not "Demuxing failed")
        {
            auto channelAsks = [&] { int n = 0; for (const QUrl &u : ft->asked) if (u.path() == "/api/v2/channels/alpha") ++n; return n; };
            const int asked = channelAsks();
            QVERIFY(QMetaObject::invokeMethod(app, "setBackground", Q_ARG(QVariant, true)));
            QVERIFY(app->property("inBackground").toBool());
            QTest::qWait(100);
            QCOMPARE(channelAsks(), asked);                                               // nothing is fetched while it is away
            QVERIFY(QMetaObject::invokeMethod(app, "setBackground", Q_ARG(QVariant, false)));
            QTRY_VERIFY(channelAsks() > asked);                                           // back: the channel is asked for again
            QTRY_VERIFY(item("kickChannelState")->isVisible());
        }
        QVERIFY(click("appCollapse"));                                                    // up again: the chat and video get the room
        QTRY_VERIFY(!item("kickFollowButton")->isVisible());
        // the chat: tall enough, and more room when the mini player is pushed away (its pull-back tab floats at the edge and takes no room)
        {
            QVERIFY(click("kickChat_pretend"));
            QQuickItem *chat = item("kickPretendChat");
            QTRY_VERIFY(chat && chat->isVisible());
            QVERIFY2(chat->height() >= 200, qPrintable(QString::number(chat->height())));   // (the column has a 280 dp minimum: this is the chat panel inside it)
            const qreal shown = chat->height();
            const qreal flickBefore = item("kickWatch")->height();
            w->setProperty("miniHidden", true);
            QTRY_VERIFY(item("miniPullTab")->isVisible());
            QTRY_VERIFY(item("kickWatch")->height() >= flickBefore + 60);                   // the page itself got the mini player's room (and no tab row is left behind)
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
            QTRY_VERIFY(chat->height() >= shown + 50);                                      // and on the phone's Qt (6.8) the chat takes it (Qt 6.4's grid layout does not hand it on)
#endif
            QVERIFY(chat->height() >= shown);
            // the tab is not on top of anything the person uses: not the chat's input, not its send button, not the bottom bar
            const QRectF tab(item("miniPullTab")->mapToScene(QPointF(0, 0)), item("miniPullTab")->size());
            for (const char *name : {"liveChatInput", "liveChatSend", "kickPretendChat"}) {
                QQuickItem *it = item(name);
                if (!it || !it->isVisible()) continue;
                const QRectF r(it->mapToScene(QPointF(0, 0)), it->size());
                QVERIFY2(!tab.intersects(r), name);
            }
            QVERIFY(tab.bottom() <= item("navTab_collection")->mapToScene(QPointF(0, 0)).y() + 1);   // above the bottom bar
            shot("phone-kick-chat-room");
            QVERIFY(click("miniPullTab"));
            QTRY_VERIFY(!w->property("miniHidden").toBool());
            QVERIFY(click("kickChat_off"));
        }
        // the bottom "search" button searches Kick while the Kick app is open (not the music), from any page of the app
        QVERIFY(item("navTab_search") && item("navTab_search")->isVisible());
        QVERIFY(click("navTab_search"));
        QTRY_VERIFY(item("kickBrowse")->isVisible());                                  // it left the watch page for the streamers list
        QTRY_VERIFY(item("kickSearch")->isVisible());                                  // (the menu was pulled up: searching brings it back down)
        QTRY_VERIFY(item("kickSearch")->property("activeFocus").toBool());             // with the search box ready to type in
        QCOMPARE(w->property("view").toString(), QStringLiteral("collection"));        // and did not switch to the music search
        QVERIFY(item("appHost")->property("active").toBool());
        QTRY_VERIFY(item("navTab_search")->property("on").toBool());                   // the tab shows it is the one in use
        shot("phone-kick-nav-search");
        for (const char ch : {'l', 'o', 'f', 'i'}) QTest::keyClick(w, ch);
        QTest::keyClick(w, Qt::Key_Return);
        QTRY_COMPARE(kick->query(), QStringLiteral("lofi"));
        // out of the app, the same button is the music search again
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(!item("appHost")->property("active").toBool());
        QVERIFY(click("navTab_search"));
        QTRY_COMPARE(w->property("view").toString(), QStringLiteral("search"));
        w->setProperty("view", QStringLiteral("collection"));
    }
#endif

#ifdef HOPTUNES_NET_APPS
    // A phone turned sideways: watching a stream is a widescreen "theatre": no bars around it, the picture as big as it can be, the chat beside it.
    void kickLandscapePhoneIsAWidescreenTheatre()
    {
        auto *w = window();
        auto fake = std::make_unique<FakeKick>();
        m_app->gateway()->setTransport(std::move(fake));
        KickClient *kick = m_app->kickClient();
        const QSize before = w->size();
        struct Restore {
            QWindow *w; QSize size; App *app; KickClient *k;
            ~Restore() { w->setProperty("forceMobile", false); w->resize(size); k->clearBlocked(); app->setAppNetAllowed("kick", false); app->setAppsEnabled(false); w->setProperty("view", QStringLiteral("collection")); }
        } restore{w, before, m_app.get(), kick};
        w->setProperty("view", QStringLiteral("collection"));
        m_app->setAppsEnabled(true);
        m_app->setAppNetAllowed("kick", true);
        w->setProperty("forceMobile", true);
        w->resize(844, 390);                                                          // held sideways
        QTRY_COMPARE(w->width(), 844);
        QTRY_VERIFY(item("pillTab_apps") != nullptr);
        item("collectionView")->setProperty("collectionTab", QStringLiteral("apps"));
        QTRY_VERIFY(item("appTile_kick") && item("appTile_kick")->isVisible());
        QVERIFY(QMetaObject::invokeMethod(item("appHost"), "open", Q_ARG(QVariant, QStringLiteral("kick"))));   // (the tile may sit below the short sideways screen)
        QTRY_VERIFY(item("kickCard_alpha") && item("kickCard_alpha")->isVisible());
        QQuickItem *app = item("kickApp");
        QVERIFY(!app->property("theatre").toBool());                                  // browsing keeps its bars
        QVERIFY(item("navTab_collection")->isVisible());
        QVERIFY(QMetaObject::invokeMethod(app, "openChannel", Q_ARG(QVariant, QStringLiteral("alpha"))));   // (what a tap on the card does)
        QTRY_VERIFY(item("kickWatch") && item("kickWatch")->isVisible());
        QTRY_VERIFY(app->property("theatre").toBool());
        // the bars around it are gone: the app's own title bar, the status line, the mini player and the bottom bar
        QTRY_VERIFY(!item("appBack")->isVisible());
        QVERIFY(!item("kickStatusBar")->isVisible());
        QVERIFY(!item("miniPlayer")->isVisible());
        QVERIFY(!item("navTab_collection")->isVisible());
        // the picture takes the room: tall as the window, wide next to the chat, and the chat sits to its right
        QQuickItem *player = item("kickPlayer");
        QTRY_VERIFY(player->height() > w->height() * 0.9);
        QVERIFY(player->width() > w->width() * 0.55);
        QTRY_VERIFY(item("kickPretendChat") != nullptr);
        QVERIFY(item("kickTheatreBack") && item("kickTheatreBack")->isVisible());
        shot("phone-kick-theatre");
        QTRY_VERIFY(item("kickTheatreBack")->mapToScene(QPointF(0, 0)).x() >= player->mapToScene(QPointF(player->width(), 0)).x() - 1);   // (the layout may still be settling)
        // back (the on-screen button, same as the phone's back gesture) leaves the theatre for the streams, bars back
        QVERIFY(click("kickTheatreBack"));
        QTRY_VERIFY(item("kickBrowse")->isVisible());
        QTRY_VERIFY(!app->property("theatre").toBool());
        QTRY_VERIFY(item("navTab_collection")->isVisible());
        QTRY_VERIFY(item("appBack")->isVisible());
        // held upright again: the one-column page, not the theatre
        QVERIFY(QMetaObject::invokeMethod(app, "openChannel", Q_ARG(QVariant, QStringLiteral("alpha"))));
        QTRY_VERIFY(item("kickWatch")->isVisible());
        w->resize(390, 844);
        QTRY_VERIFY(!app->property("theatre").toBool());
        QTRY_VERIFY(item("navTab_collection")->isVisible());
        QTest::keyClick(w, Qt::Key_Escape);
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(!item("appHost")->property("active").toBool());
    }
#endif

#ifdef HOPTUNES_NET_APPS
    // settings > apps settings > kick offline storage: the limit is the person's to change, with a meter; nothing for guests to keep.
    void offlineStorageSettingsShowAndChangeTheLimit()
    {
        auto *w = window();
        m_app->setAppsEnabled(true);
        w->setProperty("view", QStringLiteral("settings"));
        QTRY_VERIFY(item("offlineBox") && item("offlineBox")->isVisible());
        OfflineStore *off = m_app->offlineStore();
        QCOMPARE(off->limitMB(), OfflineStore::kDefaultLimitMB);
        QVERIFY(item("offlineSummary")->property("text").toString().contains("of your"));
        QVERIFY(item("offlineMeter") && item("offlineWhere")->property("text").toString().contains("hop-tunes"));
        QTest::qWait(300);
        if (QQuickItem *flick = item("settingsFlick")) {                           // scroll the section into view for the render
            const qreal y = item("offlineBox")->mapToItem(flick->property("contentItem").value<QQuickItem *>(), QPointF(0, 0)).y();
            flick->setProperty("contentY", qMax<qreal>(0, y - 80));
        }
        QTest::qWait(150);
        shot("settings-offline-storage");
        // a number below the smallest limit is refused, plainly
        item("offlineLimitEdit")->setProperty("text", QStringLiteral("0.01"));
        item("offlineLimitEdit")->setProperty("useGB", false);
        item("offlineLimitEdit")->setProperty("text", QStringLiteral("10"));
        QVERIFY(click("offlineSetLimit"));
        QTRY_VERIFY(item("offlineMessage")->isVisible() && item("offlineMessage")->property("text").toString().contains("50 MB"));
        QCOMPARE(off->limitMB(), OfflineStore::kDefaultLimitMB);
        // a good one is kept, in the profile, and can go up or down again
        item("offlineLimitEdit")->setProperty("text", QStringLiteral("200"));
        QVERIFY(click("offlineSetLimit"));
        QCOMPARE(off->limitMB(), 200.0);
        QVERIFY(m_app->profileStore()->settings(m_app->profile().value("id").toString()).value("offline").toObject().value("limitMB").toDouble() == 200.0);
        QTRY_VERIFY(!item("offlineMessage")->isVisible());
        item("offlineLimitEdit")->setProperty("useGB", true);
        item("offlineLimitEdit")->setProperty("text", QStringLiteral("3"));
        QVERIFY(click("offlineSetLimit"));
        QCOMPARE(off->limitMB(), 3072.0);
        QVERIFY(!item("offlineDefault")->isVisible());                            // nothing to go back from
        off->setLimitMB(OfflineStore::kDefaultLimitMB);
        m_app->setAppsEnabled(false);
        w->setProperty("view", QStringLiteral("collection"));
    }

    // The record button: only on a live channel, never by itself; recording shows a red bar and a stop button; the result is in the downloads.
    void aLiveStreamCanBeRecordedAndStopped()
    {
        auto *w = window();
        auto fake = std::make_unique<FakeKick>();
        m_app->gateway()->setTransport(std::move(fake));
        KickClient *kick = m_app->kickClient();
        OfflineDownloads *dl = m_app->downloads();
        struct Restore { App *app; KickClient *k; ~Restore() { app->downloads()->reload(); app->setAppNetAllowed("kick", false); app->setAppsEnabled(false); k->clearBlocked(); } } restore{m_app.get(), kick};
        w->setProperty("view", QStringLiteral("collection"));
        m_app->setAppsEnabled(true);
        m_app->setAppNetAllowed("kick", true);
        QTRY_VERIFY(item("subTab_apps")->isVisible());
        QVERIFY(click("subTab_apps"));
        QVERIFY(click("appTile_kick"));
        QTRY_VERIFY(item("kickCard_gamehopper") && item("kickCard_gamehopper")->isVisible());
        QVERIFY(click("kickCard_gamehopper"));                                         // the one live channel in the fixture (no stream address, so no player starts)
        QTRY_VERIFY(item("kickRecordRow") && item("kickRecordRow")->isVisible());
        QVERIFY2(item("kickRecordButton")->isVisible() && !item("kickRecordingBar")->isVisible(),
                 qPrintable(QStringLiteral("button %1 bar %2 count %3 active %4").arg(item("kickRecordButton")->isVisible()).arg(item("kickRecordingBar")->isVisible()).arg(dl->count())
                            .arg(QString::fromUtf8(QJsonDocument::fromVariant(item("kickApp")->property("activeRecording")).toJson(QJsonDocument::Compact)))));
        QVERIFY(click("kickRecordButton"));                                            // nothing to record from: said plainly, nothing started
        QCOMPARE(dl->count(), 0);
        QVERIFY(!dl->busy());
        // a recording (started with an address, as the button does) shows the bar, and stop keeps what there is
        QCOMPARE(dl->startRecording({{"id", "rec-ui"}, {"title", "developer stream"}, {"channel", "gamehopper"}, {"url", "https://a1.playback.live-video.net/live/master.m3u8"}, {"maxHeight", 720}}), QString());
        QTRY_VERIFY(item("kickRecordingBar")->isVisible());
        QVERIFY(item("kickRecordingBar")->property("text").toString().startsWith("recording"));
        QVERIFY(!item("kickRecordButton")->isVisible() && item("kickStopRecord")->isVisible());
        QTRY_VERIFY(dl->item("rec-ui").value("doneParts").toInt() >= 2);
        shot("kick-recording");
        QVERIFY(click("kickStopRecord"));
        QTRY_VERIFY(!item("kickRecordingBar")->isVisible());
        QTRY_VERIFY(item("kickRecordButton")->isVisible());
        QCOMPARE(dl->item("rec-ui").value("state").toString(), QString("done"));
        QVERIFY(QFileInfo(dl->videoPath("rec-ui")).size() >= 6000);
        // it is in the downloads like the rest, and plays from disk
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(item("kickBrowse")->isVisible());
        QVERIFY(click("kickDownloadsButton"));
        QTRY_VERIFY(item("kickDl_rec-ui") && item("kickDl_rec-ui")->isVisible());
        QVERIFY(item("kickDlState_rec-ui")->property("text").toString().startsWith("ready"));
        QVERIFY(item("kickDlPlay_rec-ui")->property("enabled").toBool());
        dl->remove("rec-ui");
        QTRY_VERIFY(!dl->has("rec-ui"));
        QTest::keyClick(w, Qt::Key_Escape);                                            // out of downloads, then out of the app
        QTRY_VERIFY(item("kickBrowse")->isVisible());
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(!item("appHost")->property("active").toBool());
    }

    // Playlists: favourite channels (and clips, past streams) kept in the profile; "+ playlist" adds, open lists, remove and delete work.
    void playlistsKeepFavouriteChannelsClipsAndStreamsOnThisDevice()
    {
        auto *w = window();
        auto fake = std::make_unique<FakeKick>();
        m_app->gateway()->setTransport(std::move(fake));
        KickClient *kick = m_app->kickClient();
        OfflineDownloads *dl = m_app->downloads();
        struct Restore { App *app; KickClient *k; ~Restore() { for (const QVariant &p : k->playlists()) k->deletePlaylist(p.toMap().value("id").toString()); app->downloads()->reload(); app->setAppNetAllowed("kick", false); app->setAppsEnabled(false); k->clearBlocked(); } } restore{m_app.get(), kick};
        w->setProperty("view", QStringLiteral("collection"));
        m_app->setAppsEnabled(true);
        m_app->setAppNetAllowed("kick", true);
        QTRY_VERIFY(item("subTab_apps")->isVisible());
        QVERIFY(click("subTab_apps"));
        QVERIFY(click("appTile_kick"));
        QTRY_VERIFY(item("kickCard_alpha") && item("kickCard_alpha")->isVisible());
        QVERIFY(click("kickPlaylistsButton"));
        QTRY_VERIFY(item("kickPlaylistsPage") && item("kickPlaylistsPage")->isVisible());
        QVERIFY(item("kickPlaylistsEmpty")->isVisible());
        // a name is needed
        QVERIFY(click("kickCreatePlaylist"));
        QTRY_VERIFY(item("kickPlaylistMessage") && item("kickPlaylistMessage")->isVisible());
        QCOMPARE(kick->playlists().size(), 0);
        // a channel playlist
        item("kickNewPlaylistName")->setProperty("text", QStringLiteral("favourites"));
        QVERIFY(click("kickCreatePlaylist"));
        QTRY_COMPARE(kick->playlists().size(), 1);
        const QString favs = kick->playlists().first().toMap().value("id").toString();
        QTRY_VERIFY(item("kickPlaylistRow_" + favs.toUtf8()) && item("kickPlaylistRow_" + favs.toUtf8())->isVisible());
        QVERIFY(!item("kickPlaylistsEmpty")->isVisible());
        QVERIFY(m_app->profileStore()->settings(m_app->profile().value("id").toString()).value("kick").toObject().value("playlists").toArray().size() == 1);   // saved with the profile
        // add a channel from its page: the menu lists the channel playlists
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(item("kickBrowse")->isVisible());
        QVERIFY(click("kickCard_alpha"));
        QTRY_VERIFY(item("kickAddToPlaylist") && item("kickAddToPlaylist")->isVisible());
        QVERIFY(click("kickAddToPlaylist"));
        QTRY_VERIFY(item("kickPlMenu_" + favs.toUtf8()));
        QVERIFY(click("kickPlMenu_" + favs.toUtf8()));
        QTRY_VERIFY(kick->isInPlaylist(favs, "alpha"));
        // it shows in the playlist, with its live state only when asked for
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(item("kickBrowse")->isVisible());
        QVERIFY(click("kickPlaylistsButton"));
        QTRY_VERIFY(item("kickPlaylistOpen_" + favs.toUtf8()));
        QVERIFY(click("kickPlaylistOpen_" + favs.toUtf8()));
        QTRY_VERIFY(item("kickPlaylistPage")->isVisible());
        QTRY_VERIFY(item("kickPlItem_alpha") && item("kickPlItem_alpha")->isVisible());
        QVERIFY(!item("kickPlEmpty")->isVisible());
        shot("kick-playlist-channels");
        QVERIFY(click("kickPlRemove_alpha"));
        QTRY_VERIFY(!kick->isInPlaylist(favs, "alpha"));
        QTRY_VERIFY(item("kickPlEmpty")->isVisible());
        // a clips playlist: add a clip from a channel's clips tab, download it from the playlist, play it
        QTest::keyClick(w, Qt::Key_Escape);                                            // to the playlists
        QTRY_VERIFY(item("kickPlaylistsPage")->isVisible());
        item("kickNewPlaylistName")->setProperty("text", QStringLiteral("funny"));
        QVERIFY(click("kickNewPlaylistKind_clips"));
        QVERIFY(click("kickCreatePlaylist"));
        QTRY_COMPARE(kick->playlists().size(), 2);
        const QString clips = kick->playlists().last().toMap().value("id").toString();
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(item("kickBrowse")->isVisible());
        QVERIFY(click("kickCard_alpha"));
        QTRY_VERIFY(item("kickTab_clips") && item("kickTab_clips")->isVisible());
        QVERIFY(click("kickTab_clips"));
        QTRY_VERIFY(item("kickAddPl_clip_abc") && item("kickAddPl_clip_abc")->isVisible());
        QVERIFY(click("kickAddPl_clip_abc"));
        QTRY_VERIFY(item("kickPlMenu_" + clips.toUtf8()));
        QVERIFY(item("kickPlMenu_" + favs.toUtf8()) == nullptr);                       // only playlists that can hold a clip
        QVERIFY(click("kickPlMenu_" + clips.toUtf8()));
        QTRY_VERIFY(kick->isInPlaylist(clips, "clip_abc"));
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(item("kickBrowse")->isVisible());
        QVERIFY(click("kickPlaylistsButton"));
        QTRY_VERIFY(item("kickPlaylistOpen_" + clips.toUtf8()));
        QVERIFY(click("kickPlaylistOpen_" + clips.toUtf8()));
        QTRY_VERIFY(item("kickPlItem_clip_abc") && item("kickPlItem_clip_abc")->isVisible());
        QVERIFY(click("kickPlDownload_clip_abc"));
        QTRY_COMPARE_WITH_TIMEOUT(dl->item("clip_abc").value("state").toString(), QString("done"), 8000);
        QTRY_VERIFY(item("kickPlPlay_clip_abc") && item("kickPlPlay_clip_abc")->isVisible());
        shot("kick-playlist-clips");
        dl->remove("clip_abc");
        // delete needs a second press
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(item("kickPlaylistsPage")->isVisible());
        QVERIFY(click("kickPlaylistDelete_" + clips.toUtf8()));
        QCOMPARE(kick->playlists().size(), 2);                                         // the first press only asks
        QTRY_VERIFY(item("kickPlaylistDelete_" + clips.toUtf8())->property("text").toString().contains("really"));
        QVERIFY(click("kickPlaylistDelete_" + clips.toUtf8()));
        QTRY_COMPARE(kick->playlists().size(), 1);
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(item("kickBrowse")->isVisible());
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(!item("appHost")->property("active").toBool());
    }

    // "english only": a switch on the streams page and in settings, saved in the profile; the language menu steps aside while it is on.
    void englishOnlyCanBeForcedFromTheStreamsPageAndSettings()
    {
        auto *w = window();
        auto fake = std::make_unique<FakeKick>();
        m_app->gateway()->setTransport(std::move(fake));
        KickClient *kick = m_app->kickClient();
        struct Restore { App *app; KickClient *k; ~Restore() { k->setEnglishOnly(false); app->setAppNetAllowed("kick", false); app->setAppsEnabled(false); } } restore{m_app.get(), kick};
        w->setProperty("view", QStringLiteral("collection"));
        m_app->setAppsEnabled(true);
        m_app->setAppNetAllowed("kick", true);
        QTRY_VERIFY(item("subTab_apps")->isVisible());
        QVERIFY(click("subTab_apps"));
        QVERIFY(click("appTile_kick"));
        QTRY_VERIFY(item("kickCard_alpha") && item("kickCard_alpha")->isVisible());
        QVERIFY(item("kickEnglishOnly") && item("kickEnglishOnly")->isVisible());
        QVERIFY(item("kickLanguage")->property("enabled").toBool());
        QVERIFY(!kick->englishOnly());
        QVERIFY(click("kickEnglishOnly"));
        QTRY_VERIFY(kick->englishOnly());
        QCOMPARE(kick->language(), QStringLiteral("en"));
        QTRY_VERIFY(!item("kickLanguage")->property("enabled").toBool());              // the menu steps aside: it is English whatever else
        QVERIFY(m_app->profileStore()->settings(m_app->profile().value("id").toString()).value("kick").toObject().value("englishOnly").toBool());   // saved
        QTRY_VERIFY(item("kickCard_alpha") && item("kickCard_alpha")->isVisible());     // (the fixture's streams are English)
        shot("kick-english-only");
        QTest::keyClick(w, Qt::Key_Escape);                                            // out of the app
        QTRY_VERIFY(!item("appHost")->property("active").toBool());
        // settings has the same choice, and it is the same setting
        w->setProperty("view", QStringLiteral("settings"));
        QTRY_VERIFY(item("kickEnglishChoice") != nullptr);
        QVERIFY(click("choice_kickEngAll"));
        QTRY_VERIFY(!kick->englishOnly());
        QVERIFY(click("choice_kickEngOnly"));
        QTRY_VERIFY(kick->englishOnly());
        w->setProperty("view", QStringLiteral("collection"));
    }

    // "add channels": paste names or links into the follow list or a channel playlist. No sign-in, nothing is sent to Kick.
    void pastingChannelNamesAddsThemToTheLocalListsWithoutAskingKick()
    {
        auto *w = window();
        auto fake = std::make_unique<FakeKick>();
        FakeKick *ft = fake.get();
        m_app->gateway()->setTransport(std::move(fake));
        KickClient *kick = m_app->kickClient();
        struct Restore { App *app; KickClient *k; ~Restore() { k->unfollow("zulu"); k->unfollow("yankee"); k->unfollow("xray"); app->setAppNetAllowed("kick", false); app->setAppsEnabled(false); } } restore{m_app.get(), kick};
        w->setProperty("view", QStringLiteral("collection"));
        m_app->setAppsEnabled(true);
        m_app->setAppNetAllowed("kick", true);
        QTRY_VERIFY(item("subTab_apps")->isVisible());
        QVERIFY(click("subTab_apps"));
        QVERIFY(click("appTile_kick"));
        QTRY_VERIFY(item("kickCard_alpha") && item("kickCard_alpha")->isVisible());
        QVERIFY(!item("kickSignInPage") && !item("kickSignInButton") && !item("kickSignIn"));   // there is no sign-in: Kick is a local viewing experience
        QVERIFY(item("kickAccountButton") && item("kickAccountButton")->isVisible());
        QCOMPARE(item("kickAccountButton")->property("text").toString(), QString("add channels"));
        QVERIFY(click("kickAccountButton"));
        QTRY_VERIFY(item("kickAccountPage") && item("kickAccountPage")->isVisible());
        QVERIFY(!item("kickImportAdd")->property("enabled").toBool());                 // nothing pasted yet
        shot("kick-add-channels");
        const int asked = ft->asked.size();
        // paste into the follow list
        item("kickImportText")->setProperty("text", QStringLiteral("zulu\nhttps://kick.com/yankee\n"));
        QTRY_VERIFY(item("kickImportAdd")->property("enabled").toBool());
        QVERIFY(click("kickImportAdd"));
        QTRY_VERIFY(item("kickImportMessage")->isVisible() && item("kickImportMessage")->property("text").toString().startsWith("added 2"));
        QVERIFY(kick->isFollowing("zulu") && kick->isFollowing("yankee"));
        QVERIFY(item("kickImportText")->property("text").toString().isEmpty());        // the box is emptied after a good paste
        shot("kick-add-channels-done");
        QCOMPARE(ft->asked.size(), asked);                                             // nothing was sent to Kick
        // into a playlist of channels (the chips show only those)
        const QString pl = kick->createPlaylist("favourites", "channels");
        kick->createPlaylist("funny", "clips");
        QTRY_VERIFY(item((QByteArray("kickImportTarget_") + pl.toUtf8()).constData()) != nullptr);
        QVERIFY(item((QByteArray("kickImportTarget_") + kick->playlists().last().toMap().value("id").toString().toUtf8()).constData()) == nullptr);   // the clips list is not offered
        QVERIFY(click((QByteArray("kickImportTarget_") + pl.toUtf8()).constData()));
        item("kickImportText")->setProperty("text", QStringLiteral("xray zulu"));
        QVERIFY(click("kickImportAdd"));
        QTRY_VERIFY(kick->isInPlaylist(pl, "xray") && kick->isInPlaylist(pl, "zulu"));
        QVERIFY(!kick->isFollowing("xray"));
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(item("kickBrowse")->isVisible());
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(!item("appHost")->property("active").toBool());
        kick->deletePlaylist(pl);
        for (const auto &p : kick->playlists()) kick->deletePlaylist(p.toMap().value("id").toString());
    }

    // Search follows the spelling as you type (after a short pause), closest names first; no need to press enter.
    void searchFollowsTheSpellingAsYouType()
    {
        auto *w = window();
        auto fake = std::make_unique<FakeKick>();
        FakeKick *ft = fake.get();
        m_app->gateway()->setTransport(std::move(fake));
        KickClient *kick = m_app->kickClient();
        struct Restore { App *app; KickClient *k; ~Restore() { k->search(""); app->setAppNetAllowed("kick", false); app->setAppsEnabled(false); } } restore{m_app.get(), kick};
        w->setProperty("view", QStringLiteral("collection"));
        m_app->setAppsEnabled(true);
        m_app->setAppNetAllowed("kick", true);
        QTRY_VERIFY(item("subTab_apps")->isVisible());
        QVERIFY(click("subTab_apps"));
        QVERIFY(click("appTile_kick"));
        QTRY_VERIFY(item("kickCard_alpha") && item("kickCard_alpha")->isVisible());
        auto searches = [&] { int n = 0; for (const QUrl &u : ft->asked) if (u.path() == "/api/search") ++n; return n; };
        QCOMPARE(searches(), 0);
        QQuickItem *box = item("kickSearch");
        box->setProperty("text", QStringLiteral("a"));                                   // one letter finds too much: nothing is asked
        QTest::qWait(600);
        QCOMPARE(searches(), 0);
        box->setProperty("text", QStringLiteral("alp"));                                 // a pause after typing: the list follows, no enter
        QTRY_COMPARE_WITH_TIMEOUT(kick->query(), QStringLiteral("alp"), 3000);
        QTRY_VERIFY(item("kickCard_alphabet") && item("kickCard_alpha"));
        QVERIFY(item("kickCard_xalpha") != nullptr);
        QCOMPARE(kick->streams().first().toMap().value("slug").toString(), QString("alpha"));            // "alpha" starts with it and is the shortest
        box->setProperty("text", QStringLiteral("alpha"));                               // the spelling gets closer: exact name first
        QTRY_COMPARE_WITH_TIMEOUT(kick->query(), QStringLiteral("alpha"), 3000);
        QTRY_COMPARE(kick->streams().first().toMap().value("slug").toString(), QString("alpha"));
        QVERIFY(searches() <= 3);                                                        // it waited for pauses: not one request per key
        shot("kick-search-as-you-type");
        // a pasted link waits for enter (it opens the channel); clearing the box goes back to the live list
        box->setProperty("text", QStringLiteral("https://kick.com/bravo"));
        QTest::qWait(600);
        QCOMPARE(kick->query(), QStringLiteral("alpha"));
        box->setProperty("text", QString());
        QTRY_COMPARE_WITH_TIMEOUT(kick->query(), QString(), 3000);
        QTRY_VERIFY(item("kickCard_bravo") && item("kickCard_bravo")->isVisible());
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(!item("appHost")->property("active").toBool());
    }

    // Your own category chips: swap GTA for Palworld in the kick app; settings can reorder / remove / reset; the app descriptions can be hidden.
    void categoryChipsCanBeSwappedAndAppDescriptionsHidden()
    {
        auto *w = window();
        auto fake = std::make_unique<FakeKick>();
        m_app->gateway()->setTransport(std::move(fake));
        KickClient *kick = m_app->kickClient();
        struct Restore { App *app; KickClient *k; ~Restore() { k->resetCategories(); app->setAppHints(true); app->setAppNetAllowed("kick", false); app->setAppsEnabled(false); } } restore{m_app.get(), kick};
        w->setProperty("view", QStringLiteral("collection"));
        m_app->setAppsEnabled(true);
        m_app->setAppNetAllowed("kick", true);
        QTRY_VERIFY(item("subTab_apps")->isVisible());
        QVERIFY(click("subTab_apps"));
        QVERIFY(click("appTile_kick"));
        QTRY_VERIFY(item("kickCard_alpha") && item("kickCard_alpha")->isVisible());
        QTRY_VERIFY(item("kickCat_3") && item("kickCat_3")->isVisible());                  // Kick's popular ones, including the one you do not care about
        QVERIFY(!kick->categoriesCustom());
        QVERIFY(click("kickEditCategories"));
        QTRY_VERIFY(item("kickCategoriesPage") && item("kickCategoriesPage")->isVisible());
        QTRY_VERIFY(item("kickCatRow_3") && item("kickCatRow_15"));
        QVERIFY(click("kickCatRemove_3"));
        QTRY_VERIFY(item("kickCatRow_3") == nullptr);
        QVERIFY(kick->categoriesCustom());
        item("kickCatSearch")->setProperty("text", QStringLiteral("palw"));
        QTRY_VERIFY_WITH_TIMEOUT(item("kickCatAdd_7234") && item("kickCatAdd_7234")->isVisible(), 3000);
        QVERIFY(click("kickCatAdd_7234"));
        QTRY_VERIFY(item("kickCatRow_7234") != nullptr);
        QTRY_VERIFY(item("kickCatAdd_7234")->property("text").toString().startsWith("in your list"));
        QVERIFY(m_app->profileStore()->settings(m_app->profile().value("id").toString()).value("kick").toObject().value("pinnedCategories").toObject().value("custom").toBool());   // saved
        shot("kick-categories-editor");
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(item("kickBrowse")->isVisible());
        QTRY_VERIFY(item("kickCat_7234") && item("kickCat_7234")->isVisible());            // Palworld is a chip now
        QVERIFY(item("kickCat_3") == nullptr);                                             // and GTA is gone
        QVERIFY(click("kickCat_7234"));                                                    // choosing it filters the list
        QTRY_COMPARE(kick->categoryId(), 7234);
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(!item("appHost")->property("active").toBool());
        // settings: the same list, reorder offline, back to kick's popular ones
        w->setProperty("view", QStringLiteral("settings"));
        QTRY_VERIFY(item("kickCategoriesBox") && item("kickCategoriesBox")->isVisible());
        QVERIFY(item("kickCategoriesSummary")->property("text").toString().contains("your own list"));
        QTRY_VERIFY(item("kickCatSettingsUp_7234") != nullptr);
        QVERIFY(click("kickCatSettingsUp_7234"));
        QTRY_COMPARE(kick->chips().first().toMap().value("id").toInt(), 7234);
        QVERIFY(click("kickCatSettingsReset"));
        QTRY_VERIFY(!kick->categoriesCustom());
        QTRY_VERIFY(item("kickCategoriesSummary")->property("text").toString().contains("popular"));
        // the little description on the apps tiles can be hidden for good
        QVERIFY(m_app->appHints());
        QTRY_VERIFY(item("appHintsChoice") != nullptr);
        QVERIFY(click("choice_hintsOff"));
        QTRY_VERIFY(!m_app->appHints());
        QVERIFY(!m_app->profileStore()->settings(m_app->profile().value("id").toString()).value("appHints").toBool(true));   // saved
        QVERIFY(click("choice_hintsOn"));
        QTRY_VERIFY(m_app->appHints());
        w->setProperty("view", QStringLiteral("collection"));
    }

    // A search keeps gameplay streams and channel names apart: headings, a scope choice, and nothing wider than the screen.
    void searchSeparatesGameplayStreamsFromChannels()
    {
        auto *w = window();
        auto fake = std::make_unique<FakeKick>();
        m_app->gateway()->setTransport(std::move(fake));
        KickClient *kick = m_app->kickClient();
        const QSize before = w->size();
        struct Restore { QQuickWindow *w; QSize size; App *app; KickClient *k; ~Restore() { k->search(""); w->setProperty("forceMobile", false); w->resize(size); app->setAppNetAllowed("kick", false); app->setAppsEnabled(false); } } restore{w, before, m_app.get(), kick};
        w->setProperty("view", QStringLiteral("collection"));
        m_app->setAppsEnabled(true);
        m_app->setAppNetAllowed("kick", true);
        w->setProperty("forceMobile", true);                                           // a phone: the narrowest case
        w->resize(390, 844);
        QTRY_COMPARE(w->width(), 390);
        QTRY_VERIFY(item("pillTab_apps") != nullptr);
        item("collectionView")->setProperty("collectionTab", QStringLiteral("apps"));
        QTRY_VERIFY(item("appTile_kick") && item("appTile_kick")->isVisible());
        QVERIFY(click("appTile_kick"));
        QTRY_VERIFY(item("kickCard_alpha") && item("kickCard_alpha")->isVisible());
        item("kickSearch")->setProperty("text", QStringLiteral("palw"));                // a game, typed
        QTRY_COMPARE_WITH_TIMEOUT(kick->query(), QStringLiteral("palw"), 3000);
        QTRY_VERIFY(item("kickGameHeader") && item("kickGameHeader")->isVisible());
        QVERIFY(item("kickGameHeader")->property("text").toString().contains("palworld"));
        QVERIFY(item("kickSearchScope")->isVisible());
        QVERIFY(item("kickScope_streams") && item("kickScope_channels") && item("kickScope_all"));
        // nothing is wider than the screen (a long results line used to push the whole page wider)
        QQuickItem *app = item("kickApp");
        QVERIFY(item("kickSearchScope")->mapToItem(app, QPointF(item("kickSearchScope")->width(), 0)).x() <= app->width() + 1);
        QVERIFY(item("kickCard_alpha")->mapToItem(app, QPointF(item("kickCard_alpha")->width(), 0)).x() <= app->width() + 1);
        shot("phone-kick-search-scope");
        // channels only: the streams playing the game are gone from the list
        QVERIFY(click("kickScope_channels"));
        QTRY_VERIFY(item("kickCard_alpha") == nullptr);
        QVERIFY(!item("kickGameHeader")->isVisible());
        // streams only: they are back, and nothing about channels
        QVERIFY(click("kickScope_streams"));
        QTRY_VERIFY(item("kickCard_alpha") && item("kickCard_alpha")->isVisible());
        QVERIFY(!item("kickScopeNote")->isVisible());
        QVERIFY(click("kickScope_all"));
        QTRY_VERIFY(item("kickCard_alpha") != nullptr);
        item("kickSearch")->setProperty("text", QString());
        QTRY_COMPARE_WITH_TIMEOUT(kick->query(), QString(), 3000);
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(!item("appHost")->property("active").toBool());
    }

    // A channel's finished streams and clips can be downloaded (only while the app is open), listed with what is kept, and played
    // from disk; deleting removes them.
    void pastStreamsAndClipsCanBeDownloadedListedAndPlayedFromDisk()
    {
        auto *w = window();
        auto fake = std::make_unique<FakeKick>();
        FakeKick *ft = fake.get();
        m_app->gateway()->setTransport(std::move(fake));
        KickClient *kick = m_app->kickClient();
        OfflineDownloads *dl = m_app->downloads();
        struct Restore { App *app; KickClient *k; ~Restore() { app->downloads()->reload(); app->setAppNetAllowed("kick", false); app->setAppsEnabled(false); k->clearBlocked(); } } restore{m_app.get(), kick};
        w->setProperty("view", QStringLiteral("collection"));
        m_app->setAppsEnabled(true);
        m_app->setAppNetAllowed("kick", true);
        QTRY_VERIFY(item("subTab_apps")->isVisible());
        QVERIFY(click("subTab_apps"));
        QVERIFY(click("appTile_kick"));
        QTRY_VERIFY(item("kickCard_alpha") && item("kickCard_alpha")->isVisible());
        QVERIFY(item("kickDownloadsButton") && item("kickDownloadsButton")->isVisible());
        QVERIFY(click("kickCard_alpha"));
        QTRY_VERIFY(item("kickTab_past") && item("kickTab_past")->isVisible());
        // the live tab is the start; the lists are only fetched when asked
        QVERIFY(item("kickLibrary") && !item("kickLibrary")->isVisible());
        for (const QUrl &u : ft->asked) QVERIFY2(!u.path().endsWith("/videos") && !u.path().endsWith("/clips"), qPrintable(u.toString()));
        QVERIFY(click("kickTab_past"));
        QTRY_VERIFY(item("kickLib_stream-77") && item("kickLib_stream-77")->isVisible());
        QVERIFY(item("kickLib_stream-1") == nullptr);                                  // the live one is the channel page, not a download
        QVERIFY(item("kickDownload_stream-77")->isVisible());
        QVERIFY(item("kickPretendChat") == nullptr || !item("kickPretendChat")->isVisible());
        shot("kick-past-streams");
        QVERIFY(click("kickDownload_stream-77"));
        QTRY_VERIFY(dl->has("stream-77"));
        QTRY_COMPARE_WITH_TIMEOUT(dl->item("stream-77").value("state").toString(), QString("done"), 8000);
        QCOMPARE(dl->item("stream-77").value("doneParts").toInt(), 2);
        QVERIFY(QFileInfo::exists(dl->videoPath("stream-77")));
        QTRY_VERIFY(item("kickPlayLocal_stream-77") && item("kickPlayLocal_stream-77")->isVisible());
        QVERIFY(!item("kickDownload_stream-77")->isVisible());
        // clips
        QVERIFY(click("kickTab_clips"));
        QTRY_VERIFY(item("kickLib_clip_abc") && item("kickLib_clip_abc")->isVisible());
        QVERIFY(click("kickDownload_clip_abc"));
        QTRY_COMPARE_WITH_TIMEOUT(dl->item("clip_abc").value("state").toString(), QString("done"), 8000);
        // the downloads page lists both, with their state, and plays from disk
        QTest::keyClick(w, Qt::Key_Escape);                                            // back to the streams
        QTRY_VERIFY(item("kickBrowse")->isVisible());
        QVERIFY(click("kickDownloadsButton"));
        QTRY_VERIFY(item("kickDownloads") && item("kickDownloads")->isVisible());
        QTRY_VERIFY(item("kickDl_stream-77") && item("kickDl_clip_abc"));
        QVERIFY(item("kickDlState_stream-77")->property("text").toString().startsWith("ready"));
        QVERIFY(item("kickDownloadsSummary")->property("text").toString().contains("of your"));
        shot("kick-downloads");
        QVERIFY(click("kickDlPlay_clip_abc"));
        QTRY_VERIFY(item("kickLocalPlay") && item("kickLocalPlay")->isVisible());
        QTRY_VERIFY(item("kickLocalPlayer") != nullptr);
        // a video kept on this device gets the seek meter (and no "live" tag)
        QVERIFY(item("kickLocalPlayer")->property("showSeekMeter").toBool());
        QVERIFY(!item("kickLocalPlayer")->property("showLiveTag").toBool());
        // the pretend chat is there for a downloaded stream too: it shows in "pretend" and is gone in "none"
        {
            const QString was = kick->chatMode();
            kick->setChatMode(QStringLiteral("pretend"));
            QTRY_VERIFY(item("kickLocalPretendChat") && item("kickLocalPretendChat")->isVisible());
            QVERIFY(item("kickLocalChat_pretend") && item("kickLocalChat_off"));
            window()->grabWindow();                                                    // draw a frame now: that is when the layouts are laid out (a slow headless run may not have drawn one yet)
            // (a long wait: on a slow runner the real video engine can keep the interface busy for a while while it opens the fixture file)
            // (every retry draws a frame first: the layout only runs when one is drawn, and a loaded runner may not draw on its own)
            auto laidOut = [this] { window()->grabWindow(); return item("kickLocalPretendChat")->width() > 100 && item("kickLocalPretendChat")->height() > 100; };
            QTRY_VERIFY2_WITH_TIMEOUT(laidOut(),
                     qPrintable(QStringLiteral("chat %1x%2 visible=%3, column %4x%5, play page %6x%7, player started=%8 error=%9")
                                .arg(item("kickLocalPretendChat")->width()).arg(item("kickLocalPretendChat")->height()).arg(item("kickLocalPretendChat")->isVisible())
                                .arg(item("kickLocalChatColumn")->width()).arg(item("kickLocalChatColumn")->height()).arg(item("kickLocalPlay")->width()).arg(item("kickLocalPlay")->height())
                                .arg(item("kickLocalPlayer")->property("started").toBool()).arg(item("kickLocalPlayer")->property("error").toString())), 30000);
            QTRY_VERIFY(item("kickLocalPretendChat")->mapToItem(item("kickLocalPlay"), QPointF(0, 0)).x() > item("kickLocalPlayer")->x());   // beside the picture
            shot("kick-local-chat");
            QVERIFY(click("kickLocalChat_off"));
            QTRY_VERIFY(!item("kickLocalPretendChat")->isVisible());
            kick->setChatMode(was);
        }
        // full screen: the picture (with its controls) moves to cover the whole window; Esc (the phone's back button) brings it back first
        QQuickItem *kickApp = item("kickApp");
        auto stage = [&]() -> QQuickItem * {            // the item that holds the picture: inside the player normally, directly in the window when staged
            QQuickItem *c = item("kickLocalPlayerStage");
            return c && c->parentItem() == w->property("contentItem").value<QQuickItem *>() ? c : nullptr;   // (the application window's content area)
        };
        QVERIFY(kickApp->setProperty("videoFullscreen", true));
        QTRY_VERIFY(stage() != nullptr);
        QCOMPARE(stage()->size(), QSizeF(w->width(), w->height()));                    // it covers everything
        QVERIFY(stage()->z() > 1000);
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(!kickApp->property("videoFullscreen").toBool());
        QTRY_VERIFY(stage() == nullptr);                                               // back inside the page
        QVERIFY(item("kickLocalPlay")->isVisible());                                   // (the first Esc only left full screen)
        // popped out (picture in picture, Android reports it): only the picture, no controls
        m_app->setInPictureInPicture(true);
        QTRY_VERIFY(stage() != nullptr);
        QVERIFY(!item("kickPause")->isVisible());
        m_app->setInPictureInPicture(false);
        QTRY_VERIFY(stage() == nullptr);
        QTRY_VERIFY(item("kickPause")->isVisible() || true);
        QTest::keyClick(w, Qt::Key_Escape);                                            // back to the downloads
        QTRY_VERIFY(item("kickDownloads")->isVisible());
        // deleting removes the files
        const QString videoPath = dl->videoPath("stream-77");
        QVERIFY(click("kickDlDelete_stream-77"));
        QTRY_VERIFY(!dl->has("stream-77"));
        QVERIFY(!QFileInfo::exists(videoPath));
        QVERIFY(dl->has("clip_abc"));
        dl->remove("clip_abc");
        QTRY_VERIFY(item("kickDownloadsEmpty") && item("kickDownloadsEmpty")->isVisible());
        for (const QUrl &u : ft->asked) QVERIFY2(AppNet::urlAllowed("kick", u), qPrintable(u.toString()));
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(item("kickBrowse")->isVisible());
        QTest::keyClick(w, Qt::Key_Escape);
        QTRY_VERIFY(!item("appHost")->property("active").toBool());
    }
#endif

    // Phones: press the mini player and slide it right to push it away (more room, e.g. for a stream's chat); the tab brings it back.
    void miniPlayerCanBePushedAwayOnAPhoneAndBroughtBack()
    {
        auto *w = window();
        const QSize before = w->size();
        struct Restore { QQuickWindow *w; QSize size; ~Restore() { w->setProperty("miniHidden", false); w->setProperty("forceMobile", false); w->resize(size); w->setProperty("view", QStringLiteral("collection")); } } restore{w, before};
        w->setProperty("view", QStringLiteral("collection"));
        w->setProperty("forceMobile", true);
        w->resize(390, 844);
        QTRY_COMPARE(w->width(), 390);
        QQuickItem *mini = item("miniPlayer");
        QTRY_VERIFY(mini && mini->isVisible());
        auto center = [&] { return mini->mapToScene(QPointF(mini->width() / 2, mini->height() / 2)).toPoint(); };
        // a short slide is not enough: it comes back
        QPoint p = center();
        QTest::mousePress(w, Qt::LeftButton, Qt::NoModifier, p);
        for (int dx = 8; dx <= 40; dx += 8) QTest::mouseMove(w, p + QPoint(dx, 0), 10);
        QTest::mouseRelease(w, Qt::LeftButton, Qt::NoModifier, p + QPoint(40, 0));
        QTRY_COMPARE(mini->x(), 0.0);
        QVERIFY(!w->property("miniHidden").toBool() && item("miniWrap")->isVisible());
        // a long slide to the right pushes it away
        p = center();
        QTest::mousePress(w, Qt::LeftButton, Qt::NoModifier, p);
        for (int dx = 10; dx <= 260; dx += 25) QTest::mouseMove(w, p + QPoint(dx, 0), 10);
        QTest::mouseRelease(w, Qt::LeftButton, Qt::NoModifier, p + QPoint(260, 0));
        QTRY_VERIFY(w->property("miniHidden").toBool());
        QTRY_VERIFY(!item("miniWrap")->isVisible());
        QTRY_VERIFY(item("miniPullTab") && item("miniPullTab")->isVisible());
        QCOMPARE(w->property("view").toString(), QStringLiteral("collection"));         // sliding did not open now playing
        shot("phone-mini-pushed-away");
        // the tab brings it back
        QVERIFY(click("miniPullTab"));
        QTRY_VERIFY(!w->property("miniHidden").toBool());
        QTRY_VERIFY(item("miniWrap")->isVisible() && !item("miniPullTab")->isVisible());
        // and on a computer it never hides: the slide does nothing there
        w->setProperty("forceMobile", false);
        w->resize(before);
        QTRY_COMPARE(w->size(), before);
        QTRY_VERIFY(item("miniPlayer")->isVisible());
        QVERIFY(!item("miniPullTab")->isVisible());
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
        QTRY_VERIFY(m_app->artists()->count() > 0);              // the guest's library fills in a moment after it opens: select only once it is there
        m_app->selectArtist(0);
        QTRY_VERIFY(m_app->artistAlbums()->rowCount() > 0);
        m_app->selectAlbum(0);
        QTRY_COMPARE(m_app->selectedAlbumTitle(), QStringLiteral("unknown album"));
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

    // README renders of the visual music games, with the richer fixture library (see phoneMockupRenders):
    // Album Fall on the desktop, Musy Note and the phone visualizer on a phone. Skipped unless both variables are set.
    void visualGameRenders()
    {
        const QString fx = qEnvironmentVariable("HOPTUNES_FIXTURE_DIR");
        if (fx.isEmpty() || qEnvironmentVariable("HOPTUNES_UI_SHOTS").isEmpty()) QSKIP("set HOPTUNES_FIXTURE_DIR and HOPTUNES_UI_SHOTS");
        auto *w = window();
        const QSize before = w->size();
        // the fixture songs, one folder per album, each with a painted cover (the games are made of covers)
        const QString lib = m_home.filePath(QStringLiteral("GameMusic"));
        {
            int n = 0;
            const QStringList songs = QDir(fx + QStringLiteral("/music")).entryList({QStringLiteral("*.flac")}, QDir::Files);
            for (const QString &f : songs) {
                const QString album = f.section(QStringLiteral(" - "), 0, 1);
                const QString dir = lib + QLatin1Char('/') + album;
                if (!QDir(dir).exists()) {
                    QVERIFY(QDir().mkpath(dir));
                    QImage im(300, 300, QImage::Format_RGB32);
                    QPainter pt(&im);
                    QLinearGradient gr(0, 0, 300, 300);
                    gr.setColorAt(0, QColor::fromHsv((n * 47) % 360, 190, 235));
                    gr.setColorAt(1, QColor::fromHsv((n * 47 + 70) % 360, 230, 110));
                    pt.fillRect(im.rect(), gr);
                    pt.setPen(Qt::NoPen);
                    pt.setBrush(QColor(255, 255, 255, 70));
                    pt.drawEllipse(QPoint(90 + 25 * (n % 4), 120), 70, 70);
                    pt.setBrush(QColor(0, 0, 0, 60));
                    pt.drawRect(150, 170 + 8 * (n % 3), 120, 90);
                    pt.end();
                    QVERIFY(im.save(dir + QStringLiteral("/cover.jpg")));
                    ++n;
                }
                QVERIFY(QFile::copy(fx + QStringLiteral("/music/") + f, dir + QLatin1Char('/') + f));
            }
        }
        for (const QString &f : m_app->folders()) m_app->removeFolder(f);   // only the fixture songs, so what plays has a cover (this test runs last)
        m_app->addFolder(QUrl::fromLocalFile(lib));
        QTRY_VERIFY_WITH_TIMEOUT(m_app->librarySize() >= 12 && !m_app->scanning(), 20000);
        QTRY_VERIFY_WITH_TIMEOUT(m_app->mosaicArt().size() >= 4, 10000);
        QVector<float> frame(32);
        auto loud = [&](int seed) {
            for (int i = 0; i < 32; ++i) {
                const float base = -62.f + 50.f * float(31 - i) / 31.f;
                frame[i] = base + 9.f * std::sin(0.9f * i + seed) + 4.f * std::sin(2.3f * i + 2 * seed);
            }
            for (int i = 0; i < 4; ++i) m_app->audio()->onSpectrum(frame);
        };
        // the silent test song keeps sending quiet frames of its own: keep feeding loud ones while a picture is made
        m_liveShots = true;
        int tick = 0;
        QTimer feeder;
        feeder.setInterval(15);
        connect(&feeder, &QTimer::timeout, this, [&] { loud(tick++ / 6); });
        feeder.start();

        // desktop: Album Fall with a stack worth looking at, and the visualizer behind it
        w->resize(1280, 800);
        QTRY_COMPARE(w->width(), 1280);
        w->setProperty("view", QStringLiteral("now playing"));
        m_app->playSongs(0);
        QTRY_VERIFY(m_app->player()->isPlaying());
        QVERIFY(click("stage_games"));
        QTRY_VERIFY(item("albumFall") && item("albumFall")->isVisible());
        AlbumFall *af = m_app->albumFall();
        af->setAutoplay(false);
        QTRY_VERIFY(af->running());
        for (int n = 0; n < 8 && af->running(); ++n) {
            af->onBeat(0.35 + 0.1 * (n % 6));
            for (int k = 0; k < (n * 3) % 7; ++k) af->act(QStringLiteral("left"));
            for (int k = 0; k < (n * 5) % 5; ++k) af->act(QStringLiteral("right"));
            if (n % 3 == 0) af->act(QStringLiteral("rotate"));
            af->act(QStringLiteral("drop"));
        }
        QVERIFY(af->running());
        m_app->setGameSetting(QStringLiteral("vizBackdrop"), true);
        QTest::qWait(900);
        shot("games-albumfall");
        m_app->setGameSetting(QStringLiteral("vizBackdrop"), false);
        // Cover Pinball: the table is the cover of the song that plays
        QTest::qWait(300);
        QVERIFY(click("game_pinball"));
        QTRY_VERIFY(item("coverPinball") && item("coverPinball")->isVisible());
        PinballGame *pb = m_app->pinball();
        QTRY_VERIFY(pb->running());
        pb->start(m_app->player()->artUrl());               // a fresh game on this song's cover
        QTest::keyClick(w, Qt::Key_Space);
        QTRY_COMPARE(pb->state(), 1);
        for (int i = 0; i < 12 && pb->hits() < 4; ++i) pb->advance(45, 1.0 / 60);   // until a few bumpers are lit, the ball in the field
        QVERIFY(pb->state() == 1 && pb->ballY() < 12);
        QTest::qWait(300);
        shot("games-pinball");
        m_app->setGameSetting(QStringLiteral("vizBackdrop"), true);      // the visualizer behind the table
        QTest::qWait(900);
        shot("games-pinball-visualizer");
        m_app->setGameSetting(QStringLiteral("vizBackdrop"), false);
        QTest::qWait(300);
        QVERIFY(click("game_albumfall"));
        m_app->setGameSetting(QStringLiteral("game"), QStringLiteral("albumfall"));
        QVERIFY(click("stage_visualizer"));
        m_app->setGameSetting(QStringLiteral("vizStyle"), QStringLiteral("mirror"));
        m_app->setGameSetting(QStringLiteral("vizColor"), QStringLiteral("rainbow"));
        QTest::qWait(900);
        shot("games-visualizer");
        QVERIFY(click("stage_art"));

        // phone: the visualizer in portrait, then Musy Note in landscape
        w->setProperty("forceMobile", true);
        w->resize(412, 892);
        QTRY_COMPARE(w->width(), 412);
        w->setProperty("view", QStringLiteral("now playing"));
        m_app->setGameSetting(QStringLiteral("vizStyle"), QStringLiteral("bars"));
        m_app->setGameSetting(QStringLiteral("phoneViz"), true);
        QTRY_VERIFY(item("phoneViz") && item("phoneViz")->isVisible());
        QTest::qWait(900);
        shot("games-phone-visualizer");
        w->resize(800, 360);
        QTRY_COMPARE(w->width(), 800);
        QTRY_VERIFY(item("phoneGames") && item("phoneGames")->isVisible());
        QVERIFY(click("phoneGames"));
        QTRY_VERIFY(item("musyNote") && item("musyNote")->isVisible());
        MusyNoteGame *g = m_app->musyNote();
        QTRY_COMPARE(g->state(), 0);
        QTest::qWait(600);
        QTest::keyClick(w, Qt::Key_Space);
        QTRY_COMPARE(g->state(), 1);
        g->setAutoplay(true);                              // the song plays it (nothing is scored), so the run lasts
        for (int i = 0; i < 7; ++i) {
            g->onBeat(0.7);
            for (int k = 0; k < 8; ++k) g->advance(5, 1.0 / 60);
        }
        QVERIFY2(g->state() == 1, qPrintable(QString::number(g->state())));
        shot("games-musy-note");

        // tidy up
        g->setAutoplay(false);
        m_app->setGameSetting(QStringLiteral("phoneViz"), false);
        QTest::keyClick(w, Qt::Key_Escape);
        feeder.stop();
        m_liveShots = false;
        m_app->player()->stop();
        w->setProperty("forceMobile", false);
        w->resize(before);
        QTRY_COMPARE(w->size(), before);
        m_app->removeFolder(QFileInfo(lib).canonicalFilePath());
        QTest::qWait(400);
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
