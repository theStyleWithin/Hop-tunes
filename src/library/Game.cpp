// SPDX-License-Identifier: GPL-3.0-or-later
#include "library/Game.h"

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>

namespace hop {

const char *const kManualSystem = "pcother";

namespace {

const GameSystem kSystems[] = {
    // key          shown as                 colour      file types that are only this system       folder names that mean this system
    {"pc",          "PC",                    "#1b2838", "",                                        "pc windows steam gog games"},
    {"pcother",     "Third-party PC",        "#44445e", "",                                        "thirdpartypc thirdparty"},
    {"dos",         "DOS",                   "#555555", "",                                        "dos msdos"},
    {"switch",      "Switch",                "#e4002b", "nsp xci",                                 "switch nintendoswitch nsw"},
    {"wiiu",        "Wii U",                 "#009ac7", "wux wud rpx",                             "wiiu nintendowiiu"},
    {"wii",         "Wii",                   "#3a9bc1", "wbfs",                                "wii nintendowii"},
    {"gamecube",    "GameCube",              "#5b4b9a", "gcm rvz gcz",                             "gc gamecube ngc nintendogamecube"},
    {"n64",         "Nintendo 64",           "#2e8b57", "n64 z64 v64",                             "n64 nintendo64"},
    {"snes",        "Super Nintendo",        "#6b5fa8", "sfc smc",                         "snes supernintendo superfamicom sfc"},
    {"nes",         "NES",                   "#b4262c", "nes unf unif fds",                        "nes nintendo famicom"},
    {"3ds",         "Nintendo 3DS",          "#c4452d", "3ds cia cci",                             "3ds nintendo3ds"},
    {"nds",         "Nintendo DS",           "#5a6770", "nds dsi",                                 "nds ds nintendods"},
    {"gba",         "Game Boy Advance",      "#3b4cc0", "gba",                                     "gba gameboyadvance"},
    {"gbc",         "Game Boy Color",        "#8e3b9e", "gbc",                                     "gbc gameboycolor"},
    {"gb",          "Game Boy",              "#7a8b3a", "gb",                                      "gb gameboy"},
    {"ps5",         "PlayStation 5",         "#1d3557", "",                                        "ps5 playstation5"},
    {"ps4",         "PlayStation 4",         "#003087", "",                                        "ps4 playstation4"},
    {"ps3",         "PlayStation 3",         "#25282c", "",                                        "ps3 playstation3"},
    {"ps2",         "PlayStation 2",         "#1f3a93", "",                                        "ps2 playstation2"},
    {"psx",         "PlayStation",           "#4a6fa5", "pbp",                                     "psx ps1 playstation playstation1 psone"},
    {"psp",         "PSP",                   "#2d3436", "cso",                                     "psp playstationportable"},
    {"psvita",      "PS Vita",               "#0b7285", "vpk",                                     "psvita vita"},
    {"xboxseries",  "Xbox Series",           "#107c10", "",                                        "xboxseries xboxseriesx"},
    {"xboxone",     "Xbox One",              "#0e6b0e", "",                                        "xboxone"},
    {"xbox360",     "Xbox 360",              "#6ab04c", "",                                        "xbox360 x360"},
    {"xbox",        "Xbox",                  "#2f6b2f", "xbe",                                     "xbox originalxbox"},
    {"dreamcast",   "Dreamcast",             "#e67e22", "gdi cdi",                                 "dreamcast dc"},
    {"saturn",      "Saturn",                "#34495e", "",                                        "saturn segasaturn"},
    {"segacd",      "Sega CD",               "#2c3e8c", "",                                        "segacd megacd"},
    {"sega32x",     "Sega 32X",              "#8e44ad", "32x",                                     "sega32x 32x"},
    {"genesis",     "Mega Drive / Genesis",  "#1d1d1d", "smd",                              "genesis megadrive md"},
    {"mastersystem","Master System",         "#c0392b", "sms",                                     "mastersystem sms"},
    {"gamegear",    "Game Gear",             "#2c3e50", "gg",                                      "gamegear gg"},
    {"sg1000",      "SG-1000",               "#7f8c8d", "",                                      "sg1000"},
    {"pce",         "PC Engine",             "#d35400", "pce sgx",                                 "pce pcengine turbografx tg16"},
    {"neogeo",       "Neo Geo",              "#b8860b", "neo",                                     "neogeo"},
    {"ngp",         "Neo Geo Pocket",        "#c2185b", "ngp",                                     "ngp neogeopocket ngpc"},
    {"wonderswan",  "WonderSwan",            "#00838f", "wsc",                                  "wonderswan wswan"},
    {"atari2600",   "Atari 2600",            "#8d5524", "a26",                                     "atari2600 a2600"},
    {"atari7800",   "Atari 7800",            "#6e4b2a", "a78",                                     "atari7800 a7800"},
    {"lynx",        "Atari Lynx",            "#a8741a", "lnx",                                     "lynx atarilynx"},
    {"colecovision","ColecoVision",          "#6d4c41", "",                                     "colecovision coleco"},
    {"msx",         "MSX",                   "#37474f", "mx1 mx2",                             "msx msx2"},
    {"c64",         "Commodore 64",          "#5c6bc0", "d64 t64",                         "c64 commodore64"},
    {"amiga",       "Amiga",                 "#ef6c00", "adf ipf",                                 "amiga"},
    {"zxspectrum",  "ZX Spectrum",           "#c62828", "tzx z80",                             "zxspectrum spectrum zx"},
    {"arcade",      "Arcade",                "#d4a017", "",                                        "arcade mame fba fbneo"},
};
constexpr int kSystemCount = int(sizeof(kSystems) / sizeof(kSystems[0]));

// Types shared by several systems: the folder decides. (`bin` is only a game next to its `cue`, handled by the scanner.)
const QStringList kAmbiguous = {QStringLiteral("iso"), QStringLiteral("bin"), QStringLiteral("cue"), QStringLiteral("chd"),
                                QStringLiteral("zip"), QStringLiteral("7z"), QStringLiteral("img"), QStringLiteral("mdf"),
                                // real ROM types of some systems that other programs also use (.md is Markdown, .wad is Doom): only with a system's folder name
                                QStringLiteral("md"), QStringLiteral("gen"), QStringLiteral("wad"), QStringLiteral("ws"), QStringLiteral("sg"),
                                QStringLiteral("col"), QStringLiteral("dsk"), QStringLiteral("prg"), QStringLiteral("sna"), QStringLiteral("rom")};

QString squash(QString s)
{
    s = s.toLower();
    QString out;
    for (const QChar c : s) if (c.isLetterOrNumber()) out.append(c);
    return out;
}

const QHash<QString, int> &extMap()
{
    static const QHash<QString, int> m = [] {
        QHash<QString, int> h;
        for (int i = 0; i < kSystemCount; ++i)
            for (const QString &e : QString::fromLatin1(kSystems[i].exts).split(QLatin1Char(' '), Qt::SkipEmptyParts)) h.insert(e, i);
        return h;
    }();
    return m;
}

const QHash<QString, int> &folderMap()
{
    static const QHash<QString, int> m = [] {
        QHash<QString, int> h;
        for (int i = 0; i < kSystemCount; ++i)
            for (const QString &f : QString::fromLatin1(kSystems[i].folders).split(QLatin1Char(' '), Qt::SkipEmptyParts)) h.insert(f, i);
        return h;
    }();
    return m;
}

// Steam's appmanifest is "KeyValues" text: "key"  "value" pairs and { } blocks. Only the top-level pairs are read, with a hard size cap.
QString unescape(QString s)
{
    s.replace(QStringLiteral("\\\\"), QStringLiteral("\x01")).replace(QStringLiteral("\\\""), QStringLiteral("\"")).replace(QStringLiteral("\x01"), QStringLiteral("\\"));
    return s.trimmed();
}

} // namespace

const GameSystem *gameSystems(int *count) { if (count) *count = kSystemCount; return kSystems; }

const GameSystem *gameSystemByKey(const QString &key)
{
    for (int i = 0; i < kSystemCount; ++i) if (key == QLatin1String(kSystems[i].key)) return &kSystems[i];
    return nullptr;
}

QString gameSystemName(const QString &key)
{
    const GameSystem *s = gameSystemByKey(key);
    return s ? QString::fromLatin1(s->name) : key;
}

QString gameSystemColor(const QString &key)
{
    const GameSystem *s = gameSystemByKey(key);
    return s ? QString::fromLatin1(s->color) : QStringLiteral("#4a4a4a");
}

QString gameSystemFor(const QString &fileName, const QStringList &folderNames)
{
    const QString ext = fileName.section(QLatin1Char('.'), -1).toLower();
    if (ext == fileName.toLower()) return {};                      // no dot: not a game file
    const auto e = extMap().constFind(ext);
    if (e != extMap().constEnd()) return QString::fromLatin1(kSystems[e.value()].key);
    if (!kAmbiguous.contains(ext)) return {};
    for (const QString &f : folderNames) {                          // nearest folder first
        const auto h = folderMap().constFind(squash(f));
        if (h != folderMap().constEnd()) return QString::fromLatin1(kSystems[h.value()].key);
    }
    return {};
}

bool isDiscSidecar(const QString &fileName) { return fileName.section(QLatin1Char('.'), -1).compare(QLatin1String("bin"), Qt::CaseInsensitive) == 0; }

QString cleanGameTitle(const QString &fileBaseName)
{
    static const QRegularExpression tags(QStringLiteral(R"(\s*[\(\[][^\)\]]*[\)\]])"));   // (USA) [!] (Rev 1) [b1]
    QString t = fileBaseName;
    t.replace(QLatin1Char('_'), QLatin1Char(' '));
    t.remove(tags);
    t = t.simplified();
    if (t.isEmpty()) t = fileBaseName.simplified();
    return t;
}

SteamManifest parseSteamManifest(const QByteArray &acf)
{
    SteamManifest m;
    if (acf.isEmpty() || acf.size() > 256 * 1024) return m;
    const QString text = QString::fromUtf8(acf);
    // Walk the tokens: quoted strings and braces; depth 1 pairs inside "AppState" are the ones that matter.
    int depth = 0;
    QStringList pending;                                              // a key waiting for its value at the current depth
    for (int i = 0; i < text.size();) {
        const QChar c = text.at(i);
        if (c == QLatin1Char('{')) { ++depth; pending.clear(); ++i; continue; }
        if (c == QLatin1Char('}')) { if (depth > 0) --depth; pending.clear(); ++i; continue; }
        if (c != QLatin1Char('"')) { ++i; continue; }
        QString tok;
        ++i;
        while (i < text.size() && text.at(i) != QLatin1Char('"')) {
            if (text.at(i) == QLatin1Char('\\') && i + 1 < text.size()) { tok.append(text.at(i)); ++i; }
            tok.append(text.at(i));
            ++i;
        }
        ++i;                                                          // the closing quote
        if (depth == 1) {
            if (pending.isEmpty()) pending << tok;
            else {
                const QString key = pending.takeFirst().toLower();
                const QString val = unescape(tok);
                if (key == QLatin1String("appid")) m.appId = val;
                else if (key == QLatin1String("name")) m.name = val;
                else if (key == QLatin1String("installdir")) m.installDir = val;
            }
        } else {
            pending.clear();
        }
    }
    static const QRegularExpression digits(QStringLiteral("^[0-9]{1,12}$"));
    m.ok = digits.match(m.appId).hasMatch() && !m.name.isEmpty() && m.name.size() <= 300;
    return m;
}

GogInfo parseGogInfo(const QByteArray &json)
{
    GogInfo g;
    if (json.isEmpty() || json.size() > 256 * 1024) return g;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) return g;
    const QJsonObject o = doc.object();
    auto str = [&o](const char *k) { const QJsonValue v = o.value(QLatin1String(k)); return v.isString() ? v.toString().trimmed() : v.isDouble() ? QString::number(qint64(v.toDouble())) : QString(); };
    g.gameId = str("gameId");
    g.name = str("name");
    const QString root = str("rootGameId");
    g.isAddOn = !root.isEmpty() && root != g.gameId;
    static const QRegularExpression digits(QStringLiteral("^[0-9]{1,20}$"));
    g.ok = digits.match(g.gameId).hasMatch() && !g.name.isEmpty() && g.name.size() <= 300;
    return g;
}

bool isSteamTool(const SteamManifest &m)
{
    const QString n = m.name.toLower();
    return n.startsWith(QLatin1String("proton")) || n.startsWith(QLatin1String("steam linux runtime"))
        || n.startsWith(QLatin1String("steamworks common")) || n.startsWith(QLatin1String("steam controller configs"))
        || n.contains(QLatin1String("redistributable")) || m.appId == QLatin1String("228980") || m.appId == QLatin1String("1070560")
        || m.appId == QLatin1String("1391110") || m.appId == QLatin1String("1628350");
}

} // namespace hop
