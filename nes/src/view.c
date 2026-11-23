/* Moat for the NES: the map view, the tools and the game loop (a swappable bank). */
#pragma code-name(push, "BANK2")
#pragma rodata-name(push, "BANK2R")
#define b_name b_name_v
#define castle_name castle_name_v
#define rank_name rank_name_v
#define res_name res_name_v
#include "game.h"
#include "strutil.h"
#ifndef TICKF
#define TICKF 192
#endif
#ifndef TDBG
#define TDBG 0
#endif
#define DBG(n) if (TDBG == n) for (;;) nmi_wait();
extern void ui_castle(void), ui_choose_tool(void), ui_main_menu(void);


extern const u8 tile_pal[];

static u8 msg_t;
u8 tick_t, sim_busy;
static u16 held;
static u8 rep_t;
static u8 blink;
static u8 stroke;               /* A has been held through a stroke of painting */
static u8 bridge_n, bridge_pos[6][2];
static u8 last_x, last_y;

static u16 hud_cache[8];
static char buf[34];



/* ---------------------------------------------------------------- the picture of the land */
static u8 water_at(s8 x, s8 y) {
  if (x < 0 || y < 0 || x >= MAPW || y >= MAPH) return 1;
  return is_water(GROUND(IDX(x, y)));
}
static u8 road_at(s8 x, s8 y) {
  if (x < 0 || y < 0 || x >= MAPW || y >= MAPH) return 0;
  return ROAD(IDX(x, y)) != 0;
}

static u8 building_tile(u8 id, u8 x, u8 y) {
  u8 t = bt[id];
  if (bs[id] == BS_BUILD) return T_SITE;
  return b_art[b_off[t] + (u16)(y - by[id]) * b_w[t] + (x - bx[id])];
}

static u8 tile_terrain(u8 x, u8 y) {
  u16 i = IDX(x, y);
  u8 g = GROUND(i), r = ROAD(i), z, h, m;
  if (g == GR_MOAT || g == GR_WATER) {
    if (r >= RD_BRIDGE) return (water_at(x - 1, y) || water_at(x + 1, y)) && !(road_at(x - 1, y) && road_at(x + 1, y) && !water_at(x - 1, y)) ? T_BRIDGEV : T_BRIDGEH;
    return ((x + y) & 1) ? T_WATER2 : T_WATER;
  }
  if (r) {
    m = road_at(x, y - 1) | (road_at(x + 1, y) << 1) | (road_at(x, y + 1) << 2) | (road_at(x - 1, y) << 3);
    return (r == RD_COBBLE ? T_COBBLE : T_DIRT) + m;
  }
  if (mr[i] & BLD) return T_GRASS;
  z = ZONE(i);
  if (z) return T_ZONE + z - 1;
  h = TREE(i);
  if (h) return h == 1 ? T_SAPLING : h == 2 ? T_YOUNG : ((x * 3 + y * 5) & 1) ? T_TREE2 : T_TREE;
  switch (g) {
    case GR_ROCK: return ((x + y) & 1) ? T_ROCK2 : T_ROCK;
    case GR_ORE: return T_ORE;
    case GR_SAND: return T_SAND;
  }
  h = (x * 7 + y * 13) & 15;
  return h < 11 ? T_GRASS : h == 14 ? T_GRASS3 : T_GRASS2;
}

u8 tile_at(u8 x, u8 y) {
  u16 i = IDX(x, y);
  if (mr[i] & BLD) {
    u8 id = find_building(x, y);
    if (id != 0xFF && id != 0) return building_tile(id, x, y);
  }
  return tile_terrain(x, y);
}

static u8 pal_of4(u8 a, u8 b, u8 c, u8 d) {
  u8 pa = tile_pal[a], pb = tile_pal[b], pc = tile_pal[c], pd = tile_pal[d];
  if (pa == 2 || pb == 2 || pc == 2 || pd == 2) return 2;
  if (pa == 4 || pb == 4 || pc == 4 || pd == 4) return 3;
  if (pa == 1 || pb == 1 || pc == 1 || pd == 1) return 1;
  if (pa == 3 || pb == 3 || pc == 3 || pd == 3) return 3;
  return 0;
}

#define VW 32
#define VH 22

static u8 vis(u8 x, u8 y) { return x >= vx && x < vx + VW && y >= vy && y < vy + VH; }

static u8 attr_cell(u8 ax, u8 ay) {      /* attribute byte from the picture buffer */
  u8 q, v = 0;
  for (q = 0; q < 4; q++) {
    u8 sx = (ax << 2) + ((q & 1) << 1), sy = (ay << 2) + ((q >> 1) << 1) - 4, p;
    if (sy >= VH) p = 3;
    else p = pal_of4(sbuf[sy * VW + sx], sbuf[sy * VW + sx + 1], sy + 1 < VH ? sbuf[(sy + 1) * VW + sx] : 0, sy + 1 < VH ? sbuf[(sy + 1) * VW + sx + 1] : 0);
    v |= p << (q << 1);
  }
  return v;
}

static void stamp_buildings(void) {
  u8 i;
  for (i = 1; i < MAXB; i++) {
    u8 t, w, d, x, y;
    if (!bs[i]) continue;
    t = bt[i]; w = b_w[t]; d = b_d[t];
    if (bx[i] + w <= vx || bx[i] >= vx + VW || by[i] + d <= vy || by[i] >= vy + VH) continue;
    for (y = 0; y < d; y++) for (x = 0; x < w; x++) {
      u8 gx = bx[i] + x, gy = by[i] + y;
      if (vis(gx, gy)) sbuf[(gy - vy) * VW + gx - vx] = building_tile(i, gx, gy);
    }
  }
}

static void draw_frame(void) {
  /* the HUD and status bars are black: tile 0x20 of the font is a solid dark square */
  u8 r;
  for (r = 0; r < 4; r++) ntfill(T_FONT + ' ', 0, r, 32, 1);
  for (r = 26; r < 30; r++) ntfill(T_FONT + ' ', 0, r, 32, 1);
}

void redraw_view(void) {
  u8 x, y, ax, ay;
  u16 n = 0;
  for (y = 0; y < VH; y++) for (x = 0; x < VW; x++) sbuf[n++] = tile_terrain(vx + x, vy + y);
  DBG(1)
  stamp_buildings();
  DBG(2)
  ppu_off();
  DBG(3)
  draw_frame();
  DBG(4)
  vram_adr(0x2000 + 4 * 32);
  for (n = 0; n < VW * VH; n++) PPUDATA = sbuf[n];
  DBG(5)
  vram_adr(0x23C0);
  for (ay = 0; ay < 8; ay++) for (ax = 0; ax < 8; ax++) PPUDATA = (ay == 0 || ay == 7) ? 0xFF : attr_cell(ax, ay);
  DBG(6)
  set_palettes(season_of(week));
  DBG(7)
  hud_update(1);
  DBG(8)
  ppu_on();
  view_dirty = 0;
}

void refresh_tile(u8 x, u8 y) {
  u8 sx, sy, ax, ay, v = 0, q;
  if (!vis(x, y)) return;
  sx = x - vx; sy = y - vy;
  vq_byte(0x2000 + ((u16)(sy + 4) << 5) + sx, tile_at(x, y));
  ax = sx >> 2; ay = (sy + 4) >> 2;
  for (q = 0; q < 4; q++) {
    u8 ex = vx + (ax << 2) + ((q & 1) << 1), ey = vy + (ay << 2) + ((q >> 1) << 1) - 4, p;
    if (ey + 1 >= vy + VH + 0 && ey >= vy + VH) { p = 3; }
    else p = pal_of4(tile_at(ex, ey), tile_at(ex + 1, ey), ey + 1 < vy + VH ? tile_at(ex, ey + 1) : 0, ey + 1 < vy + VH ? tile_at(ex + 1, ey + 1) : 0);
    v |= p << (q << 1);
  }
  vq_byte(0x23C0 + (ay << 3) + ax, v);
}

void refresh_rect(u8 x, u8 y, u8 w, u8 h) {
  u8 i, j;
  for (j = 0; j < h; j++) for (i = 0; i < w; i++) refresh_tile(x + i, y + j);
}

/* ---------------------------------------------------------------- HUD */
static void put_num(u8 slot, u8 x, u8 y, u16 v, u8 digits) {
  if (hud_cache[slot] == v + 1 && v + 1 != 0) return;
  hud_cache[slot] = v + 1;
  text_n(x, y, v, digits);
}

void hud_update(u8 force) {
  u8 i;
  char b[16];
  u8 n;
  s16 g, food;
  if (force) {
    for (i = 0; i < 8; i++) hud_cache[i] = 0;
    {
      static const u8 icons[5] = { T_I_GOLD, T_I_WOOD, T_I_STONE, T_I_IRON, T_I_TOOLS };
      static const u8 ix[5] = { 0, 7, 13, 19, 24 };
      for (i = 0; i < 5; i++) { u8 t = icons[i]; vq_byte(0x2000 + 32 + ix[i], t); }
    }
    vq_byte(0x2000 + 64, T_I_POP);
    vq_byte(0x2000 + 64 + 6, T_I_FOOD);
    vq_byte(0x2000 + 64 + 13, T_I_HEART);
  }
  DBG(10)
  g = whole_of(R_GOLD);
  put_num(0, 1, 1, g, 5);
  DBG(11)
  put_num(1, 8, 1, whole_of(R_WOOD), 4);
  put_num(2, 14, 1, whole_of(R_STONE), 4);
  put_num(3, 20, 1, whole_of(R_IRON), 3);
  put_num(4, 25, 1, whole_of(R_TOOLS), 3);
  put_num(5, 1, 2, headcount(), 4);
  food = (s16)r_food > 32000 ? 32000 : (s16)r_food;
  put_num(6, 7, 2, food, 5);
  put_num(7, 14, 2, happy / 16, 3);
  DBG(12)
  /* the date and the pace */
  n = 0; n = scat(b, n, month_name[inyear(week) >> 2]); b[n++] = ' '; b[n++] = 'Y'; b[n] = 0; n = ncat(b, n, week / 48 + 1);
  if (force || (week & 3) == 0 || tick_t == 0) text_pad(18, 2, b, 8);
  DBG(13)
  text_pad(27, 2, paused ? "PAUSE" : speed == 1 ? "  >  " : speed == 2 ? " >>  " : " >>> ", 5);
}

/* ---------------------------------------------------------------- sprites: castle, cursor, smoke */
static void draw_sprites(void) {
  u8 i, tx, ty, fw = 1, fh = 1, valid = 1, sx, sy, base;
  oam_busy = 1;
  sprites_clear();
  /* the castle, as sixteen sprites, with a banner on its tower */
  for (ty = 0; ty < 4; ty++) for (tx = 0; tx < 4; tx++) {
    u8 gx = CX0 + tx, gy = CY0 + ty;
    if (!vis(gx, gy)) continue;
    spr((gx - vx) << 3, (gy - vy + 4) << 3, castle_lvl * 16 + ty * 4 + tx, castle_lvl ? 2 : 1);
  }
  if (vis(CX0 + 2, CY0) && (CY0 - vy) > 0) spr((CX0 + 2 - vx) << 3, ((CY0 - vy + 4) << 3) - 6, 0x4D + ((frames >> 5) & 1), 3);
  /* smoke from chimneys */
  {
    u8 n = 0, f = (frames >> 4) & 3;
    for (i = 1; i < MAXB && n < 6; i++) {
      u8 t = bt[i];
      if (bs[i] != BS_READY || !(bfl[i] & BF_CONN)) continue;
      if (!(b_cap[t] || t == B_SMITHY || t == B_BREWERY || t == B_MILL || t == B_TAVERN || t == B_JOINERY)) continue;
      if (b_cap[t] && cold_season() == 0 && ((bx[i] + by[i]) & 3)) continue;
      if (!vis(bx[i] + b_w[t] - 1, by[i])) continue;
      if (((i + (frames >> 6)) & 3) == 3) continue;
      spr(((bx[i] + b_w[t] - 1 - vx) << 3) + 2, ((by[i] - vy + 4) << 3) - 4 - (f << 1), 0x48 + (f % 3), 0);
      n++;
    }
  }
  /* the cursor: a box, or the corners of the footprint to be built */
  if (tool == TL_BUILD) { fw = b_w[tsub]; fh = b_d[tsub]; valid = can_place(tsub, cx, cy, 0) && pay_ok(tsub); }
  if (tool == TL_BLOCK) { fw = BLK; fh = BLK; valid = block_ok(cx, cy) && blk_n < MAXBLK; }
  if (!vis(cx, cy)) { oam_busy = 0; return; }
  sx = cx - vx; sx <<= 3;
  sy = cy - vy; sy += 4; sy <<= 3;
  base = tool == TL_LOOK ? 0x60 : valid ? 0x40 : 0x50;
  if (fw == 1 && fh == 1) {
    spr(sx, sy, base + 4, 3);
  } else {
    spr(sx, sy, base, 3); spr(sx + (fw << 3) - 8, sy, base + 1, 3);
    spr(sx, sy + (fh << 3) - 8, base + 2, 3); spr(sx + (fw << 3) - 8, sy + (fh << 3) - 8, base + 3, 3);
  }
  oam_busy = 0;
}

u8 cold_season(void) { return season_of(week) == 3 || season_of(week) == 2; }

/* ---------------------------------------------------------------- messages and status */
static void set_msg(const char *m) {
  u8 i;
  for (i = 0; i < 31 && m[i]; i++) msgbuf[i] = m[i];
  msgbuf[i] = 0;
  msg_new = 1;
}

static const char *ground_name(u16 i) {
  switch (GROUND(i)) {
    case GR_ROCK: return "ROCK";
    case GR_ORE: return "IRON ORE";
    case GR_WATER: return "WATER";
    case GR_MOAT: return "MOAT";
    case GR_SAND: return "SAND";
  }
  if (TREE(i)) return TREE(i) == 3 ? "WOOD" : "YOUNG TREES";
  return "GRASS";
}

static void info_line(char *o) {
  u16 i = IDX(cx, cy);
  u8 n = 0, id;
  o[0] = 0;
  if (mr[i] & BLD) {
    id = find_building(cx, cy);
    if (id == 0) { n = scat(o, 0, castle_name[castle_lvl]); n = scat(o, n, castle_work ? " (BUILDING)" : ""); return; }
    if (id != 0xFF) {
      u8 t = bt[id];
      n = scat(o, 0, b_name[t]);
      if (bs[id] == BS_BUILD) n = scat(o, n, " (BUILDING)");
      else if (!(bfl[id] & BF_CONN)) n = scat(o, n, " - NO ROAD");
      else if (b_cap[t]) { n = scat(o, n, " HOLDS "); n = ncat(o, n, b_cap[t]); }
      else if (b_jobs[t] || t == B_FARMSTEAD) { n = scat(o, n, " "); n = ncat(o, n, bwk[id] / 32); n = scat(o, n, "/"); n = ncat(o, n, t == B_FARMSTEAD ? (farm_fields[baux[id] & 7] / 8 + 1) : b_jobs[t]); n = scat(o, n, " AT WORK"); }
      return;
    }
  }
  { u8 b = block_at(cx, cy); if (b != 0xFF && !(mr[i] & BLD)) { n = scat(o, 0, "COURTYARD "); n = ncat(o, n, (u16)block_rating(b) * 100 / 255); scat(o, n, "% RATING"); return; } }
  if (ROAD(i) == RD_GATE) { n = scat(o, 0, "GATE. A: RAISE OR LOWER"); return; }
  if (ROAD(i)) { scat(o, 0, ROAD(i) == RD_COBBLE ? "COBBLED ROAD" : ROAD(i) == RD_BRIDGE ? "BRIDGE" : "DIRT ROAD"); return; }
  if (ZONE(i)) { n = scat(o, 0, zone_name[ZONE(i)]); n = scat(o, n, " ZONE"); return; }
  n = scat(o, 0, ground_name(i));
  if (!owned_at(cx, cy)) scat(o, n, " (NOT YOURS)");
}


void status_update(void) {
  u8 n = 0;
  buf[0] = 0;
  switch (tool) {
    case TL_LOOK: n = scat(buf, 0, "LOOK"); break;
    case TL_ROAD: n = scat(buf, 0, tsub ? "COBBLED ROAD 1 STONE+1 GOLD" : "DIRT ROAD 1 GOLD"); break;
    case TL_ZONE: n = scat(buf, 0, zone_name[tsub]); n = scat(buf, n, " ZONE "); n = ncat(buf, n, zone_cost[tsub]); n = scat(buf, n, " GOLD"); break;
    case TL_BUILD: n = scat(buf, 0, b_name[tsub]); n = scat(buf, n, " "); n = ncat(buf, n, b_cg[tsub]); n = scat(buf, n, "G "); n = ncat(buf, n, b_cw[tsub]); n = scat(buf, n, "W "); n = ncat(buf, n, b_cs[tsub]); n = scat(buf, n, "S"); if (b_ci[tsub]) { n = scat(buf, n, " "); n = ncat(buf, n, b_ci[tsub]); n = scat(buf, n, "I"); } break;
    case TL_CLEAR: n = scat(buf, 0, "CLEAR"); break;
    case TL_BLOCK: n = scat(buf, 0, "BLOCK: LOOP OF STREET AND PLOTS"); break;
  }
  text_pad(0, 26, buf, 32);
  if (msg_t) text_pad(0, 27, msgbuf, 32);
  else { info_line(buf); text_pad(0, 27, buf, 32); }
}

/* ---------------------------------------------------------------- tools applied */
static void say(const char *m) { set_msg(m); msg_t = 100; status_update(); snd_play(4); }

static u8 gate_of_tile(u8 x, u8 y, u8 *gw, u8 *gs) {
  u8 w, s, gx, gy, v;
  for (w = 0; w < 9; w++) for (s = 0; s < 4; s++) {
    if (!gate_exposed(w, s)) continue;
    v = gate_pos(w, s, &gx, &gy);
    if ((x == gx && y == gy) || (x == gx + (v ? 0 : 1) && y == gy + (v ? 1 : 0))) { *gw = w; *gs = s; return 1; }
  }
  return 0;
}

static void toggle_gate(u8 w, u8 s) {
  u8 open = (gate_open[w] >> s) & 1, gx, gy, v;
  gate_set(w, s, !open);
  v = gate_pos(w, s, &gx, &gy);
  refresh_tile(gx, gy); refresh_tile(gx + (v ? 0 : 1), gy + (v ? 1 : 0));
  say(open ? "DRAWBRIDGE RAISED." : "DRAWBRIDGE LOWERED.");
}

static void undo_bridge(void) {
  while (bridge_n) {
    bridge_n--;
    set_road(bridge_pos[bridge_n][0], bridge_pos[bridge_n][1], RD_NONE);
    stock[R_WOOD] += 4L << 8; stock[R_GOLD] += 3L << 8;
    refresh_tile(bridge_pos[bridge_n][0], bridge_pos[bridge_n][1]);
  }
}

static void apply_tool(u8 first) {
  u16 i = IDX(cx, cy);
  u8 r;
  if (cx == last_x && cy == last_y && !first) return;
  last_x = cx; last_y = cy;
  switch (tool) {
    case TL_LOOK: {
      u8 gw, gs, id;
      if (!first) return;
      if (find_building(cx, cy) == 0) { call_in(BK_UI, ui_castle); view_dirty = 1; return; }
      if (ROAD(i) == RD_GATE && gate_of_tile(cx, cy, &gw, &gs)) { toggle_gate(gw, gs); return; }
      (void)id;
      break;
    }
    case TL_ROAD: {
      u8 want = tsub ? RD_COBBLE : RD_DIRT, g = GROUND(i);
      if (!owned_at(cx, cy)) { say("NOT YOUR LAND."); return; }
      if (mr[i] & BLD) { say("A BUILDING IS IN THE WAY."); return; }
      if (ROAD(i) == RD_GATE || ROAD(i) == RD_BRIDGE) return;
      if (g == GR_MOAT) { say("THE MOAT IS CROSSED AT GATES."); return; }
      if (g == GR_WATER) {
        /* a bridge: from a road, in a line, up to six tiles */
        u8 nrby = road_at(cx - 1, cy) || road_at(cx + 1, cy) || road_at(cx, cy - 1) || road_at(cx, cy + 1);
        if (!nrby || bridge_n >= 6) { say(bridge_n >= 6 ? "BRIDGES SPAN SIX TILES." : "START A BRIDGE FROM A ROAD."); return; }
        if (whole_of(R_WOOD) < 4 || whole_of(R_GOLD) < 3) { say("A BRIDGE COSTS 4 WOOD 3 GOLD."); return; }
        stock[R_WOOD] -= 4L << 8; stock[R_GOLD] -= 3L << 8;
        mr[i] = (mr[i] & ~7) | RD_BRIDGE; net_dirty = 1;
        bridge_pos[bridge_n][0] = cx; bridge_pos[bridge_n][1] = cy; bridge_n++;
        refresh_tile(cx, cy);
        refresh_tile(cx - 1, cy); refresh_tile(cx + 1, cy); refresh_tile(cx, cy - 1); refresh_tile(cx, cy + 1);
        snd_play(1);
        return;
      }
      if (g != GR_GRASS && g != GR_SAND) { say("ROCK IS IN THE WAY."); return; }
      if (ROAD(i) == want) return;
      if (whole_of(R_GOLD) < 1 || (want == RD_COBBLE && whole_of(R_STONE) < 1)) { say("NOT ENOUGH GOLD OR STONE."); return; }
      stock[R_GOLD] -= 256; if (want == RD_COBBLE) stock[R_STONE] -= 256;
      if (TREE(i)) { mt[i] &= ~0x18; stock[R_WOOD] += 256; }
      mr[i] = (mr[i] & ~0x7F) | want; net_dirty = 1;
      bridge_n = 0;
      refresh_tile(cx, cy);
      refresh_tile(cx - 1, cy); refresh_tile(cx + 1, cy); refresh_tile(cx, cy - 1); refresh_tile(cx, cy + 1);
      snd_play(1);
      break;
    }
    case TL_ZONE: {
      u8 z = tsub;
      if (ZONE(i) == z) return;
      if (!tile_ok_zone(cx, cy)) { say(TREE(i) ? "CLEAR THE TREES FIRST." : "CAN'T ZONE HERE."); return; }
      if (whole_of(R_GOLD) < zone_cost[z]) { say("NOT ENOUGH GOLD."); return; }
      stock[R_GOLD] -= (s32)zone_cost[z] << 8;
      set_zone(cx, cy, z);
      refresh_tile(cx, cy);
      snd_play(1);
      break;
    }
    case TL_BUILD: {
      u8 id;
      if (!first) return;
      id = place_player(tsub, cx, cy);
      if (id == 0xFF) { say(can_place(tsub, cx, cy, 0) ? "NO ROOM." : "CAN'T BUILD HERE."); return; }
      if (id == 0xFE) { say("NEEDS A HIGHER RANK."); return; }
      if (id == 0xFD) { say("NOT ENOUGH MATERIALS."); return; }
      refresh_rect(cx, cy, b_w[tsub], b_d[tsub]);
      snd_play(2);
      set_msg("WORK BEGINS."); msg_t = 80;
      break;
    }
    case TL_BLOCK: {
      u8 r;
      if (!first) return;
      r = block_stamp(cx, cy);
      if (r == 1) say("NO MORE BLOCKS (SIX).");
      else if (r == 2) say("NO ROOM: NEEDS 9X9 FREE LAND.");
      else if (r == 3) say("NOT ENOUGH GOLD FOR THE BLOCK.");
      else { set_msg("BLOCK LAID. JOIN IT TO YOUR ROADS."); msg_t = 120; snd_play(2); }
      break;
    }
    case TL_CLEAR: {
      if (mr[i] & BLD) { if (find_building(cx, cy) == 0) return; demolish_at(cx, cy); snd_play(8); return; }
      if (ROAD(i) && ROAD(i) != RD_GATE) {
        if (GROUND(i) == GR_WATER && ROAD(i) == RD_BRIDGE) { /* bridges come down too */ }
        set_road(cx, cy, RD_NONE);
        refresh_tile(cx, cy);
        refresh_tile(cx - 1, cy); refresh_tile(cx + 1, cy); refresh_tile(cx, cy - 1); refresh_tile(cx, cy + 1);
        snd_play(8);
      } else if (ZONE(i)) { set_zone(cx, cy, 0); refresh_tile(cx, cy); snd_play(8); }
      else if (TREE(i) && owned_at(cx, cy)) { stock[R_WOOD] += (s32)TREE(i) << 8; mt[i] &= ~0x18; refresh_tile(cx, cy); snd_play(8); }
      (void)r;
      break;
    }
  }
}

/* ---------------------------------------------------------------- the game */
static void center_view(void) {
  s16 x = (s16)cx - 16, y = (s16)cy - 11;
  if (x < 0) x = 0;
  if (x > MAPW - VW) x = MAPW - VW;
  if (y < 0) y = 0;
  if (y > MAPH - VH) y = MAPH - VH;
  vx = x & ~1; vy = y & ~1;
}

void view_new_game(void) {
  save_flag = 0;
  tool = TL_LOOK; tsub = 0; tsub2 = 0; speed = 1; paused = 0; tick_t = 0;
  cx = 24; cy = 28;
  center_view();
  mus_season = 0;
  save_magic = 0xBEEF; save_flag = 1;
}

void view_game(void) {
  u8 moved;
  mus_on = 1; mus_season = season_of(week);
  redraw_view();
#ifdef T4
  for (;;) nmi_wait();
#endif
  status_update();
  msg_t = 0;
#ifdef T1
  for (;;) nmi_wait();
#endif
  for (;;) {
    nmi_wait();
    blink++;
    moved = 0;
    /* movement, with a repeat while held */
    if (pad & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT)) {
      if (padnew & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT)) { rep_t = 14; moved = 1; }
      else if (rep_t) rep_t--; else { rep_t = 2; moved = 1; }
    } else rep_t = 0;
    if (moved) {
      if ((pad & BTN_UP) && cy > 0) cy--;
      if ((pad & BTN_DOWN) && cy < MAPH - 1) cy++;
      if ((pad & BTN_LEFT) && cx > 0) cx--;
      if ((pad & BTN_RIGHT) && cx < MAPW - 1) cx++;
      if (cx < vx + 1 && vx > 0) { vx = vx >= 8 ? vx - 8 : 0; view_dirty = 1; }
      if (cx >= vx + VW - 1 && vx < MAPW - VW) { vx += 8; if (vx > MAPW - VW) vx = MAPW - VW; view_dirty = 1; }
      if (cy < vy + 1 && vy > 0) { vy = vy >= 6 ? vy - 6 : 0; vy &= ~1; view_dirty = 1; }
      if (cy >= vy + VH - 1 && vy < MAPH - VH) { vy += 6; if (vy > MAPH - VH) vy = MAPH - VH; vy &= ~1; view_dirty = 1; }
      if (pad & BTN_A) apply_tool(0);
      status_update();
    }
    if (!vis(cx, cy)) { center_view(); view_dirty = 1; }
    if (padnew & BTN_A) { stroke = 1; apply_tool(1); status_update(); }
    if (!(pad & BTN_A) && stroke) { stroke = 0; if (bridge_n && GROUND(IDX(last_x, last_y)) == GR_WATER) { undo_bridge(); say("A BRIDGE MUST REACH THE FAR BANK."); } bridge_n = 0; last_x = 0xFF; }
    if (padnew & BTN_B) { call_in(BK_UI, ui_choose_tool); status_update(); }
    if (padnew & BTN_SEL) {
      if (tool != TL_LOOK) { last_tool = tool; last_sub = tsub; tool = TL_LOOK; }
      else { tool = last_tool; tsub = last_sub; }
      status_update();
    }
    if (padnew & BTN_START) {
      u8 quit;
      call_in(BK_UI, ui_main_menu);
      quit = ret_a;
      if (quit) { while (sim_busy && !sim_step()) ; sim_busy = 0; sim_p = 0; save_flag = gameover ? 0 : 1; return; }
      status_update();
    }
    /* the world moves on */
    if (!paused && !gameover) {
      if (!sim_busy && ++tick_t >= (u8)(TICKF / speed)) { tick_t = 0; sim_busy = 1; sim_p = 0; save_flag = 0; }
      if (sim_busy && sim_step()) {
        sim_busy = 0;
        save_flag = 1;
        if ((week % 12) == 0) { view_dirty = 1; }
        hud_update(0);
        if (msg_new) { msg_new = 0; msg_t = 150; status_update(); }
      }
    }
    if (msg_t) { msg_t--; if (!msg_t) status_update(); }
    if (view_dirty) { redraw_view(); status_update(); }
    else { u8 n = 0; while (dirty_n && n < 4) { u16 k = dirty_q[--dirty_n]; refresh_tile(k & 255, k >> 8); n++; } }
    if ((frames & 31) == 0) hud_update(0);
    draw_sprites();
  }
}

