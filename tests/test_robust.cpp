// SPDX-License-Identifier: GPL-3.0-or-later
// Hostile and messy input must never crash Hop-tunes or stop it from starting:
// damaged or fake audio files, odd file names, symlink loops, damaged databases and
// settings, malformed playlists, huge or broken pictures.
#include "App.h"
#include "core/Paths.h"
#include "library/CollectionModels.h"
#include "library/Scanner.h"
#include "library/Smart.h"
#include "library/TagReader.h"
#include "library/TrackIndex.h"
#include "playback/Player.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>

#include <gst/gst.h>

#include <memory>

using namespace hop;

namespace {

bool writeWav(const QString &path, int ms)
{
    const quint32 rate = 8000, bytes = rate * 2 * ms / 1000;
    QByteArray h;
    auto u32 = [&h](quint32 v) { char b[4]; qToLittleEndian(v, b); h.append(b, 4); };
    auto u16 = [&h](quint16 v) { char b[2]; qToLittleEndian(v, b); h.append(b, 2); };
    h.append("RIFF"); u32(36 + bytes); h.append("WAVE"); h.append("fmt "); u32(16); u16(1); u16(1);
    u32(rate); u32(rate * 2); u16(2); u16(16); h.append("data"); u32(bytes);
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(h);
    f.write(QByteArray(int(bytes), '\0'));
    return true;
}

// A tiny VP8+Opus/Matroska clip (see test_video.cpp for the same recipe).
bool writeTestVideo(const QString &path, int ms = 1000)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    GError *err = nullptr;
    const QByteArray desc =
        QStringLiteral("videotestsrc num-buffers=%1 ! video/x-raw,width=320,height=240,framerate=25/1 "
                        "! vp8enc ! matroskamux name=mux ! filesink location=\"%2\" "
                        "audiotestsrc num-buffers=%1 ! audioconvert ! opusenc ! mux.")
            .arg(ms * 25 / 1000).arg(path).toUtf8();
    GstElement *bin = gst_parse_launch(desc.constData(), &err);
    if (err) { g_error_free(err); err = nullptr; }
    if (!bin) return false;
    gst_element_set_state(bin, GST_STATE_PLAYING);
    GstBus *bus = gst_element_get_bus(bin);
    bool ok = false;
    if (GstMessage *msg = gst_bus_timed_pop_filtered(bus, 10 * GST_SECOND,
            GstMessageType(GST_MESSAGE_EOS | GST_MESSAGE_ERROR))) {
        ok = GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS;
        gst_message_unref(msg);
    }
    gst_object_unref(bus);
    gst_element_set_state(bin, GST_STATE_NULL);
    gst_object_unref(bin);
    return ok && QFile::exists(path);
}

void writeBytes(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) f.write(data);
}

QByteArray noise(int n, quint32 seed)
{
    QByteArray b(n, '\0');
    QRandomGenerator rng(seed);
    for (int i = 0; i < n; ++i) b[i] = char(rng.bounded(256));
    return b;
}

} // namespace

class TestRobust : public QObject {
    Q_OBJECT
    QTemporaryDir m_home;

private slots:
    void initTestCase()
    {
        qputenv("HOME", m_home.path().toLocal8Bit());
        for (const char *v : {"XDG_DATA_HOME", "XDG_CONFIG_HOME", "XDG_CACHE_HOME", "HOPTUNES_PROFILE"}) qunsetenv(v);
        qputenv("HOPTUNES_AUDIO_SINK", "fakesink sync=true");
    }

    // A music folder full of things that are not what they claim to be.
    void scannerSurvivesMessyFolders()
    {
        const QString m = m_home.filePath(QStringLiteral("Mess"));
        QVERIFY(writeWav(m + QStringLiteral("/good/01 real song.wav"), 500));
        writeBytes(m + QStringLiteral("/empty.mp3"), {});                                      // zero bytes
        writeBytes(m + QStringLiteral("/noise.flac"), noise(64 * 1024, 1));                    // random bytes
        writeBytes(m + QStringLiteral("/fake.ogg"), "this is a text file, not Ogg Vorbis\n");  // wrong content
        writeBytes(m + QStringLiteral("/fLaC-header-only.flac"), QByteArray("fLaC") + noise(200, 2));
        writeBytes(m + QStringLiteral("/ID3-header-only.mp3"), QByteArray("ID3\x04\x00\x00\x7f\x7f\x7f\x7f", 10) + noise(300, 3));
        writeBytes(m + QStringLiteral("/truncated.m4a"), QByteArray("\x00\x00\x00\x20" "ftypM4A ", 12));
        writeBytes(m + QStringLiteral("/broken.wav"), QByteArray("RIFF\xff\xff\xff\x7fWAVEfmt ", 16));
        // File names: unicode, emoji, right-to-left, spaces, quotes, a newline.
        QVERIFY(writeWav(m + QStringLiteral("/名前/曲 ♪.wav"), 300));
        QVERIFY(writeWav(m + QStringLiteral("/emoji 🎵🎧.wav"), 300));
        QVERIFY(writeWav(m + QStringLiteral("/עברית שיר.wav"), 300));
        QVERIFY(writeWav(m + QStringLiteral("/it's \"quoted\" & <odd>.wav"), 300));
        QVERIFY(writeWav(m + QStringLiteral("/line\nbreak.wav"), 300));
        // Deep nesting and a symlink loop.
        QString deep = m;
        for (int i = 0; i < 40; ++i) deep += QStringLiteral("/d%1").arg(i);
        QVERIFY(writeWav(deep + QStringLiteral("/deep.wav"), 300));
        QVERIFY(QFile::link(m, m + QStringLiteral("/loop")));
        QVERIFY(QFile::link(m + QStringLiteral("/does-not-exist.mp3"), m + QStringLiteral("/dangling.mp3")));

        TrackIndex index;
        QVERIFY(index.open(QStringLiteral(":memory:")));
        const Scanner::Result r = Scanner::scanFolders(&index, {m}, nullptr, {}, m_home.filePath(QStringLiteral("art")));
        // Every real song is found; the fakes are either skipped or listed by file name;
        // nothing crashes and the loop does not trap the walk.
        const QList<Track> tracks = index.tracksUnder({QFileInfo(m).canonicalFilePath()});
        QStringList titles;
        for (const Track &t : tracks) titles << t.displayTitle();
        for (const char *want : {"01 real song", "曲 ♪", "emoji 🎵🎧", "עברית שיר", "it's \"quoted\" & <odd>", "line\nbreak", "deep"})
            QVERIFY2(titles.contains(QString::fromUtf8(want)), want);
        QVERIFY(r.scanned >= 7);
        // A second scan is a no-op, even with the junk still there.
        const Scanner::Result again = Scanner::scanFolders(&index, {m}, nullptr, {}, m_home.filePath(QStringLiteral("art")));
        QCOMPARE(again.added, 0);
        QCOMPARE(again.updated, 0);
    }

    void tagReaderOnGarbage()
    {
        for (int seed = 10; seed < 60; ++seed) {
            for (const char *ext : {"mp3", "flac", "ogg", "opus", "m4a", "wma", "wav", "aiff", "ape", "wv", "mpc"}) {
                const QString f = m_home.filePath(QStringLiteral("garbage/%1.%2").arg(seed).arg(QLatin1String(ext)));
                writeBytes(f, noise(512 + seed * 97, quint32(seed)));
                const Track t = TagReader::read(QFileInfo(f));      // must return, not crash
                QVERIFY(!t.displayTitle().isEmpty());
            }
        }
    }

    void playerSkipsUnplayableFiles()
    {
        const QString d = m_home.filePath(QStringLiteral("play"));
        writeBytes(d + QStringLiteral("/noise.mp3"), noise(32 * 1024, 7));
        writeBytes(d + QStringLiteral("/empty.flac"), {});
        QVERIFY(writeWav(d + QStringLiteral("/ok.wav"), 400));
        Player p;
        Track a; a.path = d + QStringLiteral("/noise.mp3");
        Track b; b.path = d + QStringLiteral("/empty.flac");
        Track c; c.path = d + QStringLiteral("/ok.wav"); c.title = QStringLiteral("ok");
        Track gone; gone.path = d + QStringLiteral("/vanished.mp3");
        p.setQueue({a, gone, b, c}, 0);
        QTRY_COMPARE_WITH_TIMEOUT(p.title(), QStringLiteral("ok"), 10000);   // skipped three bad ones
        QTRY_VERIFY_WITH_TIMEOUT(!p.isPlaying(), 10000);
        p.setCrossfadeMs(2000);                                              // fades with bad files too
        p.setQueue({c, a, c}, 0);
        QTRY_VERIFY_WITH_TIMEOUT(!p.isPlaying(), 15000);
    }

    void damagedIndexIsRebuilt()
    {
        Paths::ensureDirs();
        writeBytes(Paths::indexDbPath(), noise(8192, 99));                  // not a database at all
        App app;
        QVERIFY(app.init());                                                // starts anyway
        QDir shared(Paths::sharedDir());
        QVERIFY(!shared.entryList({QStringLiteral("index.sqlite.damaged-*")}).isEmpty());   // kept aside
        QVERIFY(QFile::exists(Paths::indexDbPath()));
    }

    void damagedProfileFilesAreHandled()
    {
        QString id;
        {
            App app;
            QVERIFY(app.init());
            QCOMPARE(app.createProfile(QStringLiteral("Robin"), QStringLiteral("#ff6b9a"), {}, QUrl::fromLocalFile(m_home.filePath(QStringLiteral("Mess")))), QString());
            id = app.profile().value("id").toString();
            QVERIFY(!app.createPlaylist(QStringLiteral("keep me")).isEmpty());
        }
        const QString dir = Paths::profilesDir() + QLatin1Char('/') + id;
        writeBytes(dir + QStringLiteral("/profile.sqlite"), noise(4096, 5));   // damaged lists
        writeBytes(dir + QStringLiteral("/settings.json"), "{ \"folders\": [ 42, null, ");   // damaged settings
        {
            App app;
            QVERIFY(app.init());
            QVERIFY(app.profileReady());                                        // one profile, no PIN: opens
            QVERIFY(app.profileDb()->isOpen());                                 // a fresh database
            QVERIFY(!QDir(dir).entryList({QStringLiteral("profile.sqlite.damaged-*")}).isEmpty());
            QVERIFY(!app.createPlaylist(QStringLiteral("new")).isEmpty());      // and it works
            app.player()->setQueue({}, 0);
        }
        writeBytes(Paths::profilesDir() + QStringLiteral("/profiles.json"), noise(300, 6));   // damaged profile list
        {
            App app;
            QVERIFY(app.init());
            QVERIFY(!app.profileReady());                                       // asks again instead of crashing
            QVERIFY(!QDir(Paths::profilesDir()).entryList({QStringLiteral("profiles.json.unreadable-*")}).isEmpty());
        }
    }

    void playlistFilesFromTheWild()
    {
        const QString d = m_home.filePath(QStringLiteral("lists"));
        writeBytes(d + QStringLiteral("/binary.m3u"), noise(100000, 8));
        writeBytes(d + QStringLiteral("/binary.zpl"), noise(5000, 9));
        writeBytes(d + QStringLiteral("/empty.pls"), {});
        writeBytes(d + QStringLiteral("/bomb.xspf"),
                   "<?xml version=\"1.0\"?><!DOCTYPE lolz [<!ENTITY lol \"lol\">"
                   "<!ENTITY lol2 \"&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;&lol;\">"
                   "<!ENTITY lol3 \"&lol2;&lol2;&lol2;&lol2;&lol2;&lol2;&lol2;&lol2;&lol2;&lol2;\">"
                   "<!ENTITY lol4 \"&lol3;&lol3;&lol3;&lol3;&lol3;&lol3;&lol3;&lol3;&lol3;&lol3;\">"
                   "<!ENTITY lol5 \"&lol4;&lol4;&lol4;&lol4;&lol4;&lol4;&lol4;&lol4;&lol4;&lol4;\">]>"
                   "<playlist><title>&lol5;</title><trackList><track><location>&lol5;</location></track></trackList></playlist>");
        writeBytes(d + QStringLiteral("/external.xspf"),
                   "<?xml version=\"1.0\"?><!DOCTYPE x [<!ENTITY ext SYSTEM \"file:///etc/passwd\">]>"
                   "<playlist><trackList><track><location>&ext;</location></track></trackList></playlist>");
        QString lines;
        for (int i = 0; i < 20000; ++i) lines += QStringLiteral("../../../../../../tmp/x%1.mp3\n").arg(i);
        writeBytes(d + QStringLiteral("/long.m3u8"), lines.toUtf8());
        for (const QString &f : QDir(d).entryList(QDir::Files)) {
            const auto p = PlaylistIO::read(d + QLatin1Char('/') + f);
            for (const QString &e : p.entries) QVERIFY(!e.contains(QStringLiteral("root:")));   // no file contents leak in
            int missing = 0;
            PlaylistIO::resolve(p.entries, d, {QStringLiteral("/music/a.mp3")}, &missing);    // returns, does not hang
        }
    }

    // HOPTUNES_BIG=<n> raises the count for manual runs (default 3000).
    void largeLibraryStaysFast()
    {
        const int n = qEnvironmentVariableIsSet("HOPTUNES_BIG") ? qEnvironmentVariableIntValue("HOPTUNES_BIG") : 3000;
        const QString lib = m_home.filePath(QStringLiteral("Big"));
        for (int i = 0; i < n; ++i)
            QVERIFY(writeWav(lib + QStringLiteral("/Artist %1/Album %2/%3 Song %4.wav").arg(i % 97).arg(i % 13).arg(i % 20 + 1).arg(i), 10));
        TrackIndex index;
        QVERIFY(index.open(m_home.filePath(QStringLiteral("big.sqlite"))));
        QElapsedTimer t;
        t.start();
        const Scanner::Result r = Scanner::scanFolders(&index, {lib});
        const qint64 firstScan = t.elapsed();
        QCOMPARE(r.added, n);
        t.restart();
        const Scanner::Result again = Scanner::scanFolders(&index, {lib});
        const qint64 rescan = t.elapsed();
        QCOMPARE(again.added + again.updated, 0);
        t.restart();
        const QList<Track> all = index.tracksUnder({QFileInfo(lib).canonicalFilePath()});
        const qint64 load = t.elapsed();
        QCOMPARE(all.size(), n);
        t.restart();
        Collection c;
        c.build(all);
        const qint64 group = t.elapsed();
        t.restart();
        int hits = 0;
        const QStringList tok = searchTokens(QStringLiteral("song 12"));
        for (const Track &tr : all) hits += matchesSearch(tr, tok);
        const qint64 search = t.elapsed();
        qInfo("%d songs: first scan %lld ms, rescan %lld ms, load %lld ms, group %lld ms, search %lld ms",
              n, firstScan, rescan, load, group, search);
        QVERIFY(hits > 0);
        // Spec §6: 10k tracks indexed in under 60 s. Per keystroke work must stay interactive.
        QVERIFY2(firstScan < qint64(n) * 6, "first scan too slow");
        QVERIFY2(search < qMax<qint64>(250, n / 40), "search too slow");
    }

    void brokenPicturesForAvatars()
    {
        App app;
        QVERIFY(app.init());
        if (!app.profileReady()) QCOMPARE(app.createProfile(QStringLiteral("Pic"), QStringLiteral("#7cc4ff"), {}, {}), QString());
        const QString f = m_home.filePath(QStringLiteral("pic.png"));
        writeBytes(f, QByteArray("\x89PNG\r\n\x1a\n", 8) + noise(2000, 11));
        QVERIFY(!app.setProfileAvatar(QUrl::fromLocalFile(f)).isEmpty());        // refused politely
        QImage big(6000, 4000, QImage::Format_RGB32);
        big.fill(Qt::darkCyan);
        QVERIFY(big.save(m_home.filePath(QStringLiteral("big.jpg"))));
        QCOMPARE(app.setProfileAvatar(QUrl::fromLocalFile(m_home.filePath(QStringLiteral("big.jpg")))), QString());   // scaled down
        QCOMPARE(QImage(app.profileStore()->avatarPath(app.profile().value("id").toString())).size(), QSize(256, 256));
        app.player()->setQueue({}, 0);
    }

    // Opening a video file (file manager "open with", CLI) routes to the video player —
    // tested here, with no QQmlApplicationEngine/window at all (unlike test_ui), because
    // hop.videoPlayerOpen becoming true is exactly what makes VideoPlayerView.qml
    // instantiate a real engine-backed surface item; in a build where that engine is the
    // GStreamer fallback (HOPTUNES_HAVE_MPV off), doing that under test_ui's software Qt
    // Quick backend was confirmed (by testing) to crash during window teardown — the same
    // category of risk already flagged for MpvVideoItem. App-level behaviour belongs here.
    void openingAVideoFileOpensThePlayer()
    {
        App app;
        QVERIFY(app.init());
        if (!app.profileReady()) QCOMPARE(app.createProfile(QStringLiteral("Vid"), QStringLiteral("#7cc4ff"), {}, {}), QString());
        const QString clip = m_home.filePath(QStringLiteral("clip.mkv"));
        QVERIFY(writeTestVideo(clip));

        QVERIFY(!app.videoPlayerOpen());
        app.openFiles({clip});
        QVERIFY(app.videoPlayerOpen());
        QCOMPARE(QFileInfo(app.videoPlayerPath()).canonicalFilePath(), QFileInfo(clip).canonicalFilePath());
        app.closeVideoPlayer(0, 0);
        QVERIFY(!app.videoPlayerOpen());
        app.player()->setQueue({}, 0);
    }
};

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    gst_init(&argc, &argv);
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("hop-tunes"));
    QGuiApplication::setOrganizationName(QStringLiteral("hop-tunes"));
    TestRobust t;
    return QTest::qExec(&t, argc, argv);
}

#include "test_robust.moc"
