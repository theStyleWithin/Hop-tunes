// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QMetaType>
#include <QString>

namespace hop {

// One audio file in the shared index (spec §9.3, table `track`).
struct Track {
    qint64  id = 0;
    QString path;          // absolute, canonical
    qint64  mtime = 0;     // seconds since epoch
    qint64  size = 0;      // bytes
    qint64  durationMs = 0;
    QString codec;         // file extension, lower-case (mp3, flac, …)
    int     bitrate = 0;   // kbit/s
    int     sampleRate = 0;
    QString title;
    QString artist;
    QString albumArtist;
    QString album;
    QString genre;
    int     disc = 0;
    int     trackNo = 0;
    int     year = 0;
    QString art;           // cover image path (cached embedded art or folder image), may be empty

    // What the UI shows when tags are missing.
    QString displayTitle() const;
    QString displayArtist() const;
    QString displayAlbum() const;
};

} // namespace hop

Q_DECLARE_METATYPE(hop::Track)
