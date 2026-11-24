/* Moat for the NES: the economy: jobs, fields, work, eating, coin (a swappable bank). */
#pragma code-name(push, "BANK0")
#pragma rodata-name(push, "BANK0R")
#include "sim.h"

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

/* what a building in a block's core is worth to the homes around it, x255 (the web game's HEARTS table) */
static u8 heart_fine(u8 t) {
  switch (t) {
    case B_GREEN: return 217;
    case B_CATHEDRAL: return 230;
    case B_CHAPEL: case B_MARKET: return 153;
    case B_WELL: case B_GRANARY: case B_SMITHY: case B_MILL: return 102;
    case B_TAVERN: case B_HEALER: case B_STOREHOUSE: return 115;
    case B_WATCHTOWER: return 89;
    case B_BARRACKS: return 77;
  }
  return 0;
}
static u8 crop_fine(u8 z) { return z == Z_RES ? 77 : z == Z_VEG ? 92 : z == Z_ORCHARD ? 107 : z == Z_PASTURE ? 82 : z == Z_WHEAT ? 64 : 0; }

/* rate every block the way the web game does: how built the ring is, times what is at the heart */
static void blocks_week(void) {
  u8 b, i, k;
  u32 weighted = 0, total = 0, rooms = 0;
  static u8 cnt[NB];
  for (b = 0; b < blk_n; b++) {
    u8 x0 = blk_x[b], y0 = blk_y[b], s = blk_s[b], x, y, nh = 0;
    u16 zoned = 0, built = 0, corelen = 0, cropn[9], bestn = 0, cap = 0;
    u8 crop = 0, heart, closure, fill, rest = 255;
    for (k = 0; k < NB; k++) cnt[k] = 0;
    for (k = 0; k < 9; k++) cropn[k] = 0;
    for (k = 0; k < 7; k++) blk_serve[b][k] = 0;
    for (y = y0; y < y0 + s; y++) for (x = x0; x < x0 + s; x++) {
      u8 d = block_depth(b, x, y), z;
      u16 t = IDX(x, y);
      if (d == 0) continue;
      z = ZONE(t);
      if (d <= 3) { if (!(mr[t] & BLD) && (z == Z_RES || z == Z_MARKET || z == Z_CROFT || z == Z_CRAFT)) zoned++; }
      else { corelen++; if (!(mr[t] & BLD) && crop_fine(z)) cropn[z]++; }
    }
    for (i = 1; i < MAXB; i++) {
      u8 t = bt[i], d;
      if (bs[i] == BS_FREE || block_at(bx[i], by[i]) != b) continue;
      d = block_depth(b, bx[i], by[i]);
      if (d >= 1 && d <= 3 && b_auto[t]) {
        zoned++;
        if (bs[i] == BS_READY) { built++; if (b_cap[t] || b_auto[t] == Z_CROFT || b_auto[t] == Z_CRAFT) nh++; if (b_cap[t]) cap += b_cap[t]; }
      } else if (d > 3 && bs[i] == BS_READY && heart_fine(t)) {
        cnt[t]++;
        { u8 q, sk;
          for (q = 0; q < 2; q++) {
            u8 kind = q ? sv_k2[t] : sv_k[t];
            if (!kind) continue;
            sk = (u8)(((u16)(q ? sv_v2[t] : sv_v[t]) * 51 / 2 > 255 ? 255 : (u16)(q ? sv_v2[t] : sv_v[t]) * 51 / 2) * (b_jobs[t] ? (128 + (bef[i] >> 1)) : 255) / 255);
            if (sk > blk_serve[b][kind]) blk_serve[b][kind] = sk;
          }
        }
      }
    }
    for (k = 0; k < NB; k++) if (cnt[k]) {
      u16 fine = (u16)heart_fine(k) * (100 + 15 * (cnt[k] - 1)) / 100;
      if (fine > 242) fine = 242;
      rest = (u8)((u16)rest * (255 - fine) / 255);
    }
    for (k = 1; k < 9; k++) if (cropn[k] > bestn) { bestn = cropn[k]; crop = k; }
    if (crop && corelen) {
      u32 share = (u32)bestn * 255 * 100 / ((u32)corelen * 55);
      if (share > 255) share = 255;
      rest = (u8)((u16)rest * (255 - (u16)crop_fine(crop) * share / 255) / 255);
    }
    heart = corelen >= 4 ? 255 - rest : 0;
    fill = zoned ? (u8)((u32)built * 255 / zoned) : 0;
    closure = fill > 89 ? (u8)(((u16)(fill - 89) * 255 / 140) > 255 ? 255 : (u16)(fill - 89) * 255 / 140) : 0;
    closure = (u8)((u16)closure * (nh >= 6 ? 255 : nh * 255 / 6) / 255);
    blk_rate[b] = (u8)((u16)heart * closure / 255);
    weighted += (u32)cap * blk_rate[b];
    rooms += (u32)cap * blk_rate[b];
  }
  for (i = 1; i < MAXB; i++) if (bs[i] == BS_READY && b_cap[bt[i]]) total += b_cap[bt[i]];
  total += 1;
  {
    u32 share = weighted * 256 / (total * 255);
    blk_tax = (s16)(share * 46 / 256);          /* 18% of the tax, in proportion */
    blk_mood = (s16)(share * 7 * 16 / 256);     /* up to +7 */
    blk_cap = (s16)(rooms / 255 * 12 / 100);    /* 12% more beds */
  }
}

static void compute_care(void) {
  u8 k, i, n;
  u32 acc[7], weight = 0;
  for (k = 1; k < 7; k++) acc[k] = 0;
  svc_rebuild();
  blocks_week();
  for (i = 1; i < MAXB; i++) {
    u8 t = bt[i], cxm, cym, w, blk;
    u16 c[7];
    if (bs[i] != BS_READY || !b_cap[t]) continue;
    cxm = bx[i] + (b_w[t] >> 1); cym = by[i] + (b_d[t] >> 1); w = b_cap[t];
    for (k = 1; k < 7; k++) c[k] = 0;
    for (n = 0; n < svc_n; n++) {
      u8 s = svc_i[n], st = bt[s];
      s16 dx = (s16)bx[s] + (b_w[st] >> 1) - cxm, dy = (s16)by[s] + (b_d[st] >> 1) - cym, d;
      if (dx < 0) dx = -dx;
      if (dy < 0) dy = -dy;
      d = dx > dy ? dx + (dy >> 1) : dy + (dx >> 1);
      if (d > sv_r[st]) continue;
      c[sv_k[st]] += (u16)((u32)sv_v[st] * 16 * (sv_r[st] + 1 - d) / (sv_r[st] + 1));
      if (sv_k2[st]) c[sv_k2[st]] += (u16)((u32)sv_v2[st] * 16 * (sv_r[st] + 1 - d) / (sv_r[st] + 1));
    }
    /* a heart in a block lends its services to every door in the ring */
    blk = block_at(bx[i], by[i]);
    for (k = 1; k < 7; k++) {
      if (c[k] > 255) c[k] = 255;
      if (blk != 0xFF && blk_serve[blk][k] > c[k]) c[k] = blk_serve[blk][k];
      acc[k] += (u32)c[k] * w;
    }
    weight += w;
  }
  for (k = 1; k < 7; k++) {
    u32 v = weight ? acc[k] / weight : 0;
    care[k] = v > 255 ? 255 : (u8)v;
  }
}

static u8 is_foodish(u8 t) { return b_cat[t] == 2 || w_kind[t] == 2; }

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

u8 pj[MAXB], pp[MAXB];
static void allocate(void) {
  u8 i, p, prio;
  s32 workforce = (s32)pop_a * 29;     /* x32: 0.9 of the adults */
  s32 labor = workforce, employed = 0, open = 0, wanted = 0, reserve, budget, crewgiven = 0;
  s32 garr = (s32)castle_jobs[castle_lvl] * 32;
  s32 got = labor < garr ? labor : garr;
  labor -= got; employed += got;
  bef[0] = garr ? (u8)(got * 255 / garr) : 255;
  for (i = 1; i < MAXB; i++) {
    pj[i] = 0;
    if (bs[i]) { bwk[i] = 0; bef[i] = 0; }
    if (bs[i] != BS_READY) continue;
    if (!(bfl[i] & BF_CONN)) continue;
    if (bt[i] != B_FARMSTEAD && !b_jobs[bt[i]]) continue;
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

static void assign_fields(void) {
  u8 i, n = 0, k, w, pos, x, y;
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
  for (w = 0; w < 9; w++) {
    u8 x0, y0;
    if (!(wards_owned & wbit[w])) continue;
    x0 = (w % 3) << 4; y0 = (w / 3) << 4;
    for (pos = 0; ; pos++) {
      u16 t;
      u8 z, best = 0xFF;
      s16 bd = 99, dx, dy, d2;
      x = x0 + (pos & 15); y = y0 + (pos >> 4);
      t = IDX(x, y);
      z = ZONE(t);
      if (z >= Z_WHEAT && z <= Z_PASTURE && !(mr[t] & BLD)) {
        for (i = 0; i < n; i++) {
          u8 f = farm_slot[i];
          dx = (s16)x - bx[f] - 1; dy = (s16)y - by[f] - 1;
          d2 = dx * dx + dy * dy;
          if (d2 <= 36 && d2 < bd && farm_fields[i] < 48) { bd = d2; best = i; }
        }
        if (best != 0xFF) { farm_n[best][z - Z_WHEAT]++; farm_fields[best]++; }
      }
      if (pos == 255) break;
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

static u16 pick_tile(u8 cx, u8 cy, u8 r, u8 kind) {
  u8 tries, x, y;
  u16 i;
  for (tries = 0; tries < 14; tries++) {
    s16 xx = cx + (s16)rndn(2 * r + 1) - r, yy = cy + (s16)rndn(2 * r + 1) - r;
    if (xx < 1 || yy < 1 || xx >= MAPW - 1 || yy >= MAPH - 1) continue;
    x = xx; y = yy;
    if (!owned_at(x, y)) continue;
    i = IDX(x, y);
    pk_x = x; pk_y = y;
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
          mark(pk_x, pk_y);
        }
      } else if (from == 1 || from == 2) {
        while (bacc[i] >= 7680) {
          u16 j = pick_tile(cx, cy, r, from);
          bacc[i] -= 7680;
          if (j == 0xFFFF) break;
          if (AMT(j) > 0) mt[j] -= 1 << 5;
          if (AMT(j) == 0) mt[j] = GR_GRASS;
          mark(pk_x, pk_y);
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
        mark(pk_x, pk_y);
      }
      if (sk > 0 && rndn(3) == 0) {
        u16 j = pick_tile(cx, cy, w_rad[t], 4);
        if (j != 0xFFFF) { mt[j] += 1 << 3; mark(pk_x, pk_y); }
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
  /* kitchen gardens behind the houses */
  {
    s32 beds = (s32)housing_cap(0) - 8;
    if (beds > 0) gain(R_PRODUCE, beds * 7 * (s == 0 ? 51 : s == 1 ? 77 : s == 2 ? 96 : 13) >> 6);
  }
  if (tool_use) { s32 wear = tool_use * 3 / 32; if (wear > stock[R_TOOLS]) wear = stock[R_TOOLS]; spend(R_TOOLS, wear); }
}

void sa_pre(void) { capacity(); assign_fields(); allocate(); }
void sa_work(void) { farm_week(); work_week(); }
void sa_care(void) { compute_care(); }
