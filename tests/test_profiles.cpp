// SPDX-License-Identifier: GPL-3.0-or-later
// Local profiles (spec §3): profile list, PIN hashing, per-profile settings, removal.
#include "core/ProfileStore.h"
#include "library/ProfileArchive.h"
#include "library/ProfileDb.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QtTest>

using namespace hop;

class TestProfiles : public QObject {
    Q_OBJECT
private slots:
    void pbkdf2MatchesReference()
    {
        // RFC 7914 §11 vector (PBKDF2-HMAC-SHA256, P="passwd", S="salt", c=1), first block.
        QVERIFY(ProfileStore::verifyPin(QStringLiteral("passwd"),
            QStringLiteral("pbkdf2-sha256$1$c2FsdA==$VawEblbjCJ/sFpHCJUS2BflBhSFt3gRl5oudV8INrLw=")));
        // Same format as produced by Python's hashlib.pbkdf2_hmac, so other tools can read it.
        const QString py = QStringLiteral("pbkdf2-sha256$1000$MDEyMzQ1Njc4OWFiY2RlZg==$RykCuFJiXjaaWy/+Tf9rr0wUj5t7g5zOKdXms+nU5TA=");
        QVERIFY(ProfileStore::verifyPin(QStringLiteral("2468"), py));
        QVERIFY(!ProfileStore::verifyPin(QStringLiteral("2469"), py));
    }

    void pinHashRoundTrip()
    {
        const QString h = ProfileStore::hashPin(QStringLiteral("1234"));
        QVERIFY(h.startsWith(QStringLiteral("pbkdf2-sha256$")));
        QVERIFY(!h.contains(QStringLiteral("1234$")));
        QVERIFY(ProfileStore::verifyPin(QStringLiteral("1234"), h));
        QVERIFY(!ProfileStore::verifyPin(QStringLiteral("4321"), h));
        QVERIFY(h != ProfileStore::hashPin(QStringLiteral("1234")));   // salted
        QVERIFY(!ProfileStore::verifyPin(QStringLiteral("1234"), QStringLiteral("garbage")));
        QVERIFY(!ProfileStore::verifyPin(QStringLiteral("1234"), QStringLiteral("pbkdf2-sha256$0$AA==$AA==")));
    }

    void validation()
    {
        QVERIFY(ProfileStore::isValidPin(QStringLiteral("0000")));
        QVERIFY(ProfileStore::isValidPin(QStringLiteral("12345678")));
        QVERIFY(!ProfileStore::isValidPin(QStringLiteral("123")));
        QVERIFY(!ProfileStore::isValidPin(QStringLiteral("123456789")));
        QVERIFY(!ProfileStore::isValidPin(QStringLiteral("12a4")));
        QVERIFY(!ProfileStore::isValidName(QStringLiteral("   ")));
        QVERIFY(!ProfileStore::isValidName(QString(41, QLatin1Char('x'))));
        QVERIFY(ProfileStore::isValidAccent(QStringLiteral("#4fd1c5")));
        QVERIFY(!ProfileStore::isValidAccent(QStringLiteral("red")));
    }

    void createPersistAndReload()
    {
        QTemporaryDir tmp;
        const QString root = tmp.filePath(QStringLiteral("profiles"));
        ProfileStore s(root);
        QVERIFY(s.load());
        QCOMPARE(s.count(), 0);
        QVERIFY(s.create(QStringLiteral(" "), QStringLiteral("#ffffff")).isEmpty());
        QVERIFY(s.create(QStringLiteral("Ana"), QStringLiteral("#ffffff"), QStringLiteral("12")).isEmpty());

        const QString a = s.create(QStringLiteral("  Ana "), QStringLiteral("#FF6B9A"));
        const QString b = s.create(QStringLiteral("Ben"), QStringLiteral("nope"), QStringLiteral("9876"));
        QVERIFY(!a.isEmpty() && !b.isEmpty());
        QCOMPARE(s.find(a)->name, QStringLiteral("Ana"));
        QCOMPARE(s.find(a)->accent, QStringLiteral("#ff6b9a"));
        QCOMPARE(s.find(b)->accent, QStringLiteral("#4fd1c5"));   // invalid accent falls back

        // Private files: profile list 0600, folders 0700, no PIN in clear text.
        const QString json = root + QStringLiteral("/profiles.json");
#ifndef Q_OS_WIN   // group/other permission bits are a POSIX idea; Windows uses per-user folder ACLs
        QCOMPARE(QFile::permissions(json) & 0x0077, QFileDevice::Permissions{});
        QCOMPARE(QFile::permissions(s.dirFor(a)) & 0x0077, QFileDevice::Permissions{});
#endif
        QFile f(json);
        QVERIFY(f.open(QIODevice::ReadOnly));
        // The PIN must not be stored as a value. (A bare search for "9876" is not enough: the random salt,
        // hash and ids in the file contain every 4-digit sequence now and then, which made this test fail
        // about once in a thousand runs.)
        const QByteArray raw = f.readAll();
        QVERIFY(!raw.contains("\"9876\""));
        QVERIFY(!raw.contains(":9876"));
        QVERIFY(!raw.contains(": 9876"));

        QVERIFY(s.saveSettings(a, {{QStringLiteral("folders"), QJsonArray{QStringLiteral("/music")}}}));
        QVERIFY(!s.saveSettings(QStringLiteral("not-a-profile"), {}));

        ProfileStore again(root);
        QVERIFY(again.load());
        QCOMPARE(again.count(), 2);
        QVERIFY(!again.find(a)->hasPin);
        QVERIFY(again.find(b)->hasPin);
        QVERIFY(again.checkPin(a, {}));
        QVERIFY(again.checkPin(b, QStringLiteral("9876")));
        QVERIFY(!again.checkPin(b, QStringLiteral("0000")));
        QCOMPARE(again.settings(a).value(QStringLiteral("folders")).toArray().first().toString(), QStringLiteral("/music"));
    }

    void editPinAndRemove()
    {
        QTemporaryDir tmp;
        ProfileStore s(tmp.filePath(QStringLiteral("p")));
        QVERIFY(s.load());
        const QString id = s.create(QStringLiteral("Cy"), QStringLiteral("#7cc4ff"));
        QVERIFY(s.setPin(id, QStringLiteral("5555")));
        QVERIFY(s.find(id)->hasPin);
        QVERIFY(!s.setPin(id, QStringLiteral("55")));
        QVERIFY(s.checkPin(id, QStringLiteral("5555")));
        QVERIFY(s.setPin(id, {}));
        QVERIFY(!s.find(id)->hasPin);
        QVERIFY(s.rename(id, QStringLiteral("Cyrus")));
        QVERIFY(!s.rename(id, QString()));

        // Removing deletes only the profile folder.
        const QString keep = tmp.filePath(QStringLiteral("song.flac"));
        QFile(keep).open(QIODevice::WriteOnly);
        QFile(s.dirFor(id) + QStringLiteral("/settings.json")).open(QIODevice::WriteOnly);
        QVERIFY(s.remove(id));
        QVERIFY(!QDir(s.dirFor(id)).exists());
        QVERIFY(QFile::exists(keep));
        QCOMPARE(s.count(), 0);
        QVERIFY(!s.remove(id));
    }

    void rejectsHostileIds()
    {
        QTemporaryDir tmp;
        const QString root = tmp.filePath(QStringLiteral("p"));
        QDir().mkpath(root);
        QFile f(root + QStringLiteral("/profiles.json"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(R"({"version":1,"profiles":[{"id":"../../etc","name":"x"},{"id":"6f1c1f0e-8f0a-4c8e-9a3e-2f1d3c4b5a69","name":"ok","accent":"bad"}]})");
        f.close();
        ProfileStore s(root);
        QVERIFY(s.load());
        QCOMPARE(s.count(), 1);
        QCOMPARE(s.profiles().first().name, QStringLiteral("ok"));
        QCOMPARE(s.profiles().first().accent, QStringLiteral("#4fd1c5"));
    }

    void unreadableListIsAnError()
    {
        QTemporaryDir tmp;
        const QString root = tmp.filePath(QStringLiteral("p"));
        QDir().mkpath(root);
        QFile f(root + QStringLiteral("/profiles.json"));
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ not json");
        f.close();
        ProfileStore s(root);
        QVERIFY(!s.load());   // the app then moves the file aside instead of overwriting it
    }
    // ---- eject / export / import -------------------------------------------------------
    static ProfileArchive sampleArchive(QTemporaryDir &tmp)
    {
        ProfileDb db;
        const bool opened = db.open(tmp.filePath(QStringLiteral("src.sqlite")));
        Q_ASSERT(opened);
        if (!opened) qFatal("cannot open test database");
        db.recordPlay(QStringLiteral("/m/a.mp3"), 1000);
        db.recordPlay(QStringLiteral("/m/a.mp3"), 2000);
        db.setHearted(QStringLiteral("/m/a.mp3"), true);
        const qint64 pl = db.createPlaylist(QStringLiteral("Road trip"));
        db.setPlaylistItems(pl, {QStringLiteral("/m/a.mp3"), QStringLiteral("/m/b.mp3")});
        db.pin({QStringLiteral("artist"), QStringLiteral("Ana"), QStringLiteral("Ana")});
        db.setVideoProgress(QStringLiteral("/v/x.mkv"), 5000, 90000);
        db.setAlbumOverrideTitle(QStringLiteral("Ana\x1f" "Songs"), QStringLiteral("Better title"));
        db.setAlbumOverrideArt(QStringLiteral("Ana\x1f" "Songs"), QStringLiteral("/secret/cover.jpg"));
        db.setVideoThumbOverride(QStringLiteral("/v/x.mkv"), QStringLiteral("/secret/t.jpg"));
        db.addVideoComment(QStringLiteral("/v/x.mkv"), QStringLiteral("great one"), 42);
        ProfileArchive a;
        a.name = QStringLiteral("Ana");
        a.accent = QStringLiteral("#ff8800");
        a.appVersion = QStringLiteral("1.7.3");
        a.settings = {{QStringLiteral("folders"), QJsonArray{QStringLiteral("/nonexistent/music")}}, {QStringLiteral("volume"), 0.5}};
        a.data = db.exportData();
        return a;
    }

    void exportRoundTripsAndKeepsEverythingButPaths()
    {
        QTemporaryDir tmp;
        const ProfileArchive a = sampleArchive(tmp);
        ProfileArchive b;
        QString err;
        QVERIFY2(ProfileArchive::parse(a.toBytes(), &b, &err), qPrintable(err));
        QCOMPARE(b.name, QStringLiteral("Ana"));
        QCOMPARE(b.folders(), QStringList{QStringLiteral("/nonexistent/music")});
        QCOMPARE(b.rowCount(QStringLiteral("playlist")), 1);
        QCOMPARE(b.rowCount(QStringLiteral("playlist_item")), 2);

        ProfileDb fresh;
        QVERIFY(fresh.open(tmp.filePath(QStringLiteral("dst.sqlite"))));
        QVERIFY2(fresh.importData(b.data, &err), qPrintable(err));
        QCOMPARE(fresh.allStats().value(QStringLiteral("/m/a.mp3")).playCount, 2);
        QVERIFY(fresh.allStats().value(QStringLiteral("/m/a.mp3")).hearted);
        QCOMPARE(fresh.playlists().size(), 1);
        QCOMPARE(fresh.playlistItems(fresh.playlists().first().id).size(), 2);
        QCOMPARE(fresh.pins().size(), 1);
        QCOMPARE(fresh.videoProgress(QStringLiteral("/v/x.mkv")).positionMs, qint64(5000));
        QCOMPARE(fresh.videoComments(QStringLiteral("/v/x.mkv")).size(), 1);
        // a title travels, a path on the old device does not
        const auto ov = fresh.albumOverrides().value(QStringLiteral("Ana\x1f" "Songs"));
        QCOMPARE(ov.title, QStringLiteral("Better title"));
        QVERIFY(ov.art.isEmpty());
        QVERIFY(fresh.videoThumbOverrides().isEmpty());
        QVERIFY(!a.toBytes().contains("secret"));
        QVERIFY(!a.toBytes().contains("pbkdf2"));
    }

    void badFilesAreRefusedAndChangeNothing()
    {
        QTemporaryDir tmp;
        const ProfileArchive a = sampleArchive(tmp);
        ProfileArchive out;
        QString err;
        QVERIFY(!ProfileArchive::parse("", &out, &err));
        QVERIFY(!ProfileArchive::parse("not json at all", &out, &err));
        QVERIFY(!ProfileArchive::parse("{\"app\":\"other\",\"format\":1}", &out, &err));
        QVERIFY(!ProfileArchive::parse("{\"app\":\"hop-tunes\",\"format\":99}", &out, &err));
        QVERIFY(err.contains(QStringLiteral("newer")));
        QVERIFY(!ProfileArchive::parse("{\"app\":\"hop-tunes\",\"format\":1,\"profile\":{\"name\":\" \"},\"settings\":{},\"data\":{}}", &out, &err));

        int counter = 0;
        auto tryData = [&](const QJsonObject &data) {
            ProfileDb db;
            const QString path = tmp.filePath(QStringLiteral("bad%1.sqlite").arg(++counter));
            if (!db.open(path)) qFatal("cannot open test database");
            QString e;
            const bool ok = db.importData(data, &e);
            return std::make_pair(ok, db.allStats().size() + db.playlists().size());
        };
        QJsonObject d = a.data;
        d.insert(QStringLiteral("evil_table"), QJsonArray{});
        QVERIFY(!tryData(d).first);
        d = a.data;
        d.insert(QStringLiteral("stats"), QJsonArray{QJsonArray{QStringLiteral("x")}});           // wrong column count
        QVERIFY(!tryData(d).first);
        d = a.data;
        d.insert(QStringLiteral("stats"), QJsonArray{QJsonArray{1, 2, 3, 4, 5}});                 // wrong types
        auto r = tryData(d);
        QVERIFY(!r.first);
        QCOMPARE(r.second, 0);                                                                    // nothing half-imported
        d = a.data;
        d.insert(QStringLiteral("playlist_item"), QJsonArray{QJsonArray{999, 0, QStringLiteral("/x")}});   // orphan item
        QVERIFY(!tryData(d).first);
    }
};

QTEST_GUILESS_MAIN(TestProfiles)
#include "test_profiles.moc"
