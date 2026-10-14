# 01: Installer format

`ZunePackage.exe` is a set of nested containers, Russian-doll style. Every layer is a documented public format, so the whole thing unpacks on Linux without Windows or Wine.

```
ZunePackage.exe                         PE32 GUI stub, x86, 3 sections, timestamp 2005-02-24 (reused SFX stub)
├── .text / .data                       ~31 KB extractor stub
├── .rsrc  (raw size 0x10E76200)        resource section; the cabinet is appended inside it
│   └── CAB @ file offset 0x8E00        "MSCF" v1.3, 283,595,405 bytes, 1 folder, LZX (window 2^21), 168 files
└── overlay @ 0x10E7E200 (6,904 bytes)  Authenticode signature (WIN_CERTIFICATE / PKCS#7)
```

The security data directory points at the overlay, so the signature covers the whole file. Modifying any byte invalidates it.

## Layer 1: the outer cabinet (168 files)

| Group | Contents |
|---|---|
| Root | `autorun.inf` (label "Zune Setup", launches `StartZuneLocal.exe`), `StartZuneLocal.exe`, `msvcr90.dll` |
| `x86/`, `x64/` | `zunesetup.exe` bootstrapper + VC9 CRT + 23 `zunesetup.exe.mui` language files each |
| `*/packages/zune-x86.msi` / `zune-x64.msi` | **The actual product** (~50 MB each) |
| `*/packages/zunewmdu-*.msi` | Windows Media device update package |
| `*/packages/zune-<lang>.msi` | 22 language-pack MSIs (esp, fra, deu, ita, jpn, chs, cht, plk, rus, nld, ptb, ptg, kor, csy, dan, ell, fin, hun, nor, sve, ind, msl) |
| `*/packages/dw20shared*.msi` | Watson crash reporting |
| `*/packages/muauth.cab` | Microsoft Update authorisation |
| Prerequisites | `dotnetfx.exe` / `netfx64.exe` (.NET), `wmfdist11-*` (Windows Media Format 11), `Windows6.x-KB973442/KB975469` (.msu for Vista/7), `WindowsXP-KB915865` / `KB932716` per language, `WindowsMedia11-KB973442`, `VistaFont_{CHS,CHT,JPN,KOR}.exe` |

The bootstrapper (`zunesetup.exe`) installs the prerequisites for the detected OS, then the product MSI, then the matching language MSI.

Note: some entries use `X86\` (upper case) and others `x86\`. On a case-sensitive filesystem these land in two different directories unless you normalise them (`tools/xarc.c` does).

## Layer 2: the product MSI

`zune-x86.msi` is a standard Windows Installer database: an OLE Compound File (`D0 CF 11 E0 A1 B1 1A E1`) with 512-byte sectors, codepage 1252 and 33 tables. It is built with WiX: the `WixCA` / `NetFxCA` custom-action DLLs are present.

Streams of interest:

| Stream | Size | Purpose |
|---|---:|---|
| `Zune.cab` | 49,458,660 | Embedded cabinet with all 178 product files (Media table: `#Zune.cab`, LastSequence 178) |
| `Binary.zuneca` | 180,448 | Zune custom actions (driver install/remove, driver store repair) |
| `Binary.mobileca` | 27,648 | Windows Mobile/Phone custom actions |
| `Binary.WixCA`, `Binary.wixca`, `Binary.NetFxCA` | — | Stock WiX custom actions (NGEN of managed assemblies etc.) |
| `\x05SummaryInformation`, `\x05DigitalSignature` | — | Package metadata + MSI signature |

MSI stream names are stored in a packed form: pairs of characters from `[0-9A-Za-z._]` are folded into one UTF-16 code unit in `U+3800–U+47FF`, and `U+4840` marks a table stream (`!`). `tools/msiread.py` decodes this.

## Layer 3: the product cabinet

`Zune.cab` uses MSI **File-table keys** as member names (for example `Zune_Exe`), not real filenames. To rebuild the real install tree:

1. Look up each key in the `File` table, which gives `FileName` (`short|long`) and `Component_`.
2. Look up `Component_` in the `Component` table to get `Directory_`.
3. Walk `Directory.Directory_Parent` up to `TARGETDIR`, using the long part of each `DefaultDir`.

`tools/layout.py` does exactly this. The result is the tree described in [02-architecture.md](02-architecture.md):

```
ProgramFiles/Zune/                       main app, 120+ files
ProgramFiles/Zune/Drivers/Zune/          USB/MTP driver package (UMDF) + 23 language MUIs
ProgramFiles/Zune/Drivers/ZuneBusEnum/   Zumbus.sys root bus enumerator
ProgramFiles/Zune/Network Sharing/       UPnP device description XML + icons
ProgramFiles/Zune/SDKComponents/PPCRL/   Windows Live ID client (msidcrl40)
ProgramFiles/Zune/en-US/                 offline HTML help
<CommonAppData>/Microsoft/Zune/FirstLaunch.wmv
System32/drivers/UMDF/en-US/ZuneDriver.dll.mui
```

## Tooling notes

- No 7-Zip, cabextract or msitools was available in the analysis environment, and package registries were blocked. Everything was done with the system's `libarchive.so.13` (which supports LZX cabinets) and pure-Python parsers written from the public [MS-CFB] and Windows Installer database specs.
- Extracting the 283 MB LZX folder with libarchive takes about 1 second.
