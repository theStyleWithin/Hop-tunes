// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace hop::VideoThumbnail {

// Grabs one frame near the start of the video (via a GStreamer decode pipeline, paused
// and seeked — no audio/video output, nothing shown on screen), scales it down and saves
// it as a JPEG under `cacheDir`, named by the hash of the file's path + modification time
// (so a re-encoded file at the same path gets a fresh thumbnail). Returns the cached file
// path, or an empty string if the file has no video stream or GStreamer can't decode it.
QString grab(const QString &videoPath, qint64 mtime, qint64 durationMs, const QString &cacheDir);

} // namespace hop::VideoThumbnail
