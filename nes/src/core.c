/* The weekly simulation, ported from src/sim/*.js with integer arithmetic.
 * Stocks are x256 (Q8). Worker shares are x32. Rates come from the web game's catalog. */
#include "sim.h"

s32 stock[NRES];
u16 pop_c, pop_a, pop_e;
s16 happy, health;
u16 week;
u8 rank, castle_lvl, tax;
u8 castle_work, castle_prog, castle_crew;
u16 castle_weeks_total;
u8 diff;
u8 harvest_k, winter_k;
u16 next_raid, next_merchant;
u8 raid_gate;
u8 plague;
s16 shock;
u16 takings;
u8 raids_won, raids_lost, raids_bought;
u8 gameover;
u8 trade_sell[NRES], trade_buy[NRES];
s16 r_income, r_taxes, r_upkeep, r_workforce, r_employed, r_idle;
u8 r_foodsat, r_fuelsat, r_variety;
s32 r_food;
u16 r_cap;
s16 flow_made[NRES], flow_used[NRES];
char msgbuf[32];
u8 msg_new;
char chron[8][32];
u16 dirty_q[48];
u8 dirty_n;
u8 view_dirty;

/* internal state of the week */
u16 cap_food, cap_goods;
u8 care[7];             /* x255 coverage by kind 1..6 */
s32 jobless_q;          /* idle share x256 */
u8 farm_slot[MAXB];
u8 farm_n[8][4];
u16 farm_fields[8];
u16 farm_crop[8];
u8 farm_nfarms;
u16 food_avg;           /* weekly food made, EMA, x256 */
s16 happy_target;
u8 fed_ahead_f;
u16 crewk[MAXB];
u16 dig_k, castle_k;



/* ---- what the screen and the pages share */
u8 vx, vy, cx, cy, tool, tsub, tsub2, speed, paused, last_tool, last_sub;
u8 save_flag;
u16 save_magic;
u8 arg_a, arg_b, arg_c, ret_a;
char whybuf[32];

const char *const zone_name[] = { "OPEN FOR A HEART", "HOUSING", "MARKET", "WHEAT", "GARDENS", "ORCHARD", "PASTURE", "SMALLHOLDINGS", "WORKSHOPS" };
const u8 zone_cost[] = { 0, 1, 1, 1, 1, 3, 1, 1, 2 };
const char *const month_name[] = { "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC", "JAN", "FEB" };
const char *const season_name[] = { "SPRING", "SUMMER", "AUTUMN", "WINTER" };

s16 whole_of(u8 r) { s32 v = stock[r] >> 8; return v > 32000 ? 32000 : (s16)v; }

void call_in(u8 bank, void (*f)(void)) {
  u8 old = cur_bank;
  set_bank(bank);
  f();
  set_bank(old);
}

extern void sa_pre(void), sa_work(void), sa_care(void), sc_consume(void), sc_money(void);
extern void sb1_weather(void), sb1_mood(void), sb1_people(void), sb1_events(void);
extern void sb2_block(void);
extern void sb2_construct(void), sb2_grow_a(void), sb2_grow_b(void), sb2_grow_c(void), sb2_place(void), sb2_demolish(void), sb2_annex_ok(void), sb2_start_annex(void), sb2_castle_ok(void), sb2_start_castle(void);
extern void gen_new(void);
extern void view_new_game(void), view_game(void), ui_title(void);

u16 headcount(void) { return pop_c + pop_a + pop_e; }

u8 season_of(u16 w) { return (w % 48) / 12; }

u8 inyear(u16 w) { return w % 48; }

void note(const char *m) {
  u8 i;
  for (i = 7; i > 0; i--) { u8 j; for (j = 0; j < 32; j++) chron[i][j] = chron[i - 1][j]; }
  for (i = 0; i < 31 && m[i]; i++) { msgbuf[i] = m[i]; chron[0][i] = m[i]; }
  msgbuf[i] = 0; chron[0][i] = 0;
  msg_new = 1;
}

void mark(u8 x, u8 y) {
  if (dirty_n < 48) dirty_q[dirty_n++] = x | ((u16)y << 8); else view_dirty = 1;
}

u8 svc_n, svc_i[24];
u8 pk_x, pk_y;
void svc_rebuild(void) {
  u8 i, t;
  svc_n = 0;
  for (i = 1; i < MAXB && svc_n < 24; i++) {
    if (bs[i] != BS_READY) continue;
    t = bt[i];
    if (sv_k[t] || sv_k2[t]) svc_i[svc_n++] = i;
  }
}
/* how well a spot is looked after by services of a kind (0..255), from the week's list of service buildings */
void mark_rect(u8 x, u8 y, u8 w, u8 d) {
  u8 i, j;
  for (j = 0; j < d; j++) for (i = 0; i < w; i++) mark(x + i, y + j);
}
u8 coverage_at(u8 kind, u8 x, u8 y) {
  u8 n, i, t;
  u16 sum = 0;
  s16 dx, dy, d;
  for (n = 0; n < svc_n; n++) {
    i = svc_i[n]; t = bt[i];
    if (sv_k[t] != kind && sv_k2[t] != kind) continue;
    dx = (s16)bx[i] + (b_w[t] >> 1) - x; dy = (s16)by[i] + (b_d[t] >> 1) - y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    d = dx > dy ? dx + (dy >> 1) : dy + (dx >> 1);
    if (d > sv_r[t]) continue;
    sum += (u16)((u32)(sv_k[t] == kind ? sv_v[t] : sv_v2[t]) * 16 * (sv_r[t] + 1 - d) / (sv_r[t] + 1));
  }
  return sum > 255 ? 255 : (u8)sum;
}

u16 housing_cap(u8 building) {
  u16 cap = 8 + blk_cap;
  u8 i;
  for (i = 1; i < MAXB; i++) {
    if (bs[i] == BS_READY || (building && bs[i] == BS_BUILD)) cap += b_cap[bt[i]];
  }
  return cap;
}

u16 pay_ok(u8 t) {
  return whole(R_WOOD) >= (s16)b_cw[t] && whole(R_STONE) >= (s16)b_cs[t] && whole(R_IRON) >= (s16)b_ci[t] && whole(R_GOLD) >= (s16)b_cg[t];
}

void pay_cost(u8 t) {
  spend(R_WOOD, (s32)b_cw[t] << 8); spend(R_STONE, (s32)b_cs[t] << 8); spend(R_IRON, (s32)b_ci[t] << 8); spend(R_GOLD, (s32)b_cg[t] << 8);
}

u8 gates_all(u8 *list) {
  u8 n = 0, w, s;
  for (w = 0; w < 9; w++) for (s = 0; s < 4; s++) if (gate_exposed(w, s)) list[n++] = w * 4 + s;
  return n;
}

u8 block_stamp(u8 x, u8 y) { arg_a = x; arg_b = y; call_in(BK_SIMB2, sb2_block); return ret_a; }

void sim_prime(void) {
  u8 i;
  refresh_net();
  call_in(BK_SIMA, sa_pre);
  call_in(BK_SIMA, sa_care);
  (void)i;
}

void ledger_prep(void) { sim_prime(); }

u16 prof[16];
u8 sim_p;
static u8 pf_t;
#define PH(n, bank, fn) case n: pf_t = frames; call_in(bank, fn); prof[n] += (u8)(frames - pf_t); break;
/* one phase of the week per call, so the picture and the pad never stall for long; returns 1 when the week is done */
u8 sim_step(void) {
  u8 i;
  if (gameover) return 1;
  switch (sim_p) {
    case 0: for (i = 0; i < NRES; i++) { flow_made[i] = 0; flow_used[i] = 0; } call_in(BK_SIMB1, sb1_weather); break;
    case 1: refresh_net(); break;
    PH(2, BK_SIMA, sa_pre)
    PH(3, BK_SIMA, sa_work)
    PH(4, BK_SIMC, sc_consume)
    PH(5, BK_SIMC, sc_money)
    PH(6, BK_SIMA, sa_care)
    PH(7, BK_SIMB1, sb1_mood)
    PH(8, BK_SIMB1, sb1_people)
    PH(9, BK_SIMB2, sb2_construct)
    PH(10, BK_SIMB2, sb2_grow_a)
    PH(11, BK_SIMB2, sb2_grow_b)
    PH(12, BK_SIMB2, sb2_grow_c)
    PH(13, BK_SIMB1, sb1_events)
    default:
      week++;
      if (inyear(week) % 12 == 0) mus_season = season_of(week);
      sim_p = 0;
      return 1;
  }
  sim_p++;
  return 0;
}

void sim_week(void) { sim_p = 0; while (!sim_step()) ; }

/* ---- wrappers for the actions that live in banks */
u8 place_player(u8 type, u8 x, u8 y) { arg_a = type; arg_b = x; arg_c = y; call_in(BK_SIMB2, sb2_place); return ret_a; }
void demolish_at(u8 x, u8 y) { arg_a = x; arg_b = y; call_in(BK_SIMB2, sb2_demolish); }
u8 plan_annex_ok(u8 w, const char **why) { arg_a = w; call_in(BK_SIMB2, sb2_annex_ok); *why = whybuf; return ret_a; }
void start_annex(u8 w) { arg_a = w; call_in(BK_SIMB2, sb2_start_annex); }
u8 castle_plan_ok(const char **why) { call_in(BK_SIMB2, sb2_castle_ok); *why = whybuf; return ret_a; }
void start_castle(void) { call_in(BK_SIMB2, sb2_start_castle); }


void main(void) {
  for (;;) {
    call_in(BK_UI, ui_title);
    if (ret_a == 0) call_in(BK_GEN, gen_new);
#ifdef T2
    for (;;) nmi_wait();
#endif
    if (ret_a == 0) call_in(BK_VIEW, view_new_game);
#ifdef T3
    for (;;) nmi_wait();
#endif
    call_in(BK_VIEW, view_game);
  }
}
