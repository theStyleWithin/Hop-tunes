// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/Scanner.h"

#include "library/CoverArt.h"
#include "library/TagReader.h"
#include "library/TrackIndex.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMetaObject>
#include <QSet>
#include <QtConcurrent/QtConcurrentRun>

namespace hop {

Scanner::Scanner(TrackIndex *index, QObject *parent) : QObject(parent), m_index(index) {}

Scanner::~Scanner()
{
    cancel();
    m_future.waitForFinished();
}

bool Scanner::isRunning() const { return m_running.loadAcquire() != 0; }

void Scanner::cancel() { m_cancel.storeRelease(1); }

void Scanner::start(const QStringList &folders)
{
    if (!m_running.testAndSetAcquire(0, 1)) return;
    m_cancel.storeRelease(0);
    emit started();
    const QString artDir = m_artDir;
    m_future = QtConcurrent::run([this, folders, artDir] {
        const Result r = scanFolders(m_index, folders, &m_cancel, [this](int done, int total) {
            QMetaObject::invokeMethod(this, [this, done, total] { emit progress(done, total); },
                                      Qt::QueuedConnection);
        }, artDir);
        QMetaObject::invokeMethod(this, [this, r] {
            m_running.storeRelease(0);
            emit finished(r.added, r.updated, r.removed, r.total);
        }, Qt::QueuedConnection);
    });
}

Scanner::Result Scanner::scanFolders(TrackIndex *index, const QStringList &folders,
                                     const QAtomicInt *cancelFlag,
                                     const std::function<void(int, int)> &progress,
                                     const QString &artCacheDir)
{
    Result r;
    // Cover art is looked up once per (directory, album): tracks of an album share it.
    QHash<QString, QString> artMemo;
    auto cancelled = [cancelFlag] { return cancelFlag && cancelFlag->loadAcquire(); };

    for (const QString &rawFolder : folders) {
        if (cancelled()) break;
        const QFileInfo folderInfo(rawFolder);
        if (!folderInfo.isDir()) continue;            // unplugged drive: keep its rows
        const QString folder = folderInfo.canonicalFilePath();

        const QHash<QString, TrackIndex::FileStamp> known = index->stampsUnder(folder);

        // 1. Walk the tree (cheap: stat only).
        QList<QFileInfo> files;
        QDirIterator it(folder, QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QFileInfo fi = it.fileInfo();
            if (TagReader::isAudioFile(fi)) files.append(fi);
            if ((files.size() & 1023) == 0 && cancelled()) break;
        }
        if (cancelled()) break;

        // 2. Read tags only for new or changed files, committing in batches.
        QSet<QString> seen;
        seen.reserve(files.size());
        QList<Track> batch;
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
            Track t = TagReader::read(fi);
            if (!artCacheDir.isEmpty()) {
                const QString key = fi.absolutePath() + QLatin1Char('\x1f') + t.album;
                auto m = artMemo.constFind(key);
                if (m == artMemo.constEnd()) m = artMemo.insert(key, CoverArt::find(path, artCacheDir));
                t.art = *m;
            }
            batch.append(t);
            if (batch.size() >= 200) { index->upsert(batch); batch.clear(); }
            if (progress && (i % 50 == 0)) progress(i, total);
        }
        index->upsert(batch);
        if (cancelled()) break;
        if (progress) progress(total, total);

        // 3. Forget files that vanished from a folder that is still present.
        QStringList gone;
        for (auto k = known.constBegin(); k != known.constEnd(); ++k)
            if (!seen.contains(k.key())) gone.append(k.key());
        index->removePaths(gone);
        r.removed += gone.size();
    }
    r.total = index->count();
    return r;
}

} // namespace hop
