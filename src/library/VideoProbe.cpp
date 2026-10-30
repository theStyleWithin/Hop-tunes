// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/VideoProbe.h"

#include <QUrl>

#ifdef HOPTUNES_HAVE_GST
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>
#endif

namespace hop::VideoProbe {

const QStringList &videoExtensions()
{
    static const QStringList exts = {
        QStringLiteral("mkv"), QStringLiteral("mp4"),  QStringLiteral("m4v"),
        QStringLiteral("webm"), QStringLiteral("mov"), QStringLiteral("avi"),
        QStringLiteral("wmv"), QStringLiteral("asf"),  QStringLiteral("ts"),
        QStringLiteral("m2ts"),
    };
    return exts;
}

bool isVideoFile(const QFileInfo &fi)
{
    return fi.isFile() && videoExtensions().contains(fi.suffix().toLower());
}

#ifdef HOPTUNES_HAVE_GST
namespace {

// Best-effort short codec name from the stream's caps (spec §5b lists these formats).
QString codecFromCaps(const GstCaps *caps)
{
    if (!caps || gst_caps_get_size(caps) == 0) return {};
    const GstStructure *st = gst_caps_get_structure(caps, 0);
    const QString name = QString::fromUtf8(gst_structure_get_name(st));
    if (name == QLatin1String("video/x-h264")) return QStringLiteral("h264");
    if (name == QLatin1String("video/x-h265")) return QStringLiteral("hevc");
    if (name == QLatin1String("video/x-vp8")) return QStringLiteral("vp8");
    if (name == QLatin1String("video/x-vp9")) return QStringLiteral("vp9");
    if (name == QLatin1String("video/x-av1")) return QStringLiteral("av1");
    if (name == QLatin1String("video/x-theora")) return QStringLiteral("theora");
    if (name == QLatin1String("video/x-wmv")) return QStringLiteral("wmv");
    if (name == QLatin1String("video/mpeg")) {
        int version = 0;
        if (gst_structure_get_int(st, "mpegversion", &version)) {
            if (version == 1) return QStringLiteral("mpeg1");
            if (version == 2) return QStringLiteral("mpeg2");
            if (version == 4) return QStringLiteral("mpeg4");
        }
        return QStringLiteral("mpeg");
    }
    // Fall back to the caps media subtype, e.g. "x-divx" -> "divx".
    return name.section(QLatin1Char('-'), 1);
}

// Heuristic only (spec §5b's "4K · HDR · HEVC" badges are informational, not frame-exact):
// a BT.2020 colour primaries string in the negotiated colorimetry is a strong HDR signal.
bool hdrFromCaps(const GstCaps *caps)
{
    if (!caps || gst_caps_get_size(caps) == 0) return false;
    const GstStructure *st = gst_caps_get_structure(caps, 0);
    const char *colorimetry = gst_structure_get_string(st, "colorimetry");
    return colorimetry && QString::fromUtf8(colorimetry).contains(QLatin1String("2020"));
}

} // namespace

Info probe(const QString &path)
{
    Info out;
    const QString uri = QUrl::fromLocalFile(path).toString();
    GError *err = nullptr;
    GstDiscoverer *disc = gst_discoverer_new(10 * GST_SECOND, &err);
    if (!disc) { if (err) g_error_free(err); return out; }

    GstDiscovererInfo *info = gst_discoverer_discover_uri(disc, uri.toUtf8().constData(), &err);
    if (err) { g_error_free(err); err = nullptr; }
    if (!info) { g_object_unref(disc); return out; }

    const GstDiscovererResult result = gst_discoverer_info_get_result(info);
    if (result != GST_DISCOVERER_OK && result != GST_DISCOVERER_MISSING_PLUGINS) {
        gst_discoverer_info_unref(info);
        g_object_unref(disc);
        return out;
    }

    out.durationMs = qint64(gst_discoverer_info_get_duration(info) / GST_MSECOND);

    GList *streams = gst_discoverer_info_get_video_streams(info);
    if (streams) {
        auto *videoInfo = GST_DISCOVERER_VIDEO_INFO(streams->data);
        out.width = int(gst_discoverer_video_info_get_width(videoInfo));
        out.height = int(gst_discoverer_video_info_get_height(videoInfo));
        const guint num = gst_discoverer_video_info_get_framerate_num(videoInfo);
        const guint den = gst_discoverer_video_info_get_framerate_denom(videoInfo);
        if (den > 0) out.frameRate = double(num) / double(den);

        GstCaps *caps = gst_discoverer_stream_info_get_caps(GST_DISCOVERER_STREAM_INFO(videoInfo));
        out.codec = codecFromCaps(caps);
        out.hdr = hdrFromCaps(caps);
        if (caps) gst_caps_unref(caps);
        out.valid = out.width > 0 && out.height > 0;
        gst_discoverer_stream_info_list_free(streams);
    }

    gst_discoverer_info_unref(info);
    g_object_unref(disc);
    return out;
}

#else   // no GStreamer (Android for now): nothing is probed, videos still list
Info probe(const QString &) { return {}; }
#endif

} // namespace hop::VideoProbe
