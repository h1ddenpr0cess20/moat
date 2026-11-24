/* PPU access: the vblank write queue, text, palettes, sprites. */
#include "game.h"

#define VQ_MAX 56
u8 spr_n;

void wait_frames(u8 n) { while (n--) nmi_wait(); }

void ppu_off(void) {
  ppu_mask = 0;
  nmi_wait();
  PPUMASK = 0;
}

void ppu_on(void) {
  PPUADDR = 0; PPUADDR = 0;
  ppu_mask = 0x1E;
  nmi_wait();
}

void vram_adr(u16 a) { PPUADDR = a >> 8; PPUADDR = a & 0xFF; }

void vram_fill(u8 v, u16 n) { while (n--) PPUDATA = v; }

void vq_put(u16 addr, const u8 *d, u8 len) {
  u8 n, i;
  if (!len) return;
  if (!(ppu_mask & 0x18)) {
    vram_adr(addr);
    for (i = 0; i < len; i++) PPUDATA = d[i];
    return;
  }
  while ((u8)(vq_len + len + 3) > VQ_MAX || (u8)(vq_len + len + 3) < vq_len) nmi_wait();
  vq_busy = 1;
  n = vq_len;
  vq[n++] = addr >> 8; vq[n++] = addr & 0xFF; vq[n++] = len;
  for (i = 0; i < len; i++) vq[n++] = d[i];
  vq_len = n;
  vq_busy = 0;
}

void vq_byte(u16 addr, u8 v) { vq_put(addr, &v, 1); }

static u8 tbuf[34];

static u8 tchar(u8 c) {
  if (c >= 'a' && c <= 'z') c -= 32;
  if (c < 0x20 || c > 0x5F) c = '?';
  return c + T_FONT;
}

u8 text_len(const char *s) { u8 n = 0; while (s[n]) n++; return n; }

void text(u8 x, u8 y, const char *s) {
  u8 n = 0;
  while (s[n] && n < 32) { tbuf[n] = tchar(s[n]); n++; }
  vq_put(0x2000 + ((u16)y << 5) + x, tbuf, n);
}

void text_pad(u8 x, u8 y, const char *s, u8 w) {
  u8 n = 0;
  while (s[n] && n < w) { tbuf[n] = tchar(s[n]); n++; }
  while (n < w) tbuf[n++] = T_FONT + ' ';
  vq_put(0x2000 + ((u16)y << 5) + x, tbuf, w);
}

void text_n(u8 x, u8 y, u16 v, u8 digits) {
  u8 i;
  for (i = digits; i > 0; i--) {
    if (v == 0 && i < digits) { tbuf[i - 1] = T_FONT + ' '; }
    else { tbuf[i - 1] = T_FONT + '0' + (u8)(v % 10); v /= 10; }
  }
  vq_put(0x2000 + ((u16)y << 5) + x, tbuf, digits);
}

void ntfill(u8 tile, u8 x, u8 y, u8 w, u8 h) {
  u8 r;
  for (r = 0; r < h; r++) { vram_adr(0x2000 + ((u16)(y + r) << 5) + x); vram_fill(tile, w); }
}

/* make the attribute blocks over a rectangle (tile units, even-aligned) palette 3 (the UI's) */
void fill_attr_ui(u8 x, u8 y, u8 w, u8 h) {
  u8 ax, ay;
  for (ay = y >> 2; ay <= (u8)((y + h - 1) >> 2); ay++) {
    vram_adr(0x23C0 + ((u16)ay << 3) + (x >> 2));
    for (ax = x >> 2; ax <= (u8)((x + w - 1) >> 2); ax++) PPUDATA = 0xFF;
  }
}

/* palettes: ground, then BG0..3 (foliage/brown/light; glint/water/plank; dark/cream/red; black/grey/white), then sprites */
static const u8 pal_season[4][4] = {
  { 0x1A, 0x0A, 0x17, 0x29 },
  { 0x19, 0x09, 0x17, 0x38 },
  { 0x18, 0x27, 0x07, 0x38 },
  { 0x20, 0x0B, 0x00, 0x10 },
};
void set_palettes(u8 s) {
  u8 i;
  u8 g = pal_season[s][0];
  tbuf[0] = g;
  tbuf[1] = pal_season[s][1]; tbuf[2] = pal_season[s][2]; tbuf[3] = pal_season[s][3];
  tbuf[4] = g; tbuf[5] = pal_season[s][1]; tbuf[6] = 0x17; tbuf[7] = 0x11;   /* water: foliage (trees by the shore stay green, waves dark), brown (roads), blue */
  tbuf[8] = g; tbuf[9] = 0x07; tbuf[10] = 0x37; tbuf[11] = 0x16;
  tbuf[12] = g; tbuf[13] = 0x0F; tbuf[14] = 0x10; tbuf[15] = 0x30;
  tbuf[16] = g; tbuf[17] = 0x00; tbuf[18] = 0x10; tbuf[19] = 0x30;      /* smoke */
  tbuf[20] = g; tbuf[21] = 0x07; tbuf[22] = 0x17; tbuf[23] = 0x27;      /* timber keep */
  tbuf[24] = g; tbuf[25] = 0x00; tbuf[26] = 0x10; tbuf[27] = 0x30;      /* stone castle */
  tbuf[28] = g; tbuf[29] = 0x16; tbuf[30] = 0x2A; tbuf[31] = 0x30;      /* cursor: red, green, white; banners */
  (void)i;
  vq_put(0x3F00, tbuf, 32);
}

void set_title_palettes(void) {
  static const u8 t[32] = { 0x0F, 0x11, 0x21, 0x30, 0x0F, 0x22, 0x32, 0x38, 0x0F, 0x11, 0x21, 0x30, 0x0F, 0x0F, 0x10, 0x30,
                            0x0F, 0x00, 0x10, 0x30, 0x0F, 0x00, 0x10, 0x30, 0x0F, 0x00, 0x10, 0x30, 0x0F, 0x00, 0x10, 0x30 };
  vq_put(0x3F00, t, 32);
}

void sprites_clear(void) {
  u8 i;
  for (i = 0; i < 64; i++) oam[i << 2] = 0xFF;
  spr_n = 0;
}

void spr(u8 x, u8 y, u8 tile, u8 attr) {
  u8 n = spr_n;
  if (n >= 64) return;
  n <<= 2;
  oam[n] = y - 1; oam[n + 1] = tile; oam[n + 2] = attr; oam[n + 3] = x;
  spr_n++;
}
