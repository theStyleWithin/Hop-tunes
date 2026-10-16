// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/VideoIndex.h"

#include "core/Sql.h"

#include <QDateTime>
#include <QFileInfo>
#include <QMutexLocker>

#include <sqlite3.h>

namespace hop {

QString Video::displayTitle() const { return QFileInfo(path).completeBaseName(); }

namespace {

using hop::sql::Stmt;

constexpr const char *kVideoColumns =
    "id, path, mtime, size, duration_ms, codec, container, width, height, hdr, frame_rate, thumb, added";

Video readVideo(const Stmt &s)
{
    Video v;
    v.id = s.i64(0);         v.path = s.str(1);        v.mtime = s.i64(2);
    v.size = s.i64(3);       v.durationMs = s.i64(4);  v.codec = s.str(5);
    v.container = s.str(6);  v.width = s.i32(7);       v.height = s.i32(8);
    v.hdr = s.i32(9) != 0;   v.frameRate = s.f64(10);  v.thumb = s.str(11);
    v.added = s.i64(12);
    return v;
}

QString lowerBound(const QString &folder) { return folder + QLatin1Char('/'); }
QString upperBound(const QString &folder) { return folder + QLatin1Char('0'); }

QString normFolder(QString f)
{
    while (f.size() > 1 && f.endsWith(QLatin1Char('/'))) f.chop(1);
    return f;
}

} // namespace

VideoIndex::VideoIndex() = default;
VideoIndex::~VideoIndex() { close(); }

bool VideoIndex::open(const QString &dbPath)
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
    exec("PRAGMA foreign_keys=ON");
    return migrate();
}

void VideoIndex::close()
{
    QMutexLocker lock(&m_mutex);
    if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
}

bool VideoIndex::isOpen() const { QMutexLocker lock(&m_mutex); return m_db != nullptr; }
QString VideoIndex::lastError() const { QMutexLocker lock(&m_mutex); return m_error; }

bool VideoIndex::exec(const char *sql)
{
    char *err = nullptr;
    if (sqlite3_exec(m_db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        m_error = QString::fromUtf8(err ? err : "sqlite error");
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool VideoIndex::migrate()
{
    Stmt v(m_db, "PRAGMA user_version");
    const int version = (v.ok() && v.step() == SQLITE_ROW) ? v.i32(0) : 0;
    if (version < 1) {
        const bool ok = exec("BEGIN")
            && exec("CREATE TABLE IF NOT EXISTS video ("
                    "  id INTEGER PRIMARY KEY,"
                    "  path TEXT NOT NULL UNIQUE,"
                    "  mtime INTEGER NOT NULL DEFAULT 0,"
                    "  size INTEGER NOT NULL DEFAULT 0,"
                    "  duration_ms INTEGER NOT NULL DEFAULT 0,"
                    "  codec TEXT NOT NULL DEFAULT '',"
                    "  container TEXT NOT NULL DEFAULT '',"
                    "  width INTEGER NOT NULL DEFAULT 0,"
                    "  height INTEGER NOT NULL DEFAULT 0,"
                    "  hdr INTEGER NOT NULL DEFAULT 0,"
                    "  frame_rate REAL NOT NULL DEFAULT 0,"
                    "  thumb TEXT NOT NULL DEFAULT '',"
                    "  added INTEGER NOT NULL DEFAULT 0)")
            && exec("PRAGMA user_version=1")
            && exec("COMMIT");
        if (!ok) { exec("ROLLBACK"); return false; }
    }
    return true;
}

QHash<QString, VideoIndex::FileStamp> VideoIndex::stampsUnder(const QString &folder) const
{
    QMutexLocker lock(&m_mutex);
    QHash<QString, FileStamp> out;
    if (!m_db) return out;
    const QString f = normFolder(folder);
    Stmt s(m_db, "SELECT path, id, mtime, size FROM video WHERE path >= ?1 AND path < ?2");
    s.bind(1, lowerBound(f));
    s.bind(2, upperBound(f));
    while (s.step() == SQLITE_ROW)
        out.insert(s.str(0), FileStamp{s.i64(1), s.i64(2), s.i64(3)});
    return out;
}

bool VideoIndex::upsert(const QList<Video> &videos)
{
    QMutexLocker lock(&m_mutex);
    if (!m_db) return false;
    if (videos.isEmpty()) return true;
    if (!exec("BEGIN IMMEDIATE")) return false;
    Stmt s(m_db,
           "INSERT INTO video (path, mtime, size, duration_ms, codec, container, width, height, "
           "  hdr, frame_rate, thumb, added) VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12) "
           "ON CONFLICT(path) DO UPDATE SET mtime=excluded.mtime, size=excluded.size, "
           "  duration_ms=excluded.duration_ms, codec=excluded.codec, container=excluded.container, "
           "  width=excluded.width, height=excluded.height, hdr=excluded.hdr, "
           "  frame_rate=excluded.frame_rate, thumb=excluded.thumb");
    if (!s.ok()) { m_error = QString::fromUtf8(sqlite3_errmsg(m_db)); exec("ROLLBACK"); return false; }
    for (const Video &vid : videos) {
        s.bind(1, vid.path);     s.bind(2, vid.mtime);    s.bind(3, vid.size);
        s.bind(4, vid.durationMs); s.bind(5, vid.codec);  s.bind(6, vid.container);
        s.bind(7, vid.width);    s.bind(8, vid.height);   s.bind(9, vid.hdr ? 1 : 0);
        s.bind(10, vid.frameRate); s.bind(11, vid.thumb);
        s.bind(12, vid.added > 0 ? vid.added : QDateTime::currentSecsSinceEpoch());
        if (s.step() != SQLITE_DONE) {
            m_error = QString::fromUtf8(sqlite3_errmsg(m_db));
            exec("ROLLBACK");
            return false;
        }
        s.reset();
    }
    return exec("COMMIT");
}

bool VideoIndex::removePaths(const QStringList &paths)
{
    QMutexLocker lock(&m_mutex);
    if (!m_db) return false;
    if (paths.isEmpty()) return true;
    if (!exec("BEGIN IMMEDIATE")) return false;
    Stmt s(m_db, "DELETE FROM video WHERE path = ?1");
    for (const QString &p : paths) {
        s.bind(1, p);
        if (s.step() != SQLITE_DONE) { exec("ROLLBACK"); return false; }
        s.reset();
    }
    return exec("COMMIT");
}

QList<Video> VideoIndex::videosUnder(const QStringList &folders) const
{
    QMutexLocker lock(&m_mutex);
    QList<Video> out;
    if (!m_db || folders.isEmpty()) return out;

    QString sql = QStringLiteral("SELECT %1 FROM video WHERE ").arg(QLatin1String(kVideoColumns));
    for (int i = 0; i < folders.size(); ++i) {
        if (i) sql += QStringLiteral(" OR ");
        sql += QStringLiteral("(path >= ?%1 AND path < ?%2)").arg(2 * i + 1).arg(2 * i + 2);
    }
    sql += QStringLiteral(" ORDER BY added DESC, path");
    const QByteArray u = sql.toUtf8();
    Stmt s(m_db, u.constData());
    if (!s.ok()) { m_error = QString::fromUtf8(sqlite3_errmsg(m_db)); return out; }
    for (int i = 0; i < folders.size(); ++i) {
        const QString f = normFolder(folders[i]);
        s.bind(2 * i + 1, lowerBound(f));
        s.bind(2 * i + 2, upperBound(f));
    }
    while (s.step() == SQLITE_ROW) out.append(readVideo(s));
    return out;
}

Video VideoIndex::videoById(qint64 id) const
{
    QMutexLocker lock(&m_mutex);
    if (!m_db) return {};
    const QByteArray sql = QStringLiteral("SELECT %1 FROM video WHERE id = ?1")
                               .arg(QLatin1String(kVideoColumns)).toUtf8();
    Stmt s(m_db, sql.constData());
    s.bind(1, id);
    return s.step() == SQLITE_ROW ? readVideo(s) : Video{};
}

int VideoIndex::count() const
{
    QMutexLocker lock(&m_mutex);
    if (!m_db) return 0;
    Stmt s(m_db, "SELECT COUNT(*) FROM video");
    return s.step() == SQLITE_ROW ? s.i32(0) : 0;
}

} // namespace hop
