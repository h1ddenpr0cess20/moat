/* The weekly simulation, ported from src/sim/*.js with integer arithmetic.
 * Stocks are x256 (Q8). Worker shares are x32. Rates come from the web game's catalog. */
#include "game.h"

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
u16 dirty_q[16];
u8 dirty_n;
u8 view_dirty;

/* internal state of the week */
static u16 cap_food, cap_goods;
static u8 care[7];             /* x255 coverage by kind 1..6 */
static s32 jobless_q;          /* idle share x256 */
static u8 farm_slot[MAXB];
static u8 farm_n[8][4];
static u16 farm_fields[8];
static u16 farm_crop[8];
static u8 farm_nfarms;
static u16 food_avg;           /* weekly food made, EMA, x256 */
static u8 place_try;
static s16 happy_target;
static u8 fed_ahead_f;
static u16 crewk[MAXB];
static u16 dig_k, castle_k;
static u8 housing_list[MAXB], housing_n;

#define FOODS_N 6
static const u8 food_res[FOODS_N] = { R_BREAD, R_GRAIN, R_PRODUCE, R_FISH, R_MEAT, R_PRODUCE };

u16 headcount(void) { return pop_c + pop_a + pop_e; }
u8 season_of(u16 w) { return (w % 48) / 12; }
u8 inyear(u16 w) { return w % 48; }
static u8 cold_k(void) {  /* x2: 0, 1 (half) or 2 */
  u8 m = inyear(week) >> 2;
  if (m >= 9) return 2;
  if (m == 8 || m == 0) return 1;
  return 0;
}

void note(const char *m) {
  u8 i;
  for (i = 7; i > 0; i--) { u8 j; for (j = 0; j < 32; j++) chron[i][j] = chron[i - 1][j]; }
  for (i = 0; i < 31 && m[i]; i++) { msgbuf[i] = m[i]; chron[0][i] = m[i]; }
  msgbuf[i] = 0; chron[0][i] = 0;
  msg_new = 1;
}

void mark(u8 x, u8 y) {
  if (dirty_n < 16) dirty_q[dirty_n++] = IDX(x, y); else view_dirty = 1;
}

static void gain(u8 r, s32 q) { if (q <= 0) return; stock[r] += q; flow_made[r] += (s16)(q >> 8); }
static void spend(u8 r, s32 q) { if (q <= 0) return; stock[r] -= q; flow_used[r] += (s16)(q >> 8); }
static s16 whole(u8 r) { s32 v = stock[r] >> 8; return v > 32000 ? 32000 : (s16)v; }

static u8 roll16(s32 q) {   /* q is x65536: stochastic rounding */
  u8 n = q >> 16;
  if (rnd16() < (u16)(q & 0xFFFF)) n++;
  return n;
}
static u16 rolls16(s32 q) {
  u16 n = q >> 16;
  if (rnd16() < (u16)(q & 0xFFFF)) n++;
  return n;
}

static u16 clampu(s32 v, u16 lo, u16 hi) { return v < lo ? lo : v > hi ? hi : (u16)v; }

/* ---------------------------------------------------------------- capacity, coverage */
static void capacity(void) {
  u8 i;
  cap_food = castle_sfood[castle_lvl]; cap_goods = castle_sgoods[castle_lvl];
  for (i = 1; i < MAXB; i++) if (bs[i] == BS_READY) { cap_food += st_food[bt[i]]; cap_goods += st_goods[bt[i]]; }
}
static s32 cap_for(u8 r) {
  if (r == R_GOLD) return 0x7FFFFFL;
  if (r >= R_GRAIN && r <= R_FISH) return (s32)cap_food << 8;
  return (s32)cap_goods << 8;
}

/* how well a spot is looked after by services of a kind (0..255) */
u8 coverage_at(u8 kind, u8 x, u8 y) {
  u8 i, t;
  u16 sum = 0;
  s16 dx, dy, d;
  for (i = 1; i < MAXB; i++) {
    if (bs[i] != BS_READY) continue;
    t = bt[i];
    if (sv_k[t] != kind && sv_k2[t] != kind) continue;
    dx = (s16)bx[i] + b_w[t] / 2 - x; dy = (s16)by[i] + b_d[t] / 2 - y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    d = dx > dy ? dx + dy / 2 : dy + dx / 2;
    if (d > sv_r[t]) continue;
    {
      u16 v = sv_k[t] == kind ? sv_v[t] : sv_v2[t];
      /* v is x16; falloff linear to the radius edge */
      sum += (u16)((u32)v * 16 * (sv_r[t] + 1 - d) / (sv_r[t] + 1));
    }
  }
  return sum > 255 ? 255 : (u8)sum;
}

static void compute_care(void) {
  u8 k, i;
  u32 acc[7], weight = 0;
  for (k = 1; k < 7; k++) acc[k] = 0;
  housing_n = 0;
  for (i = 1; i < MAXB; i++) {
    u8 t = bt[i], cxm, cym, w;
    if (bs[i] != BS_READY || !b_cap[t]) continue;
    housing_list[housing_n++] = i;
    cxm = bx[i] + b_w[t] / 2; cym = by[i] + b_d[t] / 2; w = b_cap[t];
    for (k = 1; k < 7; k++) acc[k] += (u32)coverage_at(k, cxm, cym) * w;
    weight += w;
  }
  for (k = 1; k < 7; k++) care[k] = weight ? (u8)(acc[k] / weight) : 0;
}

/* ---------------------------------------------------------------- housing */
u16 housing_cap(u8 building) {
  u16 cap = 8;
  u8 i;
  for (i = 1; i < MAXB; i++) {
    if (bs[i] == BS_READY || (building && bs[i] == BS_BUILD)) cap += b_cap[bt[i]];
  }
  return cap;
}

/* ---------------------------------------------------------------- jobs */
static u8 is_foodish(u8 t) { return b_cat[t] == 2 || w_kind[t] == 2; }
static u8 tool_glut(void) { return whole(R_TOOLS) > 30 + (headcount() * 3) / 10; }
static u8 weeks_to_harvest(void) { u8 w = inyear(week); return w < 24 ? 24 - w : 72 - w; }

static u8 prio_of(u8 t) {
  if ((t == B_WOODCUTTER || t == B_FORESTER) && whole(R_WOOD) < 70) return 3;
  if (t == B_FARMSTEAD && season_of(week) == 3) return 1;
  if (w_o0r[t] == R_TOOLS && w_kind[t] == 3 && tool_glut()) return 1;
  return (is_foodish(t) && !fed_ahead_f) ? 3 : 2;
}

static u8 jobs_of(u8 i) {
  if (bt[i] == B_FARMSTEAD) { u8 j = (farm_fields[baux[i]] + 7) / 8; return j < 1 ? 1 : j; }
  return b_jobs[bt[i]];
}

static void allocate(void) {
  u8 i, p, prio;
  s32 workforce = (s32)pop_a * 29;     /* x32: 0.9 of the adults */
  s32 labor = workforce, employed = 0, open = 0, wanted = 0, reserve, budget, crewgiven = 0;
  s32 garr = (s32)castle_jobs[castle_lvl] * 32;
  s32 got = labor < garr ? labor : garr;
  static u8 pj[MAXB], pp[MAXB];
  labor -= got; employed += got;
  bef[0] = garr ? (u8)(got * 255 / garr) : 255;
  for (i = 1; i < MAXB; i++) {
    pj[i] = 0;
    if (bs[i]) { bwk[i] = 0; bef[i] = 0; }
    if (bs[i] != BS_READY || !b_jobs[bt[i]] && bt[i] != B_FARMSTEAD) continue;
    if (!(bfl[i] & BF_CONN)) continue;
    pj[i] = jobs_of(i); pp[i] = prio_of(bt[i]);
  }
  /* builders: a third of the hands is held back for the sites */
  if (dig_ward != 0xFF) wanted += dig_crew;
  if (castle_work) wanted += castle_crew;
  for (i = 1; i < MAXB; i++) if (bs[i] == BS_BUILD) wanted += b_crew[bt[i]];
  wanted *= 32;
  reserve = wanted < labor ? wanted : labor;
  if (reserve > workforce * 3 / 10) reserve = workforce * 3 / 10;
  labor -= reserve;
  for (prio = 3; prio >= 1; prio--) {
    s32 need = 0, share;
    if (prio == 2) {
      /* crews between the first and the second rank of work */
      labor += reserve;
      budget = workforce * 55 / 100;
      if (budget > labor) budget = labor;
      dig_k = 0; castle_k = 0;
      if (dig_ward != 0xFF) { s32 t = (s32)dig_crew * 32; s32 tk = t < budget ? t : budget; budget -= tk; crewgiven += tk; dig_k = (u16)(tk * 256 / t); }
      if (castle_work) { s32 t = (s32)castle_crew * 32; s32 tk = t < budget ? t : budget; budget -= tk; crewgiven += tk; castle_k = (u16)(tk * 256 / t); }
      for (i = 1; i < MAXB; i++) if (bs[i] == BS_BUILD) {
        s32 t = (s32)b_crew[bt[i]] * 32, tk = t < budget ? t : budget;
        budget -= tk; crewgiven += tk; crewk[i] = (u16)(tk * 256 / t);
      }
      labor -= crewgiven; employed += crewgiven;
    }
    for (i = 1; i < MAXB; i++) if (pj[i] && pp[i] == prio) need += (s32)pj[i] * 32;
    if (!need) continue;
    share = labor * 256 / need;
    if (share > 256) share = 256;
    for (i = 1; i < MAXB; i++) if (pj[i] && pp[i] == prio) {
      bwk[i] = (u8)((u32)pj[i] * 32 * share >> 8 > 255 ? 255 : (u32)pj[i] * 32 * share >> 8);
      bef[i] = share > 255 ? 255 : (u8)share;
    }
    labor -= need * share >> 8;
    employed += need * share >> 8;
    open += need * (256 - share) >> 8;
  }
  (void)p;
  r_workforce = (s16)(workforce / 32); r_employed = (s16)(employed / 32); r_idle = labor > 0 ? (s16)(labor / 32) : 0;
  jobless_q = workforce ? (labor > 0 ? labor * 256 / workforce : 0) : 0;
}

/* ---------------------------------------------------------------- fields */
static void assign_fields(void) {
  u8 i, n = 0, k, x, y;
  for (i = 1; i < MAXB; i++) {
    if (bt[i] == B_FARMSTEAD && bs[i] == BS_READY && (bfl[i] & BF_CONN) && n < 8) {
      farm_slot[n] = i; baux[i] = n;
      for (k = 0; k < 4; k++) farm_n[n][k] = 0;
      farm_fields[n] = 0;
      n++;
    }
  }
  farm_nfarms = n;
  if (!n) return;
  for (y = 1; y < MAPH - 1; y++) {
    for (x = 1; x < MAPW - 1; x++) {
      u16 t = IDX(x, y);
      u8 z = ZONE(t), best = 0xFF;
      s16 bd = 99, dx, dy, d2;
      if (z < Z_WHEAT || z > Z_PASTURE || (mr[t] & BLD)) continue;
      for (i = 0; i < n; i++) {
        u8 f = farm_slot[i];
        dx = (s16)x - bx[f] - 1; dy = (s16)y - by[f] - 1;
        d2 = dx * dx + dy * dy;
        if (d2 <= 36 && d2 < bd && farm_fields[i] < 48) { bd = d2; best = i; }
      }
      if (best != 0xFF) { farm_n[best][z - Z_WHEAT]++; farm_fields[best]++; }
    }
  }
}

static u8 haul_k(u8 i) {   /* x256 */
  s16 dx = (s16)bx[i] - 24, dy = (s16)by[i] - 24, d;
  if (dx < 0) dx = -dx;
  if (dy < 0) dy = -dy;
  d = dx + dy;
  return 255 - (d > 30 ? 30 : d) * 2;
}

static void farm_week(void) {
  u8 n, s = season_of(week);
  s32 mood = 217 + (s32)happy * 77 / 1600;
  for (n = 0; n < farm_nfarms; n++) {
    u8 f = farm_slot[n];
    u16 fields = farm_fields[n];
    s32 tended, frac, hk = haul_k(f), f2, ripe, yq;
    if (!fields) continue;
    tended = (s32)bwk[f] / 4;
    if (tended > fields) tended = fields;
    frac = tended * 256 / fields;
    f2 = frac * hk >> 8; f2 = f2 * mood >> 8;
    if (s <= 1) {
      farm_crop[n] += (u16)(255 * frac / 24 >> 8);
      if (farm_crop[n] > 255) farm_crop[n] = 255;
    } else if (s == 2) {
      ripe = (s32)farm_crop[n] * 256 / 255;
      yq = f2 * ripe >> 8; yq = yq * harvest_k >> 6;
      gain(R_GRAIN, yq * farm_n[n][0] * 181 >> 8);
      gain(R_PRODUCE, yq * farm_n[n][1] * 199 >> 8);
      gain(R_PRODUCE, yq * farm_n[n][2] * 254 >> 8);
      gain(R_MEAT, (f2 * harvest_k >> 6) * farm_n[n][3] * 27 >> 8);
    }
    gain(R_WOOL, f2 * farm_n[n][3] * 27 >> 9);
    if (inyear(week) == 36) farm_crop[n] = 0;
  }
}

/* ---------------------------------------------------------------- gathering and making */
/* a random tile within the radius that satisfies the kind; returns index or 0xFFFF */
static u16 pick_tile(u8 cx, u8 cy, u8 r, u8 kind) {
  u8 tries, x, y;
  u16 i;
  for (tries = 0; tries < 14; tries++) {
    s16 xx = cx + (s16)rndn(2 * r + 1) - r, yy = cy + (s16)rndn(2 * r + 1) - r;
    if (xx < 1 || yy < 1 || xx >= MAPW - 1 || yy >= MAPH - 1) continue;
    x = xx; y = yy;
    if (!owned_at(x, y)) continue;
    i = IDX(x, y);
    if (kind == 0 && TREE(i) > 1) return i;
    if (kind == 1 && GROUND(i) == GR_ROCK) return i;
    if (kind == 2 && GROUND(i) == GR_ORE) return i;
    if (kind == 3 && GROUND(i) == GR_GRASS && !TREE(i) && !ROAD(i) && !ZONE(i) && !(mr[i] & BLD)) return i;
    if (kind == 4 && TREE(i) > 0 && TREE(i) < 3) return i;
  }
  return 0xFFFF;
}

static void work_week(void) {
  u8 i, s = season_of(week);
  s32 mood = 217 + (s32)happy * 77 / 1600;
  u8 has_tools = whole(R_TOOLS) > 0;
  s32 tool_use = 0;
  for (i = 1; i < MAXB; i++) {
    u8 t = bt[i], k, cx, cy;
    s32 wk, out;
    if (bs[i] != BS_READY) continue;
    k = w_kind[t];
    if (!k || k == 4) continue;
    if (!(bfl[i] & BF_CONN) || bwk[i] < 2) continue;
    wk = bwk[i];   /* x32 */
    cx = bx[i] + b_w[t] / 2; cy = by[i] + b_d[t] / 2;
    if (k == 1) {            /* gather */
      u8 from = w_from[t], r = w_rad[t], res = w_res[t];
      u8 felling = from == 0 && res == R_WOOD;
      u8 cnt = count_near(cx, cy, r, felling ? 4 : from == 0 ? 0 : from == 1 ? 1 : from == 2 ? 2 : 3);
      s32 supply = 256, fuelk = 256, toolk = 256;
      if (from == 3) supply = cnt >= 8 ? 256 : (s32)cnt * 32;
      else if (from == 0 && res == R_MEAT) supply = cnt >= 16 ? 256 : (s32)cnt * 16;
      else if (!cnt) supply = 0;
      if (!supply) continue;
      if (w_fuel[t]) {
        s32 need = (s32)w_fuel[t] * bef[i] >> 8;
        if (need > 0) { fuelk = stock[R_WOOD] * 256 / need; if (fuelk > 256) fuelk = 256; spend(R_WOOD, need * fuelk >> 8); }
      }
      if (w_tools[t]) { toolk = has_tools ? 256 : 154; tool_use += wk; }
      out = (s32)w_rate[t] * wk >> 5;
      out = out * w_season[t * 4 + s] >> 6;
      out = out * toolk >> 8; out = out * haul_k(i) >> 8; out = out * mood >> 8; out = out * supply >> 8; out = out * fuelk >> 8;
      gain(res, out);
      bacc[i] += (u16)(out > 30000 ? 30000 : out);
      if (felling) {
        while (bacc[i] >= 3584) {
          u16 j = pick_tile(cx, cy, r, 0);
          bacc[i] -= 3584;
          if (j == 0xFFFF) break;
          mt[j] -= 1 << 3;
          mark(j % MAPW, j / MAPW);
        }
      } else if (from == 1 || from == 2) {
        while (bacc[i] >= 7680) {
          u16 j = pick_tile(cx, cy, r, from);
          bacc[i] -= 7680;
          if (j == 0xFFFF) break;
          if (AMT(j) > 0) mt[j] -= 1 << 5;
          if (AMT(j) == 0) mt[j] = GR_GRASS;
          mark(j % MAPW, j / MAPW);
        }
      } else bacc[i] = 0;
    } else if (k == 2) {     /* smallholdings */
      s32 base = wk * w_season[t * 4 + s] >> 6;
      s32 wx = s == 2 ? harvest_k : 64;
      base = base * haul_k(i) >> 8; base = base * mood >> 8;
      gain(R_GRAIN, (s32)w_yg[t] * base * wx >> 11);
      gain(R_PRODUCE, (s32)w_yp[t] * base * wx >> 11);
      gain(R_MEAT, (s32)w_ym[t] * base * wx >> 11);
      gain(R_WOOL, (s32)w_yw[t] * base >> 5);
    } else if (k == 5) {     /* forester */
      s32 sk = (s == 0 ? 256 : s == 1 ? 179 : s == 2 ? 205 : 0) * haul_k(i) >> 8;
      sk = sk * mood >> 8;
      bacc[i] += (u16)(230 * wk * sk >> 13);   /* 0.9 x workers per week, x256 */
      while (bacc[i] >= 256) {
        u16 j = pick_tile(cx, cy, w_rad[t], 3);
        bacc[i] -= 256;
        if (j == 0xFFFF) break;
        mt[j] = (mt[j] & ~0x18) | (1 << 3);
        mark(j % MAPW, j / MAPW);
      }
      if (sk > 0 && rndn(3) == 0) {
        u16 j = pick_tile(cx, cy, w_rad[t], 4);
        if (j != 0xFFFF) { mt[j] += 1 << 3; mark(j % MAPW, j / MAPW); }
      }
    } else if (k == 3) {     /* make */
      s32 ratio = (s32)bef[i] * haul_k(i) >> 8;
      u8 outr = w_o0r[t];
      ratio = ratio * mood >> 8;
      if (w_i0a[t]) { s32 q = stock[w_i0r[t]] * 256 / w_i0a[t]; if (q < ratio) ratio = q < 0 ? 0 : q; }
      if (w_i1a[t]) { s32 q = stock[w_i1r[t]] * 256 / w_i1a[t]; if (q < ratio) ratio = q < 0 ? 0 : q; }
      if (stock[outr] >= cap_for(outr) / 100 * 95) ratio = 0;
      if (outr == R_TOOLS && tool_glut()) ratio = 0;
      if (w_fuel[t] && ratio > 0) {
        s32 need = (s32)w_fuel[t] * ratio >> 8;
        if (stock[R_WOOD] < need) { ratio = ratio * stock[R_WOOD] / (need ? need : 1); need = stock[R_WOOD]; }
        spend(R_WOOD, need);
      }
      if (ratio <= 0) continue;
      if (w_i0a[t]) spend(w_i0r[t], (s32)w_i0a[t] * ratio >> 8);
      if (w_i1a[t]) spend(w_i1r[t], (s32)w_i1a[t] * ratio >> 8);
      gain(outr, (s32)w_o0a[t] * ratio >> 8);
    }
  }
  if (tool_use) { s32 wear = tool_use * 3 / 32; if (wear > stock[R_TOOLS]) wear = stock[R_TOOLS]; spend(R_TOOLS, wear); }
}

/* ---------------------------------------------------------------- eating, warmth, coin */
static u8 foodstat[3];
static void buy_for(u8 res, s32 n_q, u8 price) {
  s32 n = n_q >> 8, room = (cap_for(res) - stock[res]) >> 8, gold = whole(R_GOLD) - 20;
  if (gold < 0) return;
  if (n > room) n = room;
  if (n > gold / price) n = gold / price;
  if (n <= 0) return;
  spend(R_GOLD, n * price * 256);
  gain(res, n * 256);
}

static void consume(void) {
  u16 people = headcount();
  s32 need, got, sat, fuel_need, fuel_got, cloth_use, i2, wsum = 0, tot_value = 0;
  u8 g, k, cold = cold_k();
  static const u8 split[3] = { 140, 51, 64 };    /* x256: .55 .2 .25 */
  s32 fs = 0;
  u8 variety = 0;
  /* the steward buys what the stores lack */
  {
    s32 need_wk = (s32)people * 64;
    s32 have = stock[R_GRAIN] + stock[R_BREAD] + stock[R_PRODUCE] + stock[R_MEAT] + stock[R_FISH];
    if (have < need_wk * 3) {
      u8 before = whole(R_GRAIN);
      buy_for(R_GRAIN, need_wk * 6, 2);
      if (whole(R_GRAIN) > before + 4) note("STEWARD BOUGHT GRAIN.");
    }
    if (cold) {
      s32 burn = (s32)people * 12 * (cold == 2 ? 4 : 1);   /* ~0.05 per head */
      if (stock[R_WOOD] < burn * 2) { u8 b = whole(R_WOOD); buy_for(R_WOOD, burn * 6, 3); if (whole(R_WOOD) > b + 4) note("STEWARD BOUGHT FIREWOOD."); }
    }
  }
  /* the table: three groups */
  for (g = 0; g < 3; g++) {
    need = (s32)people * 64 * split[g] >> 8;
    got = 0;
    if (need <= 0) { foodstat[g] = 255; continue; }
    if (g == 0) {
      s32 t = stock[R_BREAD]; if (t > need * 10 / 14) t = need * 10 / 14; got += t * 14 / 10; stock[R_BREAD] -= t; flow_used[R_BREAD] += (s16)(t >> 8);
      if (got < need) { s32 r = (need - got); if (r > stock[R_GRAIN]) r = stock[R_GRAIN]; got += r; stock[R_GRAIN] -= r; flow_used[R_GRAIN] += (s16)(r >> 8); }
    } else if (g == 1) {
      s32 t = need; if (t > stock[R_PRODUCE]) t = stock[R_PRODUCE]; got = t; stock[R_PRODUCE] -= t; flow_used[R_PRODUCE] += (s16)(t >> 8);
    } else {
      s32 t = need * 10 / 15; if (t > stock[R_FISH]) t = stock[R_FISH]; got = t * 15 / 10; stock[R_FISH] -= t;
      if (got < need) { s32 r = (need - got) * 10 / 16; if (r > stock[R_MEAT]) r = stock[R_MEAT]; got += r * 16 / 10; stock[R_MEAT] -= r; }
    }
    sat = got * 255 / need; if (sat > 255) sat = 255;
    foodstat[g] = (u8)sat;
    fs += sat * split[g] >> 8;
    if (sat >= 200) variety++;
  }
  (void)wsum; (void)tot_value; (void)i2; (void)k;
  r_foodsat = fs > 255 ? 255 : (u8)fs;
  r_variety = variety < 1 ? 1 : variety;
  /* firewood */
  cloth_use = (s32)people * 1;      /* ~0.004 each */
  if (cloth_use > stock[R_CLOTH]) cloth_use = stock[R_CLOTH];
  spend(R_CLOTH, cloth_use);
  {
    s32 base = cold == 2 ? 12 : cold == 1 ? 31 : 3;    /* x256 per head: .05 / .03 / .012 */
    fuel_need = (s32)people * base;
    if (cold == 2) fuel_need = fuel_need * winter_k / 64;
    if (cloth_use > 0 && cold) fuel_need = fuel_need * (cold == 2 ? 85 : 92) / 100;
    fuel_got = fuel_need < stock[R_WOOD] ? fuel_need : stock[R_WOOD];
    spend(R_WOOD, fuel_got);
    r_fuelsat = fuel_need > 0 ? (u8)(fuel_got * 255 / fuel_need) : 255;
  }
  /* ale */
  {
    u8 taverns = 0, n;
    s32 want, drank;
    for (n = 1; n < MAXB; n++) if (bs[n] == BS_READY && bt[n] == B_TAVERN) taverns++;
    want = (s32)taverns * (s32)(people / 4) * 90 / 4;
    drank = want < stock[R_ALE] ? want : stock[R_ALE];
    spend(R_ALE, drank);
    flow_used[R_ALE] += 0;
    jobless_q = jobless_q;
    foodstat[2] = foodstat[2];
    care[0] = want > 0 ? (u8)(drank * 255 / want) : 255;
  }
  /* things go off, and what will not fit is lost */
  for (k = 1; k < NRES; k++) {
    s32 loss;
    loss = stock[k] / 1000 * res_spoil[k];
    stock[k] -= loss;
    if (stock[k] > cap_for(k)) stock[k] = cap_for(k);
  }
  {
    s32 fd = stock[R_GRAIN] + stock[R_BREAD] + stock[R_PRODUCE] + stock[R_MEAT] + stock[R_FISH];
    r_food = fd >> 8;
    fed_ahead_f = fd >= (s32)people * 64 * (weeks_to_harvest() + 48);
  }
  /* the weekly food EMA for "could the land keep more mouths" */
  {
    s32 made = (s32)flow_made[R_GRAIN] + flow_made[R_BREAD] + flow_made[R_PRODUCE] + flow_made[R_MEAT] + flow_made[R_FISH];
    food_avg = (u16)(((s32)food_avg * 23 + made * 256) / 24);
  }
}

static void money(void) {
  u8 i;
  s32 taxes, shops, upkeep, capacity_shop = 0, demand;
  s32 taxk = (s32)tax * 256 / 10;
  s32 mood = 179 + (s32)happy * 77 / 1600;      /* .7 + .3 * happy% */
  taxes = (s32)r_employed * 230 * taxk >> 8;     /* employed * 0.9 */
  taxes = taxes * mood >> 8;
  taxes += taxes * (care[0] * 0) >> 8;
  for (i = 1; i < MAXB; i++) {
    if (bs[i] == BS_READY && b_shop[bt[i]] && (bfl[i] & BF_CONN)) capacity_shop += (s32)b_shop[bt[i]] * 256 / 10 * (128 + (bef[i] >> 1) ) >> 8;
  }
  demand = (s32)headcount() * 256 * 6 / 100;
  shops = (capacity_shop < demand ? capacity_shop : demand) * 17 / 10 * taxk >> 8;
  upkeep = (s32)castle_jobs[castle_lvl] * 230 + (s32)castle_lvl * 307;
  for (i = 1; i < MAXB; i++) {
    if (bs[i] == BS_READY && b_upk[bt[i]]) upkeep += (s32)b_upk[bt[i]] * 256 / 10 * (128 + (bef[i] >> 1)) >> 8;
  }
  gain(R_GOLD, taxes + shops);
  {
    s32 pay = upkeep < stock[R_GOLD] ? upkeep : stock[R_GOLD];
    spend(R_GOLD, pay);
  }
  takings += (u16)((taxes + shops) >> 8);
  r_taxes = (s16)((taxes + shops) >> 8); r_upkeep = (s16)(upkeep >> 8); r_income = r_taxes - r_upkeep;
}

/* ---------------------------------------------------------------- people */
static void mood_week(void) {
  s32 h = 50 * 256;      /* x256 */
  u8 cold = cold_k();
  s16 hp;
  s32 f = r_foodsat, idle;
  u8 c;
  if (f >= 242) h += 8 * 256; else if (f >= 204) h += 2 * 256; else { s32 pen = (204 - f) * 90 * 256 / 255; if (pen > 45 * 256) pen = 45 * 256; h -= pen; }
  h += ((s32)r_variety - 1) * 3 * 256;
  if (cold) { if (r_fuelsat >= 253) h += 3 * 256; else h -= (s32)(255 - r_fuelsat) * 45 * 256 / 255 * (cold == 2 ? 2 : 1) / 2; }
  h += 4 * 256 * care[0] / 255 * (care[2] > 13 ? 2 : 1) / 2;
  if (stock[R_CLOTH] > 256) h += 2 * 256;
  h += (5 * (s32)care[1] + 7 * care[3] + 7 * care[2] + 3 * care[4] + 5 * care[5] + 3 * care[6]) * 256 / 255;
  h -= ((s32)tax - 10) * 70 * 256 / 100;
  idle = jobless_q;
  if (idle > 51) h -= (idle - 51) * 40 * 256 / 256;
  h -= (s32)shock * 256 / 16;
  shock = shock * 93 / 100;
  c = 0;
  if (h < 0) h = 0;
  if (h > 100 * 256) h = 100 * 256;
  happy_target = (s16)(h / 16);
  happy += (happy_target - happy) / 4;
  hp = 62 * 16 + (14 * 16 * (s16)care[1]) / 255 + (18 * 16 * (s16)care[4]) / 255 + ((s16)r_variety - 1) * 64 + (r_foodsat >= 230 ? 64 : -160);
  if (plague) hp -= 25 * 16;
  if (hp < 0) hp = 0;
  if (hp > 1600) hp = 1600;
  health += (hp - health) / 5;
  (void)c;
}

static u8 food_for(u16 people) {
  s32 runway = weeks_to_harvest();
  s32 stockq = r_food;
  s32 support = (s32)food_avg * 4 >> 8;   /* people fed by the weekly flow: flow / 0.25 */
  if (stockq < runway * (s32)people * 3 / 16) return 0;
  return support >= (s32)people || season_of(week) == 3 && stockq > 0;
}

static void people_week(void) {
  u16 total = headcount(), cap = housing_cap(0), born, grown, aged, dead = 0, kind;
  s32 rate;
  u8 cold = cold_k();
  r_cap = cap;
  rate = (s32)pop_a * 118 * happy / 960 * r_foodsat / 255 * r_fuelsat / 255;   /* x65536 */
  born = (total < cap && food_for(total + 1)) ? rolls16(rate) : 0;
  pop_c += born;
  grown = rolls16((s32)pop_c * 65536 / 672); if (grown > pop_c) grown = pop_c;
  pop_c -= grown; pop_a += grown;
  aged = rolls16((s32)pop_a * 65536 / 1824); if (aged > pop_a) aged = pop_a;
  pop_a -= aged; pop_e += aged;
  {
    s32 starving = r_foodsat < 204 ? (s32)(204 - r_foodsat) * 1835 / 204 : 0;      /* (.8-sat)*.035 x65536 */
    s32 freezing = cold && r_fuelsat < 255 ? (s32)(255 - r_fuelsat) * 2621 / 255 / (cold == 2 ? 1 : 2) : 0;
    s32 ill = plague ? 1311 * (255 - care[4] * 6 / 10) / 255 : 0;
    s32 frail = (s32)(1600 - health) * 2 / 16;      /* (100-h)*.00003 x65536 */
    u16 *pp[3];
    static const u16 base[3] = { 26, 33, 262 };
    u8 n;
    pp[0] = &pop_c; pp[1] = &pop_a; pp[2] = &pop_e;
    for (n = 0; n < 3; n++) {
      u16 d = rolls16((s32)*pp[n] * (base[n] + starving + freezing + ill + frail));
      if (d > *pp[n]) d = *pp[n];
      *pp[n] -= d; dead += d;
    }
    if (dead >= 3) note(starving > 700 ? "DEATHS FROM HUNGER." : freezing > 300 ? "DEATHS FROM THE COLD." : plague ? "THE PLAGUE TAKES SOME." : "SEVERAL DIED THIS WEEK.");
  }
  if (plague) { plague--; if (!plague) note("THE PLAGUE HAS PASSED."); }
  if (happy < 38 * 16 && total > 6) {
    u16 gone = rolls16((s32)total * 786 * (608 - happy) / 160);
    if (gone > pop_a) gone = pop_a;
    if (gone) { pop_a -= gone; note("FOLK LEAVE, SICK OF IT."); }
  }
  {
    s16 freeh = (s16)cap - (s16)headcount();
    if (freeh >= 2 && happy >= 50 * 16 && r_foodsat >= 242 && r_fuelsat >= 230 && food_for(total + 6) && jobless_q < 77) {
      u16 p = 64 + (happy - 800) * 3 / 16;        /* .25 + (h-50)*.012 as x256 */
      if (rnd8() < p) {
        u8 size = 2 + rndn(3), kids;
        if (size > freeh) size = freeh;
        kids = size > 3 ? 1 + rndn(2) : size > 2 ? 1 : 0;
        pop_a += size - kids; pop_c += kids;
        snd_play(1);
        if (size >= 3 && rndn(2)) note("A FAMILY HAS COME FOR A HOME.");
      }
    }
  }
  if (headcount() < 1 && week > 4 && !gameover) { gameover = 1; note("NO ONE IS LEFT. THE BARONY FELL."); }
}

/* ---------------------------------------------------------------- building and growing */
u16 pay_ok(u8 t) {
  return whole(R_WOOD) >= (s16)b_cw[t] && whole(R_STONE) >= (s16)b_cs[t] && whole(R_IRON) >= (s16)b_ci[t] && whole(R_GOLD) >= (s16)b_cg[t];
}
void pay_cost(u8 t) {
  spend(R_WOOD, (s32)b_cw[t] << 8); spend(R_STONE, (s32)b_cs[t] << 8); spend(R_IRON, (s32)b_ci[t] << 8); spend(R_GOLD, (s32)b_cg[t] << 8);
}

u8 place_player(u8 type, u8 x, u8 y) {
  u8 id;
  if (!can_place(type, x, y, 0)) return 0xFF;
  if (b_rank[type] > rank) return 0xFE;
  if (!pay_ok(type)) return 0xFD;
  pay_cost(type);
  id = place_building(type, x, y, 0);
  if (id != 0xFF) { u8 a = b_auto[type]; (void)a; }
  return id;
}

void demolish_at(u8 x, u8 y) {
  u8 id = find_building(x, y), t, i, j;
  if (id == 0xFF || id == 0) return;
  t = bt[id];
  /* half the timber and stone comes back */
  gain(R_WOOD, (s32)b_cw[t] << 7); gain(R_STONE, (s32)b_cs[t] << 7);
  i = b_w[t]; j = b_d[t];
  { u8 xx = bx[id], yy = by[id]; remove_building(id); refresh_net(); view_dirty = 1; (void)xx; (void)yy; (void)i; (void)j; }
}

/* does the footprint stand on tiles of `zone` all through, free to build on, with a connected road alongside? */
static u8 lot_ok(u8 zone, u8 t, u8 x, u8 y) {
  u8 w = b_w[t], d = b_d[t], i, j;
  if (x + w >= MAPW || y + d >= MAPH) return 0;
  for (j = 0; j < d; j++) for (i = 0; i < w; i++) {
    u16 k = IDX(x + i, y + j);
    if (ZONE(k) != zone || (mr[k] & BLD) || ROAD(k) || TREE(k) || GROUND(k) > GR_GRASS) return 0;
    if (!owned_at(x + i, y + j)) return 0;
  }
  return adjacent_road(x, y, w, d, 1);
}

static s16 lot_score(u8 t, u8 x, u8 y) {
  s16 dx = (s16)x - 24, dy = (s16)y - 24, s;
  u8 cx = x + b_w[t] / 2, cy = y + b_d[t] / 2;
  if (dx < 0) dx = -dx;
  if (dy < 0) dy = -dy;
  s = 200 - (dx + dy) * 2 + (x & 1 ? 0 : 6) + (y & 1 ? 0 : 6);
  s += (coverage_at(1, cx, cy) >> 4) + (coverage_at(3, cx, cy) >> 4) + (coverage_at(2, cx, cy) >> 4) + (coverage_at(6, cx, cy) >> 4);
  return s;
}

static const u8 grown_b[4][3] = {
  { B_COTTAGE, B_HOUSE, B_TOWNHOUSE }, { B_STALL, B_SHOP, B_MERCHANT }, { B_CROFT, B_SMALLHOLDING, B_GRANGE }, { B_WORKSHOP, B_JOINERY, B_GUILDHALL },
};
static u8 zone_idx(u8 zone) { return zone == Z_RES ? 0 : zone == Z_MARKET ? 1 : zone == Z_CROFT ? 2 : 3; }

u8 spawn(u8 zone, u8 level) {
  u8 t = grown_b[zone_idx(zone)][level - 1], tries, bestx = 0, besty = 0, found = 0, x, y;
  s16 best = -32000, sc;
  if (!pay_ok(t)) return 0;
  for (tries = 0; tries < 60; tries++) {
    x = 17 + rndn(30) , y = 1 + rndn(46);
    x = 1 + rndn(46);
    if (!owned_at(x, y) || ZONE(IDX(x, y)) != zone) continue;
    if (!lot_ok(zone, t, x, y)) continue;
    sc = lot_score(t, x, y);
    if (sc > best) { best = sc; bestx = x; besty = y; found = 1; }
    if (found && tries > 36) break;
  }
  if (!found) return 0;
  pay_cost(t);
  place_building(t, bestx, besty, 0);
  view_dirty = 1;
  return 1;
}

static void upgrade_try(void) {
  u8 n, cand[8], c = 0, i, pick, t, z, lvl, nt;
  u8 w2, d2, x, y, j, ok = 1;
  for (i = 1; i < MAXB && c < 8; i++) {
    if (bs[i] != BS_READY || !b_auto[bt[i]]) continue;
    if (b_level[bt[i]] >= 3) continue;
    if (rndn(4)) continue;
    cand[c++] = i;
  }
  if (!c) return;
  pick = cand[rndn(c)];
  t = bt[pick]; z = b_auto[t]; lvl = b_level[t]; nt = grown_b[zone_idx(z)][lvl];
  if (happy < (lvl == 1 ? 52 : 62) * 16) return;
  if (lvl == 2 && coverage_at(1, bx[pick], by[pick]) + coverage_at(3, bx[pick], by[pick]) + coverage_at(2, bx[pick], by[pick]) < 40) return;
  if (!pay_ok(nt)) return;
  x = bx[pick]; y = by[pick]; w2 = b_w[nt]; d2 = b_d[nt];
  /* any extra tiles it needs must be free and of the same zone */
  for (j = 0; j < d2; j++) for (n = 0; n < w2; n++) {
    u16 k = IDX(x + n, y + j);
    if (n < b_w[t] && j < b_d[t]) continue;
    if (x + n >= MAPW - 1 || y + j >= MAPH - 1 || ZONE(k) != z || (mr[k] & BLD) || ROAD(k) || TREE(k) || GROUND(k) > GR_GRASS || !owned_at(x + n, y + j)) ok = 0;
  }
  if (!ok) return;
  remove_building(pick);
  pay_cost(nt);
  place_building(nt, x, y, 0);
  /* restore the zone under the old footprint (place_building cleared it) */
  for (j = 0; j < d2; j++) for (n = 0; n < w2; n++) set_zone(x + n, y + j, z);
  view_dirty = 1;
}

static void construct(void) {
  u8 i;
  for (i = 1; i < MAXB; i++) {
    u16 need;
    if (bs[i] != BS_BUILD) continue;
    bacc[i] += crewk[i] > 255 ? 255 : crewk[i];
    need = (u16)b_weeks[bt[i]] * 255;
    if (bacc[i] >= need) {
      u8 t = bt[i];
      bs[i] = BS_READY; bacc[i] = 0; bprog[i] = 255;
      if (!b_auto[t]) { note("A BUILDING IS FINISHED."); snd_play(2); }
      mark(bx[i], by[i]);
      view_dirty = 1;
      net_dirty = 1;
    }
  }
  if (castle_work) {
    castle_prog += 0;
    castle_weeks_total += castle_k > 255 ? 255 : castle_k;
    if (castle_weeks_total >= (u16)castle_weeks[castle_lvl + 1] * 255) {
      castle_lvl++; castle_work = 0; castle_weeks_total = 0;
      note("THE CASTLE IS GREATER NOW!"); snd_play(7);
      view_dirty = 1;
    }
  }
  if (dig_ward != 0xFF) {
    dig_prog += 0;
    bacc[0] += dig_k > 255 ? 255 : dig_k;
    if (bacc[0] >= (u16)dig_weeks * 255) {
      u8 w = dig_ward;
      dig_ward = 0xFF; bacc[0] = 0;
      complete_annex(w);
      note("THE NEW MOAT IS DUG AND FILLING."); snd_play(7);
      view_dirty = 1;
    }
  }
}

static void grow_week(void) {
  u16 people = headcount();
  u16 supply = 0;
  u8 i, n;
  s16 want_homes = housing_cap(1) < (s16)(people + 6 + people / 8);
  if (want_homes) {
    n = 0;
    while (n < 2 && housing_cap(1) < people + 6 + people / 8 + n && spawn(Z_RES, 1)) n++;
  }
  for (i = 1; i < MAXB; i++) if (bs[i]) supply += b_shop[bt[i]];
  if (people >= 15 && supply * 4 < people * 3) spawn(Z_MARKET, 1);
  if (people >= 12) {
    u8 coming = 0, going = 0, idle_shops = 0;
    s16 idle = r_idle;
    for (i = 1; i < MAXB; i++) {
      u8 a = bs[i] ? b_auto[bt[i]] : 0;
      if (a != Z_CROFT && a != Z_CRAFT) continue;
      if (bs[i] == BS_READY && a == Z_CRAFT && bef[i] < 204) idle_shops++;
      if (bs[i] == BS_BUILD) { coming += b_jobs[bt[i]]; going++; }
    }
    {
      u8 hungry = ((u32)food_avg * 4 >> 8) < (u32)people * 13 / 10;
      if (idle - coming >= 2) { if (hungry || tool_glut()) { if (!spawn(Z_CROFT, 1)) spawn(Z_CRAFT, 1); } else if (!spawn(Z_CRAFT, 1)) spawn(Z_CROFT, 1); }
      else if (going < 2) {
        if (hungry && want_homes) spawn(Z_CROFT, 1);
        else if (whole(R_TOOLS) < 3 + (s16)(people / 25) && !idle_shops) spawn(Z_CRAFT, 1);
      }
    }
  }
  if ((week & 1) == 0) upgrade_try();
  /* the forest grows back */
  if (season_of(week) <= 1) {
    for (n = 0; n < 4; n++) {
      u8 x = rndn(MAPW), y = rndn(MAPH);
      u16 k = IDX(x, y);
      if (TREE(k) > 0 && TREE(k) < 3) { mt[k] += 1 << 3; mark(x, y); }
      else if (TREE(k) > 0 && rndn(4) == 0) {
        u8 nx = x + (rndn(2) ? 1 : -1 + 0), ny = y + (rndn(3) - 1);
        if (nx < MAPW && ny < MAPH) {
          u16 j = IDX(nx, ny);
          if (!TREE(j) && GROUND(j) == GR_GRASS && !(mr[j] & BLD) && !ROAD(j) && !ZONE(j) && (!owned_at(nx, ny) || rndn(3) == 0)) { mt[j] |= 1 << 3; mark(nx, ny); }
        }
      }
    }
  }
}

/* ---------------------------------------------------------------- events */
void burn_building(u8 id) {
  u8 t = bt[id], z = b_auto[t], i, j, x = bx[id], y = by[id], w = b_w[t], d = b_d[t];
  remove_building(id);
  if (z) for (j = 0; j < d; j++) for (i = 0; i < w; i++) set_zone(x + i, y + j, z);
  note("A BUILDING BURNED DOWN.");
  snd_play(4);
  view_dirty = 1;
}

static void fires_week(void) {
  u8 i, s = season_of(week);
  for (i = 1; i < MAXB; i++) {
    u8 t = bt[i];
    u16 chance;
    if (bs[i] != BS_READY) continue;
    /* about 0.006% a week, a little more for wood and workshops, less where a well is near */
    chance = 4 + (t == B_COTTAGE || t == B_WORKSHOP || t == B_STALL ? 4 : 0) + (b_cat[t] == 3 ? 2 : 0);
    if (s == 1) chance += chance / 3;
    chance = chance * (255 - coverage_at(1, bx[i], by[i]) * 85 / 100) / 255;
    if (rnd16() < chance) { if (t != B_WELL && t != B_GREEN) burn_building(i); return; }
  }
}

static void plague_week(void) {
  u16 people = headcount();
  u16 p;
  if (plague || people < 45) return;
  p = (u16)((u32)people * 105 / 100 * (255 - care[4] * 65 / 100) / 255 / 1);   /* ~0.16% x people/100 x65536 */
  if (rnd16() < p) { plague = 8 + rndn(5); shock += 6 * 16; note("A SICKNESS HAS COME. BUILD A HEALER."); snd_play(4); }
}

u16 defense_of(u8 gate) {
  u8 i;
  u32 d = (u32)castle_def[castle_lvl] * 16 * (153 + (u32)bef[0] * 102 / 255) / 255;
  for (i = 1; i < MAXB; i++) {
    if (bs[i] != BS_READY || !b_def[bt[i]] || !(bfl[i] & BF_CONN)) continue;
    if (bt[i] == B_GATEHOUSE && (gate == 0xFF || baux[i] != gate)) continue;
    d += (u32)b_def[bt[i]] * 16 * (128 + (bef[i] >> 1)) / 255;
  }
  return (u16)(d * 13 / 10);
}

u8 gates_all(u8 *list) {
  u8 n = 0, w, s;
  for (w = 0; w < 9; w++) for (s = 0; s < 4; s++) if (gate_exposed(w, s)) list[n++] = w * 4 + s;
  return n;
}

u8 raid_threat(void) {
  u16 t = 2 + (week / 48 + 1) * 14 / 10 + headcount() / 20 + owned_count() * 2;
  if (t > 255) t = 255;
  return (u8)t;
}

static void raids_week(void) {
  u8 list[36], n, i;
  if (diff == 0 || next_raid == 0xFFFF) return;
  if (week + 3 == next_raid) {
    u16 lowest = 65000;
    u8 pick = 0xFF;
    n = gates_all(list);
    for (i = 0; i < n; i++) {
      u16 d = defense_of(list[i]);
      if (!((gate_open[list[i] >> 2] >> (list[i] & 3)) & 1)) d = d * 24 / 10;
      if (d < lowest) { lowest = d; pick = list[i]; }
    }
    raid_gate = pick;
    note("RAIDERS MARCH ON THE REALM!");
    snd_play(6);
    return;
  }
  if (week < next_raid) return;
  {
    u16 strength = (u16)((u32)raid_threat() * (204 + rndn(128)) / 255 * 16 / 1);
    u16 def = defense_of(raid_gate);
    u8 raised = raid_gate != 0xFF && !((gate_open[raid_gate >> 2] >> (raid_gate & 3)) & 1);
    u16 ratio;
    next_raid = week + 40 + rndn(30);
    if (raids_won + raids_lost == 0) strength = strength * 65 / 100;
    if (raised) def = def * 24 / 10;
    ratio = strength ? (u16)((u32)def * 100 / strength) : 999;
    if (ratio >= 130) { gain(R_GOLD, (s32)strength / 16 * 6 * 256); raids_won++; note(raised ? "RAIDERS FOUND THE BRIDGE UP." : "RAIDERS ROUTED! PLUNDER TAKEN."); snd_play(7); }
    else if (ratio >= 100) {
      raids_won++;
      if (rndn(5) < 2) { u8 b = 1 + rndn(MAXB - 1); if (bs[b] && bt[b] != B_GATEHOUSE) { burn_building(b); } }
      note("RAIDERS DRIVEN OFF.");
    } else {
      u32 price = (u32)strength / 16 * 36 * (4 + raids_bought) / 4;
      if (whole(R_GOLD) >= (s16)(price > 30000 ? 30000 : price) && price < 30000) {
        spend(R_GOLD, (s32)price * 256); raids_bought++; shock += 2 * 16;
        note("THE STEWARD PAID THE RAIDERS OFF.");
      } else {
        u8 k, severe = ratio < 60;
        for (k = 1; k < NRES; k++) stock[k] -= stock[k] / (severe ? 4 : 8);
        raids_lost++; shock += (severe ? 14 : 6) * 16;
        { u16 dead = headcount() * (severe ? 4 : 1) / 100; if (dead > pop_a) dead = pop_a; pop_a -= dead; }
        { u8 b = 1 + rndn(MAXB - 1); if (bs[b]) burn_building(b); }
        note(severe ? "THE BARONY IS SACKED!" : "RAIDERS BROKE IN AND LOOTED.");
        snd_play(4);
      }
    }
    raid_gate = 0xFF;
  }
}

static void merchants_week(void) {
  u8 i, any_open = 0, w, s, market = 0, n = 0;
  if (week < next_merchant) return;
  for (i = 1; i < MAXB; i++) if (bs[i] == BS_READY && b_trade[bt[i]]) market = 1;
  for (w = 0; w < 9; w++) for (s = 0; s < 4; s++) if (gate_exposed(w, s) && ((gate_open[w] >> s) & 1)) any_open = 1;
  if (!market || !any_open) { next_merchant = week + 4; return; }
  next_merchant = week + 8 + rndn(5);
  for (i = 1; i < NRES; i++) {
    s32 hv = (s32)trade_sell[i] * 10 * 256, lo = (s32)trade_buy[i] * 10 * 256;
    if (trade_sell[i] && stock[i] > hv) {
      s32 q = stock[i] - hv, cap = (s32)600 / prices[i] * 256;
      if (q > cap) q = cap;
      spend(i, q); gain(R_GOLD, q * prices[i] * 85 / 100);
      n = 1;
    }
    if (trade_buy[i] && stock[i] < lo) {
      s32 q = lo - stock[i], cap = (s32)400 / prices[i] * 256, aff = stock[R_GOLD] * 100 / (prices[i] * 115);
      if (q > cap) q = cap;
      if (q > aff) q = aff;
      if (q >= 256) { spend(R_GOLD, q * prices[i] * 115 / 100); gain(i, q); n = 1; }
    }
  }
  note(n ? "MERCHANTS TRADED AT THE GATE." : "MERCHANTS CAME. SET YOUR TRADE.");
  snd_play(3);
}

static void tribute_week(void) {
  u8 y = week / 48 + 1;
  if (inyear(week) != 24) return;
  {
    u16 tk = takings; u32 due;
    takings = 0;
    if (y < 3 || diff == 0) return;
    due = (u32)owned_count() * 25 + tk / 3;
    if (diff == 1) due = due * 6 / 10;
    if (whole(R_GOLD) >= (s16)due) { spend(R_GOLD, (s32)due * 256); note("THE KING'S COLLECTOR HAS BEEN PAID."); }
    else { gain(R_GOLD, -stock[R_GOLD]); stock[R_GOLD] = 0; shock += 8 * 16; note("THE KING'S MAN TOOK ALL YOU HAD."); }
  }
}

static void ranks_week(void) {
  u8 n;
  if (week & 3) return;
  n = rank + 1;
  if (n > 4) return;
  if (headcount() < rank_pop[n] || owned_count() < rank_wards[n]) return;
  if (rank_castle[n] && castle_lvl < rank_castle[n]) return;
  if (rank_bld[n] != 255 && !bldg_of_type(rank_bld[n])) return;
  rank = n;
  shock = shock > 6 * 16 ? shock - 6 * 16 : 0;
  note("THE KING HAS RAISED YOUR RANK!");
  snd_play(7);
}

static void weather_week(void) {
  if (inyear(week) == 0 || week == 0) {
    harvest_k = 52 + rndn(22);
    winter_k = 54 + rndn(26);
  }
}

/* ---------------------------------------------------------------- the whole week */
void sim_prime(void) {
  u8 i;
  refresh_net();
  capacity();
  assign_fields();
  allocate();
  compute_care();
  for (i = 0; i < NRES; i++) { flow_made[i] = 0; flow_used[i] = 0; }
}

void ledger_prep(void) { sim_prime(); }

void sim_week(void) {
  u8 i;
  if (gameover) return;
  for (i = 0; i < NRES; i++) { flow_made[i] = 0; flow_used[i] = 0; }
  weather_week();
  refresh_net();
  capacity();
  assign_fields();
  allocate();
  farm_week();
  work_week();
  consume();
  money();
  compute_care();
  mood_week();
  people_week();
  construct();
  grow_week();
  merchants_week();
  if (diff) { fires_week(); plague_week(); raids_week(); }
  tribute_week();
  ranks_week();
  week++;
  if (inyear(week) % 12 == 0) mus_season = season_of(week);
}

/* ---------------------------------------------------------------- annexing and the castle */
u8 plan_annex_ok(u8 w, const char **why) {
  u8 owned = owned_count(), wx = w % WARDS, wy = w / WARDS, touches = 0;
  u16 g = 260 + owned * 140, wd = 60 + owned * 30, st = 50 + owned * 40;
  if ((wards_owned >> w) & 1) { *why = "ALREADY YOURS"; return 0; }
  if (dig_ward != 0xFF) { *why = "DIGGERS ARE BUSY"; return 0; }
  if (wx > 0 && ((wards_owned >> (w - 1)) & 1)) touches = 1;
  if (wx < 2 && ((wards_owned >> (w + 1)) & 1)) touches = 1;
  if (wy > 0 && ((wards_owned >> (w - 3)) & 1)) touches = 1;
  if (wy < 2 && ((wards_owned >> (w + 3)) & 1)) touches = 1;
  if (!touches) { *why = "MUST TOUCH THE REALM"; return 0; }
  if (owned >= castle_wards[castle_lvl]) { *why = "CASTLE HOLDS NO MORE"; return 0; }
  if (whole(R_GOLD) < (s16)g || whole(R_WOOD) < (s16)wd || whole(R_STONE) < (s16)st) { *why = "NOT ENOUGH GOLD/WOOD/STONE"; return 0; }
  return 1;
}

void start_annex(u8 w) {
  u8 owned = owned_count();
  spend(R_GOLD, (s32)(260 + owned * 140) << 8);
  spend(R_WOOD, (s32)(60 + owned * 30) << 8);
  spend(R_STONE, (s32)(50 + owned * 40) << 8);
  dig_ward = w; dig_weeks = 14 + owned * 2; dig_crew = 8 + owned * 2 > 24 ? 24 : 8 + owned * 2; bacc[0] = 0;
  note("THE DIGGERS BEGIN THE MOAT.");
}

u8 castle_plan_ok(const char **why) {
  u8 n = castle_lvl + 1;
  if (castle_lvl >= 3) { *why = "THE GREATEST ALREADY"; return 0; }
  if (castle_work) { *why = "THE WORK IS UNDER WAY"; return 0; }
  if (rank < castle_rank[n]) { *why = "YOUR RANK IS TOO LOW"; return 0; }
  if (whole(R_WOOD) < (s16)castle_cost[n][0] || whole(R_STONE) < (s16)castle_cost[n][1] || whole(R_IRON) < (s16)castle_cost[n][2] || whole(R_GOLD) < (s16)castle_cost[n][3]) { *why = "NOT ENOUGH MATERIALS"; return 0; }
  return 1;
}

void start_castle(void) {
  u8 n = castle_lvl + 1;
  spend(R_WOOD, (s32)castle_cost[n][0] << 8); spend(R_STONE, (s32)castle_cost[n][1] << 8);
  spend(R_IRON, (s32)castle_cost[n][2] << 8); spend(R_GOLD, (s32)castle_cost[n][3] << 8);
  castle_work = 1; castle_weeks_total = 0; castle_crew = castle_crews[n];
  note("WORK BEGINS ON THE CASTLE.");
}

/* ---------------------------------------------------------------- a new realm */
static void free_building(u8 type, u8 cx, u8 cy, u8 rad, u8 conn) {
  u8 x, y, id;
  if (!autoplace(type, cx, cy, rad, &x, &y)) return;
  id = place_building(type, x, y, 1);
  if (id != 0xFF && conn) connect_building(id);
}

static void zone_rect(u8 x0, u8 y0, u8 x1, u8 y1, u8 z) {
  u8 x, y;
  for (y = y0; y <= y1; y++) for (x = x0; x <= x1; x++) if (tile_ok_zone(x, y)) set_zone(x, y, z);
}

static void lay_line(u8 x0, u8 y0, u8 x1, u8 y1) {
  u8 x, y;
  for (y = y0; y <= y1; y++) for (x = x0; x <= x1; x++) {
    u16 k = IDX(x, y);
    if (GROUND(k) <= GR_GRASS || GROUND(k) == GR_SAND) { mt[k] &= ~0x18; mr[k] = (mr[k] & ~0x7F) | RD_DIRT; }
  }
}

void sim_new(void) {
  u8 i, x, y, n, ox, oy;
  s16 sx = 0, sy = 0, cnt = 0;
  for (i = 0; i < NRES; i++) stock[i] = 0;
  stock[R_GOLD] = 600L << 8; stock[R_WOOD] = 220L << 8; stock[R_STONE] = 110L << 8; stock[R_IRON] = 10L << 8; stock[R_TOOLS] = 6L << 8;
  stock[R_GRAIN] = 220L << 8; stock[R_PRODUCE] = 20L << 8; stock[R_MEAT] = 30L << 8; stock[R_ALE] = 20L << 8;
  pop_c = 5; pop_a = 12; pop_e = 1;
  happy = 62 * 16; health = 80 * 16; week = 0; rank = 0; castle_lvl = 0; castle_work = 0; tax = 10; diff = 2;
  harvest_k = 64; winter_k = 64; next_merchant = 12; next_raid = 110 + rndn(30); raid_gate = 0xFF; plague = 0; shock = 0; takings = 0;
  raids_won = raids_lost = raids_bought = 0; gameover = 0; food_avg = 4 * 256; dig_ward = 0xFF;
  for (i = 0; i < NRES; i++) { trade_sell[i] = 0; trade_buy[i] = 0; }
  for (i = 0; i < 8; i++) farm_crop[i] = 0;
  msg_new = 0; msgbuf[0] = 0;
  for (i = 0; i < 8; i++) chron[i][0] = 0;
  /* the hamlet: a street south of the castle, housing and a market along it, a well and granary, a farm and a woodcutter */
  lay_line(23, 27, 24, 27);
  lay_line(18, 28, 29, 28);
  zone_rect(17, 29, 30, 30, Z_RES);
  zone_rect(17, 26, 20, 27, Z_RES);
  zone_rect(27, 26, 30, 27, Z_RES);
  net_dirty = 1;
  refresh_net();
  free_building(B_GRANARY, 28, 25, 4, 1);
  free_building(B_WELL, 21, 29, 3, 0);
  for (n = 0; n < 3; n++) {
    u8 bestx = 0, besty = 0, ok = 0, tries;
    for (tries = 0; tries < 40 && !ok; tries++) {
      x = 17 + rndn(14); y = 29;
      if ((x & 1) == 0 && can_place(B_COTTAGE, x, y, 1) && ZONE(IDX(x, y)) == Z_RES) { bestx = x; besty = y; ok = 1; }
    }
    if (ok) place_building(B_COTTAGE, bestx, besty, 1);
  }
  /* a farm that already works, with fields around it, and a woodcutter in the nearest wood */
  if (autoplace(B_FARMSTEAD, 18, 20, 6, &ox, &oy)) {
    u8 id = place_building(B_FARMSTEAD, ox, oy, 1), made = 0, r;
    if (id != 0xFF) {
      connect_building(id);
      for (r = 1; r < 9 && made < 30; r++) for (y = oy > r ? oy - r : 1; y <= oy + 2 + r && y < 31; y++) for (x = ox > r ? ox - r : 1; x <= ox + 2 + r && x < 31; x++) {
        if (made < 30 && tile_ok_zone(x, y) && (x < ox || x > ox + 2 || y < oy || y > oy + 2) && ZONE(IDX(x, y)) == 0 && !(y >= 26 && y <= 30 && x >= 17)) { set_zone(x, y, Z_WHEAT); made++; }
      }
    }
  }
  for (y = 17; y < 31; y++) for (x = 17; x < 31; x++) if (TREE(IDX(x, y)) && owned_at(x, y)) { sx += x; sy += y; cnt++; }
  if (cnt > 6) free_building(B_WOODCUTTER, sx / cnt, sy / cnt, 6, 1);
  net_dirty = 1;
  sim_prime();
  note("YOUR BARONY BEGINS. BUILD ROADS, ZONE HOUSING.");
}
