// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>

class QFileSystemWatcher;

namespace hop {

// Live library updates (spec §6): watches the profile's folders via the kernel's inotify,
// through Qt's QFileSystemWatcher, and asks for a rescan a little while after something
// changes. Purely local file-system notifications — no network, no "phoning home" of any
// kind, just "a file in this folder changed". Debounced, because copying in an album
// fires many events in a burst and a scan mid-copy would see a half-written file.
//
// QFileSystemWatcher does not recurse: each directory must be added individually, and a
// brand-new subfolder (e.g. a freshly ripped album) isn't seen until the next refresh. So
// the caller re-calls setFolders() after every rescan — cheap, and keeps this class from
// having to duplicate the scanner's own directory walk.
class LibraryWatcher : public QObject {
    Q_OBJECT
public:
    explicit LibraryWatcher(QObject *parent = nullptr);
    ~LibraryWatcher() override;

    // Replaces the watch list with `folders` and every directory under them. Safe to call
    // repeatedly (e.g. after each rescan) — a no-op if the directory set hasn't changed.
    void setFolders(const QStringList &folders);

    // Off switch for devices where watching a whole tree is too heavy: with 0 nothing is watched (the library is
    // still rescanned at start-up and by "rescan now"). Walking a big tree and adding a watch per folder happens
    // on the calling (UI) thread, which froze Hop-tunes on Android ("isn't responding") after choosing a folder.
    // The default is unlimited, which is the desktop behaviour.
    void setMaxDirectories(int count) { m_maxDirs = count; }
    int watchedCount() const { return int(m_watched.size()); }

signals:
    // Something changed under a watched folder; debounced (~1.5 s after the last event).
    void changed();

private:
    void onPathChanged();
    int m_maxDirs = -1;

    QFileSystemWatcher *m_watcher;
    QTimer m_debounce;
    QSet<QString> m_watched;
};

} // namespace hop
