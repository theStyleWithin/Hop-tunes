// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QList>
#include <QString>
#include <QUrl>

namespace hop {

// Just enough of HLS (the playlist format Kick uses for past streams and clips) to download a finished video:
// a master playlist lists the qualities, a media playlist lists the pieces. No decoding, no network: text in, plain data out.
struct HlsVariant {
    QUrl url;
    qint64 bandwidth = 0;      // bits per second
    int width = 0, height = 0;
    QString name;              // "1080p60"
};

struct HlsPart {
    QUrl url;
    double seconds = 0;
    qint64 bytes = -1;         // known size, when the playlist says (byte ranges), else -1
    qint64 rangeStart = -1;    // with `bytes`: only this slice of the file is wanted (an HTTP Range request); -1 = the whole file
};

constexpr qint64 kHlsMaxJoinedPart = 16 * 1024 * 1024;

struct HlsMedia {
    QList<HlsPart> parts;      // pieces to fetch and join, in order
    bool finished = false;     // #EXT-X-ENDLIST: a finished video (a live one is still growing)
    double seconds = 0;
    qint64 firstSequence = 0;  // #EXT-X-MEDIA-SEQUENCE: the number of the first piece listed (a live playlist slides forward)
    double targetSeconds = 0;  // #EXT-X-TARGETDURATION
    QString unsupported;       // "" or why this playlist cannot be downloaded (encrypted, ...)
};

// the qualities of a master playlist, best first; an address that is not a master gives an empty list
QList<HlsVariant> parseHlsMaster(const QString &text, const QUrl &base);
// the pieces of a media playlist. Byte ranges (clips cut several files into slices) stay slices, fetched with a Range request; slices
// that follow each other in the same file are joined into one request (up to a size that fits in memory).
HlsMedia parseHlsMedia(const QString &text, const QUrl &base);
// the quality to use for a wish ("up to this many lines of height"; 0 = the best): the best one that fits, else the smallest there is
const HlsVariant *chooseHlsVariant(const QList<HlsVariant> &variants, int maxHeight);

} // namespace hop
