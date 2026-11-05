// SPDX-License-Identifier: GPL-3.0-or-later
// Playback tests: real GStreamer pipeline into a fakesink (HOPTUNES_AUDIO_SINK=fakesink).
#include "playback/Player.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QtTest>

#include <cmath>

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

    // Android: while the app is in the background the audio thread moves to the next song by itself and only commits
    // the queue position (Player::onStreamStartedThread); the screen hears about it when the main loop runs again.
    void anAdvanceCommittedOnTheAudioThreadReachesTheScreenLater()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath(QStringLiteral("a.wav")), b = dir.filePath(QStringLiteral("b.wav"));
        QVERIFY(writeWav(a, 3000));
        QVERIFY(writeWav(b, 3000));
        Player p;
        QSignalSpy current(&p, &Player::currentChanged);
        QSignalSpy started(&p, &Player::trackStarted);
        p.setQueue({wavTrack(a, 1, 3000), wavTrack(b, 2, 3000)}, 0);
        QCOMPARE(p.currentIndex(), 0);
        const int currentBefore = current.count(), startedBefore = started.count();

        {   // what the audio thread does after the first song ends
            QMutexLocker lock(&p.m_gaplessMutex);
            p.m_pendingIndex = 1;
        }
        Player::onStreamStartedThread(p.m_playbin, &p);
        QCOMPARE(p.m_index, 1);                 // the queue already moved on
        QVERIFY(p.m_pendingIndex < 0);
        QVERIFY(p.m_uiDirty.load());
        QCOMPARE(current.count(), currentBefore);   // but the screen has not been told yet

        gst_bus_post(p.m_bus, gst_message_new_stream_start(GST_OBJECT(p.m_playbin)));   // the main loop is back
        QTRY_VERIFY_WITH_TIMEOUT(current.count() > currentBefore, 3000);
        QVERIFY(started.count() > startedBefore);
        QVERIFY(!p.m_uiDirty.load());
        QCOMPARE(p.currentTrackId(), qint64(2));
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
    // The audio analysis behind the visualizer and the games: a tone goes in, 32 band levels come out, and nothing
    // is posted until it is switched on.
    void spectrumFramesArriveOnlyWhenAsked()
    {
        QTemporaryDir dir;
        const QString f = dir.filePath(QStringLiteral("tone.wav"));
        {   // 2.5 s of a loud 100 Hz tone, 8 kHz mono
            const quint32 rate = 8000, n = rate * 5 / 2;
            QByteArray pcm(int(n * 2), '\0');
            for (quint32 i = 0; i < n; ++i) { const qint16 v = qint16(20000 * std::sin(2 * M_PI * 100.0 * i / rate)); qToLittleEndian(v, pcm.data() + i * 2); }
            QByteArray h;
            auto u32 = [&h](quint32 v) { char b[4]; qToLittleEndian(v, b); h.append(b, 4); };
            auto u16 = [&h](quint16 v) { char b[2]; qToLittleEndian(v, b); h.append(b, 2); };
            h.append("RIFF"); u32(36 + quint32(pcm.size())); h.append("WAVEfmt "); u32(16); u16(1); u16(1); u32(rate); u32(rate * 2); u16(2); u16(16);
            h.append("data"); u32(quint32(pcm.size()));
            QFile out(f); QVERIFY(out.open(QIODevice::WriteOnly)); out.write(h); out.write(pcm);
        }
        const QByteArray oldSink = qgetenv("HOPTUNES_AUDIO_SINK");
        qputenv("HOPTUNES_AUDIO_SINK", "fakesink sync=true");   // in real time, so the song is still playing when asked
        const auto restore = qScopeGuard([&] { qputenv("HOPTUNES_AUDIO_SINK", oldSink); });
        Player p;
        if (!p.analysisAvailable()) QSKIP("the GStreamer spectrum plugin (gst-plugins-good) is not installed");
        QSignalSpy frames(&p, &Player::spectrum);
        QVERIFY(!p.analysisEnabled());
        p.setQueue({wavTrack(f, 1, 2500)}, 0);
        QTest::qWait(600);
        QCOMPARE(frames.size(), 0);                      // not asked: nothing posted
        p.setAnalysisEnabled(true);
        QTest::qWait(1000);
        QVERIFY2(frames.size() >= 5, qPrintable(QString::number(frames.size())));
        const QVector<float> first = frames.first().first().value<QVector<float>>();
        QCOMPARE(first.size(), 32);
        float loudest = -100;
        for (const QList<QVariant> &fr : frames) loudest = qMax(loudest, fr.first().value<QVector<float>>().first());
        QVERIFY2(loudest > -50.f, qPrintable(QString::number(double(loudest))));   // the 100 Hz tone lands in the lowest band
        p.setAnalysisEnabled(false);
    }

    // Real music is stereo 44.1 kHz in mp3/flac/ogg/m4a, not 8 kHz mono WAV. Every one of them must play, with the
    // analysis off (normal listening) and on (visualizer open), without a single bus error. (1.7.4 broke playback on
    // a real machine: every song failed and the queue was skipped to the end.)
    void realFormatsPlayWithAndWithoutAnalysis()
    {
        const QByteArray dirEnv = qgetenv("HOPTUNES_REAL_AUDIO_DIR");   // a folder holding t.mp3 t.flac t.ogg t.m4a
        if (dirEnv.isEmpty()) QSKIP("set HOPTUNES_REAL_AUDIO_DIR to a folder with t.mp3 t.flac t.ogg t.m4a");
        const QByteArray oldSink = qgetenv("HOPTUNES_AUDIO_SINK");
        qputenv("HOPTUNES_AUDIO_SINK", "audioconvert ! audioresample ! fakesink sync=true");
        const auto restore = qScopeGuard([&] { qputenv("HOPTUNES_AUDIO_SINK", oldSink); });
        for (bool analysis : {false, true}) {
            for (const char *ext : {"mp3", "flac", "ogg", "m4a"}) {
                const QString f = QDir(QString::fromLocal8Bit(dirEnv)).filePath(QStringLiteral("t.") + QLatin1String(ext));
                if (!QFile::exists(f)) continue;
                Player p;
                p.setAnalysisEnabled(analysis);
                QSignalSpy errs(&p, &Player::errorChanged);
                p.setQueue({wavTrack(f, 1, 6000), wavTrack(f, 2, 6000), wavTrack(f, 3, 6000)}, 0);
                QTest::qWait(1500);
                const QString what = QStringLiteral("%1 analysis=%2 error='%3'").arg(QLatin1String(ext)).arg(analysis).arg(p.error());
                if (p.error().contains(QLatin1String("missing a plug-in"))) { qInfo("no decoder for %s here, skipped", ext); continue; }
                QVERIFY2(p.isPlaying(), qPrintable(what));
                QVERIFY2(p.currentIndex() == 0, qPrintable(what));     // not skipped on to the end
                QVERIFY2(p.position() > 300, qPrintable(what));
                QVERIFY2(p.error().isEmpty(), qPrintable(what));
            }
        }
    }

    // A systemic failure (every file unreadable) must stop after several tries and keep its message, not march
    // through the whole library to the last song.
    void aBrokenQueueStopsAfterAFewErrors()
    {
        QTemporaryDir dir;
        QList<Track> list;
        for (int i = 0; i < 30; ++i) {
            const QString f = dir.filePath(QStringLiteral("bad%1.mp3").arg(i));
            QFile out(f); QVERIFY(out.open(QIODevice::WriteOnly)); out.write("this is not audio at all");
            list << wavTrack(f, i + 1, 1000);
        }
        Player p;
        p.setQueue(list, 0);
        QTest::qWait(2500);
        QVERIFY2(p.currentIndex() <= 8, qPrintable(QString::number(p.currentIndex())));   // stopped after a handful, not at the end of 30
        QVERIFY(!p.isPlaying());
        QVERIFY(!p.error().isEmpty());
    }

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

    // Audiobooks and podcasts resume where they were left: playFrom() seeks once the pipeline
    // has prerolled (a seek before that is silently dropped).
    void playFromResumes()
    {
        QTemporaryDir dir;
        const QString a = dir.filePath(QStringLiteral("book.wav"));
        QVERIFY(writeWav(a, 6000));
        Player p;
        p.playFrom(wavTrack(a, 1, 6000), 3000);
        QVERIFY(p.isPlaying());
        QTRY_VERIFY_WITH_TIMEOUT(p.position() >= 3000, 10000);
        p.stop();

        // From the start when there is nothing to resume.
        p.playFrom(wavTrack(a, 1, 6000), 0);
        QTRY_VERIFY_WITH_TIMEOUT(p.position() > 0, 10000);
        QVERIFY(p.position() < 2500);
        p.stop();
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
