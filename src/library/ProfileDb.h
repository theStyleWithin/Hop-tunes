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

// A profile's manual edits to an album's shown title/cover (spec: per-profile, never
// shared). Keyed the same way as an album Pin's ref: "<artist>\x1f<title>". Empty fields
// mean "no override" for that one field.
struct AlbumOverride {
    QString title;
    QString art;   // absolute path to the profile's own copy of the chosen image
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

    // album overrides: manual title/cover edits and a manual track order, all per-profile
    // and keyed by the same "<artist>\x1f<title>" ref used for pins (App::albumRef()).
    QHash<QString, AlbumOverride> albumOverrides() const;   // all of them, loaded once per profile
    bool setAlbumOverrideTitle(const QString &ref, const QString &title);   // "" clears it
    bool setAlbumOverrideArt(const QString &ref, const QString &artPath);   // "" clears it
    QHash<QString, QString> videoMetas() const;   // video path -> JSON of the details the user filled in
    bool setVideoMeta(const QString &videoPath, const QString &json);   // "" clears it
    QSet<QString> playedItems() const;                       // finished audiobooks / podcast episodes
    bool setItemPlayed(const QString &path, bool played);
    QHash<QString, QString> videoThumbOverrides() const;   // video path -> picked image, loaded once per profile
    bool setVideoThumbOverride(const QString &videoPath, const QString &thumbPath);   // "" clears it
    QStringList albumTrackOrder(const QString &ref) const;          // paths, in order; empty: natural order
    bool setAlbumTrackOrder(const QString &ref, const QStringList &paths);

    // a manual order for the whole-library "songs" list, per profile; empty: natural order
    QStringList songOrder() const;
    bool setSongOrder(const QStringList &paths);

private:
    bool exec(const char *sql);
    bool migrate();

    sqlite3 *m_db = nullptr;
    mutable QString m_error;
};

} // namespace hop
