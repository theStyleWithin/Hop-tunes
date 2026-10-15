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
    const int version = (v.ok() && v.step() == SQLITE_ROW) ? v.i32(0) : 0;
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

} // namespace hop
