#!/usr/bin/env python3
"""Builds chr.bin and the generated C header (src/data.h) for the Moat NES port.

BG pattern table ($0000): terrain, roads, zones, UI, building kit, font.
Sprite pattern table ($1000): castle (four tiers), cathedral spire, cursor, smoke, raiders.
Game numbers (costs, crews, rates) come from the web game's own catalog.js, dumped to build/catalog.json.
"""
import json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
CAT = json.load(open(os.path.join(ROOT, 'build', 'catalog.json')))

# ------------------------------------------------------------------ canvas helpers
class Px:
    def __init__(s, w, h, fill=0):
        s.w, s.h = w, h
        s.p = [[fill] * w for _ in range(h)]
    def set(s, x, y, c):
        if 0 <= x < s.w and 0 <= y < s.h: s.p[y][x] = c
    def rect(s, x0, y0, x1, y1, c):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1): s.set(x, y, c)
    def hline(s, x0, x1, y, c): s.rect(x0, y, x1, y, c)
    def vline(s, x, y0, y1, c): s.rect(x, y0, x, y1, c)
    def art(s, x0, y0, rows, tr='.'):
        for dy, row in enumerate(rows):
            for dx, ch in enumerate(row):
                if ch != tr: s.set(x0 + dx, y0 + dy, int(ch) if ch.isdigit() else 0)
    def tile(s, tx, ty):
        return tuple(tuple(s.p[ty * 8 + y][tx * 8 + x] for x in range(8)) for y in range(8))

def tile_art(rows):
    p = Px(8, 8)
    p.art(0, 0, rows)
    return p.tile(0, 0)

def tile_bytes(px):
    p0 = [sum(((px[y][x] & 1) << (7 - x)) for x in range(8)) for y in range(8)]
    p1 = [sum((((px[y][x] >> 1) & 1) << (7 - x)) for x in range(8)) for y in range(8)]
    return bytes(p0 + p1)

BLANK = tuple(tuple([0] * 8) for _ in range(8))

# ------------------------------------------------------------------ BG tile registry
bg = [BLANK] * 256
names = {}          # name -> index
def put(idx, name, t):
    assert bg[idx] is BLANK or name is None, (idx, name)
    bg[idx] = t
    if name: names[name] = idx

# --- terrain (palette 0 nature: 1 foliage, 2 brown, 3 light accent;  palette 1 water: 1 glint, 2 water, 3 plank;  palette 3: 1 dark 2 grey 3 white)
put(0x00, 'grass', BLANK)
put(0x01, 'grass2', tile_art(['........', '...1....', '..1.1...', '........', '........', '.....1..', '....1.1.', '........']))
put(0x02, 'grass3', tile_art(['........', '........', '....3...', '...323..', '....3...', '........', '.1...1..', '........']))
put(0x03, 'water', tile_art(['22222222', '22222222', '22122222', '21212222', '22222222', '22222212', '22222121', '22222222']))
put(0x04, 'water2', tile_art(['22222222', '22221222', '22212122', '22222222', '22122222', '21212222', '22222222', '22222222']))
put(0x05, 'sand', tile_art(['........', '..3..3..', '........', '3....3..', '....3...', '.3...3.3', '........', '..3.....']))
put(0x06, 'rock', tile_art(['........', '..1111..', '.122321.', '.123221.', '.122221.', '..1111..', '.11.....', '1221....']))
put(0x07, 'rock2', tile_art(['........', '.....11.', '..11.122', '.1221122', '.1221.11', '..11.1..', '.1221...', '..11....']))
put(0x08, 'ore', tile_art(['........', '..1111..', '.123221.', '.121321.', '.123121.', '..1111..', '.11.....', '1331....']))
put(0x09, 'sapling', tile_art(['........', '........', '...1....', '..131...', '...1....', '...2....', '..22....', '........']))
put(0x0A, 'young', tile_art(['........', '..111...', '.11311..', '.11111..', '..111...', '...2....', '...2....', '..22....']))
put(0x0B, 'tree', tile_art(['..1111..', '.113111.', '11111311', '11311111', '11111131', '.111111.', '..1221..', '...22...']))
put(0x0C, 'tree2', tile_art(['...11...', '..1311..', '.111111.', '..11311.', '.111111.', '11131111', '..1221..', '...22...']))
put(0x0D, 'stump', tile_art(['........', '........', '........', '........', '..2222..', '.222222.', '..2222..', '........']))
put(0x0E, 'site', tile_art(['2......2', '2.2222.2', '2.2..2.2', '2.2222.2', '2.2..2.2', '2.2222.2', '2......2', '22222222']))
put(0x0F, 'burnt', tile_art(['........', '..1..1..', '.121.1..', '..1121..', '.11221..', '.1.2.21.', '..1..1..', '1.1.1.1.']))

# --- roads (bit0 N, bit1 E, bit2 S, bit3 W): dirt, cobble
def road_tile(mask, cobble):
    p = Px(8, 8)
    c = 2
    p.rect(2, 2, 5, 5, c)
    if mask & 1: p.rect(2, 0, 5, 1, c)
    if mask & 2: p.rect(6, 2, 7, 5, c)
    if mask & 4: p.rect(2, 6, 5, 7, c)
    if mask & 8: p.rect(0, 2, 1, 5, c)
    # a solitary tile with no neighbors: a little wider so it reads
    if mask == 0: p.rect(1, 1, 6, 6, c); p.rect(1, 1, 1, 1, 0); p.rect(6, 1, 6, 1, 0); p.rect(1, 6, 1, 6, 0); p.rect(6, 6, 6, 6, 0)
    for y in range(8):
        for x in range(8):
            if p.p[y][x] == 2:
                if cobble:
                    if (x + y) % 2 == 0: p.p[y][x] = 3
                    if (x // 2 + y // 2) % 2 == 0 and (x + y) % 2 == 1: p.p[y][x] = 2
                elif (x * 3 + y * 5) % 11 == 0: p.p[y][x] = 0
    return p.tile(0, 0)
for m in range(16):
    put(0x10 + m, f'dirt{m}', road_tile(m, False))
    put(0x20 + m, f'cobble{m}', road_tile(m, True))
put(0x30, 'bridgeH', tile_art(['22222222', '33333333', '32323232', '33333333', '33333333', '32323232', '33333333', '22222222']))
put(0x31, 'bridgeV', tile_art(['23333322', '23323322', '23333322', '23333322', '23323322', '23333322', '23333322', '23323322']))
put(0x32, 'moatw', tile_art(['22222222', '22222222', '21222122', '22121212', '22222222', '22222222', '22212221', '22121212']))
put(0x33, 'gateH', tile_art(['22222222', '33333333', '32323232', '33333333', '33333333', '32323232', '33333333', '22222222']))

# --- zones
ZPAT = {
  'res':     ['........', '.2....2.', '........', '........', '........', '.2....2.', '........', '........'],
  'market':  ['3.3.3.3.', '.3.3.3.3', '3.3.3.3.', '.3.3.3.3', '3.3.3.3.', '.3.3.3.3', '3.3.3.3.', '.3.3.3.3'],
  'wheat':   ['3.3.3.3.', '3.3.3.3.', '3.3.3.3.', '3.3.3.3.', '3.3.3.3.', '3.3.3.3.', '3.3.3.3.', '3.3.3.3.'],
  'veg':     ['........', '1111.111', '........', '........', '1111.111', '........', '........', '1111.111'],
  'orchard': ['........', '.11..11.', '.12..21.', '........', '........', '.11..11.', '.21..12.', '........'],
  'pasture': ['........', '.1....1.', '........', '....1...', '........', '1....1..', '........', '..1....1'],
  'croft':   ['2222.222', '2......2', '2.3333.2', '2......2', '2.3333.2', '2......2', '2222.222', '........'],
  'craft':   ['2.2.2.2.', '.2.2.2.2', '2.2.2.2.', '.2.2.2.2', '2.2.2.2.', '.2.2.2.2', '2.2.2.2.', '.2.2.2.2'],
}
ZONES = ['res', 'market', 'wheat', 'veg', 'orchard', 'pasture', 'croft', 'craft']
for i, z in enumerate(ZONES): put(0x34 + i, 'z_' + z, tile_art(ZPAT[z]))

# --- HUD icons (palette 3)
ICON = {
 'gold':  ['........', '..3333..', '.322223.', '.32.223.', '.32.223.', '.322223.', '..3333..', '........'],
 'wood':  ['........', '.222222.', '2333332.', '2.....2.', '2333332.', '.222222.', '........', '........'],
 'stone': ['........', '...22...', '..2332..', '.233332.', '23333332', '22222222', '........', '........'],
 'iron':  ['........', '.222222.', '.233332.', '..2332..', '..2332..', '.233332.', '22222222', '........'],
 'food':  ['...3....', '..3.3...', '.3.3.3..', '..3.3...', '.3.3.3..', '..3.3...', '...3....', '...3....'],
 'pop':   ['..3333..', '..3333..', '...33...', '.333333.', '3.3333.3', '..3333..', '..3..3..', '.33..33.'],
 'tools': ['......33', '.....3.3', '....33..', '...3.2..', '..3.2...', '.3.2....', '32......', '2.......'],
 'heart': ['........', '.33..33.', '3333333.', '3333333.', '.33333..', '..333...', '...3....', '........'],
}
ICONS = list(ICON)
for i, n in enumerate(ICONS): put(0x40 + i, 'i_' + n, tile_art(ICON[n]))
UI = {
 'tl': ['33333333', '3.......', '3.111111', '3.1.....', '3.1.....', '3.1.....', '3.1.....', '3.1.....'],
 'tr': ['33333333', '.......3', '111111.3', '.....1.3', '.....1.3', '.....1.3', '.....1.3', '.....1.3'],
 'bl': ['3.1.....', '3.1.....', '3.1.....', '3.1.....', '3.1.....', '3.111111', '3.......', '33333333'],
 'br': ['.....1.3', '.....1.3', '.....1.3', '.....1.3', '.....1.3', '111111.3', '.......3', '33333333'],
 'h':  ['33333333', '........', '11111111', '........', '........', '........', '........', '........'],
 'hb': ['........', '........', '........', '........', '........', '11111111', '........', '33333333'],
 'v':  ['3.1.....', '3.1.....', '3.1.....', '3.1.....', '3.1.....', '3.1.....', '3.1.....', '3.1.....'],
 'vr': ['.....1.3', '.....1.3', '.....1.3', '.....1.3', '.....1.3', '.....1.3', '.....1.3', '.....1.3'],
}
for i, n in enumerate(UI): put(0x48 + i, 'u_' + n, tile_art([r.replace('.', '1') for r in UI[n]]))

# --- font: 5x7 (from the tunnel NES), tile = ch + 0xA0 for ch in 0x20..0x5F; glyph = 3 on 1, so it reads on black
FONT = {
 ' ':[0,0,0,0,0],'0':[0x3E,0x51,0x49,0x45,0x3E],'1':[0,0x42,0x7F,0x40,0],'2':[0x42,0x61,0x51,0x49,0x46],
 '3':[0x21,0x41,0x45,0x4B,0x31],'4':[0x18,0x14,0x12,0x7F,0x10],'5':[0x27,0x45,0x45,0x45,0x39],
 '6':[0x3C,0x4A,0x49,0x49,0x30],'7':[0x01,0x71,0x09,0x05,0x03],'8':[0x36,0x49,0x49,0x49,0x36],
 '9':[0x06,0x49,0x49,0x29,0x1E],'A':[0x7E,0x11,0x11,0x11,0x7E],'B':[0x7F,0x49,0x49,0x49,0x36],
 'C':[0x3E,0x41,0x41,0x41,0x22],'D':[0x7F,0x41,0x41,0x22,0x1C],'E':[0x7F,0x49,0x49,0x49,0x41],
 'F':[0x7F,0x09,0x09,0x09,0x01],'G':[0x3E,0x41,0x49,0x49,0x7A],'H':[0x7F,0x08,0x08,0x08,0x7F],
 'I':[0,0x41,0x7F,0x41,0],'J':[0x20,0x40,0x41,0x3F,0x01],'K':[0x7F,0x08,0x14,0x22,0x41],
 'L':[0x7F,0x40,0x40,0x40,0x40],'M':[0x7F,0x02,0x0C,0x02,0x7F],'N':[0x7F,0x04,0x08,0x10,0x7F],
 'O':[0x3E,0x41,0x41,0x41,0x3E],'P':[0x7F,0x09,0x09,0x09,0x06],'Q':[0x3E,0x41,0x51,0x21,0x5E],
 'R':[0x7F,0x09,0x19,0x29,0x46],'S':[0x46,0x49,0x49,0x49,0x31],'T':[0x01,0x01,0x7F,0x01,0x01],
 'U':[0x3F,0x40,0x40,0x40,0x3F],'V':[0x1F,0x20,0x40,0x20,0x1F],'W':[0x3F,0x40,0x38,0x40,0x3F],
 'X':[0x63,0x14,0x08,0x14,0x63],'Y':[0x07,0x08,0x70,0x08,0x07],'Z':[0x61,0x51,0x49,0x45,0x43],
 '.':[0,0x60,0x60,0,0],'-':[8,8,8,8,8],':':[0,0x36,0x36,0,0],'!':[0,0,0x5F,0,0],
 '?':[2,1,0x51,9,6],',':[0,0x50,0x30,0,0],'/':[0x20,0x10,8,4,2],"'":[0,0x05,0x03,0,0],
 '+':[8,8,0x3E,8,8],'%':[0x23,0x13,8,0x64,0x62],'(':[0,0x1C,0x22,0x41,0],')':[0,0x41,0x22,0x1C,0],
 '=':[0x14,0x14,0x14,0x14,0x14],'>':[0,0x41,0x22,0x14,8],'<':[8,0x14,0x22,0x41,0],'*':[0x14,8,0x3E,8,0x14],
 '#':[0x14,0x7F,0x14,0x7F,0x14],';':[0,0x56,0x36,0,0],'"':[0,7,0,7,0],'&':[0x36,0x49,0x55,0x22,0x50],
 '$':[0x24,0x2A,0x7F,0x2A,0x12],'@':[0x7F,0x7F,0x7F,0x7F,0x7F],'^':[4,2,1,2,4],'_':[0x40,0x40,0x40,0x40,0x40],
 '[':[0,0x7F,0x41,0x41,0],']':[0,0x41,0x41,0x7F,0],'\\':[2,4,8,0x10,0x20],
}
def glyph(cols, fg=3, bg=1):
    p = Px(8, 8, bg)
    for cx, col in enumerate(cols):
        for y in range(7):
            if col >> y & 1: p.set(cx + 1, y, fg)
    return p.tile(0, 0)
FONT_OFF = 0xA0
for ch, cols in FONT.items():
    bg[ord(ch) + FONT_OFF] = glyph(cols)
for c in range(0x20, 0x60):
    if bg[c + FONT_OFF] is BLANK: bg[c + FONT_OFF] = glyph([0] * 5)

# ------------------------------------------------------------------ building kit (palette 2: 1 dark brown, 2 tan, 3 red)
kit_next = 0x58
KIT_END = 0xA0
kit_cache = {}
kit_pal = {}
def kit(name, t, pal=2):
    """Registers a tile in the kit (deduplicated by picture and palette)."""
    global kit_next
    if t == BLANK: return 0
    key = (t, pal)
    if key in kit_cache: names[name] = kit_cache[key]; return kit_cache[key]
    assert kit_next < KIT_END, f'out of building tiles at {name}'
    bg[kit_next] = t
    kit_cache[key] = kit_next
    kit_pal[kit_next] = pal
    names[name] = kit_next
    kit_next += 1
    return kit_next - 1

def roof_pat(mat, x, y):
    if mat == 'thatch': return 1 if (x * 2 + y * 3) % 7 == 0 else 2
    if mat == 'tile':   return 1 if (y % 4 == 3 or (x + (y // 4) * 4) % 8 == 7 and y % 4 != 3) else 3
    if mat == 'slate':  return 2 if (x + y * 2) % 6 == 0 else 1
    if mat == 'wood':   return 1 if y % 3 == 2 else 2
    raise ValueError(mat)

def roof_canvas(mat, rows, inset_max):
    """A 24-px-wide roof of `rows` tile rows: left tile, middle tile, right tile columns."""
    H = rows * 8
    p = Px(24, H)
    for y in range(H):
        ins = round(inset_max * (H - 1 - y) / (H - 1)) if H > 1 else 0
        for x in range(ins, 24 - ins):
            c = roof_pat(mat, x, y)
            if x == ins or x == 23 - ins or y == 0 and False: c = 1
            p.set(x, y, c)
        if y == H - 1:
            for x in range(0, 24): p.set(x, y, 1)
    # eave shadow stays on the last row; ridge line on the first row
    for x in range(inset_max, 24 - inset_max): p.set(x, 0, 1)
    return p

def roof_tiles(mat, rows, pal=2):
    p = roof_canvas(mat, rows, 3 if rows == 1 else 7)
    out = {}
    for r in range(rows):
        for ci, c in enumerate('LMR'):
            out[(r, c)] = kit(f'roof_{mat}{rows}_{r}{c}', p.tile(ci, r), pal)
    return out

def wall_tile(kind, mat='plaster'):
    p = Px(8, 8, 2)
    if mat == 'stone':
        for y in range(8):
            for x in range(8):
                if y % 4 == 3 or (x + (y // 4) * 4) % 8 == 7: p.set(x, y, 1)
    elif mat == 'timber':
        p.vline(0, 0, 7, 1); p.vline(7, 0, 7, 1); p.hline(0, 7, 0, 1)
        for i in range(1, 7): p.set(i, i, 1)
    if kind == 'win':
        p.rect(2, 2, 5, 5, 1); p.rect(3, 3, 4, 4, 3 if mat != 'stone' else 2)
    elif kind == 'door':
        p.rect(2, 1, 5, 7, 1); p.rect(3, 2, 4, 7, 3); p.set(4, 5, 1)
    elif kind == 'arch':
        p.rect(1, 2, 6, 7, 1); p.rect(2, 3, 5, 7, 0 if False else 1); p.set(1, 2, 2); p.set(6, 2, 2)
        p.rect(2, 4, 5, 7, 3); p.set(2, 3, 3); p.set(5, 3, 3)
    elif kind == 'slit':
        p.rect(3, 1, 4, 5, 1)
    elif kind == 'base':
        p.hline(0, 7, 7, 1)
    return p.tile(0, 0)

def wall(kind, mat='plaster', pal=2):
    return kit(f'wall_{mat}_{kind}', wall_tile(kind, mat), pal)

# Special tiles are drawn on a canvas and registered by name
def special(name, rows, pal=2):
    return kit(name, tile_art(rows), pal)

# ------------------------------------------------------------------ building art: lists of rows of tile names/indices
ART = {}

def house(w, h, rmat, wmat, deco=None):
    """A gabled front: a roof row (or two), then wall rows with windows and one door. Stone buildings are slate over stone, in the grey palette."""
    pal = 4 if wmat == 'stone' else 2
    if pal == 4: rmat = 'slate'
    rows = []
    rr = 1 if h <= 2 else 2 if h >= 3 and w >= 3 else 1
    rt = roof_tiles(rmat, rr, pal)
    for r in range(rr):
        rows.append([rt[(r, 'L' if c == 0 else 'R' if c == w - 1 else 'M')] for c in range(w)])
    wr = h - rr
    for r in range(wr):
        row = []
        for c in range(w):
            if r == wr - 1 and c == w // 2: row.append(wall('arch' if pal == 4 else 'door', wmat, pal))
            else: row.append(wall('win' if (c + r) % 2 == 0 else 'plain', wmat, pal))
        rows.append(row)
    return rows

def tiles_of(rows): return rows

def windmill():
    """A 3x3 windmill: a tower with a red cap and four sails, drawn on a canvas and cut into tiles (palette 2)."""
    import math
    p = Px(24, 24)
    hx, hy = 12, 10
    for arm in range(4):
        ang = math.pi / 4 + arm * math.pi / 2
        for r in range(1, 11):
            x = int(round(hx + math.cos(ang) * r)); y = int(round(hy + math.sin(ang) * r))
            p.set(x, y, 1)
            if r >= 4:
                nx = int(round(hx + math.cos(ang) * r - math.sin(ang))); ny = int(round(hy + math.sin(ang) * r + math.cos(ang)))
                p.set(nx, ny, 2)
    # tower
    for y in range(11, 24):
        half = 3 + (y - 11) // 5
        for x in range(hx - half, hx + half):
            p.set(x, y, 2 if (x != hx - half and x != hx + half - 1) else 1)
    p.hline(hx - 5, hx + 4, 23, 1)
    for y in range(7, 12):
        half = (y - 6)
        for x in range(hx - half, hx + half): p.set(x, y, 3 if x > hx - half else 1)
    p.rect(hx - 1, 17, hx, 23, 1)         # door
    p.rect(hx - 1, 13, hx, 14, 1)         # window
    return [[kit(f'mill_{r}{c}', p.tile(c, r)) for c in range(3)] for r in range(3)]

def build_art():
    A = ART
    A['cottage']      = house(2, 2, 'thatch', 'plaster')
    A['house']        = house(2, 2, 'tile', 'timber')
    A['townhouse']    = house(2, 3, 'tile', 'timber')
    aw = special('awning', ['33333333', '31313131', '33333333', '31313131', '.1....1.', '.1....1.', '.1....1.', '........'])
    aw2 = special('awning2', ['33333333', '13131313', '33333333', '13131313', '.1....1.', '.1....1.', '.1....1.', '........'])
    A['stall']        = [[aw, aw2]]
    A['shop']         = house(2, 2, 'tile', 'plaster')
    A['merchant']     = house(2, 2, 'tile', 'timber')
    A['croft']        = house(2, 2, 'thatch', 'timber')
    A['smallholding'] = house(2, 3, 'thatch', 'plaster')
    A['grange']       = house(2, 3, 'tile', 'plaster')
    A['workshop']     = house(2, 2, 'wood', 'plaster')
    A['joinery']      = house(2, 3, 'wood', 'timber')
    A['guildhall']    = house(2, 3, 'tile', 'timber')
    A['farmstead']    = house(3, 3, 'thatch', 'timber')
    A['granary']      = house(3, 3, 'wood', 'plaster')
    A['fishery']      = house(2, 3, 'thatch', 'plaster')
    A['hunter']       = house(2, 2, 'wood', 'timber')
    A['woodcutter']   = house(2, 2, 'wood', 'plaster')
    A['forester']     = house(2, 2, 'thatch', 'timber')
    A['weaver']       = house(2, 3, 'thatch', 'timber')
    A['brewery']      = house(2, 3, 'tile', 'plaster')
    A['tavern']       = house(3, 3, 'tile', 'timber')
    A['healer']       = house(2, 2, 'thatch', 'plaster')
    A['market_sq']    = None
    # stone buildings: grey palette
    A['storehouse']   = house(3, 3, 'slate', 'stone')
    A['smithy']       = house(2, 3, 'slate', 'stone')
    A['barracks']     = house(4, 3, 'slate', 'stone')
    # towers: a crenellated top over a wall
    cren_l = special('cren_l', ['1.11.11.', '1221221.', '12222221', '12222221', '12222221', '12222221', '12222221', '12222221'], 4)
    cren_r = special('cren_r', ['.11.11.1', '.1221221', '12222221', '12222221', '12222221', '12222221', '12222221', '12222221'], 4)
    A['watchtower']   = [[cren_l, cren_r], [wall('slit', 'stone', 4), wall('arch', 'stone', 4)]]
    A['gatehouse']    = [[cren_l, cren_r], [wall('arch', 'stone', 4), wall('slit', 'stone', 4)]]
    spire = special('spire', ['...1....', '..131...', '..121...', '.11211..', '.12221..', '1122211.', '12222211', '12222221'], 4)
    A['chapel']       = house(3, 4, 'slate', 'stone')
    A['chapel'][0] = [0, spire, 0]
    sp_l = special('spire_l', ['.......1', '......13', '.....12.', '....112.', '...1122.', '..11222.', '.112222.', '11222221'], 4)
    sp_r = special('spire_r', ['1.......', '31......', '.21.....', '.211....', '.2211...', '.22211..', '.222211.', '12222211'], 4)
    A['cathedral']    = house(4, 5, 'slate', 'stone')
    A['cathedral'][0] = [0, sp_l, sp_r, 0]
    A['mill']         = windmill()
    A['quarry']       = None
    A['ironmine']     = None
    well = special('well', ['........', '..1111..', '.1.22.1.', '.1.22.1.', '..1111..', '.111111.', '.122221.', '.111111.'])
    A['well']         = [[well]]
    pole = special('maypole', ['...3....', '..3.3...', '...3.3..', '...1....', '...1....', '...1....', '...1....', '..111...'])
    bench = special('bench', ['........', '........', '........', '........', '11111111', '12222221', '1.1..1.1', '........'])
    flowers = special('flowers', ['........', '.3..3...', '..1.1.3.', '........', '3...3...', '.1.1..1.', '..3..3..', '........'])
    A['green']        = [[flowers, 0, flowers], [bench, pole, bench], [flowers, 0, flowers]]
    cross = special('mcross', ['3333.333', '3.3..3.3', '33.11.33', '...11...', '.1.11.1.', '..1111..', '3.1111.3', '33333333'])
    plaza = names['cobble15']
    A['market']       = [[plaza, plaza, plaza], [plaza, cross, plaza], [plaza, plaza, plaza]]
    # a quarry and a mine are stone yards with a little gear
    pit = special('pit', ['.2222...', '2111122.', '2111112.', '2111112.', '.211112.', '..22222.', '........', '........'], 4)
    crane = special('crane', ['...1....', '..1.1...', '..1.1...', '.1...1..', '.1111...', '1....1..', '1.....1.', '........'], 4)
    adit = special('adit', ['.222222.', '22111122', '21111112', '21111112', '21111112', '21111112', '21111112', '22222222'], 4)
    frame = special('headframe', ['..1..1..', '..1111..', '.1.11.1.', '.1.11.1.', '1..11..1', '1..11..1', '11111111', '........'], 4)
    A['quarry']       = [[names['rock'], names['rock2'], names['rock']], [names['rock2'], pit, crane], [names['rock'], names['rock2'], names['rock']]]
    A['ironmine']     = [[names['rock'], frame, names['rock2']], [names['rock2'], adit, names['ore']], [names['ore'], names['rock'], names['rock2']]]

# order of buildings as in the catalog
BNAMES = list(CAT['BUILDINGS'])
DIM_OVERRIDE = {'cathedral': (4, 5)}
def dims(n):
    w, d = CAT['BUILDINGS'][n]['size']
    return DIM_OVERRIDE.get(n, (w, d))

# the building art must match the footprint
def art_for(n):
    w, d = dims(n)
    rows = ART[n]
    assert len(rows) == d and all(len(r) == w for r in rows), (n, w, d, len(rows), [len(r) for r in rows])
    return rows

# ------------------------------------------------------------------ sprites ($1000 table)
spr = [BLANK] * 256
def sput(i, t): spr[i] = t

def castle_canvas(level):
    """32x32 px castle on its island, for the four tiers. Palette 1 of the sprites: 1 black, 2 stone, 3 pale."""
    p = Px(32, 32)
    def tower(x, y, w, h):
        p.rect(x, y, x + w - 1, y + h - 1, 2)
        p.vline(x, y, y + h - 1, 1); p.vline(x + w - 1, y, y + h - 1, 1); p.hline(x, x + w - 1, y + h - 1, 1)
        p.hline(x, x + w - 1, y, 1)
        for cx in range(x, x + w, 2): p.set(cx, y - 1, 1)
        p.rect(x + w // 2 - 1, y + 3, x + w // 2, y + 5, 1)
    def keep(x, y, w, h, roof):
        p.rect(x, y, x + w - 1, y + h - 1, 3 if roof == 'timber' else 2)
        p.hline(x, x + w - 1, y, 1); p.hline(x, x + w - 1, y + h - 1, 1)
        p.vline(x, y, y + h - 1, 1); p.vline(x + w - 1, y, y + h - 1, 1)
        for yy in range(y + 2, y + h - 3, 4):
            for xx in range(x + 2, x + w - 2, 4): p.rect(xx, yy, xx + 1, yy + 1, 1)
    def door(x, y):
        p.rect(x, y, x + 3, y + 5, 1); p.rect(x + 1, y + 1, x + 2, y + 5, 3)
    def pennant(x, y):
        p.vline(x, y - 5, y, 1); p.rect(x + 1, y - 5, x + 3, y - 3, 3)
    if level == 0:      # timber keep: palisade and an oak keep on a mound
        for x in range(2, 30, 2): p.vline(x, 14, 27, 1); p.vline(x + 1, 14, 27, 2); p.set(x, 13, 1); p.set(x + 1, 14, 3)
        keep(9, 4, 14, 14, 'timber'); p.hline(8, 23, 3, 1); p.rect(10, 1, 21, 2, 2); p.hline(10, 21, 0, 1)
        pennant(15, 3); door(14, 22)
    elif level == 1:    # stone keep, curtain wall, four small towers
        p.rect(3, 16, 28, 27, 2); p.hline(3, 28, 16, 1); p.hline(3, 28, 27, 1)
        for x in range(4, 28, 3): p.rect(x, 14, x + 1, 15, 2)
        keep(10, 4, 12, 14, 'stone')
        tower(1, 10, 5, 18); tower(26, 10, 5, 18)
        pennant(15, 3); door(14, 22)
    elif level == 2:    # castle: great hall, inner ward, taller towers
        p.rect(2, 18, 29, 27, 2); p.hline(2, 29, 18, 1); p.hline(2, 29, 27, 1)
        for x in range(3, 29, 3): p.rect(x, 16, x + 1, 17, 2)
        keep(8, 3, 16, 17, 'stone'); keep(12, 0, 8, 4, 'stone')
        tower(0, 8, 6, 20); tower(26, 8, 6, 20)
        pennant(15, 2); door(14, 22)
    else:               # citadel: concentric walls, barbican, many towers
        p.rect(0, 20, 31, 27, 2); p.hline(0, 31, 20, 1); p.hline(0, 31, 27, 1)
        for x in range(0, 31, 3): p.rect(x, 18, x + 1, 19, 2)
        p.rect(4, 12, 27, 20, 2); p.hline(4, 27, 12, 1)
        keep(9, 2, 14, 18, 'stone'); keep(13, 0, 6, 3, 'stone')
        tower(0, 8, 5, 20); tower(27, 8, 5, 20); tower(7, 10, 5, 10); tower(20, 10, 5, 10)
        pennant(16, 1); door(14, 22)
    return p

SPR_CASTLE = 0x00   # 4 levels x 16 tiles, tile n of level L at SPR_CASTLE + L*16 + ty*4 + tx
for L in range(4):
    cv = castle_canvas(L)
    for ty in range(4):
        for tx in range(4): sput(SPR_CASTLE + L * 16 + ty * 4 + tx, cv.tile(tx, ty))
# cursor: corners and a box in three colors (palette 3 of the sprites: 1 red, 2 green, 3 white): 0x40 valid, 0x50 not, 0x60 look
def cursor_set(base, c):
    k = str(c)
    rows = {
        0: ['KKKK....', 'K.......', 'K.......', 'K.......', '........', '........', '........', '........'],
        1: ['....KKKK', '.......K', '.......K', '.......K', '........', '........', '........', '........'],
        2: ['........', '........', '........', '........', 'K.......', 'K.......', 'K.......', 'KKKK....'],
        3: ['........', '........', '........', '........', '.......K', '.......K', '.......K', '....KKKK'],
        4: ['KKKKKKKK', 'K......K', 'K......K', 'K......K', 'K......K', 'K......K', 'K......K', 'KKKKKKKK'],
    }
    for i, r in rows.items(): sput(base + i, tile_art([x.replace('K', k) for x in r]))
cursor_set(0x40, 2); cursor_set(0x50, 1); cursor_set(0x60, 3)
# smoke: three frames 0x48..0x4A
sput(0x48, tile_art(['........', '........', '........', '........', '...22...', '..2332..', '...22...', '........']))
sput(0x49, tile_art(['........', '........', '..2.....', '.232....', '..2.22..', '....232.', '.....2..', '........']))
sput(0x4A, tile_art(['........', '..2..2..', '.2.22.2.', '..2..2..', '........', '...2....', '..2.2...', '........']))
# raider: spear and shield (0x4B,0x4C) / banner 0x4D
sput(0x4B, tile_art(['...1....', '..111...', '...1....', '.11111..', '1.111.1.', '..1.1...', '..1.1...', '.11.11..']))
sput(0x4C, tile_art(['3.......', '33......', '3.3.....', '3.3.....', '3.......', '3.......', '3.......', '3.......']))
sput(0x4D, tile_art(['111111..', '1111111.', '111111..', '11111...', '1.......', '1.......', '1.......', '1.......']))
sput(0x4E, tile_art(['.11111..', '1111111.', '.111111.', '..1111..', '1.......', '1.......', '1.......', '1.......']))
# ------------------------------------------------------------------ the title picture (CHR bank 1)
import random
def title_scene():
    """A moonlit castle on its island. Returns (bank1 chr bytes, 32x22 tile map, attribute bytes)."""
    rnd = random.Random(7)
    W, H = 256, 176
    img = Px(W, H)
    def dith(a, b, x, y, k):   # k 0..4: share of b in a 4x4 ordered dither
        m = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]][y & 3][x & 3]
        return b if m < k * 4 else a
    # sky (palette 0: 0 navy, 1 blue, 2 light blue, 3 white), cells y < 96
    for y in range(96):
        for x in range(W):
            t = y / 96.0
            if t < 0.18: v = 0
            elif t < 0.42: v = dith(0, 1, x, y, int((t - 0.18) / 0.24 * 4) + 1)
            elif t < 0.6: v = 1
            elif t < 0.86: v = dith(1, 2, x, y, int((t - 0.6) / 0.26 * 4) + 1)
            else: v = 2
            img.set(x, y, v)
    for _ in range(16):
        x, y = rnd.randrange(W), rnd.randrange(0, 56)
        img.set(x, y, 3)
    # the moon
    mx, my, mr = 204, 34, 14
    for y in range(my - mr - 2, my + mr + 3):
        for x in range(mx - mr - 2, mx + mr + 3):
            d = ((x - mx) ** 2 + (y - my) ** 2) ** 0.5
            if d <= mr: img.set(x, y, 3)
            elif d <= mr + 2 and img.p[y][x] < 2: img.set(x, y, 2)
    for cx, cy, r in ((199, 30, 3), (208, 40, 2), (210, 28, 2)):
        for y in range(cy - r, cy + r + 1):
            for x in range(cx - r, cx + r + 1):
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r: img.set(x, y, 2)
    # the horizon and the castle (palette 1: 0 silhouette, 1 periwinkle, 2 pale, 3 window yellow), cells 96..143
    for y in range(96, 144):
        for x in range(W):
            t = (y - 96) / 48.0
            img.set(x, y, dith(1, 2, x, y, 4 - int(t * 4)) if t < 0.5 else 2)
    import math
    for x in range(W):                      # far hills
        h = int(122 + 7 * math.sin(x / 23.0) + 4 * math.sin(x / 9.0 + 1))
        for y in range(h, 144): img.set(x, y, 0)
    def block(x0, y0, x1, y1):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1): img.set(x, y, 0)
    def crenel(x0, x1, y):
        for x in range(x0, x1 + 1, 4): block(x, y - 3, x + 1, y)
    def tower(x0, x1, top, roof=0):
        block(x0, top, x1, 143); crenel(x0, x1 - 1, top)
        if roof:
            c = (x0 + x1) // 2
            for i in range(roof):
                for x in range(c - (roof - i) // 2 - 1, c + (roof - i) // 2 + 2): img.set(x, top - 4 - i, 0)
    block(52, 122, 204, 143); crenel(52, 203, 122)          # curtain wall
    tower(60, 74, 100); tower(182, 196, 100)               # corner towers
    tower(104, 152, 86); crenel(104, 151, 86)                # the keep
    tower(116, 140, 66, 16)                                 # the high hall with a pointed roof
    tower(92, 102, 98, 10); tower(154, 164, 98, 10)
    for wx, wy in ((66, 108), (66, 118), (188, 108), (188, 118), (110, 96), (118, 96), (126, 96), (134, 96), (142, 96), (110, 108), (126, 108), (142, 108), (122, 74), (130, 74), (126, 82), (96, 108), (158, 108), (80, 128), (100, 130), (150, 130), (172, 128)):
        block(wx, wy, wx + 1, wy + 3)
        for y in range(wy, wy + 4):
            for x in range(wx, wx + 2): img.set(x, y, 3)
    block(124, 128, 131, 143)                               # the gate
    for y in range(128, 144):
        for x in range(124, 132):
            if (x - 124) ** 2 / 16 + (y - 136) ** 2 / 64 < 1 or y >= 136: img.set(x, y, 3 if (y > 130 and 125 <= x <= 130) else 0)
    # the water (palette 2: 0 navy, 1 blue, 2 light, 3 white), cells 144..175
    for y in range(144, H):
        for x in range(W):
            t = (y - 144) / 32.0
            img.set(x, y, dith(0, 1, x, y, int(t * 3) + 1))
    for ty in range(18, 22):                                # ripples: a handful of repeating patterns
        for tx in range(32):
            k = (tx * 5 + ty * 3) % 9
            if k < 4:
                y = ty * 8 + 2 + k * 2
                for i in range(4 + k): img.set(tx * 8 + 1 + i, y, 2 if (tx + ty) % 3 else 3)
    # the name, in letters three times the size of the font's
    def big(ch, x0, y0, sc, c):
        cols = FONT[ch]
        for cx, col in enumerate(cols):
            for yy in range(7):
                if col >> yy & 1:
                    for dx in range(sc):
                        for dy in range(sc): img.set(x0 + cx * sc + dx, y0 + yy * sc + dy, c)
    x = 80
    for ch in 'MOAT':
        big(ch, x, 16, 3, 3)
        x += 24
    # tiles
    tiles, tmap, seen = [], [], {}
    for ty in range(H // 8):
        for tx in range(W // 8):
            t = img.tile(tx, ty)
            if t not in seen:
                seen[t] = len(tiles); tiles.append(t)
            tmap.append(seen[t])
    assert len(tiles) <= 0xC0, len(tiles)
    chr_bg = [BLANK] * 256
    for i, t in enumerate(tiles): chr_bg[i] = t
    for c in range(0x20, 0x60): chr_bg[c + FONT_OFF] = bg[c + FONT_OFF]
    # a solid panel tile for the menu rows: the font's space
    attr = [0] * 64
    for ay in range(8):
        for ax in range(8):
            cy = ay * 2
            p = 0 if cy < 6 else 1 if cy < 9 else 2 if cy < 11 else 3
            q = 0 if p == 3 else p
            attr[ay * 8 + ax] = (p | p << 2 | p << 4 | p << 6) if cy < 11 else 0xFF
    return b''.join(tile_bytes(t) for t in chr_bg), tmap, attr, len(tiles)

# ------------------------------------------------------------------ palettes ($3F00..)
def pal_rows():
    # universal ground, then P0..P3 (3 each); seasons change the ground, canopy and crop colors
    seasons = {
        'spring': (0x2A, 0x1A, 0x17, 0x28),
        'summer': (0x1A, 0x09, 0x17, 0x38),
        'autumn': (0x28, 0x16, 0x17, 0x27),
        'winter': (0x20, 0x00, 0x10, 0x10),
    }
    return seasons

# ------------------------------------------------------------------ write everything
def main():
    build_art()
    chr_bg = b''.join(tile_bytes(t) for t in bg)
    chr_sp = b''.join(tile_bytes(t) for t in spr)
    tchr, tmap, tattr, ntt = title_scene()
    # bank 1 sprites: the castle tiers again are not needed; keep the cursor/others blank
    open(os.path.join(ROOT, 'build', 'chr.bin'), 'wb').write(chr_bg + chr_sp + tchr + b'\x00' * 4096)
    # --- C header
    H = []
    H.append('/* generated by tools/assets.py: do not edit */')
    H.append('#ifndef DATA_H\n#define DATA_H')
    for n, i in sorted(names.items(), key=lambda kv: kv[1]):
        if n.startswith(('dirt', 'cobble', 'roof_', 'wall_')): continue
        H.append(f'#define T_{n.upper()} 0x{i:02X}')
    H.append('#define T_DIRT 0x10\n#define T_COBBLE 0x20\n#define T_FONT 0xA0\n#define T_ZONE 0x34\n#define T_ICON 0x40')
    H.append(f'#define NB {len(BNAMES)}')
    for i, n in enumerate(BNAMES): H.append(f'#define B_{n.upper()} {i}')
    H.append('#endif')
    open(os.path.join(ROOT, 'src', 'data.h'), 'w').write('\n'.join(H) + '\n')
    # --- C tables
    T = ['/* generated by tools/assets.py: do not edit */', '#include "data.h"']
    TA = ['/* generated by tools/assets.py: do not edit */', '#pragma rodata-name(push, "BANK0R")', '#include "data.h"']
    TV = ['/* generated by tools/assets.py: do not edit */', '#pragma rodata-name(push, "BANK2R")', '#include "data.h"']
    def arr(name, vals, typ='const unsigned char'):
        tgt = TA if name.startswith(('w_', 'st_')) else TV if name in ('b_art', 'b_off') else T
        tgt.append(f'{typ} {name}[] = {{' + ','.join(str(v) for v in vals) + '};')
    arr('b_w', [dims(n)[0] for n in BNAMES])
    arr('b_d', [dims(n)[1] for n in BNAMES])
    offs = []; tiles = []
    for n in BNAMES:
        offs.append(len(tiles))
        for row in art_for(n): tiles.extend(row)
    arr('b_art', tiles); arr('b_off', offs, 'const unsigned int')
    B = CAT['BUILDINGS']
    for res in ('wood', 'stone', 'iron', 'gold'):
        arr('b_c' + res[0], [B[n].get('cost', {}).get(res, 0) for n in BNAMES], 'const unsigned int')
    arr('b_weeks', [B[n]['weeks'] for n in BNAMES])
    arr('b_crew', [B[n]['crew'] for n in BNAMES])
    arr('b_jobs', [B[n].get('jobs', 0) for n in BNAMES])
    arr('b_cap', [B[n].get('cap', 0) for n in BNAMES])
    arr('b_upk', [round(B[n].get('upkeep', 0) * 10) for n in BNAMES])
    arr('b_shop', [round(B[n].get('shop', 0) * 10) for n in BNAMES])
    cats = {'housing': 0, 'market': 1, 'food': 2, 'industry': 3, 'civic': 4, 'defense': 5}
    arr('b_cat', [cats[B[n]['cat']] for n in BNAMES])
    zid = {z: i + 1 for i, z in enumerate(CAT['ZONES'])}
    arr('b_auto', [zid.get(B[n].get('auto'), 0) for n in BNAMES])
    arr('b_level', [B[n].get('level', 0) for n in BNAMES])
    arr('b_rank', [B[n].get('rank', 0) for n in BNAMES])
    # ---- work, services, stores
    RES = list(CAT['RESOURCES'])
    ri = {r: i for i, r in enumerate(RES)}
    q8 = lambda v: int(round(v * 256))
    scale_r = lambda r: max(2, int(round(r * 0.62)))
    kinds = {'gather': 1, 'grow': 2, 'make': 3, 'farm': 4, 'plant': 5}
    froms = {'trees': 0, 'rock': 1, 'ore': 2, 'water': 3}
    W = [B[n].get('work') for n in BNAMES]
    arr('w_kind', [kinds.get(w['kind'], 0) if w else 0 for w in W])
    arr('w_res', [ri[w['res']] if w and w['kind'] == 'gather' else 0 for w in W])
    arr('w_rate', [q8(w['rate']) if w and w['kind'] == 'gather' else 0 for w in W], 'const unsigned int')
    arr('w_from', [froms[w['from']] if w and w['kind'] == 'gather' else 0 for w in W])
    arr('w_rad', [scale_r(w['radius']) if w and w['kind'] in ('gather', 'plant') else 0 for w in W])
    arr('w_tools', [1 if w and w.get('tools') else 0 for w in W])
    arr('w_fuel', [q8(w.get('fuel', 0)) if w else 0 for w in W], 'const unsigned int')
    seas = []
    for w in W:
        sv = (w or {}).get('season') or [1, 1, 1, 1]
        seas.extend(int(round(x * 64)) for x in sv)
    arr('w_season', seas)
    def mk(w, key, i):
        if not w or w['kind'] != 'make': return (0, 0)
        items = list(w[key].items())
        if i < len(items): return (ri[items[i][0]], q8(items[i][1]))
        return (0, 0)
    for nm, key, i in (('i0', 'in', 0), ('i1', 'in', 1), ('o0', 'out', 0)):
        arr(f'w_{nm}r', [mk(w, key, i)[0] for w in W]); arr(f'w_{nm}a', [mk(w, key, i)[1] for w in W], 'const unsigned int')
    for nm, r in (('yg', 'grain'), ('yp', 'produce'), ('ym', 'meat'), ('yw', 'wool')):
        arr('w_' + nm, [q8(w['yields'].get(r, 0)) if w and w['kind'] == 'grow' else 0 for w in W], 'const unsigned int')
    svk = {'water': 1, 'cheer': 2, 'faith': 3, 'health': 4, 'security': 5, 'commerce': 6}
    SV = [B[n].get('serve', {}) for n in BNAMES]
    def sv(s, i):
        ks = [k for k in s if k in svk]
        return (svk[ks[i]], int(round(s[ks[i]] * 16))) if i < len(ks) else (0, 0)
    arr('sv_k', [sv(s, 0)[0] for s in SV]); arr('sv_v', [sv(s, 0)[1] for s in SV])
    arr('sv_k2', [sv(s, 1)[0] for s in SV]); arr('sv_v2', [sv(s, 1)[1] for s in SV])
    arr('sv_r', [scale_r(s['radius']) if s else 0 for s in SV])
    arr('sv_ale', [q8(s.get('ale', 0)) for s in SV], 'const unsigned int')
    arr('b_def', [B[n].get('defense', 0) for n in BNAMES])
    arr('b_trade', [1 if B[n].get('trade') else 0 for n in BNAMES])
    arr('st_food', [B[n].get('store', {}).get('food', 0) for n in BNAMES], 'const unsigned int')
    arr('st_goods', [B[n].get('store', {}).get('goods', 0) for n in BNAMES], 'const unsigned int')
    CL = CAT['CASTLE_LEVELS']
    T.append('const unsigned int castle_cost[4][4] = {' + ','.join('{%d,%d,%d,%d}' % tuple((c['cost'] or {}).get(r, 0) for r in ('wood', 'stone', 'iron', 'gold')) for c in CL) + '};')
    arr('castle_weeks', [c['weeks'] for c in CL])
    arr('castle_wards', [2, 4, 6, 9])
    arr('castle_def', [c['defense'] for c in CL])
    arr('castle_jobs', [c['jobs'] for c in CL])
    arr('castle_rank', [c['rank'] for c in CL])
    arr('castle_sfood', [c['store']['food'] for c in CL], 'const unsigned int')
    arr('castle_sgoods', [c['store']['goods'] for c in CL], 'const unsigned int')
    arr('castle_crews', [4, 6, 8, 10])
    arr('prices', [CAT['PRICES'].get(r, 0) for r in RES])
    T.append('const char *const castle_name[] = {' + ','.join('"%s"' % c['name'].upper() for c in CL) + '};')
    T.append('const char *const rank_name[] = {"BARON","VISCOUNT","EARL","MARQUESS","DUKE"};')
    T.append('const char *const res_name[] = {' + ','.join('"%s"' % CAT['RESOURCES'][r]['name'].upper() for r in RES) + '};')
    arr('rank_pop', [0, 40, 100, 200, 350]); arr('rank_wards', [0, 2, 3, 5, 7])
    arr('rank_castle', [0, 0, 0, 2, 0]); arr('rank_bld', [255, 255, B_CHAPEL if False else BNAMES.index('chapel'), 255, BNAMES.index('cathedral')])
    arr('res_spoil', [round(CAT['RESOURCES'][r]['spoil'] * 1000) for r in RES])
    pal = []
    for i in range(256):
        if i in (3, 4, 0x30, 0x31, 0x32, 0x33): pal.append(1)
        elif i in (6, 7, 8): pal.append(3)
        elif 0x58 <= i < 0xA0: pal.append(kit_pal.get(i, 2))
        else: pal.append(0)
    arr('tile_pal', pal)
    arr('row_off', [y * 48 for y in range(48)], 'const unsigned int')
    arr('wbit', [1 << i for i in range(9)], 'const unsigned int')
    T.append('const char *const b_name[] = {' + ','.join('"%s"' % B[n]['name'].upper().replace('’', "'")[:16] for n in BNAMES) + '};')
    open(os.path.join(ROOT, 'src', 'tables.c'), 'w').write('\n'.join(T) + '\n')
    open(os.path.join(ROOT, 'src', 'tab_a.c'), 'w').write('\n'.join(TA) + '\n')
    open(os.path.join(ROOT, 'src', 'tab_view.c'), 'w').write('\n'.join(TV) + '\n')
    TU = ['/* generated by tools/assets.py: do not edit */', '#pragma rodata-name(push, "BANK5R")', '#include "data.h"']
    TU.append('const unsigned char title_map[] = {' + ','.join(str(v) for v in tmap[:32 * 22]) + '};')
    TU.append('const unsigned char title_attr[] = {' + ','.join(str(v) for v in tattr) + '};')
    open(os.path.join(ROOT, 'src', 'tab_ui.c'), 'w').write('\n'.join(TU) + '\n')
    print('title tiles', ntt)
    print(f'bg tiles used by building kit: {kit_next - 0x58}/{KIT_END - 0x58}')

if __name__ == '__main__':
    import math
    main()
