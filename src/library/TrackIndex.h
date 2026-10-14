// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library/Track.h"

#include <QHash>
#include <QList>
#include <QMutex>
#include <QString>
#include <QStringList>

struct sqlite3;

namespace hop {

// The shared track index (spec §3.3, §9.3): one SQLite database caching tags and
// audio properties per file, so files are never scanned twice. Thread-safe: every
// public method takes an internal lock, so the scanner thread and the UI thread can
// share one instance.
class TrackIndex {
public:
    TrackIndex();
    ~TrackIndex();
    TrackIndex(const TrackIndex &) = delete;
    TrackIndex &operator=(const TrackIndex &) = delete;

    bool open(const QString &dbPath);   // ":memory:" works for tests
    void close();
    bool isOpen() const;
    QString lastError() const;

    // Scanner support
    struct FileStamp { qint64 id; qint64 mtime; qint64 size; };
    QHash<QString, FileStamp> stampsUnder(const QString &folder) const;   // path -> stamp
    bool upsert(const QList<Track> &tracks);                              // one transaction
    bool removePaths(const QStringList &paths);

    // Queries
    // All tracks whose path lies under one of `folders`, in album order:
    // album artist, album, disc, track number, title.
    QList<Track> tracksUnder(const QStringList &folders) const;
    Track trackById(qint64 id) const;
    int count() const;

private:
    bool exec(const char *sql);
    bool migrate();

    sqlite3 *m_db = nullptr;
    mutable QMutex m_mutex;
    mutable QString m_error;
};

} // namespace hop
