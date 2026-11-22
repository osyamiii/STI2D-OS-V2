// BACKROOMS '96 - Version NumWorks (FR)
// Jeu original : OllieHallGames
// Portage NumWorks / version FR : cjnw (yamii) - discord.gg/sti2d
#include <eadk.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "assets.h"

const char eadk_app_name[] __attribute__((section(".rodata.eadk_app_name"))) = "Backrooms 96";
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;

// ---------------------------------------------------------------------------
// Outils
// ---------------------------------------------------------------------------
static uint32_t rng = 0x9E3779B9u;
static uint32_t rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static float frnd(void) { return (rnd() & 0xFFFFFF) / 16777216.0f; }
static int irnd(int n) { return (int)(rnd() % (uint32_t)n); }
static float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

static inline uint16_t shade(uint16_t c, uint32_t k) {
  uint32_t x = (c | ((uint32_t)c << 16)) & 0x07E0F81Fu;
  x = ((x * k) >> 5) & 0x07E0F81Fu;
  return (uint16_t)(x | (x >> 16));
}
static inline uint16_t blend(uint16_t a, uint16_t b, uint32_t k) { // k/32 de b
  uint32_t x = (a | ((uint32_t)a << 16)) & 0x07E0F81Fu;
  uint32_t y = (b | ((uint32_t)b << 16)) & 0x07E0F81Fu;
  x = (x + (((y - x) * k) >> 5)) & 0x07E0F81Fu;
  return (uint16_t)(x | (x >> 16));
}
#define RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define TRANSP 0xF81F

// ---------------------------------------------------------------------------
// Carte des Backrooms (générée aléatoirement, comme le générateur infini du jeu)
// ---------------------------------------------------------------------------
#define MW 40
#define MH 40
#define LMW (MW * 2)
#define LMH (MH * 2)
#define C_WALL 0x80
#define C_DAMP 0x10
#define C_BROKEN 0x20
#define C_VAR(c) ((c) & 3)
#define C_CEIL(c) (((c) >> 2) & 3)
enum { CEIL_NORM = 0, CEIL_LIGHT = 1, CEIL_VENT = 2, CEIL_DIRTY = 3 };

static uint8_t cells[MW * MH];
static uint8_t lmap[LMW * LMH];
static uint16_t dist[MW * MH];
static uint16_t bfsq[MW * MH];

typedef struct { uint8_t x, y, face, taken; } Face;
#define NPAGES 10
static Face pages[NPAGES];
static Face door;
static int pagesTaken;
static float startX, startY;

static inline int is_wall(int x, int y) {
  if (x < 0 || y < 0 || x >= MW || y >= MH) return 1;
  return cells[y * MW + x] & C_WALL;
}

static void bfs(int sx, int sy) {
  for (int i = 0; i < MW * MH; i++) dist[i] = 0xFFFF;
  int h = 0, t = 0;
  dist[sy * MW + sx] = 0; bfsq[t++] = sy * MW + sx;
  while (h < t) {
    int c = bfsq[h++]; int x = c % MW, y = c / MW; uint16_t d = dist[c] + 1;
    static const int8_t dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) {
      int nx = x + dx[k], ny = y + dy[k];
      if (is_wall(nx, ny)) continue;
      int n = ny * MW + nx;
      if (dist[n] > d) { dist[n] = d; bfsq[t++] = n; }
    }
  }
}

static int los(float ax, float ay, float bx, float by) {
  float dx = bx - ax, dy = by - ay;
  float len = sqrtf(dx * dx + dy * dy);
  int n = (int)(len * 4) + 1;
  for (int i = 1; i < n; i++) {
    float t = (float)i / n;
    if (is_wall((int)(ax + dx * t), (int)(ay + dy * t))) return 0;
  }
  return 1;
}

static void face_neighbor(const Face *f, int *nx, int *ny) {
  *nx = f->x; *ny = f->y;
  if (f->face == 0) (*nx)--; else if (f->face == 1) (*nx)++; else if (f->face == 2) (*ny)--; else (*ny)++;
}

static void compute_lightmap(void) {
  for (int sy = 0; sy < LMH; sy++)
    for (int sx = 0; sx < LMW; sx++) {
      float wx = (sx + 0.5f) * 0.5f, wy = (sy + 0.5f) * 0.5f;
      float acc = 0;
      int cx = (int)wx, cy = (int)wy;
      for (int ly = cy - 5; ly <= cy + 5; ly++)
        for (int lx = cx - 5; lx <= cx + 5; lx++) {
          if (lx < 0 || ly < 0 || lx >= MW || ly >= MH) continue;
          uint8_t c = cells[ly * MW + lx];
          if ((c & C_WALL) || C_CEIL(c) != CEIL_LIGHT || (c & C_BROKEN)) continue;
          float dx = lx + 0.5f - wx, dy = ly + 0.5f - wy, d2 = dx * dx + dy * dy;
          if (d2 > 30) continue;
          if (!los(lx + 0.5f, ly + 0.5f, wx, wy)) continue;
          acc += 235.0f / (1.0f + d2 * 0.30f);
        }
      lmap[sy * LMW + sx] = (uint8_t)clampf(acc, 0, 255);
    }
}

static void generate_map(uint32_t seed) {
  rng = seed ? seed : 1;
  for (;;) {
    for (int y = 0; y < MH; y++)
      for (int x = 0; x < MW; x++) {
        uint8_t c = 0;
        if (x == 0 || y == 0 || x == MW - 1 || y == MH - 1) c = C_WALL;
        cells[y * MW + x] = c;
      }
    int cxs = MW / 2, cys = MH / 2;
    // pièces
    for (int r = 0; r < 7; r++) {
      int w = 4 + irnd(6), h = 4 + irnd(6), x0 = 1 + irnd(MW - w - 2), y0 = 1 + irnd(MH - h - 2);
      for (int x = x0; x <= x0 + w; x++) { cells[y0 * MW + x] |= C_WALL; cells[(y0 + h) * MW + x] |= C_WALL; }
      for (int y = y0; y <= y0 + h; y++) { cells[y * MW + x0] |= C_WALL; cells[y * MW + x0 + w] |= C_WALL; }
      for (int k = 0; k < 3; k++) { // ouvertures
        int s = irnd(4);
        int x = s < 2 ? x0 + 1 + irnd(w - 1) : (s == 2 ? x0 : x0 + w);
        int y = s >= 2 ? y0 + 1 + irnd(h - 1) : (s == 0 ? y0 : y0 + h);
        if (x > 0 && y > 0 && x < MW - 1 && y < MH - 1) cells[y * MW + x] &= ~C_WALL;
      }
    }
    // segments de murs
    for (int i = 0; i < MW * MH / 13; i++) {
      int x = 1 + irnd(MW - 2), y = 1 + irnd(MH - 2), len = 2 + irnd(6), hor = irnd(2);
      for (int k = 0; k < len; k++) {
        int xx = x + (hor ? k : 0), yy = y + (hor ? 0 : k);
        if (xx >= MW - 1 || yy >= MH - 1) break;
        cells[yy * MW + xx] |= C_WALL;
      }
    }
    // piliers
    for (int i = 0; i < 40; i++) cells[(1 + irnd(MH - 2)) * MW + 1 + irnd(MW - 2)] |= C_WALL;
    for (int y = cys - 1; y <= cys + 1; y++)
      for (int x = cxs - 1; x <= cxs + 1; x++) cells[y * MW + x] &= ~C_WALL;
    bfs(cxs, cys);
    int reach = 0;
    for (int i = 0; i < MW * MH; i++) {
      if (!(cells[i] & C_WALL)) { if (dist[i] == 0xFFFF) cells[i] |= C_WALL; else reach++; }
    }
    if (reach < MW * MH * 45 / 100) continue;
    // décor : variantes de murs, plafond, moquette humide
    for (int y = 0; y < MH; y++)
      for (int x = 0; x < MW; x++) {
        uint8_t *c = &cells[y * MW + x];
        int v = irnd(100);
        *c |= v < 70 ? 0 : (v < 85 ? 1 : 2);
        int ct = CEIL_NORM;
        if (x % 3 == 1 && y % 3 == 1) { ct = CEIL_LIGHT; if (irnd(100) < 14) *c |= C_BROKEN; }
        else { int r = irnd(100); if (r < 4) ct = CEIL_VENT; else if (r < 12) ct = CEIL_DIRTY; }
        *c |= ct << 2;
      }
    for (int b = 0; b < 14; b++) {
      int bx = irnd(MW), by = irnd(MH), r = 1 + irnd(2);
      for (int y = by - r; y <= by + r; y++)
        for (int x = bx - r; x <= bx + r; x++)
          if (x >= 0 && y >= 0 && x < MW && y < MH && (x - bx) * (x - bx) + (y - by) * (y - by) <= r * r) cells[y * MW + x] |= C_DAMP;
    }
    // pages accrochées aux murs
    int ok = 1;
    for (int p = 0; p < NPAGES && ok; p++) {
      int tries = 0, placed = 0;
      while (!placed && tries < 4000) {
        tries++;
        int x = 1 + irnd(MW - 2), y = 1 + irnd(MH - 2), f = irnd(4);
        if (!(cells[y * MW + x] & C_WALL)) continue;
        Face fc = {(uint8_t)x, (uint8_t)y, (uint8_t)f, 0};
        int nx, ny; face_neighbor(&fc, &nx, &ny);
        if (is_wall(nx, ny) || dist[ny * MW + nx] == 0xFFFF || dist[ny * MW + nx] < 6) continue;
        int mind = tries < 2500 ? 7 : 3, bad = 0;
        for (int q = 0; q < p; q++) {
          int ddx = pages[q].x - x, ddy = pages[q].y - y;
          if (ddx * ddx + ddy * ddy < mind * mind) { bad = 1; break; }
        }
        if (bad) continue;
        pages[p] = fc; placed = 1;
      }
      if (!placed) ok = 0;
    }
    if (!ok) continue;
    // porte de sortie : le plus loin possible du départ
    int best = -1;
    for (int y = 1; y < MH - 1; y++)
      for (int x = 1; x < MW - 1; x++) {
        if (!(cells[y * MW + x] & C_WALL)) continue;
        for (int f = 0; f < 4; f++) {
          Face fc = {(uint8_t)x, (uint8_t)y, (uint8_t)f, 0};
          int nx, ny; face_neighbor(&fc, &nx, &ny);
          if (is_wall(nx, ny)) continue;
          int d = dist[ny * MW + nx];
          if (d == 0xFFFF) continue;
          int clash = 0;
          for (int q = 0; q < NPAGES; q++) if (pages[q].x == x && pages[q].y == y) clash = 1;
          if (clash) continue;
          d = d * 4 + irnd(8);
          if (d > best) { best = d; door = fc; }
        }
      }
    if (best < 0) continue;
    startX = cxs + 0.5f; startY = cys + 0.5f;
    break;
  }
  compute_lightmap();
}

// ---------------------------------------------------------------------------
// Rendu
// ---------------------------------------------------------------------------
static int SC = 2; // 1 = haute qualité (320x240), 2 = normale (160x120)
static int RW = 160, RH = 120;
#define BAND_OUT 16
static uint16_t obuf[320 * BAND_OUT];
static uint16_t rbuf[320 * BAND_OUT];
static uint16_t ram_carpet[64 * 64];
static uint16_t ram_ceil[64 * 64];

static float zbuf[320];
static int16_t wtop[320], wbot[320];
static const uint16_t *wcol[320];
static int32_t wty0[320], wtys[320];
static uint8_t wlit[320], wfl[320];
static uint8_t cookx[320];

// caméra
static float px, py, pa, dirx, diry, plx, ply, camH = 0.5f, horOff = 0, proj;
static float power = 1.0f;       // courant des néons (0 = coupure)
static int flashOn = 0;
static int ambient = 40;
static float centerDist; static int centerKind; // ce que le joueur regarde
enum { LOOK_NONE, LOOK_PAGE, LOOK_DOOR };
static int centerPage;

// monstre
static int monActive = 0;
static float mx, my; static int mframe = 0;

static const uint16_t *const wallTex[6] = {tex_wall0, tex_wall1, tex_wall2, tex_wall3, tex_wall4, tex_wall5};
static int doorOpened = 0;

static inline int lm_sample_fx(int32_t fx, int32_t fy) { // coordonnées monde 16.16
  int32_t lx = (fx << 1) - 32768, ly = (fy << 1) - 32768;
  if (lx < 0) lx = 0;
  if (ly < 0) ly = 0;
  int ix = lx >> 16, iy = ly >> 16;
  if (ix > LMW - 2) { ix = LMW - 2; lx = (int32_t)ix << 16 | 0xFFFF; }
  if (iy > LMH - 2) { iy = LMH - 2; ly = (int32_t)iy << 16 | 0xFFFF; }
  int ax = (lx >> 8) & 255, ay = (ly >> 8) & 255;
  const uint8_t *p = &lmap[iy * LMW + ix];
  int t = p[0] + (((p[1] - p[0]) * ax) >> 8);
  int b = p[LMW] + (((p[LMW + 1] - p[LMW]) * ax) >> 8);
  return t + (((b - t) * ay) >> 8);
}
static int lm_sample(float x, float y) { return lm_sample_fx((int32_t)(x * 65536), (int32_t)(y * 65536)); }

static inline int fog256(float d) { float f = 256.0f / (1.0f + d * d * 0.010f); return (int)f; }
static inline int flash256(float d) { if (!flashOn) return 0; return (int)(300.0f / (1.0f + d * d * 0.06f)); }

static int face_tex(int x, int y, int face, int *pageIdx) {
  if (door.x == x && door.y == y && door.face == face) return doorOpened ? 5 : 4;
  for (int i = 0; i < NPAGES; i++)
    if (!pages[i].taken && pages[i].x == x && pages[i].y == y && pages[i].face == face) { *pageIdx = i; return 3; }
  return -1;
}

static void setup_camera(void) {
  dirx = cosf(pa); diry = sinf(pa);
  const float k = 0.66f;
  plx = -diry * k; ply = dirx * k;
  proj = (RW * 0.5f) / k;
}

static void cast_rays(void) {
  float hor = RH * 0.5f + horOff;
  int pw = (int)(proj * 1.0f);
  (void)pw;
  float lp = power;
  centerKind = LOOK_NONE; centerDist = 99;
  for (int x = 0; x < RW; x++) {
    float cam = 2.0f * (x + 0.5f) / RW - 1.0f;
    float rdx = dirx + plx * cam, rdy = diry + ply * cam;
    int mapX = (int)px, mapY = (int)py;
    float ddx = rdx == 0 ? 1e30f : fabsf(1.0f / rdx);
    float ddy = rdy == 0 ? 1e30f : fabsf(1.0f / rdy);
    int stepX, stepY; float sdx, sdy;
    if (rdx < 0) { stepX = -1; sdx = (px - mapX) * ddx; } else { stepX = 1; sdx = (mapX + 1.0f - px) * ddx; }
    if (rdy < 0) { stepY = -1; sdy = (py - mapY) * ddy; } else { stepY = 1; sdy = (mapY + 1.0f - py) * ddy; }
    int side = 0, n = 0;
    for (;;) {
      if (sdx < sdy) { sdx += ddx; mapX += stepX; side = 0; } else { sdy += ddy; mapY += stepY; side = 1; }
      if (is_wall(mapX, mapY) || ++n > 80) break;
    }
    float perp = side == 0 ? sdx - ddx : sdy - ddy;
    if (perp < 0.02f) perp = 0.02f;
    int face = side == 0 ? (stepX > 0 ? 0 : 1) : (stepY > 0 ? 2 : 3);
    float wallX = side == 0 ? py + perp * rdy : px + perp * rdx;
    wallX -= floorf(wallX);
    int texX = (int)(wallX * 128);
    if ((side == 0 && rdx < 0) || (side == 1 && rdy > 0)) texX = 127 - texX;
    texX &= 127;
    int pidx = -1;
    int t = face_tex(mapX, mapY, face, &pidx);
    int v = t >= 0 ? t : C_VAR(cells[mapY * MW + mapX]);
    if (v > 5) v = 0;
    wcol[x] = wallTex[v] + texX * 128;
    zbuf[x] = perp;
    float lineH = proj / perp;
    float top = hor - (1.0f - camH) * lineH, bot = hor + camH * lineH;
    int it = (int)ceilf(top - 0.5f), ib = (int)ceilf(bot - 0.5f);
    int32_t step = (int32_t)(128.0f * 65536.0f / lineH);
    wtys[x] = step;
    wty0[x] = (int32_t)((0.5f - top) * step);
    wtop[x] = it < 0 ? 0 : (it > RH ? RH : it);
    wbot[x] = ib < 0 ? 0 : (ib > RH ? RH : ib);
    // éclairage du mur : point juste devant la surface
    float hx = px + rdx * (perp - 0.04f), hy = py + rdy * (perp - 0.04f);
    int L = lm_sample(hx, hy);
    int fg = fog256(perp);
    int k = (int)((L * fg * lp + ambient * fg) / 2048.0f);
    if (side == 1) k = k * 7 / 8; // léger contraste entre les faces
    wlit[x] = k > 32 ? 32 : k;
    wfl[x] = (uint8_t)(flash256(perp) > 255 ? 255 : flash256(perp));
    cookx[x] = (uint8_t)(x * 80 / RW);
    if (x == RW / 2) {
      centerDist = perp;
      if (t == 3) { centerKind = LOOK_PAGE; centerPage = pidx; }
      else if (t == 4 || t == 5) centerKind = LOOK_DOOR;
    }
  }
}

// sprite du monstre
static int spX0, spX1, spT, spB, spW; static float spDepth, spSX, spH; static int spK, spVis;
static void prepare_sprite(void) {
  spVis = 0;
  if (!monActive) return;
  float sx = mx - px, sy = my - py;
  float inv = 1.0f / (plx * diry - dirx * ply);
  float tx = inv * (diry * sx - dirx * sy);
  float ty = inv * (-ply * sx + plx * sy);
  if (ty < 0.15f) return;
  float hor = RH * 0.5f + horOff;
  spSX = (RW * 0.5f) * (1.0f + tx / ty);
  float lh = proj / ty;
  spH = lh * 0.97f;
  float bot = hor + camH * lh;
  spB = (int)bot; spT = (int)(bot - spH);
  float w = spH * 0.5f;
  spW = (int)w;
  spX0 = (int)(spSX - w * 0.5f); spX1 = spX0 + spW;
  spDepth = ty;
  if (spX1 < 0 || spX0 >= RW || spW <= 0) return;
  int L = lm_sample(mx, my);
  int fg = fog256(ty);
  int k = (int)((L * fg * power + ambient * fg) / 2048.0f);
  if (flashOn) {
    int c = flash_cookie[30 * 80 + (int)clampf(spSX * 80 / RW, 0, 79)];
    k += (c * flash256(ty)) >> 11;
  }
  spK = k > 32 ? 32 : (k < 1 ? 1 : k);
  spVis = 1;
}

enum { BG_SCENE, BG_SOLID, BG_JUMP };
static int bgMode = BG_SCENE;
static uint16_t bgColor = 0;
static int jumpShakeX, jumpShakeY;

static void render_rows(int y0, int nrows, uint16_t *dst) {
  float hor = RH * 0.5f + horOff;
  if (bgMode == BG_SOLID) {
    for (int i = 0; i < nrows * RW; i++) dst[i] = bgColor;
    return;
  }
  if (bgMode == BG_JUMP) {
    for (int r = 0; r < nrows; r++) {
      int y = y0 + r;
      int sy = (y * 120 / RH) + jumpShakeY; if (sy < 0) sy = 0; if (sy > 119) sy = 119;
      for (int x = 0; x < RW; x++) {
        int sx = (x * 160 / RW) + jumpShakeX; if (sx < 0) sx = 0; if (sx > 159) sx = 159;
        dst[r * RW + x] = img_jumpscare[sy * 160 + sx];
      }
    }
    return;
  }
  int amb = ambient;
  int pw = (int)(power * 256);
  for (int r = 0; r < nrows; r++) {
    int y = y0 + r;
    uint16_t *row = dst + r * RW;
    int isFloor = y + 0.5f > hor;
    float p = isFloor ? (y + 0.5f - hor) : (hor - y - 0.5f);
    if (p < 0.25f) p = 0.25f;
    float rowD = (isFloor ? camH : (1.0f - camH)) * proj / p;
    int fg = fog256(rowD);
    int A = (fg * pw) >> 8, B = amb * fg;
    int fl = flash256(rowD); if (fl > 255) fl = 255;
    const uint8_t *ck = &flash_cookie[(y * 60 / RH) * 80];
    float fx0 = px + rowD * (dirx - plx), fy0 = py + rowD * (diry - ply);
    int32_t fx = (int32_t)(fx0 * 65536), fy = (int32_t)(fy0 * 65536);
    int32_t sx = (int32_t)(rowD * 2.0f * plx / RW * 65536), sy = (int32_t)(rowD * 2.0f * ply / RW * 65536);
    fx += sx / 2; fy += sy / 2;
    int lampK = (32 * fg * pw) >> 16;
    for (int x = 0; x < RW; x++, fx += sx, fy += sy) {
      if (y >= wtop[x] && y < wbot[x]) {
        int32_t ty = wty0[x] + y * wtys[x];
        int ti = ty >> 16; if (ti > 127) ti = 127; if (ti < 0) ti = 0;
        uint16_t c = wcol[x][ti];
        int k = wlit[x];
        if (wfl[x]) k += (ck[cookx[x]] * wfl[x]) >> 11;
        row[x] = shade(c, k > 32 ? 32 : k);
        continue;
      }
      int cx = fx >> 16, cy = fy >> 16;
      if ((unsigned)cx >= MW || (unsigned)cy >= MH) { row[x] = 0; continue; }
      uint8_t cell = cells[cy * MW + cx];
      int tx = (fx >> 10) & 63, tyy = (fy >> 10) & 63;
      uint16_t c;
      int k;
      if (isFloor) {
        c = (cell & C_DAMP) ? tex_carpet_damp[tyy * 64 + tx] : ram_carpet[tyy * 64 + tx];
      } else {
        int ct = C_CEIL(cell);
        if (ct == CEIL_LIGHT) {
          c = tex_ceil_light[tyy * 64 + tx];
          if (tx < 32 && tx > 0 && tx != 31) {
            if (cell & C_BROKEN) { row[x] = shade(c, 3 + ((amb * fg) >> 13)); continue; }
            int kk = lampK + 2; row[x] = shade(c, kk > 32 ? 32 : kk); continue;
          }
        } else if (ct == CEIL_NORM) c = ram_ceil[tyy * 64 + tx];
        else if (ct == CEIL_VENT) c = tex_ceil_vent[tyy * 64 + tx];
        else c = tex_ceil_dirty[tyy * 64 + tx];
      }
      int L = lm_sample_fx(fx, fy);
      k = (L * A + B) >> 11;
      if (fl) k += (ck[cookx[x]] * fl) >> 11;
      row[x] = shade(c, k > 32 ? 32 : k);
    }
    // monstre
    if (spVis && y >= spT && y < spB) {
      int ty = (int)((y - spT + 0.5f) * 128.0f / spH);
      if (ty > 127) ty = 127;
      const uint16_t *spr = spr_monster + mframe * 64 * 128;
      int xs = spX0 < 0 ? 0 : spX0, xe = spX1 > RW ? RW : spX1;
      for (int x = xs; x < xe; x++) {
        if (zbuf[x] < spDepth) continue;
        int tx = (x - spX0) * 64 / spW;
        uint16_t c = spr[tx * 128 + ty];
        if (c == TRANSP) continue;
        row[x] = shade(c, spK);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Superposition (HUD VHS, textes, menus)
// ---------------------------------------------------------------------------
#define NCH ((int)sizeof(font_cps) / 2)
typedef struct { int16_t x, y; uint16_t col; uint8_t font, shadow; const char *s; } OvText;
typedef struct { int16_t x, y, w, h; uint16_t col; uint8_t alpha; } OvRect;
static OvText ovt[40]; static int novt;
static OvRect ovr[24]; static int novr;
static int ovPlay = 0, ovPlayX = 10, ovPlayY = 10;
static char pool[1200]; static int poolN;
static int fadeK = 32;      // 32 = normal, 0 = noir
static int whiteK = 0;      // 0 = normal, 32 = blanc
static float glitch = 0;    // intensité de l'effet VHS
static int bsodBand = 0;

static uint32_t utf8_next(const char **s) {
  const uint8_t *p = (const uint8_t *)*s; uint32_t c = *p++;
  if (c >= 0xF0) { c = ((c & 7) << 18) | ((p[0] & 63) << 12) | ((p[1] & 63) << 6) | (p[2] & 63); p += 3; }
  else if (c >= 0xE0) { c = ((c & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63); p += 2; }
  else if (c >= 0xC0) { c = ((c & 31) << 6) | (p[0] & 63); p += 1; }
  *s = (const char *)p; return c;
}
static int glyph_index(uint32_t cp) {
  if (cp >= 32 && cp < 127) return cp - 32;
  for (int i = 95; i < NCH; i++) if (font_cps[i] == cp) return i;
  return '?' - 32;
}
static int text_len(const char *s) { int n = 0; while (*s && *s != '\n') { utf8_next(&s); n++; } return n; }
static int font_w(int f) { return f ? 19 : 10; }
static int font_h(int f) { return f ? 28 : 14; }

static char *pstr(const char *s) {
  int n = strlen(s) + 1; if (poolN + n > (int)sizeof(pool)) return (char *)"";
  char *d = pool + poolN; memcpy(d, s, n); poolN += n; return d;
}
static char *itoa2(int v, char *b) { // b assez grand
  char t[12]; int n = 0; int neg = v < 0; if (neg) v = -v;
  do { t[n++] = '0' + v % 10; v /= 10; } while (v);
  int i = 0; if (neg) b[i++] = '-';
  while (n) b[i++] = t[--n];
  b[i] = 0; return b;
}
// align : 0 gauche, 1 centre, 2 droite. Les \n font des lignes successives.
static void ov_text(int x, int y, const char *s, int font, uint16_t col, int align) {
  while (s && *s) {
    if (novt >= 40) return;
    int w = text_len(s) * font_w(font);
    int xx = align == 1 ? x - w / 2 : (align == 2 ? x - w : x);
    ovt[novt++] = (OvText){(int16_t)xx, (int16_t)y, col, (uint8_t)font, 1, s};
    while (*s && *s != '\n') s++;
    if (*s == '\n') s++;
    y += font_h(font) + 3;
  }
}
static void ov_rect(int x, int y, int w, int h, uint16_t col, int alpha) {
  if (novr < 24) ovr[novr++] = (OvRect){(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, col, (uint8_t)alpha};
}
static void ov_clear(void) { novt = 0; novr = 0; poolN = 0; ovPlay = 0; }

static void draw_glyphs(uint16_t *band, int by, const OvText *t, int dx, int dy, uint16_t col) {
  int fw = font_w(t->font), fh = font_h(t->font);
  int y0 = t->y + dy;
  if (y0 + fh <= by || y0 >= by + BAND_OUT) return;
  const char *s = t->s; int x = t->x + dx;
  while (*s && *s != '\n') {
    int g = glyph_index(utf8_next(&s));
    for (int r = 0; r < fh; r++) {
      int yy = y0 + r - by;
      if (yy < 0 || yy >= BAND_OUT) continue;
      uint32_t bits = t->font ? font_l[g * 28 + r] : font_s[g * 14 + r];
      uint16_t *line = band + yy * 320;
      for (int b = 0; bits; b++, bits >>= 1)
        if ((bits & 1) && (unsigned)(x + b) < 320) line[x + b] = col;
    }
    x += fw;
  }
}

static void apply_overlay(uint16_t *band, int by) {
  for (int i = 0; i < novr; i++) {
    OvRect *r = &ovr[i];
    for (int y = r->y; y < r->y + r->h; y++) {
      if (y < by || y >= by + BAND_OUT) continue;
      uint16_t *line = band + (y - by) * 320;
      int xs = r->x < 0 ? 0 : r->x, xe = r->x + r->w > 320 ? 320 : r->x + r->w;
      for (int x = xs; x < xe; x++) line[x] = r->alpha >= 32 ? r->col : blend(line[x], r->col, r->alpha);
    }
  }
  if (ovPlay && by + BAND_OUT > ovPlayY && by < ovPlayY + 22) {
    for (int y = 0; y < 22; y++) {
      int yy = ovPlayY + y - by; if (yy < 0 || yy >= BAND_OUT) continue;
      for (int x = 0; x < 54; x++) { uint16_t c = spr_play[y * 54 + x]; if (c != TRANSP) band[yy * 320 + ovPlayX + x] = c; }
    }
  }
  for (int i = 0; i < novt; i++) {
    if (ovt[i].shadow) draw_glyphs(band, by, &ovt[i], 1, 1, 0);
    draw_glyphs(band, by, &ovt[i], 0, 0, ovt[i].col);
  }
}

static int frameNo = 0;
static void present(void) {
  int H = RH;
  int rowsPerBand = BAND_OUT / SC;
  int jitterBase = (int)(glitch * 6);
  int trackY = (frameNo * 7) % 300 - 30;
  for (int by = 0; by < 240; by += BAND_OUT) {
    int iy = by / SC;
    if (SC == 1) {
      render_rows(iy, rowsPerBand, obuf);
    } else {
      render_rows(iy, rowsPerBand, rbuf);
      for (int r = 0; r < rowsPerBand; r++) {
        uint16_t *src = rbuf + r * RW;
        uint32_t *d0 = (uint32_t *)(obuf + (2 * r) * 320);
        for (int x = 0; x < RW; x++) { uint32_t c = src[x]; d0[x] = c | (c << 16); }
        memcpy(obuf + (2 * r + 1) * 320, obuf + (2 * r) * 320, 640);
      }
    }
    (void)H;
    // effets VHS
    for (int r = 0; r < BAND_OUT; r++) {
      int y = by + r;
      uint16_t *line = obuf + r * 320;
      int shift = 0;
      if (glitch > 0.02f) {
        if ((rnd() & 1023) < (uint32_t)(glitch * 90)) shift = (int)(irnd(2 * jitterBase + 3)) - jitterBase - 1;
        if (y >= trackY && y < trackY + 6 && glitch > 0.15f) shift += 3 + irnd(4);
      }
      if (shift > 0) { memmove(line + shift, line, (320 - shift) * 2); for (int x = 0; x < shift; x++) line[x] = 0; }
      else if (shift < 0) { memmove(line, line - shift, (320 + shift) * 2); for (int x = 320 + shift; x < 320; x++) line[x] = 0; }
      if (glitch > 0.2f && (rnd() & 255) < (uint32_t)(glitch * 40)) {
        int n = 2 + irnd(10 + (int)(glitch * 30));
        for (int i = 0; i < n; i++) line[irnd(320)] = 0xFFFF;
      }
      if (y & 1) for (int x = 0; x < 320; x++) { uint16_t c = line[x]; line[x] = ((c >> 1) & 0x7BEF) + ((c >> 2) & 0x39E7); }
    }
    apply_overlay(obuf, by);
    if (fadeK < 32 || whiteK > 0) {
      for (int i = 0; i < 320 * BAND_OUT; i++) {
        uint16_t c = obuf[i];
        if (fadeK < 32) c = shade(c, fadeK);
        if (whiteK > 0) c = blend(c, 0xFFFF, whiteK);
        obuf[i] = c;
      }
    }
    // coins arrondis de l'écran cathodique
    for (int r = 0; r < BAND_OUT; r++) {
      int y = by + r, dy = y < 12 ? 12 - y : (y > 227 ? y - 227 : 0);
      if (!dy) continue;
      int cut = 12 - (int)sqrtf((float)(144 - dy * dy));
      uint16_t *line = obuf + r * 320;
      for (int x = 0; x < cut; x++) { line[x] = 0; line[319 - x] = 0; }
    }
    eadk_display_push_rect((eadk_rect_t){0, (uint16_t)by, 320, BAND_OUT}, obuf);
  }
  (void)bsodBand;
  frameNo++;
}

// ---------------------------------------------------------------------------
// Entrées
// ---------------------------------------------------------------------------
static eadk_keyboard_state_t kcur, kprev;
static int down(eadk_key_t k) { return eadk_keyboard_key_down(kcur, k); }
static int pressed(eadk_key_t k) { return eadk_keyboard_key_down(kcur, k) && !eadk_keyboard_key_down(kprev, k); }
static int ok_pressed(void) { return pressed(eadk_key_ok) || pressed(eadk_key_exe); }
static int quitRequested = 0;
static void poll_keys(void) {
  kprev = kcur; kcur = eadk_keyboard_scan();
  if (down(eadk_key_home)) quitRequested = 1;
}

// ---------------------------------------------------------------------------
// Jeu
// ---------------------------------------------------------------------------
enum { ST_TITLE, ST_CONTROLS, ST_CREDITS, ST_INTRO, ST_PLAY, ST_PAUSE, ST_JUMP, ST_DEAD, ST_ESCAPE, ST_END };
static int state = ST_TITLE, menuSel = 0;
static float stTime = 0, gameTime = 0;
static float stamina = 1, monSpawnT, lightsOutT, lightsOutDur = 0, bfsT = 0, monSeenT = 0, bob = 0;
static int exhausted = 0, monChasing = 0, deaths = 0;
static const char *msg = 0; static float msgT = 0;
static uint32_t seedBase;

static void set_quality(int s) { SC = s; RW = 320 / SC; RH = 240 / SC; }
static void show_msg(const char *m, float t) { msg = m; msgT = t; }
static void goto_state(int s) { state = s; stTime = 0; menuSel = 0; }

static void start_game(void) {
  seedBase = seedBase * 1664525u + 1013904223u + eadk_random();
  generate_map(seedBase);
  px = startX; py = startY; pa = (float)(irnd(4)) * 1.5707963f;
  pagesTaken = 0; doorOpened = 0; monActive = 0; monChasing = 0;
  stamina = 1; exhausted = 0; flashOn = 0; power = 1; ambient = 40; gameTime = 0;
  monSpawnT = 45; lightsOutT = 70 + frnd() * 40; lightsOutDur = 0;
  camH = 0.5f; horOff = 0; glitch = 0; msg = 0;
  goto_state(ST_INTRO);
#ifdef SIM_DEBUG
  // test : monstre placé devant le joueur, page juste devant
  pa = 0; setup_camera();
  for (float d = 1.0f; d < 5.0f && !is_wall((int)(px + d), (int)py); d += 0.25f) { mx = px + d; my = py; monActive = 1; }
  { int x = (int)px; while (!is_wall(x + 1, (int)py)) x++;
    pages[0].x = x + 1; pages[0].y = (int)py; pages[0].face = 0;
#if SIM_DEBUG == 2
    door = pages[0]; pages[0].x = 0; pagesTaken = NPAGES; monActive = 0;
#endif
  }
  stTime = 4.5f;
#endif
}

static void try_move(float *x, float *y, float dx, float dy, float r) {
  float nx = *x + dx;
  if (!is_wall((int)(nx + (dx > 0 ? r : -r)), (int)(*y - r)) && !is_wall((int)(nx + (dx > 0 ? r : -r)), (int)(*y + r))) *x = nx;
  float ny = *y + dy;
  if (!is_wall((int)(*x - r), (int)(ny + (dy > 0 ? r : -r))) && !is_wall((int)(*x + r), (int)(ny + (dy > 0 ? r : -r)))) *y = ny;
}

static void spawn_monster(void) {
  bfs((int)px, (int)py);
  for (int tries = 0; tries < 3000; tries++) {
    int x = 1 + irnd(MW - 2), y = 1 + irnd(MH - 2);
    int d = dist[y * MW + x];
    if (is_wall(x, y) || d == 0xFFFF || d < 14 - tries / 300) continue;
    if (los(px, py, x + 0.5f, y + 0.5f)) continue;
    mx = x + 0.5f; my = y + 0.5f; monActive = 1; return;
  }
}

static void update_monster(float dt) {
  if (!monActive) return;
  bfsT -= dt;
  if (bfsT <= 0) { bfs((int)px, (int)py); bfsT = 0.3f; }
  float dx = px - mx, dy = py - my, d = sqrtf(dx * dx + dy * dy);
  int sees = d < 14 && los(mx, my, px, py);
  if (sees) { monChasing = 1; monSeenT = 4; }
  else if (monSeenT > 0) { monSeenT -= dt; if (monSeenT <= 0) monChasing = 0; }
  float diff = 1.0f + pagesTaken * 0.035f + (pagesTaken >= NPAGES ? 0.15f : 0);
  float spd = (monChasing ? 2.05f : 1.25f) * diff;
  if (power < 0.3f) spd *= 1.1f;
  float tx, ty;
  if (sees && d < 6) { tx = px; ty = py; }
  else {
    int cx = (int)mx, cy = (int)my;
    int best = dist[cy * MW + cx]; tx = cx + 0.5f; ty = cy + 0.5f;
    static const int8_t ox[4] = {1, -1, 0, 0}, oy[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) {
      int nx = cx + ox[k], ny = cy + oy[k];
      if (is_wall(nx, ny)) continue;
      if (dist[ny * MW + nx] < best) { best = dist[ny * MW + nx]; tx = nx + 0.5f; ty = ny + 0.5f; }
    }
    if (best == dist[cy * MW + cx] && sees) { tx = px; ty = py; }
  }
  float vx = tx - mx, vy = ty - my, vl = sqrtf(vx * vx + vy * vy);
  if (vl > 0.001f) {
    float s = spd * dt; if (s > vl && !(sees && d < 6)) s = vl;
    try_move(&mx, &my, vx / vl * s, vy / vl * s, 0.2f);
  }
  static float anim = 0; anim += dt * spd * 3.2f; mframe = ((int)anim) & 3;
  if (d < 0.5f) { goto_state(ST_JUMP); deaths++; }
}

static void hud_play(void) {
  static char buf[40], clk[24];
  ovPlay = 1; ovPlayX = 14; ovPlayY = 12;
  char n[8];
  itoa2(pagesTaken, n);
  strcpy(buf, n); strcat(buf, "/10 PAGES");
  ov_text(306, 14, pstr(buf), 0, 0xFFFF, 2);
  int mins = (int)gameTime; int hh = 3 + mins / 60, mm = mins % 60;
  strcpy(clk, "AM ");
  clk[3] = '0' + (hh / 10) % 10; clk[4] = '0' + hh % 10; clk[5] = ':'; clk[6] = '0' + mm / 10; clk[7] = '0' + mm % 10; clk[8] = 0;
  ov_text(16, 196, pstr(clk), 0, 0xFFFF, 0);
  ov_text(16, 214, "OCT. 30 1996", 0, 0xFFFF, 0);
  if ((frameNo / 8) & 1) ov_text(306, 214, "SP", 0, 0xFFFF, 2);
  if (stamina < 0.999f) {
    int w = (int)(80 * stamina);
    ov_rect(119, 226, 82, 6, 0, 20);
    ov_rect(120, 227, w, 4, exhausted ? RGB(200, 40, 30) : 0xFFFF, 32);
  }
  if (centerKind == LOOK_PAGE && centerDist < 1.4f) ov_text(160, 160, "[OK] RAMASSER LA PAGE", 0, 0xFFFF, 1);
  else if (centerKind == LOOK_DOOR && centerDist < 1.4f) {
    if (pagesTaken >= NPAGES) ov_text(160, 160, "[OK] OUVRIR LA PORTE", 0, 0xFFFF, 1);
    else ov_text(160, 160, "LA PORTE EST VERROUILLÉE", 0, RGB(255, 200, 120), 1);
  }
  if (msg && msgT > 0) ov_text(160, 124, msg, 0, 0xFFFF, 1);
}

static void update_play(float dt) {
  gameTime += dt;
  if (pressed(eadk_key_back) || pressed(eadk_key_backspace)) { goto_state(ST_PAUSE); return; }
  if (pressed(eadk_key_alpha) || pressed(eadk_key_zero)) flashOn = !flashOn;
  float turn = 0, fwd = 0, strafe = 0;
  if (down(eadk_key_left) || down(eadk_key_seven)) turn -= 1;
  if (down(eadk_key_right) || down(eadk_key_nine)) turn += 1;
  if (down(eadk_key_up) || down(eadk_key_eight)) fwd += 1;
  if (down(eadk_key_down) || down(eadk_key_five) || down(eadk_key_two)) fwd -= 1;
  if (down(eadk_key_four)) strafe -= 1;
  if (down(eadk_key_six)) strafe += 1;
  int wantSprint = down(eadk_key_shift) || down(eadk_key_one);
  int moving = fwd != 0 || strafe != 0;
  int sprint = wantSprint && moving && !exhausted && fwd >= 0;
  if (sprint) { stamina -= dt / 4.5f; if (stamina <= 0) { stamina = 0; exhausted = 1; show_msg("ESSOUFFLÉ...", 1.5f); } }
  else { stamina += dt / 7.0f; if (stamina >= 1) stamina = 1; if (exhausted && stamina > 0.35f) exhausted = 0; }
  pa += turn * 2.4f * dt;
  setup_camera();
  float spd = (sprint ? 3.1f : 1.75f) * dt;
  if (fwd < 0) spd *= 0.7f;
  float mvx = dirx * fwd + (-diry) * strafe, mvy = diry * fwd + dirx * strafe;
  float ml = sqrtf(mvx * mvx + mvy * mvy);
  if (ml > 0) { try_move(&px, &py, mvx / ml * spd, mvy / ml * spd, 0.22f); bob += dt * (sprint ? 13 : 8.5f); }
  horOff = sinf(bob) * RH * (sprint ? 0.022f : 0.012f);
  // interactions
  if (ok_pressed() && centerDist < 1.4f) {
    if (centerKind == LOOK_PAGE) {
      pages[centerPage].taken = 1; pagesTaken++;
      if (pagesTaken >= NPAGES) show_msg("TOUTES LES PAGES !\nTROUVE LA PORTE DE SORTIE", 4);
      else show_msg("PAGE RAMASSÉE", 1.6f);
      if (!monActive) monSpawnT = 4 + frnd() * 6;
      glitch = 0.6f;
    } else if (centerKind == LOOK_DOOR) {
      if (pagesTaken >= NPAGES) { doorOpened = 1; goto_state(ST_ESCAPE); return; }
      show_msg("IL TE MANQUE DES PAGES", 1.6f);
    }
  }
  if (msgT > 0) msgT -= dt;
  // apparition du monstre
  if (!monActive) { monSpawnT -= dt; if (monSpawnT <= 0) spawn_monster(); }
  update_monster(dt);
  // coupures de courant
  if (lightsOutDur > 0) {
    lightsOutDur -= dt;
    float t = lightsOutDur;
    power = t > 1.0f ? 0.0f : 1.0f - t;
    if (t > 11.2f) power = (rnd() & 1) ? 0.6f : 0.0f;
    ambient = 6 + (int)(34 * power);
    if (lightsOutDur <= 0) { power = 1; ambient = 40; }
  } else {
    lightsOutT -= dt;
    if (lightsOutT <= 0 && monActive) { lightsOutDur = 12; lightsOutT = 75 + frnd() * 45; show_msg("...", 1); }
    power = 1;
  }
  // effets selon la distance du monstre
  float g = 0.03f;
  if (monActive) {
    float dx = mx - px, dy = my - py, d = sqrtf(dx * dx + dy * dy);
    if (d < 9) g += (9 - d) / 9 * 0.75f;
    if (monChasing) g += 0.15f;
    if (d < 5 && lightsOutDur <= 0 && (rnd() & 15) == 0) power = 0.55f;
  }
  if (glitch > g) glitch -= dt * 0.8f; else glitch = g;
  if (stTime < 4 && !msg) show_msg("TROUVE LES 10 PAGES", 3.5f);
  ov_clear();
  cast_rays();
  prepare_sprite();
  hud_play();
}

static void title_camera(float dt) {
  pa += dt * 0.15f; setup_camera(); horOff = 0; flashOn = 0; power = 1; ambient = 40;
  monActive = 0;
  cast_rays();
  prepare_sprite();
}

static const char *CREDIT1 = "discord.gg/sti2d";
static const char *CREDIT2 = "discord : cjnw (yamii)";

static void draw_title_common(void) {
  ov_rect(0, 0, 320, 240, 0, 14);
  ovPlay = 1; ovPlayX = 14; ovPlayY = 12;
  ov_text(160, 46, "BACKROOMS '96", 1, 0xFFFF, 1);
  ov_text(160, 78, "VERSION NUMWORKS - FR", 0, RGB(255, 230, 150), 1);
  ov_text(160, 212, CREDIT1, 0, RGB(200, 200, 255), 1);
  ov_text(160, 226, CREDIT2, 0, RGB(200, 200, 255), 1);
}

static void update_title(float dt) {
  (void)dt;
  const int N = 5;
  if (pressed(eadk_key_up)) menuSel = (menuSel + N - 1) % N;
  if (pressed(eadk_key_down)) menuSel = (menuSel + 1) % N;
  if (ok_pressed() || (menuSel == 1 && (pressed(eadk_key_left) || pressed(eadk_key_right)))) {
    if (menuSel == 0) { start_game(); return; }
    if (menuSel == 1) { set_quality(SC == 1 ? 2 : 1); }
    if (menuSel == 2) { goto_state(ST_CONTROLS); return; }
    if (menuSel == 3) { goto_state(ST_CREDITS); return; }
    if (menuSel == 4) { quitRequested = 1; return; }
  }
  if (pressed(eadk_key_back)) quitRequested = 1;
  glitch = 0.05f;
  ov_clear();
  title_camera(dt);
  draw_title_common();
  const char *items[5] = {"JOUER", SC == 1 ? "QUALITÉ : HAUTE" : "QUALITÉ : NORMALE", "COMMANDES", "CRÉDITS", "QUITTER"};
  for (int i = 0; i < N; i++) {
    int y = 104 + i * 19;
    if (i == menuSel) { ov_rect(70, y - 2, 180, 18, 0xFFFF, 32); ov_text(160, y, items[i], 0, 0, 1); ovt[novt - 1].shadow = 0; }
    else ov_text(160, y, items[i], 0, 0xFFFF, 1);
  }
}

static void update_controls(void) {
  if (ok_pressed() || pressed(eadk_key_back)) { goto_state(ST_TITLE); menuSel = 2; return; }
  ov_clear();
  title_camera(0.016f);
  ov_rect(0, 0, 320, 240, 0, 24);
  ov_text(160, 12, "COMMANDES", 1, 0xFFFF, 1);
  ov_text(14, 50,
          "HAUT / BAS    : AVANCER/RECULER\n"
          "GAUCHE/DROITE : TOURNER\n"
          "4 / 6         : PAS DE CÔTÉ\n"
          "SHIFT ou 1    : COURIR\n"
          "ALPHA ou 0    : LAMPE TORCHE\n"
          "OK / EXE      : RAMASSER/OUVRIR\n"
          "RETOUR        : PAUSE", 0, 0xFFFF, 0);
  ov_text(160, 186, "RAMASSE LES 10 PAGES PUIS\nTROUVE LA SORTIE. NE TE FAIS\nPAS ATTRAPER...", 0, RGB(255, 220, 140), 1);
}

static void update_credits(void) {
  if (ok_pressed() || pressed(eadk_key_back)) { goto_state(ST_TITLE); menuSel = 3; return; }
  ov_clear();
  title_camera(0.016f);
  ov_rect(0, 0, 320, 240, 0, 24);
  ov_text(160, 14, "CRÉDITS", 1, 0xFFFF, 1);
  ov_text(160, 58, "JEU ORIGINAL :\nOLLIEHALLGAMES", 0, RGB(255, 230, 150), 1);
  ov_text(160, 104, "PORTAGE NUMWORKS / VERSION FR :", 0, 0xFFFF, 1);
  ov_text(160, 126, CREDIT2, 0, RGB(200, 200, 255), 1);
  ov_text(160, 152, "REJOINS LE DISCORD :", 0, 0xFFFF, 1);
  ov_text(160, 174, CREDIT1, 1, RGB(200, 200, 255), 1);
}

static void update_intro(float dt) {
  (void)dt;
  ov_clear();
  if (stTime < 3.0f) {
    bgMode = BG_SOLID; bgColor = 0; glitch = 0.1f;
    ov_text(160, 110, "PRÉPARATION DE L'ENREGISTREMENT", 0, 0xFFFF, 1);
    char *dots = pstr("   "); for (int i = 0; i < ((int)(stTime * 3)) % 4 && i < 3; i++) dots[i] = '.';
    ov_text(160, 128, dots, 0, 0xFFFF, 1);
    return;
  }
  if (stTime < 4.6f) {
    bgMode = BG_SOLID; bgColor = 0; glitch = 0.5f;
    ov_text(160, 106, "RÉVEILLE-TOI", 1, 0xFFFF, 1);
    return;
  }
  bgMode = BG_SCENE;
  float t = (stTime - 4.6f) / 2.5f; if (t > 1) t = 1;
  setup_camera(); cast_rays(); prepare_sprite();
  // ouverture des yeux
  float open = t < 0.4f ? t / 0.4f * 0.3f : (t < 0.55f ? 0.3f - (t - 0.4f) * 1.5f : (t - 0.55f) / 0.45f);
  open = clampf(open, 0, 1);
  int hgt = (int)(120 * (1 - open));
  ov_rect(0, 0, 320, hgt, 0, 32); ov_rect(0, 240 - hgt, 320, hgt, 0, 32);
  glitch = 0.2f * (1 - t);
  ovPlay = 1; ovPlayX = 14; ovPlayY = 12;
  if (t >= 1) { goto_state(ST_PLAY); }
}

static void update_pause(void) {
  if (pressed(eadk_key_up) || pressed(eadk_key_down)) menuSel ^= 1;
  if (pressed(eadk_key_back)) { state = ST_PLAY; return; }
  if (ok_pressed()) {
    if (menuSel == 0) { state = ST_PLAY; return; }
    goto_state(ST_TITLE); bgMode = BG_SCENE; return;
  }
  ov_clear();
  cast_rays(); prepare_sprite();
  ov_rect(0, 0, 320, 240, 0, 16);
  ov_text(20, 14, "PAUSE ||", 1, 0xFFFF, 0);
  const char *it[2] = {"REPRENDRE", "MENU PRINCIPAL"};
  for (int i = 0; i < 2; i++) {
    int y = 104 + i * 22;
    if (i == menuSel) { ov_rect(70, y - 2, 180, 18, 0xFFFF, 32); ov_text(160, y, it[i], 0, 0, 1); ovt[novt - 1].shadow = 0; }
    else ov_text(160, y, it[i], 0, 0xFFFF, 1);
  }
  ov_text(160, 212, CREDIT1, 0, RGB(200, 200, 255), 1);
  ov_text(160, 226, CREDIT2, 0, RGB(200, 200, 255), 1);
}

static void update_jump(void) {
  ov_clear();
  bgMode = BG_JUMP; glitch = 1.0f;
  jumpShakeX = irnd(13) - 6; jumpShakeY = irnd(9) - 4;
  if ((rnd() & 3) == 0) whiteK = 6 + irnd(10); else whiteK = 0;
  if (stTime > 1.3f) { whiteK = 0; goto_state(ST_DEAD); }
}

static void update_dead(void) {
  ov_clear();
  bgMode = BG_SOLID; bgColor = RGB(0, 120, 215); glitch = stTime < 0.4f ? 0.6f : 0.0f;
  ov_text(28, 30, ":(", 1, 0xFFFF, 0);
  ov_text(28, 72, "VOTRE PC A RENCONTRÉ UN\nPROBLÈME ET DOIT REDÉMARRER.\nTU AS ÉTÉ ATTRAPÉ DANS\nLES BACKROOMS.", 0, 0xFFFF, 0);
  static char b[40]; char n[8];
  int pct = (int)(stTime * 40); if (pct > 100) pct = 100;
  itoa2(pct, n); strcpy(b, n); strcat(b, "% EFFECTUÉ");
  ov_text(28, 146, pstr(b), 0, 0xFFFF, 0);
  static char b2[40]; itoa2(pagesTaken, n); strcpy(b2, "PAGES : "); strcat(b2, n); strcat(b2, "/10");
  ov_text(28, 166, pstr(b2), 0, 0xFFFF, 0);
  if (stTime > 1.2f) ov_text(28, 196, "[OK] RÉESSAYER\n[RETOUR] MENU", 0, 0xFFFF, 0);
  if (stTime > 1.2f) {
    if (ok_pressed()) { bgMode = BG_SCENE; start_game(); return; }
    if (pressed(eadk_key_back)) { bgMode = BG_SCENE; goto_state(ST_TITLE); return; }
  }
}

static void update_escape(void) {
  ov_clear();
  bgMode = BG_SCENE; setup_camera(); cast_rays(); prepare_sprite();
  // le joueur avance vers la lumière
  float t = stTime / 2.2f;
  whiteK = (int)clampf(t * 34, 0, 32);
  glitch = 0.1f;
  if (t >= 1.1f) { whiteK = 0; goto_state(ST_END); }
}

static void update_end(void) {
  ov_clear();
  bgMode = BG_SOLID; bgColor = 0; glitch = 0.03f;
  float t = stTime;
  if (t > 0.5f) ov_text(160, 20, "Plus profond", 0, RGB(180, 180, 180), 1);
  if (t > 1.5f) ov_text(160, 40, "PLUS PROFOND", 0, 0xFFFF, 1);
  if (t > 2.5f) { ov_text(160, 60, "PLUS PROFOND", 1, 0xFFFF, 1); ov_rect(70, 90, 180, 2, 0xFFFF, 32); }
  if (t > 4.0f) ov_text(160, 100,
                        "LES BACKROOMS SONT PLUS VASTES\nQUE TU NE PEUX L'IMAGINER... LE\nTERRIER DU LAPIN VA PEUT-ÊTRE\nPLUS LOIN, MAIS C'EST LA FIN\nPOUR L'INSTANT.", 0, 0xFFFF, 1);
  if (t > 6.0f) ov_text(160, 180, "MERCI D'AVOIR JOUÉ À\nBACKROOMS '96 - OLLIE", 0, RGB(255, 230, 150), 1);
  if (t > 6.0f) ov_text(160, 218, "cjnw (yamii) | discord.gg/sti2d", 0, RGB(200, 200, 255), 1);
  if (t > 7.0f && (ok_pressed() || pressed(eadk_key_back))) { bgMode = BG_SCENE; generate_map(seedBase ^ 0x5555); px = startX; py = startY; goto_state(ST_TITLE); }
}

int main(int argc, char *argv[]) {
  (void)argc; (void)argv;
  memcpy(ram_carpet, tex_carpet, sizeof(ram_carpet));
  memcpy(ram_ceil, tex_ceil_norm, sizeof(ram_ceil));
  seedBase = eadk_random();
  set_quality(2);
  generate_map(seedBase);
  px = startX; py = startY; pa = 0.3f;
  kcur = eadk_keyboard_scan(); kprev = kcur;
  uint64_t last = eadk_timing_millis();
  while (!quitRequested) {
    uint64_t now = eadk_timing_millis();
    float dt = (now - last) / 1000.0f; last = now;
    if (dt > 0.1f) dt = 0.1f;
    if (dt <= 0) dt = 0.001f;
    poll_keys();
    if (quitRequested) break;
    stTime += dt;
    int st = state;
    switch (state) {
      case ST_TITLE: bgMode = BG_SCENE; fadeK = 32; update_title(dt); break;
      case ST_CONTROLS: update_controls(); break;
      case ST_CREDITS: update_credits(); break;
      case ST_INTRO: update_intro(dt); break;
      case ST_PLAY: bgMode = BG_SCENE; update_play(dt); break;
      case ST_PAUSE: update_pause(); break;
      case ST_JUMP: update_jump(); break;
      case ST_DEAD: update_dead(); break;
      case ST_ESCAPE: update_escape(); break;
      case ST_END: update_end(); break;
    }
    if (quitRequested) break;
    if (st != state && state != ST_PLAY && state != ST_JUMP) continue; // nouvel état : rien à afficher encore
    present();
  }
  return 0;
}
