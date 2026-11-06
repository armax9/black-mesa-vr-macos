#!/usr/bin/env python3
"""Read-only verification of critical upstream RVAs against installed Valve PEs."""
from pathlib import Path
import struct,json
G=Path('/Volumes/armax/SteamLibrary/steamapps/common/Black Mesa')
def pe(path,rva,count):
 d=path.read_bytes();nt=struct.unpack_from('<I',d,60)[0];n=struct.unpack_from('<H',d,nt+6)[0];z=struct.unpack_from('<H',d,nt+20)[0]
 for i in range(n):
  p=nt+24+z+i*40;vs,va,raw,off=struct.unpack_from('<4I',d,p+8)
  if va<=rva<va+max(vs,raw):return d[off+rva-va:off+rva-va+count]
 raise ValueError(hex(rva))
checks=[('bms/bin/client.dll',0x29cc70,'558bec83ec245356578b7d088bf185ff'),('bms/bin/client.dll',0x201db0,'558bec81ecdc020000'),('bms/bin/client.dll',0x102590,'558bece8'),('bms/bin/server.dll',0x47d460,'558becff7508e8'),('bin/materialsystem.dll',0x69bb0,'83795800'),('bin/engine.dll',0xa6d30,'558bec')]
checks += [
 ('bms/bin/client.dll',0x1f41f0,'568bf18b4e0485c9740c8b016a0468'),
 ('bms/bin/client.dll',0x2ff7c0,'558bec83ec288d45f0'),
 ('bms/bin/client.dll',0x223960,'558bec81ece4000000'),
 ('bms/bin/client.dll',0x2221d0,'558bec81ec20020000'),
 ('bin/materialsystem.dll',0x49a30,'558bec83b9c42a0000007514'),
]
r=[]
for rel,rva,pattern in checks:
 b=bytes.fromhex(pattern);actual=pe(G/rel,rva,len(b));ok=actual==b;r.append(dict(module=rel,rva=hex(rva),ok=ok,actual=actual.hex()))
print(json.dumps(r,indent=2));assert all(x['ok'] for x in r)
