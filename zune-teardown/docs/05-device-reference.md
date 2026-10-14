# 05: Device reference

## Device families and how the PC sees them

| Family | USB ID | On-device DB library | Notes |
|---|---|---|---|
| Zune classic (30, 4/8/16, 80/120) | `045E:0710` | `ZuneZMDB.Classic.dll` | Hard-drive and flash players, generations 1–3 |
| Zune HD | `045E:063E` | `ZuneZMDB.ZuneHD.dll` | Tegra-based; supports apps/games (see the patch doc) |
| Microsoft KIN | `045E:0640–0642` | `ZuneZMDB.Library.dll` (generic) | KIN phones synced media through Zune |
| Windows Phone 7 | `045E:04EC` (MI_00 MTPZ, MI_01 IP-over-USB) | `ZuneZMDB.Mobile.dll` | Also uses `WMZuneComm.exe` + DTPT pass-through |
| Zune Wireless | `umb\urn:microsoft-com:device:mtpz:1` | per family | Wi-Fi sync, surfaced through the `Zumbus.sys` virtual bus |

All of them speak **MTPZ**: standard MTP (PTP-derived, USB still-image class transport) plus an authentication handshake that has to finish before the device will allow sessions. `ZuneMTPZ.dll` is the PC side of that handshake.

## Button combinations

Summarised from the `Zune_Commands.pdf` the user supplied. The original text is from tipok.org.ua and is not reproduced here.

### Zune HD
| Action | How |
|---|---|
| Reboot | Hold **Power** (top button) ~10 s |
| Sleep | Press **Power** once |
| Power off | Hold **Power** ~3 s, then pull down on the screen-saver |
| Format / initialise (erases content, allows firmware reinstall) | From fully off, power on while holding **Home** (centre) + **Media** (side) |
| Recovery mode (keeps content, allows firmware reinstall) | Hold **Power** + **Media** ~15 s |

### Zune classic (30 / 4 / 8 / 16 / 80 / 120)
| Action | How |
|---|---|
| Reboot | Hold **Back** + **Up** |
| Sleep | Hold **Back** + **Down** |
| Clear content (keeps firmware) | Reboot; when the screen goes black, hold: Zune 30 → **Back + Right + OK**; Zune 4/8/80 → **Back + Play/Pause** |
| Initialise disk (removes firmware; device needs a PC to recover) | Reboot; when the screen goes black, hold: Zune 30 → **Back + Left + OK**; Zune 4/8/80 → **Back + OK + Play/Pause** |

### From the software
Settings → Device → Sync Options → **Erase all content** (all devices).
