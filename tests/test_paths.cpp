// SPDX-License-Identifier: GPL-3.0-or-later
// Paths: HOPTUNES_DATA_DIR moves data and cache under one folder (the portable build).
#include "core/Paths.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace hop;

class TestPaths : public QObject {
    Q_OBJECT
private slots:
    // Android's folder picker hands back content:// tree addresses; they become ordinary paths.
    void androidFolderPickerAddressesBecomePaths()
    {
        using hop::Paths::folderPath;
        QCOMPARE(folderPath(QUrl(QStringLiteral("content://com.android.externalstorage.documents/tree/primary%3AMusic"))),
                 QStringLiteral("/storage/emulated/0/Music"));
        QCOMPARE(folderPath(QUrl(QStringLiteral("content://com.android.externalstorage.documents/tree/primary%3AMovies%2FShows"))),
                 QStringLiteral("/storage/emulated/0/Movies/Shows"));
        QCOMPARE(folderPath(QUrl(QStringLiteral("content://com.android.externalstorage.documents/tree/1234-ABCD%3AMusic"))),
                 QStringLiteral("/storage/1234-ABCD/Music"));
        QCOMPARE(folderPath(QUrl::fromLocalFile(QStringLiteral("/home/x/Music"))), QStringLiteral("/home/x/Music"));
    }
    void dataDirOverrideKeepsEverythingTogether()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        qputenv("HOPTUNES_DATA_DIR", root.path().toLocal8Bit());
        const QString r = QDir::cleanPath(root.path());
        QCOMPARE(Paths::dataDir(), r + QStringLiteral("/data"));
        QCOMPARE(Paths::cacheDir(), r + QStringLiteral("/cache"));
        QVERIFY(Paths::indexDbPath().startsWith(r + QStringLiteral("/data/")));
        QVERIFY(Paths::sectionIndexDbPath().startsWith(r + QStringLiteral("/data/")));
        QVERIFY(Paths::videoThumbCacheDir().startsWith(r + QStringLiteral("/cache/")));
        QVERIFY(Paths::ensureDirs());
        QVERIFY(QFileInfo(r + QStringLiteral("/data/profiles")).isDir());
        QVERIFY(QFileInfo(r + QStringLiteral("/cache/art")).isDir());
        qunsetenv("HOPTUNES_DATA_DIR");
    }
};

QTEST_MAIN(TestPaths)
#include "test_paths.moc"
