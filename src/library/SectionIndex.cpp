// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/SectionIndex.h"

#include "core/Sql.h"

#include <QDateTime>
#include <QMutexLocker>

#include <sqlite3.h>

namespace hop {

namespace {

using hop::sql::Stmt;

constexpr const char *kColumns = "id, path, mtime, size, title, grp, duration_ms, width, height, added";

SectionItem readItem(const Stmt &s)
{
    SectionItem it;
    it.id = s.i64(0);        it.path = s.str(1);      it.mtime = s.i64(2);
    it.size = s.i64(3);      it.title = s.str(4);     it.group = s.str(5);
    it.durationMs = s.i64(6); it.width = s.i32(7);    it.height = s.i32(8);
    it.added = s.i64(9);
    return it;
}

QString lowerBound(const QString &folder) { return folder + QLatin1Char('/'); }
QString upperBound(const QString &folder) { return folder + QLatin1Char('0'); }

QString normFolder(QString f)
{
    while (f.size() > 1 && f.endsWith(QLatin1Char('/'))) f.chop(1);
    return f;
}

} // namespace

SectionIndex::SectionIndex() = default;
SectionIndex::~SectionIndex() { close(); }

bool SectionIndex::open(const QString &dbPath)
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

void SectionIndex::close()
{
    QMutexLocker lock(&m_mutex);
    if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
}

bool SectionIndex::isOpen() const { QMutexLocker lock(&m_mutex); return m_db != nullptr; }
QString SectionIndex::lastError() const { QMutexLocker lock(&m_mutex); return m_error; }

bool SectionIndex::exec(const char *sql)
{
    char *err = nullptr;
    if (sqlite3_exec(m_db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        m_error = QString::fromUtf8(err ? err : "sqlite error");
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool SectionIndex::migrate()
{
    Stmt v(m_db, "PRAGMA user_version");
    const int version = (v.ok() && v.step() == SQLITE_ROW) ? v.i32(0) : 0;
    if (version < 1) {
        const bool ok = exec("BEGIN")
            && exec("CREATE TABLE IF NOT EXISTS item ("
                    "  id INTEGER PRIMARY KEY,"
                    "  kind TEXT NOT NULL,"
                    "  path TEXT NOT NULL,"
                    "  mtime INTEGER NOT NULL DEFAULT 0,"
                    "  size INTEGER NOT NULL DEFAULT 0,"
                    "  title TEXT NOT NULL DEFAULT '',"
                    "  grp TEXT NOT NULL DEFAULT '',"
                    "  duration_ms INTEGER NOT NULL DEFAULT 0,"
                    "  width INTEGER NOT NULL DEFAULT 0,"
                    "  height INTEGER NOT NULL DEFAULT 0,"
                    "  added INTEGER NOT NULL DEFAULT 0,"
                    "  UNIQUE(kind, path))")
            && exec("PRAGMA user_version=1")
            && exec("COMMIT");
        if (!ok) { exec("ROLLBACK"); return false; }
    }
    return true;
}

QHash<QString, SectionIndex::FileStamp> SectionIndex::stampsUnder(SectionKind kind, const QString &folder) const
{
    QMutexLocker lock(&m_mutex);
    QHash<QString, FileStamp> out;
    if (!m_db) return out;
    const QString f = normFolder(folder);
    Stmt s(m_db, "SELECT path, id, mtime, size FROM item WHERE kind = ?1 AND path >= ?2 AND path < ?3");
    s.bind(1, sectionKey(kind));
    s.bind(2, lowerBound(f));
    s.bind(3, upperBound(f));
    while (s.step() == SQLITE_ROW)
        out.insert(s.str(0), FileStamp{s.i64(1), s.i64(2), s.i64(3)});
    return out;
}

bool SectionIndex::upsert(SectionKind kind, const QList<SectionItem> &items)
{
    QMutexLocker lock(&m_mutex);
    if (!m_db) return false;
    if (items.isEmpty()) return true;
    if (!exec("BEGIN IMMEDIATE")) return false;
    Stmt s(m_db,
           "INSERT INTO item (kind, path, mtime, size, title, grp, duration_ms, width, height, added) "
           "VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10) "
           "ON CONFLICT(kind, path) DO UPDATE SET mtime=excluded.mtime, size=excluded.size, "
           "  title=excluded.title, grp=excluded.grp, duration_ms=excluded.duration_ms, "
           "  width=excluded.width, height=excluded.height");
    if (!s.ok()) { m_error = QString::fromUtf8(sqlite3_errmsg(m_db)); exec("ROLLBACK"); return false; }
    const QString key = sectionKey(kind);
    for (const SectionItem &it : items) {
        s.bind(1, key);          s.bind(2, it.path);        s.bind(3, it.mtime);
        s.bind(4, it.size);      s.bind(5, it.title);       s.bind(6, it.group);
        s.bind(7, it.durationMs); s.bind(8, it.width);      s.bind(9, it.height);
        s.bind(10, it.added > 0 ? it.added : QDateTime::currentSecsSinceEpoch());
        if (s.step() != SQLITE_DONE) {
            m_error = QString::fromUtf8(sqlite3_errmsg(m_db));
            exec("ROLLBACK");
            return false;
        }
        s.reset();
    }
    return exec("COMMIT");
}

bool SectionIndex::removePaths(SectionKind kind, const QStringList &paths)
{
    QMutexLocker lock(&m_mutex);
    if (!m_db) return false;
    if (paths.isEmpty()) return true;
    if (!exec("BEGIN IMMEDIATE")) return false;
    Stmt s(m_db, "DELETE FROM item WHERE kind = ?1 AND path = ?2");
    const QString key = sectionKey(kind);
    for (const QString &p : paths) {
        s.bind(1, key);
        s.bind(2, p);
        if (s.step() != SQLITE_DONE) { exec("ROLLBACK"); return false; }
        s.reset();
    }
    return exec("COMMIT");
}

QList<SectionItem> SectionIndex::itemsUnder(SectionKind kind, const QStringList &folders) const
{
    QMutexLocker lock(&m_mutex);
    QList<SectionItem> out;
    if (!m_db || folders.isEmpty()) return out;

    QString sql = QStringLiteral("SELECT %1 FROM item WHERE kind = ?1 AND (").arg(QLatin1String(kColumns));
    for (int i = 0; i < folders.size(); ++i) {
        if (i) sql += QStringLiteral(" OR ");
        sql += QStringLiteral("(path >= ?%1 AND path < ?%2)").arg(2 * i + 2).arg(2 * i + 3);
    }
    sql += QStringLiteral(") ORDER BY grp COLLATE NOCASE, title COLLATE NOCASE, path");
    const QByteArray u = sql.toUtf8();
    Stmt s(m_db, u.constData());
    if (!s.ok()) { m_error = QString::fromUtf8(sqlite3_errmsg(m_db)); return out; }
    s.bind(1, sectionKey(kind));
    for (int i = 0; i < folders.size(); ++i) {
        const QString f = normFolder(folders[i]);
        s.bind(2 * i + 2, lowerBound(f));
        s.bind(2 * i + 3, upperBound(f));
    }
    while (s.step() == SQLITE_ROW) out.append(readItem(s));
    return out;
}

int SectionIndex::count(SectionKind kind) const
{
    QMutexLocker lock(&m_mutex);
    if (!m_db) return 0;
    Stmt s(m_db, "SELECT COUNT(*) FROM item WHERE kind = ?1");
    s.bind(1, sectionKey(kind));
    return s.step() == SQLITE_ROW ? s.i32(0) : 0;
}

} // namespace hop
