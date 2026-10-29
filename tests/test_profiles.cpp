// SPDX-License-Identifier: GPL-3.0-or-later
// Local profiles (spec §3): profile list, PIN hashing, per-profile settings, removal.
#include "core/ProfileStore.h"

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
};

QTEST_GUILESS_MAIN(TestProfiles)
#include "test_profiles.moc"
