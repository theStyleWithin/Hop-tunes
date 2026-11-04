// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/Paths.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace hop::Paths {

// Where everything lives when it must not go to the usual per-user locations: the
// HOPTUNES_DATA_DIR environment variable (the portable build's launcher sets it, and it is
// handy for tests), or on Windows a "portable.txt" next to the exe. Empty means "use the
// platform's normal data and cache folders".
static QString overrideRoot()
{
    static const QString root = [] {
        const QByteArray env = qgetenv("HOPTUNES_DATA_DIR");
        if (!env.isEmpty()) return QDir::cleanPath(QString::fromLocal8Bit(env));
#ifdef Q_OS_WIN
        wchar_t buf[32768];
        const DWORD n = GetModuleFileNameW(nullptr, buf, DWORD(sizeof buf / sizeof buf[0]));
        if (n > 0 && n < sizeof buf / sizeof buf[0]) {
            const QFileInfo exe(QString::fromWCharArray(buf, int(n)));
            if (QFileInfo::exists(exe.absolutePath() + QStringLiteral("/portable.txt")))
                return exe.absolutePath() + QStringLiteral("/Hop-tunes-data");
        }
#endif
        return QString();
    }();
    return root;
}

static QString xdg(QStandardPaths::StandardLocation loc)
{
    // GenericDataLocation = ~/.local/share, GenericCacheLocation = ~/.cache
    return QStandardPaths::writableLocation(loc) + QStringLiteral("/hop-tunes");
}

QString dataDir()     { return overrideRoot().isEmpty() ? xdg(QStandardPaths::GenericDataLocation) : overrideRoot() + QStringLiteral("/data"); }
QString sharedDir()   { return dataDir() + QStringLiteral("/shared"); }
QString indexDbPath() { return sharedDir() + QStringLiteral("/index.sqlite"); }
QString profilesDir() { return dataDir() + QStringLiteral("/profiles"); }
QString cacheDir()    { return overrideRoot().isEmpty() ? xdg(QStandardPaths::GenericCacheLocation) : overrideRoot() + QStringLiteral("/cache"); }
QString artCacheDir() { return cacheDir() + QStringLiteral("/art"); }
QString videoIndexDbPath()   { return sharedDir() + QStringLiteral("/videos.sqlite"); }
QString sectionIndexDbPath() { return sharedDir() + QStringLiteral("/sections.sqlite"); }
QString videoThumbCacheDir() { return cacheDir() + QStringLiteral("/video-thumbs"); }

bool ensureDirs()
{
    bool ok = true;
    for (const QString &d : {dataDir(), sharedDir(), profilesDir(), cacheDir(), artCacheDir(), videoThumbCacheDir()}) {
        if (!QDir().mkpath(d)) { ok = false; continue; }
        QFile::setPermissions(d, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    }
    return ok;
}

QString folderPath(const QUrl &folder)
{
    if (folder.isLocalFile()) return folder.toLocalFile();
    if (folder.scheme() == QLatin1String("content")
        && folder.host() == QLatin1String("com.android.externalstorage.documents")) {
        const QString tree = QUrl::fromPercentEncoding(folder.path().toUtf8());   // "/tree/primary:Music"
        const int t = tree.indexOf(QLatin1String("/tree/"));
        if (t >= 0) {
            const QString id = tree.mid(t + 6);   // "primary:Music/Sub"
            const int colon = id.indexOf(QLatin1Char(':'));
            const QString volume = colon >= 0 ? id.left(colon) : id;
            const QString rel = colon >= 0 ? id.mid(colon + 1) : QString();
            const QString root = volume == QLatin1String("primary") ? QStringLiteral("/storage/emulated/0")
                                                                    : QStringLiteral("/storage/") + volume;
            return rel.isEmpty() ? root : root + QLatin1Char('/') + rel;
        }
    }
    return folder.toString();
}

} // namespace hop::Paths
