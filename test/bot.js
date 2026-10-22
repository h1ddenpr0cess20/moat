import { BUILDINGS } from '../src/catalog.js';
import { CENTER, DIRS, GROUND } from '../src/config.js';
import { buildBlock, buildRoad, buildZone, placeBuilding, planAnnex, rectTiles, startAnnex, startCastle, setTrade } from '../src/sim/actions.js';
import { autoPlace } from '../src/sim/hamlet.js';
import { network, roadBlocked } from '../src/world/roads.js';
import { idx, inb } from '../src/world/world.js';

/**
 * A player that follows a sensible order of play, for tests and for tuning:
 * it lays a ring block round the castle, farms the strips beyond it, finds
 * wood and stone, builds what the town asks for, and widens the moat when it
 * can. It uses the same actions as the lord, and pays for them.
 */

const C = CENTER;

/** Builds a road from a rectangle to the nearest connected road, by the shortest free way. Returns whether it is connected after. */
export function connect(game, rect) {
  const { world } = game;
  const { dist } = network(world);
  const touching = (x, y) => DIRS.some(([dx, dy]) => inb(x + dx, y + dy) && dist[idx(x + dx, y + dy)] >= 0);
  const starts = [];
  for (let x = rect.x - 1; x <= rect.x + rect.w; x++) for (let y = rect.y - 1; y <= rect.y + rect.d; y++) {
    const inside = x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.d;
    const corner = (x < rect.x || x >= rect.x + rect.w) && (y < rect.y || y >= rect.y + rect.d);
    if (!inside && !corner && inb(x, y)) starts.push([x, y]);
  }
  if (starts.some(([x, y]) => dist[idx(x, y)] >= 0)) return true;
  const prev = new Map();
  const queue = [];
  for (const [x, y] of starts) {
    if (roadBlocked(world, x, y) && !world.road[idx(x, y)]) continue;
    prev.set(idx(x, y), -1);
    queue.push([x, y]);
  }
  for (let q = 0; q < queue.length; q++) {
    const [x, y] = queue[q];
    if (touching(x, y) || dist[idx(x, y)] >= 0) {
      const path = [];
      for (let i = idx(x, y); i !== -1; i = prev.get(i)) path.push([i % world.size, Math.floor(i / world.size)]);
      buildRoad(game, path.filter(([px, py]) => !world.road[idx(px, py)]), 'dirt');
      return true;
    }
    for (const [dx, dy] of DIRS) {
      const nx = x + dx, ny = y + dy;
      if (!inb(nx, ny) || prev.has(idx(nx, ny))) continue;
      if (roadBlocked(world, nx, ny) && !world.road[idx(nx, ny)]) continue;
      prev.set(idx(nx, ny), idx(x, y));
      queue.push([nx, ny]);
    }
  }
  return false;
}

const count = (game, type) => [...game.world.buildings.values()].filter((b) => b.type === type).length;

function put(game, type, cx, cy, radius = 10) {
  if (!BUILDINGS[type]) return null;
  const spot = autoPlace(game, type, cx, cy, radius);
  if (!spot) return null;
  const r = placeBuilding(game, type, spot.x, spot.y, spot.front);
  if (!r.building) return null;
  connect(game, r.building);
  return r.building;
}

export function createBot(game) {
  const { world } = game;
  const pop = () => game.pop.children + game.pop.adults + game.pop.elders;
  const forestAt = () => {
    let sx = 0, sy = 0, n = 0;
    for (let y = C - 12; y < C + 12; y++) for (let x = C - 12; x < C + 12; x++) { const i = idx(x, y); if (world.tree[i] && world.realm[i]) { sx += x; sy += y; n++; } }
    return n > 12 ? [sx / n, sy / n] : null;
  };
  const rockAt = () => {
    let sx = 0, sy = 0, n = 0;
    for (let y = C - 12; y < C + 12; y++) for (let x = C - 12; x < C + 12; x++) { const i = idx(x, y); if (world.ground[i] === GROUND.ROCK && world.realm[i]) { sx += x; sy += y; n++; } }
    return n > 4 ? [sx / n, sy / n] : null;
  };

  return {
    /** Does one round of thinking; call it each week. */
    think() {
      const wk = game.week;
      if (wk === 1) {
        // Wood first, while the trees stand.
        const forest = forestAt();
        if (forest) put(game, 'woodcutter', forest[0], forest[1], 10);
        // The ring road round the castle, with housing round it; the strips beyond it for the fields.
        buildBlock(game, C - 9, C - 9, C + 8, C + 8, { ring: 'res', core: 'none' });
        buildZone(game, rectTiles(C + 9, C - 12, C + 11, C + 5), 'wheat');
        buildZone(game, rectTiles(C - 12, C - 12, C - 10, C + 5), 'wheat');
        buildZone(game, rectTiles(C - 9, C - 12, C + 8, C - 10), 'veg');
        buildZone(game, rectTiles(C - 9, C + 9, C - 3, C + 11), 'market');
      }
      if (wk % 2 !== 0 || wk < 2) return;
      const s = game.stock;
      const forest = forestAt();
      if (!count(game, 'woodcutter') && forest && s.wood > 30) put(game, 'woodcutter', forest[0], forest[1], 10);
      if (count(game, 'woodcutter') === 1 && pop() > 30 && forest && s.wood > 40) put(game, 'woodcutter', forest[0], forest[1], 12);
      if (!count(game, 'farmstead') && s.wood > 50) put(game, 'farmstead', C + 10, C - 3, 4);
      if (count(game, 'farmstead') === 1 && s.wood > 70) put(game, 'farmstead', C - 11, C - 3, 4);
      if (count(game, 'farmstead') === 2 && pop() > 45 && s.wood > 70) put(game, 'farmstead', C, C - 11, 4);
      if (!count(game, 'well') && s.stone > 14) put(game, 'well', C, C + 8, 6);
      const rock = rockAt();
      if (!count(game, 'quarry') && rock && s.wood > 60 && s.gold > 40) put(game, 'quarry', rock[0], rock[1], 6);
      if (!count(game, 'mill') && count(game, 'farmstead') && s.wood > 120 && s.gold > 80) put(game, 'mill', C + 6, C - 8, 8);
      if (!count(game, 'tavern') && pop() > 24 && s.wood > 90) put(game, 'tavern', C, C + 7, 8);
      if (!count(game, 'chapel') && pop() > 34 && s.stone > 100 && s.wood > 70 && s.gold > 100) put(game, 'chapel', C - 8, C - 7, 8);
      if (!count(game, 'market') && pop() > 44 && s.stone > 45 && s.gold > 90) put(game, 'market', C - 6, C + 8, 8);
      if (!count(game, 'healer') && pop() > 55 && s.gold > 60) put(game, 'healer', C + 4, C + 8, 8);
      if (count(game, 'well') < 3 && pop() > 40 && s.stone > 20) put(game, 'well', C + (count(game, 'well') - 1) * 6, C - 8, 8);
      if (!count(game, 'storehouse') && s.wood > 250 && s.stone > 40) put(game, 'storehouse', C + 4, C + 9, 8);
      if (!count(game, 'watchtower') && game.week > 60 && s.stone > 90 && s.wood > 60 && s.gold > 80) put(game, 'watchtower', C + 8, C + 9, 8);
      if (!count(game, 'smithy') && pop() > 40 && s.iron > 20 && s.gold > 60) put(game, 'smithy', C - 7, C + 8, 8);
      if (!count(game, 'barracks') && game.rank >= 1 && s.iron > 30 && s.stone > 70 && s.gold > 140) put(game, 'barracks', C + 7, C + 8, 8);
      // Wider moat, the best-looking neighbour first.
      if (game.week > 20 && wk % 4 === 0) {
        let best = null, bestScore = -1;
        for (const w of world.wards) {
          if (w.owned || w.dig) continue;
          const plan = planAnnex(game, w.id);
          if (!plan.ok) continue;
          const score = w.stats.arable * 1.2 + w.stats.water * 0.6 + w.stats.forest * 0.3 + w.stats.ore * 3 + w.stats.rock - w.stats.hardness * 200;
          if (score > bestScore) { bestScore = score; best = w; }
        }
        if (best && s.gold > 600) startAnnex(game, best.id);
      }
      if (game.castleLevel === 0 && game.rank >= 1 && s.stone > 450 && s.gold > 650 && s.wood > 220 && s.iron > 40) startCastle(game);
      // Fish and iron once wards give them.
      for (const type of ['fishery', 'ironmine']) {
        if (count(game, type) >= 1 || s.gold < 120) continue;
        const wantWater = type === 'fishery';
        let best = null;
        for (let y = 0; y < world.size && !best; y += 3) for (let x = 0; x < world.size; x += 3) {
          const i = idx(x, y);
          if (!world.realm[i]) continue;
          if (wantWater ? world.ground[i] === GROUND.WATER : world.ground[i] === GROUND.ORE) { best = [x, y]; break; }
        }
        if (best) put(game, type, best[0], best[1], 6);
      }
      // More food whenever the barony could not feed another dozen.
      const support = game.report.foodSupport ?? 0;
      if (wk % 6 === 0 && support < pop() * 1.35 + 8 && s.gold > 90 && s.wood > 60) {
        const farms = [...world.buildings.values()].filter((b) => b.type === 'farmstead');
        let best = null, bestScore = -1;
        for (const w of world.wards) {
          if (!w.owned) continue;
          const here = farms.filter((f) => f.x >= w.gx * 30 && f.x < (w.gx + 1) * 30 && f.y >= w.gy * 30 && f.y < (w.gy + 1) * 30).length;
          const sc = w.stats.arable / (1 + here * 2);
          if (sc > bestScore) { bestScore = sc; best = w; }
        }
        const forest = forestAt();
        if (best && best.id !== 24 && game.rank >= 0) {
          const f = put(game, 'farmstead', best.gx * 30 + 15, best.gy * 30 + 15, 9);
          if (f) buildZone(game, rectTiles(f.x - 4, f.y - 4, f.x + f.w + 3, f.y + f.d + 3), 'wheat');
        } else if (forest && !count(game, 'hunter')) put(game, 'hunter', forest[0], forest[1], 10);
      }
      if (game.stock.grain > 300) setTrade(game, 'grain', { sell: 260 });
    },
  };
}
