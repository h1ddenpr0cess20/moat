#!/bin/sh
# Builds moat.nes. Needs cc65, python3 and node (the catalog is read from ../src/catalog.js).
set -e
cd "$(dirname "$0")"
mkdir -p build
node tools/dump_catalog.mjs > build/catalog.json
python3 tools/assets.py
ca65 -g -o build/crt0.o src/crt0.s
objs="build/crt0.o"
for f in ppu world core sim_a sim_b1 sim_b2 sim_c gen view ui tables tab_a tab_view tab_ui; do
  cc65 -Oi -t none $CFLAGS -I src -o build/$f.s src/$f.c
  ca65 -g -o build/$f.o build/$f.s
  objs="$objs build/$f.o"
done
ld65 -C moat.cfg -o moat.nes $objs /usr/share/cc65/lib/none.lib -Ln build/moat.lbl -m build/moat.map
echo "built $(pwd)/moat.nes ($(wc -c < moat.nes) bytes)"
