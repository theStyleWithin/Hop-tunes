#!/usr/bin/env python3
"""Minimal clean-room MSI reader: OLE CFB container + MSI table decoding.
Usage: msiread.py <file.msi> <outdir>   -> dumps all tables as TSV, raw streams, and embedded cabs.
Written from the public [MS-CFB] spec and documented MSI database layout."""
import struct, sys, os, json

class CFB:
    def __init__(self, data):
        self.d = data
        h = data[:512]
        assert h[:8] == bytes.fromhex('d0cf11e0a1b11ae1')
        self.ss = 1 << struct.unpack_from('<H', h, 0x1E)[0]
        self.mss = 1 << struct.unpack_from('<H', h, 0x20)[0]
        nfat, dir1 = struct.unpack_from('<II', h, 0x2C)
        self.cutoff, mf1, nmf, difat1, ndifat = struct.unpack_from('<IIIII', h, 0x38)
        difat = list(struct.unpack_from('<109I', h, 0x4C))
        s = difat1
        while s < 0xFFFFFFFA and ndifat:
            sec = self.sector(s)
            n = self.ss // 4
            v = struct.unpack_from('<%dI' % n, sec)
            difat += v[:-1]; s = v[-1]; ndifat -= 1
        self.fat = []
        for s in difat[:nfat]:
            self.fat += struct.unpack('<%dI' % (self.ss // 4), self.sector(s))
        self.minifat = []
        if mf1 < 0xFFFFFFFA:
            raw = self.chain(mf1)
            self.minifat = list(struct.unpack('<%dI' % (len(raw) // 4), raw))
        dirs = self.chain(dir1)
        self.entries = []
        for i in range(len(dirs) // 128):
            e = dirs[i * 128:(i + 1) * 128]
            nl = struct.unpack_from('<H', e, 64)[0]
            name = e[:max(nl - 2, 0)].decode('utf-16le', 'surrogatepass')
            typ = e[66]
            start, size = struct.unpack_from('<IQ', e, 116)
            if self.ss == 512: size &= 0xFFFFFFFF
            self.entries.append((name, typ, start, size))
        root = self.entries[0]
        self.ministream = self.chain(root[2])[:root[3]]

    def sector(self, s):
        o = (s + 1) * self.ss
        return self.d[o:o + self.ss]

    def chain(self, s):
        out = []; seen = 0
        while s < 0xFFFFFFFA:
            out.append(self.sector(s)); s = self.fat[s]; seen += 1
            if seen > len(self.fat) + 1: raise ValueError('FAT loop')
        return b''.join(out)

    def stream(self, idx):
        name, typ, start, size = self.entries[idx]
        if size < self.cutoff:
            out = []; s = start
            while s < 0xFFFFFFFA:
                o = s * self.mss; out.append(self.ministream[o:o + self.mss]); s = self.minifat[s]
            return b''.join(out)[:size]
        return self.chain(start)[:size]

    def streams(self):
        for i, (n, t, s, z) in enumerate(self.entries):
            if t == 2: yield n, i

CH = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz._'
def msi_name(n):
    out = ''
    for c in n:
        o = ord(c)
        if 0x3800 <= o < 0x4800:
            o -= 0x3800; out += CH[o & 0x3f] + CH[(o >> 6) & 0x3f]
        elif 0x4800 <= o < 0x4840: out += CH[o - 0x4800]
        elif o == 0x4840: out += '!'
        else: out += c
    return out

class MSI:
    def __init__(self, path):
        self.cfb = CFB(open(path, 'rb').read())
        self.raw = {msi_name(n): i for n, i in self.cfb.streams()}
        pool = self.get('!_StringPool'); sdata = self.get('!_StringData')
        cp, flags = struct.unpack_from('<HH', pool, 0)
        self.codepage = cp
        self.sref = 3 if flags & 0x8000 else 2
        enc = {65001: 'utf-8', 1252: 'cp1252', 0: 'cp1252'}.get(cp, 'cp%d' % cp)
        self.strings = ['']; off = 0; n = 1; ents = len(pool) // 4
        while n < ents:
            ln, refs = struct.unpack_from('<HH', pool, n * 4)
            if ln == 0 and refs != 0:
                n += 1; lo, hi = struct.unpack_from('<HH', pool, n * 4); ln = lo | (hi << 16)
            s = sdata[off:off + ln]; off += ln
            try: self.strings.append(s.decode(enc))
            except Exception: self.strings.append(s.decode('latin1'))
            n += 1
        # _Columns: Table(str) Number(i2) Name(str) Type(i2)
        cols = self.read_cols('!_Columns', [0x0800, 2, 0x0800, 2])
        self.schema = {}
        for t, num, name, typ in cols:
            self.schema.setdefault(t, []).append((num, name, typ))
        for t in self.schema: self.schema[t].sort()
        self.tables = [r[0] for r in self.read_cols('!_Tables', [0x0800])]

    def get(self, name):
        return self.cfb.stream(self.raw[name]) if name in self.raw else b''

    def width(self, typ):
        if typ & 0x0800: return self.sref
        return 4 if (typ & 0xff) == 4 else 2

    def read_cols(self, stream, types):
        d = self.get(stream)
        rw = sum(self.width(t) for t in types)
        if not rw or not d: return []
        nrows = len(d) // rw
        cols = []; off = 0
        for t in types:
            w = self.width(t); col = []
            for r in range(nrows):
                v = int.from_bytes(d[off + r * w: off + r * w + w], 'little')
                if t & 0x0800: col.append(self.strings[v] if v < len(self.strings) else '?%d' % v)
                elif w == 2: col.append(None if v == 0 else v - 0x8000)
                else: col.append(None if v == 0 else v - 0x80000000)
            cols.append(col); off += w * nrows
        return list(zip(*cols))

    def table(self, name):
        sch = self.schema.get(name)
        if not sch: return [], []
        hdr = [s[1] for s in sch]
        return hdr, self.read_cols('!' + name, [s[2] for s in sch])

if __name__ == '__main__':
    m = MSI(sys.argv[1]); out = sys.argv[2]
    os.makedirs(out + '/tables', exist_ok=True); os.makedirs(out + '/streams', exist_ok=True)
    for t in m.tables:
        hdr, rows = m.table(t)
        with open(f'{out}/tables/{t}.tsv', 'w') as f:
            f.write('\t'.join(hdr) + '\n')
            for r in rows: f.write('\t'.join('' if v is None else str(v).replace('\t', ' ').replace('\n', '\\n') for v in r) + '\n')
    for n, i in m.raw.items():
        if n.startswith('!'): continue
        safe = n.replace('/', '_').replace('\x05', '_')
        open(f'{out}/streams/{safe}', 'wb').write(m.cfb.stream(i))
    print(json.dumps({'codepage': m.codepage, 'tables': len(m.tables), 'streams': [n for n in m.raw if not n.startswith('!')][:50]}))
