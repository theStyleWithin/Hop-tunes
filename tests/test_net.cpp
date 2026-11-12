// SPDX-License-Identifier: GPL-3.0-or-later
// The gateway that apps reach the internet through, and the Kick app's data side, answered from fixtures (no real network).
#include "apps/Hls.h"
#include "apps/KickClient.h"
#include "apps/OfflineDownloads.h"
#include "apps/OfflineStore.h"
#include "apps/WebpDecode.h"
#include "net/AppNet.h"

#include <QDirIterator>
#include <QNetworkProxyFactory>
#include <QTemporaryDir>
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
    int delayMs = 0;
    NetLimits lastLimits;
    QList<NetLimits> limitsAsked;
    void get(const QUrl &url, const QByteArray &ua, const QByteArray &, NetLimits limits, Done done) override
    {
        lastLimits = limits;
        limitsAsked << limits;
        asked << url;
        agents << ua;
        NetReply r = handler ? handler(url) : NetReply{};
        QTimer::singleShot(delayMs, this, [done, r] { done(r); });
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
        // the video host family (live streams' playlists and pieces), and nothing that only looks like it
        QVERIFY(AppNet::urlAllowed("kick", QUrl("https://fa72.us-west-2.playback.live-video.net/api/video/v1/x.m3u8?token=1"), &why));
        QVERIFY(AppNet::urlAllowed("kick", QUrl("https://live-video.net/x"), &why));
        QVERIFY(!AppNet::urlAllowed("kick", QUrl("http://a.live-video.net/x"), &why));
        QVERIFY(!AppNet::urlAllowed("kick", QUrl("https://live-video.net.evil.example/x"), &why));
        QVERIFY(!AppNet::urlAllowed("kick", QUrl("https://evillive-video.net/x"), &why));
        QVERIFY(!AppNet::urlAllowed("kick", QUrl("https://a.live-video.net:8443/x"), &why));
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
    // ---- the room set aside for offline Kick ----
    static void writeFile(const QString &path, qint64 bytes)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(QByteArray(int(bytes), 'x'));
    }
    void offlineStoreKeepsALimitTheUserSets()
    {
        QTemporaryDir tmp;
        OfflineStore o;
        o.setDefaultBase(tmp.filePath("offline"));
        o.setProfileId("p1");
        o.setFreeProbe([](const QString &) { return qint64(100) * 1024 * 1024 * 1024; });
        QCOMPARE(o.limitMB(), 5.0 * 1024);                                   // 5 GB to start with
        QVERIFY(!o.setLimitMB(10));                                          // below the smallest limit: refused, nothing changes
        QCOMPARE(o.limitMB(), 5.0 * 1024);
        QVERIFY(!o.setLimitMB(-3));
        QVERIFY(o.setLimitGB(2));
        QCOMPARE(o.limitMB(), 2048.0);
        QVERIFY(o.setLimitMB(50));                                           // down to the smallest, any time
        QVERIFY(o.setLimitMB(80));
        // used space is what is in the folder; the meter and the warning follow it
        QCOMPARE(o.usedBytes(), qint64(0));
        writeFile(o.folder() + "/a/one.ts", 30 * OfflineStore::kMB);
        writeFile(o.folder() + "/two.ts", 30 * OfflineStore::kMB);
        o.refresh();
        QCOMPARE(o.usedBytes(), 60 * OfflineStore::kMB);
        QVERIFY(!o.nearLimit() && !o.full());                                // 60 of 80 is 75%: no warning yet
        writeFile(o.folder() + "/three.ts", 5 * OfflineStore::kMB);
        o.refresh();
        QVERIFY(o.nearLimit() && !o.full());                                 // 65 of 80 is 81%: the warning
    }
    void offlineStoreWarnsAndPausesButNeverDeletes()
    {
        QTemporaryDir tmp;
        OfflineStore o;
        o.setDefaultBase(tmp.filePath("offline"));
        o.setProfileId("p1");
        qint64 free = qint64(100) * 1024 * 1024 * 1024;
        o.setFreeProbe([&free](const QString &) { return free; });
        o.setLimitMB(100);
        writeFile(o.folder() + "/x.ts", 85 * OfflineStore::kMB);
        o.refresh();
        QVERIFY(o.nearLimit() && !o.full());
        QVERIFY(o.summary().contains("close to the limit"));
        QString why;
        QVERIFY(o.canStore(10 * OfflineStore::kMB, &why));                   // 95 of 100 fits
        QVERIFY(why.isEmpty());
        QVERIFY(!o.canStore(20 * OfflineStore::kMB, &why));                  // 105 of 100 does not
        QVERIFY(why.contains("limit"));
        writeFile(o.folder() + "/y.ts", 20 * OfflineStore::kMB);
        o.refresh();
        QVERIFY(o.full());
        QVERIFY(o.summary().contains("paused"));
        QVERIFY(QFileInfo::exists(o.folder() + "/x.ts") && QFileInfo::exists(o.folder() + "/y.ts"));   // nothing was deleted
        // raising the limit lets things carry on
        o.setLimitMB(500);
        QVERIFY(o.canStore(10 * OfflineStore::kMB, &why));
        // a nearly full drive pauses things too, whatever the limit
        free = 500 * OfflineStore::kMB;
        QVERIFY(!o.canStore(10 * OfflineStore::kMB, &why));
        QVERIFY(why.contains("drive"));
    }
    void offlineStoreMovesToAnotherDriveAndSurvivesItBeingUnplugged()
    {
        QTemporaryDir tmp, usb;
        OfflineStore o;
        o.setDefaultBase(tmp.filePath("offline"));
        o.setProfileId("p1");
        o.setFreeProbe([](const QString &) { return qint64(100) * 1024 * 1024 * 1024; });
        writeFile(o.folder() + "/s1/seg1.ts", 2 * OfflineStore::kMB);
        writeFile(o.folder() + "/s1/seg2.ts", 3 * OfflineStore::kMB);
        o.refresh();
        QCOMPARE(o.usedBytes(), 5 * OfflineStore::kMB);
        QSignalSpy edited(&o, &OfflineStore::edited);
        QString why;
        QVERIFY2(o.moveTo(usb.path(), &why), qPrintable(why));
        QCOMPARE(edited.count(), 1);                                          // the setting is saved with the profile
        QCOMPARE(o.location(), QDir::cleanPath(usb.path()));
        QVERIFY(o.folder().startsWith(QDir::cleanPath(usb.path()) + "/Hop-tunes offline/"));
        QVERIFY(QFileInfo::exists(o.folder() + "/s1/seg2.ts"));               // arrived
        QVERIFY(!QFileInfo::exists(tmp.filePath("offline/p1")));              // and the old copy is gone
        QCOMPARE(o.usedBytes(), 5 * OfflineStore::kMB);
        // saved and restored
        OfflineStore again;
        again.setDefaultBase(tmp.filePath("offline"));
        again.setProfileId("p1");
        again.fromJson(o.toJson());
        QCOMPARE(again.folder(), o.folder());
        QCOMPARE(again.usedBytes(), 5 * OfflineStore::kMB);
        // unplugged: unavailable, said plainly, nothing stored, no crash
        QString gone = usb.path();
        QDir(usb.path()).removeRecursively();
        again.refresh();
        QVERIFY(!again.available());
        QVERIFY(again.summary().contains("not available"));
        QVERIFY(!again.canStore(1, &why));
        QCOMPARE(again.usedBytes(), qint64(0));
        // not enough room on the new drive: refused, and nothing moved or lost
        QTemporaryDir small;
        OfflineStore o2;
        o2.setDefaultBase(tmp.filePath("offline2"));
        o2.setProfileId("p1");
        o2.setFreeProbe([](const QString &) { return qint64(10) * OfflineStore::kMB; });
        writeFile(o2.folder() + "/k.ts", 5 * OfflineStore::kMB);
        o2.refresh();
        QVERIFY(!o2.moveTo(small.path(), &why));
        QVERIFY(why.contains("room"));
        QVERIFY(QFileInfo::exists(o2.folder() + "/k.ts"));
        QVERIFY(!o2.moveTo("/definitely/not/a/folder", &why));
    }
    void offlineStoreSettingsAreSafeAgainstJunk()
    {
        OfflineStore o;
        o.setDefaultBase("/tmp/hop-offline-test");
        o.fromJson(QJsonObject{{"limitMB", 3}, {"location", "/nope/never"}});     // too small: back to the default; a missing place is just unavailable
        QCOMPARE(o.limitMB(), OfflineStore::kDefaultLimitMB);
        QVERIFY(!o.available());
        o.fromJson(QJsonObject{});
        QCOMPARE(o.limitMB(), OfflineStore::kDefaultLimitMB);
        QVERIFY(o.available());
        o.setProfileId("../../etc");                                               // a profile id never walks out of the folder
        QVERIFY(!o.folder().contains(".."));
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
    // ---- HLS playlists (how Kick serves past streams and clips) ----
    void hlsMasterAndMediaPlaylistsAreRead()
    {
        const QString master = "#EXTM3U\n#EXT-X-SESSION-DATA:DATA-ID=\"x\",VALUE=\"y\"\n"
            "#EXT-X-MEDIA:TYPE=VIDEO,GROUP-ID=\"720p60\",NAME=\"720p60\",AUTOSELECT=YES,DEFAULT=YES\n"
            "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=3484773,CODECS=\"avc1.4D401F,mp4a.40.2\",RESOLUTION=1280x720,VIDEO=\"720p60\",FRAME-RATE=60.000\n720p60/playlist.m3u8\n"
            "#EXT-X-MEDIA:TYPE=VIDEO,GROUP-ID=\"1080p60\",NAME=\"1080p60\",AUTOSELECT=YES,DEFAULT=YES\n"
            "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=6812668,CODECS=\"avc1.4D402A,mp4a.40.2\",RESOLUTION=1920x1080,VIDEO=\"1080p60\",FRAME-RATE=60.000\n1080p60/playlist.m3u8\n"
            "#EXT-X-MEDIA:TYPE=VIDEO,GROUP-ID=\"160p30\",NAME=\"160p\",AUTOSELECT=YES,DEFAULT=YES\n"
            "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=230000,CODECS=\"avc1.4D401F,mp4a.40.2\",RESOLUTION=284x160,VIDEO=\"160p30\"\n160p30/playlist.m3u8\n";
        const QUrl base("https://stream.kick.com/a/b/media/hls/master.m3u8");
        const auto vs = parseHlsMaster(master, base);
        QCOMPARE(vs.size(), 3);
        QCOMPARE(vs[0].height, 1080);                                       // best first
        QCOMPARE(vs[0].name, QString("1080p60"));
        QCOMPARE(vs[0].url, QUrl("https://stream.kick.com/a/b/media/hls/1080p60/playlist.m3u8"));
        QCOMPARE(vs[1].height, 720);
        QCOMPARE(chooseHlsVariant(vs, 0)->height, 1080);
        QCOMPARE(chooseHlsVariant(vs, 720)->height, 720);
        QCOMPARE(chooseHlsVariant(vs, 900)->height, 720);                    // the best that fits
        QCOMPARE(chooseHlsVariant(vs, 100)->height, 160);                    // none fits: the smallest there is
        QVERIFY(parseHlsMaster("not a playlist", base).isEmpty());
        QVERIFY(chooseHlsVariant({}, 0) == nullptr);

        const QString media = "#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:13\n#EXT-X-PLAYLIST-TYPE:EVENT\n"
            "#EXT-X-PROGRAM-DATE-TIME:2026-11-12T06:57:13.649Z\n#EXTINF:10.000,\n0.ts\n#EXTINF:10.000,\n1.ts\n#EXTINF:3.500,\n2.ts\n#EXT-X-ENDLIST\n";
        const auto m = parseHlsMedia(media, QUrl("https://stream.kick.com/a/b/media/hls/1080p60/playlist.m3u8"));
        QVERIFY(m.unsupported.isEmpty() && m.finished);
        QCOMPARE(m.parts.size(), 3);
        QCOMPARE(m.parts[2].url, QUrl("https://stream.kick.com/a/b/media/hls/1080p60/2.ts"));
        QVERIFY(qAbs(m.seconds - 23.5) < 0.001);
        QVERIFY(!parseHlsMedia(media.left(media.indexOf("#EXT-X-ENDLIST")), base).finished);   // a live one is not finished

        // a clip: one file cut into byte ranges is fetched as the whole file
        const QString clip = "#EXTM3U\n#EXT-X-VERSION:4\n#EXT-X-TARGETDURATION:2\n#EXT-X-BYTERANGE:1000@0\n#EXTINF:2.000,\n2701.ts\n"
            "#EXT-X-BYTERANGE:500@1000\n#EXTINF:2.000,\n2701.ts\n#EXT-X-BYTERANGE:250\n#EXTINF:1.000,\n2701.ts\n#EXT-X-ENDLIST\n";
        const auto c = parseHlsMedia(clip, QUrl("https://clips.kick.com/clips/ed/clip_1/playlist.m3u8"));
        QVERIFY(c.finished && c.unsupported.isEmpty());
        QCOMPARE(c.parts.size(), 1);
        QCOMPARE(c.parts[0].url, QUrl("https://clips.kick.com/clips/ed/clip_1/2701.ts"));
        QCOMPARE(c.parts[0].bytes, qint64(1750));                              // three slices that follow each other: one request
        QCOMPARE(c.parts[0].rangeStart, qint64(0));
        QVERIFY(qAbs(c.seconds - 5.0) < 0.001);
        // what real clips look like: many files, slices not in file order. Each stays its own slice, in the playlist's order (never a whole file)
        const QString real = "#EXTM3U\n#EXT-X-VERSION:4\n#EXT-X-BYTERANGE:2099020@8449660\n#EXTINF:2.000,\n10585.ts\n"
            "#EXT-X-BYTERANGE:2108232@0\n#EXTINF:2.000,\n10586.ts\n#EXT-X-BYTERANGE:1952380@3151256\n#EXTINF:2.000,\n10586.ts\n"
            "#EXT-X-BYTERANGE:2024008@5103636\n#EXTINF:2.000,\n10586.ts\n#EXT-X-ENDLIST\n";
        const auto rm = parseHlsMedia(real, QUrl("https://clips.kick.com/clips/e9/clip_x/playlist.m3u8"));
        QCOMPARE(rm.parts.size(), 3);                                          // (the last two slices follow each other in 10586.ts, so they are one request)
        QCOMPARE(rm.parts[0].rangeStart, qint64(8449660));
        QCOMPARE(rm.parts[0].bytes, qint64(2099020));
        QCOMPARE(rm.parts[1].url, QUrl("https://clips.kick.com/clips/e9/clip_x/10586.ts"));
        QCOMPARE(rm.parts[1].rangeStart, qint64(0));
        QCOMPARE(rm.parts[2].rangeStart, qint64(3151256));
        QCOMPARE(rm.parts[2].bytes, qint64(1952380 + 2024008));

        // encrypted or fragmented-mp4 videos are refused plainly, and junk is not a playlist
        QVERIFY(parseHlsMedia("#EXTM3U\n#EXT-X-KEY:METHOD=AES-128,URI=\"k\"\n#EXTINF:2,\na.ts\n#EXT-X-ENDLIST\n", base).unsupported.contains("encrypted"));
        QVERIFY(!parseHlsMedia("#EXTM3U\n#EXT-X-MAP:URI=\"init.mp4\"\n#EXTINF:2,\na.m4s\n#EXT-X-ENDLIST\n", base).unsupported.isEmpty());
        QVERIFY(!parseHlsMedia("<html>nope</html>", base).unsupported.isEmpty());
        QVERIFY(!parseHlsMedia("#EXTM3U\n#EXT-X-ENDLIST\n", base).unsupported.isEmpty());
    }

    // ---- downloads of finished streams and clips ----
private:
    struct Rig {
        QTemporaryDir tmp;
        OfflineStore store;
        std::unique_ptr<OfflineDownloads> dl;
        qint64 free = qint64(100) * 1024 * 1024 * 1024;
        int partRequests = 0;
        int failParts = 0;                 // this many part requests fail before they work again
        bool endlist = true;
        QString secondsEach = "10";
        QString bandwidth = "1000000";
    };
    static QByteArray partBytes(int i) { return QByteArray(3000 + i * 10, char('a' + i)); }
    void rig(Rig &r, int parts = 6)
    {
        fresh();
        r.store.setDefaultBase(r.tmp.filePath("offline"));
        r.store.setProfileId("p1");
        r.store.setFreeProbe([&r](const QString &) { return r.free; });
        r.dl = std::make_unique<OfflineDownloads>(&net, &r.store);
        r.dl->setRetryDelayMs(5);
        ft->delayMs = 15;
        connect(&net, &AppNet::opened, r.dl.get(), &OfflineDownloads::appOpened);
        connect(&net, &AppNet::closed, r.dl.get(), &OfflineDownloads::appClosed);
        ft->handler = [&r, parts](const QUrl &u) -> NetReply {
            const QString path = u.path();
            if (path.endsWith("master.m3u8"))
                return ok("#EXTM3U\n#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=" + r.bandwidth.toUtf8() + ",RESOLUTION=1280x720,VIDEO=\"720p60\"\n720p60/playlist.m3u8\n"
                          "#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=9000000,RESOLUTION=1920x1080,VIDEO=\"1080p60\"\n1080p60/playlist.m3u8\n");
            if (path.endsWith("720p60/playlist.m3u8") || path.endsWith("1080p60/playlist.m3u8")) {
                QByteArray t = "#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:13\n";
                for (int i = 0; i < parts; ++i) t += "#EXTINF:" + r.secondsEach.toUtf8() + ",\n" + QByteArray::number(i) + ".ts\n";
                if (r.endlist) t += "#EXT-X-ENDLIST\n";
                return ok(t);
            }
            if (path.endsWith(".ts")) {
                ++r.partRequests;
                if (r.failParts > 0) { --r.failParts; NetReply e; e.error = "could not connect"; return e; }
                return ok(partBytes(path.section('/', -1).section('.', 0, 0).toInt()));
            }
            if (path.endsWith("thumb.webp")) return ok(QByteArray("not an image"));
            NetReply e; e.status = 404; e.error = "the site answered 404"; return e;
        };
    }
    static QVariantMap streamInfo(const QString &id = "s1")
    {
        return {{"id", id}, {"kind", "stream"}, {"title", "A long stream"}, {"channel", "alpha"},
                {"thumb", "https://images.kick.com/video_thumbnails/x/thumb.webp"}, {"url", "https://stream.kick.com/a/b/media/hls/master.m3u8"}, {"maxHeight", 720}};
    }
    static QByteArray allParts(int n) { QByteArray b; for (int i = 0; i < n; ++i) b += partBytes(i); return b; }
    static QByteArray readAll(const QString &p) { QFile f(p); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); }

private slots:
    void downloadJoinsTheVideoIntoTheOfflineFolder()
    {
        Rig r; rig(r);
        QSignalSpy finished(r.dl.get(), &OfflineDownloads::finished);
        QCOMPARE(r.dl->enqueue(streamInfo()), QString());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
        const QVariantMap it = r.dl->item("s1");
        QCOMPARE(it.value("state").toString(), QString("done"));
        QCOMPARE(it.value("quality").toString(), QString("720p60"));                       // the wish was up to 720
        QCOMPARE(it.value("doneParts").toInt(), 6);
        QCOMPARE(readAll(r.dl->videoPath("s1")), allParts(6));                            // every piece, in order, once
        QVERIFY(r.dl->videoPath("s1").startsWith(r.store.folder()));                      // inside the offline folder
        QCOMPARE(it.value("bytes").toLongLong(), qint64(allParts(6).size()));
        QVERIFY(!r.dl->busy());
        for (const QUrl &u : ft->asked) QVERIFY2(AppNet::urlAllowed("kick", u), qPrintable(u.toString()));
        QCOMPARE(r.dl->enqueue(streamInfo()), QString("you already have this one"));
        // it survives a restart (a new manager reads the folder)
        r.store.refresh();
        QCOMPARE(r.store.usedBytes() >= allParts(6).size(), true);
        OfflineDownloads again(&net, &r.store);
        again.reload();
        QCOMPARE(again.count(), 1);
        QCOMPARE(again.item("s1").value("state").toString(), QString("done"));
        QCOMPARE(again.item("s1").value("title").toString(), QString("A long stream"));
    }
    void downloadWaitsForTheAppAndStopsAtTheDrive()
    {
        Rig r; rig(r, 8);
        QSignalSpy finished(r.dl.get(), &OfflineDownloads::finished);
        // the app closes in the middle: it waits, and carries on when the app opens again, without repeating or skipping a piece
        QCOMPARE(r.dl->enqueue(streamInfo()), QString());
        QTRY_VERIFY_WITH_TIMEOUT(r.partRequests >= 2, 5000);
        net.close("kick");
        QVERIFY(!r.dl->busy());
        QCOMPARE(r.dl->item("s1").value("state").toString(), QString("waiting"));
        const int sent = ft->asked.size();
        QTest::qWait(100);
        QCOMPARE(ft->asked.size(), sent);                                               // nothing is fetched while the app is closed
        net.setAllowed("kick", true);
        net.open("kick");
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
        QCOMPARE(readAll(r.dl->videoPath("s1")), allParts(8));

        // the drive fills up in the middle: it stops with a plain reason and nothing is deleted; room again, resume, done
        Rig q; rig(q, 8);
        QSignalSpy fin2(q.dl.get(), &OfflineDownloads::finished);
        QCOMPARE(q.dl->enqueue(streamInfo("s2")), QString());
        QTRY_VERIFY_WITH_TIMEOUT(q.partRequests >= 3, 5000);
        q.free = 200LL * 1024 * 1024;                                                   // under the 1 GB the drive must keep free
        QTRY_COMPARE_WITH_TIMEOUT(q.dl->item("s2").value("state").toString(), QString("limit"), 5000);
        QVERIFY(q.dl->item("s2").value("error").toString().contains("drive"));
        const int kept = q.dl->item("s2").value("doneParts").toInt();
        QVERIFY(kept >= 3 && kept < 8);
        QVERIFY(QFileInfo::exists(q.dl->videoPath("s2")));                              // what arrived is still there
        q.free = 100LL * 1024 * 1024 * 1024;
        q.dl->resume("s2");
        QTRY_COMPARE_WITH_TIMEOUT(fin2.count(), 1, 5000);
        QCOMPARE(readAll(q.dl->videoPath("s2")), allParts(8));
    }
    void downloadCanBePausedRetriesAndFailsPlainly()
    {
        Rig r; rig(r, 8);
        QSignalSpy finished(r.dl.get(), &OfflineDownloads::finished);
        QCOMPARE(r.dl->enqueue(streamInfo()), QString());
        QTRY_VERIFY_WITH_TIMEOUT(r.partRequests >= 2, 5000);
        r.dl->pause("s1");
        QCOMPARE(r.dl->item("s1").value("state").toString(), QString("paused"));
        const int sent = ft->asked.size();
        QTest::qWait(100);
        QVERIFY(ft->asked.size() <= sent + 1);                                          // (a piece already on the way may still arrive, then nothing more)
        r.dl->resume("s1");
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
        QCOMPARE(readAll(r.dl->videoPath("s1")), allParts(8));

        // a piece that fails now and then is retried; one that keeps failing ends in a plain message, and starts over when asked again
        Rig f; rig(f, 4);
        f.failParts = 2;
        QSignalSpy fin(f.dl.get(), &OfflineDownloads::finished);
        QCOMPARE(f.dl->enqueue(streamInfo("s3")), QString());
        QTRY_COMPARE_WITH_TIMEOUT(fin.count(), 1, 5000);
        QCOMPARE(readAll(f.dl->videoPath("s3")), allParts(4));
        Rig g; rig(g, 4);
        g.failParts = 1000;
        QCOMPARE(g.dl->enqueue(streamInfo("s4")), QString());
        QTRY_COMPARE_WITH_TIMEOUT(g.dl->item("s4").value("state").toString(), QString("failed"), 5000);
        QVERIFY(g.dl->item("s4").value("error").toString().contains("connect"));
        g.failParts = 0;
        QCOMPARE(g.dl->enqueue(streamInfo("s4")), QString());
        QTRY_COMPARE_WITH_TIMEOUT(g.dl->item("s4").value("state").toString(), QString("done"), 5000);
    }
    void downloadRefusesWhatItCannotOrShouldNotTake()
    {
        Rig r; rig(r, 4);
        r.endlist = false;                                                              // a stream that is still going
        QCOMPARE(r.dl->enqueue(streamInfo()), QString());
        QTRY_COMPARE_WITH_TIMEOUT(r.dl->item("s1").value("state").toString(), QString("failed"), 5000);
        QVERIFY(r.dl->item("s1").value("error").toString().contains("not finished"));
        QVERIFY(r.dl->videoPath("s1").isEmpty());
        // too big for the limit: said before anything is fetched from the video host
        Rig big; rig(big, 4);
        big.store.setLimitMB(50);
        big.secondsEach = "36000"; big.bandwidth = "8000000";                           // ~ 36 GB
        QCOMPARE(big.dl->enqueue(streamInfo("s2")), QString());
        QTRY_COMPARE_WITH_TIMEOUT(big.dl->item("s2").value("state").toString(), QString("limit"), 5000);
        QVERIFY(big.dl->item("s2").value("error").toString().contains("limit"));
        QCOMPARE(big.partRequests, 0);
        // not an address Kick's app may use, and guests keep nothing
        QVERIFY(!big.dl->enqueue({{"id", "x"}, {"url", "https://evil.example/m.m3u8"}}).isEmpty());
        QVERIFY(!big.dl->enqueue({{"id", "x"}, {"url", "http://kick.com/m.m3u8"}}).isEmpty());
        big.dl->setEnabled(false);
        QVERIFY(big.dl->enqueue(streamInfo("s9")).contains("guests"));
    }
    void aClipOfSlicesIsFetchedAsSlicesNotWholeFiles()
    {
        Rig r; rig(r);
        auto fileByte = [](int f, qint64 i) { return char((i * 7 + f) % 251); };
        auto slice = [&](int f, qint64 start, qint64 len) { QByteArray b; for (qint64 i = 0; i < len; ++i) b.append(fileByte(f, start + i)); return b; };
        bool shortAnswers = false;
        auto inner = ft->handler;
        ft->handler = [&, inner](const QUrl &u) -> NetReply {
            const QString path = u.path();
            if (path.endsWith("/clip_x/playlist.m3u8"))
                return ok("#EXTM3U\n#EXT-X-VERSION:4\n#EXT-X-BYTERANGE:3000@7000\n#EXTINF:2,\na.ts\n#EXT-X-BYTERANGE:2500@0\n#EXTINF:2,\nb.ts\n"
                          "#EXT-X-BYTERANGE:1000@4000\n#EXTINF:2,\nb.ts\n#EXT-X-BYTERANGE:500\n#EXTINF:1,\nb.ts\n#EXT-X-ENDLIST\n");
            if (path.endsWith("/clip_x/a.ts") || path.endsWith("/clip_x/b.ts")) {
                const int f = path.endsWith("a.ts") ? 1 : 2;
                const NetLimits &l = ft->lastLimits;
                if (l.rangeStart < 0) return ok(slice(f, 0, 12000));                      // a whole file: far more than the clip wants
                NetReply s = ok(slice(f, l.rangeStart, shortAnswers ? l.rangeLength - 1 : l.rangeLength)); s.status = 206; return s;
            }
            return inner(u);
        };
        const QVariantMap info{{"id", "clip_x"}, {"kind", "clip"}, {"title", "c"}, {"channel", "alpha"}, {"url", "https://clips.kick.com/clips/ab/clip_x/playlist.m3u8"}};
        QSignalSpy finished(r.dl.get(), &OfflineDownloads::finished);
        QCOMPARE(r.dl->enqueue(info), QString());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
        // 3000 of a.ts from 7000, then b.ts 0..2500, then b.ts 4000..5000 and 5000..5500 as one slice (4000..5500)
        const QByteArray expect = slice(1, 7000, 3000) + slice(2, 0, 2500) + slice(2, 4000, 1500);
        QCOMPARE(readAll(r.dl->videoPath("clip_x")), expect);
        int sliceRequests = 0, wholeFileRequests = 0;
        for (int i = 0; i < ft->asked.size(); ++i) {
            if (!ft->asked[i].path().endsWith(".ts")) continue;
            if (ft->limitsAsked[i].rangeStart >= 0) ++sliceRequests; else ++wholeFileRequests;
        }
        QCOMPARE(sliceRequests, 3);                                                     // three slices
        QCOMPARE(wholeFileRequests, 0);                                                 // and no whole file anywhere
        // a slice that comes back the wrong size is not trusted: retried, then a plain failure, and nothing wrong is written
        shortAnswers = true;
        r.dl->remove("clip_x");
        QCOMPARE(r.dl->enqueue(info), QString());
        QTRY_COMPARE_WITH_TIMEOUT(r.dl->item("clip_x").value("state").toString(), QString("failed"), 5000);
        QVERIFY(r.dl->item("clip_x").value("error").toString().contains("wrong size"));
        QVERIFY(readAll(r.dl->videoPath("clip_x")).isEmpty());
    }

    void deletingADownloadRemovesItsFilesAndOnlyThose()
    {
        Rig r; rig(r, 3);
        QSignalSpy finished(r.dl.get(), &OfflineDownloads::finished);
        QCOMPARE(r.dl->enqueue(streamInfo()), QString());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
        QVERIFY(QFileInfo::exists(r.dl->videoPath("s1")));
        const QString mine = r.store.folder() + "/keepme.txt";
        { QFile f(mine); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("x"); }
        r.dl->remove("s1");
        QVERIFY(!r.dl->has("s1"));
        QVERIFY(!QFileInfo::exists(r.store.folder() + "/s1"));
        QVERIFY(QFileInfo::exists(mine));                                               // a neighbour is never touched
        r.store.refresh();
        QVERIFY(r.store.usedBytes() <= 1);
    }
    // ---- recording a live stream ----
private:
    struct LiveRig { Rig *r; int polls = 0; int endAt = 1000; int failFrom = -1; };
    static const QString liveMaster() { return "https://a1.playback.live-video.net/live/master.m3u8"; }
    void liveRig(Rig &r, LiveRig &lv)
    {
        rig(r);
        lv.r = &r;
        r.dl->setPollIntervalMs(20);
        auto inner = ft->handler;
        ft->handler = [&lv, inner](const QUrl &u) -> NetReply {
            const QString path = u.path();
            if (path == "/live/master.m3u8")
                return ok("#EXTM3U\n#EXT-X-STREAM-INF:PROGRAM-ID=1,BANDWIDTH=3000000,RESOLUTION=1280x720,VIDEO=\"720p60\"\n720p/playlist.m3u8\n");
            if (path == "/live/720p/playlist.m3u8") {
                const int k = lv.polls++;
                if (lv.failFrom >= 0 && k >= lv.failFrom) { NetReply e; e.status = 404; e.error = "the site answered 404"; return e; }
                QByteArray t = "#EXTM3U\n#EXT-X-VERSION:3\n#EXT-X-TARGETDURATION:0.04\n#EXT-X-MEDIA-SEQUENCE:" + QByteArray::number(k) + "\n";
                for (int i = 0; i < 4; ++i) t += "#EXTINF:2.0,\n" + QByteArray::number(k + i) + ".ts\n";
                if (k >= lv.endAt) t += "#EXT-X-ENDLIST\n";
                return ok(t);
            }
            return inner(u);
        };
    }
    static QVariantMap recInfo(const QString &id = "rec-1")
    {
        return {{"id", id}, {"title", "live now"}, {"channel", "alpha"}, {"url", liveMaster()}, {"maxHeight", 720}};
    }
    static QByteArray partsFrom(int first, int last) { QByteArray b; for (int i = first; i <= last; ++i) b += partBytes(i); return b; }

private slots:
    void recordingFollowsTheLivePlaylistFromNowToTheEnd()
    {
        Rig r; LiveRig lv; liveRig(r, lv);
        lv.endAt = 6;
        QSignalSpy finished(r.dl.get(), &OfflineDownloads::finished);
        QCOMPARE(r.dl->startRecording(recInfo()), QString());
        QCOMPARE(r.dl->item("rec-1").value("state").toString(), QString("recording"));
        QVERIFY(r.dl->busy());
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 6000);
        const QVariantMap it = r.dl->item("rec-1");
        QCOMPARE(it.value("state").toString(), QString("done"));
        QCOMPARE(it.value("kind").toString(), QString("recording"));
        QCOMPARE(it.value("error").toString(), QString("the stream ended"));
        // from the live edge (the last three of the first window: 1, 2, 3), then every new piece once, in order, to the end (9)
        QCOMPARE(readAll(r.dl->videoPath("rec-1")), partsFrom(1, 9));
        QVERIFY(qAbs(it.value("seconds").toDouble() - 18.0) < 0.001);
        QVERIFY(!r.dl->busy());
        for (const QUrl &u : ft->asked) QVERIFY2(AppNet::urlAllowed("kick", u), qPrintable(u.toString()));
        // it is a finished item now: it survives a restart
        OfflineDownloads again(&net, &r.store);
        again.reload();
        QCOMPARE(again.item("rec-1").value("state").toString(), QString("done"));
    }
    void recordingStopsWhenToldTheAppClosesOrTheDriveFills()
    {
        // stop: what was recorded is kept, and nothing more is fetched
        Rig r; LiveRig lv; liveRig(r, lv);
        QCOMPARE(r.dl->startRecording(recInfo()), QString());
        QTRY_VERIFY_WITH_TIMEOUT(r.dl->item("rec-1").value("doneParts").toInt() >= 4, 6000);
        QVERIFY(r.dl->startRecording(recInfo("rec-2")).contains("already recording"));   // one at a time
        r.dl->stopRecording("rec-1");
        const QVariantMap it = r.dl->item("rec-1");
        QCOMPARE(it.value("state").toString(), QString("done"));
        QVERIFY(!r.dl->busy());
        const int n = it.value("doneParts").toInt();
        const int sent = ft->asked.size();
        QTest::qWait(150);
        QCOMPARE(ft->asked.size(), sent);
        QCOMPARE(readAll(r.dl->videoPath("rec-1")), partsFrom(1, n));                   // whole pieces only, in order
        // the app closes: it ends cleanly with a note
        Rig a; LiveRig la; liveRig(a, la);
        QCOMPARE(a.dl->startRecording(recInfo()), QString());
        QTRY_VERIFY_WITH_TIMEOUT(a.dl->item("rec-1").value("doneParts").toInt() >= 2, 6000);
        net.close("kick");
        QCOMPARE(a.dl->item("rec-1").value("state").toString(), QString("done"));
        QVERIFY(a.dl->item("rec-1").value("error").toString().contains("closed"));
        QVERIFY(!a.dl->busy());
        // the drive nearly fills: stopped early with a plain reason, what there is stays
        Rig d; LiveRig ld; liveRig(d, ld);
        QCOMPARE(d.dl->startRecording(recInfo()), QString());
        QTRY_VERIFY_WITH_TIMEOUT(d.dl->item("rec-1").value("doneParts").toInt() >= 3, 6000);
        d.free = 100LL * 1024 * 1024;
        QTRY_COMPARE_WITH_TIMEOUT(d.dl->item("rec-1").value("state").toString(), QString("done"), 6000);
        QVERIFY(d.dl->item("rec-1").value("error").toString().contains("stopped early"));
        QVERIFY(QFileInfo(d.dl->videoPath("rec-1")).size() > 0);
    }
    void recordingNeedsTheAppAndAGoodAddressAndEndsWhenTheChannelGoesOffline()
    {
        Rig r; LiveRig lv; liveRig(r, lv);
        QVERIFY(r.dl->startRecording({{"id", "x"}, {"url", "https://evil.example/live.m3u8"}}).contains("not one of the sites"));
        QVERIFY(r.dl->startRecording({{"id", "x"}, {"url", "http://a.live-video.net/live.m3u8"}}).contains("secure"));
        net.close("kick");
        QVERIFY(r.dl->startRecording(recInfo()).contains("open the kick app"));
        net.open("kick");
        // the channel goes offline (the playlist answers errors): the recording ends, keeping what it has
        lv.failFrom = 4;
        QCOMPARE(r.dl->startRecording(recInfo()), QString());
        QTRY_COMPARE_WITH_TIMEOUT(r.dl->item("rec-1").value("state").toString(), QString("done"), 8000);
        QCOMPARE(r.dl->item("rec-1").value("error").toString(), QString("the stream ended"));
        QVERIFY(r.dl->item("rec-1").value("doneParts").toInt() >= 3);
        // never online at all: no empty item is left pretending to be a recording
        Rig n; LiveRig ln; liveRig(n, ln);
        ln.failFrom = 0;
        QCOMPARE(n.dl->startRecording(recInfo("rec-9")), QString());
        QTRY_COMPARE_WITH_TIMEOUT(n.dl->item("rec-9").value("state").toString(), QString("failed"), 8000);
        QCOMPARE(n.dl->item("rec-9").value("doneParts").toInt(), 0);
        n.dl->setEnabled(false);
        QVERIFY(n.dl->startRecording(recInfo("rec-8")).contains("guests"));
    }

private slots:
    // ---- playlists (kept on this computer, in the profile) ----
    static QVariantMap clipItem(const QString &id = "clip_a", const QString &url = "https://clips.kick.com/clips/ab/clip_a/playlist.m3u8")
    {
        return {{"id", id}, {"kind", "clip"}, {"title", "a funny moment"}, {"channel", "alpha"}, {"url", url}, {"thumb", "https://clips.kick.com/x/thumbnail.webp"}, {"seconds", 30}};
    }
    static QVariantMap streamItem(const QString &id = "stream-1")
    {
        return {{"id", id}, {"kind", "stream"}, {"title", "last night"}, {"channel", "alpha"}, {"url", "https://stream.kick.com/a/master.m3u8"}, {"seconds", 7200}};
    }
private slots:
    void playlistsHoldChannelsClipsAndStreamsWithTheirOwnRules()
    {
        fresh();
        KickClient k(&net);
        QSignalSpy edited(&k, &KickClient::listsEdited);
        QVERIFY(k.createPlaylist("", "channels").isEmpty());                         // a name is needed
        QVERIFY(k.createPlaylist("x", "videos").isEmpty());                          // a known kind is needed
        const QString favs = k.createPlaylist("  my favourites  " + QString(100, 'x'), "channels");
        QVERIFY(!favs.isEmpty());
        QCOMPARE(edited.count(), 1);                                                  // worth saving
        QVariantMap pl = k.playlists().first().toMap();
        QVERIFY(pl.value("name").toString().startsWith("my favourites"));
        QCOMPARE(pl.value("name").toString().size(), int(KickClient::kMaxNameLength)); // long names are cut
        QCOMPARE(pl.value("kind").toString(), QString("channels"));
        // channels: names or links, normalised, never twice
        QVERIFY(k.addToPlaylist(favs, {{"slug", "https://kick.com/NightOwl"}}));
        QVERIFY(!k.addToPlaylist(favs, {{"slug", "nightowl"}}));
        QVERIFY(!k.addToPlaylist(favs, {{"slug", "not a name!"}}));
        QVERIFY(k.addToPlaylist(favs, {{"slug", "loudone"}}));
        QVERIFY(k.isInPlaylist(favs, "NIGHTOWL"));
        QCOMPARE(k.playlistsHolding("loudone").size(), 1);
        QCOMPARE(k.playlists().first().toMap().value("count").toInt(), 2);
        // a clip goes in a clips playlist, a stream in a streams one, and only addresses this app may use
        const QString clips = k.createPlaylist("funny", "clips");
        const QString streams = k.createPlaylist("long ones", "streams");
        QVERIFY(k.addToPlaylist(clips, clipItem()));
        QVERIFY(!k.addToPlaylist(clips, clipItem()));                                 // not twice
        QVERIFY(!k.addToPlaylist(clips, streamItem()));                               // wrong kind
        QVERIFY(!k.addToPlaylist(streams, clipItem("clip_b")));
        QVERIFY(k.addToPlaylist(streams, streamItem()));
        QVERIFY(!k.addToPlaylist(clips, clipItem("clip_c", "https://evil.example/c.m3u8")));
        QVERIFY(!k.addToPlaylist(clips, clipItem("clip_d", "http://clips.kick.com/c.m3u8")));
        QVERIFY(!k.addToPlaylist(favs, clipItem()));                                  // a channels playlist holds channels
        QVERIFY(!k.addToPlaylist("nope", {{"slug", "x"}}));
        // rename, remove, delete
        QVERIFY(k.renamePlaylist(clips, "very funny"));
        QVERIFY(!k.renamePlaylist(clips, "   "));
        QVERIFY(!k.renamePlaylist("nope", "x"));
        QVERIFY(k.removeFromPlaylist(favs, "loudone"));
        QVERIFY(!k.removeFromPlaylist(favs, "loudone"));
        QVERIFY(k.removeFromPlaylist(clips, "clip_a"));
        QVERIFY(k.deletePlaylist(streams));
        QVERIFY(!k.deletePlaylist(streams));
        QCOMPARE(k.playlists().size(), 2);
    }
    void playlistsKeepWithinLimitsAndRespectBlocks()
    {
        fresh();
        KickClient k(&net);
        QString id;
        for (int i = 0; i < KickClient::kMaxPlaylists; ++i) { id = k.createPlaylist(QString("p%1").arg(i), "channels"); QVERIFY(!id.isEmpty()); }
        QVERIFY(k.createPlaylist("one too many", "channels").isEmpty());
        QCOMPARE(k.playlists().size(), int(KickClient::kMaxPlaylists));
        for (int i = 0; i < KickClient::kMaxItems; ++i) QVERIFY(k.addToPlaylist(id, {{"slug", QString("chan%1").arg(i)}}));
        QVERIFY(!k.addToPlaylist(id, {{"slug", "onemore"}}));
        // a blocked streamer stays hidden everywhere, and comes back when unblocked (the playlist keeps it)
        KickClient b(&net);
        const QString pl = b.createPlaylist("favs", "channels");
        b.addToPlaylist(pl, {{"slug", "alpha"}}); b.addToPlaylist(pl, {{"slug", "bravo"}});
        QCOMPARE(b.playlists().first().toMap().value("count").toInt(), 2);
        b.block("alpha");
        QCOMPARE(b.playlists().first().toMap().value("count").toInt(), 1);
        QCOMPARE(b.playlists().first().toMap().value("items").toList().first().toMap().value("slug").toString(), QString("bravo"));
        b.unblock("alpha");
        QCOMPARE(b.playlists().first().toMap().value("count").toInt(), 2);
    }
    void playlistsSurviveSavingAndAreSafeAgainstJunk()
    {
        fresh();
        KickClient k(&net);
        const QString favs = k.createPlaylist("favs", "channels");
        k.addToPlaylist(favs, {{"slug", "alpha"}}); k.addToPlaylist(favs, {{"slug", "bravo"}});
        const QString clips = k.createPlaylist("clips", "clips");
        k.addToPlaylist(clips, clipItem());
        KickClient again(&net);
        again.setListsJson(k.listsJson());
        QCOMPARE(again.playlists().size(), 2);
        QCOMPARE(again.playlists()[0].toMap().value("count").toInt(), 2);
        QCOMPARE(again.playlists()[1].toMap().value("items").toList().first().toMap().value("title").toString(), QString("a funny moment"));
        QVERIFY(again.isInPlaylist(favs, "bravo"));
        // junk in the saved file: bad kinds, nameless, duplicate ids, other sites' addresses, wrong item shapes are all left out
        QJsonArray bad;
        bad.append(QJsonObject{{"id", "a"}, {"name", "ok"}, {"kind", "channels"}, {"items", QJsonArray{"x1", "x1", "not a name!", 5, QJsonObject{}}}});
        bad.append(QJsonObject{{"id", "b"}, {"name", ""}, {"kind", "channels"}});
        bad.append(QJsonObject{{"id", "c"}, {"name", "k"}, {"kind", "videos"}});
        bad.append(QJsonObject{{"id", "a"}, {"name", "dup"}, {"kind", "channels"}});
        bad.append(QJsonObject{{"id", "d"}, {"name", "clips"}, {"kind", "clips"}, {"items", QJsonArray{
            QJsonObject{{"id", "ok"}, {"url", "https://clips.kick.com/a.m3u8"}}, QJsonObject{{"id", "evil"}, {"url", "https://evil.example/a.m3u8"}}, "string", QJsonObject{{"url", "https://clips.kick.com/z"}}}}});
        bad.append("not even an object");
        KickClient j(&net);
        j.setListsJson(QJsonObject{{"playlists", bad}});
        QCOMPARE(j.playlists().size(), 2);
        QCOMPARE(j.playlists()[0].toMap().value("count").toInt(), 1);               // only "x1", once
        QCOMPARE(j.playlists()[1].toMap().value("count").toInt(), 1);               // only the clip on Kick's own host
        j.setListsJson(QJsonObject{{"playlists", "garbage"}});
        QVERIFY(j.playlists().isEmpty());
    }
    void aChannelPlaylistShowsWhoIsLiveWhenAsked()
    {
        fresh();
        ft->handler = [](const QUrl &u) -> NetReply {
            if (u.path() == "/api/v2/channels/liveone") return ok(channelJson("liveone", true));
            if (u.path() == "/api/v2/channels/quiet") return ok(channelJson("quiet", false));
            NetReply e; e.status = 404; e.error = "the site answered 404"; return e;
        };
        KickClient k(&net);
        k.open();
        const QString id = k.createPlaylist("favs", "channels");
        k.addToPlaylist(id, {{"slug", "liveone"}}); k.addToPlaylist(id, {{"slug", "quiet"}});
        const int before = ft->asked.size();
        QCOMPARE(k.playlists().first().toMap().value("items").toList().first().toMap().value("checked").toBool(), false);   // nothing is looked up by itself
        QTest::qWait(50);
        QCOMPARE(ft->asked.size(), before);
        k.refreshPlaylist(id);
        QTRY_VERIFY(k.playlists().first().toMap().value("items").toList().first().toMap().value("checked").toBool());
        QTRY_VERIFY(k.playlists().first().toMap().value("items").toList().last().toMap().value("checked").toBool());
        QCOMPARE(k.playlists().first().toMap().value("items").toList().first().toMap().value("live").toBool(), true);
        QCOMPARE(k.playlists().first().toMap().value("items").toList().last().toMap().value("live").toBool(), false);
    }

private slots:
    // A real clip, from the real servers, only when asked for: HOPTUNES_LIVE_KICK=<channel with clips>. Not run in CI.
    void liveClipDownloadAgainstTheRealServers()
    {
        const QString slug = qEnvironmentVariable("HOPTUNES_LIVE_KICK");
        if (slug.isEmpty()) QSKIP("set HOPTUNES_LIVE_KICK=<channel> to try the real servers");
        QNetworkProxyFactory::setUseSystemConfiguration(true);
        AppNet realNet;                                                    // the real transport
        realNet.setAllowed("kick", true);
        realNet.open("kick");
        KickClient k(&realNet);
        k.open();
        k.openChannel(slug);
        QTRY_VERIFY_WITH_TIMEOUT(!k.channel().value("loading").toBool(), 20000);
        k.loadClips();
        QTRY_VERIFY_WITH_TIMEOUT(!k.clips().isEmpty(), 20000);
        const QVariantMap clip = k.clips().first().toMap();
        QTemporaryDir tmp;
        OfflineStore store;
        store.setDefaultBase(tmp.filePath("offline"));
        store.setProfileId("live");
        OfflineDownloads dl(&realNet, &store);
        QSignalSpy finished(&dl, &OfflineDownloads::finished);
        QVariantMap info = clip;
        info["channel"] = slug;
        QCOMPARE(dl.enqueue(info), QString());
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1 || dl.item(clip.value("id").toString()).value("state").toString() == "failed", 120000);
        const QVariantMap it = dl.item(clip.value("id").toString());
        qInfo() << "live clip:" << it.value("state") << it.value("error") << it.value("bytes") << clip.value("title");
        QCOMPARE(it.value("state").toString(), QString("done"));
        const QByteArray data = readAll(dl.videoPath(clip.value("id").toString()));
        QVERIFY(data.size() > 10000);
        QCOMPARE(uchar(data[0]), uchar(0x47));                             // an MPEG-TS packet starts with the sync byte
        QCOMPARE(uchar(data[188]), uchar(0x47));
        QFile::copy(dl.videoPath(clip.value("id").toString()), qEnvironmentVariable("HOPTUNES_LIVE_OUT", tmp.filePath("clip-copy.ts")));
    }
    // A real live stream, only when asked for: HOPTUNES_LIVE_KICK=<a channel that is live now>. Records ~15 s. Not run in CI.
    void liveRecordingAgainstTheRealServers()
    {
        const QString slug = qEnvironmentVariable("HOPTUNES_LIVE_KICK");
        if (slug.isEmpty()) QSKIP("set HOPTUNES_LIVE_KICK=<live channel> to try the real servers");
        QNetworkProxyFactory::setUseSystemConfiguration(true);
        AppNet realNet;
        realNet.setAllowed("kick", true);
        realNet.open("kick");
        KickClient k(&realNet);
        k.open();
        k.openChannel(slug);
        QTRY_VERIFY_WITH_TIMEOUT(!k.channel().value("loading").toBool(), 20000);
        const QString addr = k.channel().value("playbackUrl").toString();
        if (addr.isEmpty()) QSKIP("that channel is not live right now");
        QTemporaryDir tmp;
        OfflineStore store;
        store.setDefaultBase(tmp.filePath("offline"));
        store.setProfileId("live");
        OfflineDownloads dl(&realNet, &store);
        QCOMPARE(dl.startRecording({{"id", "rec-live"}, {"title", "t"}, {"channel", slug}, {"url", addr}, {"maxHeight", 480}}), QString());
        QTest::qWait(15000);
        dl.stopRecording("rec-live");
        const QVariantMap it = dl.item("rec-live");
        qInfo() << "live recording:" << it.value("state") << it.value("error") << it.value("bytes") << it.value("seconds") << it.value("quality");
        QCOMPARE(it.value("state").toString(), QString("done"));
        const QByteArray data = readAll(dl.videoPath("rec-live"));
        QVERIFY(data.size() > 50000);
        QCOMPARE(uchar(data[0]), uchar(0x47));
        QCOMPARE(uchar(data[188]), uchar(0x47));
        QFile::copy(dl.videoPath("rec-live"), qEnvironmentVariable("HOPTUNES_LIVE_OUT", tmp.filePath("rec-copy.ts")));
    }
};

QTEST_MAIN(TestNet)
#include "test_net.moc"
