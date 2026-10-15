// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

namespace hop {

// XDG locations (spec §3.4). Everything lives under ~/.local/share/hop-tunes etc.
namespace Paths {
    QString dataDir();        // $XDG_DATA_HOME/hop-tunes
    QString sharedDir();      // dataDir()/shared
    QString indexDbPath();    // sharedDir()/index.sqlite
    QString profilesDir();    // dataDir()/profiles  (profiles.json + one folder per profile)
    QString cacheDir();       // $XDG_CACHE_HOME/hop-tunes
    QString artCacheDir();    // cacheDir()/art  (extracted cover images, content-addressed)
    bool ensureDirs();        // create the directories above (0700)
}

} // namespace hop
