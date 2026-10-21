// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/VideoThumbnail.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QSaveFile>
#include <QUrl>

#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/video.h>

namespace hop::VideoThumbnail {

namespace {

// RAII so every early return still tears the pipeline down.
struct PipelineGuard {
    GstElement *bin = nullptr;
    ~PipelineGuard()
    {
        if (!bin) return;
        gst_element_set_state(bin, GST_STATE_NULL);
        gst_object_unref(bin);
    }
};

// Blocks until the pipeline leaves ASYNC (preroll done, or a seek settled). Returns false
// on a hard failure reported on the bus.
bool awaitReady(GstElement *bin)
{
    GstState state;
    const GstStateChangeReturn r = gst_element_get_state(bin, &state, nullptr, 5 * GST_SECOND);
    return r != GST_STATE_CHANGE_FAILURE;
}

} // namespace

QString grab(const QString &videoPath, qint64 mtime, qint64 durationMs, const QString &cacheDir)
{
    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash((videoPath + QLatin1Char('|') + QString::number(mtime)).toUtf8(),
                                  QCryptographicHash::Sha1).toHex());
    const QString out = cacheDir + QLatin1Char('/') + hash + QStringLiteral(".jpg");
    if (QFileInfo::exists(out)) return out;

    const QString uri = QUrl::fromLocalFile(videoPath).toString();
    GError *err = nullptr;
    const QByteArray desc =
        QStringLiteral("uridecodebin uri=\"%1\" name=dec ! videoconvert ! videoscale "
                        "! video/x-raw,format=RGB ! appsink name=sink sync=false max-buffers=1 drop=true")
            .arg(uri).toUtf8();
    GstElement *bin = gst_parse_launch(desc.constData(), &err);
    if (err) {
        qWarning("video thumbnail: pipeline for %s failed: %s", qPrintable(videoPath), err->message);
        g_error_free(err);
        err = nullptr;
    }
    if (!bin) return {};
    PipelineGuard guard{bin};

    GstElement *sink = gst_bin_get_by_name(GST_BIN(bin), "sink");
    if (!sink) return {};

    if (gst_element_set_state(bin, GST_STATE_PAUSED) == GST_STATE_CHANGE_FAILURE
        || !awaitReady(bin)) {
        gst_object_unref(sink);
        return {};
    }

    // Seek past any leading black/silence: 10% in, at least 1 s, but never past the end.
    const gint64 target = durationMs > 0
        ? qBound<qint64>(1000, durationMs / 10, qMax<qint64>(0, durationMs - 200))
        : 0;
    if (target > 0) {
        gst_element_seek_simple(bin, GST_FORMAT_TIME,
                                 GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                                 target * GST_MSECOND);
        awaitReady(bin);
    }

    GstSample *sample = gst_app_sink_pull_preroll(GST_APP_SINK(sink));
    gst_object_unref(sink);
    if (!sample) return {};

    QString result;
    GstCaps *caps = gst_sample_get_caps(sample);
    GstVideoInfo vinfo;
    if (caps && gst_video_info_from_caps(&vinfo, caps)) {
        GstBuffer *buf = gst_sample_get_buffer(sample);
        GstVideoFrame frame;
        if (buf && gst_video_frame_map(&frame, &vinfo, buf, GST_MAP_READ)) {
            const int w = GST_VIDEO_FRAME_WIDTH(&frame);
            const int h = GST_VIDEO_FRAME_HEIGHT(&frame);
            const int stride = GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 0);
            const auto *data = static_cast<const uchar *>(GST_VIDEO_FRAME_PLANE_DATA(&frame, 0));
            QImage img(data, w, h, stride, QImage::Format_RGB888);
            if (!img.isNull()) {
                img = img.copy();   // detach before the GStreamer buffer is unmapped
                if (img.width() > 480) img = img.scaledToWidth(480, Qt::SmoothTransformation);
                QDir().mkpath(cacheDir);
                QSaveFile f(out);
                if (f.open(QIODevice::WriteOnly) && img.save(&f, "JPG", 82) && f.commit())
                    result = out;
            }
            gst_video_frame_unmap(&frame);
        }
    }
    gst_sample_unref(sample);
    return result;
}

} // namespace hop::VideoThumbnail
