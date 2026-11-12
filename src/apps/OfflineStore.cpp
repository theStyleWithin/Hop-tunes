// SPDX-License-Identifier: GPL-3.0-or-later
#include "apps/OfflineStore.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QStorageInfo>

namespace hop {

namespace {
QString fmt(qint64 n)
{
    constexpr qint64 MB = OfflineStore::kMB;
    if (n <= 0) return QStringLiteral("0 MB");
    if (n < MB) return QStringLiteral("%1 MB").arg(qMax(0.1, qRound(double(n) / MB * 10) / 10.0), 0, 'f', 1);
    if (n >= 1024 * MB) return QStringLiteral("%1 GB").arg(double(n) / (1024 * MB), 0, 'f', 2);
    return QStringLiteral("%1 MB").arg(n / MB);
}
} // namespace

void OfflineStore::setDefaultBase(const QString &dir) { m_defaultBase = dir; emit changed(); }
void OfflineStore::setProfileId(const QString &id)
{
    // a profile id becomes a folder name: keep it to plain characters
    QString clean;
    for (QChar c : id) clean += (c.isLetterOrNumber() || c == QLatin1Char('-') || c == QLatin1Char('_')) ? c : QLatin1Char('_');
    m_profileId = clean.isEmpty() ? QStringLiteral("profile") : clean;
    refresh();
}

QString OfflineStore::baseDir() const
{
    return m_location.isEmpty() ? m_defaultBase : QDir(m_location).filePath(QStringLiteral("Hop-tunes offline"));
}

QString OfflineStore::folder() const
{
    const QString base = baseDir();
    return base.isEmpty() ? QString() : QDir(base).filePath(m_profileId);
}

bool OfflineStore::available() const
{
    if (m_location.isEmpty()) return !m_defaultBase.isEmpty();
    return QFileInfo(m_location).isDir();      // an unplugged drive: the folder is not there
}

qint64 OfflineStore::freeBytes() const
{
    const QString where = folder();
    if (where.isEmpty()) return 0;
    if (m_freeProbe) return m_freeProbe(where);
    QString probe = where;
    while (!probe.isEmpty() && !QFileInfo(probe).isDir()) {                // the folder may not exist yet: ask about the nearest one that does
        const QString up = QFileInfo(probe).absolutePath();
        if (up == probe) break;
        probe = up;
    }
    return QStorageInfo(probe).bytesAvailable();
}

qint64 OfflineStore::dirSize(const QString &dir)
{
    qint64 n = 0;
    QDirIterator it(dir, QDir::Files | QDir::NoSymLinks | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) { it.next(); n += it.fileInfo().size(); }
    return n;
}

void OfflineStore::refresh()
{
    m_used = available() ? dirSize(folder()) : 0;
    emit changed();
}

QString OfflineStore::summary() const
{
    if (!available()) return QStringLiteral("the place you chose for offline kick is not available right now (is the drive plugged in?). nothing is lost; downloads wait until it is back.");
    QString s = QStringLiteral("using %1 of your %2 limit").arg(fmt(m_used), fmt(limitBytes()));
    if (full()) s += QStringLiteral(". it is full, so new downloads are paused: raise the limit or delete something");
    else if (nearLimit()) s += QStringLiteral(". you are close to the limit");
    return s + QStringLiteral(".");
}

bool OfflineStore::setLimitMB(double mb)
{
    if (!(mb >= kMinLimitMB) || mb > 1024.0 * 1024 * 100) return false;
    m_limitMB = mb;
    emit changed();
    emit edited();
    return true;
}

bool OfflineStore::setLocation(const QString &dir)
{
    const QString clean = dir.isEmpty() ? QString() : QDir::cleanPath(dir);
    if (!clean.isEmpty() && !QFileInfo(clean).isDir()) return false;
    m_location = clean;
    refresh();
    emit edited();
    return true;
}

bool OfflineStore::moveTo(const QString &dir, QString *why)
{
    auto no = [&](const QString &m) { if (why) *why = m; return false; };
    const QString clean = dir.isEmpty() ? QString() : QDir::cleanPath(dir);
    if (!clean.isEmpty() && !QFileInfo(clean).isDir()) return no(QStringLiteral("that folder is not there"));
    const QString from = folder();
    OfflineStore target;
    target.m_defaultBase = m_defaultBase; target.m_profileId = m_profileId; target.m_location = clean; target.m_freeProbe = m_freeProbe;
    const QString to = target.folder();
    if (to == from) return true;
    const qint64 need = available() ? dirSize(from) : 0;
    if (target.freeBytes() < need + kMinFree) return no(QStringLiteral("there is not enough room there for what you have stored (%1) and the drive's own free space").arg(fmt(need)));
    if (!QDir().mkpath(to)) return no(QStringLiteral("that place cannot be written to"));
    if (QFileInfo(from).isDir()) {
        // copy first, remove the old copy only once everything arrived
        QDirIterator it(from, QDir::Files | QDir::NoSymLinks | QDir::Hidden, QDirIterator::Subdirectories);
        const QDir src(from);
        while (it.hasNext()) {
            const QString f = it.next();
            const QString rel = src.relativeFilePath(f);
            const QString dest = QDir(to).filePath(rel);
            QDir().mkpath(QFileInfo(dest).absolutePath());
            if (!QFile::exists(dest) && !QFile::copy(f, dest)) return no(QStringLiteral("copying %1 failed (nothing was removed from the old place)").arg(rel));
        }
        QDir(from).removeRecursively();
    }
    m_location = clean;
    refresh();
    emit edited();
    return true;
}

bool OfflineStore::canStore(qint64 bytes, QString *why) const
{
    auto no = [&](const QString &m) { if (why) *why = m; return false; };
    if (why) why->clear();
    if (!available()) return no(QStringLiteral("the place you chose for offline kick is not available right now"));
    if (m_used + bytes > limitBytes()) return no(QStringLiteral("this would go over your offline limit of %1: raise it in settings or delete something").arg(fmt(limitBytes())));
    if (freeBytes() < bytes + kMinFree) return no(QStringLiteral("the drive is nearly full, so downloads are paused to keep room for everything else"));
    return true;
}

QJsonObject OfflineStore::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("limitMB"), m_limitMB);
    if (!m_location.isEmpty()) o.insert(QStringLiteral("location"), m_location);
    return o;
}

void OfflineStore::fromJson(const QJsonObject &o)
{
    const double mb = o.value(QStringLiteral("limitMB")).toDouble(kDefaultLimitMB);
    m_limitMB = mb >= kMinLimitMB ? mb : kDefaultLimitMB;
    m_location = o.value(QStringLiteral("location")).toString();
    refresh();
}

} // namespace hop
