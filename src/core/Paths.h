// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QUrl>

namespace hop {

// XDG locations (spec §3.4). Everything lives under ~/.local/share/hop-tunes etc.; on Windows the
// per-user AppData folders. HOPTUNES_DATA_DIR (or portable.txt beside the exe on Windows)
// moves it all under one folder, for the portable build.
namespace Paths {
    QString dataDir();        // $XDG_DATA_HOME/hop-tunes
    QString sharedDir();      // dataDir()/shared
    QString indexDbPath();    // sharedDir()/index.sqlite
    QString profilesDir();    // dataDir()/profiles  (profiles.json + one folder per profile)
    QString cacheDir();       // $XDG_CACHE_HOME/hop-tunes
    QString artCacheDir();    // cacheDir()/art  (extracted cover images, content-addressed)
    QString videoIndexDbPath();  // sharedDir()/videos.sqlite (spec §5b)
    QString sectionIndexDbPath(); // sharedDir()/sections.sqlite (pictures, audiobooks, ebooks, podcasts)
    // A folder the user picked, as a path. On Android the picker returns a content:// tree address
    // (content://com.android.externalstorage.documents/tree/primary%3AMusic), which is mapped to its
    // plain path (/storage/emulated/0/Music); anything else is returned as it came.
    QString folderPath(const QUrl &folder);
    QString videoThumbCacheDir(); // cacheDir()/video-thumbs (frame grabs, content-addressed)
    bool ensureDirs();        // create the directories above (0700)
}

} // namespace hop
