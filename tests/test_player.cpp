// SPDX-License-Identifier: GPL-3.0-or-later
// Playback tests: real GStreamer pipeline into a fakesink (HOPTUNES_AUDIO_SINK=fakesink).
#include "playback/Player.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtEndian>
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
};

QTEST_GUILESS_MAIN(TestPlayer)
#include "test_player.moc"
