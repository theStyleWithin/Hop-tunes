// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMetaType>
#include <QString>

namespace hop {

// One video file in the shared video index (spec §5b/§9.3, table `video`).
struct Video {
    qint64  id = 0;
    QString path;          // absolute, canonical
    qint64  mtime = 0;     // seconds since epoch
    qint64  size = 0;      // bytes
    qint64  durationMs = 0;
    QString codec;         // "h264", "hevc", "vp9", "av1", … (best-effort, from the demuxer caps)
    QString container;     // file extension, lower-case (mkv, mp4, …)
    int     width = 0;
    int     height = 0;
    bool    hdr = false;   // BT.2020 / PQ / HLG colorimetry
    double  frameRate = 0.0;
    QString thumb;         // cached frame-grab path, may be empty
    bool customThumb = false;   // thumb is this profile's own picked image (not stored in the index)
    qint64  added = 0;     // when the file first appeared in the index

    QString displayTitle() const;   // file name, extension stripped
    bool is4k() const { return width >= 3840 || height >= 2160; }
};

// Per-profile resume position for one video (profile.sqlite, keyed by path).
struct VideoProgress {
    qint64 positionMs = 0;
    qint64 durationMs = 0;
    qint64 updatedAt = 0;
};

} // namespace hop

Q_DECLARE_METATYPE(hop::Video)
