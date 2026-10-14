#!/usr/bin/env python3
"""Locate and carve the cabinet embedded in a Microsoft CAB-based self-extractor (e.g. ZunePackage.exe).
Usage: sfxcab.py <sfx.exe> <out.cab>
Scans for a valid CFHEADER ("MSCF", reserved1 == 0, sane size/offsets) and writes cbCabinet bytes from there."""
import struct, sys, re
d = open(sys.argv[1], 'rb').read()
for m in re.finditer(b'MSCF\0\0\0\0', d):
    o = m.start()
    cb, _, coff = struct.unpack_from('<III', d, o + 8)
    vmin, vmaj, nfold, nfile = struct.unpack_from('<BBHH', d, o + 24)
    if vmaj == 1 and 0 < coff < cb and o + cb <= len(d) and nfile:
        open(sys.argv[2], 'wb').write(d[o:o + cb])
        print(f'cab at 0x{o:x}: {cb} bytes, {nfold} folder(s), {nfile} file(s)')
        break
else:
    sys.exit('no embedded cabinet found')
