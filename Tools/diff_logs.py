import re
def msgs(p):
    out=[]
    for line in open(p,'rb').read().decode('utf8','replace').splitlines():
        if 'LogBlueprint: Error' in line and '[Compiler]' in line:
            out.append(line.split('[Compiler]',1)[1].strip())
    return sorted(out)
a=msgs('Saved/Logs/CompileAllBlueprints-5.8-r9.log')
b=msgs('Saved/Logs/CompileAllBlueprints-5.8-r10.log')
from collections import Counter
ca,cb=Counter(a),Counter(b)
print('only-r9:')
for k,v in (ca-cb).items(): print(' ',v,k)
print('only-r10:')
for k,v in (cb-ca).items(): print(' ',v,k)
