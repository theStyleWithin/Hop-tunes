// SPDX-License-Identifier: GPL-3.0-or-later
// Paths: HOPTUNES_DATA_DIR moves data and cache under one folder (the portable build).
#include "core/Paths.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace hop;

class TestPaths : public QObject {
    Q_OBJECT
private slots:
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
