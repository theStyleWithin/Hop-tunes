// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/GameIndex.h"

#include "core/Sql.h"

#include <QDateTime>
#include <QMutexLocker>

#include <sqlite3.h>

namespace hop {

namespace {

using hop::sql::Stmt;

constexpr const char *kColumns = "id, path, mtime, size, title, system, source, app_id, cover, added";

Game readGame(const Stmt &s)
{
    Game g;
    g.id = s.i64(0);      g.path = s.str(1);     g.mtime = s.i64(2);
    g.size = s.i64(3);    g.title = s.str(4);    g.system = s.str(5);
    g.source = s.str(6);  g.appId = s.str(7);    g.cover = s.str(8);
    g.added = s.i64(9);
    return g;
}

QString lowerBound(const QString &folder) { return folder + QLatin1Char('/'); }
QString upperBound(const QString &folder) { return folder + QLatin1Char('0'); }

QString normFolder(QString f)
{
    while (f.size() > 1 && f.endsWith(QLatin1Char('/'))) f.chop(1);
    return f;
}

} // namespace

GameIndex::GameIndex() = default;
GameIndex::~GameIndex() { close(); }

bool GameIndex::open(const QString &dbPath)
{
    QMutexLocker lock(&m_mutex);
    if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
    const QByteArray p = dbPath.toUtf8();
    if (sqlite3_open_v2(p.constData(), &m_db,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                        nullptr) != SQLITE_OK) {
        m_error = QString::fromUtf8(sqlite3_errmsg(m_db));
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }
    sqlite3_busy_timeout(m_db, 3000);
    exec("PRAGMA journal_mode=WAL");
    exec("PRAGMA synchronous=NORMAL");
    return migrate();
}

void GameIndex::close()
{
    QMutexLocker lock(&m_mutex);
    if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
}

bool GameIndex::isOpen() const { QMutexLocker lock(&m_mutex); return m_db != nullptr; }
QString GameIndex::lastError() const { QMutexLocker lock(&m_mutex); return m_error; }

bool GameIndex::exec(const char *sql)
{
    char *err = nullptr;
    if (sqlite3_exec(m_db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        m_error = QString::fromUtf8(err ? err : "sqlite error");
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool GameIndex::migrate()
{
    Stmt v(m_db, "PRAGMA user_version");
    const int version = (v.ok() && v.step() == SQLITE_ROW) ? v.i32(0) : 0;
    if (version < 1) {
        const bool ok = exec("BEGIN")
            && exec("CREATE TABLE IF NOT EXISTS game ("
                    "  id INTEGER PRIMARY KEY,"
                    "  path TEXT NOT NULL UNIQUE,"
                    "  mtime INTEGER NOT NULL DEFAULT 0,"
                    "  size INTEGER NOT NULL DEFAULT 0,"
                    "  title TEXT NOT NULL DEFAULT '',"
                    "  system TEXT NOT NULL DEFAULT '',"
                    "  source TEXT NOT NULL DEFAULT '',"
                    "  app_id TEXT NOT NULL DEFAULT '',"
                    "  cover TEXT NOT NULL DEFAULT '',"
                    "  added INTEGER NOT NULL DEFAULT 0)")
            && exec("PRAGMA user_version=1")
            && exec("COMMIT");
        if (!ok) { exec("ROLLBACK"); return false; }
    }
    return true;
}

QHash<QString, GameIndex::Stamp> GameIndex::stampsUnder(const QString &folder) const
{
    QMutexLocker lock(&m_mutex);
    QHash<QString, Stamp> out;
    if (!m_db) return out;
    const QString f = normFolder(folder);
    Stmt s(m_db, "SELECT path, id, mtime, size FROM game WHERE path >= ?1 AND path < ?2");
    s.bind(1, lowerBound(f));
    s.bind(2, upperBound(f));
    while (s.step() == SQLITE_ROW)
        out.insert(s.str(0), Stamp{s.i64(1), s.i64(2), s.i64(3)});
    return out;
}

bool GameIndex::upsert(const QList<Game> &games)
{
    QMutexLocker lock(&m_mutex);
    if (!m_db) return false;
    if (games.isEmpty()) return true;
    if (!exec("BEGIN IMMEDIATE")) return false;
    Stmt s(m_db,
           "INSERT INTO game (path, mtime, size, title, system, source, app_id, cover, added) "
           "VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9) "
           "ON CONFLICT(path) DO UPDATE SET mtime=excluded.mtime, size=excluded.size, title=excluded.title, "
           "  system=excluded.system, source=excluded.source, app_id=excluded.app_id, cover=excluded.cover");
    if (!s.ok()) { m_error = QString::fromUtf8(sqlite3_errmsg(m_db)); exec("ROLLBACK"); return false; }
    for (const Game &g : games) {
        s.bind(1, g.path);     s.bind(2, g.mtime);    s.bind(3, g.size);
        s.bind(4, g.title);    s.bind(5, g.system);   s.bind(6, g.source);
        s.bind(7, g.appId);    s.bind(8, g.cover);
        s.bind(9, g.added > 0 ? g.added : QDateTime::currentSecsSinceEpoch());
        if (s.step() != SQLITE_DONE) {
            m_error = QString::fromUtf8(sqlite3_errmsg(m_db));
            exec("ROLLBACK");
            return false;
        }
        s.reset();
    }
    return exec("COMMIT");
}

bool GameIndex::removePaths(const QStringList &paths)
{
    QMutexLocker lock(&m_mutex);
    if (!m_db) return false;
    if (paths.isEmpty()) return true;
    if (!exec("BEGIN IMMEDIATE")) return false;
    Stmt s(m_db, "DELETE FROM game WHERE path = ?1");
    for (const QString &p : paths) {
        s.bind(1, p);
        if (s.step() != SQLITE_DONE) { exec("ROLLBACK"); return false; }
        s.reset();
    }
    return exec("COMMIT");
}

QList<Game> GameIndex::gamesUnder(const QStringList &folders) const
{
    QMutexLocker lock(&m_mutex);
    QList<Game> out;
    if (!m_db || folders.isEmpty()) return out;

    QString sql = QStringLiteral("SELECT %1 FROM game WHERE ").arg(QLatin1String(kColumns));
    for (int i = 0; i < folders.size(); ++i) {
        if (i) sql += QStringLiteral(" OR ");
        sql += QStringLiteral("(path >= ?%1 AND path < ?%2)").arg(2 * i + 1).arg(2 * i + 2);
    }
    sql += QStringLiteral(" ORDER BY title COLLATE NOCASE, path");
    const QByteArray u = sql.toUtf8();
    Stmt s(m_db, u.constData());
    if (!s.ok()) { m_error = QString::fromUtf8(sqlite3_errmsg(m_db)); return out; }
    for (int i = 0; i < folders.size(); ++i) {
        const QString f = normFolder(folders[i]);
        s.bind(2 * i + 1, lowerBound(f));
        s.bind(2 * i + 2, upperBound(f));
    }
    while (s.step() == SQLITE_ROW) out.append(readGame(s));
    return out;
}

int GameIndex::count() const
{
    QMutexLocker lock(&m_mutex);
    if (!m_db) return 0;
    Stmt s(m_db, "SELECT COUNT(*) FROM game");
    return s.step() == SQLITE_ROW ? s.i32(0) : 0;
}

} // namespace hop
