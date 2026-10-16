// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library/Track.h"
#include "library/Video.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

struct sqlite3;

namespace hop {

struct Playlist {
    qint64 id = 0;
    QString name;
    bool smart = false;
    QString rules;     // JSON (SmartRules) for smart playlists
    qint64 created = 0;
};

struct Pin {
    QString kind;      // "album" | "artist" | "playlist"
    QString ref;       // album: "<artist>\x1f<title>", artist: name, playlist: id
    QString label;
};

struct HistoryEntry {
    QString path;
    qint64 playedAt = 0;
};

// One profile's own data (spec §3.3, §9.3): play stats, hearts, history, playlists and
// Quickplay pins. Rows are keyed by file path, not index id, so they survive a rebuilt
// index. Guest mode opens ":memory:", so it keeps nothing. UI thread only.
class ProfileDb {
public:
    ProfileDb() = default;
    ~ProfileDb();
    ProfileDb(const ProfileDb &) = delete;
    ProfileDb &operator=(const ProfileDb &) = delete;

    bool open(const QString &path);   // creates the file (0600) if needed
    void close();
    bool isOpen() const { return m_db != nullptr; }
    QString lastError() const { return m_error; }

    // stats + history
    bool recordPlay(const QString &path, qint64 when);
    bool recordSkip(const QString &path);
    bool setHearted(const QString &path, bool hearted);
    QHash<QString, TrackStats> allStats() const;
    QList<HistoryEntry> history(int limit) const;   // newest first

    // playlists
    QList<Playlist> playlists() const;
    Playlist playlist(qint64 id) const;
    qint64 createPlaylist(const QString &name, bool smart = false, const QString &rules = {});
    bool renamePlaylist(qint64 id, const QString &name);
    bool setPlaylistRules(qint64 id, const QString &rules);
    bool deletePlaylist(qint64 id);
    QStringList playlistItems(qint64 id) const;     // paths in order
    bool setPlaylistItems(qint64 id, const QStringList &paths);
    bool addToPlaylist(qint64 id, const QStringList &paths);
    bool removeFromPlaylist(qint64 id, int pos);
    bool movePlaylistItem(qint64 id, int from, int to);

    // quickplay pins
    QList<Pin> pins() const;
    bool pin(const Pin &p);
    bool unpin(const QString &kind, const QString &ref);
    bool isPinned(const QString &kind, const QString &ref) const;

    // video resume position (spec §5b), keyed by path like everything else here
    VideoProgress videoProgress(const QString &path) const;
    bool setVideoProgress(const QString &path, qint64 positionMs, qint64 durationMs);

private:
    bool exec(const char *sql);
    bool migrate();

    sqlite3 *m_db = nullptr;
    mutable QString m_error;
};

} // namespace hop
