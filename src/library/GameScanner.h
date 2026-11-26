// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library/Game.h"

#include <QAtomicInt>
#include <QFuture>
#include <QObject>
#include <QStringList>

#include <functional>

namespace hop {

class GameIndex;

// Incremental scanner for the game shelf. Finds Steam games (appmanifest_*.acf, in the folder or its steamapps) and ROM / disc image files, and
// writes them to the GameIndex. Same behaviour as the other scanners: only new or changed files are looked at, rows for files that disappeared
// are dropped, and a folder that is missing (an unplugged drive) is skipped, so its games come back with the drive. It never reads inside a
// ROM and never walks into a Steam game's own files (steamapps/common and friends).
class GameScanner : public QObject {
    Q_OBJECT
public:
    struct Result { int scanned = 0; int added = 0; int updated = 0; int removed = 0; int total = 0; };

    explicit GameScanner(GameIndex *index, QObject *parent = nullptr);
    ~GameScanner() override;

    bool isRunning() const;
    void start(const QStringList &folders);   // no-op while running
    void cancel();

    // Synchronous core, used by the worker and by tests.
    static Result scanFolders(GameIndex *index, const QStringList &folders,
                              const QAtomicInt *cancelFlag = nullptr,
                              const std::function<void(int done, int total)> &progress = {});

signals:
    void started();
    void progress(int done, int total);
    void finished(int added, int updated, int removed, int total);

private:
    GameIndex *m_index;
    QFuture<void> m_future;
    QAtomicInt m_cancel{0};
    QAtomicInt m_running{0};
};

} // namespace hop
