/* Moat for the NES: menus, pages and the title (a swappable bank). */
#pragma code-name(push, "BANK5")
#pragma rodata-name(push, "BANK5R")
#define b_name b_name_u
#define castle_name castle_name_u
#define rank_name rank_name_u
#define res_name res_name_u
#include "game.h"
#include "strutil.h"
#ifndef TDBG
#define TDBG 0
#endif
#define DBG(n) if (TDBG == n) for (;;) nmi_wait();

static u8 menu_cat, menu_ids[16], menu_nids;
static u8 trade_i;
static u8 wsel;
/* ---------------------------------------------------------------- menus */
typedef void (*fmt_fn)(u8 i, char *o);

static void box(u8 x, u8 y, u8 w, u8 h) {
  u8 r;
  ntfill(T_U_TL, x, y, 1, 1); ntfill(T_U_H, x + 1, y, w - 2, 1); ntfill(T_U_TR, x + w - 1, y, 1, 1);
  for (r = 1; r < h - 1; r++) {
    ntfill(T_U_V, x, y + r, 1, 1); ntfill(T_FONT + ' ', x + 1, y + r, w - 2, 1); ntfill(T_U_VR, x + w - 1, y + r, 1, 1);
  }
  ntfill(T_U_BL, x, y + h - 1, 1, 1); ntfill(T_U_HB, x + 1, y + h - 1, w - 2, 1); ntfill(T_U_BR, x + w - 1, y + h - 1, 1, 1);
}

static void draw_box_text(u8 x, u8 y, const char *s) {
  vram_adr(0x2000 + ((u16)y << 5) + x);
  while (*s) { u8 c = *s++; if (c >= 'a' && c <= 'z') c -= 32; PPUDATA = c + T_FONT; }
}

static void wait_release(void) { while (pad) nmi_wait(); }

/* a list popup. returns the chosen index, or 0xFF if cancelled */
static u8 menu(const char *title, u8 n, fmt_fn fmt, u8 sel, u8 w) {
  u8 vis_n = n > 12 ? 12 : n, h, x0, y0, top = 0, i, redraw = 1;
  char line[34];
  w = (w + 7) & ~7;                  /* whole attribute cells, so the frame does not spill into the map's colors */
  h = (vis_n + 4 + 3) & ~3;
  x0 = (32 - w) >> 1;
  y0 = (4 + ((22 - h) >> 1)) & ~3;
  if (y0 < 4) y0 = 4;
  if (sel >= n) sel = 0;
  if (sel >= vis_n) top = sel - vis_n + 1;
  sprites_clear();
  nmi_wait();
  ppu_off();
  box(x0, y0, w, h);
  draw_box_text(x0 + 2, y0 + 1, title);
  fill_attr_ui(x0, y0, w, h);
  DBG(21)
  ppu_on();
  DBG(22)
  wait_release();
  DBG(23)
  for (;;) {
    if (redraw) {
      for (i = 0; i < vis_n; i++) {
        u8 k = top + i;
        line[0] = k == sel ? '>' : ' ';
        DBG(24)
        fmt(k, line + 1);
        DBG(25)
        text_pad(x0 + 1, y0 + 2 + i, line, w - 2);
      }
      redraw = 0;
    }
    nmi_wait();
    if (padnew & BTN_DOWN) { sel = sel + 1 >= n ? 0 : sel + 1; redraw = 1; }
    if (padnew & BTN_UP) { sel = sel == 0 ? n - 1 : sel - 1; redraw = 1; }
    if (redraw) { if (sel < top) top = sel; if (sel >= top + vis_n) top = sel - vis_n + 1; }
    if (padnew & (BTN_A | BTN_START)) { snd_play(1); wait_release(); return sel; }
    if (padnew & BTN_B) { wait_release(); return 0xFF; }
  }
}

static void f_tool(u8 i, char *o) {
  static const char *const n[] = { "LOOK", "ROADS", "ZONES", "BUILD", "CLEAR", "BLOCKS" };
  scat(o, 0, n[i]);
}
static void f_road(u8 i, char *o) { scat(o, 0, i ? "COBBLE 1 STONE 1 GOLD" : "DIRT 1 GOLD"); }
static void f_zone(u8 i, char *o) { u8 n = scat(o, 0, zone_name[i + 1]); n = scat(o, n, " "); ncat(o, n, zone_cost[i + 1]); }
static void f_cat(u8 i, char *o) { static const char *const n[] = { "FOOD", "INDUSTRY", "CIVIC", "DEFENSE" }; scat(o, 0, n[i]); }

static void collect(void) {
  u8 t;
  menu_nids = 0;
  for (t = 0; t < NB; t++) {
    u8 c = b_cat[t];
    if (b_auto[t]) continue;
    if ((menu_cat == 0 && c == 2) || (menu_cat == 1 && c == 3) || (menu_cat == 2 && c == 4) || (menu_cat == 3 && c == 5)) menu_ids[menu_nids++] = t;
  }
}
static void f_bld(u8 i, char *o) {
  u8 t = menu_ids[i], n = scat(o, 0, b_name[t]);
  while (n < 15) o[n++] = ' ';
  n = ncat(o, n, b_cg[t]); n = scat(o, n, "G");
  if (b_cw[t]) { n = scat(o, n, " "); n = ncat(o, n, b_cw[t]); n = scat(o, n, "W"); }
  if (b_cs[t]) { n = scat(o, n, " "); n = ncat(o, n, b_cs[t]); n = scat(o, n, "S"); }
  if (b_rank[t] > rank) scat(o, n, " *");
}

void ui_choose_tool(void) {
  u8 t = menu("TOOLS", 6, f_tool, tool, 14);
  if (t == 0xFF) return;
  if (t == TL_LOOK || t == TL_CLEAR || t == TL_BLOCK) { tool = t; }
  else if (t == TL_ROAD) { u8 s = menu("ROADS", 2, f_road, tsub2 & 1, 26); if (s != 0xFF) { tool = TL_ROAD; tsub = s; tsub2 = s; } }
  else if (t == TL_ZONE) { u8 s = menu("ZONES", 8, f_zone, 0, 20); if (s != 0xFF) { tool = TL_ZONE; tsub = s + 1; } }
  else if (t == TL_BUILD) {
    u8 c = menu("BUILD", 4, f_cat, menu_cat, 14);
    if (c != 0xFF) {
      u8 s;
      menu_cat = c; collect();
      s = menu("CHOOSE", menu_nids, f_bld, 0, 30);
      if (s != 0xFF) { tool = TL_BUILD; tsub = menu_ids[s]; }
    }
  }
  view_dirty = 1;
}

/* ---------------------------------------------------------------- the pages */
static void page_begin(const char *title) {
  sprites_clear();
  nmi_wait();
  ppu_off();
  ntfill(T_FONT + ' ', 0, 0, 32, 1);
  { u8 r; for (r = 0; r < 30; r++) ntfill(T_FONT + ' ', 0, r, 32, 1); }
  fill_attr_ui(0, 0, 32, 30);
  draw_box_text(1, 1, title);
}
static void page_end(void) { ppu_on(); }

static void page_wait_b(void) {
  for (;;) { nmi_wait(); if (padnew & (BTN_B | BTN_START | BTN_A)) { wait_release(); return; } }
}

static void line_res(u8 y, u8 r) {
  char o[34];
  u8 n = 0;
  n = scat(o, 0, res_name[r]);
  while (n < 9) o[n++] = ' ';
  n += utoa(whole_of(r), o + n);
  while (n < 16) o[n++] = ' ';
  o[n++] = '+'; n += utoa(flow_made[r] > 0 ? flow_made[r] : 0, o + n);
  while (n < 23) o[n++] = ' ';
  o[n++] = '-'; n += utoa(flow_used[r] > 0 ? flow_used[r] : 0, o + n);
  o[n] = 0;
  draw_box_text(2, y, o);
}

static void page_ledger(void) {
  u8 r, n, pg = 0;
  char o[34];
  for (;;) {
    page_begin(pg ? "THE TOWN" : "STORES");
    if (pg == 0) {
      draw_box_text(2, 3, "GOOD     STOCK   MADE  USED");
      for (r = 0; r < NRES; r++) line_res(5 + r, r);
      n = scat(o, 0, "FOOD CAP "); n = ncat(o, n, castle_sfood[castle_lvl]); n = scat(o, n, " GOODS CAP "); n = ncat(o, n, castle_sgoods[castle_lvl]);
      draw_box_text(2, 20, o);
      draw_box_text(2, 24, "A: THE TOWN   B: BACK");
    } else {
      n = scat(o, 0, "PEOPLE "); n = ncat(o, n, headcount()); n = scat(o, n, " OF "); n = ncat(o, n, r_cap); draw_box_text(2, 3, o);
      n = scat(o, 0, "CHILDREN "); n = ncat(o, n, pop_c); n = scat(o, n, " ADULTS "); n = ncat(o, n, pop_a); n = scat(o, n, " ELDERS "); n = ncat(o, n, pop_e); draw_box_text(2, 5, o);
      n = scat(o, 0, "WORKERS "); n = ncat(o, n, r_workforce); n = scat(o, n, " EMPLOYED "); n = ncat(o, n, r_employed); n = scat(o, n, " IDLE "); n = ncat(o, n, r_idle); draw_box_text(2, 7, o);
      n = scat(o, 0, "MOOD "); n = ncat(o, n, happy / 16); n = scat(o, n, "%  HEALTH "); n = ncat(o, n, health / 16); n = scat(o, n, "%"); draw_box_text(2, 9, o);
      n = scat(o, 0, "TAX "); n = ncat(o, n, tax); n = scat(o, n, "%  (LEFT/RIGHT CHANGES)"); draw_box_text(2, 11, o);
      n = scat(o, 0, "TAKEN "); n = ncat(o, n, r_taxes); n = scat(o, n, " UPKEEP "); n = ncat(o, n, r_upkeep); draw_box_text(2, 13, o);
      n = scat(o, 0, r_income >= 0 ? "INCOME +" : "INCOME -"); n = ncat(o, n, r_income >= 0 ? r_income : -r_income); n = scat(o, n, " A WEEK"); draw_box_text(2, 15, o);
      n = scat(o, 0, "FOOD "); n = ncat(o, n, (u16)r_foodsat * 100 / 255); n = scat(o, n, "%  FUEL "); n = ncat(o, n, (u16)r_fuelsat * 100 / 255); n = scat(o, n, "%"); draw_box_text(2, 17, o);
      n = scat(o, 0, "RANK "); n = scat(o, n, rank_name[rank]); n = scat(o, n, "  WARDS "); n = ncat(o, n, owned_count()); draw_box_text(2, 19, o);
      n = scat(o, 0, "RAIDS WON "); n = ncat(o, n, raids_won); n = scat(o, n, " LOST "); n = ncat(o, n, raids_lost); draw_box_text(2, 21, o);
      draw_box_text(2, 24, "A: STORES   B: BACK");
    }
    page_end();
    for (;;) {
      nmi_wait();
      if (padnew & BTN_B) { wait_release(); return; }
      if (padnew & BTN_A) { pg ^= 1; break; }
      if (pg && (padnew & BTN_LEFT) && tax > 0) { tax -= 5; break; }
      if (pg && (padnew & BTN_RIGHT) && tax < 30) { tax += 5; break; }
    }
  }
}

static void page_chronicle(void) {
  u8 i;
  page_begin("THE CHRONICLE");
  for (i = 0; i < 8; i++) draw_box_text(1, 4 + i * 2, chron[i]);
  draw_box_text(2, 24, "B: BACK");
  page_end();
  page_wait_b();
}

static void page_trade(void) {
  u8 sel = 1, r, redraw = 1;
  char o[34];
  for (;;) {
    if (redraw) {
      page_begin("TRADE: MERCHANTS AT THE GATE");
      draw_box_text(2, 3, "GOOD      SELL OVER  BUY UNDER");
      for (r = 1; r < NRES; r++) {
        u8 n = 0;
        o[0] = r == sel ? '>' : ' '; n = 1;
        n = scat(o, n, res_name[r]);
        while (n < 11) o[n++] = ' ';
        if (trade_sell[r]) { n = ncat(o, n, (u16)trade_sell[r] * 10); } else n = scat(o, n, "-");
        while (n < 21) o[n++] = ' ';
        if (trade_buy[r]) { n = ncat(o, n, (u16)trade_buy[r] * 10); } else n = scat(o, n, "-");
        o[n] = 0;
        draw_box_text(1, 4 + r, o);
      }
      draw_box_text(1, 20, "UP/DOWN CHOOSE  LEFT/RIGHT SELL");
      draw_box_text(1, 21, "A + LEFT/RIGHT: BUY UNDER");
      draw_box_text(1, 22, "MERCHANTS NEED A MARKET SQUARE");
      draw_box_text(1, 24, "B: BACK");
      page_end();
      redraw = 0;
    }
    nmi_wait();
    if (padnew & BTN_B) { wait_release(); return; }
    if (padnew & BTN_DOWN) { sel = sel >= NRES - 1 ? 1 : sel + 1; redraw = 1; }
    if (padnew & BTN_UP) { sel = sel <= 1 ? NRES - 1 : sel - 1; redraw = 1; }
    if (padnew & (BTN_LEFT | BTN_RIGHT)) {
      u8 *p = (pad & BTN_A) ? &trade_buy[sel] : &trade_sell[sel];
      if (padnew & BTN_RIGHT) { if (*p < 250) *p += (*p < 20 ? 5 : 10); }
      else if (*p >= 25) *p -= 10; else *p = 0;
      redraw = 1;
    }
  }
}

static void page_realm(void) {
  u8 w, redraw = 1, wx, wy, i;
  const char *why;
  char o[34];
  wsel = 4;
  for (;;) {
    if (redraw) {
      page_begin("THE REALM");
      for (w = 0; w < 9; w++) {
        u8 x0 = 3 + (w % 3) * 9, y0 = 4 + (w / 3) * 6;
        u8 own = (wards_owned >> w) & 1;
        ntfill(T_U_TL, x0, y0, 1, 1); ntfill(T_U_H, x0 + 1, y0, 6, 1); ntfill(T_U_TR, x0 + 7, y0, 1, 1);
        for (i = 1; i < 4; i++) { ntfill(T_U_V, x0, y0 + i, 1, 1); ntfill(T_U_VR, x0 + 7, y0 + i, 1, 1); }
        ntfill(T_U_BL, x0, y0 + 4, 1, 1); ntfill(T_U_HB, x0 + 1, y0 + 4, 6, 1); ntfill(T_U_BR, x0 + 7, y0 + 4, 1, 1);
        draw_box_text(x0 + 2, y0 + 1, w == 4 ? "CASTLE" : own ? "YOURS" : (dig_ward == w) ? "DIGGING" : "WILD");
        o[0] = w == wsel ? '>' : ' '; o[1] = 'A' + w; o[2] = w == wsel ? '<' : ' '; o[3] = 0;
        draw_box_text(x0 + 2, y0 + 3, o);
      }
      if (dig_ward != 0xFF) { u8 n = scat(o, 0, "DIGGING: "); n = ncat(o, n, (u16)bacc[0] / (u16)((u16)dig_weeks * 255 / 100 + 1)); n = scat(o, n, "%"); draw_box_text(2, 24, o); }
      else if (plan_annex_ok(wsel, &why)) { u8 owned = owned_count(), n; n = scat(o, 0, "A: DIG THE MOAT "); n = ncat(o, n, 260 + owned * 140); n = scat(o, n, "G"); draw_box_text(2, 24, o); }
      else draw_box_text(2, 24, why);
      draw_box_text(2, 26, "D-PAD CHOOSE  B: BACK");
      page_end();
      redraw = 0;
    }
    nmi_wait();
    wx = wsel % 3; wy = wsel / 3;
    if (padnew & BTN_B) { wait_release(); return; }
    if (padnew & BTN_LEFT && wx > 0) { wsel--; redraw = 1; }
    if (padnew & BTN_RIGHT && wx < 2) { wsel++; redraw = 1; }
    if (padnew & BTN_UP && wy > 0) { wsel -= 3; redraw = 1; }
    if (padnew & BTN_DOWN && wy < 2) { wsel += 3; redraw = 1; }
    if ((padnew & BTN_A) && dig_ward == 0xFF && plan_annex_ok(wsel, &why)) { start_annex(wsel); snd_play(2); redraw = 1; }
  }
}

void ui_castle(void) {
  char o[34];
  u8 n, nl = castle_lvl + 1;
  const char *why;
  for (;;) {
    page_begin("THE CASTLE");
    draw_box_text(2, 4, castle_name[castle_lvl]);
    n = scat(o, 0, "DEFENSE "); n = ncat(o, n, castle_def[castle_lvl]); n = scat(o, n, "  WARDS "); n = ncat(o, n, castle_wards[castle_lvl]); draw_box_text(2, 6, o);
    n = scat(o, 0, "GARRISON "); n = ncat(o, n, castle_jobs[castle_lvl]); n = scat(o, n, "  RANK "); n = scat(o, n, rank_name[rank]); draw_box_text(2, 8, o);
    if (castle_work) {
      n = scat(o, 0, "BUILDING: "); n = scat(o, n, castle_name[nl]); draw_box_text(2, 11, o);
      n = scat(o, 0, "PROGRESS "); n = ncat(o, n, castle_weeks_total / ((u16)castle_weeks[nl] * 255 / 100 + 1)); n = scat(o, n, "%"); draw_box_text(2, 13, o);
    } else if (castle_lvl < 3) {
      n = scat(o, 0, "NEXT: "); n = scat(o, n, castle_name[nl]); draw_box_text(2, 11, o);
      n = scat(o, 0, "COST "); n = ncat(o, n, castle_cost[nl][3]); n = scat(o, n, "G "); n = ncat(o, n, castle_cost[nl][0]); n = scat(o, n, "W "); n = ncat(o, n, castle_cost[nl][1]); n = scat(o, n, "S "); n = ncat(o, n, castle_cost[nl][2]); n = scat(o, n, "I"); draw_box_text(2, 13, o);
      n = scat(o, 0, "TAKES "); n = ncat(o, n, castle_weeks[nl]); n = scat(o, n, " WEEKS. HOLDS "); n = ncat(o, n, castle_wards[nl]); n = scat(o, n, " WARDS."); draw_box_text(2, 15, o);
      n = scat(o, 0, "NEEDS RANK "); n = scat(o, n, rank_name[castle_rank[nl]]); draw_box_text(2, 17, o);
      if (castle_plan_ok(&why)) draw_box_text(2, 20, "A: BEGIN THE WORK"); else draw_box_text(2, 20, why);
    } else draw_box_text(2, 11, "THE GREATEST OF CASTLES.");
    draw_box_text(2, 24, "B: BACK");
    page_end();
    for (;;) {
      nmi_wait();
      if (padnew & BTN_B) { wait_release(); return; }
      if ((padnew & BTN_A) && castle_plan_ok(&why)) { start_castle(); snd_play(2); break; }
    }
  }
}

/* ---------------------------------------------------------------- the title */
extern const u8 title_map[], title_attr[];
void ui_title(void) {
  u8 sel = 0, i, r, valid = (save_magic == 0xBEEF && save_flag == 1);
  u16 seed = 1, n;
  for (;;) {
    sprites_clear();
    nmi_wait();
    ppu_off();
    set_chr(2);
    mus_on = 0;
    vram_adr(0x2000);
    for (n = 0; n < 704; n++) PPUDATA = title_map[n];
    for (r = 22; r < 24; r++) { vram_adr(0x2000 + ((u16)r << 5)); for (i = 0; i < 32; i++) PPUDATA = title_map[21 * 32 + i]; }
    for (r = 24; r < 30; r++) ntfill(T_FONT + ' ', 0, r, 32, 1);
    vram_adr(0x23C0);
    for (i = 0; i < 64; i++) PPUDATA = title_attr[i];
    set_title_palettes();
    draw_box_text(5, 24, "A MEDIEVAL TOWN BUILDER");
    draw_box_text(11, 26, sel == 0 ? ">NEW REALM" : " NEW REALM");
    if (valid) draw_box_text(11, 27, sel == 1 ? ">CONTINUE" : " CONTINUE");
    ppu_on();
    for (;;) {
      nmi_wait();
      seed += frames + 7;
      if (padnew & BTN_UP && valid) { sel = 0; break; }
      if (padnew & BTN_DOWN && valid) { sel = 1; break; }
      if (padnew & (BTN_START | BTN_A)) { rng = seed | 1; snd_play(2); ret_a = sel; ppu_off(); set_chr(0); return; }
    }
  }
}

/* ---------------------------------------------------------------- the main menu */
static void f_main(u8 i, char *o) {
  static const char *const n[] = { "BACK TO THE MAP", "THE REALM", "STORES AND TOWN", "THE CASTLE", "TRADE", "THE CHRONICLE", "SPEED", "TAX", "SAVE AND QUIT" };
  u8 k = scat(o, 0, n[i]);
  if (i == 6) { k = scat(o, k, ": "); scat(o, k, paused ? "PAUSED" : speed == 1 ? "NORMAL" : speed == 2 ? "FAST" : "FASTEST"); }
  if (i == 7) { k = scat(o, k, " "); k = ncat(o, k, tax); scat(o, k, "%"); }
}

void ui_main_menu(void) {
  u8 sel = 0;
  for (;;) {
    sel = menu("MENU", 9, f_main, sel, 24);
    if (sel == 0xFF || sel == 0) { ret_a = 0; view_dirty = 1; return; }
    if (sel == 1) page_realm();
    else if (sel == 2) { sim_prime(); page_ledger(); }
    else if (sel == 3) ui_castle();
    else if (sel == 4) page_trade();
    else if (sel == 5) page_chronicle();
    else if (sel == 6) { if (paused) { paused = 0; speed = 1; } else if (speed == 1) speed = 2; else if (speed == 2) speed = 4; else paused = 1; continue; }
    else if (sel == 7) { tax = tax >= 20 ? 0 : tax + 5; continue; }
    else if (sel == 8) { ret_a = 1; return; }
    ret_a = 0; view_dirty = 1;
    return;
  }
}
