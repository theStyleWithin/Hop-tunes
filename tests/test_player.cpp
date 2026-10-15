// SPDX-License-Identifier: GPL-3.0-or-later
// Playback tests: real GStreamer pipeline into a fakesink (HOPTUNES_AUDIO_SINK=fakesink).
#include "playback/Player.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QtTest>

#include <gst/gst.h>

using namespace hop;

namespace {

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

Track wavTrack(const QString &path, qint64 id, int ms)
{
    Track t;
    t.id = id; t.path = path; t.durationMs = ms;
    t.title = QStringLiteral("song %1").arg(id);
    return t;
}

} // namespace

class TestPlayer : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        if (qEnvironmentVariableIsEmpty("HOPTUNES_AUDIO_SINK")) qputenv("HOPTUNES_AUDIO_SINK", "fakesink");
        gst_init(nullptr, nullptr);
        if (!gst_element_factory_find("wavparse") || !gst_element_factory_find("fakesink"))
            QSKIP("GStreamer wavparse/fakesink plugins not installed");
        guint major = 0, minor = 0, micro = 0, nano = 0;
        gst_version(&major, &minor, &micro, &nano);
        if (qgetenv("HOPTUNES_PLAYBIN") == "playbin3" && major == 1 && minor < 22)
            QSKIP("Hop-tunes does not use playbin3 before GStreamer 1.22 (known gapless bug)");
    }

    void emptyPlayer()
    {
        Player p;
        QVERIFY(!p.hasTrack());
        QVERIFY(!p.isPlaying());
        QCOMPARE(p.currentIndex(), -1);
        p.play();       // no-ops, must not crash
        p.next();
        p.previous();
        QVERIFY(!p.isPlaying());
    }

    // Two tracks queued: the second must start through the gapless path
    // (about-to-finish -> STREAM_START), then the queue ends with EOS.
    void gaplessAdvanceAndEnd()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath(QStringLiteral("a.wav")), b = dir.filePath(QStringLiteral("b.wav"));
        QVERIFY(writeWav(a, 400));
        QVERIFY(writeWav(b, 400));

        Player p;
        QVERIFY2(p.error().isEmpty(), qPrintable(p.error()));
        QSignalSpy current(&p, &Player::currentChanged);
        QSignalSpy playing(&p, &Player::playingChanged);

        p.setQueue({wavTrack(a, 1, 400), wavTrack(b, 2, 400)}, 0);
        QVERIFY(p.isPlaying());
        QCOMPARE(p.currentIndex(), 0);
        QCOMPARE(p.title(), QStringLiteral("song 1"));

        QTRY_COMPARE_WITH_TIMEOUT(p.currentIndex(), 1, 10000);   // advanced without our help
        QCOMPARE(p.currentTrackId(), qint64(2));
        QTRY_VERIFY_WITH_TIMEOUT(!p.isPlaying(), 10000);         // EOS at end of queue
        QVERIFY2(p.error().isEmpty(), qPrintable(p.error()));
        QVERIFY(current.count() >= 2);
    }

    void nextPreviousAndMissingFile()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath(QStringLiteral("a.wav"));
        QVERIFY(writeWav(a, 3000));
        Player p;
        p.setQueue({wavTrack(a, 1, 3000), wavTrack(dir.filePath(QStringLiteral("missing.wav")), 2, 1000),
                    wavTrack(a, 3, 3000)}, 0);
        QCOMPARE(p.currentIndex(), 0);
        p.next();                                   // jumps to the missing file...
        QTRY_COMPARE_WITH_TIMEOUT(p.currentIndex(), 2, 10000);   // ...which is skipped on error
        QVERIFY(!p.error().isEmpty());
        p.previous();                               // < 3 s in: goes back one track
        QCOMPARE(p.currentIndex(), 1);
        p.stop();
        QVERIFY(!p.isPlaying());
    }

    // ---- M5 ----
    void albumContinuity()
    {
        Track a; a.album = "A"; a.albumArtist = "X"; a.disc = 1; a.trackNo = 3;
        Track b = a; b.trackNo = 4;
        QVERIFY(Player::continuesAlbum(a, b));
        b.trackNo = 6;
        QVERIFY(!Player::continuesAlbum(a, b));          // skipping ahead: may fade
        Track c = a; c.disc = 2; c.trackNo = 1;
        QVERIFY(Player::continuesAlbum(a, c));           // onto disc 2
        Track d = b; d.trackNo = 4; d.album = "B";
        QVERIFY(!Player::continuesAlbum(a, d));
        Track e; Track f;                                // untagged: never "an album"
        QVERIFY(!Player::continuesAlbum(e, f));
    }

    void repeatModes()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath("a.wav"), b = dir.filePath("b.wav");
        QVERIFY(writeWav(a, 400));
        QVERIFY(writeWav(b, 400));
        Player p;
        QSignalSpy started(&p, &Player::trackStarted);
        p.setRepeatMode(Player::RepeatOne);
        p.setQueue({wavTrack(a, 1, 400), wavTrack(b, 2, 400)}, 0);
        QTRY_VERIFY_WITH_TIMEOUT(started.count() >= 3, 10000);     // the same song again and again
        QCOMPARE(p.currentIndex(), 0);
        p.next();                                                  // a manual skip still moves on
        QCOMPARE(p.currentIndex(), 1);
        p.setRepeatMode(Player::RepeatAll);
        QTRY_COMPARE_WITH_TIMEOUT(p.currentIndex(), 0, 10000);     // wraps around
        QVERIFY(p.isPlaying());
        p.cycleRepeat();
        QCOMPARE(p.repeatMode(), int(Player::RepeatOne));
        p.setRepeatMode(Player::RepeatOff);
        QTRY_VERIFY_WITH_TIMEOUT(!p.isPlaying(), 10000);
        QVERIFY2(p.error().isEmpty(), qPrintable(p.error()));
    }

    void shuffleKeepsCurrentAndRestores()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath("a.wav");
        QVERIFY(writeWav(a, 3000));
        QList<Track> q;
        for (int i = 0; i < 30; ++i) { Track t = wavTrack(a, i + 1, 3000); t.path = a + QString("#%1").arg(i); t.title = QString::number(i); q << t; }
        q[0].path = a;
        Player p;
        p.setQueue(q, 0);
        p.setShuffle(true);
        QCOMPARE(p.currentIndex(), 0);
        QCOMPARE(p.queue().first().title, QStringLiteral("0"));
        QCOMPARE(p.queueLength(), 30);
        QStringList order;
        for (const Track &t : p.queue()) order << t.title;
        QVERIFY(order != QStringList([] { QStringList l; for (int i = 0; i < 30; ++i) l << QString::number(i); return l; }()));
        p.setShuffle(false);
        QCOMPARE(p.queue().at(5).title, QStringLiteral("5"));      // original order back
        QCOMPARE(p.currentIndex(), 0);
        // With shuffle on, playing from row 7 starts with row 7.
        p.setShuffle(true);
        p.setQueue(q, 7);
        QCOMPARE(p.currentIndex(), 0);
        QCOMPARE(p.title(), QStringLiteral("7"));
        p.stop();
    }

    void moveAndRemove()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath("a.wav");
        QVERIFY(writeWav(a, 3000));
        QList<Track> q;
        for (int i = 0; i < 5; ++i) { Track t = wavTrack(a, i + 1, 3000); t.title = QString::number(i); q << t; }
        Player p;
        p.setQueue(q, 2);
        QVERIFY(p.moveQueueItem(4, 0));          // a later song to the top: current shifts down
        QCOMPARE(p.currentIndex(), 3);
        QCOMPARE(p.title(), QStringLiteral("2"));
        QVERIFY(p.moveQueueItem(3, 4));          // move the playing song itself
        QCOMPARE(p.currentIndex(), 4);
        QCOMPARE(p.title(), QStringLiteral("2"));
        QVERIFY(!p.removeFromQueue(4));          // not the current song
        QVERIFY(p.removeFromQueue(0));
        QCOMPARE(p.currentIndex(), 3);
        QCOMPARE(p.queueLength(), 4);
        QCOMPARE(p.queueModel()->count(), 4);
        QVERIFY(!p.moveQueueItem(0, 9));
        p.stop();
    }

    void crossfadeBetweenAlbums()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath("a.wav"), b = dir.filePath("b.wav");
        QVERIFY(writeWav(a, 3000));
        QVERIFY(writeWav(b, 3000));
        Track ta = wavTrack(a, 1, 3000); ta.album = "One"; ta.trackNo = 1;
        Track tb = wavTrack(b, 2, 3000); tb.album = "Two"; tb.trackNo = 1;
        Player p;
        p.setCrossfadeMs(1500);
        QSignalSpy fading(&p, &Player::fadingChanged);
        QElapsedTimer clock;
        clock.start();
        p.setQueue({ta, tb}, 0);
        QTRY_COMPARE_WITH_TIMEOUT(p.currentIndex(), 1, 10000);
        const qint64 switchedAt = clock.elapsed();
        QVERIFY2(switchedAt < 2600, qPrintable(QString::number(switchedAt)));   // before the first song ended
        QVERIFY(p.fading() || fading.count() >= 1);
        QTRY_VERIFY_WITH_TIMEOUT(!p.fading(), 5000);
        QVERIFY(p.isPlaying());
        QCOMPARE(fading.count(), 2);                 // started and finished
        QTRY_VERIFY_WITH_TIMEOUT(!p.isPlaying(), 10000);
        QVERIFY2(p.error().isEmpty(), qPrintable(p.error()));
    }

    void albumStaysGapless()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath("a.wav"), b = dir.filePath("b.wav");
        QVERIFY(writeWav(a, 1200));
        QVERIFY(writeWav(b, 1200));
        Track ta = wavTrack(a, 1, 1200); ta.album = "Same"; ta.albumArtist = "X"; ta.trackNo = 1;
        Track tb = wavTrack(b, 2, 1200); tb.album = "Same"; tb.albumArtist = "X"; tb.trackNo = 2;
        Player p;
        p.setCrossfadeMs(1000);
        QSignalSpy fading(&p, &Player::fadingChanged);
        p.setQueue({ta, tb}, 0);
        QTRY_COMPARE_WITH_TIMEOUT(p.currentIndex(), 1, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!p.isPlaying(), 10000);
        QCOMPARE(fading.count(), 0);
    }

    void replayGain()
    {
        Player p;
        if (!p.replayGainAvailable()) QSKIP("GStreamer replaygain plugin (rgvolume) not installed");
        QTemporaryDir dir;
        const QString a = dir.filePath("a.wav");
        QVERIFY(writeWav(a, 600));
        p.setReplayGainMode(Player::GainAlbum);
        p.setReplayGainPreamp(3.0);
        QCOMPARE(p.replayGainMode(), int(Player::GainAlbum));
        p.setQueue({wavTrack(a, 1, 600)}, 0);
        QVERIFY(p.isPlaying());
        QTRY_VERIFY_WITH_TIMEOUT(!p.isPlaying(), 10000);   // plays through the gain filter
        QVERIFY2(p.error().isEmpty(), qPrintable(p.error()));
        p.setReplayGainPreamp(99);
        QCOMPARE(p.replayGainPreamp(), 15.0);
    }
};

QTEST_GUILESS_MAIN(TestPlayer)
#include "test_player.moc"
