// SPDX-License-Identifier: GPL-3.0-or-later
// The game shelf: system detection, Steam manifests, the index, the scanner and the list model.
#include "library/Game.h"
#include "library/GameIndex.h"
#include "library/GameListModel.h"
#include "library/GameScanner.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace hop;

class TestGames : public QObject {
    Q_OBJECT

    static void touch(const QString &path, const QByteArray &data = "x")
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }

    static QByteArray acf(const QString &appId, const QString &name, const QString &dir = QStringLiteral("Dir"))
    {
        return QStringLiteral("\"AppState\"\n{\n\t\"appid\"\t\t\"%1\"\n\t\"Universe\"\t\t\"1\"\n\t\"name\"\t\t\"%2\"\n\t\"installdir\"\t\t\"%3\"\n"
                              "\t\"InstalledDepots\"\n\t{\n\t\t\"1\"\n\t\t{\n\t\t\t\"name\"\t\"not this\"\n\t\t}\n\t}\n}\n").arg(appId, name, dir).toUtf8();
    }

    static QStringList titles(const QList<Game> &gs)
    {
        QStringList out;
        for (const Game &g : gs) out << g.title;
        out.sort();
        return out;
    }

private slots:
    void systemsAreUniqueAndComplete()
    {
        int n = 0;
        const GameSystem *s = gameSystems(&n);
        QVERIFY(n >= 40);
        QSet<QString> keys;
        for (int i = 0; i < n; ++i) {
            QVERIFY2(!keys.contains(QLatin1String(s[i].key)), s[i].key);
            keys.insert(QLatin1String(s[i].key));
            QVERIFY(QColor(QLatin1String(s[i].color)).isValid());
            QVERIFY(!QLatin1String(s[i].name).isEmpty());
        }
        for (const char *k : {"pc", "switch", "wiiu", "wii", "gamecube", "gb", "gbc", "gba", "n64", "psp", "ps3", "ps2", "psx", "nds", "3ds", "genesis", "arcade"})
            QVERIFY2(keys.contains(QLatin1String(k)), k);
        QCOMPARE(gameSystemName(QStringLiteral("pc")), QStringLiteral("PC"));
        QCOMPARE(gameSystemName(QStringLiteral("nosuch")), QStringLiteral("nosuch"));
    }

    void systemsFromFileTypesAndFolders()
    {
        QCOMPARE(gameSystemFor(QStringLiteral("Zelda.z64"), {}), QStringLiteral("n64"));
        QCOMPARE(gameSystemFor(QStringLiteral("Mario.SFC"), {}), QStringLiteral("snes"));      // case does not matter
        QCOMPARE(gameSystemFor(QStringLiteral("a.nsp"), {}), QStringLiteral("switch"));
        QCOMPARE(gameSystemFor(QStringLiteral("a.rvz"), {}), QStringLiteral("gamecube"));
        // the type alone is not enough for a disc image: the nearest folder that names a system decides
        QCOMPARE(gameSystemFor(QStringLiteral("Game.iso"), {}), QString());
        QCOMPARE(gameSystemFor(QStringLiteral("Game.iso"), {QStringLiteral("Disc 1"), QStringLiteral("PS2"), QStringLiteral("Games")}), QStringLiteral("ps2"));
        QCOMPARE(gameSystemFor(QStringLiteral("Game.iso"), {QStringLiteral("Wii U"), QStringLiteral("PS2")}), QStringLiteral("wiiu"));     // nearest first
        QCOMPARE(gameSystemFor(QStringLiteral("Game.iso"), {QStringLiteral("Game-Cube")}), QStringLiteral("gamecube"));
        QCOMPARE(gameSystemFor(QStringLiteral("a.zip"), {QStringLiteral("MAME")}), QStringLiteral("arcade"));
        // types other programs use too are only a game inside a system's folder: a README.md or a Doom .wad is not a console game
        QCOMPARE(gameSystemFor(QStringLiteral("README.md"), {}), QString());
        QCOMPARE(gameSystemFor(QStringLiteral("README.md"), {QStringLiteral("docs")}), QString());
        QCOMPARE(gameSystemFor(QStringLiteral("doom.wad"), {QStringLiteral("doom")}), QString());
        QCOMPARE(gameSystemFor(QStringLiteral("Sonic.md"), {QStringLiteral("Genesis")}), QStringLiteral("genesis"));
        QCOMPARE(gameSystemFor(QStringLiteral("notes.txt"), {QStringLiteral("snes")}), QString());
        QCOMPARE(gameSystemFor(QStringLiteral("noextension"), {QStringLiteral("snes")}), QString());
        QCOMPARE(gameSystemFor(QStringLiteral("photo.jpg"), {QStringLiteral("psp")}), QString());
    }

    void titlesAreCleaned()
    {
        QCOMPARE(cleanGameTitle(QStringLiteral("Super Mario World (USA) [!]")), QStringLiteral("Super Mario World"));
        QCOMPARE(cleanGameTitle(QStringLiteral("Zelda_-_Link's_Awakening (Rev 1) (Europe)")), QStringLiteral("Zelda - Link's Awakening"));
        QCOMPARE(cleanGameTitle(QStringLiteral("(USA)")), QStringLiteral("(USA)"));        // never an empty title
        QCOMPARE(cleanGameTitle(QStringLiteral("Metroid")), QStringLiteral("Metroid"));
    }

    void steamManifests()
    {
        const SteamManifest m = parseSteamManifest(acf(QStringLiteral("620"), QStringLiteral("Portal 2")));
        QVERIFY(m.ok);
        QCOMPARE(m.appId, QStringLiteral("620"));
        QCOMPARE(m.name, QStringLiteral("Portal 2"));          // not the nested "name"
        QCOMPARE(m.installDir, QStringLiteral("Dir"));
        QVERIFY(!isSteamTool(m));
        QVERIFY(parseSteamManifest(acf(QStringLiteral("1"), QStringLiteral("Say \\\"Hi\\\""))).name.contains(QLatin1String("\"Hi\"")));
        QVERIFY(isSteamTool(parseSteamManifest(acf(QStringLiteral("1493710"), QStringLiteral("Proton Experimental")))));
        QVERIFY(isSteamTool(parseSteamManifest(acf(QStringLiteral("1628350"), QStringLiteral("Steam Linux Runtime 3.0 (sniper)")))));
        QVERIFY(isSteamTool(parseSteamManifest(acf(QStringLiteral("228980"), QStringLiteral("Steamworks Common Redistributables")))));
        // garbage never crashes and is never a game
        QVERIFY(!parseSteamManifest({}).ok);
        QVERIFY(!parseSteamManifest("not a manifest at all").ok);
        QVERIFY(!parseSteamManifest("\"AppState\" { \"appid\" \"abc\" \"name\" \"x\" }").ok);          // the id must be a number
        QVERIFY(!parseSteamManifest("\"AppState\" { \"appid\" \"5\" \"name\" ").ok);                    // cut off
        QVERIFY(!parseSteamManifest(QByteArray(300 * 1024, 'a')).ok);                                  // far too large
        QVERIFY(!parseSteamManifest("\"AppState\" { { { { } } \"appid\" \"5\" }").ok);
        QByteArray bin;
        for (int i = 0; i < 4000; ++i) bin.append(char(i * 31));
        parseSteamManifest(bin);                                                                         // just must not crash
    }

    void scannerFindsSteamAndRoms()
    {
        QTemporaryDir d;
        QVERIFY(d.isValid());
        const QString root = d.path();
        // a Steam library: manifests in steamapps, the game's own files (with a ".iso" and a ".wad" in them) must never be read
        touch(root + "/SteamLibrary/steamapps/appmanifest_620.acf", acf(QStringLiteral("620"), QStringLiteral("Portal 2")));
        touch(root + "/SteamLibrary/steamapps/appmanifest_1493710.acf", acf(QStringLiteral("1493710"), QStringLiteral("Proton Experimental")));
        touch(root + "/SteamLibrary/steamapps/appmanifest_bad.acf", "garbage");
        touch(root + "/SteamLibrary/steamapps/common/Portal 2/data.iso");
        touch(root + "/SteamLibrary/steamapps/common/Portal 2/nested/appmanifest_999.acf", acf(QStringLiteral("999"), QStringLiteral("Not a library entry")));
        touch(root + "/SteamLibrary/steamapps/compatdata/620/pfx/game.z64");
        // ROMs
        touch(root + "/roms/snes/Super Mario World (USA).sfc");
        touch(root + "/roms/snes/readme.txt");
        touch(root + "/roms/n64/Zelda (USA) [!].z64");
        touch(root + "/roms/ps2/Gran Turismo 4.iso");
        touch(root + "/roms/psx/Crash/Crash.cue");
        touch(root + "/roms/psx/Crash/Crash (Track 1).bin");
        touch(root + "/roms/psx/Crash/Crash (Track 2).bin");
        touch(root + "/roms/genesis/Sonic.md");
        touch(root + "/roms/Wii U/Mario Kart 8.wux");
        touch(root + "/stuff/random.iso");                      // no system named: not a game
        touch(root + "/stuff/README.md");

        GameIndex idx;
        QVERIFY(idx.open(QStringLiteral(":memory:")));
        const GameScanner::Result r = GameScanner::scanFolders(&idx, {root});
        QCOMPARE(r.removed, 0);
        const QList<Game> all = idx.gamesUnder({QFileInfo(root).canonicalFilePath()});
        QCOMPARE(titles(all), (QStringList{"Crash", "Gran Turismo 4", "Mario Kart 8", "Portal 2", "Sonic", "Super Mario World", "Zelda"}));
        auto sys = [&](const QString &title) { for (const Game &g : all) if (g.title == title) return g.system; return QString(); };
        QCOMPARE(sys(QStringLiteral("Portal 2")), QStringLiteral("pc"));
        QCOMPARE(sys(QStringLiteral("Super Mario World")), QStringLiteral("snes"));
        QCOMPARE(sys(QStringLiteral("Zelda")), QStringLiteral("n64"));
        QCOMPARE(sys(QStringLiteral("Gran Turismo 4")), QStringLiteral("ps2"));
        QCOMPARE(sys(QStringLiteral("Crash")), QStringLiteral("psx"));
        QCOMPARE(sys(QStringLiteral("Sonic")), QStringLiteral("genesis"));
        QCOMPARE(sys(QStringLiteral("Mario Kart 8")), QStringLiteral("wiiu"));
        for (const Game &g : all) if (g.title == QLatin1String("Portal 2")) {
            QCOMPARE(g.source, QStringLiteral("steam"));
            QCOMPARE(g.appId, QStringLiteral("620"));
        }
    }

    void gogInfoFiles()
    {
        const GogInfo g = parseGogInfo(R"({"gameId":"1207658930","name":"The Witcher 3","rootGameId":"1207658930","buildId":"1"})");
        QVERIFY(g.ok);
        QCOMPARE(g.gameId, QStringLiteral("1207658930"));
        QCOMPARE(g.name, QStringLiteral("The Witcher 3"));
        QVERIFY(!g.isAddOn);
        QVERIFY(parseGogInfo(R"({"gameId":"5","name":"Add-on pack","rootGameId":"4"})").isAddOn);     // an add-on is not a game of its own
        QVERIFY(parseGogInfo(R"({"gameId":12345,"name":"Number id"})").ok);                            // some write the id as a number
        QVERIFY(!parseGogInfo({}).ok);
        QVERIFY(!parseGogInfo("not json").ok);
        QVERIFY(!parseGogInfo("[1,2,3]").ok);
        QVERIFY(!parseGogInfo(R"({"gameId":"abc","name":"x"})").ok);
        QVERIFY(!parseGogInfo(R"({"gameId":"5"})").ok);
        QVERIFY(!parseGogInfo(QByteArray(300 * 1024, '{')).ok);
    }

    void pcIsSteamAndGogTogetherAndEpicIsNotScanned()
    {
        QTemporaryDir d;
        const QString root = d.path();
        touch(root + "/Steam/steamapps/appmanifest_620.acf", acf(QStringLiteral("620"), QStringLiteral("Portal 2")));
        touch(root + "/GOG Games/Witcher 3/goggame-1207658930.info", R"({"gameId":"1207658930","name":"The Witcher 3","rootGameId":"1207658930"})");
        touch(root + "/GOG Games/Witcher 3/goggame-111.info", R"({"gameId":"111","name":"Blood and Wine","rootGameId":"1207658930"})");   // add-on
        touch(root + "/GOG Games/Witcher 3/witcher3.exe");
        touch(root + "/GOG Games/Broken/goggame-9.info", "nonsense");
        // Epic Games Store: its manifests and its install folders are never read, whatever is in them
        touch(root + "/Epic/Manifests/ABC.item", R"({"DisplayName":"Fortnite","AppName":"Fortnite","InstallLocation":"x"})");
        touch(root + "/Epic/Fortnite/.egstore/ABC.manifest");
        touch(root + "/Epic/Fortnite/FortniteGame.exe");

        GameIndex idx;
        QVERIFY(idx.open(QStringLiteral(":memory:")));
        GameScanner::scanFolders(&idx, {root});
        const QList<Game> all = idx.gamesUnder({QFileInfo(root).canonicalFilePath()});
        QCOMPARE(titles(all), (QStringList{"Portal 2", "The Witcher 3"}));
        for (const Game &g : all) QCOMPARE(g.system, QStringLiteral("pc"));          // Steam and GOG share the one PC shelf
        QSet<QString> sources;
        for (const Game &g : all) sources.insert(g.source);
        QCOMPARE(sources, (QSet<QString>{"steam", "gog"}));
        // a game from another launcher goes under third-party PC, by hand
        QVERIFY(gameSystemByKey(QString::fromLatin1(kManualSystem)));
        QCOMPARE(gameSystemName(QString::fromLatin1(kManualSystem)), QStringLiteral("Third-party PC"));
        QCOMPARE(gameSystemFor(QStringLiteral("a.iso"), {QStringLiteral("Third-party PC")}), QString::fromLatin1(kManualSystem));
    }

    void steamCoverFromTheLibraryCache()
    {
        QTemporaryDir d;
        const QString root = d.path();
        touch(root + "/Steam/steamapps/appmanifest_620.acf", acf(QStringLiteral("620"), QStringLiteral("Portal 2")));
        touch(root + "/Steam/steamapps/appmanifest_10.acf", acf(QStringLiteral("10"), QStringLiteral("Counter-Strike")));
        touch(root + "/Steam/appcache/librarycache/620_library_600x900.jpg");                       // old layout
        touch(root + "/Steam/appcache/librarycache/10/abcdef/library_600x900.jpg");                // newer layout
        GameIndex idx;
        QVERIFY(idx.open(QStringLiteral(":memory:")));
        GameScanner::scanFolders(&idx, {root + "/Steam/steamapps"});
        for (const Game &g : idx.gamesUnder({QFileInfo(root).canonicalFilePath()})) {
            QVERIFY2(!g.cover.isEmpty(), qPrintable(g.title));
            QVERIFY(QFileInfo::exists(g.cover));
        }
    }

    void incrementalRescanAndGoneGames()
    {
        QTemporaryDir d;
        const QString root = d.path();
        touch(root + "/nes/A.nes");
        touch(root + "/nes/B.nes");
        GameIndex idx;
        QVERIFY(idx.open(QStringLiteral(":memory:")));
        GameScanner::Result r = GameScanner::scanFolders(&idx, {root});
        QCOMPARE(r.added, 2);
        r = GameScanner::scanFolders(&idx, {root});
        QCOMPARE(r.added + r.updated + r.removed, 0);          // nothing changed: nothing rewritten
        QCOMPARE(r.total, 2);
        QVERIFY(QFile::remove(root + "/nes/B.nes"));
        touch(root + "/nes/C.nes");
        r = GameScanner::scanFolders(&idx, {root});
        QCOMPARE(r.added, 1);
        QCOMPARE(r.removed, 1);
        QCOMPARE(titles(idx.gamesUnder({QFileInfo(root).canonicalFilePath()})), (QStringList{"A", "C"}));
    }

    void aMissingFolderKeepsItsGamesAndLosesNothingElse()
    {
        QTemporaryDir d;
        const QString a = d.path() + "/driveA", b = d.path() + "/driveB";
        touch(a + "/gba/One.gba");
        touch(b + "/gba/Two.gba");
        GameIndex idx;
        QVERIFY(idx.open(QStringLiteral(":memory:")));
        GameScanner::scanFolders(&idx, {a, b});
        QCOMPARE(idx.count(), 2);
        QDir(b).removeRecursively();                               // the drive is unplugged
        const GameScanner::Result r = GameScanner::scanFolders(&idx, {a, b});
        QCOMPARE(r.removed, 0);
        QCOMPARE(idx.count(), 2);                                  // its rows wait for the drive to come back
        touch(b + "/gba/Two.gba");
        GameScanner::scanFolders(&idx, {a, b});
        QCOMPARE(idx.count(), 2);
    }

    void aDriveGoneMidScanLosesNothing()
    {
        QTemporaryDir d;
        const QString a = d.path() + "/driveA", b = d.path() + "/driveB";
        touch(a + "/gba/One.gba");
        touch(b + "/gba/Two.gba");
        GameIndex idx;
        QVERIFY(idx.open(QStringLiteral(":memory:")));
        GameScanner::scanFolders(&idx, {a, b});
        QCOMPARE(idx.count(), 2);
        QVERIFY(QFile::remove(a + "/gba/One.gba"));                // something to tidy up in A, so a scan is in progress
        bool pulled = false;
        const GameScanner::Result r = GameScanner::scanFolders(&idx, {a, b}, nullptr, [&](int, int) {
            if (!pulled) { pulled = true; QDir(b).removeRecursively(); }      // B's drive goes away while the scan is running
        });
        QVERIFY(pulled);
        QCOMPARE(r.removed, 1);                                    // only the file that really went
        QCOMPARE(titles(idx.gamesUnder({QFileInfo(d.path()).canonicalFilePath()})), (QStringList{"Two"}));
    }

    void anUnreadableIndexIsRefused()
    {
        GameIndex idx;
        QVERIFY(!idx.open(QStringLiteral("/nonexistent-dir/x/games.sqlite")));
        QVERIFY(!idx.isOpen());
        QCOMPARE(idx.count(), 0);
        QVERIFY(idx.gamesUnder({QStringLiteral("/")}).isEmpty());
    }

    void listModelFiltersBySystemAndWord()
    {
        auto entry = [](const QString &path, const QString &title, const QString &system) {
            GameListModel::Entry e;
            e.game.path = path; e.game.title = title; e.game.system = system; e.game.source = system == QLatin1String("pc") ? "steam" : "rom";
            e.title = title;
            return e;
        };
        GameListModel m;
        m.setEntries({entry("/a", "Portal 2", "pc"), entry("/b", "Zelda", "n64"), entry("/c", "Mario 64", "n64"), entry("/d", "Mario World", "snes")});
        QCOMPARE(m.total(), 4);
        QCOMPARE(m.count(), 4);
        QCOMPARE(m.titleAt(0), QStringLiteral("Mario 64"));          // by title
        const QVariantList sy = m.systems();
        QCOMPARE(sy.size(), 3);
        QCOMPARE(sy.at(0).toMap().value("key").toString(), QStringLiteral("pc"));      // PC first, the others by name
        QCOMPARE(sy.at(1).toMap().value("key").toString(), QStringLiteral("n64"));
        QCOMPARE(sy.at(1).toMap().value("count").toInt(), 2);
        m.setSystemFilter(QStringLiteral("n64"));
        QCOMPARE(m.count(), 2);
        QCOMPARE(m.total(), 4);
        m.setTextFilter(QStringLiteral("zel"));
        QCOMPARE(m.count(), 1);
        QCOMPARE(m.pathAt(0), QStringLiteral("/b"));
        m.setSystemFilter(QString());
        m.setTextFilter(QStringLiteral("mario"));
        QCOMPARE(m.count(), 2);
        // the filtered-out system disappearing from the shelf resets the filter
        m.setTextFilter(QString());
        m.setSystemFilter(QStringLiteral("snes"));
        m.setEntries({entry("/a", "Portal 2", "pc")});
        QCOMPARE(m.systemFilter(), QString());
        QCOMPARE(m.count(), 1);
        QCOMPARE(m.pathAt(5), QString());
        // the bookshelf display reads the shown games in one go
        const QVariantList shelf = m.shelfItems();
        QCOMPARE(shelf.size(), 1);
        QCOMPARE(shelf.first().toMap().value("path").toString(), QStringLiteral("/a"));
        QCOMPARE(shelf.first().toMap().value("title").toString(), QStringLiteral("Portal 2"));
        QCOMPARE(shelf.first().toMap().value("systemName").toString(), QStringLiteral("PC"));
        QVERIFY(QColor(shelf.first().toMap().value("systemColor").toString()).isValid());
        QVERIFY(m.entryForPath(QStringLiteral("/a")));
        QVERIFY(!m.entryForPath(QStringLiteral("/zzz")));
    }
};

QTEST_MAIN(TestGames)
#include "test_games.moc"
