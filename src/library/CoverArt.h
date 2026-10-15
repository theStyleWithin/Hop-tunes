// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>

namespace hop::CoverArt {

struct Picture {
    QByteArray data;
    QString mime;           // "image/jpeg", "image/png", … (may be empty)
    bool isNull() const { return data.isEmpty(); }
};

// Embedded front cover (or first picture) from an audio file. Supports ID3v2 (MP3, WAV, AIFF),
// FLAC, MP4/M4A, Ogg Vorbis, Opus and ASF/WMA. Works with TagLib 1.11+ and 2.x.
Picture embedded(const QString &audioPath);

// A cover image sitting next to the audio files: cover/folder/front/album .jpg/.jpeg/.png,
// case-insensitive. Returns the image path or an empty string.
QString folderImage(const QString &directory);

// Writes picture data into `cacheDir` named by its SHA-1 (so identical covers are stored once)
// and returns the file path, or an empty string on failure.
QString store(const Picture &picture, const QString &cacheDir);

// Embedded art (stored in the cache) first, then a folder image.
QString find(const QString &audioPath, const QString &cacheDir);

} // namespace hop::CoverArt
