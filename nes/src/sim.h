/* shared by the simulation's banks */
#ifndef SIM_H
#define SIM_H
#include "game.h"
extern u16 cap_food, cap_goods;
extern u8 care[7];
extern s32 jobless_q;
extern u16 food_avg;
extern u8 fed_ahead_f;
extern u16 crewk[MAXB];
extern u16 dig_k, castle_k;
extern u8 farm_slot[MAXB];
extern u8 farm_n[8][4];
extern u16 farm_fields[8];
extern u16 farm_crop[8];
extern u8 farm_nfarms;
extern s16 happy_target;
extern u8 arg_a, arg_b, arg_c, ret_a;
extern const char *arg_s;

static void gain(u8 r, s32 q) { if (q <= 0) return; stock[r] += q; flow_made[r] += (s16)(q >> 8); }
static void spend(u8 r, s32 q) { if (q <= 0) return; stock[r] -= q; flow_used[r] += (s16)(q >> 8); }
static s16 whole(u8 r) { s32 v = stock[r] >> 8; return v > 32000 ? 32000 : (s16)v; }
static u8 rolls16_chk(void) { return 0; }
static u16 rolls16(s32 q) {
  u16 n = q >> 16;
  if (rnd16() < (u16)(q & 0xFFFF)) n++;
  return n;
}
static u8 cold_k(void) {  /* x2: 0, 1 (half) or 2 */
  u8 m = inyear(week) >> 2;
  if (m >= 9) return 2;
  if (m == 8 || m == 0) return 1;
  return 0;
}
static u8 weeks_to_harvest(void) { u8 w = inyear(week); return w < 24 ? 24 - w : 72 - w; }
static u8 tool_glut(void) { return whole(R_TOOLS) > 30 + (headcount() * 3) / 10; }
#endif
