// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace hop {

// The game shelf (collection → games). A "game" is either a Steam game found through its appmanifest_*.acf file, or a ROM / disc image found by
// its file type and (when the type is ambiguous) the name of the folder it lives in. Hop-tunes only reads names: ROM files are never opened,
// and it never launches anything.
struct Game {
    qint64  id = 0;
    QString path;          // the appmanifest file (Steam) or the ROM file: absolute, canonical; the game's identity
    qint64  mtime = 0;
    qint64  size = 0;
    QString title;         // the game's name, cleaned up
    QString system;        // a system key from gameSystems(): "pc" (Steam games), "nes", "snes", "switch", "psx" ...
    QString source;        // "steam" | "gog" | "rom" | "manual" (added by hand)
    QString appId;         // Steam only
    QString cover;         // a cover picture found on this computer (Steam's own library cache), or empty
    qint64  added = 0;
};

struct GameSystem {
    const char *key;        // stored in the index
    const char *name;       // shown on the shelf
    const char *color;      // the colour of its drawn cover
    const char *exts;       // space separated, lower-case, no dot: file types that are unambiguous for this system
    const char *folders;    // space separated folder-name hints (lower-case, spaces/dashes/underscores removed)
};

const GameSystem *gameSystems(int *count);                   // every system Hop-tunes knows (not "steam")
const GameSystem *gameSystemByKey(const QString &key);       // null for unknown keys
QString gameSystemName(const QString &key);                  // "PC", "Super Nintendo" ... (the key itself when unknown)
QString gameSystemColor(const QString &key);                 // "#rrggbb"

// The system a file belongs to, or empty when it is not a game: its file type decides when it is unambiguous; otherwise (iso, bin, cue, chd,
// zip, 7z ...) the nearest enclosing folder name that names a system decides. `folderNames` is the folder names from the file's own folder up.
QString gameSystemFor(const QString &fileName, const QStringList &folderNames);
bool isDiscSidecar(const QString &fileName);                 // .bin next to a .cue is one game, not two (the caller checks the sibling)
QString cleanGameTitle(const QString &fileBaseName);         // "Super Mario World (USA) [!]" -> "Super Mario World"

// Steam: the few keys of an appmanifest that Hop-tunes uses. Tolerant of garbage; `ok` is false when there is no app id and name.
struct SteamManifest { bool ok = false; QString appId; QString name; QString installDir; };
SteamManifest parseSteamManifest(const QByteArray &acf);
bool isSteamTool(const SteamManifest &m);                    // Proton, runtimes, redistributables: not games

// GOG: each installed game has a goggame-<id>.info (JSON) in its folder. Add-ons (DLC) carry the id of the game they belong to and are not games of their own.
struct GogInfo { bool ok = false; QString gameId; QString name; bool isAddOn = false; };
GogInfo parseGogInfo(const QByteArray &json);

// Epic Games Store libraries are deliberately not scanned. A game from there (or any other launcher) is added by hand, under "Third-party PC".
extern const char *const kManualSystem;                       // "pcother"

} // namespace hop
