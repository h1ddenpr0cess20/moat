import { ZONE_ID } from '../catalog.js';
import { CENTER, DIRS } from '../config.js';
import { check, addBuilding, frontFor } from '../world/buildings.js';
import { findLot, zoneBlocked } from '../world/zoning.js';
import { idx, inb } from '../world/world.js';
import { buildRoad, buildZone, placeBuilding, rectTiles } from './actions.js';

/**
 * The lord does not start with nothing: an avenue from the castle's
 * drawbridge to the south gate, a cross street, ground zoned for houses along
 * both, three cottages, a granary and a well. A town to grow from.
 */

/** Looks outward from a spot for somewhere a building can stand (nearest first), with a road beside it if one can be had. */
export function autoPlace(game, type, cx, cy, radius = 12) {
  cx = Math.round(cx);
  cy = Math.round(cy);
  let best = null, bestD = Infinity;
  for (let r = 0; r <= radius; r++) {
    for (let dy = -r; dy <= r; dy++) {
      for (let dx = -r; dx <= r; dx++) {
        if (Math.max(Math.abs(dx), Math.abs(dy)) !== r) continue;
        const x = cx + dx, y = cy + dy;
        for (let front = 0; front < 4; front++) {
          const c = check(game.world, type, x, y, front);
          if (!c.ok) continue;
          const f = frontFor(game.world, c.rect.x, c.rect.y, c.rect.w, c.rect.d);
          if (f !== front) continue;
          const d = Math.hypot(dx, dy);
          if (d < bestD) { bestD = d; best = { x, y, front }; }
        }
      }
    }
    if (best) return best;
  }
  return null;
}

export function seedHamlet(game) {
  const C = CENTER;
  const free = { free: true };
  buildRoad(game, rectTiles(C - 1, C + 6, C, C + 11), 'dirt', free);
  buildRoad(game, rectTiles(C - 11, C + 8, C + 10, C + 8), 'dirt', free);
  // Housing along both streets, leaving the woods standing.
  for (const [x0, y0, x1, y1] of [[C - 11, C + 9, C - 2, C + 11], [C + 1, C + 9, C + 10, C + 11], [C - 11, C + 6, C - 2, C + 7], [C + 1, C + 6, C + 10, C + 7]]) {
    buildZone(game, rectTiles(x0, y0, x1, y1).filter(([x, y]) => !game.world.tree[idx(x, y)]), 'res', free);
  }
  // The granary and the well first, then the cottages in what is left.
  const gran = autoPlace(game, 'granary', C + 3, C + 10, 6);
  if (gran) placeBuilding(game, 'granary', gran.x, gran.y, gran.front, free);
  const well = autoPlace(game, 'well', C - 3, C + 7, 6);
  if (well) placeBuilding(game, 'well', well.x, well.y, well.front, free);
  for (let n = 0; n < 3; n++) {
    const lot = findLot(game.world, 'res', [2, 2], game.rnd, (x, y) => -Math.hypot(x - C, y - (C + 8)) * 0.1);
    if (!lot) break;
    addBuilding(game.world, 'cottage', lot.x, lot.y, lot.front, { state: 'ready', rect: lot });
  }
  // A farm that is already working, and a woodcutter, so the first harvest is a real one.
  const farm = autoPlace(game, 'farmstead', C + 9, C + 9, 8);
  if (farm) {
    const r = placeBuilding(game, 'farmstead', farm.x, farm.y, farm.front, free);
    if (r.building) fieldsAround(game, r.building, 26);
  }
  let sx = 0, sy = 0, n = 0;
  for (let y = C - 12; y < C + 12; y++) for (let x = C - 12; x < C + 12; x++) { const i = idx(x, y); if (game.world.tree[i] && game.world.realm[i]) { sx += x; sy += y; n++; } }
  if (n > 8) {
    const wood = autoPlace(game, 'woodcutter', sx / n, sy / n, 10);
    if (wood) placeBuilding(game, 'woodcutter', wood.x, wood.y, wood.front, free);
  }
  game.weather.harvest = 1;
  game.world.bver = (game.world.bver ?? 0) + 1;
}

/** Zones `count` tiles of wheat in a clump round a farmstead: the nearest free, treeless ground first. */
export function fieldsAround(game, farm, count = 24, zone = 'wheat') {
  const { world } = game;
  const cx = farm.x + farm.w / 2, cy = farm.y + farm.d / 2;
  const chosen = [];
  const seen = new Set();
  const queue = [];
  for (let x = farm.x - 1; x <= farm.x + farm.w; x++) for (let y = farm.y - 1; y <= farm.y + farm.d; y++) queue.push([x, y]);
  while (queue.length && chosen.length < count) {
    // Open ground first, then ground that has to be cleared of trees.
    const cost = ([x, y]) => Math.hypot(x + 0.5 - cx, y + 0.5 - cy) + (inb(x, y) && world.tree[idx(x, y)] ? 3 : 0);
    queue.sort((a, b) => cost(a) - cost(b));
    const [x, y] = queue.shift();
    const key = idx(x, y);
    if (!inb(x, y) || seen.has(key)) continue;
    seen.add(key);
    if (Math.hypot(x + 0.5 - cx, y + 0.5 - cy) > 7.5) continue;
    if (zoneBlocked(world, x, y) || (world.zone[key] && world.zone[key] !== ZONE_ID.res)) continue;
    chosen.push([x, y]);
    for (const [dx, dy] of DIRS) queue.push([x + dx, y + dy]);
  }
  buildZone(game, chosen, zone, { free: true });
  return chosen.length;
}
