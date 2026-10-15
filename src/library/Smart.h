// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library/Track.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace hop {

// ---- instant search (spec §4.2) -----------------------------------------------------
// Case- and accent-insensitive; every word must appear in title, artist, album artist,
// album or genre ("beyonce lemon" finds "Beyoncé — Lemonade").
QString foldForSearch(const QString &s);
QStringList searchTokens(const QString &query);
bool matchesSearch(const Track &t, const QStringList &tokens);

// ---- smart playlists (spec §7) ------------------------------------------------------
struct SmartRule {
    QString field;   // title artist album genre | year plays skips | added_days played_days | hearted
    QString op;      // text: is is_not contains not_contains · number: eq ne gt lt · days: within not_within · hearted: is
    QString value;
};

struct SmartRules {
    bool matchAll = true;
    QList<SmartRule> rules;
    QString sort = QStringLiteral("album");   // album added plays played random title
    int limit = 0;                             // 0 = no limit

    static SmartRules fromJson(const QString &json);
    QString toJson() const;
    bool matches(const Track &t, const TrackStats &s, qint64 now) const;
    QList<Track> apply(const QList<Track> &library, const QHash<QString, TrackStats> &stats,
                       qint64 now, quint32 seed = 0) const;
};

// Built-in lists every profile has (spec §7 "auto lists").
struct AutoList { QString key; QString name; SmartRules rules; };
QList<AutoList> autoLists();

// ---- smart DJ (spec §4.4) -----------------------------------------------------------
// A mix seeded by an artist, from the local library only: the seed artist, then tracks
// close to it by genre and era, nudged by hearts and play history. No network.
QList<Track> smartMix(const QString &seedArtist, const QList<Track> &library,
                      const QHash<QString, TrackStats> &stats, int count, quint32 seed);

// ---- playlist files (spec §6 import, §7 export) ---------------------------------------
namespace PlaylistIO {
    struct Parsed {
        QString name;          // from the file's title, else its base name
        QStringList entries;   // as written in the file, normalised to '/' separators
        QString error;
    };
    // m3u, m3u8, pls, xspf, zpl and wpl (Zune / Windows Media playlists).
    Parsed read(const QString &file);
    // Maps entries to library paths: exact or relative to the playlist's folder first,
    // then by trailing path parts (artist/album/file), which finds songs from a playlist
    // made on another computer, such as a Windows Zune library.
    QStringList resolve(const QStringList &entries, const QString &playlistDir,
                        const QStringList &libraryPaths, int *unmatched = nullptr);
    // Extended M3U, UTF-8, paths relative to the playlist file.
    bool writeM3u8(const QString &file, const QString &name, const QList<Track> &tracks, QString *error = nullptr);
}

} // namespace hop
