# Zune teardown

A structural teardown of **Microsoft Zune software 4.8.2345**, the final release before the service was discontinued. It is the research groundwork for Hop-tunes, an independent open-source replacement.

This folder documents **how the software is put together**: how the installer is packed, what each component does, how the pieces depend on each other, what gets registered on the system, and which device IDs and services it uses. It does **not** contain decompiled or disassembled code, and it does **not** contain any Microsoft binaries, installers or resources. Everything here is original write-up plus original tooling, so the repo stays clean for a clean-room re-implementation.

## Source analysed

| Item | Value |
|---|---|
| Input file | `ZunePackage.exe` (283,639,032 bytes) |
| SHA-256 | `ef5e5debd76ca7df2f7b6bf7095e1354ee4f9bafdbbd29c96cb1ca87695fddee` |
| Product | Zune, version `04.08.2345.00` |
| ProductCode | `{9B75648B-6C30-4A0D-9DE6-0D09D20AF5A5}` |
| UpgradeCode | `{E22D2482-B645-498A-863D-8812B1AD5AAD}` |
| `zune-x86.msi` SHA-256 | `297a032a650fabc800e4f8ca1962af8b94f4220326bbccbb9ba7ab65f115a350` |
| `zune-x64.msi` SHA-256 | `f8418785da8a804a2fd6e3d0b50268195214301619f71e272daa11cdffd0f9a8` |

## Contents

| Path | What's in it |
|---|---|
| [`docs/01-installer-format.md`](docs/01-installer-format.md) | How `ZunePackage.exe` is packed, layer by layer, and how to unpack it on Linux |
| [`docs/02-architecture.md`](docs/02-architecture.md) | Component map: UI, app logic, library database, media pipeline, device stack, services |
| [`docs/03-installer-database.md`](docs/03-installer-database.md) | What the MSI installs and registers: services, drivers, USB IDs, registry keys, file associations, upgrade history |
| [`docs/04-community-revival-patch.md`](docs/04-community-revival-patch.md) | How the community "re-enable features" patch works (registry flags + hosts redirect) |
| [`docs/05-device-reference.md`](docs/05-device-reference.md) | Hardware IDs per device family and the reset/recovery button combos |
| [`docs/06-hop-tunes-implications.md`](docs/06-hop-tunes-implications.md) | What this means for a Linux re-implementation, plus open questions |
| [`data/binaries.md`](data/binaries.md) | Table of all 73 PE binaries with kind, size and role |
| [`data/binaries.json`](data/binaries.json) | Machine-readable triage: arch, managed/native, imports per DLL, exports, version info, ASLR/NX |
| [`tools/`](tools/) | The original extraction and triage tools used to produce all of the above |
| [`community-patch/`](community-patch/) | The community registry/hosts patch files that were analysed |

## Reproducing

All tools are pure Python 3 (no third-party packages) plus one small C program that uses the system `libarchive`. See [`tools/README.md`](tools/README.md).

```sh
tools/build.sh                                   # builds tools/xarc
tools/unpack_all.sh ZunePackage.exe ./work       # SFX → CAB → MSI → installed tree + tables
python3 tools/petriage.py ./work/tree data/binaries.json
```

## Scope and legal notes

- This work is interoperability research on discontinued software. Its aim is to document behaviour and interfaces well enough that Hop-tunes can be written independently.
- **No Microsoft code or assets are committed.** Do not add binaries, MSIs, extracted resources, or decompiler output to this repo. `.gitignore` blocks the common file types.
- Hop-tunes code must be original. Write it from these docs, public specs (MTP, UPnP/DLNA, codec specs) and observed behaviour, not from Microsoft's code.
