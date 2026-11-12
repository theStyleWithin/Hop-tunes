// SPDX-License-Identifier: GPL-3.0-or-later
// The gateway that apps reach the internet through, and the Kick app's data side, answered from fixtures (no real network).
#include "apps/KickClient.h"
#include "apps/WebpDecode.h"
#include "net/AppNet.h"

#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest>

using namespace hop;

namespace {

// Answers from a function, a moment later (like a real reply), and remembers what was asked.
class FakeTransport : public NetTransport {
public:
    std::function<NetReply(const QUrl &)> handler;
    QList<QUrl> asked;
    QList<QByteArray> agents;
    int aborted = 0;
    void get(const QUrl &url, const QByteArray &ua, const QByteArray &, Done done) override
    {
        asked << url;
        agents << ua;
        NetReply r = handler ? handler(url) : NetReply{};
        QTimer::singleShot(0, this, [done, r] { done(r); });
    }
    void abortAll() override { ++aborted; }
};

NetReply ok(const QByteArray &json) { NetReply r; r.status = 200; r.body = json; return r; }

QString firstSlug(const KickClient &k) { const QVariantList l = k.streams(); return l.isEmpty() ? QString() : l.first().toMap().value("slug").toString(); }

QByteArray streamsJson(const QStringList &slugs, const QString &next, int viewersFrom = 5000, const QStringList &mature = {})
{
    QJsonArray a;
    int v = viewersFrom;
    for (const QString &s : slugs) {
        a.append(QJsonObject{{"id", 1}, {"title", "title of " + s}, {"viewer_count", v}, {"show_view_count", true},
                             {"thumbnail", QJsonObject{{"src", "https://images.kick.com/video_thumbnails/x/" + s + "/720.webp"}}},
                             {"channel", QJsonObject{{"id", 7}, {"slug", s}, {"username", s.toUpper()}, {"profile_pic", "https://files.kick.com/images/user/" + s + ".webp"}}},
                             {"category", QJsonObject{{"id", 15}, {"name", "Just Chatting"}, {"slug", "just-chatting"}}},
                             {"language", "en"}, {"is_mature", mature.contains(s)}});
        v -= 100;
    }
    return QJsonDocument(QJsonObject{{"data", QJsonObject{{"livestreams", a}, {"pagination", QJsonObject{{"next_cursor", next}}}}}, {"message", "Success"}}).toJson();
}

QByteArray channelJson(const QString &slug, bool live, bool mature = false)
{
    QJsonObject o{{"id", 3}, {"slug", slug}, {"playback_url", "https://playback.live-video.net/x.m3u8"}, {"followers_count", 12},
                  {"user", QJsonObject{{"username", slug}, {"profile_pic", "https://files.kick.com/p.webp"}}},
                  {"chatroom", QJsonObject{{"id", 668}}}};
    o["livestream"] = live ? QJsonValue(QJsonObject{{"session_title", "a title"}, {"viewer_count", 1234}, {"is_mature", mature},
                                                    {"thumbnail", QJsonObject{{"url", "https://stream.kick.com/thumbnails/x.jpg"}}},
                                                    {"categories", QJsonArray{QJsonObject{{"name", "Music"}}}}}) : QJsonValue();
    return QJsonDocument(o).toJson();
}

} // namespace

class TestNet : public QObject {
    Q_OBJECT
    AppNet net;
    FakeTransport *ft = nullptr;
    void fresh()
    {
        auto t = std::make_unique<FakeTransport>();
        ft = t.get();
        net.setTransport(std::move(t));
        net.setAllowed("kick", true);
        net.open("kick");
    }
private slots:
    void init() { net.close("kick"); net.setAllowed("kick", false); }

    void addressRules()
    {
        QString why;
        QVERIFY(AppNet::urlAllowed("kick", QUrl("https://kick.com/api/v2/channels/x"), &why));
        QVERIFY(AppNet::urlAllowed("kick", QUrl("https://web.kick.com/api/v1/livestreams?limit=1"), &why));
        QVERIFY(AppNet::urlAllowed("kick", QUrl("https://images.kick.com/a.webp"), &why));
        QVERIFY(AppNet::urlAllowed("kick", QUrl("https://stream.kick.com/thumbnails/x.jpg"), &why));
        QVERIFY(!AppNet::urlAllowed("kick", QUrl("http://kick.com/"), &why));                 // not secure
        QVERIFY(!AppNet::urlAllowed("kick", QUrl("https://kick.com:8443/"), &why));           // not the standard port
        QVERIFY(!AppNet::urlAllowed("kick", QUrl("https://user:pw@kick.com/"), &why));        // a user name
        QVERIFY(!AppNet::urlAllowed("kick", QUrl("https://evil.example/"), &why));            // another site
        QVERIFY(!AppNet::urlAllowed("kick", QUrl("https://kick.com.evil.example/"), &why));   // look-alike
        QVERIFY(!AppNet::urlAllowed("kick", QUrl("https://notkick.com/"), &why));
        QVERIFY(!AppNet::urlAllowed("kick", QUrl("file:///etc/passwd"), &why));
        QVERIFY(!AppNet::urlAllowed("someapp", QUrl("https://kick.com/"), &why));             // an app with no declared sites
    }

    void nothingGoesOutUnlessAllowedAndOpen()
    {
        auto t = std::make_unique<FakeTransport>();
        ft = t.get();
        net.setTransport(std::move(t));
        ft->handler = [](const QUrl &) { return ok("{}"); };
        QString err;
        net.get("kick", QUrl("https://kick.com/x"), [&](NetReply r) { err = r.error; });   // not allowed, not open
        QVERIFY(err.contains("not allowed"));
        net.setAllowed("kick", true);
        err.clear();
        net.get("kick", QUrl("https://kick.com/x"), [&](NetReply r) { err = r.error; });   // allowed, not open
        QVERIFY(err.contains("not open"));
        net.open("kick");
        err = "unset";
        net.get("kick", QUrl("https://evil.example/x"), [&](NetReply r) { err = r.error; });
        QVERIFY(err.contains("not one of the sites"));
        QCOMPARE(ft->asked.size(), 0);                                                      // nothing was sent in any of these cases
        bool got = false;
        net.get("kick", QUrl("https://kick.com/x"), [&](NetReply r) { got = r.error.isEmpty(); });
        QTRY_VERIFY(got);
        QCOMPARE(ft->asked.size(), 1);
        QCOMPARE(ft->agents.first(), QByteArray("Mozilla/5.0"));                            // a generic user agent, nothing about Hop-tunes
        QCOMPARE(net.contacted("kick"), QStringList{"kick.com"});
        net.setAllowed("kick", false);                                                       // taking the permission back closes it
        QVERIFY(!net.isOpen("kick"));
        QVERIFY(net.contacted("kick").isEmpty());
    }

    void redirectsStayOnTheList()
    {
        fresh();
        ft->handler = [](const QUrl &u) {
            NetReply r;
            if (u.path() == "/a") { r.status = 302; r.location = QUrl("https://images.kick.com/b"); return r; }          // fine
            if (u.path() == "/c") { r.status = 302; r.location = QUrl("https://evil.example/steal"); return r; }         // refused
            if (u.path() == "/d") { r.status = 301; r.location = QUrl("http://kick.com/e"); return r; }                  // downgrade refused
            if (u.path() == "/loop") { r.status = 302; r.location = QUrl("https://kick.com/loop"); return r; }           // too many
            return ok("fine");
        };
        QString e1, e2, e3, e4; QByteArray b1;
        net.get("kick", QUrl("https://kick.com/a"), [&](NetReply r) { e1 = r.error; b1 = r.body; });
        net.get("kick", QUrl("https://kick.com/c"), [&](NetReply r) { e2 = r.error.isEmpty() ? "none" : r.error; });
        net.get("kick", QUrl("https://kick.com/d"), [&](NetReply r) { e3 = r.error.isEmpty() ? "none" : r.error; });
        net.get("kick", QUrl("https://kick.com/loop"), [&](NetReply r) { e4 = r.error.isEmpty() ? "none" : r.error; });
        QTRY_VERIFY(!e2.isEmpty() && !e3.isEmpty() && !e4.isEmpty() && !b1.isEmpty());
        QVERIFY(e1.isEmpty()); QCOMPARE(b1, QByteArray("fine"));
        QVERIFY(e2.contains("redirect refused")); QVERIFY(e3.contains("redirect refused")); QVERIFY(e4.contains("too many"));
        for (const QUrl &u : ft->asked) QVERIFY2(AppNet::urlAllowed("kick", u), qPrintable(u.toString()));   // nothing off the list was ever asked
    }

    void closingDropsLateAnswers()
    {
        fresh();
        ft->handler = [](const QUrl &) { return ok("late"); };
        bool delivered = false;
        net.get("kick", QUrl("https://kick.com/x"), [&](NetReply) { delivered = true; });
        net.close("kick");                                                                  // before the answer arrives
        QTest::qWait(50);
        QVERIFY(!delivered);
        QVERIFY(ft->aborted >= 1);
    }

    void onlyTheGatewayTouchesTheNetwork()
    {
        // No other code in Hop-tunes opens a connection: the words for it appear only in src/net and src/apps.
        const QStringList words = {"QNetworkAccessManager", "QNetworkRequest", "QNetworkReply", "QTcpSocket", "QSslSocket", "QUdpSocket",
                                   "QWebSocket", "QtNetwork", "XMLHttpRequest", "QHttpServer", "QLocalSocket", "WebSocket {", "WebView"};
        const QString root = QStringLiteral(HOPTUNES_SOURCE_DIR) + "/src";
        int scanned = 0;
        QDirIterator it(root, {"*.cpp", "*.h", "*.qml", "*.js"}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QString rel = path.mid(root.size() + 1);
            if (rel.startsWith("net/") || rel.startsWith("apps/")) continue;
            QFile f(path);
            QVERIFY(f.open(QIODevice::ReadOnly));
            const QString body = QString::fromUtf8(f.readAll());
            ++scanned;
            for (const QString &w : words) QVERIFY2(!body.contains(w), qPrintable(rel + " uses " + w));
        }
        QVERIFY(scanned > 50);
    }

    // ---- the Kick app's data ----

    void readsTheLiveListAndLeavesOutBlockedAndAdultStreams()
    {
        fresh();
        KickClient k(&net);
        ft->handler = [](const QUrl &u) {
            if (u.host() == "web.kick.com") return ok(streamsJson({"alpha", "bravo", "charlie", "delta"}, "CUR2", 9000, {"charlie"}));
            return ok("{\"data\":[]}");
        };
        k.open();
        QTRY_COMPARE(k.streams().size(), 3);                                       // charlie is 18+ and hidden by default
        QCOMPARE(k.streams().first().toMap().value("slug").toString(), QString("alpha"));
        QCOMPARE(k.streams().first().toMap().value("viewersText").toString(), QString("9.0k"));
        QVERIFY(k.hasMore());
        QVERIFY(k.streams().first().toMap().value("thumb").toString().startsWith("https://images.kick.com/"));
        k.setShowMature(true);
        QCOMPARE(k.streams().size(), 4);
        k.setShowMature(false);
        QVERIFY(k.block("Bravo"));                                                 // blocking hides it at once, without asking Kick again
        const int asked = ft->asked.size();
        QCOMPARE(k.streams().size(), 2);
        QCOMPARE(ft->asked.size(), asked);
        QVERIFY(k.isBlocked("bravo"));
        k.unblock("bravo");
        QCOMPARE(k.streams().size(), 3);
        // the request carried the page size, the sort and nothing else about the person
        const QUrl first = ft->asked.at(1);
        QVERIFY(first.toString().contains("limit=36") && first.toString().contains("sort=viewer_count_desc"));
        QVERIFY(!first.toString().contains("after="));
    }

    void showMoreAsksForTheNextPageOnlyWhenAsked()
    {
        fresh();
        KickClient k(&net);
        ft->handler = [](const QUrl &u) {
            if (u.host() != "web.kick.com") return ok("{\"data\":[]}");
            if (u.query().contains("after=CUR2")) return ok(streamsJson({"echo", "foxtrot"}, "", 4000));
            return ok(streamsJson({"alpha", "bravo"}, "CUR2"));
        };
        k.open();
        QTRY_COMPARE(k.streams().size(), 2);
        QTest::qWait(300);
        int listCalls = 0; for (const QUrl &u : ft->asked) if (u.host() == "web.kick.com") ++listCalls;
        QCOMPARE(listCalls, 1);                                                    // nothing loaded the next page by itself
        k.loadMore();
        QTRY_COMPARE(k.streams().size(), 4);
        QVERIFY(!k.hasMore());
        QVERIFY(ft->asked.last().toString().contains("after=CUR2"));
    }

    void aPageOfBlockedStreamsDoesNotLookLikeADeadEnd()
    {
        fresh();
        KickClient k(&net);
        k.setListsJson(QJsonObject{{"blocked", QJsonArray{"alpha", "bravo"}}});
        ft->handler = [](const QUrl &u) {
            if (u.host() != "web.kick.com") return ok("{\"data\":[]}");
            if (u.query().contains("after=P2")) return ok(streamsJson({"golf"}, ""));
            return ok(streamsJson({"alpha", "bravo"}, "P2"));
        };
        k.open();
        QTRY_COMPARE(k.streams().size(), 1);                                       // the first page was all blocked: the next one came in
        QCOMPARE(k.streams().first().toMap().value("slug").toString(), QString("golf"));
    }

    void followBlockAndTheirStorage()
    {
        KickClient k(&net);
        QSignalSpy edited(&k, &KickClient::listsEdited);
        QCOMPARE(KickClient::normalize("https://kick.com/Some_Name?x=1"), QString("some_name"));
        QCOMPARE(KickClient::normalize("www.kick.com/abc/videos"), QString("abc"));
        QCOMPARE(KickClient::normalize("@Zed"), QString("zed"));
        QVERIFY(KickClient::normalize("not a name!").isEmpty());
        QVERIFY(KickClient::normalize("https://evil.example/abc").isEmpty());
        QVERIFY(k.follow("https://kick.com/NightOwl")); QVERIFY(!k.follow("nightowl"));
        QVERIFY(k.block("loudone")); QVERIFY(!k.block("loudone"));
        QVERIFY(k.isFollowing("nightowl")); QVERIFY(k.isBlocked("loudone"));
        QVERIFY(k.follow("loudone"));                                              // following someone lifts the block
        QVERIFY(!k.isBlocked("loudone"));
        QVERIFY(k.block("loudone"));                                               // and blocking unfollows
        QVERIFY(!k.isFollowing("loudone"));
        QVERIFY(edited.size() >= 4);
        const QJsonObject saved = k.listsJson();
        KickClient other(&net);
        other.setListsJson(saved);                                                 // comes back with the profile
        QCOMPARE(other.listsJson(), saved);
        QCOMPARE(other.blocked(), QStringList{"loudone"});
        QCOMPARE(other.following().size(), 1);
        other.setListsJson(QJsonObject{{"following", QJsonArray{"a", "b!", 5, "a"}}, {"blocked", QJsonArray{"a"}}});   // junk in the file is ignored
        QCOMPARE(other.following().size(), 0);                                     // "a" is blocked, "b!" is not a name
        QCOMPARE(other.blocked(), QStringList{"a"});
    }

    void followingShowsLiveDotsOnlyWhenAsked()
    {
        fresh();
        KickClient k(&net);
        k.follow("liveone"); k.follow("quiet");
        ft->handler = [](const QUrl &u) {
            if (u.host() == "kick.com" && u.path().endsWith("/liveone")) return ok(channelJson("liveone", true));
            if (u.host() == "kick.com" && u.path().endsWith("/quiet")) return ok(channelJson("quiet", false));
            return ok(streamsJson({}, ""));
        };
        k.open();                                                                  // opening asks once per followed channel
        QTRY_VERIFY(k.following().first().toMap().value("checked").toBool() && k.following().last().toMap().value("checked").toBool());
        QVERIFY(k.following().first().toMap().value("live").toBool());
        QVERIFY(!k.following().last().toMap().value("live").toBool());
        QCOMPARE(k.following().first().toMap().value("viewers").toInt(), 1234);
        const int n = ft->asked.size();
        QTest::qWait(300);
        QCOMPARE(ft->asked.size(), n);                                             // no polling in the background
    }

    void aChannelOpensWithItsStreamAndChat()
    {
        fresh();
        KickClient k(&net);
        ft->handler = [](const QUrl &u) {
            if (u.path().endsWith("/gone")) { NetReply r; r.status = 404; r.error = "the site answered 404"; return r; }
            if (u.path().endsWith("/offline")) return ok(channelJson("offline", false));
            if (u.host() == "kick.com") return ok(channelJson("tune", true));
            return ok("{\"data\":[]}");
        };
        k.open();
        k.openChannel("https://kick.com/Tune");
        QTRY_VERIFY(k.channel().value("live").toBool());
        QCOMPARE(k.channel().value("playbackUrl").toString(), QString("https://playback.live-video.net/x.m3u8"));
        QCOMPARE(k.channel().value("chatroomId").toInt(), 668);
        QCOMPARE(k.channel().value("title").toString(), QString("a title"));
        QCOMPARE(k.channel().value("category").toString(), QString("Music"));
        k.openChannel("offline");
        QTRY_VERIFY(!k.channel().value("loading").toBool());
        QVERIFY(!k.channel().value("live").toBool());
        QVERIFY(k.channel().value("playbackUrl").toString().isEmpty());            // an offline channel has nothing to play
        k.openChannel("gone");
        QTRY_VERIFY(k.channel().value("notFound").toBool());
        k.openChannel("not a name!");
        QVERIFY(k.error().contains("not a kick channel"));
    }

    void failuresAreSaidInPlainWords()
    {
        fresh();
        KickClient k(&net);
        ft->handler = [](const QUrl &) { NetReply r; r.error = "could not connect: no route"; return r; };
        k.open();
        QTRY_VERIFY(!k.error().isEmpty());
        QVERIFY(k.error().contains("could not connect"));
        ft->handler = [](const QUrl &u) { return u.host() == "web.kick.com" ? ok("<html>blocked</html>") : ok("{}"); };
        k.refresh();
        QTRY_VERIFY(k.error().contains("not in a form this version understands"));
        net.setAllowed("kick", false);                                              // permission taken back
        KickClient k2(&net);
        k2.open();
        QVERIFY(k2.error().contains("not allowed"));
        QVERIFY(k2.streams().isEmpty());
    }

    void chatIsNoneOrPretendUnlessSignedIn()
    {
        KickClient k(&net);
        QCOMPARE(k.chatMode(), QString("off"));                                    // the start: no chat
        QVERIFY(!k.signedIn());
        k.setChatMode("kick");                                                     // Kick's own chat needs a sign-in the person chose
        QCOMPARE(k.chatMode(), QString("off"));
        k.setChatMode("whatever");
        QCOMPARE(k.chatMode(), QString("off"));
        QSignalSpy edited(&k, &KickClient::listsEdited);
        k.setChatMode("pretend");
        QCOMPARE(k.chatMode(), QString("pretend"));
        QCOMPARE(edited.size(), 1);
        QCOMPARE(k.listsJson().value("chatMode").toString(), QString("pretend"));
        KickClient other(&net);
        other.setListsJson(QJsonObject{{"chatMode", "pretend"}});
        QCOMPARE(other.chatMode(), QString("pretend"));
        other.setListsJson(QJsonObject{{"chatMode", "kick"}});                     // a saved "kick" without a sign-in falls back to none
        QCOMPARE(other.chatMode(), QString("off"));
        other.setListsJson(QJsonObject{{"chatMode", 5}});
        QCOMPARE(other.chatMode(), QString("off"));
    }

    void theDevelopersLiveStreamTakesTheFirstCardOfTheDefaultFeed()
    {
        fresh();
        KickClient k(&net);
        bool devLive = true, devMature = false;
        ft->handler = [&](const QUrl &u) {
            if (u.host() == "web.kick.com") return ok(streamsJson({"alpha", "gamehopper", "bravo"}, ""));
            if (u.host() == "kick.com" && u.path().endsWith("/gamehopper")) return ok(channelJson("gamehopper", devLive, devMature));
            return ok("{\"data\":[]}");
        };
        k.open();
        QTRY_COMPARE(firstSlug(k), QString("gamehopper"));                         // first card, even though Kick listed it second
        QCOMPARE(k.streams().size(), 3);                                           // and not twice
        const QVariantMap card = k.streams().first().toMap();
        QVERIFY(card.value("developer").toBool());                                 // labelled on the card
        QCOMPARE(card.value("thumb").toString(), QString("https://stream.kick.com/thumbnails/x.jpg"));
        QVERIFY(!k.streams().at(1).toMap().value("developer").toBool());
        for (const QUrl &u : ft->asked) QVERIFY2(AppNet::urlAllowed("kick", u), qPrintable(u.toString()));
        // not in a search, a category or a language: those lists are what the person asked for
        k.setCategoryId(15);
        QTRY_COMPARE(firstSlug(k), QString("alpha"));
        k.setCategoryId(0);
        QTRY_COMPARE(firstSlug(k), QString("gamehopper"));
        k.setLanguage("en");
        QTRY_COMPARE(firstSlug(k), QString("alpha"));
        k.setLanguage("");
        QTRY_COMPARE(firstSlug(k), QString("gamehopper"));
        // the viewer's own choices win: a block, and 18+ hidden
        QVERIFY(k.block("gamehopper"));
        QCOMPARE(firstSlug(k), QString("alpha"));
        k.unblock("gamehopper");
        QCOMPARE(firstSlug(k), QString("gamehopper"));
        devMature = true;
        k.refresh();
        QTRY_COMPARE(firstSlug(k), QString("alpha"));                              // 18+ stream and 18+ hidden: not shown
        k.setShowMature(true);
        QTRY_COMPARE(firstSlug(k), QString("gamehopper"));
        k.setShowMature(false);
        // offline: no card, and no other channel can take it
        devMature = false; devLive = false;
        k.refresh();
        QTRY_COMPARE(firstSlug(k), QString("alpha"));
        QCOMPARE(k.streams().size(), 3);
        QVERIFY(!k.streams().at(1).toMap().value("developer").toBool());
    }

    void onlyTheDevelopersChannelCanTakeThatCard()
    {
        fresh();
        KickClient k(&net);
        ft->handler = [](const QUrl &u) {
            if (u.host() == "web.kick.com") return ok(streamsJson({"alpha", "bravo"}, ""));
            if (u.host() == "kick.com") return ok(channelJson("someoneelse", true));    // whatever the lookup answers, it is not the developer
            return ok("{\"data\":[]}");
        };
        k.open();
        QTRY_COMPARE(k.streams().size(), 2);
        QTest::qWait(100);
        QCOMPARE(firstSlug(k), QString("alpha"));
    }

    void searchAndFilters()
    {
        fresh();
        KickClient k(&net);
        QStringList urls;
        ft->handler = [&urls](const QUrl &u) {
            urls << u.toString();
            if (u.path() == "/api/search") return ok("{\"channels\":[{\"slug\":\"lofi_one\",\"isLive\":true,\"followers_count\":99,\"user\":{\"username\":\"LofiOne\",\"profilePic\":\"https://files.kick.com/x.webp\"}},{\"slug\":\"bad name!\"}],\"categories\":[]}");
            if (u.path().startsWith("/api/v1/subcategories")) return ok("{\"data\":[{\"id\":15,\"name\":\"Just Chatting\",\"slug\":\"just-chatting\"},{\"id\":3,\"name\":\"Fortnite\",\"slug\":\"fortnite\"},{\"id\":0,\"name\":\"bad\"}]}");
            return ok(streamsJson({"alpha"}, ""));
        };
        k.open();
        QTRY_COMPARE(k.categories().size(), 2);
        k.setCategoryId(15); k.setLanguage("EN"); k.setSort("featured");
        QTRY_VERIFY(!k.loading());
        const QString last = urls.last();
        QVERIFY(last.contains("category_id=15") && last.contains("language=en") && last.contains("sort=featured"));
        k.search("lofi");
        QTRY_COMPARE(firstSlug(k), QString("lofi_one"));
        QCOMPARE(k.streams().size(), 1);                                           // the nameless "bad name!" entry was dropped
        QVERIFY(urls.last().contains("searched_word=lofi"));
        k.search("");
        QTRY_COMPARE(firstSlug(k), QString("alpha"));
        k.close();
        QVERIFY(k.streams().isEmpty());
        QVERIFY(!net.isOpen("kick"));
    }
    // Kick's pictures are all WebP: they must be readable without Qt's optional WebP plugin (not in the Android app or every Linux install)
    void webpPicturesAreReadWithoutQtsPlugin()
    {
        if (!webpDecoderBuilt()) QSKIP("built without libwebp");
        const QByteArray webp = QByteArray::fromBase64("UklGRkoAAABXRUJQVlA4ID4AAAAwAwCdASogABIAPjEYikOiIaEUBAAgAwSygDsAfgAAQZQQAP70UR//+YJX5Bu1D//84i/OIvziL/zcAAAAAA==");        // 32x18, a red picture
        const QImage img = decodeWebp(webp);
        QVERIFY(!img.isNull());
        QCOMPARE(img.size(), QSize(32, 18));
        const QColor c = img.pixelColor(16, 9);
        QVERIFY(c.red() > 170 && c.green() < 80 && c.blue() < 80);
        QVERIFY(decodeWebp(QByteArray("not a picture at all")).isNull());     // garbage and truncated files are refused, never a crash
        QVERIFY(decodeWebp(webp.left(20)).isNull());
        QVERIFY(decodeWebp(QByteArray()).isNull());
    }
};

QTEST_MAIN(TestNet)
#include "test_net.moc"
