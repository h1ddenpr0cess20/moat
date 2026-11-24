#!/usr/bin/env python3
"""Renders build/chr.bin as a contact sheet (build/chr.png) using the game's palettes, to check the art by eye."""
import os, sys
from PIL import Image
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NES = [0x666666,0x002A88,0x1412A7,0x3B00A4,0x5C007E,0x6E0040,0x6C0600,0x561D00,0x333500,0x0B4800,0x005200,0x004F08,0x00404D,0,0,0,
 0xADADAD,0x155FD9,0x4240FF,0x7527FE,0xA01ACC,0xB71E7B,0xB53120,0x994E00,0x6B6D00,0x388700,0x0C9300,0x008F32,0x007C8D,0,0,0,
 0xFFFEFF,0x64B0FF,0x9290FF,0xC676FF,0xF36AFF,0xFE6ECC,0xFE8170,0xEA9E22,0xBCBE00,0x88D800,0x5CE430,0x45E082,0x48CDDE,0x4F4F4F,0,0,
 0xFFFEFF,0xC0DFFF,0xD3D2FF,0xE8C8FF,0xFBC2FF,0xFEC4EA,0xFECCC5,0xF7D8A5,0xE4E594,0xCFEF96,0xBDF4AB,0xB3F3CC,0xB5EBF2,0xB8B8B8,0,0]
def rgb(i): return NES[i] if NES[i] else 0
sys.path.insert(0, os.path.join(ROOT, 'src'))
def load():
    d = open(os.path.join(ROOT, 'build', 'chr.bin'), 'rb').read()
    return d[:4096], d[4096:]
def tile(d, i):
    b = d[i*16:i*16+16]
    return [[((b[y] >> (7-x)) & 1) | (((b[y+8] >> (7-x)) & 1) << 1) for x in range(8)] for y in range(8)]
def sheet(d, pals, palof, scale=3, path='x.png'):
    im = Image.new('RGB', (16*8*scale, 16*8*scale), (30,30,30))
    for i in range(256):
        pal = pals[palof(i)]
        t = tile(d, i)
        for y in range(8):
            for x in range(8):
                c = rgb(pal[t[y][x]])
                for sy in range(scale):
                    for sx in range(scale):
                        im.putpixel((((i%16)*8+x)*scale+sx, ((i//16)*8+y)*scale+sy), ((c>>16)&255,(c>>8)&255,c&255))
    im.save(path)
if __name__ == '__main__':
    bg, sp = load()
    g = 0x1A
    pals = [[g,0x09,0x17,0x38],[g,0x01,0x11,0x17],[g,0x17,0x27,0x16],[g,0x0F,0x10,0x30]]
    def palof(i):
        if i < 0x0D: return 1 if i in (3,4) else 3 if i in (6,7,8) else 0
        if i == 0x30 or i == 0x31 or i == 0x32 or i == 0x33: return 1
        if i < 0x40: return 0
        if i < 0x58: return 3
        if i < 0xA0: return 2
        return 3
    sheet(bg, pals, palof, 3, os.path.join(ROOT, 'build', 'chr_bg.png'))
    spal = [[g,0x00,0x10,0x30],[g,0x0F,0x00,0x20],[g,0x0F,0x19,0x30],[g,0x0F,0x16,0x30]]
    sheet(sp, spal, lambda i: 1 if i < 0x40 else 2 if i < 0x46 else 0, 3, os.path.join(ROOT, 'build', 'chr_sp.png'))
