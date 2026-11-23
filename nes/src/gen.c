/* Moat for the NES: making the land and a new realm (a swappable bank). */
#pragma code-name(push, "BANK3")
#pragma rodata-name(push, "BANK3R")
#include "sim.h"

static void set_tree(u8 x, u8 y, u8 lv) {
  u16 i;
  if (x >= MAPW || y >= MAPH) return;
  i = IDX(x, y);
  if (GROUND(i) != GR_GRASS) return;
  mt[i] = (mt[i] & ~0x18) | (lv << 3);
}
static void set_ground(u8 x, u8 y, u8 g) {
  u16 i;
  if (x >= MAPW || y >= MAPH) return;
  i = IDX(x, y);
  mt[i] = g | (g == GR_ROCK || g == GR_ORE ? 7 << 5 : 0);
}

/* kind: 0 forest, 1 rock, 2 rock with ore, 3 pond */
static void blob(s8 cx, s8 cy, u8 r, u8 kind) {
  s8 dx, dy;
  s16 d2, r2 = r * r + r / 2;
  u8 p, x, y;
  for (dy = -(s8)r; dy <= (s8)r; dy++) {
    for (dx = -(s8)r; dx <= (s8)r; dx++) {
      x = cx + dx; y = cy + dy;
      if (cx + dx < 1 || cy + dy < 1 || cx + dx > MAPW - 2 || cy + dy > MAPH - 2) continue;
      d2 = dx * dx + dy * dy;
      if (d2 > r2) continue;
      p = 100 - (u8)((u16)d2 * 55 / (r2 + 1));
      if (rndn(100) >= p) continue;
      if (kind == 0) { set_tree(x, y, d2 * 3 < r2 ? 3 : 1 + rndn(2)); }
      else if (kind == 3) set_ground(x, y, GR_WATER);
      else {
        mt[IDX(x, y)] &= ~0x18;
        set_ground(x, y, kind == 2 && rndn(100) < 35 ? GR_ORE : GR_ROCK);
      }
    }
  }
}

static void river(void) {
  s8 rx = rndn(2) ? 4 + rndn(7) : 36 + rndn(8);
  u8 y;
  for (y = 0; y < MAPH; y++) {
    set_ground(rx, y, GR_WATER); set_ground(rx + 1, y, GR_WATER);
    if (rndn(3) == 0) rx += rndn(2) ? 1 : -1;
    if (rx < 3) rx = 3;
    if (rx > 12 && rx < 30) rx = 12;
    if (rx >= 30 && rx < 35) rx = 35;
    if (rx > 44) rx = 44;
  }
}

static void clear_rect(u8 x0, u8 y0, u8 x1, u8 y1) {
  u8 x, y;
  for (y = y0; y <= y1; y++) for (x = x0; x <= x1; x++) mt[IDX(x, y)] = GR_GRASS;
}

void world_gen(u16 seed) {
  u8 i, x, y;
  u16 k;
  rng = seed ? seed : 0xACE1;
  for (k = 0; k < NTILES; k++) { mt[k] = 0; mr[k] = 0; }
  for (k = 0; k < 288; k++) conn[k] = 0;
  for (i = 0; i < MAXB; i++) bs[i] = 0;
  for (i = 0; i < 9; i++) gate_open[i] = 0;
  /* the wild: forests, rock, ore, a river and a pond or two */
  for (i = 0; i < 9; i++) blob(3 + rndn(42), 3 + rndn(42), 3 + rndn(3), 0);
  for (i = 0; i < 4; i++) blob(3 + rndn(42), 3 + rndn(42), 2 + rndn(2), 2);
  for (i = 0; i < 2; i++) blob(3 + rndn(42), 3 + rndn(42), 2 + rndn(2), 3);
  river();
  /* the home ward gets a small wood of its own in its north-west corner, for the woodcutter */
  blob(18, 18, 2, 0);
  /* the castle's island and its bank, and the street south of it, are clear */
  clear_rect(19, 20, 30, 28);
  for (x = 17; x <= 30; x++) { mt[IDX(x, 29)] = GR_GRASS; mt[IDX(x, 30)] = GR_GRASS; }
  for (x = 21; x <= 30; x++) for (y = 17; y <= 19; y++) mt[IDX(x, y)] = GR_GRASS;
  /* sand along the water */
  for (y = 1; y < MAPH - 1; y++) for (x = 1; x < MAPW - 1; x++) {
    k = IDX(x, y);
    if (GROUND(k) != GR_GRASS) continue;
    if ((GROUND(k - 1) == GR_WATER || GROUND(k + 1) == GR_WATER || GROUND(k - MAPW) == GR_WATER || GROUND(k + MAPW) == GR_WATER) && rndn(100) < 55) mt[k] = (mt[k] & ~0x18) | GR_SAND;
  }
  wards_owned = 1 << 4;
  dig_ward = 0xFF;
  rebuild_moat();
  /* the castle itself is building 0 */
  bs[0] = BS_READY; bt[0] = 255; bx[0] = CX0; by[0] = CY0;
  for (y = CY0; y < CY0 + 4; y++) for (x = CX0; x < CX0 + 4; x++) { k = IDX(x, y); mr[k] |= BLD; mt[k] = GR_GRASS; }
  for (x = 23; x <= 24; x++) { k = IDX(x, 27); mt[k] = GR_GRASS; mr[k] = (mr[k] & ~7) | RD_DIRT; }
  net_dirty = 1;
}

/* a spot for a building near (cx, cy) */
u8 autoplace(u8 type, u8 cx, u8 cy, u8 radius, u8 *ox, u8 *oy) {
  s16 best = 32000, d2, dx, dy;
  u8 found = 0, x, y;
  s16 x0 = cx - radius, y0 = cy - radius;
  for (y = y0 < 1 ? 1 : y0; y <= cy + radius && y < MAPH; y++) {
    for (x = x0 < 1 ? 1 : x0; x <= cx + radius && x < MAPW; x++) {
      dx = (s16)x - cx; dy = (s16)y - cy;
      d2 = dx * dx + dy * dy;
      if (d2 >= best) continue;
      if (!can_place(type, x, y, 1)) continue;
      best = d2; *ox = x; *oy = y; found = 1;
    }
  }
  return found;
}

static u8 lay(u8 x, u8 y) {
  u16 k = IDX(x, y);
  if (ROAD(k)) return 1;
  if (GROUND(k) > GR_GRASS && GROUND(k) != GR_SAND) return 0;
  if (TREE(k) || (mr[k] & BLD) || !owned_at(x, y)) return 0;
  mr[k] = (mr[k] & ~0x7F) | RD_DIRT;
  return 1;
}

/* lay dirt road from a building to the nearest connected road, free of charge (the hamlet's own streets) */
void connect_building(u8 id) {
  u8 w = b_w[bt[id]], d = b_d[bt[id]];
  s8 sx = bx[id] + w / 2, sy = by[id] + d;    /* below the front */
  s8 tx = 24, ty = 28, x, y;
  u16 best = 65000, c;
  u8 yy, xx;
  refresh_net();
  /* nearest connected road tile */
  for (yy = 17; yy < 31; yy++) for (xx = 17; xx < 31; xx++) {
    c = IDX(xx, yy);
    if (ROAD(c) && (conn[c >> 3] & (1 << (c & 7)))) {
      u16 dist = (xx > sx ? xx - sx : sx - xx) + (yy > sy ? yy - sy : sy - yy);
      if (dist < best) { best = dist; tx = xx; ty = yy; }
    }
  }
  x = sx; y = sy;
  if (y >= MAPH) return;
  lay(x, y);
  while (x != tx || y != ty) {
    if (x != tx && (y == ty || (x - tx > 1 || tx - x > 1))) { if (!lay(x + (tx > x ? 1 : -1), y)) break; x += tx > x ? 1 : -1; }
    else if (y != ty) { if (!lay(x, y + (ty > y ? 1 : -1))) break; y += ty > y ? 1 : -1; }
    else if (x != tx) { if (!lay(x + (tx > x ? 1 : -1), y)) break; x += tx > x ? 1 : -1; }
  }
  net_dirty = 1;
}

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
  u8 i, x, y, n;
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
  /* the hamlet: a road around the castle, houses along its south side, a farm and a woodcutter to the north */
  lay_line(23, 27, 24, 27);
  lay_line(19, 28, 28, 28);
  lay_line(19, 20, 19, 28);
  lay_line(28, 20, 28, 28);
  lay_line(19, 20, 28, 20);
  zone_rect(17, 29, 30, 30, Z_RES);
  zone_rect(17, 21, 18, 28, Z_RES);
  zone_rect(29, 21, 30, 28, Z_RES);
  net_dirty = 1;
  refresh_net();
  place_building(B_WELL, 20, 27, 1);
  for (n = 0; n < 3; n++) {
    x = 20 + n * 2;
    mt[IDX(x, 29)] = GR_GRASS; mt[IDX(x + 1, 29)] = GR_GRASS; mt[IDX(x, 30)] = GR_GRASS; mt[IDX(x + 1, 30)] = GR_GRASS;
    place_building(B_COTTAGE, x, 29, 1);
  }
  /* a farm that already works, with wheat around it, and a woodcutter in the wood */
  place_building(B_FARMSTEAD, 22, 17, 1);
  zone_rect(25, 17, 28, 19, Z_WHEAT);
  zone_rect(29, 17, 30, 19, Z_VEG);
  zone_rect(20, 17, 21, 19, Z_WHEAT);
  mt[IDX(20, 18)] = GR_GRASS; mt[IDX(21, 18)] = GR_GRASS; mt[IDX(20, 19)] = GR_GRASS; mt[IDX(21, 19)] = GR_GRASS;
  mr[IDX(20, 18)] &= 0x87; mr[IDX(21, 18)] &= 0x87; mr[IDX(20, 19)] &= 0x87; mr[IDX(21, 19)] &= 0x87;
  place_building(B_WOODCUTTER, 20, 18, 1);
  place_building(B_FISHERY, 29, 22, 1);
  for (y = 17; y <= 19; y++) for (x = 17; x <= 19; x++) mr[IDX(x, y)] &= 0x87;
  net_dirty = 1;
  sim_prime();
  note("YOUR BARONY BEGINS. BUILD ROADS, ZONE HOUSING.");
}

void gen_new(void) {
  world_gen(rng);
  sim_new();
}
