/* Moat for the NES: the moat itself: where it lies, how it is redrawn when a ward is taken in (a swappable bank). */
#pragma code-name(push, "BANK6")
#pragma rodata-name(push, "BANK6R")
#include "game.h"
void gate_tiles(u8 w, u8 side, u8 road);
u8 own_w(s8 wx, s8 wy);

/* the castle's island (4x4, local 6..9 in the home ward) with a ring of moat and a causeway to the south */
static u8 castle_ring(u8 x, u8 y) {
  if (x < 21 || x > 26 || y < 21 || y > 26) return 0;
  return x == 21 || x == 26 || y == 21 || y == 26;
}
static u8 castle_causeway(u8 x, u8 y) { return y == 26 && (x == 23 || x == 24); }

static u8 is_moat(u8 x, u8 y) {
  s8 wx = x >> 4, wy = y >> 4;
  u8 lx = x & 15, ly = y & 15;
  if (!own_w(wx, wy)) return 0;
  if (lx == 0 && !own_w(wx - 1, wy)) return 1;
  if (lx == 15 && !own_w(wx + 1, wy)) return 1;
  if (ly == 0 && !own_w(wx, wy - 1)) return 1;
  if (ly == 15 && !own_w(wx, wy + 1)) return 1;
  return 0;
}

static void m_rebuild(void) {
  u8 x, y, w, s;
  u16 i;
  for (y = 0; y < MAPH; y++) {
    for (x = 0; x < MAPW; x++) {
      i = IDX(x, y);
      if (is_moat(x, y) || castle_ring(x, y)) {
        if (GROUND(i) != GR_MOAT) { mt[i] = GR_MOAT; mr[i] = 0; }
        else mr[i] &= BLD ? 0xFF : 0xFF;
        if (castle_causeway(x, y)) mr[i] = (mr[i] & ~7) | RD_GATE;
      } else if (GROUND(i) == GR_MOAT) {
        mt[i] = GR_GRASS;
        if (ROAD(i) == RD_GATE) mr[i] &= ~7;
      }
    }
  }
  /* no trees in the strip beside the moat: they would be painted in the water's colors */
  for (y = 0; y < MAPH; y++) for (x = 0; x < MAPW; x++) {
    u8 lx = x & 15, ly = y & 15;
    if ((lx == 1 || lx == 14 || ly == 1 || ly == 14) && owned_at(x, y) && TREE(IDX(x, y))) mt[IDX(x, y)] &= ~0x18;
  }
  for (w = 0; w < 9; w++) {
    for (s = 0; s < 4; s++) {
      if (!gate_exposed(w, s)) { gate_open[w] &= ~(1 << s); continue; }
      gate_tiles(w, s, (gate_open[w] >> s) & 1 ? RD_GATE : RD_NONE);
    }
  }
  net_dirty = 1;
}

static void m_annex(u8 w) {
  u8 s;
  wards_owned |= 1 << w;
  for (s = 0; s < 4; s++) if (gate_exposed(w, s)) gate_open[w] |= 1 << s;
  m_rebuild();
  /* gatehouses whose gates are gone go with them */
  for (s = 0; s < MAXB; s++) {
    if (bs[s] && bt[s] == B_GATEHOUSE) {
      u8 gw = baux[s] >> 2, gs = baux[s] & 3;
      if (!gate_exposed(gw, gs)) remove_building(s);
    }
  }
}

/* ---------------------------------------------------------------- generation */

void mc_rebuild(void) { m_rebuild(); }
void mc_annex(void) { m_annex(arg_a); }
