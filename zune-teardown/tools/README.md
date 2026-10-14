# Tools

Original tooling written for this teardown. Pure Python 3 standard library, plus one tiny C program linked against the system `libarchive` (which decodes LZX/MSZIP cabinets). No Wine, 7-Zip, cabextract or msitools needed.

| Tool | What it does |
|---|---|
| `build.sh` | Builds `xarc` |
| `xarc.c` | Extracts any archive libarchive supports (used for the LZX cabinets). Normalises `\` → `/`, folds `X86/` → `x86/`, refuses `..` paths |
| `sfxcab.py` | Finds and carves the cabinet embedded in a Microsoft CAB self-extractor |
| `msiread.py` | MSI reader written from the specs: OLE Compound File parser, MSI stream-name decoding, string pool, `_Tables`/`_Columns` schema, every table dumped to TSV, all streams dumped raw |
| `layout.py` | Maps `File`/`Component`/`Directory` tables onto cabinet members to rebuild the real install tree |
| `petriage.py` | PE triage: arch, managed vs native (CLR header), subsystem, ASLR/NX, imports, exports, version strings → JSON |
| `unpack_all.sh` | Runs the whole chain |

```sh
./build.sh
./unpack_all.sh /path/to/ZunePackage.exe ../work x86
python3 petriage.py ../work/tree ../work/binaries.json
```

Treat extracted files as untrusted: keep `work/` outside the repo (it is git-ignored) and run Python with `-I`.

Licence: MIT (see `LICENSE`).
