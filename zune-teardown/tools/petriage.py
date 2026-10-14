#!/usr/bin/env python3
"""Clean-room PE triage: arch, managed/native, imports, exports, version strings, CLR metadata summary."""
import struct, sys, os, json, re

class PE:
    def __init__(self, path):
        self.path = path; d = self.d = open(path, 'rb').read()
        if d[:2] != b'MZ': raise ValueError('not PE')
        pe = struct.unpack_from('<I', d, 0x3c)[0]
        if d[pe:pe + 4] != b'PE\0\0': raise ValueError('not PE')
        self.machine, nsec, self.ts = struct.unpack_from('<HHI', d, pe + 4)
        optsz, self.chars = struct.unpack_from('<HH', d, pe + 20)
        o = pe + 24; self.magic = struct.unpack_from('<H', d, o)[0]
        self.is64 = self.magic == 0x20b
        self.subsys = struct.unpack_from('<H', d, o + 68)[0]
        self.dllchars = struct.unpack_from('<H', d, o + 70)[0]
        ddo = o + (112 if self.is64 else 96)
        n = struct.unpack_from('<I', d, ddo - 4)[0]
        self.dirs = [struct.unpack_from('<II', d, ddo + 8 * i) for i in range(min(n, 16))]
        so = o + optsz; self.secs = []
        for i in range(nsec):
            nm, vs, va, rs, ra = struct.unpack_from('<8sIIII', d, so + 40 * i)
            self.secs.append((nm.rstrip(b'\0').decode('latin1'), va, vs, ra, rs))

    def off(self, rva):
        for nm, va, vs, ra, rs in self.secs:
            if va <= rva < va + max(vs, rs): return rva - va + ra
        return rva

    def cstr(self, rva):
        o = self.off(rva); e = self.d.index(b'\0', o); return self.d[o:e].decode('latin1')

    def imports(self):
        rva, sz = self.dirs[1]; out = {}
        if not rva: return out
        o = self.off(rva)
        while True:
            ilt, ts, fc, name, iat = struct.unpack_from('<IIIII', self.d, o); o += 20
            if not name: break
            dll = self.cstr(name); fns = []
            t = self.off(ilt or iat); w = 8 if self.is64 else 4
            while True:
                v = int.from_bytes(self.d[t:t + w], 'little'); t += w
                if not v: break
                if v >> (63 if self.is64 else 31): fns.append('#%d' % (v & 0xffff))
                else: fns.append(self.cstr((v & 0x7fffffff) + 2))
            out[dll] = fns
        return out

    def exports(self):
        rva, sz = self.dirs[0]
        if not rva: return []
        o = self.off(rva)
        nf, nn, af, anames, aords = struct.unpack_from('<IIIII', self.d, o + 20)
        return [self.cstr(struct.unpack_from('<I', self.d, self.off(anames) + 4 * i)[0]) for i in range(nn)]

    def clr(self):
        return len(self.dirs) > 14 and self.dirs[14][0] != 0

    def version(self):
        # scan VS_VERSION_INFO string table (UTF-16) for common keys
        out = {}
        for k in ['FileDescription', 'FileVersion', 'ProductVersion', 'OriginalFilename', 'InternalName', 'CompanyName']:
            pat = k.encode('utf-16le') + b'\0\0'
            i = self.d.rfind(pat)
            if i < 0: continue
            j = i + len(pat)
            while j % 4: j += 1
            e = self.d.find(b'\0\0', j)
            while e > 0 and (e - j) % 2: e = self.d.find(b'\0\0', e + 1)
            try: out[k] = self.d[j:e].decode('utf-16le')
            except Exception: pass
        return out

MACH = {0x14c: 'x86', 0x8664: 'x64', 0x1c0: 'ARM', 0x1c4: 'ARMv7', 0x166: 'MIPS'}
if __name__ == '__main__':
    res = []
    for root, _, files in os.walk(sys.argv[1]):
        for f in sorted(files):
            p = os.path.join(root, f)
            if not re.search(r'\.(dll|exe|sys|acm|ax)$', f, re.I) or f.endswith('.mui'): continue
            try: pe = PE(p)
            except Exception: continue
            imp = pe.imports()
            res.append({'file': os.path.relpath(p, sys.argv[1]), 'size': len(pe.d), 'arch': MACH.get(pe.machine, hex(pe.machine)),
                        'managed': pe.clr(), 'subsys': pe.subsys, 'dll': bool(pe.chars & 0x2000),
                        'aslr': bool(pe.dllchars & 0x40), 'nx': bool(pe.dllchars & 0x100),
                        'imports': {k: len(v) for k, v in imp.items()}, 'exports': pe.exports()[:60],
                        'nexports': len(pe.exports()), 'version': pe.version()})
    json.dump(res, open(sys.argv[2], 'w'), indent=1)
    for r in res:
        print(f"{r['file']:<52} {r['arch']:<4} {'NET ' if r['managed'] else 'nat '} {r['size']//1024:>6}K  {r['version'].get('FileDescription','')[:60]}")
