// SPDX-License-Identifier: GPL-3.0-or-later
// M4: per-profile database, smart playlists, search, smart DJ, playlist import/export.
#include "library/ProfileDb.h"
#include "library/Smart.h"

#include <QDir>
#include <QFile>
#include <QSet>
#include <QTemporaryDir>
#include <QtTest>

using namespace hop;

namespace {

Track mk(const QString &path, const QString &title, const QString &artist, const QString &album,
         const QString &genre = {}, int year = 0, qint64 added = 0)
{
    Track t;
    t.path = path; t.title = title; t.artist = artist; t.album = album; t.genre = genre;
    t.year = year; t.added = added; t.durationMs = 180000;
    return t;
}

void write(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

} // namespace

class TestM4 : public QObject {
    Q_OBJECT
private slots:
    // ---- ProfileDb ----
    void statsAndHistory()
    {
        QTemporaryDir tmp;
        const QString file = tmp.filePath(QStringLiteral("profile.sqlite"));
        {
            ProfileDb db;
            QVERIFY(db.open(file));
#ifndef Q_OS_WIN   // POSIX group/other bits do not exist on Windows
            QCOMPARE(QFile::permissions(file) & 0x0077, QFileDevice::Permissions{});
#endif
            QVERIFY(db.recordPlay(QStringLiteral("/m/a.flac"), 100));
            QVERIFY(db.recordPlay(QStringLiteral("/m/a.flac"), 200));
            QVERIFY(db.recordPlay(QStringLiteral("/m/b.flac"), 150));
            QVERIFY(db.recordSkip(QStringLiteral("/m/c.flac")));
            QVERIFY(db.setHearted(QStringLiteral("/m/b.flac"), true));
        }
        ProfileDb db;
        QVERIFY(db.open(file));   // reopen: data persisted
        const auto st = db.allStats();
        QCOMPARE(st.value(QStringLiteral("/m/a.flac")).playCount, 2);
        QCOMPARE(st.value(QStringLiteral("/m/a.flac")).lastPlayed, qint64(200));
        QVERIFY(st.value(QStringLiteral("/m/b.flac")).hearted);
        QCOMPARE(st.value(QStringLiteral("/m/c.flac")).skipCount, 1);
        QCOMPARE(st.value(QStringLiteral("/m/c.flac")).playCount, 0);
        const auto h = db.history(10);
        QCOMPARE(h.size(), 3);
        QCOMPARE(h.first().path, QStringLiteral("/m/a.flac"));
        QCOMPARE(h.first().playedAt, qint64(200));
        QVERIFY(db.setHearted(QStringLiteral("/m/b.flac"), false));
        QVERIFY(!db.allStats().value(QStringLiteral("/m/b.flac")).hearted);
    }

    void albumOverridesAndTrackOrder()
    {
        QTemporaryDir tmp;
        const QString file = tmp.filePath(QStringLiteral("profile.sqlite"));
        const QString ref = QStringLiteral("Artist\x1fAlbum");
        {
            ProfileDb db;
            QVERIFY(db.open(file));
            QVERIFY(db.albumOverrides().isEmpty());
            QVERIFY(db.setAlbumOverrideTitle(ref, QStringLiteral("My Title")));
            QVERIFY(db.setAlbumOverrideArt(ref, QStringLiteral("/profiles/x/album-art/abc.jpg")));
            QVERIFY(db.setAlbumTrackOrder(ref, {QStringLiteral("/m/b.flac"), QStringLiteral("/m/a.flac")}));
        }
        ProfileDb db;
        QVERIFY(db.open(file));   // reopen: data persisted
        const auto overrides = db.albumOverrides();
        QCOMPARE(overrides.size(), 1);
        QCOMPARE(overrides.value(ref).title, QStringLiteral("My Title"));
        QCOMPARE(overrides.value(ref).art, QStringLiteral("/profiles/x/album-art/abc.jpg"));
        QCOMPARE(db.albumTrackOrder(ref), (QStringList{"/m/b.flac", "/m/a.flac"}));

        // Setting one field leaves the other untouched (partial upsert).
        QVERIFY(db.setAlbumOverrideTitle(ref, QStringLiteral("Renamed Again")));
        QCOMPARE(db.albumOverrides().value(ref).art, QStringLiteral("/profiles/x/album-art/abc.jpg"));

        // Clearing (empty string) removes just that field's override.
        QVERIFY(db.setAlbumOverrideArt(ref, QString()));
        QVERIFY(db.albumOverrides().value(ref).art.isEmpty());
        QCOMPARE(db.albumOverrides().value(ref).title, QStringLiteral("Renamed Again"));

        // A different album never sees this one's order or overrides.
        QVERIFY(db.albumTrackOrder(QStringLiteral("Other\x1fAlbum")).isEmpty());
        QVERIFY(!db.albumOverrides().contains(QStringLiteral("Other\x1fAlbum")));
    }

    void videoMetaRoundTrips()
    {
        QTemporaryDir tmp;
        const QString file = tmp.filePath(QStringLiteral("profile.sqlite"));
        const QString v = QStringLiteral("/m/movie.mkv");
        {
            ProfileDb db;
            QVERIFY(db.open(file));
            QVERIFY(db.videoMetas().isEmpty());
            QVERIFY(db.setVideoMeta(v, QStringLiteral("{\"year\":\"1998\"}")));
        }
        ProfileDb db;
        QVERIFY(db.open(file));   // persisted
        QCOMPARE(db.videoMetas().value(v), QStringLiteral("{\"year\":\"1998\"}"));
        QVERIFY(db.setVideoMeta(v, QStringLiteral("{\"year\":\"1999\"}")));   // replace
        QCOMPARE(db.videoMetas().size(), 1);
        QVERIFY(db.setVideoMeta(v, QString()));   // clear
        QVERIFY(db.videoMetas().isEmpty());
    }

    void videoComments()
    {
        QTemporaryDir tmp;
        const QString file = tmp.filePath(QStringLiteral("profile.sqlite"));
        const QString v = QStringLiteral("/m/movie.mkv");
        qint64 a = 0, b = 0;
        {
            ProfileDb db;
            QVERIFY(db.open(file));
            QVERIFY(db.videoComments(v).isEmpty());
            a = db.addVideoComment(v, QStringLiteral("first"), 1000);
            b = db.addVideoComment(v, QStringLiteral("second"), 2000);
            QVERIFY(a > 0 && b > a);
            QCOMPARE(db.addVideoComment(v, QString(), 3000), qint64(0));   // nothing to say, nothing saved
            QVERIFY(db.addVideoComment(QStringLiteral("/m/other.mkv"), QStringLiteral("elsewhere"), 1500) > 0);
        }
        ProfileDb db;
        QVERIFY(db.open(file));   // persisted
        const QList<VideoComment> list = db.videoComments(v);
        QCOMPARE(list.size(), 2);
        QCOMPARE(list.first().text, QStringLiteral("second"));   // newest first
        QVERIFY(db.removeVideoComment(a));
        QCOMPARE(db.videoComments(v).size(), 1);
        QCOMPARE(db.videoComments(QStringLiteral("/m/other.mkv")).size(), 1);
    }

    void playedItems()
    {
        QTemporaryDir tmp;
        const QString file = tmp.filePath(QStringLiteral("profile.sqlite"));
        {
            ProfileDb db;
            QVERIFY(db.open(file));
            QVERIFY(db.playedItems().isEmpty());
            QVERIFY(db.setItemPlayed(QStringLiteral("/p/ep1.mp3"), true));
            QVERIFY(db.setItemPlayed(QStringLiteral("/p/ep1.mp3"), true));   // twice is fine
            QVERIFY(db.setItemPlayed(QStringLiteral("/p/ep2.mp3"), true));
        }
        ProfileDb db;
        QVERIFY(db.open(file));   // persisted
        QCOMPARE(db.playedItems().size(), 2);
        QVERIFY(db.setItemPlayed(QStringLiteral("/p/ep1.mp3"), false));
        QCOMPARE(db.playedItems(), QSet<QString>{QStringLiteral("/p/ep2.mp3")});
    }

    void videoThumbOverrides()
    {
        QTemporaryDir tmp;
        const QString file = tmp.filePath(QStringLiteral("profile.sqlite"));
        const QString v = QStringLiteral("/m/movie.mp4");
        {
            ProfileDb db;
            QVERIFY(db.open(file));
            QVERIFY(db.videoThumbOverrides().isEmpty());
            QVERIFY(db.setVideoThumbOverride(v, QStringLiteral("/profiles/x/video-thumbs/a.jpg")));
        }
        ProfileDb db;
        QVERIFY(db.open(file));   // reopen: persisted
        QCOMPARE(db.videoThumbOverrides().value(v), QStringLiteral("/profiles/x/video-thumbs/a.jpg"));
        QVERIFY(db.setVideoThumbOverride(v, QStringLiteral("/profiles/x/video-thumbs/b.jpg")));   // replace
        QCOMPARE(db.videoThumbOverrides().size(), 1);
        QVERIFY(db.setVideoThumbOverride(v, QString()));   // clear
        QVERIFY(db.videoThumbOverrides().isEmpty());
    }

    void songOrder()
    {
        QTemporaryDir tmp;
        const QString file = tmp.filePath(QStringLiteral("profile.sqlite"));
        {
            ProfileDb db;
            QVERIFY(db.open(file));
            QVERIFY(db.songOrder().isEmpty());
            QVERIFY(db.setSongOrder({QStringLiteral("/m/c.flac"), QStringLiteral("/m/a.flac"), QStringLiteral("/m/b.flac")}));
        }
        ProfileDb db;
        QVERIFY(db.open(file));   // reopen: order persisted
        QCOMPARE(db.songOrder(), (QStringList{"/m/c.flac", "/m/a.flac", "/m/b.flac"}));

        // Saving again fully replaces the previous order, not merges with it.
        QVERIFY(db.setSongOrder({QStringLiteral("/m/b.flac"), QStringLiteral("/m/a.flac")}));
        QCOMPARE(db.songOrder(), (QStringList{"/m/b.flac", "/m/a.flac"}));
    }

    void playlistsAndPins()
    {
        ProfileDb db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        QCOMPARE(db.createPlaylist(QStringLiteral("  ")), qint64(0));
        const qint64 a = db.createPlaylist(QStringLiteral("road trip"));
        const qint64 s = db.createPlaylist(QStringLiteral("new jazz"), true, QStringLiteral("{}"));
        QVERIFY(a && s);
        QVERIFY(db.addToPlaylist(a, {QStringLiteral("/1"), QStringLiteral("/2"), QStringLiteral("/3")}));
        QVERIFY(db.addToPlaylist(a, {QStringLiteral("/2")}));          // duplicates are allowed
        QCOMPARE(db.playlistItems(a), (QStringList{"/1", "/2", "/3", "/2"}));
        QVERIFY(db.movePlaylistItem(a, 0, 2));
        QCOMPARE(db.playlistItems(a), (QStringList{"/2", "/3", "/1", "/2"}));
        QVERIFY(!db.movePlaylistItem(a, 0, 9));
        QVERIFY(db.removeFromPlaylist(a, 3));
        QCOMPARE(db.playlistItems(a), (QStringList{"/2", "/3", "/1"}));
        QVERIFY(db.renamePlaylist(a, QStringLiteral("Road Trip")));
        QVERIFY(!db.setPlaylistRules(a, QStringLiteral("{}")));        // not a smart playlist
        QVERIFY(db.setPlaylistRules(s, QStringLiteral("{\"match\":\"any\"}")));
        QCOMPARE(db.playlists().size(), 2);
        QCOMPARE(db.playlists().first().name, QStringLiteral("new jazz"));   // sorted by name
        QVERIFY(db.playlist(s).smart);

        QVERIFY(db.pin({QStringLiteral("playlist"), QString::number(a), QStringLiteral("Road Trip")}));
        QVERIFY(db.pin({QStringLiteral("album"), QStringLiteral("X\x1fY"), QStringLiteral("Y")}));
        QVERIFY(db.pin({QStringLiteral("album"), QStringLiteral("X\x1fY"), QStringLiteral("Y2")}));   // no duplicate
        QCOMPARE(db.pins().size(), 2);
        QVERIFY(db.isPinned(QStringLiteral("album"), QStringLiteral("X\x1fY")));

        // Deleting a playlist removes its items and its pin.
        QVERIFY(db.deletePlaylist(a));
        QVERIFY(db.playlistItems(a).isEmpty());
        QVERIFY(!db.isPinned(QStringLiteral("playlist"), QString::number(a)));
        QCOMPARE(db.pins().size(), 1);
        QVERIFY(db.unpin(QStringLiteral("album"), QStringLiteral("X\x1fY")));
        QVERIFY(db.pins().isEmpty());
    }

    // ---- search ----
    void search()
    {
        const Track t = mk(QStringLiteral("/m/x.flac"), QStringLiteral("Formation"), QStringLiteral("Beyoncé"),
                           QStringLiteral("Lemonade"), QStringLiteral("R&B"));
        QVERIFY(matchesSearch(t, searchTokens(QStringLiteral("beyonce"))));
        QVERIFY(matchesSearch(t, searchTokens(QStringLiteral("  BEYONCÉ   lemon "))));
        QVERIFY(matchesSearch(t, searchTokens(QStringLiteral("r&b form"))));
        QVERIFY(!matchesSearch(t, searchTokens(QStringLiteral("beyonce folklore"))));
        QVERIFY(matchesSearch(t, searchTokens(QString())));
        QCOMPARE(foldForSearch(QStringLiteral("Sigur Rós")), QStringLiteral("sigur ros"));
        // Untitled files are found by file name.
        QVERIFY(matchesSearch(mk(QStringLiteral("/m/Track 07.mp3"), {}, {}, {}), searchTokens(QStringLiteral("track 07"))));
    }

    // ---- smart rules ----
    void smartRules()
    {
        const qint64 now = 1'000'000'000;
        QList<Track> lib{
            mk("/a", "Song A", "Miles", "Kind of Blue", "Jazz", 1959, now - 2 * 86400),
            mk("/b", "Song B", "Coltrane", "Blue Train", "Jazz", 1957, now - 90 * 86400),
            mk("/c", "Song C", "Daft Punk", "Discovery", "Electronic", 2001, now - 5 * 86400),
            mk("/d", "Song D", "Miles", "Bitches Brew", "Jazz Fusion", 1970, now - 400 * 86400),
        };
        QHash<QString, TrackStats> st;
        st["/a"] = {5, 0, now - 3600, true};
        st["/c"] = {12, 1, now - 86400 * 40, false};

        SmartRules r;
        r.rules = {{"genre", "contains", "jazz"}};
        QCOMPARE(r.apply(lib, st, now).size(), 3);
        r.rules = {{"genre", "is", "JAZZ"}};
        QCOMPARE(r.apply(lib, st, now).size(), 2);
        r.rules = {{"genre", "is", "jazz"}, {"year", "lt", "1958"}};
        QCOMPARE(r.apply(lib, st, now).size(), 1);
        r.matchAll = false;   // any
        QCOMPARE(r.apply(lib, st, now).size(), 2);
        r = {};
        r.rules = {{"added_days", "within", "30"}};
        QCOMPARE(r.apply(lib, st, now).size(), 2);
        r.rules = {{"played_days", "not_within", "7"}};   // never played counts as "not in the last 7 days"
        QCOMPARE(r.apply(lib, st, now).size(), 3);
        r.rules = {{"plays", "gt", "4"}};
        r.sort = "plays";
        auto out = r.apply(lib, st, now);
        QCOMPARE(out.size(), 2);
        QCOMPARE(out.first().path, QStringLiteral("/c"));
        r.rules = {{"hearted", "is", "true"}};
        QCOMPARE(r.apply(lib, st, now).size(), 1);
        r.rules = {{"artist", "is_not", "miles"}};
        r.sort = "title";
        r.limit = 1;
        out = r.apply(lib, st, now);
        QCOMPARE(out.size(), 1);
        QCOMPARE(out.first().path, QStringLiteral("/b"));

        // JSON round trip.
        SmartRules j;
        j.matchAll = false; j.sort = "random"; j.limit = 25;
        j.rules = {{"genre", "contains", "jazz"}, {"plays", "eq", "0"}};
        const SmartRules back = SmartRules::fromJson(j.toJson());
        QCOMPARE(back.matchAll, false);
        QCOMPARE(back.sort, QStringLiteral("random"));
        QCOMPARE(back.limit, 25);
        QCOMPARE(back.rules.size(), 2);
        QCOMPARE(back.rules[1].op, QStringLiteral("eq"));
        QVERIFY(SmartRules::fromJson(QStringLiteral("not json")).rules.isEmpty());

        // Random order is stable for a given seed (a smart playlist does not reshuffle on every look).
        SmartRules rnd; rnd.sort = "random";
        QCOMPARE(rnd.apply(lib, st, now, 7).first().path, rnd.apply(lib, st, now, 7).first().path);

        // Auto lists.
        const auto autos = autoLists();
        QCOMPARE(autos.size(), 4);
        QCOMPARE(autos[0].rules.apply(lib, st, now).size(), 1);   // hearted
        QCOMPARE(autos[1].rules.apply(lib, st, now).first().path, QStringLiteral("/c"));   // most played
        QCOMPARE(autos[3].rules.apply(lib, st, now).size(), 2);   // never played
    }

    // ---- smart DJ ----
    void smartDj()
    {
        QList<Track> lib;
        for (int i = 0; i < 20; ++i) lib << mk(QString("/miles/%1").arg(i), QString("m%1").arg(i), "Miles", "A", "Jazz", 1959);
        for (int i = 0; i < 20; ++i) lib << mk(QString("/trane/%1").arg(i), QString("t%1").arg(i), "Coltrane", "B", "Jazz", 1961);
        for (int i = 0; i < 40; ++i) lib << mk(QString("/edm/%1").arg(i), QString("e%1").arg(i), "Daft Punk", "C", "Electronic", 2001);
        const QHash<QString, TrackStats> st;
        const auto mix = smartMix("miles", lib, st, 25, 42);
        QCOMPARE(mix.size(), 25);
        QSet<QString> paths;
        int jazz = 0;
        for (const Track &t : mix) { paths.insert(t.path); if (t.genre == "Jazz") ++jazz; }
        QCOMPARE(paths.size(), 25);                 // no repeats
        QCOMPARE(jazz, 25);                         // nothing unrelated while related music exists
        int sameInARow = 0;
        for (int i = 1; i < mix.size(); ++i) if (mix[i].artist == mix[i - 1].artist) ++sameInARow;
        QVERIFY2(sameInARow < 12, "artists should be spread out");
        QCOMPARE(smartMix("miles", lib, st, 25, 42).first().path, mix.first().path);   // seeded
        // Unknown artist / tiny overlap still yields music.
        QCOMPARE(smartMix("nobody", lib, st, 10, 1).size(), 10);
        QVERIFY(smartMix("miles", {}, st, 10, 1).isEmpty());
    }

    // ---- playlist files ----
    void importFormats()
    {
        QTemporaryDir tmp;
        const QString lib = tmp.filePath("Music");
        const QStringList songs{lib + "/Miles Davis/Kind of Blue/01 So What.flac",
                                lib + "/Miles Davis/Kind of Blue/02 Freddie Freeloader.flac",
                                lib + "/Björk/Homogenic/01 Hunter.mp3"};
        for (const QString &s : songs) write(s, "x");
        const QString pl = tmp.filePath("Music/lists");

        // m3u8 with comments, relative path, file:// URL, BOM, a stream URL to ignore, a missing file
        write(pl + "/mix.m3u8", "\xEF\xBB\xBF#EXTM3U\n#PLAYLIST:Late Night\n#EXTINF:545,Miles Davis - So What\n"
                                "../Miles Davis/Kind of Blue/01 So What.flac\n"
                                "file://" + QUrl::toPercentEncoding(lib + "/Björk/Homogenic/01 Hunter.mp3", "/") + "\n"
                                "http://radio.example/stream\n../gone.mp3\n");
        auto p = PlaylistIO::read(pl + "/mix.m3u8");
        QCOMPARE(p.name, QStringLiteral("Late Night"));
        QCOMPARE(p.entries.size(), 3);
        int missing = -1;
        auto r = PlaylistIO::resolve(p.entries, pl, songs, &missing);
        QCOMPARE(r, (QStringList{songs[0], songs[2]}));
        QCOMPARE(missing, 1);

        // Latin-1 .m3u
        write(pl + "/old.m3u", QByteArray("../Bj\xF6rk/Homogenic/01 Hunter.mp3\r\n"));
        r = PlaylistIO::resolve(PlaylistIO::read(pl + "/old.m3u").entries, pl, songs);
        QCOMPARE(r, QStringList{songs[2]});

        // pls
        write(pl + "/a.pls", "[playlist]\nFile1=" + songs[1].toUtf8() + "\nTitle1=x\nNumberOfEntries=1\n");
        QCOMPARE(PlaylistIO::resolve(PlaylistIO::read(pl + "/a.pls").entries, pl, songs), QStringList{songs[1]});

        // xspf
        write(pl + "/a.xspf", "<?xml version=\"1.0\"?><playlist version=\"1\" xmlns=\"http://xspf.org/ns/0/\"><title>Blue</title>"
                              "<trackList><track><location>file://" + QUrl::toPercentEncoding(songs[0], "/") + "</location></track></trackList></playlist>");
        p = PlaylistIO::read(pl + "/a.xspf");
        QCOMPARE(p.name, QStringLiteral("Blue"));
        QCOMPARE(PlaylistIO::resolve(p.entries, pl, songs), QStringList{songs[0]});

        // Zune .zpl made on Windows: absolute Windows paths. Matched by artist/album/file.
        write(pl + "/zune.zpl", "<?zpl version=\"2.0\"?><smil><head><title>Zune Favorites</title></head><body><seq>"
                                "<media src=\"C:\\Users\\me\\Music\\Zune\\Miles Davis\\Kind of Blue\\02 Freddie Freeloader.flac\" />"
                                "<media src=\"..\\Bj&#246;rk\\Homogenic\\01 Hunter.mp3\" />"
                                "<media src=\"C:\\Users\\me\\Music\\Nope\\nothing.wma\" />"
                                "</seq></body></smil>");
        p = PlaylistIO::read(pl + "/zune.zpl");
        QCOMPARE(p.name, QStringLiteral("Zune Favorites"));
        QCOMPARE(p.entries.size(), 3);
        r = PlaylistIO::resolve(p.entries, pl, songs, &missing);
        QCOMPARE(r, (QStringList{songs[1], songs[2]}));
        QCOMPARE(missing, 1);

        QVERIFY(!PlaylistIO::read(pl + "/does-not-exist.m3u").error.isEmpty());
    }

    void exportRoundTrip()
    {
        QTemporaryDir tmp;
        QList<Track> tracks{mk(tmp.filePath("Music/A/1 One.flac"), "One", "A", "X"), mk(tmp.filePath("Music/B/2 Two.flac"), "Two", "B", "Y")};
        for (const Track &t : tracks) write(t.path, "x");
        const QString out = tmp.filePath("Music/lists/export.m3u8");
        QDir().mkpath(QFileInfo(out).absolutePath());
        QVERIFY(PlaylistIO::writeM3u8(out, "Two songs", tracks));
        QFile f(out);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(f.readAll());
        QVERIFY(text.startsWith("#EXTM3U"));
        QVERIFY(text.contains("../A/1 One.flac"));          // relative, so the folder can move
        QVERIFY(!text.contains(tmp.path()));
        const auto p = PlaylistIO::read(out);
        QCOMPARE(p.name, QStringLiteral("Two songs"));
        QCOMPARE(PlaylistIO::resolve(p.entries, QFileInfo(out).absolutePath(), {tracks[0].path, tracks[1].path}),
                 (QStringList{tracks[0].path, tracks[1].path}));
    }
};

QTEST_GUILESS_MAIN(TestM4)
#include "test_m4.moc"
