/* The land: generation, the moat and its gates, buildings on the map, the road network. */
#include "game.h"

u8 mt[NTILES];
u8 mr[NTILES];
u8 conn[288];
u16 wards_owned;
u8 gate_open[WARDS * WARDS];
u8 dig_ward, dig_weeks, dig_prog, dig_crew;
u8 bt[MAXB], bx[MAXB], by[MAXB], bs[MAXB], bprog[MAXB], bwk[MAXB], bef[MAXB], bfl[MAXB], baux[MAXB];
u16 bacc[MAXB];
u8 net_dirty;
u16 rng;

#pragma bss-name(push, "SCREEN")
u8 sbuf[704];
#pragma bss-name(pop)


u16 rnd16(void) {
  rng ^= rng << 7;
  rng ^= rng >> 9;
  rng ^= rng << 8;
  return rng;
}
u8 rnd8(void) { return rnd16() >> 8; }
u8 rndn(u8 n) { return ((u16)rnd8() * n) >> 8; }

u8 ward_of(u8 x, u8 y) { static const u8 r3[3] = { 0, 3, 6 }; return r3[y >> 4] + (x >> 4); }
u8 owned_at(u8 x, u8 y) { return (wards_owned & wbit[ward_of(x, y)]) != 0; }
u8 own_w(s8 wx, s8 wy) {
  if (wx < 0 || wy < 0 || wx >= WARDS || wy >= WARDS) return 0;
  return (wards_owned >> (wy * WARDS + wx)) & 1;
}
u8 owned_count(void) {
  u8 n = 0, i;
  for (i = 0; i < 9; i++) n += (wards_owned >> i) & 1;
  return n;
}
u8 is_water(u8 g) { return g == GR_WATER || g == GR_MOAT; }

/* ---------------------------------------------------------------- moat and gates */
u8 gate_exposed(u8 w, u8 side) {
  s8 wx = w % WARDS, wy = w / WARDS;
  if (!own_w(wx, wy)) return 0;
  switch (side) {
    case 0: return !own_w(wx, wy - 1);
    case 1: return !own_w(wx + 1, wy);
    case 2: return !own_w(wx, wy + 1);
    default: return !own_w(wx - 1, wy);
  }
}

/* the first of the two gate tiles of a side; the other is next along it */
u8 gate_pos(u8 w, u8 side, u8 *gx, u8 *gy) {
  u8 x0 = (w % WARDS) << 4, y0 = (w / WARDS) << 4;
  switch (side) {
    case 0: *gx = x0 + 7; *gy = y0; break;
    case 1: *gx = x0 + 15; *gy = y0 + 7; break;
    case 2: *gx = x0 + 7; *gy = y0 + 15; break;
    default: *gx = x0; *gy = y0 + 7; break;
  }
  return side & 1;     /* 1: the second tile is below, 0: to the right */
}

void gate_tiles(u8 w, u8 side, u8 road) {
  u8 gx, gy, vert = gate_pos(w, side, &gx, &gy);
  u16 i = IDX(gx, gy), j = vert ? IDX(gx, gy + 1) : IDX(gx + 1, gy);
  mr[i] = (mr[i] & ~7) | road;
  mr[j] = (mr[j] & ~7) | road;
}

void gate_set(u8 w, u8 side, u8 open) {
  if (open) gate_open[w] |= 1 << side; else gate_open[w] &= ~(1 << side);
  gate_tiles(w, side, open ? RD_GATE : RD_NONE);
  net_dirty = 1;
}

void rebuild_moat(void) { extern void mc_rebuild(void); call_in(BK_SIMC, mc_rebuild); }
void complete_annex(u8 wd) { extern void mc_annex(void); arg_a = wd; call_in(BK_SIMC, mc_annex); }

/* ---------------------------------------------------------------- near things */
u8 count_near(u8 cx, u8 cy, u8 r, u8 kind) {
  u8 n = 0, x, y, x0 = cx > r ? cx - r : 0, y0 = cy > r ? cy - r : 0, x1 = cx + r, y1 = cy + r;
  s16 dx, dy, r2 = r * r + r;
  u16 i;
  u8 g;
  if (x1 >= MAPW) x1 = MAPW - 1;
  if (y1 >= MAPH) y1 = MAPH - 1;
  for (y = y0; y <= y1; y++) {
    dy = (s16)y - cy;
    for (x = x0; x <= x1; x++) {
      dx = (s16)x - cx;
      if (dx * dx + dy * dy > r2) continue;
      i = IDX(x, y);
      g = GROUND(i);
      if (kind == 3) { if (is_water(g)) n++; }
      else if (!owned_at(x, y)) continue;
      else if (kind == 0) { if (TREE(i)) n++; }
      else if (kind == 4) { if (TREE(i) > 1) n++; }
      else if (kind == 1) { if (g == GR_ROCK) n++; }
      else if (g == GR_ORE) n++;
      if (n >= 250) return n;
    }
  }
  return n;
}

/* ---------------------------------------------------------------- roads and zones */
void set_road(u8 x, u8 y, u8 r) {
  u16 i = IDX(x, y);
  mr[i] = (mr[i] & ~7) | r;
  net_dirty = 1;
}
void set_zone(u8 x, u8 y, u8 z) {
  u16 i = IDX(x, y);
  mr[i] = (mr[i] & ~0x78) | (z << 3);
}
u8 tile_ok_zone(u8 x, u8 y) {
  u16 i;
  if (x >= MAPW || y >= MAPH || !owned_at(x, y)) return 0;
  i = IDX(x, y);
  if (GROUND(i) > GR_GRASS && GROUND(i) != GR_SAND) return 0;
  if (TREE(i) || ROAD(i) || (mr[i] & BLD)) return 0;
  return 1;
}

/* ---------------------------------------------------------------- buildings */
u8 nb_count(void) { u8 n = 0, i; for (i = 1; i < MAXB; i++) if (bs[i]) n++; return n; }
u8 bldg_of_type(u8 type) {
  u8 i;
  for (i = 1; i < MAXB; i++) if (bs[i] == BS_READY && bt[i] == type) return i;
  return 0;
}
u8 find_building(u8 x, u8 y) {
  u8 i;
  if (x >= CX0 && x < CX0 + 4 && y >= CY0 && y < CY0 + 4) return 0;
  for (i = 1; i < MAXB; i++) {
    if (!bs[i]) continue;
    if (x >= bx[i] && x < bx[i] + b_w[bt[i]] && y >= by[i] && y < by[i] + b_d[bt[i]]) return i;
  }
  return 0xFF;
}

/* the gate whose tiles a footprint stands within three tiles of, as ward*4+side, or 0xFF */
static u8 gate_near(u8 x, u8 y, u8 w, u8 d) {
  u8 g, s, gx, gy, v;
  for (g = 0; g < 9; g++) for (s = 0; s < 4; s++) {
    s16 x1, y1;
    if (!gate_exposed(g, s)) continue;
    v = gate_pos(g, s, &gx, &gy);
    x1 = gx + (v ? 0 : 1); y1 = gy + (v ? 1 : 0);
    if ((s16)x + w + 2 >= gx && (s16)x <= x1 + 3 && (s16)y + d + 2 >= gy && (s16)y <= y1 + 3) return g * 4 + s;
  }
  return 0xFF;
}

u8 can_place(u8 type, u8 x, u8 y, u8 free) {
  u8 w = b_w[type], d = b_d[type], i, j;
  u16 k;
  (void)free;
  if (x + w > MAPW || y + d > MAPH) return 0;
  for (j = 0; j < d; j++) for (i = 0; i < w; i++) {
    if (!owned_at(x + i, y + j)) return 0;
    k = IDX(x + i, y + j);
    if (GROUND(k) > GR_GRASS && GROUND(k) != GR_SAND) return 0;
    if (TREE(k) || ROAD(k) || (mr[k] & BLD)) return 0;
    if (((x + i) & 15) == 0 || ((x + i) & 15) == 15 || ((y + j) & 15) == 0 || ((y + j) & 15) == 15) return 0;
  }
  switch (type) {
    case B_FISHERY: if (count_near(x + 1, y + 1, 3, 3) < 5) return 0; break;
    case B_HUNTER: if (count_near(x + 1, y + 1, 6, 0) < 8) return 0; break;
    case B_WOODCUTTER: if (count_near(x + 1, y + 1, 6, 0) < 4) return 0; break;
    case B_QUARRY: if (count_near(x + 1, y + 1, 5, 1) < 3) return 0; break;
    case B_IRONMINE: if (count_near(x + 1, y + 1, 5, 2) < 2) return 0; break;
    case B_GATEHOUSE: {
      u8 g = gate_near(x, y, w, d), n;
      if (g == 0xFF) return 0;
      for (n = 1; n < MAXB; n++) if (bs[n] && bt[n] == B_GATEHOUSE && baux[n] == g) return 0;
      break;
    }
  }
  return 1;
}

u8 place_building(u8 type, u8 x, u8 y, u8 ready) {
  u8 id, i, j, w = b_w[type], d = b_d[type];
  for (id = 1; id < MAXB; id++) if (!bs[id]) break;
  if (id >= MAXB) return 0xFF;
  bt[id] = type; bx[id] = x; by[id] = y; bs[id] = ready ? BS_READY : BS_BUILD;
  bprog[id] = ready ? 255 : 0; bwk[id] = 0; bef[id] = 0; bfl[id] = 0; bacc[id] = 0; baux[id] = 0;
  if (type == B_GATEHOUSE) baux[id] = gate_near(x, y, w, d);
  for (j = 0; j < d; j++) for (i = 0; i < w; i++) {
    u16 k = IDX(x + i, y + j);
    /* a house on its own housing zone keeps the zone under it (so it grows back if it burns); anything else clears it */
    if (b_auto[type] && ZONE(k) == b_auto[type]) mr[k] |= BLD; else mr[k] = (mr[k] & ~0x78) | BLD;
  }
  net_dirty = 1;
  return id;
}

void remove_building(u8 id) {
  u8 i, j, type = bt[id], w, d;
  if (!id || !bs[id]) return;
  w = b_w[type]; d = b_d[type];
  for (j = 0; j < d; j++) for (i = 0; i < w; i++) mr[IDX(bx[id] + i, by[id] + j)] &= ~BLD;
  bs[id] = BS_FREE;
  net_dirty = 1;
}

/* is any road tile touching the footprint? (connected: only those joined to the castle) */
u8 adjacent_road(u8 x, u8 y, u8 w, u8 d, u8 connected) {
  u8 i, j;
  u16 k;
  for (j = 0; j < d; j++) {
    for (i = 0; i < w; i++) {
      u8 edge = (i == 0) | ((i == w - 1) << 1) | ((j == 0) << 2) | ((j == d - 1) << 3);
      u8 n;
      if (!edge) continue;
      for (n = 0; n < 4; n++) {
        s8 nx = x + i, ny = y + j;
        if (!(edge & (1 << n))) continue;
        if (n == 0) nx--; else if (n == 1) nx++; else if (n == 2) ny--; else ny++;
        if (nx < 0 || ny < 0 || nx >= MAPW || ny >= MAPH) continue;
        k = IDX(nx, ny);
        if (ROAD(k) == RD_NONE || ROAD(k) == RD_BRIDGE && 0) continue;
        if (!connected || (conn[k >> 3] & (1 << (k & 7)))) return 1;
      }
    }
  }
  return 0;
}

void refresh_net(void) {
  extern void sc_net(void);
  if (!net_dirty) return;
  call_in(BK_SIMC, sc_net);
}


/* ---------------------------------------------------------------- donut blocks */
u8 blk_n, blk_x[MAXBLK], blk_y[MAXBLK], blk_s[MAXBLK], blk_cz[MAXBLK], blk_rate[MAXBLK], blk_serve[MAXBLK][7];
s16 blk_tax, blk_mood, blk_cap;
u8 blk_size = 14, blk_ring = 1, blk_core = 1;

/* a square of free ground in the realm, clear of the moat and of buildings (streets and zones already there are taken over) */
u8 block_ok(u8 x, u8 y, u8 s) {
  u8 i, j;
  u16 k;
  if (x + s > MAPW || y + s > MAPH) return 0;
  for (j = 0; j < s; j++) for (i = 0; i < s; i++) {
    u8 lx, ly;
    if (!owned_at(x + i, y + j)) return 0;
    lx = (x + i) & 15; ly = (y + j) & 15;
    if (lx == 0 || lx == 15 || ly == 0 || ly == 15) return 0;
    k = IDX(x + i, y + j);
    if (GROUND(k) > GR_GRASS && GROUND(k) != GR_SAND) return 0;
    if (mr[k] & BLD) return 0;
  }
  return 1;
}

u8 block_at(u8 x, u8 y) {
  u8 b;
  for (b = 0; b < blk_n; b++) if (x >= blk_x[b] && x < blk_x[b] + blk_s[b] && y >= blk_y[b] && y < blk_y[b] + blk_s[b]) return b;
  return 0xFF;
}

u8 block_depth(u8 b, u8 x, u8 y) {
  u8 d = x - blk_x[b], e = blk_x[b] + blk_s[b] - 1 - x;
  if (e < d) d = e;
  e = y - blk_y[b]; if (e < d) d = e;
  e = blk_y[b] + blk_s[b] - 1 - y; if (e < d) d = e;
  return d;
}
