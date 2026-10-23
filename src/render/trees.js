import { CHUNK, CHUNKS, GROUND, WARDS } from '../config.js';
import { ZONE_ID } from '../catalog.js';
import { hash2 } from '../rng.js';
import { HV, SUB, heightAt, idx } from '../world/world.js';
import { Batch, Mesher } from 'madness-engine/mesher';
import { freeTree } from './dispose.js';

/**
 * The forests and what grows in them: oaks, birches and pines standing in the
 * woodland tiles, boulders on the outcrops, fruit trees in the orchards. Each
 * chunk's trees are stamped into a few meshes; far from the camera a cheaper
 * set of the same trees is shown instead. Leaves are materials the seasons
 * colour, so a tree is built once and is green in summer and bare in winter.
 */

/** The share of tiles that keep a tree in the far wood: less on a bigger map, which has more of it to hold. */
const FAR_KEEP = Math.min(0.72, 0.72 * Math.pow(7 / WARDS, 0.8));

const LEAF = ['leafA', 'leafB', 'leafC'];
const TRUNK = ['#6a4a2c', '#5a3e26', '#74532f'];

/** A colour part of the way from one hex colour to another. */
function mix(a, b, t) {
  const p = (h) => [1, 3, 5].map((i) => parseInt(h.slice(i, i + 2), 16));
  const A = p(a), B = p(b);
  return '#' + A.map((v, i) => Math.round(v + (B[i] - v) * t).toString(16).padStart(2, '0')).join('');
}

/** A canopy blob in near-white (the leaf material colours it), brighter at the top. */
function blob(m, x, y, z, rx, ry, rz, mat, detail, seed) {
  m.sphere(mat, x, y, z, rx, ry, rz, '#e4e4e4', { detail, jitter: 0.32, light: 0.35, seed });
}

/** The trees, made once: `{ lod0, lod1 }` for each kind and variant, each a template of parts by material. */
export function buildTemplates() {
  const out = { oak: [], birch: [], pine: [], fruit: {}, boulder: [], ore: [] };
  const make = (fn) => {
    const hi = new Mesher(), lo = new Mesher();
    fn(hi, true);
    fn(lo, false);
    return { lod0: hi.finish(), lod1: lo.finish() };
  };
  for (let v = 0; v < 6; v++) {
    const leaf = LEAF[v % 3];
    out.oak.push(make((m, hi) => {
      const rnd = (k) => hash2(v * 13 + k, k * 7, 3);
      const trunk = TRUNK[v % 3];
      m.cylinder('bark', 0, -0.05, 0, 0.045, 0.075, 0.62, hi ? 7 : 5, trunk, { smooth: hi });
      if (hi) {
        for (let b = 0; b < 3; b++) {
          const a = b * 2.1 + rnd(b) * 1.5;
          m.beam('bark', [0, 0.5, 0], [Math.cos(a) * 0.22, 0.88 + rnd(b + 3) * 0.1, Math.sin(a) * 0.22], 0.035, trunk);
        }
      }
      const R = 0.36 + rnd(1) * 0.1;
      blob(m, 0, 0.92, 0, R, R * 0.85, R, leaf, hi ? 1 : 0, v);
      blob(m, 0.2 * (rnd(2) - 0.5) * 2, 1.12, 0.18 * (rnd(3) - 0.5) * 2, R * 0.8, R * 0.7, R * 0.8, leaf, hi ? 1 : 0, v + 3);
      // One more lobe at the side is enough: each lobe is eighty triangles, and a wood has thousands of trees.
      if (hi) blob(m, -0.22, 0.84, 0.12, R * 0.66, R * 0.58, R * 0.66, leaf, 1, v + 5);
    }));
  }
  for (let v = 0; v < 3; v++) {
    out.birch.push(make((m, hi) => {
      m.cylinder('bark', 0, -0.05, 0, 0.03, 0.045, 0.95, hi ? 6 : 4, '#dcd8cc', { smooth: false });
      if (hi) for (let k = 0; k < 6; k++) m.box('plain', -0.03, 0.15 + k * 0.13, -0.002, 0.03, 0.17 + k * 0.13, 0.04, '#3a3630', { skip: ['bottom'] });
      blob(m, 0, 1.12, 0, 0.26, 0.32, 0.26, 'leafB', hi ? 1 : 0, v);
      blob(m, 0.1, 0.84, 0.05, 0.2, 0.22, 0.2, 'leafB', hi ? 1 : 0, v + 2);
      if (hi) blob(m, -0.1, 0.78, -0.05, 0.18, 0.2, 0.18, 'leafB', 1, v + 4);
    }));
  }
  for (let v = 0; v < 4; v++) {
    out.pine.push(make((m, hi) => {
      const tiers = hi ? 5 : 3;
      m.cylinder('bark', 0, -0.05, 0, 0.04, 0.06, 0.4, hi ? 6 : 4, '#5a3e26', { smooth: false });
      for (let t = 0; t < tiers; t++) {
        const f = t / (tiers - 1);
        const r = (0.46 - f * 0.28) * (0.9 + 0.2 * hash2(v, t, 5));
        const y = 0.3 + f * (hi ? 0.95 : 0.85);
        m.cylinder('needle', 0, y, 0, 0.01, r, hi ? 0.42 : 0.55, hi ? 8 : 5, '#d4d4d4', { smooth: false, caps: false });
        if (hi && t < tiers - 1) m.cylinder('needle', 0, y - 0.03, 0, r * 0.8, r, 0.05, 8, '#9a9a9a', { smooth: false, caps: true });
      }
    }));
  }
  for (const [name, season] of [['spring', '#f6d8e8'], ['summer', null], ['autumn', '#d8402a'], ['winter', null]]) {
    out.fruit[name] = [0, 1].map((v) => make((m, hi) => {
      m.cylinder('bark', 0, -0.05, 0, 0.035, 0.055, 0.34, hi ? 6 : 4, '#5a3e26', { smooth: false });
      blob(m, 0, 0.5, 0, 0.3, 0.26, 0.3, v ? 'leafB' : 'leafA', hi ? 1 : 0, v + 9);
      if (hi && season) for (let k = 0; k < 9; k++) {
        const a = k * 2.4 + v, d = 0.18 + 0.1 * hash2(k, v, 4);
        m.sphere('plain', Math.cos(a) * d, 0.42 + 0.2 * hash2(k, 3, v), Math.sin(a) * d, 0.026, 0.026, 0.026, season, { detail: 0 });
      }
    }));
  }
  // Outcrops: a heap of knocked-about rock, a big one with smaller ones and chips against its foot, each variant its own colour and shape.
  const GREY = [['#7a756b', '#6a655d', '#8a8478'], ['#8a8478', '#756f66', '#7f796c'], ['#6a655d', '#5c5851', '#7a756b'], ['#7f796c', '#8c8676', '#6e695f']];
  const DARK = [['#5e5a52', '#4e4a44', '#6c675e'], ['#6c675e', '#58544c', '#625c54'], ['#4e4a44', '#5e5a52', '#403c38'], ['#625c54', '#6c675e', '#54504a']];
  for (let v = 0; v < 4; v++) {
    out.boulder.push(make((m, hi) => {
      const c = GREY[v], d = hi ? 1 : 0, D = hi ? 2 : 1;
      m.rock('rock', 0, 0.1, 0, 0.3 + 0.05 * v, 0.2 + 0.03 * v, 0.26 + 0.04 * v, c[0], { detail: D, rough: 0.3, seed: v, cuts: 5 });
      m.rock('rock', 0.27 + 0.03 * v, 0.04, 0.12, 0.15, 0.11, 0.13, c[1], { detail: d, rough: 0.28, seed: v + 4, cuts: 3 });
      m.rock('rock', -0.2, 0.03, -0.24 + 0.04 * v, 0.12, 0.09, 0.11, c[2], { detail: d, rough: 0.28, seed: v + 8, cuts: 3 });
      if (hi) m.rock('rock', 0.1, 0.01, 0.3, 0.06, 0.04, 0.05, c[0], { detail: 0, rough: 0.2, seed: v + 12, cuts: 2 });
    }));
    out.ore.push(make((m, hi) => {
      const c = DARK[v], d = hi ? 1 : 0, D = hi ? 2 : 1;
      // Iron shows as rust-brown seams and stains through dark rock, not as lumps stuck on it.
      const seams = (dir, lump, grain) => {
        const s = Math.abs(Math.sin((dir[0] * 3.1 + dir[1] * 5.3 - dir[2] * 2.2 + v) * 2.6 + lump * 4));
        const w = Math.max(0, 1 - s * 7) * 0.9 + Math.max(0, grain - 0.72) * 2;
        return w > 0.08 ? { col: '#8a4a2e', w: Math.min(1, w) } : null;
      };
      const tint = (dir, lump, grain) => { const t = seams(dir, lump, grain); return t ? mix(c[0], t.col, t.w) : c[0]; };
      m.rock('rock', 0, 0.1, 0, 0.27 + 0.04 * v, 0.2 + 0.03 * v, 0.23 + 0.04 * v, c[0], { detail: D, rough: 0.28, seed: v + 20, cuts: 5, lichen: 0.15, colorFn: tint });
      m.rock('rock', -0.24, 0.03, 0.1, 0.13, 0.1, 0.12, c[1], { detail: d, rough: 0.26, seed: v + 24, cuts: 3, lichen: 0.15, colorFn: (dir, l, g) => { const t = seams(dir, l, g); return t ? mix(c[1], t.col, t.w) : c[1]; } });
      for (let k = 0; k < 3; k++) {
        const a = k * 2.3 + v, r = 0.2 + 0.03 * hash2(k, v, 5);
        m.rock('rock', Math.cos(a) * r, 0.05 + 0.03 * k, Math.sin(a) * r * 0.9, 0.07, 0.05, 0.06, k % 2 ? '#7a4630' : '#69392a', { detail: 0, rough: 0.18, seed: k + v * 5, cuts: 3, base: 0.3 });
      }
    }));
  }
  return out;
}

export function createTrees(GFX, world, materials, scene) {
  const { mat } = materials;
  const T = buildTemplates();
  const group = new GFX.Group();
  group.name = 'trees';
  scene.add(group);
  const chunks = Array.from({ length: CHUNKS * CHUNKS }, (_, i) => ({ i, lod0: null, lod1: null, built: false, near: false, season: -1, orchard: false }));
  const world_ = world;

  const stampTile = (b0, b1, x, y, orchard, season) => {
    const i = idx(x, y);
    const g = world_.ground[i];
    if (world_.tree[i] > 0) {
      const n = world_.tree[i];
      const alt = world_.h[(y * SUB + 1) * HV + x * SUB + 1];
      for (let k = 0; k < n; k++) {
        const fx = 0.12 + 0.76 * hash2(x * 3 + k, y, 11), fz = 0.12 + 0.76 * hash2(y * 3 + k, x, 13);
        const wx = x + fx, wz = y + fz;
        const wy = heightAt(world_, wx, wz) - 0.02;
        const r = hash2(x, y * 5 + k, 17);
        const s = 0.82 + 0.5 * hash2(x + k, y, 19) + (n - 1) * 0.06;
        const yaw = hash2(x, y, 23 + k) * 6.28;
        let kind;
        if (alt > 3.4 || r < 0.2) kind = T.pine[(x + y + k) % 4];
        else if (r < 0.34) kind = T.birch[(x * 3 + y + k) % 3];
        else kind = T.oak[(x * 7 + y * 3 + k) % 6];
        const tint = [0.9 + 0.2 * hash2(x, y + k, 29), 0.9 + 0.2 * hash2(x + 1, y, 31), 0.9 + 0.2 * hash2(x, y, 37 + k)];
        b0?.stamp(kind.lod0, wx, wy, wz, yaw, s, tint);
        // Far off, a wood is drawn thin and a little larger: about a tree to a tile, and a few tiles bare (the ground is painted as forest floor).
        if (k < (n > 2 ? 2 : 1) && hash2(x, y, 77) < FAR_KEEP) b1.stamp(kind.lod1, wx, wy, wz, yaw, s * 1.22, tint);
      }
    } else if (g === GROUND.ROCK || g === GROUND.ORE) {
      const list = g === GROUND.ORE ? T.ore : T.boulder;
      const count = 1 + (hash2(x, y, 41) < 0.5 ? 1 : 0);
      for (let k = 0; k < count; k++) {
        const wx = x + 0.2 + 0.6 * hash2(x, y + k, 43), wz = y + 0.2 + 0.6 * hash2(y, x + k, 47);
        const wy = heightAt(world_, wx, wz) - 0.04;
        const t = list[(x * 3 + y + k) % list.length];
        const s = 0.9 + 0.5 * hash2(x, y, 53 + k);
        b0?.stamp(t.lod0, wx, wy, wz, hash2(x, y, 59) * 6.28, s);
        b1.stamp(t.lod1, wx, wy, wz, hash2(x, y, 59) * 6.28, s);
      }
    } else if (orchard) {
      const wx = x + 0.5 + (hash2(x, y, 61) - 0.5) * 0.2, wz = y + 0.5 + (hash2(y, x, 67) - 0.5) * 0.2;
      const wy = heightAt(world_, wx, wz);
      const t = T.fruit[['spring', 'summer', 'autumn', 'winter'][season]][(x + y) % 2];
      b0?.stamp(t.lod0, wx, wy, wz, hash2(x, y, 71) * 6.28, 0.95 + 0.2 * hash2(x, y, 73));
      b1.stamp(t.lod1, wx, wy, wz, hash2(x, y, 71) * 6.28, 0.95 + 0.2 * hash2(x, y, 73));
    }
  };

  const disposeOf = (c) => { for (const k of ['lod0', 'lod1']) if (c[k]) { group.remove(c[k]); freeTree(c[k]); c[k] = null; } };

  const build = (i, season) => {
    const cx = i % CHUNKS, cy = Math.floor(i / CHUNKS);
    const c = chunks[i];
    disposeOf(c);
    // The full trees are only made for chunks near the camera (they are most of the memory); the light ones are always there.
    const b0 = c.near ? new Batch() : null, b1 = new Batch();
    let orchard = false;
    for (let y = cy * CHUNK; y < (cy + 1) * CHUNK; y++) {
      for (let x = cx * CHUNK; x < (cx + 1) * CHUNK; x++) {
        const t = idx(x, y);
        const isOrchard = world_.zone[t] === ZONE_ID.orchard && !world_.bld[t];
        if (isOrchard) orchard = true;
        if (world_.tree[t] > 0 || world_.ground[t] === GROUND.ROCK || world_.ground[t] === GROUND.ORE || isOrchard) stampTile(b0, b1, x, y, isOrchard, season);
      }
    }
    const wrap = (batch, shadows) => {
      if (!batch) return null;
      const g = new GFX.Group();
      g.name = 'trees';
      for (const [name, acc] of batch.accs) {
        const geometry = acc.geometry(GFX);
        if (!geometry || !mat[name]) continue;
        const mesh = new GFX.Mesh(geometry, mat[name]);
        mesh.castShadow = shadows;
        mesh.receiveShadow = true;
        g.add(mesh);
      }
      group.add(g);
      return g;
    };
    // Trees take shadows but do not throw them. Near trees threw them and far ones could not, so a straight line of shade ran along every chunk border; the forest floor is painted dark under the woods instead.
    c.lod0 = wrap(b0, false);
    c.lod1 = wrap(b1, false);
    if (c.lod0) c.lod0.visible = true;
    c.lod1.visible = !c.near;
    c.built = true;
    c.season = season;
    c.orchard = orchard;
  };

  return {
    group,
    /** Builds what has changed. `season` colours the orchards' blossom and fruit. */
    sync(season, budget = 1) {
      let n = 0;
      for (const i of [...world_.dirty.trees]) {
        world_.dirty.trees.delete(i);
        build(i, season);
        if (++n >= budget) return;
      }
      for (const c of chunks) {
        if (c.built && c.orchard && c.season !== season) { build(c.i, season); if (++n >= budget) return; }
      }
    },
    /** Builds every chunk that has not been, the nearest first. */
    async buildAll(season, near, progress = () => {}) {
      const order = chunks.map((c) => c.i).sort((a, b) => Math.hypot(((a % CHUNKS) + 0.5) * CHUNK - near[0], (Math.floor(a / CHUNKS) + 0.5) * CHUNK - near[1]) - Math.hypot(((b % CHUNKS) + 0.5) * CHUNK - near[0], (Math.floor(b / CHUNKS) + 0.5) * CHUNK - near[1]));
      let n = 0;
      for (const i of order) {
        build(i, season);
        world_.dirty.trees.delete(i);
        progress(++n / order.length);
        if (n % 3 === 0) await new Promise((r) => setTimeout(r, 0));
      }
    },
    /** Makes the full trees for the chunks near the camera and drops them for those far off; at most `budget` chunks a call. */
    updateLod(cx, cz, near = 45, budget = 2) {
      let spent = 0;
      for (const c of chunks) {
        if (!c.built || spent >= budget) continue;
        const x = ((c.i % CHUNKS) + 0.5) * CHUNK, z = (Math.floor(c.i / CHUNKS) + 0.5) * CHUNK;
        const d = Math.hypot(x - cx, z - cz);
        const want = c.near ? d < near + 14 : d < near - 6;
        if (want !== c.near) {
          c.near = want;
          build(c.i, c.season);
          spent++;
        }
      }
    },
  };
}
