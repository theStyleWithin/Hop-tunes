// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/ProfileDb.h"

#include "core/Sql.h"

#include <QDateTime>
#include <QFile>

#include <sqlite3.h>

namespace hop {

using sql::Stmt;

ProfileDb::~ProfileDb() { close(); }

void ProfileDb::close()
{
    if (m_db) sqlite3_close(m_db);
    m_db = nullptr;
}

bool ProfileDb::exec(const char *sql)
{
    char *err = nullptr;
    if (sqlite3_exec(m_db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        m_error = QString::fromUtf8(err ? err : "sqlite error");
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool ProfileDb::open(const QString &path)
{
    close();
    const bool memory = path == QLatin1String(":memory:");
    if (sqlite3_open_v2(path.toUtf8().constData(), &m_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) {
        m_error = QString::fromUtf8(sqlite3_errmsg(m_db));
        close();
        return false;
    }
    if (!memory) QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    exec("PRAGMA foreign_keys=ON");
    exec("PRAGMA journal_mode=WAL");
    if (!migrate()) { close(); return false; }
    return true;
}

bool ProfileDb::migrate()
{
    Stmt v(m_db, "PRAGMA user_version");
    int version = (v.ok() && v.step() == SQLITE_ROW) ? v.i32(0) : 0;
    if (version < 1) {
        const bool ok = exec("BEGIN")
            && exec("CREATE TABLE stats (path TEXT PRIMARY KEY, play_count INTEGER NOT NULL DEFAULT 0,"
                    " skip_count INTEGER NOT NULL DEFAULT 0, last_played INTEGER NOT NULL DEFAULT 0,"
                    " hearted INTEGER NOT NULL DEFAULT 0)")
            && exec("CREATE TABLE history (id INTEGER PRIMARY KEY, path TEXT NOT NULL, played_at INTEGER NOT NULL)")
            && exec("CREATE INDEX history_time ON history(played_at)")
            && exec("CREATE TABLE playlist (id INTEGER PRIMARY KEY, name TEXT NOT NULL, smart INTEGER NOT NULL DEFAULT 0,"
                    " rules TEXT NOT NULL DEFAULT '', created INTEGER NOT NULL DEFAULT 0)")
            && exec("CREATE TABLE playlist_item (playlist_id INTEGER NOT NULL REFERENCES playlist(id) ON DELETE CASCADE,"
                    " pos INTEGER NOT NULL, path TEXT NOT NULL, PRIMARY KEY (playlist_id, pos))")
            && exec("CREATE TABLE pin (kind TEXT NOT NULL, ref TEXT NOT NULL, label TEXT NOT NULL DEFAULT '',"
                    " pos INTEGER NOT NULL DEFAULT 0, PRIMARY KEY (kind, ref))")
            && exec("PRAGMA user_version=1")
            && exec("COMMIT");
        if (!ok) { exec("ROLLBACK"); return false; }
        version = 1;
    }
    if (version < 2) {
        // v2: video resume position (spec §5b).
        const bool ok = exec("BEGIN")
            && exec("CREATE TABLE video_progress (path TEXT PRIMARY KEY, position_ms INTEGER NOT NULL DEFAULT 0,"
                    " duration_ms INTEGER NOT NULL DEFAULT 0, updated_at INTEGER NOT NULL DEFAULT 0)")
            && exec("PRAGMA user_version=2")
            && exec("COMMIT");
        if (!ok) { exec("ROLLBACK"); return false; }
        version = 2;
    }
    if (version < 3) {
        // v3: per-profile album overrides (custom title/cover) and a manual track order.
        const bool ok = exec("BEGIN")
            && exec("CREATE TABLE album_override (ref TEXT PRIMARY KEY,"
                    " title TEXT NOT NULL DEFAULT '', art TEXT NOT NULL DEFAULT '')")
            && exec("CREATE TABLE album_track_order (ref TEXT NOT NULL, pos INTEGER NOT NULL,"
                    " path TEXT NOT NULL, PRIMARY KEY (ref, pos))")
            && exec("PRAGMA user_version=3")
            && exec("COMMIT");
        if (!ok) { exec("ROLLBACK"); return false; }
        version = 3;
    }
    if (version < 4) {
        // v4: a manual order for the whole-library "songs" list, per profile.
        const bool ok = exec("BEGIN")
            && exec("CREATE TABLE song_order (pos INTEGER PRIMARY KEY, path TEXT NOT NULL)")
            && exec("PRAGMA user_version=4")
            && exec("COMMIT");
        if (!ok) { exec("ROLLBACK"); return false; }
        version = 4;
    }
    if (version < 5) {
        // v5: a picked thumbnail per video, per profile.
        const bool ok = exec("BEGIN")
            && exec("CREATE TABLE video_thumb_override (path TEXT PRIMARY KEY, thumb TEXT NOT NULL)")
            && exec("PRAGMA user_version=5")
            && exec("COMMIT");
        if (!ok) { exec("ROLLBACK"); return false; }
        version = 5;
    }
    if (version < 6) {
        // v6: finished audiobooks / podcast episodes (resume positions are forgotten at the end).
        const bool ok = exec("BEGIN")
            && exec("CREATE TABLE item_played (path TEXT PRIMARY KEY, played_at INTEGER NOT NULL DEFAULT 0)")
            && exec("PRAGMA user_version=6")
            && exec("COMMIT");
        if (!ok) { exec("ROLLBACK"); return false; }
        version = 6;
    }
    if (version < 7) {
        // v7: details the user fills in for a video (title, year, cast, ...), one JSON object per video.
        const bool ok = exec("BEGIN")
            && exec("CREATE TABLE video_meta (path TEXT PRIMARY KEY, json TEXT NOT NULL)")
            && exec("PRAGMA user_version=7")
            && exec("COMMIT");
        if (!ok) { exec("ROLLBACK"); return false; }
        version = 7;
    }
    if (version < 8) {
        // v8: the user's own comments under a video (everyone else's are generated, never stored).
        const bool ok = exec("BEGIN")
            && exec("CREATE TABLE video_comment (id INTEGER PRIMARY KEY, path TEXT NOT NULL, "
                    "text TEXT NOT NULL, at INTEGER NOT NULL DEFAULT 0)")
            && exec("CREATE INDEX video_comment_path ON video_comment(path)")
            && exec("PRAGMA user_version=8")
            && exec("COMMIT");
        if (!ok) { exec("ROLLBACK"); return false; }
        version = 8;
    }
    if (version < 9) {
        // v9: library sharing: the people allowed to connect, and what each may see (default: nothing).
        const bool ok = exec("BEGIN")
            && exec("CREATE TABLE share_guest (id INTEGER PRIMARY KEY, name TEXT NOT NULL, "
                    "secret TEXT NOT NULL UNIQUE, created INTEGER NOT NULL DEFAULT 0)")
            && exec("CREATE TABLE share_grant (guest_id INTEGER NOT NULL, path TEXT NOT NULL, "
                    "PRIMARY KEY (guest_id, path))")
            && exec("PRAGMA user_version=9")
            && exec("COMMIT");
        if (!ok) { exec("ROLLBACK"); return false; }
        version = 9;
    }
    if (version < 10) {
        // v10: libraries other people shared with this profile (it is their guest), with the key for each.
        const bool ok = exec("BEGIN")
            && exec("CREATE TABLE share_host (id INTEGER PRIMARY KEY, name TEXT NOT NULL, address TEXT NOT NULL, "
                    "port INTEGER NOT NULL, guest_id INTEGER NOT NULL, secret TEXT NOT NULL, added INTEGER NOT NULL DEFAULT 0)")
            && exec("PRAGMA user_version=10")
            && exec("COMMIT");
        if (!ok) { exec("ROLLBACK"); return false; }
    }
    return true;
}

// ---- stats + history ---------------------------------------------------------------

bool ProfileDb::recordPlay(const QString &path, qint64 when)
{
    if (!m_db) return false;
    Stmt s(m_db, "INSERT INTO stats(path, play_count, last_played) VALUES(?1, 1, ?2) "
                 "ON CONFLICT(path) DO UPDATE SET play_count = play_count + 1, last_played = ?2");
    s.bind(1, path); s.bind(2, when);
    Stmt h(m_db, "INSERT INTO history(path, played_at) VALUES(?1, ?2)");
    h.bind(1, path); h.bind(2, when);
    const bool ok = s.run() && h.run();
    // History is for "recently played"; keep it bounded.
    exec("DELETE FROM history WHERE id <= (SELECT MAX(id) - 5000 FROM history)");
    return ok;
}

bool ProfileDb::recordSkip(const QString &path)
{
    if (!m_db) return false;
    Stmt s(m_db, "INSERT INTO stats(path, skip_count) VALUES(?1, 1) "
                 "ON CONFLICT(path) DO UPDATE SET skip_count = skip_count + 1");
    s.bind(1, path);
    return s.run();
}

bool ProfileDb::setHearted(const QString &path, bool hearted)
{
    if (!m_db) return false;
    Stmt s(m_db, "INSERT INTO stats(path, hearted) VALUES(?1, ?2) ON CONFLICT(path) DO UPDATE SET hearted = ?2");
    s.bind(1, path); s.bind(2, hearted ? 1 : 0);
    return s.run();
}

QHash<QString, TrackStats> ProfileDb::allStats() const
{
    QHash<QString, TrackStats> out;
    if (!m_db) return out;
    Stmt s(m_db, "SELECT path, play_count, skip_count, last_played, hearted FROM stats");
    while (s.step() == SQLITE_ROW)
        out.insert(s.str(0), TrackStats{s.i32(1), s.i32(2), s.i64(3), s.i32(4) != 0});
    return out;
}

QList<HistoryEntry> ProfileDb::history(int limit) const
{
    QList<HistoryEntry> out;
    if (!m_db) return out;
    Stmt s(m_db, "SELECT path, played_at FROM history ORDER BY played_at DESC, id DESC LIMIT ?1");
    s.bind(1, limit);
    while (s.step() == SQLITE_ROW) out.append({s.str(0), s.i64(1)});
    return out;
}

// ---- playlists -------------------------------------------------------------------

QList<Playlist> ProfileDb::playlists() const
{
    QList<Playlist> out;
    if (!m_db) return out;
    Stmt s(m_db, "SELECT id, name, smart, rules, created FROM playlist ORDER BY name COLLATE NOCASE, id");
    while (s.step() == SQLITE_ROW) out.append({s.i64(0), s.str(1), s.i32(2) != 0, s.str(3), s.i64(4)});
    return out;
}

Playlist ProfileDb::playlist(qint64 id) const
{
    if (!m_db) return {};
    Stmt s(m_db, "SELECT id, name, smart, rules, created FROM playlist WHERE id = ?1");
    s.bind(1, id);
    return s.step() == SQLITE_ROW ? Playlist{s.i64(0), s.str(1), s.i32(2) != 0, s.str(3), s.i64(4)} : Playlist{};
}

qint64 ProfileDb::createPlaylist(const QString &name, bool smart, const QString &rules)
{
    if (!m_db || name.trimmed().isEmpty()) return 0;
    Stmt s(m_db, "INSERT INTO playlist(name, smart, rules, created) VALUES(?1, ?2, ?3, ?4)");
    s.bind(1, name.trimmed()); s.bind(2, smart ? 1 : 0); s.bind(3, rules);
    s.bind(4, QDateTime::currentSecsSinceEpoch());
    return s.run() ? sqlite3_last_insert_rowid(m_db) : 0;
}

bool ProfileDb::renamePlaylist(qint64 id, const QString &name)
{
    if (!m_db || name.trimmed().isEmpty()) return false;
    Stmt s(m_db, "UPDATE playlist SET name = ?2 WHERE id = ?1");
    s.bind(1, id); s.bind(2, name.trimmed());
    return s.run() && sqlite3_changes(m_db) == 1;
}

bool ProfileDb::setPlaylistRules(qint64 id, const QString &rules)
{
    if (!m_db) return false;
    Stmt s(m_db, "UPDATE playlist SET rules = ?2 WHERE id = ?1 AND smart = 1");
    s.bind(1, id); s.bind(2, rules);
    return s.run() && sqlite3_changes(m_db) == 1;
}

bool ProfileDb::deletePlaylist(qint64 id)
{
    if (!m_db) return false;
    Stmt s(m_db, "DELETE FROM playlist WHERE id = ?1");
    s.bind(1, id);
    if (!s.run() || sqlite3_changes(m_db) != 1) return false;
    unpin(QStringLiteral("playlist"), QString::number(id));
    return true;
}

QStringList ProfileDb::playlistItems(qint64 id) const
{
    QStringList out;
    if (!m_db) return out;
    Stmt s(m_db, "SELECT path FROM playlist_item WHERE playlist_id = ?1 ORDER BY pos");
    s.bind(1, id);
    while (s.step() == SQLITE_ROW) out << s.str(0);
    return out;
}

bool ProfileDb::setPlaylistItems(qint64 id, const QStringList &paths)
{
    if (!m_db || playlist(id).id == 0) return false;
    if (!exec("BEGIN")) return false;
    Stmt d(m_db, "DELETE FROM playlist_item WHERE playlist_id = ?1");
    d.bind(1, id);
    bool ok = d.run();
    Stmt ins(m_db, "INSERT INTO playlist_item(playlist_id, pos, path) VALUES(?1, ?2, ?3)");
    for (int i = 0; ok && i < paths.size(); ++i) {
        ins.bind(1, id); ins.bind(2, i); ins.bind(3, paths[i]);
        ok = ins.run();
        ins.reset();
    }
    if (!ok) { exec("ROLLBACK"); return false; }
    return exec("COMMIT");
}

bool ProfileDb::addToPlaylist(qint64 id, const QStringList &paths)
{
    return setPlaylistItems(id, playlistItems(id) + paths);
}

bool ProfileDb::removeFromPlaylist(qint64 id, int pos)
{
    QStringList items = playlistItems(id);
    if (pos < 0 || pos >= items.size()) return false;
    items.removeAt(pos);
    return setPlaylistItems(id, items);
}

bool ProfileDb::movePlaylistItem(qint64 id, int from, int to)
{
    QStringList items = playlistItems(id);
    if (from < 0 || from >= items.size() || to < 0 || to >= items.size()) return false;
    if (from == to) return true;
    items.move(from, to);
    return setPlaylistItems(id, items);
}

// ---- pins --------------------------------------------------------------------------

QList<Pin> ProfileDb::pins() const
{
    QList<Pin> out;
    if (!m_db) return out;
    Stmt s(m_db, "SELECT kind, ref, label FROM pin ORDER BY pos, rowid");
    while (s.step() == SQLITE_ROW) out.append({s.str(0), s.str(1), s.str(2)});
    return out;
}

bool ProfileDb::pin(const Pin &p)
{
    if (!m_db || p.kind.isEmpty() || p.ref.isEmpty()) return false;
    Stmt s(m_db, "INSERT INTO pin(kind, ref, label, pos) VALUES(?1, ?2, ?3, (SELECT COALESCE(MAX(pos), 0) + 1 FROM pin)) "
                 "ON CONFLICT(kind, ref) DO UPDATE SET label = ?3");
    s.bind(1, p.kind); s.bind(2, p.ref); s.bind(3, p.label);
    return s.run();
}

bool ProfileDb::unpin(const QString &kind, const QString &ref)
{
    if (!m_db) return false;
    Stmt s(m_db, "DELETE FROM pin WHERE kind = ?1 AND ref = ?2");
    s.bind(1, kind); s.bind(2, ref);
    return s.run();
}

bool ProfileDb::isPinned(const QString &kind, const QString &ref) const
{
    if (!m_db) return false;
    Stmt s(m_db, "SELECT 1 FROM pin WHERE kind = ?1 AND ref = ?2");
    s.bind(1, kind); s.bind(2, ref);
    return s.step() == SQLITE_ROW;
}

// ---- video resume position (spec §5b) -----------------------------------------------

VideoProgress ProfileDb::videoProgress(const QString &path) const
{
    VideoProgress p;
    if (!m_db) return p;
    Stmt s(m_db, "SELECT position_ms, duration_ms, updated_at FROM video_progress WHERE path = ?1");
    s.bind(1, path);
    if (s.step() == SQLITE_ROW) { p.positionMs = s.i64(0); p.durationMs = s.i64(1); p.updatedAt = s.i64(2); }
    return p;
}

bool ProfileDb::setVideoProgress(const QString &path, qint64 positionMs, qint64 durationMs)
{
    if (!m_db) return false;
    // Finished (or near enough): forget it, so it plays from the start next time rather
    // than appearing to resume one second from the end.
    if (durationMs > 0 && positionMs >= durationMs - 5000) {
        Stmt d(m_db, "DELETE FROM video_progress WHERE path = ?1");
        d.bind(1, path);
        return d.run();
    }
    Stmt s(m_db, "INSERT INTO video_progress(path, position_ms, duration_ms, updated_at) VALUES(?1, ?2, ?3, ?4) "
                 "ON CONFLICT(path) DO UPDATE SET position_ms = ?2, duration_ms = ?3, updated_at = ?4");
    s.bind(1, path); s.bind(2, positionMs); s.bind(3, durationMs); s.bind(4, QDateTime::currentSecsSinceEpoch());
    return s.run();
}

// ---- album overrides ----------------------------------------------------------------

QHash<QString, AlbumOverride> ProfileDb::albumOverrides() const
{
    QHash<QString, AlbumOverride> out;
    if (!m_db) return out;
    Stmt s(m_db, "SELECT ref, title, art FROM album_override");
    while (s.step() == SQLITE_ROW) out.insert(s.str(0), AlbumOverride{s.str(1), s.str(2)});
    return out;
}

bool ProfileDb::setAlbumOverrideTitle(const QString &ref, const QString &title)
{
    if (!m_db || ref.isEmpty()) return false;
    Stmt s(m_db, "INSERT INTO album_override(ref, title) VALUES(?1, ?2) "
                 "ON CONFLICT(ref) DO UPDATE SET title = ?2");
    s.bind(1, ref); s.bind(2, title.trimmed());
    return s.run();
}

bool ProfileDb::setAlbumOverrideArt(const QString &ref, const QString &artPath)
{
    if (!m_db || ref.isEmpty()) return false;
    Stmt s(m_db, "INSERT INTO album_override(ref, art) VALUES(?1, ?2) "
                 "ON CONFLICT(ref) DO UPDATE SET art = ?2");
    s.bind(1, ref); s.bind(2, artPath);
    return s.run();
}

QHash<QString, QString> ProfileDb::videoMetas() const
{
    QHash<QString, QString> out;
    if (!m_db) return out;
    Stmt s(m_db, "SELECT path, json FROM video_meta");
    while (s.step() == SQLITE_ROW) out.insert(s.str(0), s.str(1));
    return out;
}

bool ProfileDb::setVideoMeta(const QString &videoPath, const QString &json)
{
    if (!m_db || videoPath.isEmpty()) return false;
    if (json.isEmpty()) {
        Stmt d(m_db, "DELETE FROM video_meta WHERE path = ?1");
        d.bind(1, videoPath);
        return d.run();
    }
    Stmt s(m_db, "INSERT INTO video_meta(path, json) VALUES(?1, ?2) ON CONFLICT(path) DO UPDATE SET json = ?2");
    s.bind(1, videoPath); s.bind(2, json);
    return s.run();
}

// ---- library sharing ----------------------------------------------------------------

qint64 ProfileDb::addShareHost(const ShareHost &h)
{
    if (!m_db || h.address.isEmpty() || h.port == 0 || h.secret.size() != 64) return 0;
    Stmt s(m_db, "INSERT INTO share_host(name, address, port, guest_id, secret, added) VALUES(?1, ?2, ?3, ?4, ?5, ?6)");
    s.bind(1, h.name.left(60)); s.bind(2, h.address); s.bind(3, int(h.port)); s.bind(4, h.guestId); s.bind(5, h.secret); s.bind(6, h.added);
    return s.run() ? sqlite3_last_insert_rowid(m_db) : 0;
}

QList<ShareHost> ProfileDb::shareHosts() const
{
    QList<ShareHost> out;
    if (!m_db) return out;
    Stmt s(m_db, "SELECT id, name, address, port, guest_id, secret, added FROM share_host ORDER BY added, id");
    while (s.step() == SQLITE_ROW)
        out.append(ShareHost{s.i64(0), s.str(1), s.str(2), quint16(s.i32(3)), s.i64(4), s.str(5), s.i64(6)});
    return out;
}

bool ProfileDb::removeShareHost(qint64 id)
{
    if (!m_db) return false;
    Stmt s(m_db, "DELETE FROM share_host WHERE id = ?1");
    s.bind(1, id);
    return s.run();
}

bool ProfileDb::setShareHostAddress(qint64 id, const QString &address, quint16 port)
{
    if (!m_db || address.isEmpty() || port == 0) return false;
    Stmt s(m_db, "UPDATE share_host SET address = ?2, port = ?3 WHERE id = ?1");
    s.bind(1, id); s.bind(2, address); s.bind(3, int(port));
    return s.run();
}

qint64 ProfileDb::addShareGuest(const QString &name, const QString &secretHex, qint64 created)
{
    if (!m_db || secretHex.size() != 64) return 0;
    Stmt s(m_db, "INSERT INTO share_guest(name, secret, created) VALUES(?1, ?2, ?3)");
    s.bind(1, name.left(60)); s.bind(2, secretHex); s.bind(3, created);
    return s.run() ? sqlite3_last_insert_rowid(m_db) : 0;
}

QList<ShareGuest> ProfileDb::shareGuests() const
{
    QList<ShareGuest> out;
    if (!m_db) return out;
    Stmt s(m_db, "SELECT id, name, created FROM share_guest ORDER BY created, id");
    while (s.step() == SQLITE_ROW) out.append(ShareGuest{s.i64(0), s.str(1), s.i64(2)});
    return out;
}

bool ProfileDb::removeShareGuest(qint64 id)
{
    if (!m_db) return false;
    Stmt g(m_db, "DELETE FROM share_grant WHERE guest_id = ?1");
    g.bind(1, id);
    if (!g.run()) return false;
    Stmt s(m_db, "DELETE FROM share_guest WHERE id = ?1");
    s.bind(1, id);
    return s.run();
}

QByteArray ProfileDb::shareGuestSecret(qint64 id) const
{
    if (!m_db || id <= 0) return {};
    Stmt s(m_db, "SELECT secret FROM share_guest WHERE id = ?1");
    s.bind(1, id);
    return s.step() == SQLITE_ROW ? QByteArray::fromHex(s.str(0).toLatin1()) : QByteArray();
}

bool ProfileDb::setShareGrants(qint64 guestId, const QStringList &paths, bool granted)
{
    if (!m_db || guestId <= 0) return false;
    if (paths.isEmpty()) return true;
    if (!exec("BEGIN")) return false;
    Stmt s(m_db, granted ? "INSERT OR IGNORE INTO share_grant(guest_id, path) VALUES(?1, ?2)"
                         : "DELETE FROM share_grant WHERE guest_id = ?1 AND path = ?2");
    for (const QString &p : paths) {
        s.bind(1, guestId); s.bind(2, p);
        if (s.step() != SQLITE_DONE) { exec("ROLLBACK"); return false; }
        s.reset();
    }
    return exec("COMMIT");
}

bool ProfileDb::isShared(qint64 guestId, const QString &path) const
{
    if (!m_db || guestId <= 0) return false;
    Stmt s(m_db, "SELECT 1 FROM share_grant WHERE guest_id = ?1 AND path = ?2");
    s.bind(1, guestId); s.bind(2, path);
    return s.step() == SQLITE_ROW;
}

QSet<QString> ProfileDb::shareGrants(qint64 guestId) const
{
    QSet<QString> out;
    if (!m_db || guestId <= 0) return out;
    Stmt s(m_db, "SELECT path FROM share_grant WHERE guest_id = ?1");
    s.bind(1, guestId);
    while (s.step() == SQLITE_ROW) out.insert(s.str(0));
    return out;
}

QList<VideoComment> ProfileDb::videoComments(const QString &videoPath) const
{
    QList<VideoComment> out;
    if (!m_db) return out;
    Stmt s(m_db, "SELECT id, text, at FROM video_comment WHERE path = ?1 ORDER BY at DESC, id DESC");
    s.bind(1, videoPath);
    while (s.step() == SQLITE_ROW) out.append(VideoComment{s.i64(0), s.str(1), s.i64(2)});
    return out;
}

qint64 ProfileDb::addVideoComment(const QString &videoPath, const QString &text, qint64 at)
{
    if (!m_db || videoPath.isEmpty() || text.isEmpty()) return 0;
    Stmt s(m_db, "INSERT INTO video_comment(path, text, at) VALUES(?1, ?2, ?3)");
    s.bind(1, videoPath); s.bind(2, text); s.bind(3, at);
    return s.run() ? sqlite3_last_insert_rowid(m_db) : 0;
}

bool ProfileDb::removeVideoComment(qint64 id)
{
    if (!m_db) return false;
    Stmt s(m_db, "DELETE FROM video_comment WHERE id = ?1");
    s.bind(1, id);
    return s.run();
}

QSet<QString> ProfileDb::playedItems() const
{
    QSet<QString> out;
    if (!m_db) return out;
    Stmt s(m_db, "SELECT path FROM item_played");
    while (s.step() == SQLITE_ROW) out.insert(s.str(0));
    return out;
}

bool ProfileDb::setItemPlayed(const QString &path, bool played)
{
    if (!m_db || path.isEmpty()) return false;
    if (!played) {
        Stmt d(m_db, "DELETE FROM item_played WHERE path = ?1");
        d.bind(1, path);
        return d.run();
    }
    Stmt s(m_db, "INSERT INTO item_played(path, played_at) VALUES(?1, ?2) ON CONFLICT(path) DO NOTHING");
    s.bind(1, path); s.bind(2, qint64(QDateTime::currentSecsSinceEpoch()));
    return s.run();
}

QHash<QString, QString> ProfileDb::videoThumbOverrides() const
{
    QHash<QString, QString> out;
    if (!m_db) return out;
    Stmt s(m_db, "SELECT path, thumb FROM video_thumb_override");
    while (s.step() == SQLITE_ROW) out.insert(s.str(0), s.str(1));
    return out;
}

bool ProfileDb::setVideoThumbOverride(const QString &videoPath, const QString &thumbPath)
{
    if (!m_db || videoPath.isEmpty()) return false;
    if (thumbPath.isEmpty()) {
        Stmt d(m_db, "DELETE FROM video_thumb_override WHERE path = ?1");
        d.bind(1, videoPath);
        return d.run();
    }
    Stmt s(m_db, "INSERT INTO video_thumb_override(path, thumb) VALUES(?1, ?2) "
                 "ON CONFLICT(path) DO UPDATE SET thumb = ?2");
    s.bind(1, videoPath); s.bind(2, thumbPath);
    return s.run();
}

QStringList ProfileDb::albumTrackOrder(const QString &ref) const
{
    QStringList out;
    if (!m_db) return out;
    Stmt s(m_db, "SELECT path FROM album_track_order WHERE ref = ?1 ORDER BY pos");
    s.bind(1, ref);
    while (s.step() == SQLITE_ROW) out << s.str(0);
    return out;
}

bool ProfileDb::setAlbumTrackOrder(const QString &ref, const QStringList &paths)
{
    if (!m_db || ref.isEmpty()) return false;
    if (!exec("BEGIN")) return false;
    Stmt d(m_db, "DELETE FROM album_track_order WHERE ref = ?1");
    d.bind(1, ref);
    bool ok = d.run();
    Stmt ins(m_db, "INSERT INTO album_track_order(ref, pos, path) VALUES(?1, ?2, ?3)");
    for (int i = 0; ok && i < paths.size(); ++i) {
        ins.bind(1, ref); ins.bind(2, i); ins.bind(3, paths[i]);
        ok = ins.run();
        ins.reset();
    }
    if (!ok) { exec("ROLLBACK"); return false; }
    return exec("COMMIT");
}

// ---- whole-library song order ---------------------------------------------------------

QStringList ProfileDb::songOrder() const
{
    QStringList out;
    if (!m_db) return out;
    Stmt s(m_db, "SELECT path FROM song_order ORDER BY pos");
    while (s.step() == SQLITE_ROW) out << s.str(0);
    return out;
}

bool ProfileDb::setSongOrder(const QStringList &paths)
{
    if (!m_db) return false;
    if (!exec("BEGIN")) return false;
    bool ok = exec("DELETE FROM song_order");
    Stmt ins(m_db, "INSERT INTO song_order(pos, path) VALUES(?1, ?2)");
    for (int i = 0; ok && i < paths.size(); ++i) {
        ins.bind(1, i); ins.bind(2, paths[i]);
        ok = ins.run();
        ins.reset();
    }
    if (!ok) { exec("ROLLBACK"); return false; }
    return exec("COMMIT");
}

} // namespace hop
