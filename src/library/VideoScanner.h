// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QAtomicInt>
#include <QFuture>
#include <QObject>
#include <QStringList>

#include <functional>

namespace hop {

class VideoIndex;

// Incremental video folder scanner (spec §5b). Mirrors Scanner (music) exactly, but walks
// its own extension list into a separate VideoIndex/videos.sqlite — kept apart from the
// music scan path on purpose, so this new, less battle-tested code can never regress the
// already-shipped, already-tested audio scanner.
class VideoScanner : public QObject {
    Q_OBJECT
public:
    struct Result { int scanned = 0; int added = 0; int updated = 0; int removed = 0; int total = 0; };

    explicit VideoScanner(VideoIndex *index, QObject *parent = nullptr);
    ~VideoScanner() override;

    bool isRunning() const;
    void setThumbCacheDir(const QString &dir) { m_thumbDir = dir; }
    void start(const QStringList &folders);   // no-op while running
    void cancel();

    // Synchronous core, used by the worker and by tests.
    static Result scanFolders(VideoIndex *index, const QStringList &folders,
                              const QAtomicInt *cancelFlag = nullptr,
                              const std::function<void(int done, int total)> &progress = {},
                              const QString &thumbCacheDir = {},
                              const std::function<void()> &saved = {});

signals:
    void started();
    void progress(int done, int total);
    void saved();   // new rows reached the index mid-scan, so the lists can refresh
    void finished(int added, int updated, int removed, int total);

private:
    VideoIndex *m_index;
    QString m_thumbDir;
    QFuture<void> m_future;
    QAtomicInt m_cancel{0};
    QAtomicInt m_running{0};
};

} // namespace hop
