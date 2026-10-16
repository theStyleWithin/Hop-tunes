// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/LibraryWatcher.h"

#include <QDirIterator>
#include <QFileSystemWatcher>

namespace hop {

LibraryWatcher::LibraryWatcher(QObject *parent)
    : QObject(parent), m_watcher(new QFileSystemWatcher(this))
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(1500);
    connect(&m_debounce, &QTimer::timeout, this, &LibraryWatcher::changed);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &LibraryWatcher::onPathChanged);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &LibraryWatcher::onPathChanged);
}

LibraryWatcher::~LibraryWatcher() = default;

void LibraryWatcher::onPathChanged()
{
    m_debounce.start();   // restart: a burst of events (copying an album) collapses to one rescan
}

void LibraryWatcher::setFolders(const QStringList &folders)
{
    QSet<QString> want;
    for (const QString &folder : folders) {
        if (folder.isEmpty()) continue;
        want.insert(folder);
        QDirIterator it(folder, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) want.insert(it.next());
    }
    if (want == m_watched) return;

    if (!m_watcher->files().isEmpty()) m_watcher->removePaths(m_watcher->files());
    if (!m_watcher->directories().isEmpty()) m_watcher->removePaths(m_watcher->directories());
    if (!want.isEmpty()) m_watcher->addPaths(QStringList(want.cbegin(), want.cend()));
    m_watched = want;
}

} // namespace hop
