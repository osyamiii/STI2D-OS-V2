import struct,zlib,os,sys
U='/root/.claude/uploads/c3e27356-189c-521d-a7e3-4abd53dd9843/'
parts=[U+'34e38fe8-Backrooms96_FR_Complet.z01',U+'a8e777da-Backrooms96_FR_Complet.z02',U+'d72ac058-Backrooms96_FR_Complet.z03',U+'009da878-Backrooms96_FR_Complet.z04',U+'9dc8e728-Backrooms96_FR_Complet.zip']
sizes=[os.path.getsize(p) if p else 29360128 for p in parts]
class Cat:
  def read(self,disk,off,n):
    out=b''
    while n>0:
      if parts[disk] is None: raise IOError('missing disk %d'%disk)
      f=open(parts[disk],'rb');f.seek(off);b=f.read(n);f.close()
      out+=b;n-=len(b);disk+=1;off=0
    return out
c=Cat()
d=open(parts[4],'rb').read()
p=22517841;out=sys.argv[1]
while d[p:p+4]==b'PK\x01\x02':
  (sig,vm,vn,fl,meth,t,dt,crc,cs,us,nl,el,cl,disk,ia,ea,lo)=struct.unpack('<4s6H3L5H2L',d[p:p+46])
  n=d[p+46:p+46+nl].decode('cp437');p+=46+nl+el+cl
  if n.endswith('/'): continue
  try:
    h=c.read(disk,lo,30); nl2,el2=struct.unpack('<HH',h[26:30])
    # compute position after header
    pos=lo+30+nl2+el2; dk=disk
    while pos>=sizes[dk]: pos-=sizes[dk]; dk+=1
    raw=c.read(dk,pos,cs)
    data=zlib.decompress(raw,-15) if meth==8 else raw
    assert zlib.crc32(data)==crc
    fp=os.path.join(out,n);os.makedirs(os.path.dirname(fp),exist_ok=True);open(fp,'wb').write(data)
  except Exception as e: print('FAIL',n,e)
