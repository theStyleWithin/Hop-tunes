// SPDX-License-Identifier: GPL-3.0-or-later
// Closing a video that is NOT the whole window (the small stream window, a player that is closed while the window lives on) used to abort
// inside libmpv: mpv was destroyed while its OpenGL render context was still alive. This plays a real clip through the real video item on a real
// (software) OpenGL context, then removes the item again and again; if mpv aborts, the whole test process dies and the test fails.
// It needs a display with OpenGL (xcb on Xvfb), so CMake only registers it where xvfb-run exists.
#include "ui/MpvVideoItem.h"

#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <gst/gst.h>

namespace {
// A ~4 s VP8/Matroska clip made with elements the audio playback already needs (gst-plugins-good)
bool writeClip(const QString &path)
{
    GError *err = nullptr;
    const QByteArray desc = QStringLiteral("videotestsrc num-buffers=100 ! video/x-raw,width=320,height=240,framerate=25/1 ! vp8enc ! matroskamux ! filesink location=\"%1\"").arg(path).toUtf8();
    GstElement *bin = gst_parse_launch(desc.constData(), &err);
    if (err) g_error_free(err);
    if (!bin) return false;
    gst_element_set_state(bin, GST_STATE_PLAYING);
    GstBus *bus = gst_element_get_bus(bin);
    GstMessage *msg = gst_bus_timed_pop_filtered(bus, 20 * GST_SECOND, GstMessageType(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
    const bool ok = msg && GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS;
    if (msg) gst_message_unref(msg);
    gst_object_unref(bus);
    gst_element_set_state(bin, GST_STATE_NULL);
    gst_object_unref(bin);
    return ok && QFile::exists(path);
}
}

class TestVideoTeardown : public QObject {
    Q_OBJECT
private slots:
    void aVideoClosedWhileTheWindowLivesDoesNotBringMpvDown()
    {
        gst_init(nullptr, nullptr);
        if (QGuiApplication::platformName() != QLatin1String("xcb")) QSKIP("needs a display with OpenGL (xcb)");
        QTemporaryDir dir;
        const QString clip = dir.filePath(QStringLiteral("clip.mkv"));
        if (!writeClip(clip)) QSKIP("GStreamer vp8enc/matroskamux not installed: cannot make the test clip");

        hop::registerVideoQmlTypes();
        QQmlApplicationEngine engine;
        engine.setInitialProperties({{"clip", clip}});
        engine.loadData(R"(
import QtQuick
import QtQuick.Window
import HopTunes.Video 1.0
Window {
  id: w; visible: true; width: 640; height: 360
  property string clip: ""
  property bool on: false
  property int shown: 0
  Loader { anchors.fill: parent; active: w.on
    sourceComponent: VideoSurface { Component.onCompleted: { source = w.clip; play() } } }
}
)", QUrl(QStringLiteral("file:///videoteardown/main.qml")));
        QVERIFY(!engine.rootObjects().isEmpty());
        auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(win);
        win->setProperty("clip", clip);
        QVERIFY(QTest::qWaitForWindowExposed(win));
        for (int i = 0; i < 6; ++i) {
            win->setProperty("on", true);                       // the video appears and draws for a moment (mpv makes its render context here)
            QTest::qWait(900);
            win->setProperty("on", false);                      // ...and goes while the window stays: this is what used to abort mpv
            QTest::qWait(500);
        }
        win->setProperty("on", true);                           // and a video that is still on screen when the window goes
        QTest::qWait(600);
        QVERIFY(true);                                          // (reaching here at all is the test: an aborting mpv ends the process)
    }
};

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    TestVideoTeardown t;
    return QTest::qExec(&t, argc, argv);
}
#include "test_video_teardown.moc"
