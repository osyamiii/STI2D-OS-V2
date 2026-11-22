# Génère src/assets.h à partir des ressources extraites du jeu original
import sys,os,math
sys.path.insert(0,os.path.dirname(__file__))
import numpy as np
from PIL import Image,ImageDraw,ImageFont,ImageFilter
from raster import load,render
T='tex/'
out=[]
def img(n): return Image.open(T+n+'.png').convert('RGBA')
def rgb565(a):
  a=np.asarray(a,dtype=np.float32)
  r=np.clip(a[...,0],0,255).astype(np.uint16)>>3; g=np.clip(a[...,1],0,255).astype(np.uint16)>>2; b=np.clip(a[...,2],0,255).astype(np.uint16)>>3
  return (r<<11)|(g<<5)|b
def emit16(name,arr):
  flat=np.asarray(arr,dtype=np.uint16).ravel()
  s=','.join('0x%04x'%v for v in flat)
  out.append('static const uint16_t %s[%d] = {%s};'%(name,len(flat),s))
def emit8(name,arr,sec=None):
  flat=np.asarray(arr,dtype=np.uint8).ravel()
  s=','.join(str(int(v)) for v in flat)
  attr=' __attribute__((section("%s")))'%sec if sec else ''
  st='' if sec else 'static '
  out.append('%sconst uint8_t %s[%d]%s = {%s};'%(st,name,len(flat),attr,s))
def save_preview(name,a): Image.fromarray(np.clip(a,0,255).astype(np.uint8)).save('preview_'+name+'.png')

def tile(im,n): # répète l'image n x n
  w,h=im.size; o=Image.new(im.mode,(w*n,h*n))
  for i in range(n):
    for j in range(n): o.paste(im,(i*w,j*h))
  return o
# --- Murs 128x128 (stockés en colonnes) ---
wp=img('T_BackRoomsWall01_AlbedoTransparency_167').convert('RGB')
wall=tile(wp.resize((64,64),Image.LANCZOS),2)
base=img('T_BackRoomsWallBases01_AlbedoTransparency_164').convert('RGB').resize((128,12),Image.LANCZOS)
wall.paste(base,(0,116))
# liseré sombre au-dessus de la plinthe
W0=np.asarray(wall,dtype=np.float32)
W0[115]*=0.55
def over(dst,src_rgba,x,y):
  s=np.asarray(src_rgba,dtype=np.float32); h,w=s.shape[:2]; a=s[...,3:4]/255.
  dst[y:y+h,x:x+w]=dst[y:y+h,x:x+w]*(1-a)+s[...,:3]*a
walls=[]
walls.append(W0.copy())
g=W0.copy(); over(g,img('T_TexturesCom_DecalBottom0006_1_masked_S_124').resize((128,64),Image.LANCZOS),0,64); walls.append(g)
d=W0.copy(); over(d,img('T_TexturesCom_DecalsLeaking0330_2_masked_S_128').resize((128,40),Image.LANCZOS),0,0); walls.append(d)
# page accrochée au mur
pg=W0.copy()
paper=img('T_largepreview_121').convert('RGB').resize((36,48),Image.LANCZOS)
pa=np.asarray(paper,dtype=np.float32)*0.92
pg[38:38+48+2,48:48+36+2]*=0.55  # ombre
pg[36:36+48,46:46+36]=pa
pg[37:39,62:66]=[200,40,30]  # punaise
walls.append(pg)
# porte de sortie (bois + cadre + poignée)
dr=W0.copy()
wood=np.asarray(img('T_wood_134').convert('RGB').resize((44,90),Image.LANCZOS),dtype=np.float32)
dr[24:116,40:88]=[70,52,30]
dr[26:116,42:86]=wood*0.85
for yy in (34,72):
  dr[yy:yy+30,48:80]=dr[yy:yy+30,48:80]*0.82
  dr[yy,48:80]*=0.6; dr[yy+29,48:80]*=1.15
dr[70:74,78:82]=[200,190,150]
walls.append(dr)
# porte ouverte : portail blanc
po=W0.copy()
po[24:116,40:88]=[70,52,30]
yy,xx=np.mgrid[0:90,0:44]
glow=255-np.minimum(255,(np.abs(xx-21.5)**2/8+np.abs(yy-45)**2/60))*0.15
po[26:116,42:86]=np.stack([glow,glow,glow*0.97],-1)
walls.append(po)
for i,wv in enumerate(walls):
  emit16('tex_wall%d'%i,rgb565(np.transpose(wv,(1,0,2))))  # colonne par colonne
save_preview('walls',np.concatenate(walls,1))
# --- Sol (moquette) 64x64 ---
carpet=np.asarray(img('T_BackRoomsRough_AlbedoTransparency v2_153').convert('RGB').resize((64,64),Image.LANCZOS),dtype=np.float32)
dirty=np.asarray(img('T_BackRoomsDirty_AlbedoTransparency_104').convert('RGB').resize((64,64),Image.LANCZOS),dtype=np.float32)
emit16('tex_carpet',rgb565(carpet))
damp=carpet*0.45+dirty*0.55
emit16('tex_carpet_damp',rgb565(damp))
# --- Plafond 64x64 (2x2 dalles) ---
tint=np.array([1.0,0.88,0.5])
ct=np.asarray(img('T_BackRoomsCeilingTile_AlbedoTransparency_90').convert('RGB').resize((32,32),Image.LANCZOS),dtype=np.float32)*tint
fr=np.asarray(img('T_BackRoomsCeilingFrame_AlbedoTransparency_157').convert('RGB').resize((64,64),Image.LANCZOS),dtype=np.float32)
dt=np.asarray(img('T_BackRoomsCeilingDirtyTile_AlbedoTransparency.png.meta_71').convert('RGB').resize((32,32),Image.LANCZOS),dtype=np.float32)
def ceil_base(tiles):
  c=np.zeros((64,64,3),np.float32)
  for j in range(2):
    for i in range(2): c[j*32:j*32+32,i*32:i*32+32]=tiles[j][i]
  fcol=fr*np.array([0.85,0.8,0.65])
  for k in (0,31,32,63):
    c[k,:]=fcol[k,:]; c[:,k]=fcol[:,k]
  return c
c_norm=ceil_base([[ct,ct],[ct,ct]])
light=np.zeros((32,32,3),np.float32)+[255,252,235]
light[1:31,1:31]=[255,255,248]
c_light=ceil_base([[light,ct],[light,ct]])
vent=ct.copy()
vent[4:28,4:28]=[60,56,45]
for k in range(5,28,3): vent[k,4:28]=[150,140,110]
c_vent=ceil_base([[ct,ct],[ct,vent]])
dirt=ct*0.5+dt*tint*0.25+ct*dt/255*0.5
c_dirty=ceil_base([[ct,dirt],[dirt,ct]])
for n,c in (('norm',c_norm),('light',c_light),('vent',c_vent),('dirty',c_dirty)): emit16('tex_ceil_'+n,rgb565(c))
save_preview('floorceil',np.concatenate([carpet,damp,c_norm,c_light,c_vent,c_dirty],1))
# --- Lampe torche (cookie original) 80x60 ---
ck=Image.open(T+'T_200px-Cookie_tutorial_texture_flashlight_98.png').convert('RGBA').getchannel('A')
ck=ck.resize((60,60),Image.LANCZOS); cookie=Image.new('L',(80,60)); cookie.paste(ck,(10,0))
c=np.asarray(cookie,dtype=np.float32); c=np.clip(c*255/max(1,c.max()),0,255)
emit8('flash_cookie',c)
save_preview('cookie',np.stack([c]*3,-1))
# --- Monstre : rendu du vrai modèle 3D, 4 images de marche ---
V,TT,F=load('mesh/thing_275.obj')
mtex=np.asarray(Image.open(T+'T_thing_Base_color_59.png').convert('RGB'))/255.
memi=np.asarray(Image.open(T+'T_thing_Emissive_171.png').convert('RGB'))/255.
R=np.array([[-1,0,0],[0,0,1],[0,1,0]],dtype=np.float64)  # face caméra, Z vers le haut
R=np.array([[1,0,0],[0,0,1],[0,-1,0]],dtype=np.float64)@np.array([[-1,0,0],[0,-1,0],[0,0,1]],dtype=np.float64)
def walk(ph):
  P=V.copy(); hip=25.5
  legs=(P[:,2]<hip)&(np.abs(P[:,0])<7.5)
  for side in (-1,1):
    m=legs&(np.sign(P[:,0])==side)
    a=math.sin(ph)*0.35*side
    y=P[m,1];z=P[m,2]-hip
    P[m,1]=y*math.cos(a)-z*math.sin(a); P[m,2]=y*math.sin(a)+z*math.cos(a)+hip
    # pied levé
  arms=(np.abs(P[:,0])>7.5)
  for side in (-1,1):
    m=arms&(np.sign(P[:,0])==side)
    a=-math.sin(ph)*0.25*side; sh=45
    y=P[m,1];z=P[m,2]-sh
    P[m,1]=y*math.cos(a)-z*math.sin(a); P[m,2]=y*math.sin(a)+z*math.cos(a)+sh
  return P
frames=[]
# boîte englobante fixe pour toutes les images
for k in range(4):
  P=walk(k*math.pi/2)
  P=np.vstack([P,[[-16,0,-0.2],[16,0,64]]])  # bornes fixes
  im=render(P,TT,F,mtex,memi,R,128,256)
  a=Image.fromarray((im*255).astype(np.uint8),'RGBA').resize((64,128),Image.LANCZOS)
  frames.append(np.asarray(a,dtype=np.float32))
mon=[]
for fimg in frames:
  rgb=fimg[...,:3]; al=fimg[...,3]
  c=rgb565(rgb*1.25)
  c[c==0xF81F]=0xF81E
  c[al<110]=0xF81F
  mon.append(np.transpose(c,(1,0)))
emit16('spr_monster',np.stack(mon))
save_preview('monster',np.concatenate([f[...,:3]*(f[...,3:]/255) for f in frames],1))
# visage pour le jumpscare 160x120
Rh=R
headF=[f for f in F if V[f[0][0]-1,2]>47]
im=render(V,TT,headF,mtex,memi,R,480,480,light=np.array([0.0,0.6,-1.0]))
a=Image.fromarray((im*255).astype(np.uint8),'RGBA')
bb=a.getbbox(); a=a.crop(bb)
w,h=a.size
ch=h*0.8; cw=ch*4/3
face=a.crop((int(w/2-cw/2),int(h*0.02),int(w/2+cw/2),int(h*0.02+ch)))
bg=Image.new('RGB',face.size,(8,4,4)); bg.paste(face,(0,0),face)
bg=bg.resize((160,120),Image.LANCZOS)
fa=np.asarray(bg,dtype=np.float32)*1.3
emit16('img_jumpscare',rgb565(fa))
save_preview('jumpscare',fa)
# --- Icône PLAY du HUD ---
pl=img('T_Play Icon_92').resize((54,22),Image.LANCZOS)
pa=np.asarray(pl,dtype=np.float32); c=rgb565(pa); c[c==0xF81F]=0xF81E; c[pa[...,3]<100]=0xF81F
emit16('spr_play',c)
# --- Polices (VCR OSD Mono originale) ---
CHARS=''.join(chr(i) for i in range(32,127))+'éèêëàâäçùûüîïôöÉÈÊÀÂÇÙÛÎÔœ«»°'
out.append('static const char font_chars[] = "%s";'%CHARS.replace('\\','\\\\').replace('"','\\"'))
cps=','.join(str(ord(ch)) for ch in CHARS)
out.append('static const uint16_t font_cps[%d] = {%s};'%(len(CHARS),cps))
def mkfont(name,size,cw,ch,yoff):
  f=ImageFont.truetype('fonts/VCR_OSD_MONO_1.001.ttf',size)
  bits=[]
  sheet=Image.new('L',(cw*len(CHARS),ch))
  for i,chh in enumerate(CHARS):
    im=Image.new('L',(cw,ch),0); d=ImageDraw.Draw(im); d.text((0,yoff),chh,font=f,fill=255)
    a=np.asarray(im)>110
    sheet.paste(im,(i*cw,0))
    for row in a:
      v=0
      for x,b in enumerate(row):
        if b: v|=1<<x
      bits.append(v)
  out.append('static const uint%d_t %s[%d] = {%s};'%(16 if cw<=16 else 32,name,len(bits),','.join(str(b) for b in bits)))
  sheet.save('preview_'+name+'.png')
mkfont('font_s',16,10,14,-1)
mkfont('font_l',32,19,28,-1)
# --- Icône de l'application 55x56 (LZ4) ---
ic=Image.fromarray(np.clip(W0[0:56,0:55],0,255).astype(np.uint8)).convert('RGBA')
ic=Image.blend(ic,Image.new('RGBA',ic.size,(0,0,0,255)),0.35)
fm=Image.fromarray((frames[0]).astype(np.uint8),'RGBA').crop((8,0,56,44)).resize((48,44),Image.LANCZOS)
ic.paste(fm,(4,2),fm)
dd=ImageDraw.Draw(ic); fnt=ImageFont.truetype('fonts/VCR_OSD_MONO_1.001.ttf',14)
dd.rectangle((0,43,55,56),fill=(0,0,0,255)); dd.text((4,42),"B'96",font=fnt,fill=(255,255,255,255))
ic.save('preview_icon.png')
px=np.asarray(ic.convert('RGB'),dtype=np.uint16)
v=((px[...,0]>>3)<<11)|((px[...,1]>>2)<<5)|(px[...,2]>>3)
raw=bytearray()
for x in v.ravel(): raw+=bytes([int(x)&255,int(x)>>8])
n=len(raw); blk=bytearray([0xF0]); r=n-15
while r>=255: blk.append(255); r-=255
blk.append(r); blk+=raw
emit8('eadk_app_icon',blk,'.rodata.eadk_app_icon')
open(sys.argv[1],'w').write('// Généré automatiquement depuis les ressources de Backrooms \'96\n#include <stdint.h>\n'+'\n'.join(out)+'\n')
print('ok',len(CHARS))
