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
};

struct HlsMedia {
    QList<HlsPart> parts;      // pieces to fetch and join, in order
    bool finished = false;     // #EXT-X-ENDLIST: a finished video (a live one is still growing)
    double seconds = 0;
    QString unsupported;       // "" or why this playlist cannot be downloaded (encrypted, ...)
};

// the qualities of a master playlist, best first; an address that is not a master gives an empty list
QList<HlsVariant> parseHlsMaster(const QString &text, const QUrl &base);
// the pieces of a media playlist. A byte-range playlist that cuts one file into many ranges (as clips do) becomes ONE part: the whole
// file, which is the same bytes in the same order.
HlsMedia parseHlsMedia(const QString &text, const QUrl &base);
// the quality to use for a wish ("up to this many lines of height"; 0 = the best): the best one that fits, else the smallest there is
const HlsVariant *chooseHlsVariant(const QList<HlsVariant> &variants, int maxHeight);

} // namespace hop
