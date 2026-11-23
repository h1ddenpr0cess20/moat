/* Moat for the NES: shared declarations. cc65: int is 16 bits, long 32. */
#ifndef GAME_H
#define GAME_H
#include "data.h"

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned int u16;
typedef int s16;
typedef unsigned long u32;
typedef long s32;

/* ---- hardware (crt0.s) */
extern u8 vq_busy, oam_busy, pad, padnew, frames, ppu_mask, vq_len, scrx, scry;
extern u8 oam[256];
extern u8 vq[256];
void nmi_wait(void);
void snd_play(u8 id);
extern u8 mus_on, mus_season;

#define PPUCTRL (*(volatile u8 *)0x2000)
#define PPUMASK (*(volatile u8 *)0x2001)
#define PPUSTAT (*(volatile u8 *)0x2002)
#define PPUADDR (*(volatile u8 *)0x2006)
#define PPUDATA (*(volatile u8 *)0x2007)

#define BTN_A 0x80
#define BTN_B 0x40
#define BTN_SEL 0x20
#define BTN_START 0x10
#define BTN_UP 0x08
#define BTN_DOWN 0x04
#define BTN_LEFT 0x02
#define BTN_RIGHT 0x01

/* ---- the land */
#define MAPW 48
#define MAPH 48
#define WARD 16
#define WARDS 3
#define NTILES 2304
#define CX0 22
#define CY0 22
#define MAXB 56

/* ground (mt bits 0-2), trees (bits 3-4), ore/rock left (bits 5-7) */
#define GR_GRASS 0
#define GR_ROCK 1
#define GR_ORE 2
#define GR_WATER 3
#define GR_SAND 4
#define GR_MOAT 5
/* mr: road (bits 0-2), zone (bits 3-6), building (bit 7) */
#define RD_NONE 0
#define RD_DIRT 1
#define RD_COBBLE 2
#define RD_BRIDGE 3
#define RD_GATE 4
#define BLD 0x80

extern u8 mt[NTILES];
extern u8 mr[NTILES];
extern u8 conn[288];

extern const u16 row_off[];
extern const u16 wbit[];
#define IDX(x, y) (row_off[(y)] + (x))
#define GROUND(i) (mt[i] & 7)
#define TREE(i) ((mt[i] >> 3) & 3)
#define AMT(i) (mt[i] >> 5)
#define ROAD(i) (mr[i] & 7)
#define ZONE(i) ((mr[i] >> 3) & 15)

/* zones, as in the catalog */
#define Z_RES 1
#define Z_MARKET 2
#define Z_WHEAT 3
#define Z_VEG 4
#define Z_ORCHARD 5
#define Z_PASTURE 6
#define Z_CROFT 7
#define Z_CRAFT 8

/* goods */
#define R_GOLD 0
#define R_WOOD 1
#define R_STONE 2
#define R_IRON 3
#define R_TOOLS 4
#define R_GRAIN 5
#define R_BREAD 6
#define R_PRODUCE 7
#define R_MEAT 8
#define R_FISH 9
#define R_ALE 10
#define R_WOOL 11
#define R_CLOTH 12
#define NRES 13

#define TL_LOOK 0
#define TL_ROAD 1
#define TL_ZONE 2
#define TL_BUILD 3
#define TL_CLEAR 4
#define TL_BLOCK 5
#define BLK 9
#define MAXBLK 6

/* ---- banks */
#define BK_SIMA 0
#define BK_SIMB1 1
#define BK_SIMB2 4
#define BK_GEN 3
#define BK_VIEW 2
#define BK_UI 5
#define BK_SIMC 6
extern u8 cur_bank;
void set_bank(u8 b);
void set_chr(u8 b);
void call_in(u8 bank, void (*f)(void));
extern u8 arg_a, arg_b, arg_c, ret_a;
extern char whybuf[32];
extern u8 sbuf[704];
extern u8 tsub2, last_tool, last_sub, save_flag;
extern u16 save_magic;
extern const char *const zone_name[];
extern const u8 zone_cost[];
extern const char *const month_name[];
extern const char *const season_name[];
s16 whole_of(u8 r);
void rebuild_moat(void);
void burn_building(u8 id);
extern u16 farm_fields[8];

/* donut blocks: a loop of street with a ring of plots and a core */
extern u8 blk_n, blk_x[MAXBLK], blk_y[MAXBLK];
extern s16 blk_tax, blk_mood, blk_cap;   /* what the courtyards earn the realm this week: tax x256, mood x16, beds */
u8 block_ok(u8 x, u8 y);
u8 block_rating(u8 b);                  /* 0..255 */
u8 block_at(u8 x, u8 y);                /* index or 0xFF */
u8 block_stamp(u8 x, u8 y);             /* 0 ok, else a reason code */

/* ---- state (all of it in battery WRAM) */
extern u16 wards_owned;          /* bit w for ward wy*3+wx */
extern u8 gate_open[WARDS * WARDS]; /* bits: N E S W (1 = open) */
extern u8 dig_ward, dig_weeks, dig_prog, dig_crew; /* 0xFF when idle */

extern u8 bt[MAXB], bx[MAXB], by[MAXB], bs[MAXB], bprog[MAXB], bwk[MAXB], bef[MAXB], bfl[MAXB], baux[MAXB];
extern u16 bacc[MAXB];
#define BS_FREE 0
#define BS_BUILD 1
#define BS_READY 2
#define BF_CONN 1
#define BF_SMOKE 2

extern s32 stock[NRES];          /* x256 */
extern u16 pop_c, pop_a, pop_e;
extern s16 happy, health;        /* x16 */
extern u16 week;
extern u8 rank, castle_lvl, tax;
extern u8 castle_work, castle_prog, castle_crew; /* castle_work: weeks left of the work for tier castle_lvl+1, 0 = none */
extern u16 castle_weeks_total;
extern u8 diff;
extern u8 harvest_k, winter_k;   /* x64 */
extern u16 next_raid, next_merchant;
extern u8 raid_gate;             /* gate id being marched on, 0xFF none */
extern u8 plague;
extern s16 shock;
extern u16 takings;
extern u8 raids_won, raids_lost, raids_bought;
extern u8 gameover;
extern u16 rng;
extern u8 trade_sell[NRES], trade_buy[NRES];

/* weekly figures for the ledger */
extern s16 r_income, r_taxes, r_upkeep, r_workforce, r_employed, r_idle;
extern u8 r_foodsat, r_fuelsat, r_variety;
extern s32 r_food;
extern u16 r_cap;

/* ---- ppu.c */
void ppu_off(void);
void ppu_on(void);
void vq_put(u16 addr, const u8 *data, u8 len);
void vq_byte(u16 addr, u8 v);
void vram_adr(u16 a);
void vram_fill(u8 v, u16 n);
void text(u8 x, u8 y, const char *s);
void text_n(u8 x, u8 y, u16 v, u8 digits);       /* right-aligned number, space padded */
void text_pad(u8 x, u8 y, const char *s, u8 w);  /* text, padded with spaces to w */
void set_palettes(u8 season);
void set_title_palettes(void);
void sprites_clear(void);
void spr(u8 x, u8 y, u8 tile, u8 attr);
extern u8 spr_n;
void wait_frames(u8 n);
u8 text_len(const char *s);
void ntfill(u8 tile, u8 x, u8 y, u8 w, u8 h);   /* direct; rendering off */
void fill_attr_ui(u8 x, u8 y, u8 w, u8 h);

/* ---- world.c */
u8 rnd8(void);
u16 rnd16(void);
u8 rndn(u8 n);
void world_gen(u16 seed);
u8 ward_of(u8 x, u8 y);
u8 owned_at(u8 x, u8 y);
u8 owned_count(void);
u8 can_place(u8 type, u8 x, u8 y, u8 free);
u8 place_building(u8 type, u8 x, u8 y, u8 ready);
void remove_building(u8 id);
u8 find_building(u8 x, u8 y);
u8 adjacent_road(u8 x, u8 y, u8 w, u8 d, u8 connected);
void refresh_net(void);
extern u8 net_dirty;
u8 is_water(u8 i);
u8 count_near(u8 cx, u8 cy, u8 r, u8 kind);   /* 0 trees,1 rock,2 ore,3 water */
void set_road(u8 x, u8 y, u8 r);
void set_zone(u8 x, u8 y, u8 z);
u8 tile_ok_zone(u8 x, u8 y);
void connect_building(u8 id);
u8 autoplace(u8 type, u8 cx, u8 cy, u8 radius, u8 *ox, u8 *oy);
void complete_annex(u8 w);
void gate_set(u8 w, u8 side, u8 open);
u8 gate_pos(u8 w, u8 side, u8 *gx, u8 *gy);
u8 gate_exposed(u8 w, u8 side);
u8 nb_count(void);
u8 bldg_of_type(u8 type);

/* ---- sim.c */
void sim_new(void);
void sim_week(void);
u8 sim_step(void);
extern u8 sim_p;
void sim_prime(void);
u16 headcount(void);
void note(const char *msg);
extern char msgbuf[32];
extern u8 msg_new;
u16 pay_ok(u8 type);
void pay_cost(u8 type);
u8 place_player(u8 type, u8 x, u8 y);
void demolish_at(u8 x, u8 y);
u8 plan_annex_ok(u8 w, const char **why);
void start_annex(u8 w);
u8 castle_plan_ok(const char **why);
void start_castle(void);
extern const u16 castle_cost[4][4];
u16 housing_cap(u8 building);
u8 coverage_at(u8 kind, u8 x, u8 y);
void svc_rebuild(void);
extern u8 svc_n, svc_i[24];
extern u8 pk_x, pk_y;
u8 raid_threat(void);
u16 defense_of(u8 gate);
void save_ok(u8 v);
u8 season_of(u16 w);
u8 inyear(u16 w);
extern u8 flows_made_dirty;
extern s16 flow_made[NRES], flow_used[NRES];
void ledger_prep(void);
u8 gates_all(u8 *list);

/* ---- tables.c */
extern const u8 b_w[], b_d[], b_art[], b_weeks[], b_crew[], b_jobs[], b_cap[], b_upk[], b_shop[], b_cat[], b_auto[], b_level[], b_rank[];
extern const u16 b_off[], b_cw[], b_cs[], b_ci[], b_cg[];
extern const char *const b_name[];
extern const u8 w_kind[], w_res[], w_from[], w_rad[], w_tools[], w_season[], w_i0r[], w_i1r[], w_o0r[];
extern const u16 w_rate[], w_fuel[], w_i0a[], w_i1a[], w_o0a[], w_yg[], w_yp[], w_ym[], w_yw[], sv_ale[], st_food[], st_goods[];
extern const u8 sv_k[], sv_v[], sv_k2[], sv_v2[], sv_r[], b_def[], b_trade[];
extern const u8 castle_weeks[], castle_wards[], castle_def[], castle_jobs[], castle_rank[], castle_crews[], prices[];
extern const u16 castle_sfood[], castle_sgoods[];
extern const char *const castle_name[];
extern const char *const rank_name[];
extern const char *const res_name[];
extern const u8 rank_pop[], rank_wards[], rank_castle[], rank_bld[], res_spoil[];
extern u8 view_dirty;
u8 cold_season(void);
void mark(u8 x, u8 y);
void mark_rect(u8 x, u8 y, u8 w, u8 d);
extern u16 dirty_q[48];
extern u8 dirty_n;
extern char chron[8][32];

/* ---- main.c / ui.c */
extern u8 vx, vy, cx, cy, tool, tsub, speed, paused;
void redraw_view(void);
void refresh_tile(u8 x, u8 y);
void refresh_rect(u8 x, u8 y, u8 w, u8 h);
void hud_update(u8 force);
void status_update(void);
u8 tile_at(u8 x, u8 y);

#endif
