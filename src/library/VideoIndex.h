// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library/Video.h"

#include <QHash>
#include <QList>
#include <QMutex>
#include <QString>
#include <QStringList>

struct sqlite3;

namespace hop {

// The shared video index (spec §5b): a second SQLite database, parallel to TrackIndex,
// so a scan of a mixed music+video folder never touches the (already well-tested) audio
// path. Thread-safe like TrackIndex: the scanner thread and the UI thread share one instance.
class VideoIndex {
public:
    VideoIndex();
    ~VideoIndex();
    VideoIndex(const VideoIndex &) = delete;
    VideoIndex &operator=(const VideoIndex &) = delete;

    bool open(const QString &dbPath);   // ":memory:" works for tests
    void close();
    bool isOpen() const;
    QString lastError() const;

    struct FileStamp { qint64 id; qint64 mtime; qint64 size; qint64 durationMs; QString thumb; };
    QHash<QString, FileStamp> stampsUnder(const QString &folder) const;
    bool upsert(const QList<Video> &videos);   // one transaction
    bool removePaths(const QStringList &paths);

    QList<Video> videosUnder(const QStringList &folders) const;   // newest-added first
    Video videoById(qint64 id) const;
    int count() const;

private:
    bool exec(const char *sql);
    bool migrate();

    sqlite3 *m_db = nullptr;
    mutable QMutex m_mutex;
    mutable QString m_error;
};

} // namespace hop
