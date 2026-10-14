# 03: What the installer installs and registers

Decoded from the `zune-x86.msi` tables with `tools/msiread.py`. The x64 MSI has the same ProductCode and version, with x64 binaries.

## Launch conditions
- 32-bit package refuses to install on 64-bit Windows (`Msix64 = ""`).
- Requires Windows XP SP2 or later (`VersionNT >= 501`).
- Requires administrator rights.

## Features

| Feature | Notes |
|---|---|
| `Zune` (root) | — |
| `PlayerFeature` | Main application |
| `Splash` | First-launch video (`FirstLaunch.wmv` in CommonAppData) |
| `SqlCE` | Embedded database engine (ZuneDB / ZuneQP / ZuneSE) |
| `DeviceSupport` | WMDM provider, ZMDB libraries |
| `DeviceDrivers` | UMDF/WinUSB driver package + bus enumerator |
| `NetworkSharingService` | `ZuneNss.exe` UPnP/DLNA server |
| `Transcoder` | `ZuneEnc.exe` / `ZuneEncEng.dll` |

## Services

| Service name | Display name | Binary | Type / start | Account | Depends on |
|---|---|---|---|---|---|
| `ZuneWlanCfgSvc` | Zune Wireless Configuration Service | `ZuneWlanCfgSvc.exe` | own process / manual | LocalSystem | RPCSS |
| `ZuneBusEnum` | Zune Bus Enumerator | `ZuneBusEnum.exe` | own process, interactive / **auto** | LocalSystem | — |
| `WMZuneComm` | Zune Windows Mobile Connectivity Service | `WMZuneComm.exe` | own process / manual | LocalService | — |
| `ZuneNetworkSvc` | Zune Network Sharing Service | `ZuneNss.exe` | own process / manual | NetworkService | upnphost, http |

## Drivers and USB hardware IDs

The `zuneca` custom action installs and removes drivers per hardware ID. `Zune.inf` maps all of them to the `ZuneMTPZUSB` install section (WinUSB + UMDF `ZuneDriver.dll`), with device class `{EEC5AD98-8080-425F-922A-DABF3DE3F69A}` (WPD).

| Hardware ID | Device (from INF strings / CA names) |
|---|---|
| `USB\VID_045E&PID_0710` | Zune (classic: 30/4/8/16/80/120) |
| `USB\VID_045E&PID_063E` | Zune HD |
| `USB\VID_045E&PID_0640` / `0641` / `0642` | Microsoft KIN (PMX1–3) |
| `USB\VID_045E&PID_04EC&MI_00` | Windows Phone 7 (MTPZ interface) |
| `USB\VID_045E&PID_04EC&MI_01` | Windows Phone 7 (serial/IP interface, `WMZuneSerUSB.inf`, class `{1169EC26-0CFF-41FB-8D18-1D0EC75D68B0}`) |
| `USB\MS_COMP_MTPZ` | Any device exposing the MTPZ compatible ID |
| `umb\urn:microsoft-com:device:mtpz:1` | Zune Wireless (via the root bus enumerator) |
| `root\umbus` | `Zumbus.sys` bus enumerator (class `{4D36E97D-…}`, System) |

The custom action `RepairDriverStore` runs first to clean up stale driver-store entries.

## Upgrade history (Upgrade table)
The single UpgradeCode `{E22D2482-B645-498A-863D-8812B1AD5AAD}` spans every Zune release, which gives a version timeline:

| Range | Property | Generation |
|---|---|---|
| 1.0.5341.0 – 1.3.5728.0 | `VELA_FOUND` | Zune 1.x (codename **Vela**) |
| 2.1.0697.0 – 2.1.0888.2 | `ZUNE_21_FOUND` | 2.1 |
| 2.3.0000.0 – 2.3.1338.0 | `ZUNE_23_FOUND` | 2.3 |
| 2.5.0000.0 – 4.0.0387.0 | `ZUNE_3X_FOUND` | 2.5 / 3.x |
| 4.0.0000.0 – 4.2.0202.0 | `ZUNE_4X_FOUND` | 4.0–4.2 |
| 4.7.0972.0 – 4.8.0000.0 | `ZUNE_47_FOUND` | 4.7 |

## Registry (highlights from 1,458 rows)

**Application keys** (HKLM)
- `SOFTWARE\Microsoft\Zune`: `Installation Directory`, `CurrentVersion`
- `SOFTWARE\Microsoft\Zune\Setup`: `OSVersion`, `ServicePack`
- `SOFTWARE\Microsoft\Zune\{Shell, NSS, HME, HMESQM, Sync\Associations}`: created empty and filled at runtime
- `SOFTWARE\Microsoft\Windows\CurrentVersion\Run` → `Zune Launcher` = `ZuneLauncher.exe` (auto-launch on device connect)
- User-agent token `Zune 4.7` is added under `Internet Settings\5.0\User Agent\Post Platform`. Even the 4.8 build advertises 4.7.

**Audio processing chain**: `SOFTWARE\Microsoft\Zune\NodeCLSIDs\{CLSID}` with `AutoInsert=1` and a `Priority`

| CLSID | Name |
|---|---|
| `{43BE26DE-0CF0-43FD-BA4E-D301EE4A2ECE}` | Spectrum Analyzer DMO |
| `{D7E9C0B4-0E4D-46B4-BC46-1D0222F92C6F}` | Seamless Audio DMO |
| `{FB02E8EF-ACFE-4CC0-96DF-8B5C7098272C}` | Time Compression DMO |

**Built-in radio presets**: `SOFTWARE\Microsoft\Zune\Radio\<name>` with `Image` (a `res://` URI) and `SourceURL`. Stations: 107.7 The End, KEXP, National Public Radio. The stream URLs are long dead.

**Device pass-through**: `Software\Microsoft\Zune\Driver\PassThrough`: `Tcp2UdpPort`, and `Handlers\DNS` → DLL `ZunePTDNS.dll`, `Init=InitDNSHandler`, `HandlePacket=HandleDNSPacket`, `Port=53`.

**WMDM**: `Windows Media Device Manager\KnownDeviceClasses\{Zune,ZuneIp}` → `FilterParameter=UseExtendedWmdm`; `Plugins\SP\ZuneSP` → `ProgID=ZuneSP.ZuneServiceProvider`, `PnPAware=1`.

**File associations**: `SOFTWARE\Microsoft\Zune\Capabilities\FileAssociations` (Default Programs)

`.m3u .m4a .m4b .m4v .mp3 .mp4 .wma .wmv .zpl .mbr` → `Microsoft.Zune.2.<EXT>`. The open command is `Zune.exe /PlayMedia:"%L"`. `.zpl` is the Zune playlist format, an XML/SMIL variant shared with Windows Media Player.

**Disk Cleanup**: `VolumeCaches\ZuneTranscodedFilesCache` → `%LocalAppData%\Microsoft\Zune\Transcoded Files Cache`.

**Not in the installer:** `HKCU\Software\Microsoft\Zune\FeaturesOverride`. The app only reads this key at runtime.

## UPnP network sharing (ZuneNss)
`Network Sharing/` contains the UPnP service descriptions the sharing service publishes:

| File | Service | Actions |
|---|---|---|
| `ContentDirectory.xml` | Standard UPnP AV ContentDirectory:1 | GetSearchCapabilities, GetSortCapabilities, GetSystemUpdateID, Browse, Search |
| `MediaReceiverRegistrar.xml` | Standard WMDRM-ND / Xbox MediaReceiverRegistrar | IsAuthorized, RegisterDevice, IsValidated |
| `Handshake.xml` | **Zune-specific** | DoHandshake(Request → Response, Id), Confirm(ConfirmationData) |

ContentDirectory and MediaReceiverRegistrar are public specs. `Handshake` is the Zune-specific pairing step that Zune devices perform before they browse the library over Wi-Fi.
