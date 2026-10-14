// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAtomicInt>
#include <QFuture>
#include <QObject>
#include <QStringList>

#include <functional>

namespace hop {

class TrackIndex;

// Incremental folder scanner (spec §6). Walks folders on a worker thread, reads tags
// only for files that are new or whose size/mtime changed, and drops index rows for
// files that disappeared. Folders that are missing entirely (unplugged drive) are
// skipped, not purged, so tracks come back when the drive does.
class Scanner : public QObject {
    Q_OBJECT
public:
    struct Result { int scanned = 0; int added = 0; int updated = 0; int removed = 0; int total = 0; };

    explicit Scanner(TrackIndex *index, QObject *parent = nullptr);
    ~Scanner() override;

    bool isRunning() const;
    void start(const QStringList &folders);   // no-op while running
    void cancel();

    // Synchronous core, used by the worker and by tests.
    static Result scanFolders(TrackIndex *index, const QStringList &folders,
                              const QAtomicInt *cancelFlag = nullptr,
                              const std::function<void(int done, int total)> &progress = {});

signals:
    void started();
    void progress(int done, int total);
    void finished(int added, int updated, int removed, int total);

private:
    TrackIndex *m_index;
    QFuture<void> m_future;
    QAtomicInt m_cancel{0};
    QAtomicInt m_running{0};
};

} // namespace hop
