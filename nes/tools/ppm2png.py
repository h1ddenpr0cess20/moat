#!/usr/bin/env python3
"""Converts PPM screenshots (from shot.mjs) to PNGs, upscaled 2x."""
import sys, os
from PIL import Image
for p in sys.argv[1:]:
    im = Image.open(p)
    im = im.resize((im.width * 2, im.height * 2), Image.NEAREST)
    im.save(os.path.splitext(p)[0] + '.png')
