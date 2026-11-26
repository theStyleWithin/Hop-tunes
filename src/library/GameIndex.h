// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library/Game.h"

#include <QHash>
#include <QList>
#include <QMutex>

struct sqlite3;

namespace hop {

// The index behind the game shelf: one SQLite database (games.sqlite), one table keyed by path, parallel to SectionIndex and VideoIndex so none of
// the older scan paths can be affected. Thread-safe the same way.
class GameIndex {
public:
    GameIndex();
    ~GameIndex();
    GameIndex(const GameIndex &) = delete;
    GameIndex &operator=(const GameIndex &) = delete;

    bool open(const QString &dbPath);   // ":memory:" works for tests
    void close();
    bool isOpen() const;
    QString lastError() const;

    struct Stamp { qint64 id; qint64 mtime; qint64 size; };
    QHash<QString, Stamp> stampsUnder(const QString &folder) const;
    bool upsert(const QList<Game> &games);                 // one transaction
    bool removePaths(const QStringList &paths);

    QList<Game> gamesUnder(const QStringList &folders) const;   // by title
    int count() const;

private:
    bool exec(const char *sql);
    bool migrate();

    sqlite3 *m_db = nullptr;
    mutable QMutex m_mutex;
    mutable QString m_error;
};

} // namespace hop
