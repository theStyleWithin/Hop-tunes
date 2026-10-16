// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFileInfo>
#include <QStringList>

namespace hop::VideoProbe {

// Lower-case extensions the scanner treats as video (spec §5b).
const QStringList &videoExtensions();
bool isVideoFile(const QFileInfo &fi);

struct Info {
    qint64  durationMs = 0;
    QString codec;      // best-effort short name: h264, hevc, vp9, av1, mpeg4, vc1, mpeg2, …
    int     width = 0;
    int     height = 0;
    bool    hdr = false; // BT.2020 primaries or a PQ/HLG transfer function
    double  frameRate = 0.0;
    bool    valid = false;
};

// Probes codec/resolution/duration without decoding the whole file, via GStreamer's
// GstDiscoverer (part of gst-plugins-base, already a hard dependency for audio playback).
// Safe to call from any thread; blocks for up to a few seconds per file.
Info probe(const QString &path);

} // namespace hop::VideoProbe
