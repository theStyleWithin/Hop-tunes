// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library/Section.h"

#include <QHash>
#include <QList>
#include <QMutex>

struct sqlite3;

namespace hop {

// The shared index for the pictures / audiobooks / ebooks / podcasts sections: one SQLite
// database, one table keyed by (kind, path), parallel to TrackIndex and VideoIndex so none
// of the older, already-shipped scan paths can be affected. Thread-safe the same way.
class SectionIndex {
public:
    SectionIndex();
    ~SectionIndex();
    SectionIndex(const SectionIndex &) = delete;
    SectionIndex &operator=(const SectionIndex &) = delete;

    bool open(const QString &dbPath);   // ":memory:" works for tests
    void close();
    bool isOpen() const;
    QString lastError() const;

    struct FileStamp { qint64 id; qint64 mtime; qint64 size; };
    QHash<QString, FileStamp> stampsUnder(SectionKind kind, const QString &folder) const;
    bool upsert(SectionKind kind, const QList<SectionItem> &items);   // one transaction
    bool removePaths(SectionKind kind, const QStringList &paths);

    QList<SectionItem> itemsUnder(SectionKind kind, const QStringList &folders) const;   // by group, title
    int count(SectionKind kind) const;

private:
    bool exec(const char *sql);
    bool migrate();

    sqlite3 *m_db = nullptr;
    mutable QMutex m_mutex;
    mutable QString m_error;
};

} // namespace hop
