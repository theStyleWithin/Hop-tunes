// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/Paths.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace hop::Paths {

static QString xdg(QStandardPaths::StandardLocation loc)
{
    // GenericDataLocation = ~/.local/share, GenericCacheLocation = ~/.cache
    return QStandardPaths::writableLocation(loc) + QStringLiteral("/hop-tunes");
}

QString dataDir()     { return xdg(QStandardPaths::GenericDataLocation); }
QString sharedDir()   { return dataDir() + QStringLiteral("/shared"); }
QString indexDbPath() { return sharedDir() + QStringLiteral("/index.sqlite"); }
QString profilesDir() { return dataDir() + QStringLiteral("/profiles"); }
QString cacheDir()    { return xdg(QStandardPaths::GenericCacheLocation); }
QString artCacheDir() { return cacheDir() + QStringLiteral("/art"); }

bool ensureDirs()
{
    bool ok = true;
    for (const QString &d : {dataDir(), sharedDir(), profilesDir(), cacheDir(), artCacheDir()}) {
        if (!QDir().mkpath(d)) { ok = false; continue; }
        QFile::setPermissions(d, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    }
    return ok;
}

} // namespace hop::Paths
