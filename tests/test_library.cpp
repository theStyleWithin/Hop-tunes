// SPDX-License-Identifier: GPL-3.0-or-later
// Unit tests for the library layer: track index + incremental scanner.
#include "library/Scanner.h"
#include "library/TagReader.h"
#include "library/TrackIndex.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

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
