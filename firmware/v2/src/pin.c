/* STI2D-OS V2 — PIN storage and checks.
 *
 * Design (replaces the V2 draft, which could be reset with a code derived
 * from the public salt, and which set a default PIN 0000):
 *   - no default PIN: the user creates it (enter + confirm);
 *   - the PIN is never stored: PBKDF2-HMAC-SHA256(pin, 16-byte salt, 4000
 *     iterations); the salt comes from the calculator's hardware random
 *     generator (eadk_random);
 *   - when a PIN is set, a 12-digit recovery code is drawn at random, shown
 *     ONCE, and stored the same way (its own salt). Knowing the record's
 *     content (salts, hashes) does not give the recovery code;
 *   - failures are counted in the record (they survive a restart): after
 *     STI_PIN_FREE_ATTEMPTS, each attempt waits longer (30 s, 60 s, ... 1 h).
 *
 * Honest limits (docs/SECURITY.md): this locks the STI2D-OS interface, it does
 * not encrypt anything. Someone with the calculator and a computer can
 * reinstall apps (which erases this record) or read the record and try every
 * 4 digit PIN offline. The calculator's own apps are not locked.
 *
 * Record "sti2dos.pin": not a Python script on purpose, so it is neither
 * listed nor editable in the Python app. */
#include <stddef.h>
#include <string.h>
#include "sti2d_v2.h"

const uint8_t *np_file_read(const char *name, uint32_t *len);
bool np_file_write(const char *name, const void *data, uint32_t len);
bool np_storage_delete(const char *name);

#define PIN_NAME "sti2dos.pin"
#define PIN_SETUP_NAME "sti2dos-pin-setup.py"

typedef struct {
  uint8_t magic[4]; /* 'S','P','I','N' */
  uint8_t version;  /* 1 */
  uint8_t enabled;
  uint8_t failures;
  uint8_t rec_failures;
  uint32_t iterations;
  uint8_t salt[16], hash[32];
  uint8_t rsalt[16], rhash[32];
  uint8_t check[8]; /* first bytes of SHA-256 of all of the above */
} pin_rec_t;

static pin_rec_t rec;
static bool loaded;

static bool store(void);
static bool consume_portal_setup(void);

static void rec_check(const pin_rec_t *r, uint8_t out[8]) {
  uint8_t h[32];
  sti_sha256((const uint8_t *)r, (uint32_t)offsetof(pin_rec_t, check), h);
  memcpy(out, h, 8);
}

void sti_v2_pin_reload(void) { loaded = false; }

static void load(void) {
  if (loaded) return;
  loaded = true;
  memset(&rec, 0, sizeof rec);
  if (consume_portal_setup()) return;
  uint32_t n = 0;
  const uint8_t *d = np_file_read(PIN_NAME, &n);
  if (!d || n != sizeof rec) return;
  pin_rec_t tmp;
  memcpy(&tmp, d, sizeof tmp);
  uint8_t c[8];
  rec_check(&tmp, c);
  if (memcmp(tmp.magic, "SPIN", 4) || tmp.version != 1 || memcmp(c, tmp.check, 8)) return;
  if (tmp.iterations < 1000 || tmp.iterations > 200000) return;
  rec = tmp;
}

static bool store(void) {
  memcpy(rec.magic, "SPIN", 4);
  rec.version = 1;
  rec_check(&rec, rec.check);
  return np_file_write(PIN_NAME, &rec, sizeof rec);
}

static int hex_digit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

/* Read an exact-length hex value from a line-oriented setup script. */
static bool setup_hex_field(const char *s, uint32_t n, const char *key, uint8_t *out, uint32_t bytes) {
  uint32_t kl = (uint32_t)strlen(key);
  for (uint32_t i = 0; i + kl <= n; i++) {
    if (i && s[i - 1] != '\n' && s[i - 1] != 0) continue;
    if (memcmp(s + i, key, kl)) continue;
    uint32_t at = i + kl, end = at;
    while (end < n && s[end] != '\n' && s[end] != '\r' && s[end] != 0) end++;
    if (end - at != bytes * 2) return false;
    for (uint32_t j = 0; j < bytes; j++) {
      int hi = hex_digit(s[at + j * 2]), lo = hex_digit(s[at + j * 2 + 1]);
      if (hi < 0 || lo < 0) return false;
      out[j] = (uint8_t)((hi << 4) | lo);
    }
    return true;
  }
  return false;
}

/* The portal temporarily handles the PIN in the browser to derive its hash,
 * but the one-time Python file contains only salts and PBKDF2 hashes. Convert
 * it to the private binary record, then remove the setup script from Epsilon's
 * user-visible Python storage. */
static bool consume_portal_setup(void) {
  uint32_t n = 0;
  const char *s = (const char *)np_file_read(PIN_SETUP_NAME, &n);
  if (!s || n == 0 || n > 512) return false;
  pin_rec_t tmp;
  memset(&tmp, 0, sizeof tmp);
  if (!setup_hex_field(s, n, "PINSALT=", tmp.salt, sizeof tmp.salt) ||
      !setup_hex_field(s, n, "PINHASH=", tmp.hash, sizeof tmp.hash) ||
      !setup_hex_field(s, n, "RECSALT=", tmp.rsalt, sizeof tmp.rsalt) ||
      !setup_hex_field(s, n, "RECHASH=", tmp.rhash, sizeof tmp.rhash)) return false;
  tmp.enabled = 1;
  tmp.iterations = STI_PIN_ITERATIONS;
  rec = tmp;
  if (!store()) {
    memset(&rec, 0, sizeof rec);
    return false;
  }
  np_storage_delete(PIN_SETUP_NAME);
  return true;
}

static void random_bytes(uint8_t *p, int n) {
  for (int i = 0; i < n; i += 4) {
    uint32_t r = sti_random32();
    for (int j = 0; j < 4 && i + j < n; j++) p[i + j] = (uint8_t)(r >> (8 * j));
  }
}

static void derive(const char *code, const uint8_t salt[16], uint32_t iters, uint8_t out[32]) {
  sti_pbkdf2_sha256((const uint8_t *)code, (uint32_t)strlen(code), salt, 16, iters, out);
}

bool sti_v2_pin_valid_format(const char *pin) {
  if (!pin) return false;
  int len = 0;
  while (pin[len]) {
    if (pin[len] < '0' || pin[len] > '9') return false;
    if (++len > STI_PIN_MAX_LEN) return false;
  }
  return len >= STI_PIN_MIN_LEN;
}

bool sti_v2_pin_enabled(void) {
  load();
  return rec.enabled != 0;
}

int sti_v2_pin_failures(void) {
  load();
  return rec.failures > rec.rec_failures ? rec.failures : rec.rec_failures;
}

uint32_t sti_v2_pin_wait_seconds(int failures) {
  if (failures < STI_PIN_FREE_ATTEMPTS) return 0;
  int k = failures - STI_PIN_FREE_ATTEMPTS;
  if (k > 7) return 3600;
  uint32_t s = 30u << k;
  return s > 3600 ? 3600 : s;
}

bool sti_v2_pin_set(const char *pin, char recovery_out[STI_RECOVERY_LEN + 1]) {
  if (!sti_v2_pin_valid_format(pin) || !recovery_out) return false;
  load();
  pin_rec_t backup = rec;
  for (int i = 0; i < STI_RECOVERY_LEN; i++) {
    uint32_t r;
    do r = sti_random32();
    while (r >= 4294967290u); /* no bias: 429496729 * 10 values */
    recovery_out[i] = (char)('0' + r % 10);
  }
  recovery_out[STI_RECOVERY_LEN] = 0;
  rec.iterations = STI_PIN_ITERATIONS;
  random_bytes(rec.salt, 16);
  random_bytes(rec.rsalt, 16);
  derive(pin, rec.salt, rec.iterations, rec.hash);
  derive(recovery_out, rec.rsalt, rec.iterations, rec.rhash);
  rec.enabled = 1;
  rec.failures = 0;
  rec.rec_failures = 0;
  if (!store()) {
    rec = backup; /* file system full: keep what was there */
    memset(recovery_out, 0, STI_RECOVERY_LEN + 1);
    return false;
  }
  return true;
}

static bool verify(const char *code, const uint8_t salt[16], const uint8_t hash[32], uint8_t *failures) {
  uint8_t h[32];
  derive(code, salt, rec.iterations, h);
  bool ok = sti_ct_equal(h, hash, 32);
  memset(h, 0, sizeof h);
  if (ok) {
    *failures = 0;
  } else if (*failures < 255) {
    (*failures)++;
  }
  store();
  return ok;
}

bool sti_v2_pin_check(const char *pin) {
  load();
  if (!rec.enabled) return true;
  if (!sti_v2_pin_valid_format(pin)) {
    if (rec.failures < 255) rec.failures++;
    store();
    return false;
  }
  bool ok = verify(pin, rec.salt, rec.hash, &rec.failures);
  if (ok) rec.rec_failures = 0, store();
  return ok;
}

bool sti_v2_pin_recovery_check(const char *code) {
  load();
  if (!rec.enabled) return false;
  int n = 0;
  while (code && code[n] >= '0' && code[n] <= '9') n++;
  if (!code || n != STI_RECOVERY_LEN || code[n]) {
    if (rec.rec_failures < 255) rec.rec_failures++;
    store();
    return false;
  }
  return verify(code, rec.rsalt, rec.rhash, &rec.rec_failures);
}

void sti_v2_pin_force_clear(void) {
  load();
  memset(&rec, 0, sizeof rec);
  store();
}

bool sti_v2_pin_disable(const char *pin) {
  if (!sti_v2_pin_check(pin)) return false;
  sti_v2_pin_force_clear();
  return true;
}
