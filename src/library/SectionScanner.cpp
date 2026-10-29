// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/SectionScanner.h"

#include "library/SectionIndex.h"
#include "library/TagReader.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QImageReader>
#include <QMetaObject>
#include <QSet>
#include <QtConcurrent/QtConcurrentRun>

namespace hop {

namespace {

QString folderName(const QFileInfo &fi) { return fi.dir().dirName(); }

// Fills the kind-specific fields. Cheap for pictures and ebooks; audio kinds read tags once.
void describe(SectionKind kind, const QFileInfo &fi, SectionItem &it)
{
    it.title = fi.completeBaseName();
    it.group = folderName(fi);
    switch (kind) {
    case SectionKind::Pictures: {
        QImageReader r(it.path);
        const QSize s = r.size();
        it.width = s.width();
        it.height = s.height();
        break;
    }
    case SectionKind::Audiobooks:
    case SectionKind::Podcasts: {
        const Track t = TagReader::read(fi);
        if (!t.title.trimmed().isEmpty()) it.title = t.title.trimmed();
        if (!t.album.trimmed().isEmpty()) it.group = t.album.trimmed();
        it.durationMs = t.durationMs;
        break;
    }
    case SectionKind::Ebooks:
        break;
    }
}

} // namespace

SectionScanner::SectionScanner(SectionKind kind, SectionIndex *index, QObject *parent)
    : QObject(parent), m_kind(kind), m_index(index) {}

SectionScanner::~SectionScanner()
{
    cancel();
    m_future.waitForFinished();
}

bool SectionScanner::isRunning() const { return m_running.loadAcquire() != 0; }

void SectionScanner::cancel() { m_cancel.storeRelease(1); }

void SectionScanner::start(const QStringList &folders)
{
    if (!m_running.testAndSetAcquire(0, 1)) return;
    m_cancel.storeRelease(0);
    emit started();
    m_future = QtConcurrent::run([this, folders] {
        const Result r = scanFolders(m_kind, m_index, folders, &m_cancel, [this](int done, int total) {
            QMetaObject::invokeMethod(this, [this, done, total] { emit progress(done, total); },
                                      Qt::QueuedConnection);
        });
        QMetaObject::invokeMethod(this, [this, r] {
            m_running.storeRelease(0);
            emit finished(r.added, r.updated, r.removed, r.total);
        }, Qt::QueuedConnection);
    });
}

SectionScanner::Result SectionScanner::scanFolders(SectionKind kind, SectionIndex *index,
                                                   const QStringList &folders,
                                                   const QAtomicInt *cancelFlag,
                                                   const std::function<void(int, int)> &progress)
{
    Result r;
    auto cancelled = [cancelFlag] { return cancelFlag && cancelFlag->loadAcquire(); };
    const QStringList &exts = sectionExtensions(kind);

    for (const QString &rawFolder : folders) {
        if (cancelled()) break;
        const QFileInfo folderInfo(rawFolder);
        if (!folderInfo.isDir()) continue;            // unplugged drive: keep its rows
        const QString folder = folderInfo.canonicalFilePath();
        const QHash<QString, SectionIndex::FileStamp> known = index->stampsUnder(kind, folder);

        QList<QFileInfo> files;
        QDirIterator it(folder, QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QFileInfo fi = it.fileInfo();
            if (exts.contains(fi.suffix().toLower())) files.append(fi);
            if ((files.size() & 1023) == 0 && cancelled()) break;
        }
        if (cancelled()) break;

        QSet<QString> seen;
        seen.reserve(files.size());
        QList<SectionItem> batch;
        const int total = files.size();
        for (int i = 0; i < total; ++i) {
            if (cancelled()) break;
            const QFileInfo &fi = files[i];
            const QString path = fi.absoluteFilePath();
            seen.insert(path);
            ++r.scanned;
            const auto k = known.constFind(path);
            const qint64 mtime = fi.lastModified().toSecsSinceEpoch();
            if (k != known.constEnd() && k->mtime == mtime && k->size == fi.size()) continue;
            (k == known.constEnd() ? r.added : r.updated)++;

            SectionItem item;
            item.path = path;
            item.mtime = mtime;
            item.size = fi.size();
            describe(kind, fi, item);
            batch.append(item);
            if (batch.size() >= 50) { index->upsert(kind, batch); batch.clear(); }
            if (progress && (i % 10 == 0)) progress(i, total);
        }
        index->upsert(kind, batch);
        if (cancelled()) break;
        if (progress) progress(total, total);

        QStringList gone;
        for (auto k = known.constBegin(); k != known.constEnd(); ++k)
            if (!seen.contains(k.key())) gone.append(k.key());
        index->removePaths(kind, gone);
        r.removed += gone.size();
    }
    r.total = index->count(kind);
    return r;
}

} // namespace hop
