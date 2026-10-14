# 04: Community revival patch

The files in [`../community-patch/`](../community-patch/) are a community-made patch (it points to zuneupdate.com). It brings back parts of the Zune software that went dark when Microsoft shut down the Zune services. It changes **no Microsoft binaries**: it works entirely through two inputs the client already reads, a registry key and DNS resolution.

## 1. Feature flags: `HKCU\Software\Microsoft\Zune\FeaturesOverride`

The installer never writes this key. The shell reads it at startup and uses it to override the server- and region-driven feature set. Each value is a `REG_DWORD` (`1` = force on, `0` = force off).

| Flag | Patch value | What it gates (from the name and the UI area it maps to) |
|---|:-:|---|
| `Music`, `Videos`, `Podcasts`, `Art` | 1 | Collection/Marketplace media categories and album art |
| `Apps`, `Games` | 1 | Zune HD apps and games section (needed for side-loading the `Applications` folder) |
| `Marketplace` | 1 | Marketplace pivot |
| `Channels`, `Picks` | 1 | Curated channels and personal picks |
| `MusicVideos` | 1 | Music-video content type |
| `MBRPreview`, `MBRPurchase`, `MBRRental` | 1 | Multi-bitrate streaming paths (`ZuneMBR.dll`) for previews, purchases and rentals |
| `Nowplaying`, `NowplayingArt` | 1 | Now Playing view and its art mosaic |
| `Quickplay` | 1 | Quickplay start page |
| `QuickMixLocal`, `QuickMixZmp` | 1 | Smart DJ / QuickMix, both the local and the Zune Marketplace-backed variant |
| `SignInAvailable` | 1 | Shows the sign-in UI |
| `SubscriptionFreeTracks` | 1 | Zune Pass "keep 10 tracks" UI |
| `FirstLaunchIntroVideo` | 1 | Plays `FirstLaunch.wmv` on first run |
| `Social` | **0** | Zune Social, left off because its backend is gone for good |

Both `.reg` files supplied (`9a4e51ee-…` and `4838efca-…`) are byte-identical, so only one copy is kept here.

## 2. Hosts redirect: `resources.zune.net`

```
66.115.173.227 resources.zune.net
```

At startup the client fetches its resource and configuration documents from `resources.zune.net`. With Microsoft's server gone, those requests fail and the related UI stays empty. The hosts entry sends that one hostname to a community server that serves replacement documents. Only that hostname is redirected; any other `*.zune.net` endpoints still fail.

> Security note: a hosts entry like this trusts whoever runs that IP with all of the client's resource traffic. Check who operates the server before you apply it, and remove the line if the community service ever disappears.

## 3. Side-loaded Zune HD apps

`Instructions.txt` says to copy an `Applications` folder into `%LocalAppData%\Microsoft\Zune\` and then enable the `Apps` flag. The client enumerates that folder as the local app collection, and those apps can then sync to a Zune HD. The README says this only works for Zune HD.

## 4. Helper batch files (not copied; trivial)

| File | Does |
|---|---|
| `Paste_Hosts_File_into_ETC_Folder.bat` | Opens `%SystemRoot%\System32\drivers\etc` in Explorer |
| `Shortcut_to_Zune_in_Appdata.bat` | Opens `%LocalAppData%\Microsoft\Zune` in Explorer |
| `Zuneupdate.com.bat` | Opens `https://www.zuneupdate.com/resources/` |

## Relevance to Hop-tunes
- The flag list is effectively the client's **feature matrix**, which is a useful checklist for scoping what Hop-tunes re-implements.
- The redirect shows that the client is **data-driven from a resource endpoint**. A re-implementation can keep its catalog and metadata sources pluggable in the same way.
