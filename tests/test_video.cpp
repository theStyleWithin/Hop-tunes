// SPDX-License-Identifier: GPL-3.0-or-later
// Video (spec §5b): probing, scanning, thumbnails, the headless (vo=null/ao=null) mpv
// core, and resume-position persistence. No GPU/display needed — MpvVideoItem's render
// glue is exercised only by building, not by any test here (see its header comment).
#include "library/ProfileDb.h"
#include "library/VideoIndex.h"
#include "library/VideoProbe.h"
#include "library/VideoScanner.h"
#include "library/VideoThumbnail.h"
#include "playback/GstVideoPlayer.h"
#include "playback/MpvCore.h"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest>

#include <clocale>

#include <gst/gst.h>

using namespace hop;

namespace {

// A ~2s VP8+Opus/Matroska clip, built with elements already required for audio playback
// (gst-plugins-good/base) — no extra runtime dependency for this test fixture.
bool writeTestClip(const QString &path, int width = 320, int height = 240, int ms = 2000)
{
    GError *err = nullptr;
    const QByteArray desc =
        QStringLiteral("videotestsrc num-buffers=%1 ! video/x-raw,width=%2,height=%3,framerate=25/1 "
                        "! vp8enc ! matroskamux name=mux ! filesink location=\"%4\" "
                        "audiotestsrc num-buffers=%1 ! audioconvert ! opusenc ! mux.")
            .arg(ms * 25 / 1000).arg(width).arg(height).arg(path).toUtf8();
    GstElement *bin = gst_parse_launch(desc.constData(), &err);
    if (err) { g_error_free(err); err = nullptr; }
    if (!bin) return false;
    gst_element_set_state(bin, GST_STATE_PLAYING);
    GstBus *bus = gst_element_get_bus(bin);
    bool ok = false;
    while (true) {
        GstMessage *msg = gst_bus_timed_pop_filtered(bus, 10 * GST_SECOND,
            GstMessageType(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
        if (!msg) break;
        ok = GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS;
        gst_message_unref(msg);
        break;
    }
    gst_object_unref(bus);
    gst_element_set_state(bin, GST_STATE_NULL);
    gst_object_unref(bin);
    return ok && QFile::exists(path);
}

} // namespace

class TestVideo : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        gst_init(nullptr, nullptr);
        if (!gst_element_factory_find("vp8enc") || !gst_element_factory_find("matroskamux")
            || !gst_element_factory_find("opusenc"))
            QSKIP("GStreamer vp8enc/matroskamux/opusenc not installed; cannot build the test clip");
    }

    void probeReadsCodecResolutionAndDuration()
    {
        QTemporaryDir dir;
        const QString clip = dir.filePath(QStringLiteral("clip.mkv"));
        QVERIFY(writeTestClip(clip, 320, 240, 2000));

        const VideoProbe::Info info = VideoProbe::probe(clip);
        QVERIFY(info.valid);
        QCOMPARE(info.width, 320);
        QCOMPARE(info.height, 240);
        QCOMPARE(info.codec, QStringLiteral("vp8"));
        QVERIFY(info.durationMs > 1500 && info.durationMs < 2600);
        QVERIFY(!info.hdr);   // plain videotestsrc output, no BT.2020 colorimetry
    }

    void probeRejectsNonVideo()
    {
        QTemporaryDir dir;
        const QString notVideo = dir.filePath(QStringLiteral("not-a-video.mkv"));
        QFile f(notVideo);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("not actually a video file");
        f.close();
        QVERIFY(!VideoProbe::probe(notVideo).valid);
    }

    void thumbnailGrabsAndCaches()
    {
        QTemporaryDir dir;
        const QString clip = dir.filePath(QStringLiteral("clip.mkv"));
        QVERIFY(writeTestClip(clip, 320, 240, 2000));
        const QString cacheDir = dir.filePath(QStringLiteral("thumbs"));
        const qint64 mtime = QFileInfo(clip).lastModified().toSecsSinceEpoch();

        const QString first = VideoThumbnail::grab(clip, mtime, 2000, cacheDir);
        QVERIFY(!first.isEmpty());
        QVERIFY(QFileInfo::exists(first));
        QVERIFY(QImage(first).width() > 0);

        // Same path+mtime: cached, no second decode.
        QCOMPARE(VideoThumbnail::grab(clip, mtime, 2000, cacheDir), first);
    }

    void scannerFindsUpdatesAndRemoves()
    {
        QTemporaryDir dir;
        const QString root = dir.path();
        QVERIFY(writeTestClip(root + QStringLiteral("/a.mkv"), 320, 240, 1000));

        VideoIndex idx;
        QVERIFY(idx.open(QStringLiteral(":memory:")));

        auto r = VideoScanner::scanFolders(&idx, {root});
        QCOMPARE(r.added, 1);
        QCOMPARE(idx.count(), 1);
        const Video v = idx.videosUnder({root}).constFirst();
        QCOMPARE(v.codec, QStringLiteral("vp8"));
        QCOMPARE(v.width, 320);
        QVERIFY(v.durationMs > 0);

        r = VideoScanner::scanFolders(&idx, {root});   // nothing changed
        QCOMPARE(r.added, 0);
        QCOMPARE(r.updated, 0);
        QCOMPARE(r.removed, 0);

        QFile::remove(root + QStringLiteral("/a.mkv"));
        r = VideoScanner::scanFolders(&idx, {root});
        QCOMPARE(r.removed, 1);
        QCOMPARE(idx.count(), 0);
    }

    // Regression test for a real-hardware bug (Steam Deck running CachyOS Handheld
    // Edition, v1.1.0): libmpv's own sanity check in mpv_create() rejects a process whose
    // LC_NUMERIC isn't "C" (it parses its own option values assuming '.' as the decimal
    // separator), and Qt/GLib/GStreamer
    // startup code commonly sets the process locale from the environment before any
    // MpvCore exists — reproduced locally with LANG=de_DE.UTF-8: mpv printed "Non-C locale
    // detected. This is not supported." and mpv_create() returned null. CI's own runners
    // default to the C locale already, which is exactly why this never showed up there.
    void mpvCoreInitializesUnderNonCLocale()
    {
#ifndef HOPTUNES_HAVE_MPV
        QSKIP("built without libmpv (see CMakeLists.txt / the Flatpak manifest's comment)");
#endif
        // RAII: QVERIFY/QVERIFY2 return out of this function early on failure, so a plain
        // restore call at the bottom would never run on the very failure this test exists
        // to catch — leaving the process locale poisoned for every test that runs after it.
        struct LocaleGuard {
            QByteArray saved = QByteArray(setlocale(LC_ALL, nullptr));
            ~LocaleGuard() { setlocale(LC_ALL, saved.constData()); }
        } guard;

        bool nonCLocaleAvailable = false;
        for (const char *name : {"de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8", "fr_FR.utf8"}) {
            if (setlocale(LC_ALL, name)) { nonCLocaleAvailable = true; break; }
        }
        if (!nonCLocaleAvailable)
            QSKIP("no non-C locale installed on this system to reproduce the bug with");
        QVERIFY(QByteArray(setlocale(LC_NUMERIC, nullptr)) != "C");

        MpvCore core;
        core.setOption(QStringLiteral("vo"), QStringLiteral("null"));
        core.setOption(QStringLiteral("ao"), QStringLiteral("null"));
        QVERIFY2(core.init(), qPrintable(core.error()));
    }

    void mpvCoreLoadsPlaysSeeksAndReportsDuration()
    {
#ifndef HOPTUNES_HAVE_MPV
        QSKIP("built without libmpv (see CMakeLists.txt / the Flatpak manifest's comment)");
#endif
        QTemporaryDir dir;
        const QString clip = dir.filePath(QStringLiteral("clip.mkv"));
        QVERIFY(writeTestClip(clip, 320, 240, 2000));

        MpvCore core;
        core.setOption(QStringLiteral("vo"), QStringLiteral("null"));
        core.setOption(QStringLiteral("ao"), QStringLiteral("null"));
        QVERIFY2(core.init(), qPrintable(core.error()));

        QSignalSpy loaded(&core, &MpvCore::fileLoaded);
        QSignalSpy durationChanged(&core, &MpvCore::durationChanged);
        QVERIFY(core.open(clip));
        QVERIFY(loaded.wait(10000));
        QVERIFY(durationChanged.wait(5000) || core.duration() > 0);
        QVERIFY(core.duration() > 1500 && core.duration() < 2600);

        core.pause();
        QTest::qWait(50);
        // processEvents is driven by mpv's wakeup callback (queued), give it a beat.
        QTRY_VERIFY_WITH_TIMEOUT(core.paused(), 2000);

        core.seek(1000);
        QTRY_VERIFY_WITH_TIMEOUT(core.position() > 500, 3000);

        core.play();
        QTRY_VERIFY_WITH_TIMEOUT(!core.paused(), 2000);
    }

    void mpvCoreAutoloadsSidecarSubtitle()
    {
#ifndef HOPTUNES_HAVE_MPV
        QSKIP("built without libmpv (see CMakeLists.txt / the Flatpak manifest's comment)");
#endif
        QTemporaryDir dir;
        const QString clip = dir.filePath(QStringLiteral("movie.mkv"));
        QVERIFY(writeTestClip(clip, 320, 240, 1000));
        QFile srt(dir.filePath(QStringLiteral("movie.srt")));
        QVERIFY(srt.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream(&srt) << "1\n00:00:00,000 --> 00:00:00,900\nhello\n";
        srt.close();

        MpvCore core;
        core.setOption(QStringLiteral("vo"), QStringLiteral("null"));
        core.setOption(QStringLiteral("ao"), QStringLiteral("null"));
        QVERIFY(core.init());
        QSignalSpy loaded(&core, &MpvCore::fileLoaded);
        QVERIFY(core.open(clip));
        QVERIFY(loaded.wait(10000));
        QTRY_VERIFY_WITH_TIMEOUT(!core.subtitleTracks().isEmpty(), 3000);
    }

    // The GStreamer fallback engine (spec §5b, used when HOPTUNES_HAVE_MPV is off —
    // currently Flatpak). HOPTUNES_VIDEO_SINK=fakesink bypasses the real qml6glsink/
    // glsinkbin chain entirely (same idea as HOPTUNES_AUDIO_SINK for Player.cpp), so this
    // doesn't depend on those plugins being installed, and attachSurface(nullptr) is valid
    // — nothing here needs a real QQuickItem, unlike the on-screen render path.
    void gstVideoPlayerLoadsPlaysSeeksAndReportsDuration()
    {
        QTemporaryDir dir;
        const QString clip = dir.filePath(QStringLiteral("clip.mkv"));
        QVERIFY(writeTestClip(clip, 320, 240, 2000));

        qputenv("HOPTUNES_VIDEO_SINK", "fakesink sync=true");
        GstVideoPlayer player;
        player.attachSurface(nullptr);
        QVERIFY2(player.error().isEmpty(), qPrintable(player.error()));

        QSignalSpy loaded(&player, &GstVideoPlayer::fileLoaded);
        QSignalSpy durationChanged(&player, &GstVideoPlayer::durationChanged);
        QVERIFY(player.openSource(clip));
        QVERIFY(loaded.wait(10000));
        QVERIFY(durationChanged.wait(5000) || player.duration() > 0);
        QVERIFY(player.duration() > 1500 && player.duration() < 2600);
        QVERIFY(!player.paused());

        player.pause();
        QTRY_VERIFY_WITH_TIMEOUT(player.paused(), 2000);

        player.seek(1000);
        QTRY_VERIFY_WITH_TIMEOUT(player.position() > 500, 3000);

        player.play();
        QTRY_VERIFY_WITH_TIMEOUT(!player.paused(), 2000);
        qunsetenv("HOPTUNES_VIDEO_SINK");
    }

    void gstVideoPlayerFindsSidecarSubtitle()
    {
        QTemporaryDir dir;
        const QString clip = dir.filePath(QStringLiteral("movie.mkv"));
        QVERIFY(writeTestClip(clip, 320, 240, 1000));
        QFile srt(dir.filePath(QStringLiteral("movie.srt")));
        QVERIFY(srt.open(QIODevice::WriteOnly | QIODevice::Text));
        // GStreamer's SRT typefind needs more than a one-liner to commit to a match
        // (confirmed by testing — real subtitle files are never this short anyway).
        QTextStream(&srt) << "1\n00:00:00,000 --> 00:00:00,900\nHello, this is a test subtitle line.\n\n"
                              "2\n00:00:00,900 --> 00:00:01,900\nA second line to pad the file out a little more.\n\n";
        srt.close();

        qputenv("HOPTUNES_VIDEO_SINK", "fakesink sync=true");
        GstVideoPlayer player;
        player.attachSurface(nullptr);
        QSignalSpy loaded(&player, &GstVideoPlayer::fileLoaded);
        QVERIFY(player.openSource(clip));
        QVERIFY(loaded.wait(10000));
        QTRY_VERIFY_WITH_TIMEOUT(!player.subtitleTracks().isEmpty(), 3000);
        qunsetenv("HOPTUNES_VIDEO_SINK");
    }

    void resumePositionRoundTripsAndForgetsWhenFinished()
    {
        ProfileDb db;
        QVERIFY(db.open(QStringLiteral(":memory:")));

        QCOMPARE(db.videoProgress(QStringLiteral("/a.mkv")).positionMs, qint64(0));
        QVERIFY(db.setVideoProgress(QStringLiteral("/a.mkv"), 42000, 100000));
        QCOMPARE(db.videoProgress(QStringLiteral("/a.mkv")).positionMs, qint64(42000));

        // Close enough to the end: forgotten, so it starts over next time.
        QVERIFY(db.setVideoProgress(QStringLiteral("/a.mkv"), 99000, 100000));
        QCOMPARE(db.videoProgress(QStringLiteral("/a.mkv")).positionMs, qint64(0));
    }
};

QTEST_GUILESS_MAIN(TestVideo)
#include "test_video.moc"
