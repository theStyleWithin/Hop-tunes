// SPDX-License-Identifier: GPL-3.0-or-later
// Library sharing: invites, guest keys and per-guest approvals (docs/design-sharing.md). No networking yet.
#include "library/ProfileDb.h"
#include "library/ShareAuth.h"

#include <QtTest>

using namespace hop;

class TestShare : public QObject {
    Q_OBJECT
private slots:
    void inviteIsOneTimeAndTradesForAKey()
    {
        QTemporaryDir dir;
        ProfileDb db;
        QVERIFY(db.open(dir.filePath(QStringLiteral("p.sqlite"))));
        ShareAuth auth(db);
        const QString code = auth.createInvite(3600, 1000);
        QCOMPARE(code.size(), ShareAuth::kCodeLength);
        QVERIFY(!code.contains(QLatin1Char('0')) && !code.contains(QLatin1Char('O')) && !code.contains(QLatin1Char('1')));

        // typed any way a person would: lower case, spaces, a dash
        const QString typed = code.left(4).toLower() + QStringLiteral(" - ") + code.mid(4).toLower();
        const QString key = auth.redeem(typed, QStringLiteral("  Dad  "), 1100);
        QCOMPARE(key.size(), 64);
        const qint64 id = auth.authenticate(key);
        QVERIFY(id > 0);
        QCOMPARE(db.shareGuests().size(), 1);
        QCOMPARE(db.shareGuests().first().name, QStringLiteral("Dad"));

        QVERIFY(auth.redeem(code, QStringLiteral("Dad again"), 1200).isEmpty());   // used up
        QCOMPARE(db.shareGuests().size(), 1);
    }

    void keysAreNeverStoredAndUnknownOnesAreRefused()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("p.sqlite"));
        QString key;
        {
            ProfileDb db;
            QVERIFY(db.open(file));
            ShareAuth auth(db);
            key = auth.redeem(auth.createInvite(3600, 0), QStringLiteral("Dad"), 5);
            QVERIFY(auth.authenticate(key) > 0);
            QVERIFY(auth.authenticate(QString(64, QLatin1Char('a'))) == 0);
            QVERIFY(auth.authenticate(QString()) == 0);
            QVERIFY(auth.authenticate(key.left(63)) == 0);
        }
        // the key itself is nowhere in the database file
        QFile f(file);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QVERIFY(!f.readAll().contains(key.toLatin1()));
        f.close();
        // and it still works after a restart
        ProfileDb db;
        QVERIFY(db.open(file));
        QVERIFY(ShareAuth(db).authenticate(key) > 0);
    }

    void expiredAndWrongCodesAreRefusedAndGuessingIsCutOff()
    {
        QTemporaryDir dir;
        ProfileDb db;
        QVERIFY(db.open(dir.filePath(QStringLiteral("p.sqlite"))));
        ShareAuth auth(db);
        const QString old = auth.createInvite(600, 0);
        QVERIFY(auth.redeem(old, QStringLiteral("x"), 601).isEmpty());   // ten minutes is the shortest, and it ran out
        QCOMPARE(db.shareGuests().size(), 0);

        const QString good = auth.createInvite(600, 1000);
        for (int i = 0; i < ShareAuth::kMaxMisses; ++i)
            QVERIFY(auth.redeem(QStringLiteral("WRONGONE"), QStringLiteral("x"), 1001).isEmpty());
        QCOMPARE(auth.pendingInvites(), 0);                              // every invite was cancelled
        QVERIFY(auth.redeem(good, QStringLiteral("x"), 1002).isEmpty());   // even the right code is now dead
        QCOMPARE(db.shareGuests().size(), 0);

        const QString fresh = auth.createInvite(600, 2000);              // the host can simply make a new one
        QVERIFY(!auth.redeem(fresh, QStringLiteral("x"), 2001).isEmpty());
    }

    void nothingIsSharedUntilApprovedAndRemovalForgetsEverything()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("p.sqlite"));
        qint64 dad = 0, mum = 0;
        QString dadKey;
        {
            ProfileDb db;
            QVERIFY(db.open(file));
            ShareAuth auth(db);
            dadKey = auth.redeem(auth.createInvite(3600, 0), QStringLiteral("Dad"), 1);
            const QString mumKey = auth.redeem(auth.createInvite(3600, 0), QStringLiteral("Mum"), 2);
            dad = auth.authenticate(dadKey);
            mum = auth.authenticate(mumKey);
            QVERIFY(dad > 0 && mum > 0 && dad != mum);

            const QString movie = QStringLiteral("/m/movie.mkv"), song = QStringLiteral("/m/song.mp3");
            QVERIFY(!db.isShared(dad, movie));                           // default: nothing
            QVERIFY(db.shareGrants(dad).isEmpty());
            QVERIFY(db.setShareGrants(dad, {movie, song}, true));
            QVERIFY(db.setShareGrants(dad, {movie}, true));              // approving twice is harmless
            QVERIFY(db.isShared(dad, movie) && db.isShared(dad, song));
            QVERIFY(!db.isShared(mum, movie));                           // approvals are per guest
            QVERIFY(!db.isShared(dad, QStringLiteral("/m/other.mkv")));
            QVERIFY(db.setShareGrants(dad, {song}, false));              // taking one back
            QVERIFY(!db.isShared(dad, song) && db.isShared(dad, movie));
            QVERIFY(!db.isShared(0, movie));                             // no guest, no access
        }
        // approvals are remembered for the next visit
        ProfileDb db;
        QVERIFY(db.open(file));
        ShareAuth auth(db);
        QCOMPARE(auth.authenticate(dadKey), dad);
        QVERIFY(db.isShared(dad, QStringLiteral("/m/movie.mkv")));

        QVERIFY(auth.removeGuest(dad));
        QCOMPARE(auth.authenticate(dadKey), qint64(0));                  // the key stops working
        QVERIFY(db.shareGrants(dad).isEmpty());                          // and the approvals are gone
        QCOMPARE(db.shareGuests().size(), 1);
    }
};

QTEST_APPLESS_MAIN(TestShare)
#include "test_share.moc"
