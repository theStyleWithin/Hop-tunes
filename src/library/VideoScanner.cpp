// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/VideoScanner.h"

#include "library/VideoIndex.h"
#include "library/VideoProbe.h"
#include "library/VideoThumbnail.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMetaObject>
#include <QSet>
#include <QtConcurrent/QtConcurrentRun>

namespace hop {

VideoScanner::VideoScanner(VideoIndex *index, QObject *parent) : QObject(parent), m_index(index) {}

VideoScanner::~VideoScanner()
{
    cancel();
    m_future.waitForFinished();
}

bool VideoScanner::isRunning() const { return m_running.loadAcquire() != 0; }

void VideoScanner::cancel() { m_cancel.storeRelease(1); }

void VideoScanner::start(const QStringList &folders)
{
    if (!m_running.testAndSetAcquire(0, 1)) return;
    m_cancel.storeRelease(0);
    emit started();
    const QString thumbDir = m_thumbDir;
    m_future = QtConcurrent::run([this, folders, thumbDir] {
        const Result r = scanFolders(m_index, folders, &m_cancel, [this](int done, int total) {
            QMetaObject::invokeMethod(this, [this, done, total] { emit progress(done, total); },
                                      Qt::QueuedConnection);
        }, thumbDir, [this] {
            QMetaObject::invokeMethod(this, [this] { emit saved(); }, Qt::QueuedConnection);
        });
        QMetaObject::invokeMethod(this, [this, r] {
            m_running.storeRelease(0);
            emit finished(r.added, r.updated, r.removed, r.total);
        }, Qt::QueuedConnection);
    });
}

VideoScanner::Result VideoScanner::scanFolders(VideoIndex *index, const QStringList &folders,
                                               const QAtomicInt *cancelFlag,
                                               const std::function<void(int, int)> &progress,
                                               const QString &thumbCacheDir,
                                               const std::function<void()> &saved)
{
    Result r;
    auto cancelled = [cancelFlag] { return cancelFlag && cancelFlag->loadAcquire(); };

    for (const QString &rawFolder : folders) {
        if (cancelled()) break;
        const QFileInfo folderInfo(rawFolder);
        if (!folderInfo.isDir()) continue;
        const QString folder = folderInfo.canonicalFilePath();

        const QHash<QString, VideoIndex::FileStamp> known = index->stampsUnder(folder);

        QList<QFileInfo> files;
        QDirIterator it(folder, QDir::Files | QDir::Readable | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QFileInfo fi = it.fileInfo();
            if (VideoProbe::isVideoFile(fi)) files.append(fi);
            if ((files.size() & 1023) == 0 && cancelled()) break;
        }
        if (cancelled()) break;

        // Phase 1: list every new file at once as a bare row (mtime 0 marks "not probed yet"),
        // so shows and films appear right away. Closing the app mid-scan then loses nothing:
        // the next scan sees the mismatch and carries on probing exactly the unfinished files.
        {
            QList<Video> stubs;
            for (const QFileInfo &fi : std::as_const(files)) {
                if (known.contains(fi.absoluteFilePath())) continue;
                Video v;
                v.path = fi.absoluteFilePath();
                v.size = fi.size();
                v.container = fi.suffix().toLower();
                stubs.append(v);
            }
            if (!stubs.isEmpty()) { index->upsert(stubs); if (saved) saved(); }
        }
        if (cancelled()) break;

        QSet<QString> seen;
        seen.reserve(files.size());
        QList<Video> batch;
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
            (k == known.constEnd() || k->mtime == 0 ? r.added : r.updated)++;

            Video v;
            v.path = path;
            v.mtime = mtime;
            v.size = fi.size();
            v.container = fi.suffix().toLower();
            const VideoProbe::Info info = VideoProbe::probe(path);
            if (info.valid) {
                v.durationMs = info.durationMs;
                v.codec = info.codec;
                v.width = info.width;
                v.height = info.height;
                v.hdr = info.hdr;
                v.frameRate = info.frameRate;
                if (!thumbCacheDir.isEmpty())
                    v.thumb = VideoThumbnail::grab(path, mtime, v.durationMs, thumbCacheDir);
            }
            batch.append(v);
            if (batch.size() >= 50) { index->upsert(batch); batch.clear(); if (saved) saved(); }
            if (progress && (i % 10 == 0)) progress(i, total);
        }
        index->upsert(batch);
        if (cancelled()) break;
        if (progress) progress(total, total);

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
