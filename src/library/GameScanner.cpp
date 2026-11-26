// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/GameScanner.h"

#include "library/GameIndex.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QSet>
#include <QtConcurrent/QtConcurrentRun>

namespace hop {

namespace {

constexpr int kMaxDepth = 10;
constexpr int kMaxGames = 200000;

// Steam keeps each game's own files in these: thousands of files that are never games of their own.
bool isSteamBulk(const QString &parentName, const QString &name)
{
    if (parentName.compare(QLatin1String("steamapps"), Qt::CaseInsensitive) != 0) return false;
    static const QStringList bulk = {QStringLiteral("common"), QStringLiteral("compatdata"), QStringLiteral("shadercache"),
                                     QStringLiteral("workshop"), QStringLiteral("downloading"), QStringLiteral("temp")};
    return bulk.contains(name.toLower());
}

// Steam's own picture of a game (its library cache), when this computer has one near the chosen folder: old layout `<appid>_library_600x900.jpg`,
// newer layout `<appid>/<hash>/library_600x900.jpg`. Best effort: Steam changes this layout from time to time.
QString steamCoverFor(const QString &manifestPath, const QString &appId)
{
    QDir dir = QFileInfo(manifestPath).dir();
    for (int up = 0; up < 5 && dir.exists(); ++up) {
        const QString cache = dir.filePath(QStringLiteral("appcache/librarycache"));
        if (QFileInfo(cache).isDir()) {
            const QString old = cache + QLatin1Char('/') + appId + QStringLiteral("_library_600x900.jpg");
            if (QFileInfo::exists(old)) return old;
            QDirIterator it(cache + QLatin1Char('/') + appId, {QStringLiteral("library_600x900*.jpg")}, QDir::Files, QDirIterator::Subdirectories);
            if (it.hasNext()) return it.next();
            return {};
        }
        if (!dir.cdUp()) break;
    }
    return {};
}

struct Walker {
    QList<Game> found;
    QAtomicInt const *cancel = nullptr;
    bool cancelled() const { return cancel && cancel->loadAcquire(); }

    void walk(const QDir &dir, const QStringList &folderNames, int depth)
    {
        if (depth > kMaxDepth || cancelled() || found.size() >= kMaxGames) return;
        const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::Readable | QDir::NoDotAndDotDot, QDir::Name);
        bool hasCue = false;
        for (const QFileInfo &fi : files) if (fi.suffix().compare(QLatin1String("cue"), Qt::CaseInsensitive) == 0) { hasCue = true; break; }

        for (const QFileInfo &fi : files) {
            const QString name = fi.fileName();
            if (name.startsWith(QLatin1String("appmanifest_"), Qt::CaseInsensitive) && name.endsWith(QLatin1String(".acf"), Qt::CaseInsensitive)) {
                Game g;
                g.path = fi.absoluteFilePath(); g.mtime = fi.lastModified().toSecsSinceEpoch(); g.size = fi.size();
                g.source = QStringLiteral("steam"); g.system = QStringLiteral("pc");
                found.append(g);
                continue;
            }
            if (name.startsWith(QLatin1String("goggame-"), Qt::CaseInsensitive) && name.endsWith(QLatin1String(".info"), Qt::CaseInsensitive)) {
                Game g;
                g.path = fi.absoluteFilePath(); g.mtime = fi.lastModified().toSecsSinceEpoch(); g.size = fi.size();
                g.source = QStringLiteral("gog"); g.system = QStringLiteral("pc");
                found.append(g);
                continue;
            }
            if (hasCue && isDiscSidecar(name)) continue;               // the tracks of a disc: the .cue is the game
            const QString system = gameSystemFor(name, folderNames);
            if (system.isEmpty()) continue;
            Game g;
            g.path = fi.absoluteFilePath(); g.mtime = fi.lastModified().toSecsSinceEpoch(); g.size = fi.size();
            g.source = QStringLiteral("rom"); g.system = system;
            g.title = cleanGameTitle(fi.completeBaseName());
            found.append(g);
        }

        const QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::Readable | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Name);
        for (const QFileInfo &d : dirs) {
            if (cancelled()) return;
            if (isSteamBulk(dir.dirName(), d.fileName())) continue;
            QStringList names = folderNames;
            names.prepend(d.fileName());
            walk(QDir(d.absoluteFilePath()), names, depth + 1);
        }
    }
};

} // namespace

GameScanner::GameScanner(GameIndex *index, QObject *parent) : QObject(parent), m_index(index) {}

GameScanner::~GameScanner()
{
    cancel();
    m_future.waitForFinished();
}

bool GameScanner::isRunning() const { return m_running.loadAcquire() != 0; }

void GameScanner::cancel() { m_cancel.storeRelease(1); }

void GameScanner::start(const QStringList &folders)
{
    if (!m_running.testAndSetAcquire(0, 1)) return;
    m_cancel.storeRelease(0);
    emit started();
    m_future = QtConcurrent::run([this, folders] {
        const Result r = scanFolders(m_index, folders, &m_cancel, [this](int done, int total) {
            QMetaObject::invokeMethod(this, [this, done, total] { emit progress(done, total); }, Qt::QueuedConnection);
        });
        QMetaObject::invokeMethod(this, [this, r] {
            m_running.storeRelease(0);
            emit finished(r.added, r.updated, r.removed, r.total);
        }, Qt::QueuedConnection);
    });
}

GameScanner::Result GameScanner::scanFolders(GameIndex *index, const QStringList &folders, const QAtomicInt *cancelFlag,
                                             const std::function<void(int, int)> &progress)
{
    Result r;
    auto cancelled = [cancelFlag] { return cancelFlag && cancelFlag->loadAcquire(); };

    for (const QString &rawFolder : folders) {
        if (cancelled()) break;
        const QFileInfo folderInfo(rawFolder);
        if (!folderInfo.isDir()) continue;                 // unplugged drive: keep its rows
        const QString folder = folderInfo.canonicalFilePath();
        if (folder.isEmpty()) continue;                     // the drive went away between the two questions: an empty path would match every row
        const QHash<QString, GameIndex::Stamp> known = index->stampsUnder(folder);

        Walker w;
        w.cancel = cancelFlag;
        w.walk(QDir(folder), {QFileInfo(folder).fileName()}, 0);
        if (cancelled()) break;

        QSet<QString> seen;
        seen.reserve(w.found.size());
        QList<Game> batch;
        const int total = w.found.size();
        for (int i = 0; i < total; ++i) {
            if (cancelled()) break;
            Game g = w.found[i];
            seen.insert(g.path);
            ++r.scanned;
            const auto k = known.constFind(g.path);
            if (k != known.constEnd() && k->mtime == g.mtime && k->size == g.size) continue;

            if (g.source == QLatin1String("steam")) {
                QFile f(g.path);
                if (!f.open(QIODevice::ReadOnly)) continue;
                const SteamManifest m = parseSteamManifest(f.read(256 * 1024 + 1));
                if (!m.ok || isSteamTool(m)) {
                    // not a game (a tool, or a manifest that is mid-write): leave it out, and drop it if it was known
                    seen.remove(g.path);
                    continue;
                }
                g.title = m.name;
                g.appId = m.appId;
                g.cover = steamCoverFor(g.path, m.appId);
            }
            else if (g.source == QLatin1String("gog")) {
                QFile f(g.path);
                if (!f.open(QIODevice::ReadOnly)) continue;
                const GogInfo gi = parseGogInfo(f.read(256 * 1024 + 1));
                if (!gi.ok || gi.isAddOn) { seen.remove(g.path); continue; }
                g.title = gi.name;
                g.appId = gi.gameId;
            }
            (k == known.constEnd() ? r.added : r.updated)++;
            batch.append(g);
            if (batch.size() >= 50) { index->upsert(batch); batch.clear(); }
            if (progress && (i % 10 == 0)) progress(i, total);
        }
        index->upsert(batch);
        if (cancelled()) break;
        if (progress) progress(total, total);

        // A drive pulled out while it was being walked gives a short (or empty) list: that is not a reason to forget its games, or anyone else's.
        if (!QFileInfo(folder).isDir()) continue;
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
