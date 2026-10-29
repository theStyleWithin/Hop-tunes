// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library/Section.h"

#include <QAtomicInt>
#include <QFuture>
#include <QObject>
#include <QStringList>

#include <functional>

namespace hop {

class SectionIndex;

// Incremental folder scanner for one section. Same behaviour as Scanner and VideoScanner:
// reads metadata only for new or changed files, drops rows for files that disappeared, and
// skips (does not purge) folders that are missing, so an unplugged drive's items come back.
class SectionScanner : public QObject {
    Q_OBJECT
public:
    struct Result { int scanned = 0; int added = 0; int updated = 0; int removed = 0; int total = 0; };

    SectionScanner(SectionKind kind, SectionIndex *index, QObject *parent = nullptr);
    ~SectionScanner() override;

    bool isRunning() const;
    void start(const QStringList &folders);   // no-op while running
    void cancel();

    // Synchronous core, used by the worker and by tests.
    static Result scanFolders(SectionKind kind, SectionIndex *index, const QStringList &folders,
                              const QAtomicInt *cancelFlag = nullptr,
                              const std::function<void(int done, int total)> &progress = {});

signals:
    void started();
    void progress(int done, int total);
    void finished(int added, int updated, int removed, int total);

private:
    SectionKind m_kind;
    SectionIndex *m_index;
    QFuture<void> m_future;
    QAtomicInt m_cancel{0};
    QAtomicInt m_running{0};
};

} // namespace hop
