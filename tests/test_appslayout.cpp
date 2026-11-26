// SPDX-License-Identifier: GPL-3.0-or-later
// Where the apps are on the apps page, and the folders they are filed in (src/core/AppsLayout): no screen needed.
#include "core/AppsLayout.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QtTest>

using namespace hop;

class TestAppsLayout : public QObject {
    Q_OBJECT
    static QStringList apps() { return {"kick", "archive", "deviantart", "nekocafe"}; }
    // every known app is exactly once on the grid or in exactly one folder, every folder once on the grid, in both layouts
    static void checkInvariant(AppsLayout &l, const QStringList &known)
    {
        for (const QString &form : {QStringLiteral("wide"), QStringLiteral("phone")}) {
            l.setForm(form);
            QSet<QString> seen;
            for (int s = 0; s < AppsLayout::kMaxSlots; ++s) {
                const QString it = l.itemAt(s);
                if (it.isEmpty()) continue;
                QVERIFY2(!seen.contains(it), qPrintable(form + " has " + it + " twice"));
                seen.insert(it);
            }
            for (const QString &a : known)
                QVERIFY2(seen.contains("app:" + a) != !l.folderOf(a).isEmpty(), qPrintable(form + ": " + a + " must be on the grid or in a folder, not both or neither"));
            for (const QVariant &f : l.folders()) QVERIFY(seen.contains("folder:" + f.toMap().value("id").toString()));
            int apps = 0;
            for (const QString &i : seen) apps += i.startsWith("app:");
            int inFolders = 0;
            for (const QVariant &f : l.folders()) inFolders += f.toMap().value("members").toStringList().size();
            QCOMPARE(apps + inFolders, known.size());
        }
        l.setForm(QStringLiteral("wide"));
    }

private slots:
    void aFreshLayoutIsTheDefaultOrder()
    {
        AppsLayout l;
        l.setKnownApps(apps());
        for (int i = 0; i < 4; ++i) QCOMPARE(l.itemAt(i), "app:" + apps().at(i));
        QCOMPARE(l.itemAt(4), QString());
        QCOMPARE(l.usedSlots(), 4);
        checkInvariant(l, apps());
    }

    void anAppCanSitAnywhereAndAnOccupiedSpotSwaps()
    {
        AppsLayout l;
        l.setKnownApps(apps());
        QVERIFY(l.moveItem("app:kick", 22));                                    // far away, on another page, leaving a gap
        QCOMPARE(l.itemAt(22), QString("app:kick"));
        QCOMPARE(l.itemAt(0), QString());
        QCOMPARE(l.slotOf("app:kick"), 22);
        QCOMPARE(l.usedSlots(), 23);
        QVERIFY(l.moveItem("app:archive", 22));                                 // an occupied spot swaps
        QCOMPARE(l.itemAt(22), QString("app:archive"));
        QCOMPARE(l.itemAt(1), QString("app:kick"));
        QVERIFY(l.moveItem("app:kick", 1));                                     // the same place: fine, nothing changes
        QVERIFY(!l.moveItem("app:kick", -1) && !l.moveItem("app:kick", AppsLayout::kMaxSlots));
        QVERIFY(!l.moveItem("app:nothere", 3));
        checkInvariant(l, apps());
    }

    void wideAndPhoneLayoutsAreSeparate()
    {
        AppsLayout l;
        l.setKnownApps(apps());
        QVERIFY(l.moveItem("app:kick", 9));
        l.setForm("phone");
        QCOMPARE(l.itemAt(0), QString("app:kick"));                              // the phone layout was not touched
        QVERIFY(l.moveItem("app:kick", 4));
        l.setForm("wide");
        QCOMPARE(l.slotOf("app:kick"), 9);
        l.setForm("phone");
        QCOMPARE(l.slotOf("app:kick"), 4);
        checkInvariant(l, apps());
    }

    void foldersTakeAppsOffTheGridAndGiveThemBack()
    {
        AppsLayout l;
        l.setKnownApps(apps());
        QSignalSpy edited(&l, &AppsLayout::edited);
        const QString f = l.createFolder("  my   games ");
        QVERIFY(!f.isEmpty());
        QCOMPARE(l.folder(f).value("name").toString(), QString("my games"));
        QVERIFY(l.slotOf("folder:" + f) >= 0);
        QVERIFY(l.addToFolder(f, "nekocafe"));
        QCOMPARE(l.folderOf("nekocafe"), f);
        QCOMPARE(l.slotOf("app:nekocafe"), -1);                                   // it left the grid (in both layouts)
        l.setForm("phone");
        QCOMPARE(l.slotOf("app:nekocafe"), -1);
        l.setForm("wide");
        QVERIFY(l.addToFolder(f, "kick") && l.addToFolder(f, "kick"));            // (again: nothing doubles)
        QCOMPARE(l.folder(f).value("members").toStringList(), QStringList({"nekocafe", "kick"}));
        QVERIFY(!l.addToFolder(f, "not an app!") && !l.addToFolder("f999", "archive"));
        QVERIFY(!l.addToFolder(f, "madeup"));                                     // an app that does not exist cannot be filed
        QVERIFY(l.moveInFolder(f, "kick", 0));
        QCOMPARE(l.folder(f).value("members").toStringList(), QStringList({"kick", "nekocafe"}));
        QVERIFY(edited.count() > 0);
        checkInvariant(l, apps());
        // out onto a chosen free slot
        QVERIFY(l.removeFromFolder("kick", 17));
        QCOMPARE(l.itemAt(17), QString("app:kick"));
        QVERIFY(l.folderOf("kick").isEmpty());
        QVERIFY(!l.removeFromFolder("kick"));                                     // (it is not in a folder any more)
        // a drag of an app inside a folder onto the grid takes it out as well
        QVERIFY(l.moveItem("app:nekocafe", 30));
        QCOMPARE(l.itemAt(30), QString("app:nekocafe"));
        QVERIFY(l.folderOf("nekocafe").isEmpty());
        // moving an app from one folder to another
        const QString g = l.createFolder("");
        QCOMPARE(l.folder(g).value("name").toString(), QString("folder"));
        QVERIFY(l.addToFolder(f, "archive"));
        QVERIFY(l.addToFolder(g, "archive"));
        QCOMPARE(l.folderOf("archive"), g);
        QVERIFY(!l.folder(f).value("members").toStringList().contains("archive"));
        checkInvariant(l, apps());
    }

    void deletingAFolderGivesItsAppsBack()
    {
        AppsLayout l;
        l.setKnownApps(apps());
        const QString f = l.createFolder("x");
        QVERIFY(l.setFolderPictureFile(f, "pic-1.jpg"));
        QSignalSpy dropped(&l, &AppsLayout::pictureDropped);
        QVERIFY(l.addToFolder(f, "kick") && l.addToFolder(f, "archive"));
        QVERIFY(l.deleteFolder(f));
        QVERIFY(l.folder(f).isEmpty());
        QVERIFY(l.slotOf("app:kick") >= 0 && l.slotOf("app:archive") >= 0);       // both are back on the grid, nothing is lost
        QCOMPARE(dropped.count(), 1);
        QCOMPARE(dropped.first().first().toString(), QString("pic-1.jpg"));         // its picture file can be removed
        QVERIFY(!l.deleteFolder(f));
        checkInvariant(l, apps());
    }

    void renameColourAndPictureAreChecked()
    {
        AppsLayout l;
        l.setKnownApps(apps());
        const QString f = l.createFolder("a");
        QVERIFY(l.renameFolder(f, "  Fun & games  "));
        QCOMPARE(l.folder(f).value("name").toString(), QString("Fun & games"));
        QVERIFY(!l.renameFolder(f, "   ") && !l.renameFolder("nope", "x"));
        QVERIFY(l.renameFolder(f, QString(100, 'x')));
        QCOMPARE(l.folder(f).value("name").toString().size(), int(AppsLayout::kMaxName));
        QVERIFY(l.setFolderColor(f, "#FF6B9A"));
        QCOMPARE(l.folder(f).value("color").toString(), QString("#ff6b9a"));
        QVERIFY(!l.setFolderColor(f, "red") && !l.setFolderColor(f, "#12345"));
        QCOMPARE(l.folder(f).value("color").toString(), QString("#ff6b9a"));
        QVERIFY(l.setFolderColor(f, ""));                                          // back to the clear glass
        QVERIFY(l.folder(f).value("color").toString().isEmpty());
        QSignalSpy dropped(&l, &AppsLayout::pictureDropped);
        QVERIFY(l.setFolderPictureFile(f, "p1.jpg"));
        QVERIFY(!l.setFolderPictureFile(f, "../../etc/passwd") && !l.setFolderPictureFile(f, "UPPER.jpg"));
        QCOMPARE(l.folder(f).value("picture").toString(), QString("p1.jpg"));
        QVERIFY(l.setFolderPictureFile(f, "p2.jpg"));                              // replacing hands the old file back
        QCOMPARE(dropped.count(), 1);
        QVERIFY(l.setFolderPictureFile(f, ""));
        QCOMPARE(dropped.count(), 2);
        QVERIFY(l.folder(f).value("picture").toString().isEmpty());
    }

    void limitsAreKept()
    {
        AppsLayout l;
        l.setKnownApps(apps());
        for (int i = 0; i < AppsLayout::kMaxFolders; ++i) QVERIFY(!l.createFolder("f").isEmpty());
        QVERIFY(l.createFolder("one too many").isEmpty());
        QCOMPARE(l.folders().size(), int(AppsLayout::kMaxFolders));
        checkInvariant(l, apps());
    }

    void newAndRemovedAppsAreHandled()
    {
        AppsLayout l;
        l.setKnownApps(apps());
        QVERIFY(l.moveItem("app:kick", 7));
        const QString f = l.createFolder("f");
        QVERIFY(l.addToFolder(f, "archive"));
        QStringList more = apps(); more << "odysee2";                              // a new app appears (a new version): it takes the first free spot
        l.setKnownApps(more);
        QCOMPARE(l.slotOf("app:odysee2"), 1);                                        // the first free spot (kick left 0 but the new folder took it; archive left 1)
        QCOMPARE(l.slotOf("app:kick"), 7);                                           // everything else stays where the person put it
        QVERIFY(l.folderOf("archive") == f);
        checkInvariant(l, more);
        QStringList fewer = {"kick", "nekocafe"};                                   // apps that are gone vanish from the grid and from folders
        l.setKnownApps(fewer);
        QVERIFY(l.slotOf("app:deviantart") < 0 && l.slotOf("app:odysee2") < 0);
        QVERIFY(l.folder(f).value("members").toStringList().isEmpty());              // (the folder stays, empty, until it is deleted)
        checkInvariant(l, fewer);
    }

    void saveAndLoadKeepsEverything()
    {
        AppsLayout l;
        l.setKnownApps(apps());
        QVERIFY(l.moveItem("app:kick", 31));
        const QString f = l.createFolder("Pals", 5);
        QVERIFY(l.addToFolder(f, "nekocafe") && l.setFolderColor(f, "#4fd1c5") && l.setFolderPictureFile(f, "pic.jpg"));
        l.setForm("phone");
        QVERIFY(l.moveItem("app:archive", 40));
        const QJsonObject saved = l.toJson();
        const QByteArray bytes = QJsonDocument(saved).toJson();                    // (through the file format, as the profile keeps it)
        AppsLayout again;
        again.fromJson(QJsonDocument::fromJson(bytes).object());
        again.setKnownApps(apps());
        again.setForm("wide");
        QCOMPARE(again.slotOf("app:kick"), 31);
        QCOMPARE(again.folder(f).value("name").toString(), QString("Pals"));
        QCOMPARE(again.folder(f).value("color").toString(), QString("#4fd1c5"));
        QCOMPARE(again.folder(f).value("picture").toString(), QString("pic.jpg"));
        QCOMPARE(again.folderOf("nekocafe"), f);
        again.setForm("phone");
        QCOMPARE(again.slotOf("app:archive"), 40);
        QCOMPARE(again.toJson(), saved);
        // a folder made after a reload gets a fresh id
        const QString g = again.createFolder("new");
        QVERIFY(g != f && !g.isEmpty());
        checkInvariant(again, apps());
    }

    void hostileOrBrokenSettingsGiveASaneLayout()
    {
        AppsLayout l;
        l.fromJson(QJsonObject{});                                                  // nothing saved yet
        l.setKnownApps(apps());
        QCOMPARE(l.itemAt(0), QString("app:kick"));
        QJsonObject bad{{"version", 1}, {"next", -5},
            {"folders", QJsonArray{QJsonObject{{"id", "../x"}, {"name", "bad"}}, QJsonObject{{"id", "f1"}, {"name", "<b>ok</b>"}, {"color", "javascript:1"}, {"picture", "../../x.jpg"}, {"members", QJsonArray{"kick", "kick", 5, "madeup", "kick; rm"}}},
                                   QJsonObject{{"id", "f1"}, {"name", "double"}}}},
            {"slots", QJsonObject{{"wide", QJsonObject{{"0", "app:kick"}, {"1", "folder:f1"}, {"2", "folder:nope"}, {"3", "app:archive"}, {"4", "app:archive"}, {"-1", "app:nekocafe"}, {"9999", "app:nekocafe"}, {"x", "app:nekocafe"}, {"5", 7}, {"6", "evil"}}},
                                  {"phone", "not an object"}}}};
        AppsLayout h;
        h.fromJson(bad);
        h.setKnownApps(apps());
        checkInvariant(h, apps());
        QCOMPARE(h.folders().size(), 1);
        const QVariantMap f = h.folder("f1");
        QCOMPARE(f.value("members").toStringList(), QStringList({"kick"}));          // only a real app, once
        QVERIFY(f.value("color").toString().isEmpty() && f.value("picture").toString().isEmpty());
        QCOMPARE(f.value("name").toString(), QString("<b>ok</b>"));                  // (shown as plain text by the screen)
        QVERIFY(h.slotOf("app:kick") < 0 && h.slotOf("app:archive") == 3);
        // the wrong version number: the default
        AppsLayout v;
        v.fromJson(QJsonObject{{"version", 99}, {"folders", QJsonArray{QJsonObject{{"id", "f1"}, {"name", "x"}}}}});
        v.setKnownApps(apps());
        QVERIFY(v.folders().isEmpty());
        QCOMPARE(v.itemAt(0), QString("app:kick"));
        // a slot map that is far too big is cut down to the slot limit
        QJsonObject huge;
        for (int i = 0; i < 5000; ++i) huge.insert(QString::number(i), "app:kick");
        AppsLayout g;
        g.fromJson(QJsonObject{{"version", 1}, {"slots", QJsonObject{{"wide", huge}}}});
        g.setKnownApps(apps());
        checkInvariant(g, apps());
    }

    void resetGivesTheDefault()
    {
        AppsLayout l;
        l.setKnownApps(apps());
        QVERIFY(l.moveItem("app:kick", 50));
        const QString f = l.createFolder("x");
        QVERIFY(l.addToFolder(f, "archive"));
        l.reset();
        QVERIFY(l.folders().isEmpty());
        for (int i = 0; i < 4; ++i) QCOMPARE(l.itemAt(i), "app:" + apps().at(i));
    }
};

QTEST_GUILESS_MAIN(TestAppsLayout)
#include "test_appslayout.moc"
