// SPDX-License-Identifier: GPL-3.0-or-later
// The shared index, scanner and list model behind pictures / audiobooks / ebooks / podcasts.
#include "library/SectionIndex.h"
#include "library/SectionListModel.h"
#include "library/SectionScanner.h"

#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

using namespace hop;

class TestSections : public QObject {
    Q_OBJECT

    static void touch(const QString &path, const QByteArray &data = "x")
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }

private slots:
    void keysRoundTrip()
    {
        for (SectionKind k : kAllSections) {
            SectionKind back;
            QVERIFY(sectionFromKey(sectionKey(k), &back));
            QCOMPARE(back, k);
            QVERIFY(!sectionExtensions(k).isEmpty());
        }
        QVERIFY(!sectionFromKey(QStringLiteral("nope"), nullptr));
    }

    void picturesScanAndIncrementalRescan()
    {
        QTemporaryDir tmp;
        const QString root = QFileInfo(tmp.path()).canonicalFilePath();
        QImage img(64, 32, QImage::Format_RGB32);
        img.fill(Qt::red);
        QVERIFY(QDir().mkpath(root + "/trip"));
        QVERIFY(img.save(root + "/trip/a.png"));
        QVERIFY(img.save(root + "/trip/b.jpg"));
        touch(root + "/trip/notes.txt");   // not a picture

        SectionIndex index;
        QVERIFY(index.open(QStringLiteral(":memory:")));
        auto r = SectionScanner::scanFolders(SectionKind::Pictures, &index, {root});
        QCOMPARE(r.added, 2);
        QCOMPARE(r.total, 2);

        const auto items = index.itemsUnder(SectionKind::Pictures, {root});
        QCOMPARE(items.size(), 2);
        QCOMPARE(items[0].width, 64);
        QCOMPARE(items[0].height, 32);
        QCOMPARE(items[0].group, QStringLiteral("trip"));   // the folder it lives in

        // Nothing changed: nothing re-read.
        r = SectionScanner::scanFolders(SectionKind::Pictures, &index, {root});
        QCOMPARE(r.added + r.updated + r.removed, 0);

        // A removed file leaves the index.
        QVERIFY(QFile::remove(root + "/trip/a.png"));
        r = SectionScanner::scanFolders(SectionKind::Pictures, &index, {root});
        QCOMPARE(r.removed, 1);
        QCOMPARE(index.count(SectionKind::Pictures), 1);
    }

    void kindsDoNotMixAndMissingFolderKeepsRows()
    {
        QTemporaryDir tmp;
        const QString root = QFileInfo(tmp.path()).canonicalFilePath();
        QVERIFY(QDir().mkpath(root + "/lib"));
        touch(root + "/lib/Dune.epub");
        touch(root + "/lib/Manual.pdf");
        QImage img(8, 8, QImage::Format_RGB32);
        QVERIFY(img.save(root + "/lib/cover.png"));

        SectionIndex index;
        QVERIFY(index.open(QStringLiteral(":memory:")));
        SectionScanner::scanFolders(SectionKind::Ebooks, &index, {root});
        SectionScanner::scanFolders(SectionKind::Pictures, &index, {root});
        QCOMPARE(index.count(SectionKind::Ebooks), 2);
        QCOMPARE(index.count(SectionKind::Pictures), 1);

        // Scanning a folder that is not there (unplugged drive) must not purge its rows.
        const auto r = SectionScanner::scanFolders(SectionKind::Ebooks, &index, {root + "/gone"});
        QCOMPARE(r.removed, 0);
        QCOMPARE(index.count(SectionKind::Ebooks), 2);

        // Items are only listed under the folders asked for.
        QCOMPARE(index.itemsUnder(SectionKind::Ebooks, {root + "/elsewhere"}).size(), 0);
    }

    void listModelRolesAndProgress()
    {
        SectionListModel m(SectionKind::Audiobooks);
        SectionItem it;
        it.path = QStringLiteral("/b/Dune.m4b");
        it.title = QStringLiteral("Dune");
        it.group = QStringLiteral("Frank Herbert");
        it.durationMs = 3'725'000;
        it.size = 3 * 1024 * 1024;
        m.setItems({it});
        QCOMPARE(m.count(), 1);
        const QModelIndex i = m.index(0);
        QCOMPARE(m.data(i, SectionListModel::TitleRole).toString(), QStringLiteral("Dune"));
        QCOMPARE(m.data(i, SectionListModel::GroupRole).toString(), QStringLiteral("Frank Herbert"));
        QCOMPARE(m.data(i, SectionListModel::DurationRole).toString(), QStringLiteral("1:02:05"));
        QCOMPARE(m.data(i, SectionListModel::SizeTextRole).toString(), QStringLiteral("3.0 MB"));
        QCOMPARE(m.data(i, SectionListModel::ProgressRole).toDouble(), 0.0);
        QSignalSpy changed(&m, &QAbstractItemModel::dataChanged);
        m.setProgress(it.path, 0.25);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(m.data(i, SectionListModel::ProgressRole).toDouble(), 0.25);
    }
};

QTEST_MAIN(TestSections)
#include "test_sections.moc"
