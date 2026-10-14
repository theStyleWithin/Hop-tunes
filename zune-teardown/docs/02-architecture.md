# 02: Architecture

Zune 4.8 is a hybrid application. The UI and app logic are **.NET 2.0-era managed code** running on Microsoft's internal **UIX** framework, the same lineage as the Windows Media Center UI. Almost everything underneath is **native x86/x64 C++**: the database, media playback, encoding, device communication and network services.

Of 73 PE binaries, 6 are managed. One of those is the VC9 `msvcm90.dll` C++/CLI runtime, so 5 are real managed Zune components. 68 of the 73 are built with ASLR and 70 with NX/DEP.

Per-file details: [`../data/binaries.md`](../data/binaries.md).

## Layer diagram

```
┌──────────────────────────────────────────────────────────────────────────┐
│ Zune.exe (native host, 202 KB) ── loads CLR ──► ZuneShell.dll (managed)  │  app logic, navigation,
│                                                     │                    │  collection, marketplace,
│                                                     ▼                    │  sync, settings
│  UIX.dll · UIXcontrols.dll · UIX.renderapi.dll (managed)                 │  UI framework
│  UIXrender.dll (native renderer) · UIXsup.dll (debug support)            │
│  ZuneShellResources.dll (16.5 MB) · ZuneMarketplaceResources.dll         │  UI markup + art
│  ZuneResources.dll (+ .mui per language)                                 │  strings
├──────────────────────────────────────────────────────────────────────────┤
│ ZuneDBApi.dll (C++/CLI bridge) ─► ZuneDB.dll (OLE DB provider)           │  media library DB
│                                   ZuneQP.dll (query processor)           │  (SQL CE-derived engine)
│                                   ZuneSE.dll (storage engine)            │
│ ZuneNativeLib.dll (7.3 MB) · ZuneCore.dll · ZuneService.dll · ZuneSA.dll │  native core services
│ ZuneMde.dll (metadata) · ZuneSrcWrp.dll · ZuneTaskbar.dll · ZuneEffects  │
├──────────────────────────────────────────────────────────────────────────┤
│ Media pipeline (Media Foundation / DirectShow DMOs)                      │
│  ZuneH264Dec · ZuneAACDec · ZUNEMP4SDECD · l3codecp.acm (MP3)            │
│  ZuneEvr (renderer) · ZunePresenter · ZuneDXVA2 · ZuneMBR (MBR source)   │
│  ZuneEncEng.dll + ZuneEnc.exe (transcoding for sync, out-of-process)     │
├──────────────────────────────────────────────────────────────────────────┤
│ Device stack                                                             │
│  ZuneSP.dll (WMDM service provider) · ZuneSH.dll (device comm support)   │
│  ZuneZMDB.{Classic,ZuneHD,Mobile,Library}.dll (on-device DB per family)  │
│  Drivers/Zune: ZuneDriver.dll (UMDF) · ZuneUsbTransport (WinUSB)         │
│                ZuneIpTransport (Wi-Fi sync) · ZuneMTPZ.dll (MTPZ auth)   │
│                ZuneNetProxy · ZuneTcp2Udp · ZunePTDNS (device internet)  │
│  Drivers/ZuneBusEnum: Zumbus.sys + ZuneBusEnum.exe (root bus enum)       │
├──────────────────────────────────────────────────────────────────────────┤
│ Windows services                                                         │
│  ZuneNss.exe (Network Sharing, UPnP/DLNA) · ZuneWlanCfgSvc.exe (Wi-Fi)   │
│  WMZuneComm.exe (Windows Mobile/Phone) · ZuneBusEnum                     │
├──────────────────────────────────────────────────────────────────────────┤
│ Helpers: ZuneLauncher.exe (auto-launch on device connect, Run key)       │
│  ZuneHost.exe · ZuneConfig.exe/ZuneCfg.dll · ZuneShellExt.dll (Explorer) │
│  ZuneShare.exe · ZuneSetup.exe (installer/updater/uninstaller)           │
│  SDKComponents/PPCRL/msidcrl40.dll (Windows Live ID sign-in)             │
└──────────────────────────────────────────────────────────────────────────┘
```

## Subsystems

### UI: UIX
- UIX is a retained-mode, markup-driven UI framework. Its scenes are defined in markup resources compiled into `ZuneShellResources.dll` and `ZuneMarketplaceResources.dll`, and referenced with `res://ZuneShellResources!<name>` URIs. The registry's radio presets use the same scheme, for example `res://ZuneShellResources!Radio.KEXP.jpg`.
- `UIXrender.dll` is the native rendering backend. Its animations and effects are what give the Zune "Metro" look.
- `Zune.exe` is a thin native host. It is registered under `App Paths\Zune.exe` and opens media with `/PlayMedia:"%L"`. The managed entry point is the ProgID `Microsoft.Zune.Shell.LaunchZuneShell`.

### App logic: `ZuneShell.dll`
- This is the managed core: navigation (Collection / Marketplace / Social / Device / Settings pivots), playback control, sync rules, and marketplace/catalog web calls.
- Feature visibility is gated by flags that can be overridden under `HKCU\Software\Microsoft\Zune\FeaturesOverride`. The installer never writes that key; it is only read at runtime. See [04-community-revival-patch.md](04-community-revival-patch.md).

### Library database
- `ZuneDB.dll` describes itself as an "OLE DB Provider", `ZuneQP.dll` as a "Query Processor" and `ZuneSE.dll` as a "Storage Engine", all 32-bit. That three-part split matches an embedded SQL Server Compact engine. The MSI also has a `SqlCE` feature.
- `ZuneDBApi.dll` is mixed-mode C++/CLI, the bridge from managed code to the engine.
- The on-device database is a separate format, handled per device family by the `ZuneZMDB.*.dll` libraries.

### Media pipeline
- Zune ships its own decoders as DMOs/MFTs rather than relying on what the OS provides. The installer registers them under `DirectShow\MediaObjects`:
  - AAC decoder DMO `{A74E98F2-52D6-4B4E-885B-E0A6CA4F187A}`
  - MPEG-4 Simple decoder DMO `{C56FC25C-0FC6-404A-9503-B10BF51A8AB9}`, which accepts `mp4s`, `MP4S`, `m4s2`, `M4S2` and outputs YUY2
- Audio post-processing nodes are auto-inserted via `HKLM\SOFTWARE\Microsoft\Zune\NodeCLSIDs`: Spectrum Analyzer DMO, Seamless Audio DMO (gapless playback) and Time Compression DMO.
- Transcoding for sync runs out of process in `ZuneEnc.exe` / `ZuneEncEng.dll`. Its cache is `%LocalAppData%\Microsoft\Zune\Transcoded Files Cache`, registered with Disk Cleanup.
- The `.mbr` extension and `ZuneMBR.dll` (MBR source filter) handle multi-bitrate streaming of marketplace previews and rentals, which is why the feature flags include `MBRPreview`, `MBRPurchase` and `MBRRental`.

### Device stack
- Devices are reached through **Windows Media Device Manager (WMDM)**. `ZuneSP.dll` registers as a WMDM service provider (`ZuneSP.ZuneServiceProvider`, AppID `{C6E93FFB-C08C-4DC1-A0FF-E0C62E9A997C}`) for the device classes `Zune` and `ZuneIp`, with `UseExtendedWmdm`.
- The driver is **UMDF (user-mode)**, built on WinUSB: `ZuneDriver.dll` plus `ZuneUsbTransport.dll`. Wireless sync uses `ZuneIpTransport.dll` instead.
- `ZuneMTPZ.dll` ("Zune MTPZ Core Protocol Component") implements **MTPZ**, the authenticated MTP variant. Zune devices will not accept ordinary MTP sessions until it completes.
- **Device internet pass-through:** `ZuneNetProxy.dll`, `ZuneTcp2Udp.dll` and `ZunePTDNS.dll` let a tethered device reach the network through the PC. A pluggable handler table under `Software\Microsoft\Zune\Driver\PassThrough\Handlers` maps a port to a DLL with `Init` / `HandlePacket` exports. The only shipped handler is DNS on port 53 (`InitDNSHandler` / `HandleDNSPacket`). Windows Phone 7 has an equivalent under `Zune\WindowsPhone\DTPT`.
- `Zumbus.sys` + `ZuneBusEnum.exe` form a root-enumerated bus (`root\umbus`) used to surface wireless or virtual devices as if they were plugged in.

### Services (installed by the MSI)
See the table in [03-installer-database.md](03-installer-database.md#services).

### Online
- **Sign-in:** `msidcrl40.dll` (IDCRL / Passport, the Windows Live ID client).
- **Catalog and resources:** HTTP(S) to `*.zune.net`. The community patch redirects `resources.zune.net`, which shows the client fetches its resource and config manifests from there at startup.
- **Errors and telemetry:** Watson (`dw20shared`) for crash reports, and `HMESQM` registry keys for usage metrics.
