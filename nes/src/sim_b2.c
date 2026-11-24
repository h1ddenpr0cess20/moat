/* Moat for the NES: growth, building, annexing and the castle (a swappable bank). */
#pragma code-name(push, "BANK4")
#pragma rodata-name(push, "BANK4R")
#include "sim.h"

static const u8 grown_b[4][3] = {
  { B_COTTAGE, B_HOUSE, B_TOWNHOUSE }, { B_STALL, B_SHOP, B_MERCHANT }, { B_CROFT, B_SMALLHOLDING, B_GRANGE }, { B_WORKSHOP, B_JOINERY, B_GUILDHALL },
};
static u8 zone_idx(u8 zone) { return zone == Z_RES ? 0 : zone == Z_MARKET ? 1 : zone == Z_CROFT ? 2 : 3; }

static u8 b2_place_player(u8 type, u8 x, u8 y) {
  u8 id;
  if (!can_place(type, x, y, 0)) return 0xFF;
  if (b_rank[type] > rank) return 0xFE;
  if (!pay_ok(type)) return 0xFD;
  pay_cost(type);
  id = place_building(type, x, y, 0);
  if (id != 0xFF) { u8 a = b_auto[type]; (void)a; }
  return id;
}

static void b2_demolish_at(u8 x, u8 y) {
  u8 id = find_building(x, y), t, i, j;
  if (id == 0xFF || id == 0) return;
  t = bt[id];
  /* half the timber and stone comes back */
  gain(R_WOOD, (s32)b_cw[t] << 7); gain(R_STONE, (s32)b_cs[t] << 7);
  i = b_w[t]; j = b_d[t];
  { u8 xx = bx[id], yy = by[id], ww = b_w[t], dd = b_d[t]; remove_building(id); mark_rect(xx, yy, ww, dd); (void)i; (void)j; }
}

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

static u8 spawn_cd[9];
static u8 b2_spawn(u8 zone, u8 level) {
  u8 t = grown_b[zone_idx(zone)][level - 1], w, found = 0, bestx = 0, besty = 0, evals = 0, k, start;
  s16 best = -32000, sc;
  if (spawn_cd[zone] || !pay_ok(t)) return 0;
  for (w = 0; w < 9 && evals < 6; w++) {
    if (!((wards_owned >> w) & 1)) continue;
    start = rnd8();
    for (k = 0; k < 255 && evals < 6; k++) {
      u8 pos = start + k, x = ((w % 3) << 4) + (pos & 15), y = ((w / 3) << 4) + (pos >> 4);
      if (ZONE(IDX(x, y)) != zone || (mr[IDX(x, y)] & BLD)) continue;
      evals++;
      if (!lot_ok(zone, t, x, y)) continue;
      sc = lot_score(t, x, y);
      if (sc > best) { best = sc; bestx = x; besty = y; found = 1; }
    }
  }
  if (!found) { spawn_cd[zone] = 4 + rndn(4); return 0; }
  pay_cost(t);
  place_building(t, bestx, besty, 0);
  mark_rect(bestx, besty, b_w[t], b_d[t]);
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
  { u8 bk = block_at(bx[pick], by[pick]); if (happy < (lvl == 1 ? 52 : 62) * 16 - (bk != 0xFF ? (s16)blk_rate[bk] * 10 * 16 / 255 : 0)) return; }
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
  mark_rect(x, y, w2, d2);
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
      mark_rect(bx[i], by[i], b_w[t], b_d[t]);
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

static s16 homes_wanted(void) { u16 people = headcount(); return housing_cap(1) < (s16)(people + 6 + people / 8); }

static void grow_homes(void) {
  u16 people = headcount(), supply = 0;
  u8 i, n;
  for (n = 0; n < 9; n++) if (spawn_cd[n]) spawn_cd[n]--;
  if (homes_wanted()) {
    n = 0;
    while (n < 2 && housing_cap(1) < people + 6 + people / 8 + n && b2_spawn(Z_RES, 1)) n++;
  }
  for (i = 1; i < MAXB; i++) if (bs[i]) supply += b_shop[bt[i]];
  if (people >= 15 && supply * 4 < people * 3) b2_spawn(Z_MARKET, 1);
}

static void grow_industry(void) {
  u16 people = headcount();
  u8 i;
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
      if (idle - coming >= 2) { if (hungry || tool_glut()) { if (!b2_spawn(Z_CROFT, 1)) b2_spawn(Z_CRAFT, 1); } else if (!b2_spawn(Z_CRAFT, 1)) b2_spawn(Z_CROFT, 1); }
      else if (going < 2) {
        if (hungry && homes_wanted()) b2_spawn(Z_CROFT, 1);
        else if (whole(R_TOOLS) < 3 + (s16)(people / 25) && !idle_shops) b2_spawn(Z_CRAFT, 1);
      }
    }
  }
}

static void grow_misc(void) {
  u8 n;
  if ((week & 1) == 0) upgrade_try();
  /* the forest grows back */
  if (season_of(week) <= 1) {
    for (n = 0; n < 4; n++) {
      u8 x = rndn(MAPW), y = rndn(MAPH);
      u16 k = IDX(x, y);
      if (TREE(k) > 0 && TREE(k) < 3) { mt[k] += 1 << 3; mark(x, y); }
      else if (TREE(k) > 0 && rndn(4) == 0) {
        u8 nx = x + (rndn(2) ? 1 : -1), ny = y + (rndn(3) - 1);
        if (nx < MAPW && ny < MAPH) {
          u16 j = IDX(nx, ny);
          if (!TREE(j) && GROUND(j) == GR_GRASS && !(mr[j] & BLD) && !ROAD(j) && !ZONE(j) && (!owned_at(nx, ny) || rndn(3) == 0)) { mt[j] |= 1 << 3; mark(nx, ny); }
        }
      }
    }
  }
}

static u8 b2_plan_annex_ok(u8 w, const char **why) {
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

static void b2_start_annex(u8 w) {
  u8 owned = owned_count();
  spend(R_GOLD, (s32)(260 + owned * 140) << 8);
  spend(R_WOOD, (s32)(60 + owned * 30) << 8);
  spend(R_STONE, (s32)(50 + owned * 40) << 8);
  dig_ward = w; dig_weeks = 14 + owned * 2; dig_crew = 8 + owned * 2 > 24 ? 24 : 8 + owned * 2; bacc[0] = 0;
  note("THE DIGGERS BEGIN THE MOAT.");
}

static u8 b2_castle_plan_ok(const char **why) {
  u8 n = castle_lvl + 1;
  if (castle_lvl >= 3) { *why = "THE GREATEST ALREADY"; return 0; }
  if (castle_work) { *why = "THE WORK IS UNDER WAY"; return 0; }
  if (rank < castle_rank[n]) { *why = "YOUR RANK IS TOO LOW"; return 0; }
  if (whole(R_WOOD) < (s16)castle_cost[n][0] || whole(R_STONE) < (s16)castle_cost[n][1] || whole(R_IRON) < (s16)castle_cost[n][2] || whole(R_GOLD) < (s16)castle_cost[n][3]) { *why = "NOT ENOUGH MATERIALS"; return 0; }
  return 1;
}

static void b2_start_castle(void) {
  u8 n = castle_lvl + 1;
  spend(R_WOOD, (s32)castle_cost[n][0] << 8); spend(R_STONE, (s32)castle_cost[n][1] << 8);
  spend(R_IRON, (s32)castle_cost[n][2] << 8); spend(R_GOLD, (s32)castle_cost[n][3] << 8);
  castle_work = 1; castle_weeks_total = 0; castle_crew = castle_crews[n];
  note("WORK BEGINS ON THE CASTLE.");
}

/* stamp a loop of street with a ring of plots three deep and a core, each zoned as chosen */
static u8 b2_block(u8 x, u8 y) {
  u8 i, j, s = blk_size, d;
  u16 cost = 0, k;
  if (blk_n >= MAXBLK) return 1;
  if (!block_ok(x, y, s)) return 2;
  for (j = 0; j < s; j++) for (i = 0; i < s; i++) {
    d = i < j ? i : j;
    if (s - 1 - i < d) d = s - 1 - i;
    if (s - 1 - j < d) d = s - 1 - j;
    k = IDX(x + i, y + j);
    if (d == 0) { if (ROAD(k) == RD_NONE) cost++; }
    else if (d <= 3) cost += zone_cost[blk_ring];
    else if (blk_core) cost += zone_cost[blk_core];
  }
  if (whole(R_GOLD) < (s16)cost) return 3;
  spend(R_GOLD, (s32)cost << 8);
  for (j = 0; j < s; j++) for (i = 0; i < s; i++) {
    d = i < j ? i : j;
    if (s - 1 - i < d) d = s - 1 - i;
    if (s - 1 - j < d) d = s - 1 - j;
    k = IDX(x + i, y + j);
    mt[k] &= ~0x18;
    if (d == 0) mr[k] = (mr[k] & ~0x7F) | RD_DIRT;
    else if (d <= 3) mr[k] = (mr[k] & ~0x78) | (blk_ring << 3);
    else mr[k] = (mr[k] & ~0x78) | (blk_core << 3);
  }
  blk_x[blk_n] = x; blk_y[blk_n] = y; blk_s[blk_n] = s; blk_cz[blk_n] = blk_core; blk_rate[blk_n] = 0; blk_n++;
  net_dirty = 1;
  view_dirty = 1;
  return 0;
}

static void cpy_why(const char *w) { u8 i; for (i = 0; i < 31 && w[i]; i++) whybuf[i] = w[i]; whybuf[i] = 0; }
void sb2_construct(void) { construct(); }
void sb2_grow_a(void) { grow_homes(); }
void sb2_grow_b(void) { grow_industry(); }
void sb2_grow_c(void) { grow_misc(); }
void sb2_place(void) { ret_a = b2_place_player(arg_a, arg_b, arg_c); }
void sb2_demolish(void) { b2_demolish_at(arg_a, arg_b); }
void sb2_annex_ok(void) { const char *w = ""; ret_a = b2_plan_annex_ok(arg_a, &w); cpy_why(w); }
void sb2_start_annex(void) { b2_start_annex(arg_a); }
void sb2_castle_ok(void) { const char *w = ""; ret_a = b2_castle_plan_ok(&w); cpy_why(w); }
void sb2_start_castle(void) { b2_start_castle(); }
void sb2_block(void) { ret_a = b2_block(arg_a, arg_b); }
