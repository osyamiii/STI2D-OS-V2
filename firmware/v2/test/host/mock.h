/* Host harness for STI2D-OS V2 (see mock.c). */
#ifndef MOCK_H
#define MOCK_H
#include <stdbool.h>
#include <stdint.h>

#define MOCK_MAX_ACTS 512
enum { ACT_KEY, ACT_WAIT, ACT_SNAP };
typedef struct {
  int kind;
  uint64_t mask;
  int hold, gap;
  const char *name;
} act_t;

void script_reset(void);
void key(int k);
void key_with(int modifier, int k);
void key_n(int k, int n);
void key_hold(int k, int ms);
void wait_ms(int ms);
void snap(const char *name);
/* Runs fn until it returns (0) or the script is over (1). */
int mock_run(int (*fn)(void), int *ret);
void mock_set_snap_dir(const char *d);
void mock_snap_now(const char *name);
uint32_t mock_now(void);
void mock_advance(uint32_t ms);
void mock_fs_reset(void);
int mock_count_color(int x, int y, int w, int h, uint16_t c);
extern uint16_t mock_fb[240][320];
extern int mock_last_game;
extern uint32_t mock_frames, mock_scans;
#endif
