// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/TrackIndex.h"

#include <QFileInfo>
#include <QMutexLocker>

#include <sqlite3.h>

namespace hop {

// ---- Track display helpers ----------------------------------------------------

QString Track::displayTitle() const
{
    return title.isEmpty() ? QFileInfo(path).completeBaseName() : title;
}

QString Track::displayArtist() const
{
    if (!artist.isEmpty()) return artist;
    if (!albumArtist.isEmpty()) return albumArtist;
    return QStringLiteral("unknown artist");
}

QString Track::displayAlbum() const
{
    return album.isEmpty() ? QStringLiteral("unknown album") : album;
}

// ---- small RAII statement wrapper ---------------------------------------------

namespace {

class Stmt {
public:
    Stmt(sqlite3 *db, const char *sql) { sqlite3_prepare_v2(db, sql, -1, &m_s, nullptr); }
    ~Stmt() { sqlite3_finalize(m_s); }
    Stmt(const Stmt &) = delete;
    Stmt &operator=(const Stmt &) = delete;

    bool ok() const { return m_s != nullptr; }
    void bind(int i, const QString &v)
    {
        const QByteArray u = v.toUtf8();
        sqlite3_bind_text(m_s, i, u.constData(), u.size(), SQLITE_TRANSIENT);
    }
    void bind(int i, qint64 v) { sqlite3_bind_int64(m_s, i, v); }
    void bind(int i, int v) { sqlite3_bind_int(m_s, i, v); }
    int step() { return sqlite3_step(m_s); }
    void reset() { sqlite3_reset(m_s); sqlite3_clear_bindings(m_s); }

    qint64 i64(int c) const { return sqlite3_column_int64(m_s, c); }
    int i32(int c) const { return sqlite3_column_int(m_s, c); }
    QString str(int c) const
    {
        const auto *t = reinterpret_cast<const char *>(sqlite3_column_text(m_s, c));
        return t ? QString::fromUtf8(t, sqlite3_column_bytes(m_s, c)) : QString();
    }

private:
    sqlite3_stmt *m_s = nullptr;
};

constexpr const char *kTrackColumns =
    "id, path, mtime, size, duration_ms, codec, bitrate, sample_rate, title, artist, "
    "album_artist, album, genre, disc, track_no, year, art";

Track readTrack(const Stmt &s)
{
    Track t;
    t.id = s.i64(0);          t.path = s.str(1);        t.mtime = s.i64(2);
    t.size = s.i64(3);        t.durationMs = s.i64(4);  t.codec = s.str(5);
    t.bitrate = s.i32(6);     t.sampleRate = s.i32(7);  t.title = s.str(8);
    t.artist = s.str(9);      t.albumArtist = s.str(10); t.album = s.str(11);
    t.genre = s.str(12);      t.disc = s.i32(13);       t.trackNo = s.i32(14);
    t.year = s.i32(15);       t.art = s.str(16);
    return t;
}

// "path is under folder" as an index-friendly range: '/' + 1 == '0'.
QString lowerBound(const QString &folder) { return folder + QLatin1Char('/'); }
QString upperBound(const QString &folder) { return folder + QLatin1Char('0'); }

QString normFolder(QString f)
{
    while (f.size() > 1 && f.endsWith(QLatin1Char('/'))) f.chop(1);
    return f;
}

} // namespace

// ---- TrackIndex ----------------------------------------------------------------

TrackIndex::TrackIndex() = default;
TrackIndex::~TrackIndex() { close(); }

bool TrackIndex::open(const QString &dbPath)
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

void TrackIndex::close()
{
    QMutexLocker lock(&m_mutex);
    if (m_db) { sqlite3_close(m_db); m_db = nullptr; }
}

bool TrackIndex::isOpen() const { QMutexLocker lock(&m_mutex); return m_db != nullptr; }
QString TrackIndex::lastError() const { QMutexLocker lock(&m_mutex); return m_error; }

bool TrackIndex::exec(const char *sql)
{
    char *err = nullptr;
    if (sqlite3_exec(m_db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        m_error = QString::fromUtf8(err ? err : "sqlite error");
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool TrackIndex::migrate()
{
    // user_version tracks the schema; bump and add a step for each change.
    Stmt v(m_db, "PRAGMA user_version");
    int version = (v.ok() && v.step() == SQLITE_ROW) ? v.i32(0) : 0;

    if (version < 1) {
        const bool ok = exec("BEGIN")
            && exec("CREATE TABLE IF NOT EXISTS track ("
                    "  id INTEGER PRIMARY KEY,"
                    "  path TEXT NOT NULL UNIQUE,"
                    "  mtime INTEGER NOT NULL DEFAULT 0,"
                    "  size INTEGER NOT NULL DEFAULT 0,"
                    "  duration_ms INTEGER NOT NULL DEFAULT 0,"
                    "  codec TEXT NOT NULL DEFAULT '',"
                    "  bitrate INTEGER NOT NULL DEFAULT 0,"
                    "  sample_rate INTEGER NOT NULL DEFAULT 0,"
                    "  title TEXT NOT NULL DEFAULT '',"
                    "  artist TEXT NOT NULL DEFAULT '',"
                    "  album_artist TEXT NOT NULL DEFAULT '',"
                    "  album TEXT NOT NULL DEFAULT '',"
                    "  genre TEXT NOT NULL DEFAULT '',"
                    "  disc INTEGER NOT NULL DEFAULT 0,"
                    "  track_no INTEGER NOT NULL DEFAULT 0,"
                    "  year INTEGER NOT NULL DEFAULT 0)")
            && exec("PRAGMA user_version=1")
            && exec("COMMIT");
        if (!ok) { exec("ROLLBACK"); return false; }
        version = 1;
    }
    if (version < 2) {
        // v2: cover art. Zeroing mtime makes the next scan re-read every file once to find art.
        const bool ok = exec("BEGIN")
            && exec("ALTER TABLE track ADD COLUMN art TEXT NOT NULL DEFAULT ''")
            && exec("UPDATE track SET mtime = 0")
            && exec("PRAGMA user_version=2")
            && exec("COMMIT");
        if (!ok) { exec("ROLLBACK"); return false; }
        version = 2;
    }
    return true;
}

QHash<QString, TrackIndex::FileStamp> TrackIndex::stampsUnder(const QString &folder) const
{
    QMutexLocker lock(&m_mutex);
    QHash<QString, FileStamp> out;
    if (!m_db) return out;
    const QString f = normFolder(folder);
    Stmt s(m_db, "SELECT path, id, mtime, size FROM track WHERE path >= ?1 AND path < ?2");
    s.bind(1, lowerBound(f));
    s.bind(2, upperBound(f));
    while (s.step() == SQLITE_ROW)
        out.insert(s.str(0), FileStamp{s.i64(1), s.i64(2), s.i64(3)});
    return out;
}

bool TrackIndex::upsert(const QList<Track> &tracks)
{
    QMutexLocker lock(&m_mutex);
    if (!m_db) return false;
    if (tracks.isEmpty()) return true;
    if (!exec("BEGIN IMMEDIATE")) return false;
    Stmt s(m_db,
           "INSERT INTO track (path, mtime, size, duration_ms, codec, bitrate, sample_rate, title, "
           "  artist, album_artist, album, genre, disc, track_no, year, art) "
           "VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16) "
           "ON CONFLICT(path) DO UPDATE SET mtime=excluded.mtime, size=excluded.size, "
           "  duration_ms=excluded.duration_ms, codec=excluded.codec, bitrate=excluded.bitrate, "
           "  sample_rate=excluded.sample_rate, title=excluded.title, artist=excluded.artist, "
           "  album_artist=excluded.album_artist, album=excluded.album, genre=excluded.genre, "
           "  disc=excluded.disc, track_no=excluded.track_no, year=excluded.year, art=excluded.art");
    if (!s.ok()) { m_error = QString::fromUtf8(sqlite3_errmsg(m_db)); exec("ROLLBACK"); return false; }
    for (const Track &t : tracks) {
        s.bind(1, t.path);       s.bind(2, t.mtime);       s.bind(3, t.size);
        s.bind(4, t.durationMs); s.bind(5, t.codec);       s.bind(6, t.bitrate);
        s.bind(7, t.sampleRate); s.bind(8, t.title);       s.bind(9, t.artist);
        s.bind(10, t.albumArtist); s.bind(11, t.album);    s.bind(12, t.genre);
        s.bind(13, t.disc);      s.bind(14, t.trackNo);    s.bind(15, t.year);
        s.bind(16, t.art);
        if (s.step() != SQLITE_DONE) {
            m_error = QString::fromUtf8(sqlite3_errmsg(m_db));
            exec("ROLLBACK");
            return false;
        }
        s.reset();
    }
    return exec("COMMIT");
}

bool TrackIndex::removePaths(const QStringList &paths)
{
    QMutexLocker lock(&m_mutex);
    if (!m_db) return false;
    if (paths.isEmpty()) return true;
    if (!exec("BEGIN IMMEDIATE")) return false;
    Stmt s(m_db, "DELETE FROM track WHERE path = ?1");
    for (const QString &p : paths) {
        s.bind(1, p);
        if (s.step() != SQLITE_DONE) { exec("ROLLBACK"); return false; }
        s.reset();
    }
    return exec("COMMIT");
}

QList<Track> TrackIndex::tracksUnder(const QStringList &folders) const
{
    QMutexLocker lock(&m_mutex);
    QList<Track> out;
    if (!m_db || folders.isEmpty()) return out;

    // Two profiles may watch overlapping folders; dedupe by id while merging.
    QString sql = QStringLiteral("SELECT %1 FROM track WHERE ").arg(QLatin1String(kTrackColumns));
    for (int i = 0; i < folders.size(); ++i) {
        if (i) sql += QStringLiteral(" OR ");
        sql += QStringLiteral("(path >= ?%1 AND path < ?%2)").arg(2 * i + 1).arg(2 * i + 2);
    }
    sql += QStringLiteral(
        " ORDER BY COALESCE(NULLIF(album_artist,''), NULLIF(artist,''), '~') COLLATE NOCASE,"
        " album COLLATE NOCASE, disc, track_no, title COLLATE NOCASE, path");
    const QByteArray u = sql.toUtf8();
    Stmt s(m_db, u.constData());
    if (!s.ok()) { m_error = QString::fromUtf8(sqlite3_errmsg(m_db)); return out; }
    for (int i = 0; i < folders.size(); ++i) {
        const QString f = normFolder(folders[i]);
        s.bind(2 * i + 1, lowerBound(f));
        s.bind(2 * i + 2, upperBound(f));
    }
    while (s.step() == SQLITE_ROW) out.append(readTrack(s));
    return out;
}

Track TrackIndex::trackById(qint64 id) const
{
    QMutexLocker lock(&m_mutex);
    if (!m_db) return {};
    const QByteArray sql = QStringLiteral("SELECT %1 FROM track WHERE id = ?1")
                               .arg(QLatin1String(kTrackColumns)).toUtf8();
    Stmt s(m_db, sql.constData());
    s.bind(1, id);
    return s.step() == SQLITE_ROW ? readTrack(s) : Track{};
}

int TrackIndex::count() const
{
    QMutexLocker lock(&m_mutex);
    if (!m_db) return 0;
    Stmt s(m_db, "SELECT COUNT(*) FROM track");
    return s.step() == SQLITE_ROW ? s.i32(0) : 0;
}

} // namespace hop
