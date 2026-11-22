// Matrices V2 - assistant hors ligne STI2D / maths / calculs pour NumWorks
// Clavier on-screen AZERTY + numerique + symboles, menu burger, suggestions, reflexion
#include "eadk.h"
#include "mini.h"
#include <stdbool.h>
#include <stdint.h>
#include "kb.h"
#include "qr_data.h"

#if !NP_MODULE
const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Matrices";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;
#endif

#define RGB(r,g,b) ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3)))
typedef struct { uint16_t bg,fg,mut,acc,pbg; const char *name; } Theme;
static const Theme TH[] = {
  {RGB(214,226,240),RGB(15,30,60),  RGB(90,115,150), RGB(0,170,230),  RGB(245,250,255),"Futuriste (glass)"},
  {RGB(4,4,14),     RGB(255,230,0), RGB(0,200,200),  RGB(255,0,140),  RGB(6,6,18),     "Cyberpunk"},
  {RGB(110,80,50),  RGB(255,255,255),RGB(200,200,200),RGB(90,200,60), RGB(58,42,28),   "Minecraft"},
  {0xFFFF,          0x0000,         0x8410,          0x4208,          0xEF7D,          "Classique clair"},
  {0x0000,          0xFFFF,         0xC618,          0x07FF,          0x0000,          "Classique sombre"},
  {RGB(255,255,255),RGB(0,0,0),     RGB(110,110,110),RGB(220,0,0),    RGB(255,255,255),"Manga"},
  {RGB(0,0,0),      RGB(0,255,70),  RGB(0,140,40),   RGB(0,255,70),   RGB(0,12,0),     "Hackeur"},
  // themes bonus (deverrouilles par sti2d)
  {RGB(40,50,30),   RGB(200,220,160),RGB(120,140,90), RGB(180,200,60), RGB(50,60,40),   "Militaire"},
  {RGB(180,30,30),  RGB(255,255,255),RGB(255,200,100),RGB(255,220,0),  RGB(200,40,40),  "Mario"},
  {RGB(255,180,200),RGB(80,20,60),  RGB(200,100,140),RGB(255,100,150),RGB(255,220,230),"Girly"},
  {RGB(255,230,240),RGB(180,80,120),RGB(220,150,180),RGB(255,150,200),RGB(255,240,245),"Kawaii"},
  {RGB(255,200,220),RGB(100,40,80), RGB(220,120,160),RGB(255,105,180),RGB(255,230,240),"Hello Kitty"},
};
#define NTHEME_BASE 7
#define NTHEME_MAX 12
static int ntheme = NTHEME_BASE;  // augmente quand sti2d trouve
#define NTHEME ntheme

static int theme=3;
static bool cheat_mode=false;
static bool short_explain=true;
static int persona=0; // 0=Classique 1=E-Girl 2=Bonhomme 3=Hacker 4=Prof
static const char *PERSONA_N[]={"Classique","E-Girl","Bonhomme","Hacker","Prof"};
static int kb_pos=0; // 0=bas, 1=plus haut (volant)
static bool sti2d_unlocked=false;
#define C_BG  (TH[theme].bg)
#define C_FG  (TH[theme].fg)
#define C_MUT (TH[theme].mut)
#define C_ACC (TH[theme].acc)
#define C_PBG (TH[theme].pbg)
#define COLS  42
#define QLEN  48
#define ALEN  280
#define NEX   8
#define NCONV 5

/* ---------- Calcul scientifique (sans libm) ---------- */
#define PI 3.14159265358979
static const char *cp; static bool cerr;
static double expr(void);
static void sp(void){ while(*cp==' ') cp++; }
static double fabs_(double x){ return x<0?-x:x; }
static double sqrt_(double x){ if(x<0){ cerr=true; return 0; } if(x==0) return 0; double g=x>1?x/2:1; for(int i=0;i<60;i++) g=(g+x/g)/2; return g; }
static double exp_(double x){ if(fabs_(x)>700){ cerr=true; return 0; } int k=0; while(fabs_(x)>0.5){ x/=2; k++; }
  double t=1,s=1; for(int i=1;i<14;i++){ t*=x/i; s+=t; } while(k--) s*=s; return s; }
static double ln_(double x){ if(x<=0){ cerr=true; return 0; } int k=0; while(x>1.5){ x/=2; k++; } while(x<0.75){ x*=2; k--; }
  double y=(x-1)/(x+1), y2=y*y, s=0, t=y; for(int i=1;i<20;i+=2){ s+=t/i; t*=y2; } return 2*s + k*0.693147180559945; }
static double sin_(double x){ x=x- (int)(x/(2*PI))*2*PI; if(x>PI) x-=2*PI; if(x<-PI) x+=2*PI;
  double s=0,t=x; for(int i=1;i<14;i++){ s+=t; t*=-x*x/((2*i)*(2*i+1)); } return s; }
static double pow_(double a,double b){ long n=(long)b;
  if(b==(double)n && n>=-60 && n<=60){ double r=1; long m=n<0?-n:n; while(m--) r*=a; if(n<0){ if(r==0){ cerr=true; return 0; } return 1/r; } return r; }
  if(a<=0){ cerr=true; return 0; } return exp_(b*ln_(a)); }
static double atom(void){
  sp();
  if(*cp=='('){ cp++; double v=expr(); sp(); if(*cp==')') cp++; else cerr=true; return v; }
  if(*cp=='-'){ cp++; return -atom(); }
  if(*cp>='a'&&*cp<='z'){ char nm[6]; int n=0; while(*cp>='a'&&*cp<='z'){ if(n<5) nm[n]=*cp; n++; cp++; }
    if(n>5){ cerr=true; return 0; } nm[n]=0;
    if(!m_strcmp(nm,"pi")) return PI;
    double a=atom();
    if(!m_strcmp(nm,"sqrt")) return sqrt_(a);
    if(!m_strcmp(nm,"ln")) return ln_(a);
    if(!m_strcmp(nm,"log")) return ln_(a)/2.302585092994046;
    if(!m_strcmp(nm,"exp")) return exp_(a);
    if(!m_strcmp(nm,"abs")) return fabs_(a);
    if(!m_strcmp(nm,"sin")) return sin_(a);
    if(!m_strcmp(nm,"cos")) return sin_(a+PI/2);
    if(!m_strcmp(nm,"tan")){ double c=sin_(a+PI/2); if(fabs_(c)<1e-12){ cerr=true; return 0; } return sin_(a)/c; }
    cerr=true; return 0; }
  if((*cp<'0'||*cp>'9')&&*cp!='.'){ cerr=true; return 0; }
  double v=0; while(*cp>='0'&&*cp<='9'){ v=v*10+(*cp-'0'); cp++; }
  if(*cp=='.'||*cp==','){ cp++; double f=0.1; while(*cp>='0'&&*cp<='9'){ v+=(*cp-'0')*f; f/=10; cp++; } }
  return v;
}
static double pw(void){ sp(); if(*cp=='-'){ cp++; return -pw(); } double v=atom(); sp(); if(*cp=='^'){ cp++; return pow_(v,pw()); } return v; }
static double term(void){
  double v=pw();
  for(;;){ sp();
    if(*cp=='*'){ cp++; v*=pw(); }
    else if(*cp=='/'){ cp++; double d=pw(); if(d==0){ cerr=true; return 0; } v/=d; }
    else return v; }
}
static double expr(void){
  double v=term();
  for(;;){ sp();
    if(*cp=='+'){ cp++; v+=term(); }
    else if(*cp=='-'){ cp++; v-=term(); }
    else return v; }
}
static void fmt(double v, char *o){
  char t[24]; int n=0, i=0; bool neg=v<0; if(neg) v=-v;
  if(v>4e9){ m_strcpy(o,"Nombre trop grand."); return; }
  unsigned long ip=(unsigned long)v; double fr=v-ip; unsigned long f=(unsigned long)(fr*1000000+0.5);
  if(f>=1000000){ ip++; f=0; }
  if(neg&&(ip>0||f>0)) o[i++]='-';
  if(ip==0) t[n++]='0';
  while(ip>0){ t[n++]='0'+(ip%10); ip/=10; }
  while(n>0) o[i++]=t[--n];
  if(f>0){ char d[6]; for(int k=5;k>=0;k--){ d[k]='0'+(f%10); f/=10; }
    int e=6; while(e>0&&d[e-1]=='0') e--; o[i++]='.'; for(int k=0;k<e;k++) o[i++]=d[k]; }
  o[i]=0;
}
static void appn(char *o,const char *lab,double v,const char *unit){ char t[32]; m_strcat(o,lab); fmt(v,t); m_strcat(o,t); m_strcat(o,unit); }

/* ---------- Moteur de reponse (matching plus strict pour reponses uniques) ---------- */
static bool fname(const char *w,int n){ static const char *F[]={"pi","sqrt","sin","cos","tan","ln","log","exp","abs"};
  for(int i=0;i<9;i++) if((int)m_strlen(F[i])==n && !m_memcmp(F[i],w,n)) return true; return false; }
static bool is_calc(const char *q){
  bool dig=false, op=false;
  for(const char *p=q;*p;){
    if(*p>='a'&&*p<='z'){ const char *w=p; while(*p>='a'&&*p<='z') p++; if(!fname(w,(int)(p-w))) return false; dig=true; op=true; continue; }
    if(*p>='0'&&*p<='9') dig=true; if(*p=='+'||*p=='-'||*p=='*'||*p=='/'||*p=='^') op=true; p++; }
  return dig&&op;
}
static bool starts(const char *q,const char *w){ int n=m_strlen(w); return !m_strncmp(q,w,n) && (q[n]==' '||q[n]==0); }
static int wlen(const char *s){ int n=0; while(s[n]&&s[n]!=' ') n++; return n; }
static bool circuit(const char *q,char *out){
  double U=0,I=0,R=0,P=0; bool hu=0,hi=0,hr=0,hp=0; int kn=0;
  for(const char *p=q;*p;p++){
    if((p==q||p[-1]==' ') && (*p=='u'||*p=='i'||*p=='r'||*p=='p') && p[1]=='='){
      cp=p+2; cerr=false; double v=atom(); if(cerr) return false;
      if(*p=='u'&&!hu){ U=v; hu=true; kn++; } else if(*p=='i'&&!hi){ I=v; hi=true; kn++; }
      else if(*p=='r'&&!hr){ R=v; hr=true; kn++; } else if(*p=='p'&&!hp){ P=v; hp=true; kn++; } } }
  if(kn<2) return false;
  cerr=false;
  if(hu&&hi){ if(I==0){ cerr=true; } else { R=U/I; P=U*I; } }
  else if(hu&&hr){ if(R==0){ cerr=true; } else { I=U/R; P=U*U/R; } }
  else if(hi&&hr){ U=R*I; P=R*I*I; }
  else if(hu&&hp){ if(U==0){ cerr=true; } else { I=P/U; R=U*U/P; } }
  else if(hi&&hp){ if(I==0){ cerr=true; } else { U=P/I; R=P/(I*I); } }
  else { if(R<=0||P<0){ cerr=true; } else { U=sqrt_(P*R); I=sqrt_(P/R); } }
  if(cerr){ m_strcpy(out,"Valeurs impossibles (division par zero ou negatif)."); return true; }
  out[0]=0; appn(out,"U=",U," V  "); appn(out,"I=",I," A  "); appn(out,"R=",R," ohm  "); appn(out,"P=",P," W");
  return true;
}
static bool wordat(const char *h,const char *n,int nl){
  for(const char *p=h;*p;p++) if((p==h||p[-1]==' ') && !m_strncmp(p,n,nl) && (p[nl]==0||p[nl]==' '||(p[nl]=='s'&&(p[nl+1]==0||p[nl+1]==' ')))) return true;
  return false;
}
static bool capital_q(const char *q,char *out){
  if(!wordat(q,"capitale",8) && !wordat(q,"capital",7)) return false;
  char qn[QLEN+1]; int i=0; for(const char *p=q;*p&&i<QLEN;p++){ char c=*p; if(c>='A'&&c<='Z') c+=32; qn[i++]=c; } qn[i]=0;
  for(unsigned i=0;i<CAP_N;i++){
    int k=m_strlen(CAP[i].key); if(k>=3 && wordat(qn,CAP[i].key,k)){ m_strcpy(out,CAP[i].name); m_strcat(out," : capitale = "); m_strcat(out,CAP[i].cap); m_strcat(out,"."); return true; }
    char c[32]; m_strcpy(c,CAP[i].cap); for(int j=0;c[j];j++) if(c[j]>='A'&&c[j]<='Z') c[j]+=32;
    k=m_strlen(c); while(k>0&&c[k-1]==' ') k--; c[k]=0;
    if(k>=3 && wordat(qn,c,k)){ m_strcpy(out,CAP[i].cap); m_strcat(out," est la capitale de "); m_strcat(out,CAP[i].name); m_strcat(out,"."); return true; } }
  return false;
}

static void apply_persona(char *out){
  if(!out[0] || persona==0) return;
  char tmp[ALEN];
  if(persona==1){ // E-Girl
    m_strcpy(tmp, "* "); m_strcat(tmp, out); m_strcat(tmp, " uwu");
    m_strcpy(out, tmp);
  } else if(persona==2){ // Bonhomme
    m_strcpy(tmp, "Hola! "); m_strcat(tmp, out);
    m_strcpy(out, tmp);
  } else if(persona==3){ // Hacker
    m_strcpy(tmp, "> "); m_strcat(tmp, out);
    m_strcpy(out, tmp);
  } else if(persona==4){ // Prof
    m_strcpy(tmp, "Bien. "); m_strcat(tmp, out);
    m_strcpy(out, tmp);
  }
}

static void answer(const char *q, char *out){
  out[0]=0;
  // --- Traduction rapide ---
  // "traduit X en italien/anglais/anglais"
  {
    const char *p = q;
    // detecte "traduit " ou "translate "
    int is_tr = 0;
    if(p[0]=='t'||p[0]=='T'){
      if((p[1]=='r'||p[1]=='R')&&(p[2]=='a'||p[2]=='A')&&(p[3]=='d'||p[3]=='D')) is_tr=1;
      if((p[1]=='r'||p[1]=='R')&&(p[2]=='a'||p[2]=='A')&&(p[3]=='n'||p[3]=='N')) is_tr=1;
    }
    if(is_tr){
      // table mini
      static const char *fr[]={"bonjour","salut","merci","au revoir","oui","non","s il te plait","je t aime","eau","pain","ecole","livre","bonjour !","ca va"};
      static const char *it[]={"buongiorno","ciao","grazie","arrivederci","si","no","per favore","ti amo","acqua","pane","scuola","libro","ciao !","come stai"};
      static const char *en[]={"hello","hi","thank you","goodbye","yes","no","please","i love you","water","bread","school","book","hello !","how are you"};
      int to_it=0, to_en=0;
      // cherche "italien" / "anglais" / "english" dans q
      for(int i=0;q[i];i++){
        if((q[i]=='i'||q[i]=='I')&&(q[i+1]=='t'||q[i+1]=='T')&&(q[i+2]=='a'||q[i+2]=='A')) to_it=1;
        if((q[i]=='a'||q[i]=='A')&&(q[i+1]=='n'||q[i+1]=='N')&&(q[i+2]=='g'||q[i+2]=='G')) to_en=1;
        if((q[i]=='e'||q[i]=='E')&&(q[i+1]=='n'||q[i+1]=='N')&&(q[i+2]=='g'||q[i+2]=='G')) to_en=1;
      }
      // extrait le mot a traduire (apres traduit/translate, avant "en")
      char word[32]; word[0]=0;
      // saute le premier mot
      while(*p && *p!=' ') p++; while(*p==' ') p++;
      int wi=0;
      while(*p && *p!=' ' && wi<30){ word[wi++]=*p++; } word[wi]=0;
      // lowercase simple
      for(int i=0;word[i];i++) if(word[i]>='A'&&word[i]<='Z') word[i]+=32;
      for(int i=0;i<14;i++){
        if(!m_strcmp(word,fr[i])){
          if(to_it){ m_strcpy(out, it[i]); return; }
          if(to_en){ m_strcpy(out, en[i]); return; }
        }
        if(!m_strcmp(word,en[i])){
          if(to_it){ m_strcpy(out, it[i]); return; }
          // en->fr
          m_strcpy(out, fr[i]); return;
        }
        if(!m_strcmp(word,it[i])){
          if(to_en){ m_strcpy(out, en[i]); return; }
          m_strcpy(out, fr[i]); return;
        }
      }
      m_strcpy(out, "Mot inconnu. Ex: traduit bonjour en italien");
      return;
    }
  }
  // Easter-eggs
  if(!m_strcmp(q,"sti2d") || !m_strcmp(q,"STI2D")){
    if(!sti2d_unlocked){ sti2d_unlocked=true; ntheme=NTHEME_MAX; m_strcpy(out,"Themes bonus deverrouilles ! Va dans Menu > Themes"); }
    else m_strcpy(out,"Themes bonus deja deverrouilles.");
    return;
  }
  if(!m_strcmp(q,"triche") || !m_strcmp(q,"TRICHE")){
    cheat_mode=true; m_strcpy(out,"Passage en mode Matrices...");
    return;
  }
  // --- Mini-jeux ---
  {
    // detecte 8ball / boule magique
    int is8=0, isde=0, ispile=0, isppc=0;
    for(int i=0;q[i];i++){
      if((q[i]=='8')&&(q[i+1]=='b'||q[i+1]=='B')) is8=1;
      if((q[i]=='b'||q[i]=='B')&&(q[i+1]=='o'||q[i+1]=='O')&&(q[i+2]=='u'||q[i+2]=='U')&&(q[i+3]=='l'||q[i+3]=='L')) is8=1;
      if((q[i]=='d'||q[i]=='D')&&(q[i+1]=='e')&&(q[i+2]==0||q[i+2]==' ')) isde=1;
      if((q[i]=='p'||q[i]=='P')&&(q[i+1]=='i'||q[i+1]=='I')&&(q[i+2]=='l'||q[i+2]=='L')&&(q[i+3]=='e'||q[i+3]=='E')) ispile=1;
      if((q[i]=='p'||q[i]=='P')&&(q[i+1]=='i'||q[i+1]=='I')&&(q[i+2]=='e'||q[i+2]=='E')&&(q[i+3]=='r'||q[i+3]=='R')) isppc=1;
    }
    // hash simple de la question pour "aleatoire" stable
    unsigned h=0; for(const char *t=q;*t;t++) h = h*33 + (unsigned char)(*t);
    if(is8){
      static const char *rep[]={
        "Oui, clairement.","Non, pas vraiment.","Peut-etre...","C'est certain.",
        "N'y compte pas.","Oui, absolument.","Mes sources disent non.","Sans aucun doute.",
        "Repose la question.","Les signes disent oui.","Tres douteux.","Evidemment.",
        "Oui, mais attention.","Je dirais non.","Concentre-toi et redemande."
      };
      m_strcpy(out, "8-Ball: "); m_strcat(out, rep[h%15]); return;
    }
    if(ispile){
      m_strcpy(out, (h&1) ? "Pile !" : "Face !"); return;
    }
    if(isde){
      int n = (h%6)+1; char b[24];
      b[0]='D'; b[1]='e'; b[2]=':'; b[3]=' '; b[4]=(char)('0'+n); b[5]=0;
      m_strcpy(out, b); return;
    }
    if(isppc){
      static const char *c[]={"pierre","papier","ciseaux"};
      m_strcpy(out, "Je joue... "); m_strcat(out, c[h%3]); m_strcat(out, " !"); return;
    }
  }
  // Convertisseur unites simple
  if(starts(q,"conv ") || starts(q,"convert ")){
    const char *p = q + (q[0]=='c' && q[1]=='o' ? 5 : 8);
    while(*p==' ') p++;
    // formats : 12 V en mV   |  3.3 kOhm en Ohm  |  25 C en K
    double v=0; char unit[8]={0}; char target[8]={0};
    // parse naive
    const char *s=p; while(*s>='0'&&*s<='9' || *s=='.' || *s==',') s++;
    // skip for simplicity - basic known
    if(wordat(q,"mv",2) || wordat(q,"millivolt",9)){ m_strcpy(out,"1 V = 1000 mV | 1 mV = 0.001 V"); return; }
    if(wordat(q,"kohm",4) || wordat(q,"kohm",4)){ m_strcpy(out,"1 kOhm = 1000 Ohm | 1 MOhm = 1e6 Ohm"); return; }
    if(wordat(q,"celsius",7) || wordat(q," kelvin",7) || wordat(q," k ",3)){ m_strcpy(out,"T(K) = T(C) + 273.15 | T(C) = T(K) - 273.15"); return; }
    m_strcpy(out,"Ex: conv 5 V en mV | conv 2.2 kOhm | conv 25 C en K"); return;
  }
  // QCM generateur
  if(starts(q,"qcm ")){
    const char *ch = q+4;
    if(wordat(ch,"maille",6) || wordat(ch,"mailles",7)){
      m_strcpy(out,"QCM Mailles: La somme des tensions dans une maille est... A) max B) nulle C) egale a I  -> B nulle (Kirchhoff)");
      return;
    }
    if(wordat(ch,"noeud",5) || wordat(ch,"noeuds",6)){
      m_strcpy(out,"QCM Noeuds: Somme des courants a un noeud = ... A) U B) 0 C) R  -> B 0 (loi des noeuds)");
      return;
    }
    m_strcpy(out,"QCM dispo: qcm mailles | qcm noeuds | qcm ohm | qcm binaire"); return;
  }
  if(starts(q,"racines")||starts(q,"delta")){
    cp=q+(q[0]=='r'?7:5); cerr=false; double a=atom(), b=atom(), c=atom();
    if(cerr||a==0){ m_strcpy(out,"Usage : racines a b c  (ax^2+bx+c=0, a different de 0)"); return; }
    double d=b*b-4*a*c; appn(out,"delta=",d,"");
    if(d<0) m_strcat(out,"  pas de racine reelle");
    else if(d==0) appn(out,"  x=",-b/(2*a),"");
    else { double r=sqrt_(d); appn(out,"  x1=",(-b-r)/(2*a),""); appn(out,"  x2=",(-b+r)/(2*a),""); }
    return; }
  if(starts(q,"bin")||starts(q,"hex")){
    bool hx=(q[0]=='h'); cp=q+3; cerr=false; double v=expr();
    if(cerr||v<0||v>4e9){ m_strcpy(out,"Usage : bin n  ou  hex n  (entier positif)"); return; }
    unsigned long n=(unsigned long)v; char t[40]; int k=0; unsigned base=hx?16:2;
    if(n==0) t[k++]='0'; while(n>0){ unsigned dgt=n%base; t[k++]=dgt<10?'0'+dgt:'A'+dgt-10; n/=base; }
    int i=0; while(k>0) out[i++]=t[--k]; out[i++]=hx?'h':'b'; out[i]=0; return; }
  if(starts(q,"debin")||starts(q,"dehex")){
    bool hx=(q[2]=='h'); const char *p=q+5; while(*p==' ') p++; unsigned long n=0; int cnt=0;
    for(;*p;p++,cnt++){ int dgt=(*p>='0'&&*p<='9')?*p-'0':(*p>='a'&&*p<='f')?*p-'a'+10:99;
      if(dgt>=(hx?16:2)||cnt>=8){ cnt=-1; break; } n=n*(hx?16:2)+dgt; }
    if(cnt<=0){ m_strcpy(out,"Usage : debin 1101  ou  dehex ff"); return; }
    fmt((double)n,out); return; }
  { const char *e=q; bool eq=false; for(;*e;e++) if(*e=='=') eq=true;
    if(eq && circuit(q,out)) return; }
  if(is_calc(q)){ cp=q; cerr=false; double v=expr(); sp();
    if(cerr||*cp){ m_strcpy(out,"Calcul invalide."); } else { m_strcpy(out,"= "); fmt(v,out+2); } return; }
  if(capital_q(q,out)) return;
  // Matching strict : score eleve + preference mots longs (reponse unique)
  int best=0, bi=-1, second=0;
  for(unsigned e=0;e<KB_N;e++){ int sc=0;
    const char *t=q; while(*t){ while(*t==' ') t++; int tl=wlen(t); if(!tl) break;
      const char *k=KB[e].kw; while(*k){ while(*k==' ') k++; int kl=wlen(k); if(!kl) break;
        if(tl==kl && !m_memcmp(t,k,tl)) sc+=tl*2;           // exact = fort
        else if(tl>3 && t[tl-1]=='s' && tl-1==kl && !m_memcmp(t,k,kl)) sc+=kl;
        else if(tl>=5 && tl==kl){ int mm=0; for(int i=0;i<tl&&mm<2;i++) if(t[i]!=k[i]) mm++; if(mm==1) sc+=tl; }
        else if(tl>=4 && kl>=4 && !m_memcmp(t,k,4)) sc+=1;
        k+=kl; }
      t+=tl; }
    if(sc>best){ second=best; best=sc; bi=e; }
    else if(sc>second) second=sc;
  }
  // Si le meilleur score est faible ou trop proche du 2e, on reste prudent
  if(bi>=0 && best>=4 && best > second + 1){ m_strncpy(out,KB[bi].ans,ALEN-1); out[ALEN-1]=0; }
  else if(bi>=0 && best>=3){ m_strncpy(out,KB[bi].ans,ALEN-1); out[ALEN-1]=0; }
  else m_strcpy(out,"Je ne sais pas encore. Tape 'aide' pour voir ce que je connais.");
}

/* ---------- Correcteur + suggestions de questions liees ---------- */
static int sg_start(const char *in){ int p=m_strlen(in); while(p>0&&in[p-1]!=' ') p--; return p; }
static int suggest(const char *in, const char **o){
  int p=sg_start(in); const char *pre=in+p; int pl=m_strlen(pre), n=0;
  if(pl<2) return 0;
  for(unsigned i=0;i<NWORDS&&n<3;i++)
    if((int)m_strlen(WORDS[i])>pl && !m_strncmp(WORDS[i],pre,pl)) o[n++]=WORDS[i];
  return n;
}
// Suggestions de questions liees apres reponse (mots-cles de l'entree trouvee)
static int related_n=0;
static char related[3][40];
static void build_related(const char *q){
  related_n=0;
  int best=0, bi=-1;
  for(unsigned e=0;e<KB_N;e++){ int sc=0;
    const char *t=q; while(*t){ while(*t==' ') t++; int tl=wlen(t); if(!tl) break;
      const char *k=KB[e].kw; while(*k){ while(*k==' ') k++; int kl=wlen(k); if(!kl) break;
        if(tl==kl && !m_memcmp(t,k,tl)) sc+=tl*2;
        k+=kl; }
      t+=tl; }
    if(sc>best){ best=sc; bi=e; }
  }
  if(bi<0 || best<3) return;
  // Prendre 2-3 autres entrees qui partagent au moins un mot-cle long
  const char *kw=KB[bi].kw;
  for(unsigned e=0;e<KB_N && related_n<3;e++){
    if((int)e==bi) continue;
    const char *k=kw; bool share=false;
    while(*k){ while(*k==' ') k++; int kl=wlen(k); if(!kl) break;
      if(kl>=4 && wordat(KB[e].kw,k,kl)){ share=true; break; }
      k+=kl; }
    if(share){
      // Forme une question courte a partir du premier mot-cle de l'entree
      const char *p=KB[e].kw; while(*p==' ') p++;
      int i=0; while(*p && *p!=' ' && i<36) related[related_n][i++]=*p++;
      related[related_n][i]=0;
      // Eviter doublons
      bool dup=false; for(int j=0;j<related_n;j++) if(!m_strcmp(related[j],related[related_n])) dup=true;
      if(!dup && i>2) related_n++;
    }
  }
}

/* ---------- Discussion ---------- */
static char hq[NEX][QLEN+1]; static int hn=0;
static int pinned=-1; // index epingle (-1 = aucun)
static void store_save(void){}
static void store_load(void){}
static void conv_add(const char *q){
  // garde toujours la discussion epinglee
  if(hn>=NEX){
    if(pinned==0){ // epinglee en 0 -> on vire la suivante
      m_memmove(hq[1],hq[2],(NEX-2)*(QLEN+1)); hn--;
    } else if(pinned>0){
      m_memmove(hq[0],hq[1],pinned*(QLEN+1));
      m_memmove(hq[pinned],hq[pinned+1],(NEX-pinned-1)*(QLEN+1));
      hn--; pinned--;
    } else {
      m_memmove(hq[0],hq[1],(NEX-1)*(QLEN+1)); hn--;
    }
  }
  m_strcpy(hq[hn++],q);
}
static void conv_reset(void){ hn=0; pinned=-1; related_n=0; }

static void check_easter(const char *q){
  if(!m_strcmp(q,"sti2d") || !m_strcmp(q,"STI2D")){ theme=7; cheat_mode=false; }
  else if(!m_strcmp(q,"triche") || !m_strcmp(q,"TRICHE")){ theme=8; cheat_mode=true; }
}


/* ---------- Clavier on-screen UHQ ---------- */
enum { KB_ALPHA=0, KB_NUM=1, KB_SYM=2 };
static int kb_mode=KB_ALPHA;
static int kx=0, ky=0;          // curseur clavier
static bool shift_on=false;     // majuscules
static bool show_kb=true;       // clavier visible
static int rsel=0;              // selection suggestion post-reponse

// Layouts alphabetiques. Shift bascule maj.
static int kb_lang=0; // 0=AZERTY 1=QWERTY 2=Italiano
static const char *const ROW_AZ[4] = {
  "a z e r t y u i o p",
  "q s d f g h j k l m",
  "w x c v b n , . ' ?",
  "123  MAJ  ESP  DEL  EXE"
};
static const char *const ROW_AZ_UP[4] = {
  "A Z E R T Y U I O P",
  "Q S D F G H J K L M",
  "W X C V B N ; : \" !",
  "123  maj  ESP  DEL  EXE"
};
static const char *const ROW_QW[4] = {
  "q w e r t y u i o p",
  "a s d f g h j k l",
  "z x c v b n m , . ?",
  "123  MAJ  ESP  DEL  EXE"
};
static const char *const ROW_QW_UP[4] = {
  "Q W E R T Y U I O P",
  "A S D F G H J K L",
  "Z X C V B N M ; : !",
  "123  maj  ESP  DEL  EXE"
};
// Italiano (QWERTY + accese communes en ligne basse)
static const char *const ROW_IT[4] = {
  "q w e r t y u i o p",
  "a s d f g h j k l",
  "z x c v b n m a e i",
  "123  MAJ  ESP  DEL  EXE"
};
static const char *const ROW_IT_UP[4] = {
  "Q W E R T Y U I O P",
  "A S D F G H J K L",
  "Z X C V B N M A E I",
  "123  maj  ESP  DEL  EXE"
};
static const char *const ROW_N[4] = {
  "1 2 3 4 5 6 7 8 9 0",
  "+ - * / ^ ( ) = . ,",
  "pi sqrt sin cos ln",
  "ABC  SYM  ESP  DEL  EXE"
};
static const char *const ROW_S[4] = {
  "! @ # $ % & * ( ) _",
  "- + = [ ] { } < > /",
  "\\ | ~ ` ^ \" ' : ;",
  "ABC  123  ESP  DEL  EXE"
};

static int key_count(const char *row){
  int n=0; const char *p=row;
  while(*p){ while(*p==' ') p++; if(!*p) break; n++; while(*p && *p!=' ') p++; }
  return n;
}
static void key_at(const char *row, int idx, char *buf, int blen){
  int n=0; const char *p=row; buf[0]=0;
  while(*p){ while(*p==' ') p++; if(!*p) break;
    if(n==idx){ int i=0; while(*p && *p!=' ' && i<blen-1) buf[i++]=*p++; buf[i]=0; return; }
    while(*p && *p!=' ') p++; n++; }
}
static const char *const *current_rows(void){
  if(kb_mode==KB_NUM) return ROW_N;
  if(kb_mode==KB_SYM) return ROW_S;
  if(kb_lang==1) return shift_on ? ROW_QW_UP : ROW_QW;
  if(kb_lang==2) return shift_on ? ROW_IT_UP : ROW_IT;
  return shift_on ? ROW_AZ_UP : ROW_AZ;
}

/* ---------- Affichage ---------- */
enum { S_HOME, S_CHAT, S_MENU, S_CONF, S_THEME, S_CREDITS, S_HIST, S_KBCFG, S_CHEAT, S_TUTO, S_POPUP, S_QR, S_STYLE };
static int scr=S_HOME, msel=0, ssel=0, vi=0, tsel=0;
static int sg_focus=0;
static char in[QLEN+1];
static bool thinking=false;

static void fill(int x,int y,int w,int h,uint16_t c){ eadk_display_push_rect_uniform((eadk_rect_t){x,y,w,h},c); }
// Noir ou blanc selon la luminance d'une couleur de fond (pour un texte toujours lisible)
static uint16_t lum_on(uint16_t c){ int r=((c>>11)&31)<<3, g=((c>>5)&63)<<2, b=(c&31)<<3;
  int Y=(r*77+g*150+b*29)>>8; return Y>140?0x0000:0xFFFF; }
static uint32_t hs(int x,int y){ uint32_t h=(uint32_t)x*374761393u+(uint32_t)y*668265263u; h=(h^(h>>13))*1274126177u; return h^(h>>16); }
static void disc(int cx,int cy,int r,uint16_t c){
  for(int dy=-r;dy<=r;dy++){ int w=0; while((w+1)*(w+1)+dy*dy<=r*r) w++;
    int x0=cx-w, x1=cx+w, y=cy+dy; if(y<0||y>239||x1<0||x0>319) continue;
    if(x0<0) x0=0; if(x1>319) x1=319; fill(x0,y,x1-x0+1,1,c); } }
static void ln(int x0,int y0,int x1,int y1,int w,uint16_t c){
  int dx=x1-x0, dy=y1-y0, adx=dx<0?-dx:dx, ady=dy<0?-dy:dy, n=adx>ady?adx:ady; if(!n) n=1;
  for(int i=0;i<=n;i+=w){ int x=x0+(int)((long)dx*i/n), y=y0+(int)((long)dy*i/n);
    if(x<0||y<0||x+w>320||y+w>240) continue; fill(x,y,w,w,c); } }
static void sakura(int cx,int cy,int r){
  static const int8_t o[5][2]={{0,-2},{2,-1},{1,2},{-1,2},{-2,-1}};
  uint16_t p=RGB(255,192,208), d=RGB(238,130,160);
  for(int i=0;i<5;i++) disc(cx+o[i][0]*r/2*1,cy+o[i][1]*r/2*1,r,p);
  disc(cx,cy,1,d); }
static void petal(int x,int y){ fill(x,y,3,2,RGB(255,192,208)); fill(x+1,y+2,2,1,RGB(238,130,160)); }
static void bubble(int cx,int cy,int r){
  disc(cx,cy,r,RGB(246,250,255)); disc(cx,cy,r-2,RGB(222,234,248)); disc(cx-r/3,cy-r/3,r/4+1,RGB(255,255,255)); }
static void draw_bg(void){
  switch(theme){
  case 0:
    for(int i=0;i<30;i++) fill(0,i*8,320,8,RGB(190+i*2,204+i*2,226+i*1));
    bubble(36,190,18); bubble(290,72,10); bubble(268,196,22); bubble(70,60,7); bubble(180,215,9);
    break;
  case 1: {
    fill(0,0,320,240,C_BG);
    for(int i=0;i<34;i++) fill(hs(i,1)%320,hs(i,2)%110,1,1,RGB(130,130,210));
    disc(160,112,38,RGB(255,0,140)); disc(160,112,30,RGB(255,96,64));
    fill(0,122,320,118,RGB(6,2,20));
    for(int xb=-240;xb<=560;xb+=70) ln(160,122,xb,239,2,RGB(100,0,130));
    fill(0,122,320,1,RGB(0,200,200));
    break; }
  case 2: {
    static const uint16_t d[4]={RGB(110,80,50),RGB(96,68,42),RGB(124,92,58),RGB(84,58,36)};
    static const uint16_t g[3]={RGB(80,170,50),RGB(64,140,40),RGB(96,190,60)};
    for(int ty=0;ty<15;ty++) for(int tx=0;tx<20;tx++) fill(tx*16,ty*16,16,16,d[hs(tx,ty)%4]);
    for(int tx=0;tx<20;tx++){ fill(tx*16,0,16,16,g[hs(tx,99)%3]); fill(tx*16,0,16,3,RGB(110,200,70)); }
    break; }
  case 5:
    fill(0,0,320,240,C_BG);
    for(int y=0;y<240;y+=8) for(int x=(y/8)%2*4;x<320;x+=8){ int dd=x+(240-y); if(dd<190) fill(x,y,2,2,RGB(150,150,150)); }
    sakura(198,38,3); sakura(232,30,3); sakura(262,22,3);
    break;
  case 6:
    fill(0,0,320,240,C_BG);
    for(int x=0;x<320;x+=8){ int hd=(int)(hs(x,0)%240);
      for(int k=0;k<9;k++) fill(x,(hd-k*8+480)%240,4,5,k==0?RGB(190,255,210):RGB(0,235-k*22,50-k*3)); }
    break;
  case 7: // Militaire
    fill(0,0,320,240,C_BG);
    for(int y=0;y<240;y+=4) for(int x=0;x<320;x+=4) if(((x+y)/4)%3==0) fill(x,y,2,2,RGB(60,70,45));
    break;
  case 8: // Mario
    fill(0,0,320,240,C_BG);
    for(int i=0;i<20;i++) fill(0,i*12,320,6, (i%2)?RGB(200,40,40):RGB(180,30,30));
    break;
  case 9: // Girly
    fill(0,0,320,240,C_BG);
    for(int i=0;i<15;i++) disc(20+i*22, 30+(i%3)*60, 8, RGB(255,150,180));
    break;
  case 10: // Kawaii
    fill(0,0,320,240,C_BG);
    for(int i=0;i<12;i++) disc(30+i*25, 40+(i%4)*45, 6, RGB(255,180,200));
    break;
  case 11: // Hello Kitty
    fill(0,0,320,240,C_BG);
    disc(160,100,40,RGB(255,220,230)); disc(160,90,25,RGB(255,255,255));
    break;
  default: fill(0,0,320,240,C_BG);
  }
}
static void clear(void){ draw_bg(); }
static void panel(int x,int y,int w,int h){
  switch(theme){
  case 0: fill(x,y,w,h,RGB(255,255,255)); fill(x+1,y+1,w-2,h-2,C_ACC); fill(x+2,y+2,w-4,h-4,C_PBG); break;
  case 1: fill(x,y,w,h,RGB(0,170,170)); fill(x+1,y+1,w-2,h-2,C_PBG); break;
  case 2: fill(x,y,w,h,RGB(40,40,40)); fill(x+1,y+1,w-2,h-2,RGB(170,170,170)); fill(x+3,y+3,w-6,h-6,C_PBG); break;
  case 5: fill(x,y,w,h,RGB(0,0,0)); fill(x+3,y+3,w-6,h-6,C_PBG); break;
  case 6: fill(x,y,w,h,RGB(0,160,50)); fill(x+1,y+1,w-2,h-2,C_PBG); break;
  default: fill(x,y,w,h,C_PBG);
  }
}
static void textbg(const char *s,int x,int y,bool big,uint16_t c,uint16_t bg){
  eadk_display_draw_string(s,(eadk_point_t){x,y},big,c,bg); }
static void text(const char *s,int x,int y,bool big,uint16_t c){ textbg(s,x,y,big,c,C_PBG); }
static int wrap(const char *s,int y,uint16_t c){
  int n=m_strlen(s), i=0; char l[COLS+1];
  while(i<n){ int len=n-i>COLS?COLS:n-i;
    if(i+len<n){ int b=len; while(b>0&&s[i+b]!=' ') b--; if(b>0) len=b; }
    for(int q=0;q<len;q++) l[q]=s[i+q]; l[len]=0; text(l,8,y,false,c); y+=12; i+=len; while(s[i]==' ') i++; }
  return y;
}
static void header(void){
  fill(0,0,320,19,C_PBG);
  fill(0,0,320,2,C_ACC);          // liseré d'accent en haut
  fill(0,18,320,1,C_MUT);         // separateur bas
  text(".gg/STi2D IA",8,5,false,C_FG);
  // petite icone burger a droite
  fill(294,5,16,2,C_FG); fill(294,9,16,2,C_FG); fill(294,13,16,2,C_FG);
}

/* ---------- Clavier dessin ---------- */
// Une touche pleine, coins adoucis, etat selectionne net. EXE toujours mis en avant.
static void draw_key(int x,int y,int w,int h,const char *lab,bool on,bool accent){
  uint16_t cap = (on||accent) ? C_ACC : C_PBG;
  uint16_t brd = on ? C_FG : C_MUT;
  uint16_t txt = (on||accent) ? lum_on(cap) : C_FG;
  // corps (coins coupes de 1px)
  fill(x+1, y,   w-2, h,   cap);
  fill(x,   y+1, w,   h-2, cap);
  // bord
  fill(x+1, y,     w-2, 1, brd); fill(x+1, y+h-1, w-2, 1, brd);
  fill(x,   y+1,   1, h-2, brd); fill(x+w-1, y+1,  1, h-2, brd);
  int tw = (int)m_strlen(lab)*7;
  int tx = x + (w-tw)/2; if(tx<x+2) tx=x+2;
  textbg(lab, tx, y + (h-11)/2, false, txt, cap);
}
static void draw_keyboard(void){
  if(!show_kb) return;
  const char *const *rows = current_rows();
  // kb_pos: 0 = bas fixe, 1 = plus haut (volant)
  int base_y = (kb_pos==0) ? 150 : 100;
  int kh = 20;
  // panneau du clavier (fond uni -> plus de texte qui "bave" sur le theme)
  fill(0, base_y-4, 320, 4*22+2, C_BG);
  fill(0, base_y-4, 320, 1, C_ACC);
  for(int r=0;r<4;r++){
    int nc = key_count(rows[r]);
    int kw = 314 / (nc > 0 ? nc : 1);
    if(kw > 54) kw = 54;
    int startx = (320 - nc*kw)/2;
    int y = base_y + r*22;
    for(int c=0;c<nc;c++){
      char kbuf[12]; key_at(rows[r], c, kbuf, 12);
      int x = startx + c*kw;
      bool on = (c==kx && r==ky);
      bool accent = (!m_strcmp(kbuf,"EXE")||!m_strcmp(kbuf,"exe"));
      draw_key(x, y, kw-2, kh, kbuf, on, accent);
    }
  }
}

static void draw_home(void){
  clear();
  // carte centrale avec liseré d'accent
  int cw=252, ch=96, cx=(320-cw)/2, cy=52;
  fill(cx-2, cy-2, cw+4, ch+4, C_ACC);
  fill(cx, cy, cw, ch, C_PBG);
  text(".gg/STi2D IA", cx+46, cy+22, true, C_FG);
  fill(cx+28, cy+50, cw-56, 1, C_MUT);
  text("Appuie sur OK pour commencer", cx+24, cy+64, false, C_MUT);
  // pied de page
  int fw=206, fx=(320-fw)/2;
  fill(fx, 196, fw, 22, C_PBG);
  fill(fx, 196, fw, 1, C_MUT);
  text("Matrices  -  by yamii", fx+34, 202, false, C_MUT);
}

static void draw_chat(void){
  clear(); header();
  int kb_top = (kb_pos==0) ? 150 : 100;
  int iy = kb_top - 22;          // champ de saisie
  int sy = iy - 15;              // suggestions correcteur
  int card_top = 19, card_bot = sy - 3;
  // Carte de conversation (fond uni -> texte net, zero bloc parasite)
  fill(0, card_top, 320, card_bot-card_top, C_PBG);
  int y = card_top + 7;
  if(thinking){
    text("IA", 10, y, false, C_MUT);
    text("Reflexion en cours...", 10, y+14, false, C_ACC);
  } else if(hn>0){
    int k=hn-1-vi; if(k<0) k=0;
    static char ans[ALEN];
    answer(hq[k], ans);
    apply_persona(ans);
    textbg(" VOUS ", 10, y, false, lum_on(C_ACC), C_ACC); y=wrap(hq[k], y+14, C_FG)+5;
    textbg(" IA ",   10, y, false, lum_on(C_ACC), C_ACC); y=wrap(ans, y+14, C_FG)+2;
    // Suggestions de questions liees (cliquables)
    if(related_n>0 && vi==0){
      y+=3;
      for(int i=0;i<related_n && y<card_bot-13;i++){
        bool on=(i==rsel);
        char lab[44]; lab[0]='>'; lab[1]=' '; m_strncpy(lab+2, related[i], 40); lab[42]=0;
        textbg(lab, 10, y, false, on?lum_on(C_ACC):C_ACC, on?C_ACC:C_PBG);
        y+=13;
      }
    }
  } else {
    text("Ecris une question puis EXE.", 10, y, false, C_FG);
    text("Fleches = naviguer   OK = taper", 10, y+16, false, C_MUT);
    text("Back = menu   Shift = majuscules", 10, y+30, false, C_MUT);
  }
  // Suggestions correcteur (bande au-dessus de la saisie)
  {
    const char *sg[3]; int ns=suggest(in,sg);
    fill(0, sy-2, 320, 15, C_BG);
    int x=6;
    for(int i=0;i<ns;i++){
      bool on=(i==ssel);
      textbg(sg[i], x, sy, false, on?lum_on(C_ACC):C_ACC, on?C_ACC:C_BG);
      x += (int)m_strlen(sg[i])*7 + 14;
    }
  }
  // Champ de saisie + bouton relancer
  {
    fill(0, iy-2, 320, 1, C_MUT);
    fill(4, iy, 278, 18, C_MUT);
    fill(5, iy+1, 276, 16, C_BG);
    char l[QLEN+4]; m_strcpy(l, in); int m=m_strlen(l);
    if(m<QLEN){ l[m]='_'; l[m+1]=0; }
    textbg(l, 9, iy+4, false, C_FG, C_BG);
    fill(286, iy, 30, 18, C_ACC);
    textbg("R", 297, iy+4, false, lum_on(C_ACC), C_ACC);
  }
  draw_keyboard();
}

static void draw_menu(void){
  clear(); header();
  panel(4,22,312,210);
  text("MENU",12,28,false,C_MUT);
#if NP_MODULE
  text("Back = retour a STI2D-Play",120,28,false,C_MUT);
#endif
  static const char *it[]={
    "Reprendre",
    "Themes",
    "Clavier",
    "Style IA",
    "Historique",
    "Tutoriel",
    "Credits",
    "Reset"
  };
  for(int i=0;i<8;i++){
    bool on=(i==msel);
    textbg(it[i], 16, 44+i*20, false, on?C_PBG:C_FG, on?C_FG:C_PBG);
  }
}

static void draw_conf(void){
  clear(); header(); panel(4,40,312,80);
  text("Effacer toute la discussion ?",12,55,false,C_FG);
  text("OK = oui     Retour = non",12,85,false,C_MUT);
}

static void draw_theme(void){
  clear(); header(); panel(4,22,312,210);
  text("Direction artistique (OK = choisir)",10,28,false,C_MUT);
  for(int i=0;i<NTHEME;i++){
    char l[32]; l[0]=(i==theme)?'*':' '; l[1]=' '; m_strncpy(l+2,TH[i].name,28); l[30]=0;
    bool on=(i==tsel);
    textbg(l, 12, 46+i*18, false, on?C_PBG:C_FG, on?C_FG:C_PBG);
  }
}

static void draw_credits(void){
  clear(); header(); panel(4,22,312,210);
  text("CREDITS",12,28,false,C_ACC);
  text("Imagine et developpe",12,50,false,C_FG);
  text("entierement par yamii.",12,64,false,C_FG);
  text("Discord : cjnw (yamii)",12,88,false,C_FG);
  text("Serveur : discord.gg/sti2d",12,102,false,C_FG);
  text("Merci a toutes les personnes",12,126,false,C_MUT);
  text("qui ont contribue a faire",12,140,false,C_MUT);
  text("vivre cette IA  <3",12,154,false,C_MUT);
  text("discord.gg/sti2d",12,170,false,C_ACC);
  // Vrai QR code (discord.gg/sti2d)
  for(int y=0;y<41;y++) for(int x=0;x<41;x++){
    if(QR[y][x]) fill(230+x, 28+y, 1, 1, C_FG);
  }
}

static void draw_hist(void){
  clear(); header();
  fill(0,20,320,1,C_MUT);
  text("HISTORIQUE",12,28,false,C_ACC);
  text("OK=ouvrir  Shift=epingler",12,44,false,C_MUT);
  if(hn==0) text("Aucune discussion.",16,80,false,C_FG);
  else {
    for(int i=0;i<hn && i<5;i++){
      int idx = hn-1-i;
      char lab[52];
      lab[0]=(idx==pinned)?'*':' ';
      lab[1]=' ';
      lab[2]= (char)('1'+i);
      lab[3]='.';
      lab[4]=' ';
      int j=0; while(hq[idx][j] && j<40){ lab[5+j]=hq[idx][j]; j++; }
      lab[5+j]=0;
      bool on=(i==msel);
      textbg(lab, 10, 65+i*22, false, on?C_PBG:C_FG, on?C_FG:C_PBG);
    }
  }
  if(pinned>=0) text("* = discussion epinglee",12,200,false,C_ACC);
}

static void draw_kbcfg(void){
  clear(); header(); panel(4,22,312,210);
  text("CLAVIER",12,28,false,C_ACC);
  char o[8][32];
  m_strcpy(o[0], kb_mode==0?"* Mode : Lettres":"  Mode : Lettres");
  m_strcpy(o[1], kb_mode==1?"* Mode : Numerique":"  Mode : Numerique");
  m_strcpy(o[2], kb_mode==2?"* Mode : Symboles":"  Mode : Symboles");
  m_strcpy(o[3], kb_lang==0?"* Disposition : AZERTY":"  Disposition : AZERTY");
  m_strcpy(o[4], kb_lang==1?"* Disposition : QWERTY":"  Disposition : QWERTY");
  m_strcpy(o[5], kb_pos==0?"* Position : bas fixe":"  Position : bas fixe");
  m_strcpy(o[6], kb_pos==1?"* Position : plus haut":"  Position : plus haut");
  m_strcpy(o[7], "  Retour");
  for(int i=0;i<8;i++){
    bool on=(i==msel);
    textbg(o[i], 16, 48+i*20, false, on?C_PBG:C_FG, on?C_FG:C_PBG);
  }
}


static void draw_cheat(void){
  // Interface Matrices pure (style NumWorks)
  fill(0,0,320,240,0xFFFF); // fond blanc classique
  fill(0,0,320,20,RGB(230,230,230));
  textbg("Matrices", 10, 4, false, 0x0000, RGB(230,230,230));
  // grille 3x3
  for(int r=0;r<3;r++) for(int c=0;c<3;c++){
    int x=50+c*70, y=50+r*45;
    fill(x,y,60,38,RGB(240,240,245));
    fill(x+1,y+1,58,36,0xFFFF);
    textbg("0", x+24, y+12, false, 0x0000, 0xFFFF);
  }
  textbg("OK = edit   EXE = det", 50, 200, false, RGB(80,80,80), 0xFFFF);
}

static void draw_tuto(void){
  clear(); header();
  panel(4,22,312,210);
  text("TUTORIEL",12,28,false,C_ACC);
  text("Fleches = naviguer clavier",12,48,false,C_FG);
  text("OK = taper   EXE = envoyer",12,64,false,C_FG);
  text("DEL = effacer  ESP = espace",12,80,false,C_FG);
  text("--- Correcteur auto ---",12,100,false,C_ACC);
  text("1. Tape le debut d'un mot",12,116,false,C_FG);
  text("2. Haut (en haut clavier) = suggestions",12,132,false,C_FG);
  text("3. Gauche/Droite = choisir le mot",12,148,false,C_FG);
  text("4. OK = valider la suggestion",12,164,false,C_FG);
  text("5. Bas = revenir au clavier",12,180,false,C_FG);
  text("Back = menu",12,200,false,C_MUT);
}


static void draw_popup(void){
  // garde le fond du theme visible
  draw_bg();
  // fenetre style Windows (petite, cadre)
  fill(40, 60, 240, 120, RGB(200,200,210));
  fill(42, 62, 236, 116, RGB(240,240,245));
  // barre de titre
  fill(42, 62, 236, 18, RGB(0,100,200));
  textbg("Easter egg", 50, 65, false, RGB(255,255,255), RGB(0,100,200));
  // croix
  fill(256, 64, 16, 14, RGB(200,50,50));
  textbg("X", 260, 65, false, RGB(255,255,255), RGB(200,50,50));
  // contenu
  text("gg tu as trouve", 70, 95, false, RGB(20,20,30));
  text("l'easter egg !", 80, 110, false, RGB(20,20,30));
  text("Nouveaux themes offerts", 55, 135, false, RGB(40,40,50));
  text("dans Menu > Themes", 70, 150, false, RGB(40,40,50));
}


static void draw_qr(void){
  fill(0,0,320,240,0xFFFF);
  // QR agrandi (x3) centre
  int ox = (320 - 41*3)/2;
  int oy = (240 - 41*3)/2 - 10;
  for(int y=0;y<41;y++) for(int x=0;x<41;x++){
    if(QR[y][x]) fill(ox+x*3, oy+y*3, 3, 3, 0x0000);
  }
  textbg("Retour pour fermer", 90, 220, false, 0x0000, 0xFFFF);
}


static void draw_style(void){
  clear(); header(); panel(4,22,312,210);
  text("STYLE DE L'IA",12,28,false,C_ACC);
  for(int i=0;i<5;i++){
    char l[36];
    l[0]=(i==persona)?'*':' '; l[1]=' ';
    m_strcpy(l+2, PERSONA_N[i]);
    bool on=(i==msel);
    textbg(l, 16, 55+i*28, false, on?C_PBG:C_FG, on?C_FG:C_PBG);
  }
  text("OK = choisir",16,200,false,C_MUT);
}

static void draw(void){
  if(scr==S_HOME) draw_home();
  else if(scr==S_CHAT) draw_chat();
  else if(scr==S_MENU) draw_menu();
  else if(scr==S_THEME) draw_theme();
  else if(scr==S_CONF) draw_conf();
  else if(scr==S_CREDITS) draw_credits();
  else if(scr==S_HIST) draw_hist();
  else if(scr==S_KBCFG) draw_kbcfg();
  else if(scr==S_CHEAT) draw_cheat();
  else if(scr==S_TUTO) draw_tuto();
  else if(scr==S_POPUP) draw_popup();
  else if(scr==S_QR) draw_qr();
  else if(scr==S_STYLE) draw_style();
}

/* ---------- Action clavier on-screen ---------- */
static void kb_press(void){
  const char *const *rows = current_rows();
  char kbuf[12]; key_at(rows[ky], kx, kbuf, 12);
  int n = m_strlen(in);
  if(!m_strcmp(kbuf,"⏎") || !m_strcmp(kbuf,"EXE") || !m_strcmp(kbuf,"exe")){
    if(n>0){
      while(n>0 && in[n-1]==' ') in[--n]=0;
      // easter / special first
      if(!m_strcmp(in,"triche") || !m_strcmp(in,"TRICHE")){
        cheat_mode=true; in[0]=0; scr=S_CHEAT; return;
      }
      if(!m_strcmp(in,"sti2d") || !m_strcmp(in,"STI2D")){
        if(!sti2d_unlocked){ sti2d_unlocked=true; ntheme=NTHEME_MAX; }
        in[0]=0; scr=S_POPUP; return;
      }
      thinking=true; draw();
      for(volatile int t=0;t<600000;t++){}
      build_related(in);
      conv_add(in); vi=0; rsel=0; in[0]=0; ssel=0; sg_focus=0; thinking=false;
      store_save();
    }
    return;
  }
  if(!m_strcmp(kbuf,"⌫") || !m_strcmp(kbuf,"DEL") || !m_strcmp(kbuf,"del")){
    if(n>0){ in[n-1]=0; }
    return;
  }
  if(!m_strcmp(kbuf,"⎵") || !m_strcmp(kbuf,"ESP") || !m_strcmp(kbuf,"esp")){
    if(n<QLEN){ in[n]=' '; in[n+1]=0; }
    return;
  }
  if(!m_strcmp(kbuf,"123")){ kb_mode=KB_NUM; kx=0; ky=0; return; }
  if(!m_strcmp(kbuf,"ABC") || !m_strcmp(kbuf,"abc")){ kb_mode=KB_ALPHA; kx=0; ky=0; return; }
  if(!m_strcmp(kbuf,"SYM") || !m_strcmp(kbuf,"sym")){ kb_mode=KB_SYM; kx=0; ky=0; return; }
  if(!m_strcmp(kbuf,"MAJ") || !m_strcmp(kbuf,"maj")){ shift_on=!shift_on; return; }
  // Touche caractere (peut etre multi-car comme "pi","sqrt")
  if(!m_strcmp(kbuf,"pi")){ if(n+2<=QLEN){ m_strcat(in,"pi"); } return; }
  if(!m_strcmp(kbuf,"sqrt")){ if(n+4<=QLEN){ m_strcat(in,"sqrt"); } return; }
  if(!m_strcmp(kbuf,"sin")){ if(n+3<=QLEN){ m_strcat(in,"sin"); } return; }
  if(!m_strcmp(kbuf,"cos")){ if(n+3<=QLEN){ m_strcat(in,"cos"); } return; }
  if(!m_strcmp(kbuf,"ln")){ if(n+2<=QLEN){ m_strcat(in,"ln"); } return; }
  // caractere simple
  if(kbuf[0] && !kbuf[1] && n<QLEN){ in[n]=kbuf[0]; in[n+1]=0; }
}

int main(int argc, char *argv[]){
  (void)argc; (void)argv;
  eadk_keyboard_state_t prev=0; store_load();
#if NP_MODULE
  prev=eadk_keyboard_scan(); /* la touche qui a lance Matrices est peut-etre encore enfoncee */
#endif
  in[0]=0; draw();
  for(;;){
    eadk_keyboard_state_t s=eadk_keyboard_scan();
    eadk_keyboard_state_t p=s&~prev; // edge only: 1 appui = 1 action
    prev=s;
    #define PR(k) eadk_keyboard_key_down(p,k)
    bool d=false;
    if(scr==S_HOME){
      if(PR(eadk_key_ok)||PR(eadk_key_exe)){ scr=S_CHAT; d=true; }
#if NP_MODULE
      if(PR(eadk_key_back)) return 0; /* dans STI2D-Play : retour au launcher */
#endif
    }
    else if(scr==S_CHAT){
      const char *sg[3]; int ns=suggest(in,sg);
      const char *const *rows = current_rows();
      int nc = key_count(rows[ky]);
      // Navigation: suggestions OU clavier
      if(sg_focus && ns>0){
        if(PR(eadk_key_left)){ ssel=(ssel+ns-1)%ns; d=true; }
        if(PR(eadk_key_right)){ ssel=(ssel+1)%ns; d=true; }
        if(PR(eadk_key_up)){ ssel=(ssel+ns-1)%ns; d=true; }
        if(PR(eadk_key_down)){ sg_focus=0; d=true; }
        if(PR(eadk_key_ok)){
          int st=sg_start(in); int wl=m_strlen(sg[ssel]);
          if(st+wl+1<=QLEN){ in[st]=0; m_strcat(in,sg[ssel]); m_strcat(in," "); }
          ssel=0; sg_focus=0; d=true;
        }
      } else {
        if(PR(eadk_key_right)){ kx=(kx+1)% (nc>0?nc:1); d=true; }
        if(PR(eadk_key_left)){ kx=(kx+nc-1)% (nc>0?nc:1); d=true; }
        if(PR(eadk_key_up)){
          if(ns>0 && ky==0){ sg_focus=1; d=true; }
          else if(related_n>0 && ky==0){ rsel=(rsel-1+related_n)%related_n; d=true; }
          else { ky=(ky+3)%4; nc=key_count(rows[ky]); if(kx>=nc) kx=nc-1; d=true; }
        }
        if(PR(eadk_key_down)){
          if(related_n>0 && ky==3){ rsel=(rsel+1)%related_n; d=true; }
          else { ky=(ky+1)%4; nc=key_count(rows[ky]); if(kx>=nc) kx=nc-1; d=true; }
        }
        if(PR(eadk_key_ok)){ kb_press(); d=true; }
      }
      if(PR(eadk_key_backspace)){ int n=m_strlen(in); if(n>0) in[n-1]=0; d=true; }
      // EXE physique = TOUJOURS envoyer (pas dependant du curseur)
      if(PR(eadk_key_exe)){
        int n=m_strlen(in);
        if(related_n>0 && rsel>=0 && rsel<related_n && n==0){
          m_strcpy(in, related[rsel]);
          thinking=true; draw();
          for(volatile int t=0;t<400000;t++){}
          build_related(in); conv_add(in); vi=0; rsel=0; in[0]=0; thinking=false;
          store_save(); d=true;
        } else if(n>0){
          while(n>0 && in[n-1]==' ') in[--n]=0;
          if(!m_strcmp(in,"triche")||!m_strcmp(in,"TRICHE")){ cheat_mode=true; in[0]=0; scr=S_CHEAT; d=true; }
          else if(!m_strcmp(in,"sti2d")||!m_strcmp(in,"STI2D")){
            if(!sti2d_unlocked){ sti2d_unlocked=true; ntheme=NTHEME_MAX; }
            in[0]=0; scr=S_POPUP; d=true;
          } else {
            thinking=true; draw();
            for(volatile int t=0;t<400000;t++){}
            build_related(in); conv_add(in); vi=0; rsel=0; in[0]=0; ssel=0; sg_focus=0; thinking=false;
            store_save(); d=true;
          }
        }
      }
      if(PR(eadk_key_shift)){ shift_on=!shift_on; d=true; }
      if(PR(eadk_key_back)){ scr=S_MENU; msel=0; d=true; }
    }
    else if(scr==S_MENU){
      if(PR(eadk_key_up)&&msel>0){ msel--; d=true; }
      if(PR(eadk_key_down)&&msel<7){ msel++; d=true; }
#if NP_MODULE
      if(PR(eadk_key_back)) return 0; /* menu + Back : retour a STI2D-Play */
#else
      if(PR(eadk_key_back)){ scr=S_CHAT; d=true; }
#endif
      if(PR(eadk_key_ok)||PR(eadk_key_exe)){
        d=true;
        if(msel==0) scr=S_CHAT;
        else if(msel==1){ scr=S_THEME; tsel=theme; }
        else if(msel==2){ scr=S_KBCFG; msel=0; }
        else if(msel==3){ scr=S_STYLE; msel=persona; }
        else if(msel==4){ scr=S_HIST; msel=0; }
        else if(msel==5){ scr=S_TUTO; }
        else if(msel==6) scr=S_CREDITS;
        else if(msel==7) scr=S_CONF;
      }
    }
    else if(scr==S_THEME){
      if(PR(eadk_key_up)&&tsel>0){ tsel--; d=true; }
      if(PR(eadk_key_down)&&tsel<NTHEME-1){ tsel++; d=true; }
      if(PR(eadk_key_ok)||PR(eadk_key_exe)){ theme=tsel; store_save(); scr=S_CHAT; d=true; }
      if(PR(eadk_key_back)){ scr=S_MENU; d=true; }
    }
    else if(scr==S_CONF){
      if(PR(eadk_key_ok)){ conv_reset(); vi=0; store_save(); scr=S_CHAT; d=true; }
      if(PR(eadk_key_back)){ scr=S_MENU; d=true; }
    }
    else if(scr==S_CREDITS){
      if(PR(eadk_key_ok)||PR(eadk_key_exe)){ scr=S_QR; d=true; }
      if(PR(eadk_key_back)){ scr=S_MENU; d=true; }
    }
    else if(scr==S_QR){
      if(PR(eadk_key_back)||PR(eadk_key_ok)){ scr=S_CREDITS; d=true; }
    }
    else if(scr==S_HIST){
      if(PR(eadk_key_up)&&msel>0){ msel--; d=true; }
      if(PR(eadk_key_down)&&msel<hn-1&&msel<4){ msel++; d=true; }
      if(PR(eadk_key_ok)||PR(eadk_key_exe)){
        if(hn>0){ int idx=hn-1-msel; vi=hn-1-idx; related_n=0; scr=S_CHAT; }
        d=true;
      }
      if(PR(eadk_key_shift)&&hn>0){ // epingler
        int idx=hn-1-msel; pinned=(pinned==idx)?-1:idx; d=true;
      }
      if(PR(eadk_key_back)){ scr=S_MENU; d=true; }
    }
    else if(scr==S_KBCFG){
      if(PR(eadk_key_up)&&msel>0){ msel--; d=true; }
      if(PR(eadk_key_down)&&msel<7){ msel++; d=true; }
      if(PR(eadk_key_ok)||PR(eadk_key_exe)){
        if(msel==0){ kb_mode=KB_ALPHA; }
        else if(msel==1){ kb_mode=KB_NUM; }
        else if(msel==2){ kb_mode=KB_SYM; }
        else if(msel==3){ kb_lang=0; kb_mode=KB_ALPHA; }
        else if(msel==4){ kb_lang=1; kb_mode=KB_ALPHA; }
        else if(msel==5){ kb_pos=0; }
        else if(msel==6){ kb_pos=1; }
        else if(msel==7){ scr=S_MENU; msel=0; }
        d=true;
      }
      if(PR(eadk_key_back)){ scr=S_MENU; d=true; }
    }
    else if(scr==S_TUTO){
      if(PR(eadk_key_back) || PR(eadk_key_ok) || PR(eadk_key_exe)){ scr=S_MENU; d=true; }
    }
    else if(scr==S_POPUP){
      if(PR(eadk_key_back) || PR(eadk_key_ok) || PR(eadk_key_exe)){ scr=S_CHAT; d=true; }
    }
    else if(scr==S_CHEAT){
      if(PR(eadk_key_back)){ cheat_mode=false; scr=S_CHAT; d=true; }
    }
    else if(scr==S_STYLE){
      if(PR(eadk_key_up) && msel>0){ msel--; d=true; }
      if(PR(eadk_key_down) && msel<4){ msel++; d=true; }
      if(PR(eadk_key_ok) || PR(eadk_key_exe)){
        persona = msel;
        scr = S_MENU;
        msel = 3; // revenir sur Style IA dans le menu
        d = true;
      }
      if(PR(eadk_key_back)){ scr=S_MENU; msel=3; d=true; }
    }
    if(d) draw();
  }
}
