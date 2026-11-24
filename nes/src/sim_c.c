/* Moat for the NES: eating, warmth, coin, and the road network flood (a swappable bank). */
#pragma code-name(push, "BANK6")
#pragma rodata-name(push, "BANK6R")
#include "sim.h"

#define bfsq ((u16 *)sbuf)
#define BQ 340
static s32 cap_for(u8 r) {
  if (r == R_GOLD) return 0x7FFFFFL;
  if (r >= R_GRAIN && r <= R_FISH) return (s32)cap_food << 8;
  return (s32)cap_goods << 8;
}

static void buy_for(u8 res, s32 n_q, u8 price) {
  s32 n = n_q >> 8, room = (cap_for(res) - stock[res]) >> 8, gold = whole(R_GOLD) - 20;
  if (gold < 0) return;
  if (n > room) n = room;
  if (n > gold / price) n = gold / price;
  if (n <= 0) return;
  spend(R_GOLD, n * price * 256);
  gain(res, n * 256);
}

static u8 foodstat[3];
static u8 foodstat[3];
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
  taxes += taxes * blk_tax >> 8;
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


void sc_consume(void) { consume(); }
void sc_money(void) { money(); }

/* flood the road network from the castle's causeway; then see which buildings a road reaches */
/* does any tile of the block's loop belong to the castle's connected network? */
static u8 block_joined(u8 b) {
  u8 k;
  for (k = 0; k < blk_s[b]; k++) {
    u16 a = IDX(blk_x[b] + k, blk_y[b]), c = IDX(blk_x[b] + k, blk_y[b] + blk_s[b] - 1), d = IDX(blk_x[b], blk_y[b] + k), e = IDX(blk_x[b] + blk_s[b] - 1, blk_y[b] + k);
    if ((conn[a >> 3] & (1 << (a & 7))) || (conn[c >> 3] & (1 << (c & 7))) || (conn[d >> 3] & (1 << (d & 7))) || (conn[e >> 3] & (1 << (e & 7)))) return 1;
  }
  return 0;
}

void sc_net(void) {
  u16 head = 0, tail = 0, k, q;
  u8 i;
  net_dirty = 0;
  for (k = 0; k < 288; k++) conn[k] = 0;
  for (k = 0; k < 2; k++) {
    q = IDX(23 + k, 27);
    if (ROAD(q) != RD_NONE) { conn[q >> 3] |= 1 << (q & 7); bfsq[tail++] = q; }
  }
  while (head != tail) {
    u16 c = bfsq[head++];
    u8 x = c % MAPW, y = c / MAPW, n;
    if (head >= BQ) head = 0;
    for (n = 0; n < 4; n++) {
      s8 nx = x, ny = y;
      if (n == 0) nx--; else if (n == 1) nx++; else if (n == 2) ny--; else ny++;
      if (nx < 0 || ny < 0 || nx >= MAPW || ny >= MAPH) continue;
      q = IDX(nx, ny);
      if (ROAD(q) == RD_NONE || (conn[q >> 3] & (1 << (q & 7)))) continue;
      conn[q >> 3] |= 1 << (q & 7);
      bfsq[tail++] = q;
      if (tail >= BQ) tail = 0;
    }
  }
  bfl[0] = BF_CONN;
  for (i = 1; i < MAXB; i++) {
    if (!bs[i]) continue;
    if (adjacent_road(bx[i], by[i], b_w[bt[i]], b_d[bt[i]], 1)) bfl[i] |= BF_CONN;
    else {
      /* the heart of a block touches no street of its own: the block's loop reaches it, if the loop joins the castle's roads */
      u8 b = block_at(bx[i], by[i]);
      if (b != 0xFF && block_joined(b)) bfl[i] |= BF_CONN; else bfl[i] &= ~BF_CONN;
    }
  }
}

