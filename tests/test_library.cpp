// SPDX-License-Identifier: GPL-3.0-or-later
// Unit tests for the library layer: track index + incremental scanner.
#include "library/CollectionModels.h"
#include "library/CoverArt.h"
#include "library/Scanner.h"
#include "library/TagReader.h"
#include "library/TrackIndex.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

#include <sqlite3.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/id3v2tag.h>
#include <taglib/wavfile.h>

using namespace hop;

namespace {

// Writes a valid PCM WAV file: mono, 16-bit, 8 kHz, `ms` long, silent.
bool writeWav(const QString &path, int ms)
{
    const quint32 rate = 8000, channels = 1, bits = 16;
    const quint32 dataBytes = rate * channels * (bits / 8) * ms / 1000;
    QByteArray h;
    auto u32 = [&h](quint32 v) { char b[4]; qToLittleEndian(v, b); h.append(b, 4); };
    auto u16 = [&h](quint16 v) { char b[2]; qToLittleEndian(v, b); h.append(b, 2); };
    h.append("RIFF"); u32(36 + dataBytes); h.append("WAVE");
    h.append("fmt "); u32(16); u16(1); u16(channels); u32(rate);
    u32(rate * channels * bits / 8); u16(channels * bits / 8); u16(bits);
    h.append("data"); u32(dataBytes);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(h);
    f.write(QByteArray(int(dataBytes), '\0'));
    return true;
}

// A tiny valid PNG (1x1) so the art bytes are realistic.
QByteArray tinyPng()
{
    return QByteArray::fromHex("89504e470d0a1a0a0000000d4948445200000001000000010806000000"
                               "1f15c4890000000d4944415478da63f8cfc0f01f0005000201a4c0c6a00000000049454e44ae426082");
}

bool embedPng(const QString &wavPath, const QByteArray &png)
{
    TagLib::RIFF::WAV::File f(QFile::encodeName(wavPath).constData());
    if (!f.isValid()) return false;
    auto *frame = new TagLib::ID3v2::AttachedPictureFrame;
    frame->setMimeType("image/png");
    frame->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
    frame->setPicture(TagLib::ByteVector(png.constData(), uint(png.size())));
    f.ID3v2Tag()->addFrame(frame);
    f.ID3v2Tag()->setAlbum("Art Album");
    return f.save();
}

Track makeTrack(const QString &path, const QString &albumArtist, const QString &album, int disc, int no)
{
    Track t;
    t.path = path; t.albumArtist = albumArtist; t.album = album; t.disc = disc; t.trackNo = no;
    t.title = QStringLiteral("t%1").arg(no); t.mtime = 1; t.size = 1;
    return t;
}

} // namespace

class TestLibrary : public QObject {
    Q_OBJECT
private slots:
    void indexOrdersByAlbum()
    {
        TrackIndex idx;
        QVERIFY2(idx.open(QStringLiteral(":memory:")), qPrintable(idx.lastError()));
        QVERIFY(idx.upsert({
            makeTrack("/m/b/2.flac", "Beta", "Second", 1, 2),
            makeTrack("/m/a/1.flac", "alpha", "First", 1, 1),
            makeTrack("/m/b/1.flac", "Beta", "Second", 1, 1),
            makeTrack("/m/b/d2.flac", "Beta", "Second", 2, 1),
        }));
        const QList<Track> all = idx.tracksUnder({QStringLiteral("/m")});
        QCOMPARE(all.size(), 4);
        QCOMPARE(all[0].path, QStringLiteral("/m/a/1.flac"));   // case-insensitive "alpha" < "Beta"
        QCOMPARE(all[1].path, QStringLiteral("/m/b/1.flac"));
        QCOMPARE(all[2].path, QStringLiteral("/m/b/2.flac"));
        QCOMPARE(all[3].path, QStringLiteral("/m/b/d2.flac"));  // disc 2 last
        QCOMPARE(idx.trackById(all[0].id).album, QStringLiteral("First"));
    }

    void folderPrefixIsExact()
    {
        TrackIndex idx;
        QVERIFY(idx.open(QStringLiteral(":memory:")));
        QVERIFY(idx.upsert({makeTrack("/music/a.mp3", "", "", 0, 1),
                            makeTrack("/music2/b.mp3", "", "", 0, 1)}));
        QCOMPARE(idx.tracksUnder({QStringLiteral("/music")}).size(), 1);
        QCOMPARE(idx.tracksUnder({QStringLiteral("/music/")}).size(), 1);   // trailing slash ok
        QCOMPARE(idx.stampsUnder(QStringLiteral("/music")).size(), 1);
        QCOMPARE(idx.tracksUnder({QStringLiteral("/music"), QStringLiteral("/music2")}).size(), 2);
    }

    void upsertUpdatesInPlace()
    {
        TrackIndex idx;
        QVERIFY(idx.open(QStringLiteral(":memory:")));
        Track t = makeTrack("/x/song.ogg", "A", "Old", 0, 1);
        QVERIFY(idx.upsert({t}));
        t.album = QStringLiteral("New");
        QVERIFY(idx.upsert({t}));
        QCOMPARE(idx.count(), 1);
        QCOMPARE(idx.tracksUnder({QStringLiteral("/x")}).first().album, QStringLiteral("New"));
        QVERIFY(idx.removePaths({QStringLiteral("/x/song.ogg")}));
        QCOMPARE(idx.count(), 0);
    }

    void displayFallbacks()
    {
        Track t;
        t.path = QStringLiteral("/a/b/My Song.mp3");
        QCOMPARE(t.displayTitle(), QStringLiteral("My Song"));
        QCOMPARE(t.displayArtist(), QStringLiteral("unknown artist"));
        QCOMPARE(t.displayAlbum(), QStringLiteral("unknown album"));
    }

    void tagReaderReadsWavProperties()
    {
        QTemporaryDir dir;
        const QString p = dir.filePath(QStringLiteral("tone.wav"));
        QVERIFY(writeWav(p, 1500));
        const Track t = TagReader::read(QFileInfo(p));
        QCOMPARE(t.codec, QStringLiteral("wav"));
        QVERIFY2(qAbs(t.durationMs - 1500) <= 5, qPrintable(QString::number(t.durationMs)));
        QCOMPARE(t.sampleRate, 8000);
        QCOMPARE(t.displayTitle(), QStringLiteral("tone"));
        QVERIFY(!TagReader::isAudioFile(QFileInfo(dir.filePath(QStringLiteral("notes.txt")))));
    }

    void scannerIsIncremental()
    {
        QTemporaryDir dir;
        const QString root = QFileInfo(dir.path()).canonicalFilePath();
        QVERIFY(QDir(root).mkpath(QStringLiteral("Artist/Album")));
        QVERIFY(writeWav(root + "/Artist/Album/01 One.wav", 500));
        QVERIFY(writeWav(root + "/Artist/Album/02 Two.wav", 700));
        QFile txt(root + "/Artist/Album/cover.txt");
        QVERIFY(txt.open(QIODevice::WriteOnly));
        txt.close();

        TrackIndex idx;
        QVERIFY(idx.open(QStringLiteral(":memory:")));

        auto r = Scanner::scanFolders(&idx, {root});
        QCOMPARE(r.added, 2);
        QCOMPARE(r.total, 2);

        r = Scanner::scanFolders(&idx, {root});      // nothing changed
        QCOMPARE(r.added, 0);
        QCOMPARE(r.updated, 0);
        QCOMPARE(r.removed, 0);

        QVERIFY(QFile::remove(root + "/Artist/Album/02 Two.wav"));
        r = Scanner::scanFolders(&idx, {root});
        QCOMPARE(r.removed, 1);
        QCOMPARE(r.total, 1);

        // A folder that is missing (unplugged drive) must not wipe its tracks.
        r = Scanner::scanFolders(&idx, {root + "/does-not-exist"});
        QCOMPARE(r.total, 1);
    }

    void coverArtEmbeddedAndFolder()
    {
        QTemporaryDir dir;
        const QString root = QFileInfo(dir.path()).canonicalFilePath();
        const QString cache = root + "/cache";
        QVERIFY(QDir(root).mkpath("withart"));
        QVERIFY(QDir(root).mkpath("folderart"));
        QVERIFY(QDir(root).mkpath("noart"));
        QVERIFY(writeWav(root + "/withart/a.wav", 300));
        QVERIFY(writeWav(root + "/withart/b.wav", 300));
        QVERIFY(embedPng(root + "/withart/a.wav", tinyPng()));
        QVERIFY(embedPng(root + "/withart/b.wav", tinyPng()));
        QVERIFY(writeWav(root + "/folderart/c.wav", 300));
        QFile cover(root + "/folderart/Cover.JPG");
        QVERIFY(cover.open(QIODevice::WriteOnly));
        cover.write("not really a jpeg");
        cover.close();
        QVERIFY(writeWav(root + "/noart/d.wav", 300));

        const CoverArt::Picture pic = CoverArt::embedded(root + "/withart/a.wav");
        QCOMPARE(pic.data, tinyPng());
        QCOMPARE(pic.mime, QStringLiteral("image/png"));

        TrackIndex idx;
        QVERIFY(idx.open(QStringLiteral(":memory:")));
        Scanner::scanFolders(&idx, {root}, nullptr, {}, cache);
        QHash<QString, QString> art;
        for (const Track &t : idx.tracksUnder({root})) art.insert(QFileInfo(t.path).fileName(), t.art);
        QVERIFY(art.value("a.wav").startsWith(cache) && art.value("a.wav").endsWith(".png"));
        QCOMPARE(art.value("b.wav"), art.value("a.wav"));                // same cover stored once
        QCOMPARE(QDir(cache).entryList(QDir::Files).size(), 1);
        QFile stored(art.value("a.wav"));
        QVERIFY(stored.open(QIODevice::ReadOnly));
        QCOMPARE(stored.readAll(), tinyPng());
        QCOMPARE(art.value("c.wav"), root + "/folderart/Cover.JPG");      // case-insensitive folder image
        QVERIFY(art.value("d.wav").isEmpty());
    }

    void migratesVersion1Database()
    {
        QTemporaryDir dir;
        const QString db = dir.filePath("old.sqlite");
        {   // An index written by Hop-tunes 0.1 (schema v1, no art column).
            sqlite3 *h = nullptr;
            QCOMPARE(sqlite3_open(db.toUtf8().constData(), &h), SQLITE_OK);
            const char *sql =
                "CREATE TABLE track (id INTEGER PRIMARY KEY, path TEXT NOT NULL UNIQUE, mtime INTEGER NOT NULL DEFAULT 0,"
                " size INTEGER NOT NULL DEFAULT 0, duration_ms INTEGER NOT NULL DEFAULT 0, codec TEXT NOT NULL DEFAULT '',"
                " bitrate INTEGER NOT NULL DEFAULT 0, sample_rate INTEGER NOT NULL DEFAULT 0, title TEXT NOT NULL DEFAULT '',"
                " artist TEXT NOT NULL DEFAULT '', album_artist TEXT NOT NULL DEFAULT '', album TEXT NOT NULL DEFAULT '',"
                " genre TEXT NOT NULL DEFAULT '', disc INTEGER NOT NULL DEFAULT 0, track_no INTEGER NOT NULL DEFAULT 0,"
                " year INTEGER NOT NULL DEFAULT 0);"
                "INSERT INTO track (path, mtime, size, title) VALUES ('/m/old.mp3', 123, 456, 'Old Song');"
                "PRAGMA user_version=1;";
            QCOMPARE(sqlite3_exec(h, sql, nullptr, nullptr, nullptr), SQLITE_OK);
            sqlite3_close(h);
        }
        TrackIndex idx;
        QVERIFY2(idx.open(db), qPrintable(idx.lastError()));
        const QList<Track> all = idx.tracksUnder({QStringLiteral("/m")});
        QCOMPARE(all.size(), 1);
        QCOMPARE(all.first().title, QStringLiteral("Old Song"));    // data kept
        QVERIFY(all.first().art.isEmpty());
        QCOMPARE(idx.stampsUnder("/m").value("/m/old.mp3").mtime, qint64(0));   // forces one re-read for art
    }

    void collectionGrouping()
    {
        auto t = [](const char *artist, const char *album, int year, const char *art = "") {
            Track x; x.artist = artist; x.album = album; x.year = year; x.art = art; return x;
        };
        QList<Track> tracks = {t("The Zebras", "Stripes", 2001), t("The Zebras", "Stripes", 2001, "/a.png"),
                               t("The Zebras", "Later", 2020), t("Alpha", "First", 0), t("", "", 0),
                               t("beta", "B", 1999)};
        Collection c;
        c.build(tracks);
        QCOMPARE(c.albums().size(), 5);
        QStringList names;
        for (const Artist &a : c.artists()) names << a.name;
        // case-insensitive, "The" ignored for sorting, unknown artist last
        QCOMPARE(names, QStringList({"Alpha", "beta", "The Zebras", "unknown artist"}));
        const Artist &z = c.artists()[2];
        QCOMPARE(z.songCount, 3);
        QCOMPARE(c.albums()[z.albumIndices[0]].title, QStringLiteral("Later"));   // newest first
        QCOMPARE(c.albums()[z.albumIndices[1]].art, QStringLiteral("/a.png"));    // first art found
        QCOMPARE(c.albums()[z.albumIndices[1]].trackRows, QList<int>({0, 1}));
    }

    void asyncScannerSignals()
    {
        QTemporaryDir dir;
        QVERIFY(writeWav(dir.filePath(QStringLiteral("a.wav")), 200));
        TrackIndex idx;
        QVERIFY(idx.open(QStringLiteral(":memory:")));
        Scanner scanner(&idx);
        QSignalSpy done(&scanner, &Scanner::finished);
        scanner.start({dir.path()});
        QVERIFY(scanner.isRunning());
        QVERIFY(done.wait(10000));
        QCOMPARE(done.first().at(0).toInt(), 1);   // added
        QVERIFY(!scanner.isRunning());
    }
};

QTEST_GUILESS_MAIN(TestLibrary)
#include "test_library.moc"
