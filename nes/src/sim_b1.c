/* Moat for the NES: people, mood and events (a swappable bank). */
#pragma code-name(push, "BANK1")
#pragma rodata-name(push, "BANK1R")
#include "sim.h"

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
  h += (s32)blk_mood * 16;
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

void burn_building(u8 id) {
  u8 t = bt[id], z = b_auto[t], i, j, x = bx[id], y = by[id], w = b_w[t], d = b_d[t];
  remove_building(id);
  if (z) for (j = 0; j < d; j++) for (i = 0; i < w; i++) set_zone(x + i, y + j, z);
  note("A BUILDING BURNED DOWN.");
  snd_play(4);
  mark_rect(x, y, w, d);
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
    { u8 bk = block_at(bx[i], by[i]); if (bk != 0xFF) chance = chance * (255 - (u16)blk_rate[bk] * 30 / 100) / 255; }
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

void sb1_weather(void) { weather_week(); }
void sb1_mood(void) { mood_week(); }
void sb1_people(void) { people_week(); }
void sb1_events(void) {
  merchants_week();
  if (diff) { fires_week(); plague_week(); raids_week(); }
  tribute_week();
  ranks_week();
}
