#define _GNU_SOURCE
/* STI2D-OS V2 — host tests.
 *
 * Part 1: units (crypto test vectors, PIN storage, double π, preferences,
 *         images, calendar, terminal interpreter).
 * Part 2: the real launcher (launcher/src: main.c, sti2d.c, gfx.c, ui.c, ...)
 *         and every V2 screen, driven by scripted key presses on the mock
 *         EADK; screenshots go to build/host-shots/*.ppm.
 *
 * Build and run: make -C firmware/v2 test  (see firmware/v2/Makefile). */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <eadk.h>
#include "sti2d_v2.h"
#include "sti_v2_ui.h"
#include "mock.h"

int launcher_main(void);
int sti_v2_cal_parse(const char *s, uint32_t n);
int sti_v2_cal_serialize(char *out, int cap);

static int passed, failed;
#define CHECK(c)                                                      \
  do {                                                                \
    if (c) passed++;                                                  \
    else {                                                            \
      failed++;                                                       \
      printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);           \
    }                                                                 \
  } while (0)

static void hex(const uint8_t *d, int n, char *out) {
  for (int i = 0; i < n; i++) sprintf(out + 2 * i, "%02x", d[i]);
}

static const char *rec_text(const char *name) {
  static char buf[4096];
  uint32_t n = 0;
  const uint8_t *d = np_file_read(name, &n);
  if (!d) return "";
  if (n >= sizeof buf) n = sizeof buf - 1;
  /* skip the leading NUL of Python scripts */
  uint32_t k = 0;
  for (uint32_t i = 0; i < n; i++)
    if (d[i]) buf[k++] = (char)d[i];
  buf[k] = 0;
  return buf;
}

/* ================================================================ units */
static void test_crypto(void) {
  printf("crypto\n");
  uint8_t h[32];
  char x[65];
  sti_sha256((const uint8_t *)"abc", 3, h);
  hex(h, 32, x);
  CHECK(!strcmp(x, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
  sti_sha256((const uint8_t *)"", 0, h);
  hex(h, 32, x);
  CHECK(!strcmp(x, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
  const char *long_msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  sti_sha256((const uint8_t *)long_msg, (uint32_t)strlen(long_msg), h);
  hex(h, 32, x);
  CHECK(!strcmp(x, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
  /* RFC 4231 test case 2 */
  sti_hmac_sha256((const uint8_t *)"Jefe", 4, (const uint8_t *)"what do ya want for nothing?", 28, h);
  hex(h, 32, x);
  CHECK(!strcmp(x, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"));
  /* PBKDF2-HMAC-SHA256, P = "password", S = "salt" */
  sti_pbkdf2_sha256((const uint8_t *)"password", 8, (const uint8_t *)"salt", 4, 1, h);
  hex(h, 32, x);
  CHECK(!strcmp(x, "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b"));
  sti_pbkdf2_sha256((const uint8_t *)"password", 8, (const uint8_t *)"salt", 4, 2, h);
  hex(h, 32, x);
  CHECK(!strcmp(x, "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43"));
  sti_pbkdf2_sha256((const uint8_t *)"password", 8, (const uint8_t *)"salt", 4, 4096, h);
  hex(h, 32, x);
  CHECK(!strcmp(x, "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a"));
  uint8_t a[4] = {1, 2, 3, 4}, b[4] = {1, 2, 3, 5};
  CHECK(sti_ct_equal(a, a, 4) && !sti_ct_equal(a, b, 4));
}

static void test_pin(void) {
  printf("pin\n");
  mock_fs_reset();
  sti_v2_pin_reload();
  CHECK(!sti_v2_pin_enabled()); /* no default PIN */
  CHECK(sti_v2_pin_check("0000")); /* nothing to check when there is no PIN */
  CHECK(!sti_v2_pin_valid_format("123"));
  CHECK(!sti_v2_pin_valid_format("123456789"));
  CHECK(!sti_v2_pin_valid_format("12a4"));
  CHECK(sti_v2_pin_valid_format("1234") && !sti_v2_pin_valid_format("12345") && !sti_v2_pin_valid_format("12345678"));
  /* A hash-only setup file exported by the website is consumed on first boot. */
  {
    uint8_t ps[16], rs[16], ph[32], rh[32];
    for (int i = 0; i < 16; i++) ps[i] = (uint8_t)(i + 1), rs[i] = (uint8_t)(31 - i);
    const char *site_pin = "7319", *site_recovery = "012345678901";
    sti_pbkdf2_sha256((const uint8_t *)site_pin, 4, ps, 16, STI_PIN_ITERATIONS, ph);
    sti_pbkdf2_sha256((const uint8_t *)site_recovery, 12, rs, 16, STI_PIN_ITERATIONS, rh);
    char psh[33], rsh[33], phh[65], rhh[65];
    hex(ps, 16, psh); hex(rs, 16, rsh); hex(ph, 32, phh); hex(rh, 32, rhh);
    char setup[384];
    setup[0] = 0; /* Python script auto-import disabled */
    int setup_len = snprintf(setup + 1, sizeof setup - 1,
      "# STI2D-OS PIN setup\nSTI2D_PIN_SETUP_V1=1\nPINSALT=%s\nPINHASH=%s\nRECSALT=%s\nRECHASH=%s\n",
      psh, phh, rsh, rhh);
    CHECK(setup_len > 0 && (size_t)setup_len < sizeof setup - 1 &&
          np_file_write("sti2dos-pin-setup.py", setup, (uint32_t)setup_len + 1));
    sti_v2_pin_reload();
    CHECK(sti_v2_pin_enabled());
    CHECK(sti_v2_pin_check(site_pin));
    CHECK(sti_v2_pin_recovery_check(site_recovery));
    CHECK(np_file_read("sti2dos-pin-setup.py", 0) == 0); /* consumed and deleted */
  }
  mock_fs_reset();
  sti_v2_pin_reload();
  CHECK(!sti_v2_pin_enabled());
  char rec[STI_RECOVERY_LEN + 1];
  CHECK(!sti_v2_pin_set("12", rec));
  CHECK(sti_v2_pin_set("4821", rec));
  CHECK(strlen(rec) == STI_RECOVERY_LEN);
  for (int i = 0; i < STI_RECOVERY_LEN; i++) CHECK(rec[i] >= '0' && rec[i] <= '9');
  CHECK(sti_v2_pin_enabled());
  /* the record holds neither the PIN nor the recovery code */
  uint32_t n;
  const uint8_t *d = np_file_read("sti2dos.pin", &n);
  CHECK(d != 0);
  CHECK(!memmem(d, n, "4821", 4));
  CHECK(!memmem(d, n, rec, STI_RECOVERY_LEN));
  /* survives a restart */
  sti_v2_pin_reload();
  CHECK(sti_v2_pin_enabled());
  CHECK(sti_v2_pin_check("4821"));
  CHECK(sti_v2_pin_failures() == 0);
  for (int i = 0; i < 5; i++) CHECK(!sti_v2_pin_check("0000"));
  CHECK(sti_v2_pin_failures() == 5);
  CHECK(sti_v2_pin_wait_seconds(4) == 0);
  CHECK(sti_v2_pin_wait_seconds(5) == 30);
  CHECK(sti_v2_pin_wait_seconds(6) == 60);
  CHECK(sti_v2_pin_wait_seconds(40) == 3600);
  sti_v2_pin_reload(); /* failures are kept across a restart */
  CHECK(sti_v2_pin_failures() == 5);
  CHECK(sti_v2_pin_check("4821"));
  CHECK(sti_v2_pin_failures() == 0);
  /* the salt (public) gives nothing: the draft's "recovery from salt" is gone */
  CHECK(!sti_v2_pin_recovery_check("000000"));
  CHECK(!sti_v2_pin_recovery_check("000000000000"));
  CHECK(sti_v2_pin_recovery_check(rec));
  sti_v2_pin_force_clear();
  CHECK(!sti_v2_pin_enabled());
  /* disable needs the PIN */
  CHECK(sti_v2_pin_set("1357", rec));
  CHECK(!sti_v2_pin_disable("1111"));
  CHECK(sti_v2_pin_enabled());
  CHECK(sti_v2_pin_disable("1357"));
  CHECK(!sti_v2_pin_enabled());
  /* a damaged record is refused */
  CHECK(sti_v2_pin_set("2468", rec));
  d = np_file_read("sti2dos.pin", &n);
  uint8_t copy[256];
  memcpy(copy, d, n);
  copy[20] ^= 1;
  np_file_write("sti2dos.pin", copy, n);
  sti_v2_pin_reload();
  CHECK(!sti_v2_pin_enabled());
  mock_fs_reset();
  sti_v2_pin_reload();
}

static void test_pi(void) {
  printf("double pi\n");
  sti_v2_pi_reset();
  CHECK(!sti_v2_pi_feed(true, 1000));            /* one press: nothing */
  CHECK(!sti_v2_pi_feed(false, 1100));
  CHECK(sti_v2_pi_feed(true, 1300));             /* second within 450 ms */
  CHECK(!sti_v2_pi_feed(true, 1400));            /* a third starts a new pair */
  CHECK(!sti_v2_pi_feed(true, 2000));            /* too late for the pair */
  CHECK(sti_v2_pi_feed(true, 2200));
  sti_v2_pi_reset();
  CHECK(!sti_v2_pi_feed(true, 5000));
  CHECK(!sti_v2_pi_feed(false, 5300));
  CHECK(!sti_v2_pi_feed(false, 5500));
  CHECK(!sti_v2_pi_feed(true, 5600));            /* 600 ms later: not a double */
}

static void test_cfg(void) {
  printf("preferences\n");
  sti_v2_cfg_t c, d;
  sti_v2_cfg_defaults(&c);
  c.env = STI_ENV_CLASSIC, c.accent = 4, c.animations = 0, c.wallpaper = STI_WALL_IMAGE;
  strcpy(c.wall_image, "stimg_cat.py");
  c.date = 20261119, c.minute_of_day = 870, c.set_millis = 123456;
  char buf[256];
  int n = sti_v2_cfg_serialize(&c, buf, sizeof buf);
  CHECK(buf[0] == 0);
  sti_v2_cfg_defaults(&d);
  CHECK(sti_v2_cfg_parse(&d, buf, (uint32_t)n));
  CHECK(d.env == STI_ENV_CLASSIC && d.accent == 4 && d.animations == 0 && d.wallpaper == STI_WALL_IMAGE);
  CHECK(!strcmp(d.wall_image, "stimg_cat.py") && d.date == 20261119 && d.minute_of_day == 870 && d.set_millis == 123456);
  /* out of range values are ignored, the rest is kept */
  sti_v2_cfg_defaults(&d);
  const char *bad = "E=9\nA=77\nN=1\nW=2\nD=abc\n";
  CHECK(sti_v2_cfg_parse(&d, bad, (uint32_t)strlen(bad)));
  CHECK(d.env == STI_ENV_OS && d.accent == 0 && d.wallpaper == STI_WALL_LIVE);
  /* the exact file the web portal exports (Profils > sti2dos.py), with and without the record's flag byte */
  static const char portal[] = "# STI2D-OS V2 : reglages (ne pas modifier a la main)\nE=1\nA=4\nN=1\nW=3\nI=stimg_coucher_de_sol.py\n";
  for (int flag = 0; flag < 2; flag++) {
    char rec[160];
    rec[0] = 0;
    memcpy(rec + flag, portal, sizeof portal);
    sti_v2_cfg_defaults(&d);
    CHECK(sti_v2_cfg_parse(&d, rec, (uint32_t)(sizeof portal - 1 + flag)));
    CHECK(d.env == STI_ENV_CLASSIC && d.accent == 4 && d.animations == 1 && d.wallpaper == STI_WALL_IMAGE);
    CHECK(!strcmp(d.wall_image, "stimg_coucher_de_sol.py"));
  }
  /* calendar maths */
  CHECK(sti_v2_days_in_month(2024, 2) == 29 && sti_v2_days_in_month(2026, 2) == 28 && sti_v2_days_in_month(2000, 2) == 29);
  CHECK(sti_v2_weekday(2026, 11, 19) == 3); /* a Thursday */
  CHECK(sti_v2_weekday(2024, 1, 1) == 0);   /* a Monday */
  int y = 2026, m = 12, dd = 30;
  sti_v2_add_days(&y, &m, &dd, 3);
  CHECK(y == 2027 && m == 1 && dd == 2);
  sti_v2_add_days(&y, &m, &dd, -3);
  CHECK(y == 2026 && m == 12 && dd == 30);
  y = 2024, m = 3, dd = 1;
  sti_v2_add_days(&y, &m, &dd, -1);
  CHECK(m == 2 && dd == 29);
}

/* a 2-colour test image in the portal's format */
static int make_stimg(char *out, int w, int h) {
  int n = sprintf(out, "%c#STIMG1 %d %d 2\n#P F800001F\n#D ", 0, w, h);
  /* stripes of 16-pixel runs: alternate indices 0 and 1 */
  int total = w * h, at = 0, k = 0;
  uint8_t bytes[4096];
  int nb = 0;
  while (at < total) {
    int run = total - at < 16 ? total - at : 16;
    bytes[nb++] = (uint8_t)((run - 1) << 4 | (k++ / 4 & 1));
    at += run;
  }
  static const char *b = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  for (int i = 0; i < nb; i += 3) {
    uint32_t v = (uint32_t)bytes[i] << 16 | (i + 1 < nb ? (uint32_t)bytes[i + 1] << 8 : 0) | (i + 2 < nb ? bytes[i + 2] : 0);
    out[n++] = b[v >> 18 & 63];
    out[n++] = b[v >> 12 & 63];
    out[n++] = i + 1 < nb ? b[v >> 6 & 63] : '=';
    out[n++] = i + 2 < nb ? b[v & 63] : '=';
    if ((i / 3) % 30 == 29) n += sprintf(out + n, "\n#D ");
  }
  out[n++] = '\n';
  return n;
}

static void test_stimg(void) {
  printf("images\n");
  static char s[8192];
  static uint8_t px[STIMG_MAX_W * STIMG_MAX_H];
  int n = make_stimg(s, 160, 120);
  stimg_t img;
  CHECK(stimg_decode(s, (uint32_t)n, &img, px, sizeof px));
  CHECK(img.w == 160 && img.h == 120 && img.ncolors == 2 && img.palette[0] == 0xF800 && img.palette[1] == 0x001F);
  CHECK(px[0] == 0 && px[64] == 1);
  CHECK(!stimg_decode(s, (uint32_t)n, &img, px, 1000)); /* too big for the buffer */
  char bad[64];
  int m = sprintf(bad, "#STIMG1 200 120 2\n#P F800001F\n#D AAAA\n");
  CHECK(!stimg_decode(bad, (uint32_t)m, &img, px, sizeof px)); /* wider than 160 */
  m = sprintf(bad, "#STIMG1 4 1 2\n#P F800001F\n#D Aw==\n");      /* 1 pixel of 4 */
  CHECK(!stimg_decode(bad, (uint32_t)m, &img, px, sizeof px));
  m = sprintf(bad, "#STIMG1 4 1 2\n#P F800001F\n#D MA==\n");      /* 0x30: run 4, index 0 */
  CHECK(stimg_decode(bad, (uint32_t)m, &img, px, sizeof px));
  m = sprintf(bad, "#STIMG1 4 1 2\n#P F800001F\n#D Mw==\n");      /* index 3 >= 2 colours */
  CHECK(!stimg_decode(bad, (uint32_t)m, &img, px, sizeof px));
}

static void test_calendar_format(void) {
  printf("calendar format\n");
  static const char s[] = "\0# x\nE=20261120,0930,Controle maths\nE=20261201,----,Vacances\nE=2026,bad\nE=20261105,2575,bad time\n";
  uint32_t len = sizeof s - 1;
  CHECK(sti_v2_cal_parse(s, len) == 2);
  char out[1024];
  int n = sti_v2_cal_serialize(out, sizeof out);
  CHECK(memmem(out, (size_t)n, "E=20261120,0930,Controle maths\n", 31) != 0);
  CHECK(memmem(out, (size_t)n, "E=20261201,----,Vacances\n", 25) != 0);
}

static int nlines;
static char lastline[128], alltext[4096];
static void collect(const char *l, void *ctx) {
  (void)ctx;
  nlines++;
  snprintf(lastline, sizeof lastline, "%s", l);
  strncat(alltext, l, sizeof alltext - strlen(alltext) - 2);
  strcat(alltext, "\n");
}

static void test_terminal_exec(void) {
  printf("terminal interpreter\n");
  nlines = 0, alltext[0] = 0;
  sti_v2_term_exec("help", collect, 0);
  CHECK(nlines > 8 && strstr(alltext, "pin-status"));
  nlines = 0, alltext[0] = 0;
  sti_v2_term_exec("version", collect, 0);
  CHECK(strstr(alltext, STI2D_V2_VERSION_STRING) != 0);
  nlines = 0, alltext[0] = 0;
  sti_v2_term_exec("mem", collect, 0);
  CHECK(strstr(alltext, "Memoire de travail") && strstr(alltext, "Fichiers"));
  nlines = 0, alltext[0] = 0;
  sti_v2_term_exec("pin-status", collect, 0);
  CHECK(strstr(alltext, "PIN : desactive") != 0);
  nlines = 0, alltext[0] = 0;
  sti_v2_term_exec("rm -rf /", collect, 0);
  CHECK(strstr(alltext, "commande inconnue") != 0);
  nlines = 0, alltext[0] = 0;
  sti_v2_term_exec("echo bonjour", collect, 0);
  CHECK(!strcmp(lastline, "bonjour"));
  nlines = 0, alltext[0] = 0;
  sti_v2_term_exec("battery", collect, 0);
  CHECK(strstr(alltext, "Batterie") != 0);
}

/* ================================================================ launcher runs */
static int run(void) {
  sti_v2_pin_reload();
  int r = 0;
  return mock_run(launcher_main, &r);
}

/* digits on the keyboard */
static const int DIG[10] = {eadk_key_zero, eadk_key_one, eadk_key_two,   eadk_key_three, eadk_key_four,
                            eadk_key_five, eadk_key_six, eadk_key_seven, eadk_key_eight, eadk_key_nine};
static void type_digits(const char *s) {
  for (; *s; s++) key(DIG[*s - '0']);
}
/* letters, alpha lock on */
static void type_letters(const char *s) {
  static const int L[26] = {eadk_key_exp,    eadk_key_ln,     eadk_key_log,     eadk_key_imaginary, eadk_key_comma,
                            eadk_key_power,  eadk_key_sine,   eadk_key_cosine,  eadk_key_tangent,   eadk_key_pi,
                            eadk_key_sqrt,   eadk_key_square, eadk_key_seven,   eadk_key_eight,     eadk_key_nine,
                            eadk_key_left_parenthesis, eadk_key_right_parenthesis, eadk_key_four, eadk_key_five,
                            eadk_key_six,    eadk_key_multiplication, eadk_key_division, eadk_key_one, eadk_key_two,
                            eadk_key_three,  eadk_key_plus};
  for (; *s; s++) {
    if (*s == '-') key(eadk_key_minus);
    else if (*s == ' ') key(eadk_key_zero);
    else key(L[*s - 'a']);
  }
}

static void scenario_home_and_pi(void) {
  printf("scenario: STI2D-OS home, single and double pi\n");
  mock_fs_reset();
  sti_v2_load();
  script_reset();
  wait_ms(900);
  snap("01_home_os");
  key(eadk_key_pi); /* one press: nothing */
  wait_ms(800);
  snap("02_home_after_single_pi");
  run();
  CHECK(sti_v2_env() == STI_ENV_OS);

  script_reset();
  wait_ms(400);
  key(eadk_key_pi);
  key(eadk_key_pi); /* two presses 150 ms apart */
  wait_ms(900);
  snap("03_classic_after_double_pi");
  run();
  CHECK(sti_v2_env() == STI_ENV_CLASSIC);
  CHECK(strstr(rec_text("sti2dos.py"), "E=1") != 0);

  /* and back from STI2D Classic */
  script_reset();
  wait_ms(400);
  key(eadk_key_pi);
  key(eadk_key_pi);
  wait_ms(900);
  snap("04_os_after_double_pi_from_classic");
  run();
  CHECK(sti_v2_env() == STI_ENV_OS);
  CHECK(strstr(rec_text("sti2dos.py"), "E=0") != 0);
}

static void scenario_pi_in_list(void) {
  printf("scenario: pi inside the games list is a letter, not the shortcut\n");
  script_reset();
  wait_ms(300);
  key(eadk_key_ok); /* Jeux */
  wait_ms(300);
  key(eadk_key_pi);
  key(eadk_key_pi);
  wait_ms(300);
  snap("05_list_after_pi_pi");
  run();
  CHECK(sti_v2_env() == STI_ENV_OS);
  /* back, back: query cleared, then the list closes */
  script_reset();
  wait_ms(200);
  key(eadk_key_down); /* Horloge */
  key(eadk_key_up);   /* back to the first row: Favoris is the nearest */
  key(eadk_key_left); /* Jeux */
  key(eadk_key_ok);
  key(eadk_key_down); /* second game */
  key(eadk_key_ok);   /* play it */
  wait_ms(300);
  run();
  CHECK(mock_last_game == 2); /* the list hides Matrices: Snake, Tetris... the second is Tetris */
}

static void scenario_control_center(void) {
  printf("scenario: Control Center\n");
  script_reset();
  wait_ms(200);
  key(eadk_key_down); /* Horloge */
  key(eadk_key_down); /* Centre de controle */
  key(eadk_key_ok);
  wait_ms(200);
  snap("06_control_center");
  key_n(eadk_key_down, 3); /* Accent */
  key(eadk_key_right);     /* Orange */
  key(eadk_key_right);     /* Rose */
  key(eadk_key_down);      /* Animations */
  key(eadk_key_ok);        /* off */
  key(eadk_key_down);      /* Fond d'ecran */
  key(eadk_key_left);      /* Degrade -> Uni */
  wait_ms(150);
  snap("07_control_center_changed");
  key_n(eadk_key_down, 3); /* Informations systeme */
  key(eadk_key_ok);
  wait_ms(200);
  snap("08_system_info");
  key(eadk_key_back);
  key(eadk_key_back);
  wait_ms(300);
  snap("09_home_after_cc");
  run();
  const char *t = rec_text("sti2dos.py");
  CHECK(strstr(t, "A=2") != 0);
  CHECK(strstr(t, "N=0") != 0);
  CHECK(strstr(t, "W=0") != 0);
  /* theme row changes the shared STI2D-Play theme (sti2dplay.py) */
  script_reset();
  wait_ms(100);
  key(eadk_key_down);
  key(eadk_key_down);
  key(eadk_key_ok);
  key(eadk_key_down); /* Theme */
  key(eadk_key_right);
  key(eadk_key_down); /* Mode */
  key(eadk_key_right);
  wait_ms(150);
  snap("10_cc_theme_light");
  key(eadk_key_left);
  key(eadk_key_up);
  key(eadk_key_left);
  run();
  CHECK(strstr(rec_text("sti2dplay.py"), "T=0") != 0);
  CHECK(strstr(rec_text("sti2dplay.py"), "M=0") != 0);
}

static void goto_pin_settings(void) {
  key(eadk_key_down);
  key(eadk_key_down);
  key(eadk_key_ok);        /* Centre */
  key_n(eadk_key_down, 7); /* Code PIN */
  key(eadk_key_ok);
  wait_ms(150);
}

static void scenario_pin(void) {
  printf("scenario: PIN create, lock, wrong attempts, wait, unlock\n");
  script_reset();
  wait_ms(200);
  goto_pin_settings();
  snap("11_pin_settings_off");
  key(eadk_key_ok); /* Creer */
  type_digits("2580");
  snap("12_pin_entry");
  key(eadk_key_ok);
  type_digits("2581"); /* different */
  key(eadk_key_ok);
  wait_ms(100);
  snap("13_pin_mismatch");
  key(eadk_key_ok);
  type_digits("2580");
  key(eadk_key_ok);
  type_digits("2580");
  key(eadk_key_ok);
  wait_ms(200);
  snap("14_pin_recovery_code");
  key(eadk_key_ok);
  wait_ms(150);
  snap("15_pin_settings_on");
  run();
  CHECK(sti_v2_pin_enabled());

  /* a new session: the lock screen */
  script_reset();
  wait_ms(200);
  snap("16_lock_screen");
  for (int i = 0; i < 5; i++) {
    type_digits("1111");
    key(eadk_key_ok);
  }
  wait_ms(200);
  snap("17_lock_wait");
  type_digits("2580"); /* refused while waiting */
  key(eadk_key_ok);
  run();
  CHECK(sti_v2_pin_failures() == 5);

  script_reset();
  wait_ms(31000); /* the 30 s wait */
  type_digits("2580");
  key(eadk_key_ok);
  wait_ms(500);
  snap("18_unlocked_home");
  run();
  CHECK(sti_v2_pin_failures() == 0);

  /* Back on the lock screen quits the app: nothing is opened */
  script_reset();
  wait_ms(200);
  key(eadk_key_back);
  int r = run();
  CHECK(r == 0); /* launcher_main returned */

  /* disable it from the Control Center (asks for the PIN) */
  script_reset();
  wait_ms(250);
  type_digits("2580");
  key(eadk_key_ok);
  wait_ms(200);
  goto_pin_settings();
  key(eadk_key_down); /* Desactiver */
  key(eadk_key_ok);
  type_digits("2580");
  key(eadk_key_ok);
  wait_ms(100);
  snap("18b_pin_disabled_msg");
  key(eadk_key_ok);
  run();
  CHECK(!sti_v2_pin_enabled());
}

static void scenario_terminal(void) {
  printf("scenario: Terminal\n");
  script_reset();
  wait_ms(200);
  key(eadk_key_down);
  key(eadk_key_down);
  key(eadk_key_left); /* Terminal */
  key(eadk_key_ok);
  wait_ms(100);
  type_letters("help");
  key(eadk_key_ok);
  wait_ms(100);
  snap("19_terminal_help");
  type_letters("pin-status");
  key(eadk_key_ok);
  type_letters("mem");
  key(eadk_key_ok);
  key(eadk_key_alpha); /* digits mode */
  type_digits("42");
  key(eadk_key_ok);
  key(eadk_key_up); /* history: "42" */
  key(eadk_key_up); /* "mem" */
  wait_ms(100);
  snap("20_terminal_history");
  run();
}

static void scenario_clock(void) {
  printf("scenario: Clock\n");
  script_reset();
  wait_ms(200);
  key(eadk_key_down); /* Horloge */
  key(eadk_key_ok);
  wait_ms(200);
  snap("21_clock_unset");
  key(eadk_key_ok); /* set */
  key_n(eadk_key_left, 3);
  type_digits("19");
  type_digits("11");
  type_digits("2026");
  type_digits("14");
  type_digits("30");
  snap("22_clock_set_screen");
  key(eadk_key_ok);
  wait_ms(1200);
  snap("23_clock_time");
  key(eadk_key_right); /* Chrono */
  key(eadk_key_ok);
  wait_ms(2500);
  snap("24_stopwatch");
  key(eadk_key_ok);
  key(eadk_key_right); /* Minuteur */
  key_n(eadk_key_down, 4);
  key_n(eadk_key_minus, 6);
  key(eadk_key_ok);
  wait_ms(10500);
  snap("25_timer_alert");
  key(eadk_key_ok); /* stops the alert */
  wait_ms(200);
  snap("26_timer_after_alert");
  run();
  CHECK(sti_v2_cfg()->date == 20261119);
  CHECK(sti_v2_cfg()->minute_of_day == 14 * 60 + 30);
  sti_datetime_t t = sti_v2_now();
  CHECK(t.valid && t.day == 19 && t.hour == 14);
}

static void scenario_calendar(void) {
  printf("scenario: Calendar\n");
  script_reset();
  wait_ms(200);
  key(eadk_key_down);
  key(eadk_key_right); /* Calendrier */
  key(eadk_key_ok);
  wait_ms(200);
  snap("27_calendar_month");
  key(eadk_key_right); /* the 20th */
  key(eadk_key_ok);    /* the day */
  key(eadk_key_ok);    /* + Nouvel evenement */
  type_letters("controle maths");
  key(eadk_key_down); /* time */
  key(eadk_key_up);   /* 13 h */
  key(eadk_key_right);
  key_n(eadk_key_up, 6); /* 30 min */
  snap("28_calendar_form");
  key(eadk_key_ok);
  wait_ms(100);
  snap("29_calendar_day");
  /* the day's list shows the new event (it was drawn before the list was filled again) */
  key(eadk_key_back);
  key(eadk_key_shift); /* to come */
  wait_ms(100);
  snap("30_calendar_upcoming");
  key(eadk_key_back);
  wait_ms(100);
  snap("31_calendar_month_dot");
  run();
  CHECK(strstr(rec_text("stical.py"), "E=20261120,1330,controle maths") != 0);
  /* delete it */
  script_reset();
  wait_ms(250);
  key(eadk_key_down);
  key(eadk_key_right);
  key(eadk_key_ok);
  key(eadk_key_shift);
  key(eadk_key_backspace);
  key(eadk_key_ok); /* confirm */
  run();
  CHECK(strstr(rec_text("stical.py"), "controle") == 0);
}

static void scenario_gallery(void) {
  printf("scenario: Gallery and wallpaper\n");
  static char img[8192];
  int n = make_stimg(img, 160, 120);
  CHECK(np_file_write("stimg_rayures.py", img, (uint32_t)n));
  n = make_stimg(img, 80, 60);
  CHECK(np_file_write("stimg_petit_logo.py", img, (uint32_t)n));
  script_reset();
  wait_ms(200);
  key(eadk_key_down);
  key_n(eadk_key_right, 2); /* Galerie */
  key(eadk_key_ok);
  wait_ms(200);
  snap("32_gallery");
  key(eadk_key_ok);
  wait_ms(100);
  snap("33_gallery_full");
  key(eadk_key_back);
  key(eadk_key_shift); /* wallpaper */
  key(eadk_key_ok);    /* closes the message */
  key(eadk_key_back);
  wait_ms(600);
  snap("34_home_image_wallpaper");
  run();
  CHECK(sti_v2_cfg()->wallpaper == STI_WALL_IMAGE);
  const char *wall = sti_v2_cfg()->wall_image;
  CHECK(!strcmp(wall, "stimg_rayures.py") || !strcmp(wall, "stimg_petit_logo.py"));
  char chosen[32];
  strcpy(chosen, wall);
  script_reset();
  wait_ms(250);
  key(eadk_key_down);
  key_n(eadk_key_right, 2);
  key(eadk_key_ok);
  key(eadk_key_backspace);
  wait_ms(100);
  snap("35_gallery_delete_confirm");
  key(eadk_key_ok);
  wait_ms(100);
  snap("36_gallery_after_delete");
  run();
  CHECK(np_file_read(chosen, 0) == 0);
  CHECK(sti_v2_cfg()->wallpaper != STI_WALL_IMAGE);
  /* empty gallery explains how to import */
  mock_fs_reset();
  sti_v2_load();
  script_reset();
  wait_ms(250);
  key(eadk_key_down);
  key_n(eadk_key_right, 2);
  key(eadk_key_ok);
  wait_ms(100);
  snap("37_gallery_empty");
  run();
}

int main(int argc, char **argv) {
  if (argc > 1) mock_set_snap_dir(argv[1]);
  test_crypto();
  test_pin();
  test_pi();
  test_cfg();
  test_stimg();
  test_calendar_format();
  test_terminal_exec();
  scenario_home_and_pi();
  scenario_pi_in_list();
  scenario_control_center();
  scenario_pin();
  scenario_terminal();
  scenario_clock();
  scenario_calendar();
  scenario_gallery();
  printf("\n%d checks passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
