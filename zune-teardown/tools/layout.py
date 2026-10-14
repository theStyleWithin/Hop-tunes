import csv,sys,os,shutil
t=sys.argv[1]; cab=sys.argv[2]; out=sys.argv[3]
def rd(n): return list(csv.DictReader(open(f'{t}/{n}.tsv'),delimiter='\t',quoting=csv.QUOTE_NONE))
D={r['Directory']:r for r in rd('Directory')}
def lng(s): s=s.split(':')[-1]; return s.split('|')[-1]
def path(d):
    r=D[d]; nm=lng(r['DefaultDir'])
    if not r['Directory_Parent'] or r['Directory_Parent']==d: return d
    p=path(r['Directory_Parent']); return p if nm=='.' else p+'/'+nm
C={r['Component']:r for r in rd('Component')}
n=0
for f in rd('File'):
    src=os.path.join(cab,f['File'])
    if not os.path.exists(src): continue
    dst=os.path.join(out,path(C[f['Component_']]['Directory_']),lng(f['FileName']))
    os.makedirs(os.path.dirname(dst),exist_ok=True); shutil.copyfile(src,dst); n+=1
print(n,'files placed')
