import numpy as np,sys,math
from PIL import Image
def load(fn):
  V=[];T=[];F=[]
  for l in open(fn):
    p=l.split()
    if not p: continue
    if p[0]=='v': V.append([float(x) for x in p[1:4]])
    elif p[0]=='vt': T.append([float(x) for x in p[1:3]])
    elif p[0]=='f':
      idx=[[int(a) if a else 0 for a in q.split('/')] for q in p[1:]]
      for i in range(1,len(idx)-1): F.append([idx[0],idx[i],idx[i+1]])
  return np.array(V),np.array(T),F
def render(V,T,F,tex,emi,R,W,H,light=np.array([0.3,0.5,-1.0])):
  P=V@R.T
  mn=P[:,:2].min(0);mx=P[:,:2].max(0);s=min((W-4)/(mx[0]-mn[0]),(H-4)/(mx[1]-mn[1]))
  sx=(P[:,0]-mn[0])*s+ (W-(mx[0]-mn[0])*s)/2; sy=H-2-(P[:,1]-mn[1])*s
  img=np.zeros((H,W,4));zb=np.full((H,W),1e9)
  th,tw=tex.shape[:2]
  light=light/np.linalg.norm(light)
  for f in F:
    vi=[a[0]-1 for a in f]; ti=[a[1]-1 for a in f]
    x=sx[vi];y=sy[vi];z=P[vi,2]
    n=np.cross(P[vi[1]]-P[vi[0]],P[vi[2]]-P[vi[0]]);nl=np.linalg.norm(n)
    if nl==0: continue
    n/=nl
    sh=0.35+0.65*abs(n@light)
    x0,x1=int(max(0,math.floor(x.min()))),int(min(W-1,math.ceil(x.max())));y0,y1=int(max(0,math.floor(y.min()))),int(min(H-1,math.ceil(y.max())))
    if x1<x0 or y1<y0: continue
    gx,gy=np.meshgrid(np.arange(x0,x1+1)+.5,np.arange(y0,y1+1)+.5)
    d=(y[1]-y[2])*(x[0]-x[2])+(x[2]-x[1])*(y[0]-y[2])
    if abs(d)<1e-9: continue
    a=((y[1]-y[2])*(gx-x[2])+(x[2]-x[1])*(gy-y[2]))/d
    b=((y[2]-y[0])*(gx-x[2])+(x[0]-x[2])*(gy-y[2]))/d
    c=1-a-b
    m=(a>=-1e-4)&(b>=-1e-4)&(c>=-1e-4)
    if not m.any(): continue
    zz=a*z[0]+b*z[1]+c*z[2]
    uv=T[ti]
    u=a*uv[0,0]+b*uv[1,0]+c*uv[2,0]; v=a*uv[0,1]+b*uv[1,1]+c*uv[2,1]
    tx=(np.mod(u,1)*(tw-1)).astype(int); ty=((1-np.mod(v,1))*(th-1)).astype(int)
    col=tex[ty,tx,:3]*0.44*1.6*sh+emi[ty,tx,:3]*1.0
    sub=zb[y0:y1+1,x0:x1+1]; upd=m&(zz<sub)
    sub[upd]=zz[upd]
    img[y0:y1+1,x0:x1+1][upd]=np.concatenate([np.clip(col,0,1),np.ones(col.shape[:2]+(1,))],-1)[upd]
  return img
if __name__=='__main__':
  V,T,F=load(sys.argv[1])
  print(V.min(0),V.max(0),len(F))
  tex=np.asarray(Image.open(sys.argv[2]).convert('RGB').resize((512,512)))/255.
  emi=np.asarray(Image.open(sys.argv[3]).convert('RGB').resize((512,512)))/255.
  out=Image.new('RGBA',(6*160,240))
  def rot(ax,ang):
    c,s=math.cos(ang),math.sin(ang)
    if ax=='x': return np.array([[1,0,0],[0,c,-s],[0,s,c]])
    if ax=='y': return np.array([[c,0,s],[0,1,0],[-s,0,c]])
    return np.array([[c,-s,0],[s,c,0],[0,0,1]])
  views=[np.eye(3),rot('y',math.pi),rot('x',-math.pi/2),rot('x',math.pi/2),rot('y',math.pi/2),rot('y',-math.pi/2)]
  for i,R in enumerate(views):
    im=render(V,T,F,tex,emi,R,160,240)
    out.paste(Image.fromarray((im*255).astype(np.uint8)),(i*160,0))
  out.save(sys.argv[4])
