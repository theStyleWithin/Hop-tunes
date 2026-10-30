// SPDX-License-Identifier: GPL-3.0-or-later
// Library sharing (docs/design-sharing.md): invites, guest keys, approvals, and the encrypted server.
#include "library/ProfileDb.h"
#include "library/ShareAuth.h"
#include "library/ShareClient.h"
#include "library/ShareServer.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslCipher>
#include <QSslConfiguration>
#include <QSslPreSharedKeyAuthenticator>
#include <QSslSocket>
#include <QTcpSocket>
#include <QtTest>

using namespace hop;

namespace {

// Run the event loop until the handshake finishes (or fails, or time runs out).
bool waitEncrypted(QSslSocket &s, int timeoutMs)
{
    QElapsedTimer t; t.start();
    while (!s.isEncrypted() && t.elapsed() < timeoutMs && s.state() != QAbstractSocket::UnconnectedState) QTest::qWait(10);
    return s.isEncrypted();
}

struct Reply {
    bool connected = false;   // the encrypted connection was made
    int status = 0;
    QByteArray head, body;
    QJsonObject json() const { return QJsonDocument::fromJson(body).object(); }
};

// A small client: connect with an identity and a shared key, send one request, read the whole reply.
Reply ask(quint16 port, const QByteArray &identity, const QByteArray &key, const QByteArray &request, int timeoutMs = 4000)
{
    Reply r;
    QSslConfiguration conf = QSslConfiguration::defaultConfiguration();
    conf.setProtocol(QSsl::TlsV1_2);
    QList<QSslCipher> cs;
    for (const char *n : {"ECDHE-PSK-CHACHA20-POLY1305", "DHE-PSK-AES256-GCM-SHA384"}) cs << QSslCipher(QString::fromLatin1(n), QSsl::TlsV1_2);
    conf.setCiphers(cs);
    conf.setPeerVerifyMode(QSslSocket::VerifyNone);
    QSslSocket s;
    s.setSslConfiguration(conf);
    QObject::connect(&s, &QSslSocket::preSharedKeyAuthenticationRequired, [&](QSslPreSharedKeyAuthenticator *a) {
        a->setIdentity(identity); a->setPreSharedKey(key);
    });
    s.connectToHostEncrypted(QStringLiteral("127.0.0.1"), port);
    if (!waitEncrypted(s, timeoutMs)) return r;
    r.connected = true;
    s.write(request);
    QByteArray all;
    QElapsedTimer t; t.start();
    int headEnd = -1; qint64 want = -1;
    while (t.elapsed() < timeoutMs) {
        QTest::qWait(10);   // the server shares this thread, so the event loop has to run
        all += s.readAll();
        if (headEnd < 0) headEnd = all.indexOf("\r\n\r\n");
        if (headEnd >= 0 && want < 0) {
            const QByteArray h = all.left(headEnd).toLower();
            const int c = h.indexOf("content-length:");
            want = c >= 0 ? h.mid(c + 15, h.indexOf('\r', c) - c - 15).trimmed().toLongLong() : 0;
        }
        if (headEnd >= 0 && all.size() >= headEnd + 4 + want) break;
        if (s.state() != QAbstractSocket::ConnectedState && s.bytesAvailable() == 0) break;
    }
    if (headEnd < 0) return r;
    r.head = all.left(headEnd);
    r.body = all.mid(headEnd + 4);
    r.status = r.head.mid(9, 3).toInt();
    return r;
}

QByteArray get(const QByteArray &path, const QByteArray &extra = {})
{
    return "GET " + path + " HTTP/1.1\r\nHost: x\r\n" + extra + "\r\n";
}

QByteArray hex(const QString &key) { return QByteArray::fromHex(key.toLatin1()); }

} // namespace

class TestShare : public QObject {
    Q_OBJECT
    // a profile database, auth, a library and a running server, for one test
    struct Rig {
        QTemporaryDir dir;
        ProfileDb db;
        ShareAuth auth{db};
        GrantLibrary lib{db, auth, QStringLiteral("Home")};
        ShareServer server{db, auth, lib};
        bool ok() { return db.open(dir.filePath(QStringLiteral("p.sqlite"))) && server.start(0); }
        // a guest who has joined: returns the guest's id and key
        QPair<qint64, QString> join(const QString &name)
        {
            const QString code = auth.createInvite(3600, QDateTime::currentSecsSinceEpoch());
            const QString label = ShareAuth::normalizeCode(code).left(4);
            const Reply r = ask(server.port(), "invite:" + label.toLatin1(), ShareAuth::secretFromCode(code),
                                "POST /v1/redeem HTTP/1.1\r\nContent-Length: " + QByteArray::number(name.toUtf8().size() + 10)
                                + "\r\n\r\n{\"name\":\"" + name.toUtf8() + "\"}");
            return {qint64(r.json().value("guest").toDouble()), r.json().value("key").toString()};
        }
        QString file(const QString &name, const QByteArray &data)
        {
            const QString p = dir.filePath(name);
            QFile f(p);
            f.open(QIODevice::WriteOnly);
            f.write(data);
            return p;
        }
    };

private slots:
    // ---- invites and keys (no network) ----
    void inviteLooksRightAndIsOneTime()
    {
        QTemporaryDir dir;
        ProfileDb db;
        QVERIFY(db.open(dir.filePath(QStringLiteral("p.sqlite"))));
        ShareAuth auth(db);
        const QString code = auth.createInvite(3600, 1000);
        QCOMPARE(code.size(), 19);   // ABCD-EFGH-JKMN-PQRS
        QCOMPARE(ShareAuth::normalizeCode(code).size(), ShareAuth::kCodeLength);
        QVERIFY(!code.contains(QLatin1Char('0')) && !code.contains(QLatin1Char('O')) && !code.contains(QLatin1Char('1')));
        const QString label = ShareAuth::normalizeCode(code).left(4);
        QCOMPARE(auth.inviteSecret(label.toLower(), 1100), ShareAuth::secretFromCode(code.toLower().remove(QLatin1Char('-'))));
        QVERIFY(auth.inviteSecret(label, 1000 + 3600).isEmpty());          // expired
        qint64 id = 0;
        const QString key = auth.redeemInvite(label, QStringLiteral("  Guest A  "), 1100, &id);
        QCOMPARE(key.size(), 64);
        QVERIFY(id > 0);
        QCOMPARE(db.shareGuests().first().name, QStringLiteral("Guest A"));
        QCOMPARE(auth.guestSecret(id), hex(key));
        QVERIFY(auth.inviteSecret(label, 1100).isEmpty());                  // used up
        QVERIFY(auth.redeemInvite(label, QStringLiteral("again"), 1200).isEmpty());
        QCOMPARE(db.shareGuests().size(), 1);
    }

    void wrongAttemptsCancelTheInvite()
    {
        QTemporaryDir dir;
        ProfileDb db;
        QVERIFY(db.open(dir.filePath(QStringLiteral("p.sqlite"))));
        ShareAuth auth(db);
        const QString code = auth.createInvite(600, 0);
        const QString label = ShareAuth::normalizeCode(code).left(4);
        for (int i = 0; i < ShareAuth::kMaxMisses - 1; ++i) auth.noteInviteMiss(label);
        QVERIFY(!auth.inviteSecret(label, 1).isEmpty());   // still alive one miss short
        auth.noteInviteMiss(label);
        QVERIFY(auth.inviteSecret(label, 1).isEmpty());    // cancelled: even the right code is dead now
        QVERIFY(auth.redeemInvite(label, QStringLiteral("x"), 2).isEmpty());
        QCOMPARE(auth.pendingInvites(), 0);
    }

    void nothingIsSharedUntilApprovedAndRemovalForgetsEverything()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("p.sqlite"));
        qint64 guestA = 0, guestB = 0;
        {
            ProfileDb db;
            QVERIFY(db.open(file));
            ShareAuth auth(db);
            const QString keyA = auth.redeemInvite(ShareAuth::normalizeCode(auth.createInvite(3600, 0)).left(4), QStringLiteral("Guest A"), 1, &guestA);
            const QString keyB = auth.redeemInvite(ShareAuth::normalizeCode(auth.createInvite(3600, 0)).left(4), QStringLiteral("Guest B"), 2, &guestB);
            QVERIFY(!keyA.isEmpty() && !keyB.isEmpty() && guestA != guestB);
            const QString movie = QStringLiteral("/m/movie.mkv"), song = QStringLiteral("/m/song.mp3");
            QVERIFY(!db.isShared(guestA, movie) && db.shareGrants(guestA).isEmpty());        // default: nothing
            QVERIFY(db.setShareGrants(guestA, {movie, song}, true));
            QVERIFY(db.setShareGrants(guestA, {movie}, true));                             // twice is harmless
            QVERIFY(db.isShared(guestA, movie) && db.isShared(guestA, song) && !db.isShared(guestB, movie));   // per guest
            QVERIFY(db.setShareGrants(guestA, {song}, false));
            QVERIFY(!db.isShared(guestA, song) && db.isShared(guestA, movie));
            QVERIFY(!db.isShared(0, movie));
        }
        ProfileDb db;
        QVERIFY(db.open(file));                                                         // remembered for the next visit
        ShareAuth auth(db);
        QVERIFY(db.isShared(guestA, QStringLiteral("/m/movie.mkv")));
        QVERIFY(!auth.guestSecret(guestA).isEmpty());
        QVERIFY(auth.removeGuest(guestA));
        QVERIFY(auth.guestSecret(guestA).isEmpty());                                       // the key stops working
        QVERIFY(db.shareGrants(guestA).isEmpty());                                         // and the approvals are gone
        QCOMPARE(db.shareGuests().size(), 1);
    }

    // ---- the guest's side ----
    void inviteLinks()
    {
        ShareClient::Invite inv;
        QVERIFY(ShareClient::parseInvite(QStringLiteral("  hop://192.168.1.20:47800/abcd-efgh-jkmn-pqrs  "), &inv));
        QCOMPARE(inv.address, QStringLiteral("192.168.1.20"));
        QCOMPARE(inv.port, quint16(47800));
        QCOMPARE(inv.code, QStringLiteral("ABCDEFGHJKMNPQRS"));
        QVERIFY(ShareClient::parseInvite(QStringLiteral("hop://[fe80::1]:5000/ABCD-EFGH-JKMN-PQRS"), &inv));
        QCOMPARE(inv.address, QStringLiteral("fe80::1"));
        for (const char *bad : {"", "hop://host/ABCD-EFGH-JKMN-PQRS", "hop://h:1/ABCD-EFGH", "http://h:1/ABCD-EFGH-JKMN-PQRS",
                                "hop://:1/ABCD-EFGH-JKMN-PQRS", "hop://h:99999/ABCD-EFGH-JKMN-PQRS", "not a link"})
            QVERIFY2(!ShareClient::parseInvite(QString::fromLatin1(bad), nullptr), bad);
    }

    void aGuestJoinsBrowsesAndStreamsThroughTheLocalProxy()
    {
        if (!ShareServer::encryptionAvailable()) QSKIP("no TLS library");
        Rig r;
        QVERIFY(r.ok());
        const QByteArray data = [] { QByteArray d(600000, 0); for (int i = 0; i < d.size(); ++i) d[i] = char(i * 13); return d; }();
        const QString movie = r.file(QStringLiteral("A Film.mkv"), data);
        const QString hidden = r.file(QStringLiteral("Hidden.mkv"), QByteArray(100, 'h'));
        Q_UNUSED(hidden);

        // join with the invite link, exactly as a guest would paste it
        const QString code = r.auth.createInvite(3600, QDateTime::currentSecsSinceEpoch());
        ShareClient::Invite inv;
        QVERIFY(ShareClient::parseInvite(QStringLiteral("hop://127.0.0.1:%1/%2").arg(r.server.port()).arg(code), &inv));
        bool done = false, ok = false; qint64 gid = 0; QString key;
        ShareClient::redeem(inv, QStringLiteral("Guest A"), this, [&](bool o, qint64 g, const QString &k, const QString &) { ok = o; gid = g; key = k; done = true; });
        QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
        QVERIFY(ok && gid > 0 && key.size() == 64);

        const ShareEndpoint ep{QStringLiteral("127.0.0.1"), r.server.port(), "guest:" + QByteArray::number(gid), QByteArray::fromHex(key.toLatin1())};
        // a wrong key reports an error, not a hang
        done = false; int status = -1; QString err;
        ShareClient::fetchJson({ep.address, ep.port, ep.identity, QByteArray(32, 'x')}, "GET", "/v1/hello", {}, this,
                               [&](int s, const QJsonObject &, const QString &e) { status = s; err = e; done = true; });
        QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
        QCOMPARE(status, 0);
        QVERIFY(!err.isEmpty());

        // nothing is approved yet; then the host approves the film
        done = false; QJsonObject lib;
        ShareClient::fetchJson(ep, "GET", "/v1/library", {}, this, [&](int s, const QJsonObject &o, const QString &) { status = s; lib = o; done = true; });
        QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
        QCOMPARE(status, 200);
        QVERIFY(lib.value("items").toArray().isEmpty());
        QVERIFY(r.db.setShareGrants(gid, {movie}, true));
        done = false;
        ShareClient::fetchJson(ep, "GET", "/v1/library", {}, this, [&](int, const QJsonObject &o, const QString &) { lib = o; done = true; });
        QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
        QCOMPARE(lib.value("items").toArray().size(), 1);
        const QString itemId = lib.value("items").toArray().first().toObject().value("id").toString();

        // the player reads an ordinary local address; the proxy fetches it from the host
        ShareProxy proxy;
        QVERIFY(proxy.start());
        const QUrl url(proxy.urlFor(ep, itemId));
        QCOMPARE(url.host(), QStringLiteral("127.0.0.1"));
        auto get = [&](const QUrl &u, const QByteArray &range, int &st, QByteArray &head) {
            QTcpSocket s;
            s.connectToHost(QHostAddress::LocalHost, quint16(u.port()));
            if (!s.waitForConnected(2000)) return QByteArray();
            s.write("GET " + u.path().toUtf8() + " HTTP/1.1\r\nHost: x\r\n" + (range.isEmpty() ? QByteArray() : "Range: " + range + "\r\n") + "\r\n");
            QByteArray all;
            QElapsedTimer t; t.start();
            while (t.elapsed() < 8000 && (s.state() == QAbstractSocket::ConnectedState || s.bytesAvailable())) { QTest::qWait(10); all += s.readAll(); }
            const int e = all.indexOf("\r\n\r\n");
            head = e >= 0 ? all.left(e) : all;
            st = head.mid(9, 3).toInt();
            return e >= 0 ? all.mid(e + 4) : QByteArray();
        };
        int st = 0; QByteArray head;
        QCOMPARE(get(url, {}, st, head), data);                                             // the whole film
        QCOMPARE(st, 200);
        QCOMPARE(get(url, "bytes=100000-100999", st, head), data.mid(100000, 1000));         // a seek
        QCOMPARE(st, 206);
        QVERIFY(head.toLower().contains("content-range: bytes 100000-100999/600000"));
        QCOMPARE(get(url, "bytes=-10", st, head), data.right(10));
        get(url, "bytes=999999999-", st, head);
        QCOMPARE(st, 416);                                                                   // the host's errors pass through

        // only what the app asked for is reachable
        const QUrl guess(QStringLiteral("http://127.0.0.1:%1/s/0123456789abcdef0123456789abcdef").arg(proxy.port()));
        get(guess, {}, st, head);
        QCOMPARE(st, 404);
        get(QUrl(QStringLiteral("http://127.0.0.1:%1/v1/library").arg(proxy.port())), {}, st, head);
        QCOMPARE(st, 404);

        // when the host withdraws approval the guest's next read fails
        QVERIFY(r.db.setShareGrants(gid, {movie}, false));
        get(url, "bytes=0-9", st, head);
        QCOMPARE(st, 404);

        // when the host stops, the player gets a clear error rather than a hang
        r.server.stop();
        get(url, "bytes=0-9", st, head);
        QCOMPARE(st, 502);
    }

    void librariesSharedWithMeAreRemembered()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("p.sqlite"));
        qint64 id = 0;
        {
            ProfileDb db;
            QVERIFY(db.open(file));
            QVERIFY(db.shareHosts().isEmpty());
            QCOMPARE(db.addShareHost(ShareHost{0, QStringLiteral("Home"), QString(), 47800, 3, QString(64, QLatin1Char('a')), 1}), qint64(0));   // no address
            QCOMPARE(db.addShareHost(ShareHost{0, QStringLiteral("Home"), QStringLiteral("10.0.0.2"), 47800, 3, QStringLiteral("short"), 1}), qint64(0));   // not a key
            id = db.addShareHost(ShareHost{0, QStringLiteral("Home"), QStringLiteral("10.0.0.2"), 47800, 3, QString(64, QLatin1Char('a')), 1});
            QVERIFY(id > 0);
        }
        ProfileDb db;
        QVERIFY(db.open(file));                                   // still there next time
        QCOMPARE(db.shareHosts().size(), 1);
        QCOMPARE(db.shareHosts().first().guestId, qint64(3));
        QVERIFY(db.setShareHostAddress(id, QStringLiteral("10.0.0.9"), 5000));   // the host's address changed
        QCOMPARE(db.shareHosts().first().address, QStringLiteral("10.0.0.9"));
        QCOMPARE(db.shareHosts().first().port, quint16(5000));
        QVERIFY(!db.setShareHostAddress(id, QString(), 5000));
        QVERIFY(db.removeShareHost(id));
        QVERIFY(db.shareHosts().isEmpty());
    }

    void outsideConnectionsNeedTheSwitch()
    {
        QTemporaryDir dir;
        ProfileDb db;
        QVERIFY(db.open(dir.filePath(QStringLiteral("p.sqlite"))));
        ShareAuth auth(db);
        GrantLibrary lib(db, auth, QStringLiteral("Home"));
        ShareServer server(db, auth, lib);
        const QHostAddress home(QStringLiteral("192.168.1.9")), world(QStringLiteral("8.8.8.8")), world6(QStringLiteral("2001:db8::1"));
        QVERIFY(!server.allowInternet());
        QVERIFY(server.acceptsPeer(home));
        QVERIFY(!server.acceptsPeer(world));                       // the default: home network only
        QVERIFY(!server.acceptsPeer(world6));
        server.setAllowInternet(true);
        QVERIFY(server.acceptsPeer(world) && server.acceptsPeer(world6) && server.acceptsPeer(home));
        server.setAllowInternet(false);
        QVERIFY(!server.acceptsPeer(world));
    }

    void localAddresses()
    {
        for (const char *a : {"127.0.0.1", "10.1.2.3", "172.16.0.9", "172.31.255.1", "192.168.1.50", "169.254.3.4",
                              "::1", "fe80::1", "fd12:3456::1", "::ffff:192.168.1.5"})
            QVERIFY2(ShareServer::isLocalAddress(QHostAddress(QString::fromLatin1(a))), a);
        for (const char *a : {"8.8.8.8", "172.32.0.1", "172.15.255.255", "11.0.0.1", "2001:db8::1", "::ffff:8.8.8.8", "100.64.0.1"})
            QVERIFY2(!ShareServer::isLocalAddress(QHostAddress(QString::fromLatin1(a))), a);
    }

    // ---- the server ----
    void joiningWithAnInviteThenReturningWithTheKey()
    {
        if (!ShareServer::encryptionAvailable()) QSKIP("no TLS library");
        Rig r;
        QVERIFY(r.ok());
        const QString code = r.auth.createInvite(3600, QDateTime::currentSecsSinceEpoch());
        const QByteArray label = ShareAuth::normalizeCode(code).left(4).toLatin1();
        QSignalSpy joined(&r.server, &ShareServer::inviteRedeemed);

        // an invite connection can do one thing only
        Reply nope = ask(r.server.port(), "invite:" + label, ShareAuth::secretFromCode(code), get("/v1/library"));
        QVERIFY(nope.connected);
        QCOMPARE(nope.status, 403);

        const QString code2 = r.auth.createInvite(3600, QDateTime::currentSecsSinceEpoch());
        const QByteArray label2 = ShareAuth::normalizeCode(code2).left(4).toLatin1();
        const QByteArray body = "{\"name\":\"Guest A\"}";
        const Reply red = ask(r.server.port(), "invite:" + label2, ShareAuth::secretFromCode(code2),
                              "POST /v1/redeem HTTP/1.1\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\n\r\n" + body);
        QCOMPARE(red.status, 200);
        QCOMPARE(joined.count(), 1);
        const QString key = red.json().value("key").toString();
        const qint64 id = qint64(red.json().value("guest").toDouble());
        QCOMPARE(key.size(), 64);
        QCOMPARE(r.db.shareGuests().first().name, QStringLiteral("Guest A"));

        // the invite is used up
        QVERIFY(!ask(r.server.port(), "invite:" + label2, ShareAuth::secretFromCode(code2), get("/v1/hello")).connected);

        // the guest comes back with the key
        const Reply hi = ask(r.server.port(), "guest:" + QByteArray::number(id), hex(key), get("/v1/hello"));
        QCOMPARE(hi.status, 200);
        QCOMPARE(hi.json().value("name").toString(), QStringLiteral("Home"));
        QCOMPARE(hi.json().value("app").toString(), QStringLiteral("hop-tunes"));
    }

    void wrongKeysAndUnknownGuestsAreRefusedAtTheHandshake()
    {
        if (!ShareServer::encryptionAvailable()) QSKIP("no TLS library");
        Rig r;
        QVERIFY(r.ok());
        const auto [id, key] = r.join(QStringLiteral("Guest A"));
        QVERIFY(id > 0);
        const QByteArray gid = "guest:" + QByteArray::number(id);
        QByteArray wrong = hex(key); wrong[0] = char(wrong[0] ^ 0x55);
        QVERIFY(!ask(r.server.port(), gid, wrong, get("/v1/hello")).connected);
        QVERIFY(!ask(r.server.port(), "guest:9999", hex(key), get("/v1/hello")).connected);
        QVERIFY(!ask(r.server.port(), "who:me", hex(key), get("/v1/hello")).connected);
        QVERIFY(ask(r.server.port(), gid, hex(key), get("/v1/hello")).connected);
    }

    void guessingAnInviteCancelsIt()
    {
        if (!ShareServer::encryptionAvailable()) QSKIP("no TLS library");
        Rig r;
        QVERIFY(r.ok());
        const QString code = r.auth.createInvite(3600, QDateTime::currentSecsSinceEpoch());
        const QByteArray label = ShareAuth::normalizeCode(code).left(4).toLatin1();
        for (int i = 0; i < ShareAuth::kMaxMisses; ++i)
            QVERIFY(!ask(r.server.port(), "invite:" + label, QByteArray(32, 'x'), get("/v1/hello")).connected);
        QTest::qWait(100);
        // now even the right code gets nowhere
        QVERIFY(!ask(r.server.port(), "invite:" + label, ShareAuth::secretFromCode(code), get("/v1/hello")).connected);
        QCOMPARE(r.auth.pendingInvites(), 0);
    }

    void aGuestSeesNothingUntilApprovedAndNeverAPath()
    {
        if (!ShareServer::encryptionAvailable()) QSKIP("no TLS library");
        Rig r;
        QVERIFY(r.ok());
        const auto [id, key] = r.join(QStringLiteral("Guest A"));
        const auto [id2, key2] = r.join(QStringLiteral("Guest B"));
        const QByteArray gid = "guest:" + QByteArray::number(id), gid2 = "guest:" + QByteArray::number(id2);
        const QString movie = r.file(QStringLiteral("Secret Movie.mkv"), QByteArray(1000, 'm'));
        const QString song = r.file(QStringLiteral("A Song.mp3"), QByteArray(500, 's'));

        Reply lib = ask(r.server.port(), gid, hex(key), get("/v1/library"));
        QCOMPARE(lib.status, 200);
        QVERIFY(lib.json().value("items").toArray().isEmpty());             // the waiting room

        QVERIFY(r.db.setShareGrants(id, {movie}, true));
        lib = ask(r.server.port(), gid, hex(key), get("/v1/library"));
        const QJsonArray items = lib.json().value("items").toArray();
        QCOMPARE(items.size(), 1);
        QCOMPARE(items.first().toObject().value("title").toString(), QStringLiteral("Secret Movie"));
        QCOMPARE(items.first().toObject().value("kind").toString(), QStringLiteral("video"));
        QVERIFY(!lib.body.contains(r.dir.path().toUtf8()));                // no path ever leaves the machine
        QVERIFY(!lib.body.contains("A Song"));                             // not approved: not even named

        // another guest sees none of it, and ids differ between guests
        QVERIFY(ask(r.server.port(), gid2, hex(key2), get("/v1/library")).json().value("items").toArray().isEmpty());
        QVERIFY(r.db.setShareGrants(id2, {movie}, true));
        const QString id1 = items.first().toObject().value("id").toString();
        const QString idOfB = ask(r.server.port(), gid2, hex(key2), get("/v1/library")).json().value("items").toArray().first().toObject().value("id").toString();
        QVERIFY(!idOfB.isEmpty() && idOfB != id1);
        QCOMPARE(ask(r.server.port(), gid2, hex(key2), get("/v1/stream/" + id1.toLatin1())).status, 404);   // one guest's id means nothing to another
        // an unapproved file cannot be reached even by guessing: there are no paths, only ids
        QCOMPARE(ask(r.server.port(), gid, hex(key), get("/v1/stream/" + QByteArray(32, '0'))).status, 404);
        Q_UNUSED(song);
    }

    void streamingHonoursRangesAndIsNeverADownloadRoute()
    {
        if (!ShareServer::encryptionAvailable()) QSKIP("no TLS library");
        Rig r;
        QVERIFY(r.ok());
        const auto [id, key] = r.join(QStringLiteral("Guest A"));
        const QByteArray gid = "guest:" + QByteArray::number(id);
        QByteArray data(300000, 0);
        for (int i = 0; i < data.size(); ++i) data[i] = char(i * 7);
        const QString movie = r.file(QStringLiteral("m.mkv"), data);
        QVERIFY(r.db.setShareGrants(id, {movie}, true));
        const QByteArray item = ask(r.server.port(), gid, hex(key), get("/v1/library")).json().value("items").toArray().first().toObject().value("id").toString().toLatin1();
        const QByteArray route = "/v1/stream/" + item;

        Reply whole = ask(r.server.port(), gid, hex(key), get(route));
        QCOMPARE(whole.status, 200);
        QCOMPARE(whole.body, data);
        QVERIFY(whole.head.contains("Accept-Ranges: bytes"));
        QVERIFY(whole.head.contains("Cache-Control: no-store"));

        Reply part = ask(r.server.port(), gid, hex(key), get(route, "Range: bytes=1000-1999\r\n"));
        QCOMPARE(part.status, 206);
        QCOMPARE(part.body, data.mid(1000, 1000));
        QVERIFY(part.head.contains("Content-Range: bytes 1000-1999/300000"));
        QCOMPARE(ask(r.server.port(), gid, hex(key), get(route, "Range: bytes=299990-\r\n")).body, data.mid(299990));
        QCOMPARE(ask(r.server.port(), gid, hex(key), get(route, "Range: bytes=-10\r\n")).body, data.right(10));
        QCOMPARE(ask(r.server.port(), gid, hex(key), get(route, "Range: bytes=400000-\r\n")).status, 416);
        QCOMPARE(ask(r.server.port(), gid, hex(key), get(route, "Range: bytes=5-2\r\n")).status, 416);
        QCOMPARE(ask(r.server.port(), gid, hex(key), get(route, "Range: bytes=0-1,5-6\r\n")).status, 416);

        // no download or file-listing routes, and nothing but GET/HEAD
        for (const char *p : {"/v1/download", "/v1/download/x", "/v1/files", "/", "/v1/stream", "/v1/redeem"})
            QCOMPARE(ask(r.server.port(), gid, hex(key), get(p)).status, 404);
        QCOMPARE(ask(r.server.port(), gid, hex(key), "DELETE " + route + " HTTP/1.1\r\n\r\n").status, 405);
        QCOMPARE(ask(r.server.port(), gid, hex(key), "POST " + route + " HTTP/1.1\r\nContent-Length: 0\r\n\r\n").status, 405);
    }

    void withdrawingApprovalOrRemovingAGuestCutsOffAccess()
    {
        if (!ShareServer::encryptionAvailable()) QSKIP("no TLS library");
        Rig r;
        QVERIFY(r.ok());
        const auto [id, key] = r.join(QStringLiteral("Guest A"));
        const QByteArray gid = "guest:" + QByteArray::number(id);
        const QString movie = r.file(QStringLiteral("m.mkv"), QByteArray(5000000, 'x'));
        QVERIFY(r.db.setShareGrants(id, {movie}, true));
        const QByteArray item = ask(r.server.port(), gid, hex(key), get("/v1/library")).json().value("items").toArray().first().toObject().value("id").toString().toLatin1();
        QCOMPARE(ask(r.server.port(), gid, hex(key), get("/v1/stream/" + item, "Range: bytes=0-9\r\n")).status, 206);

        QVERIFY(r.db.setShareGrants(id, {movie}, false));                  // approval withdrawn
        QCOMPARE(ask(r.server.port(), gid, hex(key), get("/v1/stream/" + item)).status, 404);
        QVERIFY(ask(r.server.port(), gid, hex(key), get("/v1/library")).json().value("items").toArray().isEmpty());

        QVERIFY(r.db.setShareGrants(id, {movie}, true));
        QVERIFY(r.auth.removeGuest(id));                                    // the guest is removed: the key is dead
        QVERIFY(!ask(r.server.port(), gid, hex(key), get("/v1/hello")).connected);
    }

    void aStreamInProgressIsCutWhenApprovalIsWithdrawn()
    {
        if (!ShareServer::encryptionAvailable()) QSKIP("no TLS library");
        Rig r;
        QVERIFY(r.ok());
        const auto [id, key] = r.join(QStringLiteral("Guest A"));
        const QString movie = r.file(QStringLiteral("big.mkv"), QByteArray(40 * 1024 * 1024, 'x'));
        QVERIFY(r.db.setShareGrants(id, {movie}, true));
        const QByteArray gid = "guest:" + QByteArray::number(id);
        const QByteArray item = ask(r.server.port(), gid, hex(key), get("/v1/library")).json().value("items").toArray().first().toObject().value("id").toString().toLatin1();

        QSslConfiguration conf = QSslConfiguration::defaultConfiguration();
        conf.setProtocol(QSsl::TlsV1_2);
        conf.setCiphers({QSslCipher(QStringLiteral("ECDHE-PSK-CHACHA20-POLY1305"), QSsl::TlsV1_2)});
        conf.setPeerVerifyMode(QSslSocket::VerifyNone);
        QSslSocket s;
        s.setSslConfiguration(conf);
        const QByteArray k = hex(key);
        QObject::connect(&s, &QSslSocket::preSharedKeyAuthenticationRequired, [&](QSslPreSharedKeyAuthenticator *a) { a->setIdentity(gid); a->setPreSharedKey(k); });
        s.setReadBufferSize(64 * 1024);   // the client reads slowly, so the server is mid-stream
        s.connectToHostEncrypted(QStringLiteral("127.0.0.1"), r.server.port());
        QVERIFY(waitEncrypted(s, 4000));
        s.write(get("/v1/stream/" + item));
        qint64 got = 0;
        QElapsedTimer t; t.start();
        while (got < 1024 * 1024 && t.elapsed() < 4000) { QTest::qWait(20); got += s.readAll().size(); }
        QVERIFY(got >= 1024 * 1024);
        QVERIFY(r.db.setShareGrants(id, {movie}, false));                  // withdrawn mid-stream
        QElapsedTimer t2; t2.start();
        while (s.state() == QAbstractSocket::ConnectedState && got < 40 * 1024 * 1024 && t2.elapsed() < 6000) { QTest::qWait(20); got += s.readAll().size(); }
        QVERIFY(got < 40 * 1024 * 1024);                                   // it did not finish
    }

    void connectedDotFollowsConnections()
    {
        if (!ShareServer::encryptionAvailable()) QSKIP("no TLS library");
        Rig r;
        QVERIFY(r.ok());
        const auto [id, key] = r.join(QStringLiteral("Guest A"));
        QVERIFY(r.server.connectedGuests().isEmpty());
        QSslConfiguration conf = QSslConfiguration::defaultConfiguration();
        conf.setProtocol(QSsl::TlsV1_2);
        conf.setCiphers({QSslCipher(QStringLiteral("ECDHE-PSK-CHACHA20-POLY1305"), QSsl::TlsV1_2)});
        conf.setPeerVerifyMode(QSslSocket::VerifyNone);
        QSslSocket s;
        s.setSslConfiguration(conf);
        const QByteArray gid = "guest:" + QByteArray::number(id), k = hex(key);
        QObject::connect(&s, &QSslSocket::preSharedKeyAuthenticationRequired, [&](QSslPreSharedKeyAuthenticator *a) { a->setIdentity(gid); a->setPreSharedKey(k); });
        s.connectToHostEncrypted(QStringLiteral("127.0.0.1"), r.server.port());
        QVERIFY(waitEncrypted(s, 4000));
        QTRY_COMPARE(r.server.connectedGuests(), QList<qint64>{id});
        r.server.kick(id);
        QTRY_VERIFY(r.server.connectedGuests().isEmpty());
    }

    void oversizedAndBrokenRequestsAreRefused()
    {
        if (!ShareServer::encryptionAvailable()) QSKIP("no TLS library");
        Rig r;
        QVERIFY(r.ok());
        const auto [id, key] = r.join(QStringLiteral("Guest A"));
        const QByteArray gid = "guest:" + QByteArray::number(id);
        QCOMPARE(ask(r.server.port(), gid, hex(key), "GET /v1/hello HTTP/1.1\r\nX: " + QByteArray(40000, 'a') + "\r\n\r\n").status, 431);
        QCOMPARE(ask(r.server.port(), gid, hex(key), "POST /v1/hello HTTP/1.1\r\nContent-Length: 999999\r\n\r\n").status, 413);
        QCOMPARE(ask(r.server.port(), gid, hex(key), "nonsense\r\n\r\n").status, 400);
    }

    void stoppingClosesEverything()
    {
        if (!ShareServer::encryptionAvailable()) QSKIP("no TLS library");
        Rig r;
        QVERIFY(r.ok());
        const auto [id, key] = r.join(QStringLiteral("Guest A"));
        const quint16 port = r.server.port();
        r.server.stop();
        QVERIFY(!r.server.isListening());
        QVERIFY(!ask(port, "guest:" + QByteArray::number(id), hex(key), get("/v1/hello"), 800).connected);
    }
};

QTEST_MAIN(TestShare)
#include "test_share.moc"
