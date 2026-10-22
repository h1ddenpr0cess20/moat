import { CHUNK, CHUNKS } from '../config.js';
import { heightAt } from '../world/world.js';
import { Batch } from './mesher.js';
import { modelFor } from './models/index.js';
import { castleScaffold } from './models/castle.js';
import { freeTree } from './dispose.js';

/**
 * The buildings: each chunk's are stamped into a mesh a material, so a whole
 * ward draws in a handful of calls. What moves by itself (a windmill's sails)
 * is a mesh of its own, turned each frame. Where chimneys are, and where fires
 * burn, are noted for the smoke.
 */

/** The turn to put a model's front (+z) toward each way a building can face: north, east, south, west. */
export const YAW = [Math.PI, Math.PI / 2, 0, -Math.PI / 2];

const NO_SHADOW = new Set(['glow', 'glass', 'flame']);

export function createBuildings(GFX, world, materials, scene) {
  const { mat } = materials;
  const group = new GFX.Group();
  group.name = 'buildings';
  scene.add(group);
  const chunks = Array.from({ length: CHUNKS * CHUNKS }, (_, i) => ({ i, group: null, anims: [], smoke: [], lights: [] }));
  const scaffold = { group: null, parts: castleScaffold() };

  const meshesFor = (batch, parent) => {
    for (const [name, acc] of batch.accs) {
      const geometry = acc.geometry(GFX);
      if (!geometry) continue;
      const material = mat[name];
      if (!material) { console.warn('moat: no material', name); continue; }
      const mesh = new GFX.Mesh(geometry, material);
      mesh.castShadow = !NO_SHADOW.has(name);
      mesh.receiveShadow = true;
      if (name === 'glow' || name === 'flame') mesh.renderOrder = 3;
      parent.add(mesh);
    }
  };

  const clear = (c) => {
    if (c.group) { group.remove(c.group); freeTree(c.group); c.group = null; }
    c.anims = []; c.smoke = []; c.lights = [];
  };

  const build = (i) => {
    const c = chunks[i];
    clear(c);
    const cx = i % CHUNKS, cy = Math.floor(i / CHUNKS);
    const batch = new Batch();
    const g = new GFX.Group();
    g.name = `buildings ${cx},${cy}`;
    for (const b of world.buildings.values()) {
      const mx = b.x + b.w / 2, mz = b.y + b.d / 2;
      if (Math.floor(mx / CHUNK) !== cx || Math.floor(mz / CHUNK) !== cy) continue;
      const building = b.state === 'building';
      const t = modelFor(b.type, { level: b.level, variant: b.variant, building, progress: b.progress });
      const baseY = heightAt(world, mx, mz);
      const yaw = YAW[b.front] ?? 0;
      batch.stamp(t.parts, mx, baseY, mz, yaw);
      const sy = Math.sin(yaw), cs = Math.cos(yaw);
      const at = (p) => [mx + p[0] * cs + p[2] * sy, baseY + p[1], mz - p[0] * sy + p[2] * cs];
      if (!building) {
        for (const s of t.smoke) c.smoke.push({ at: at(s), id: b.id, type: b.type });
        for (const l of t.lights) c.lights.push({ at: at(l.at), color: l.color, reach: l.reach, id: b.id });
        for (const a of t.anim) {
          const pivot = new GFX.Group();
          const holder = new GFX.Group();
          holder.position.set(mx, baseY, mz);
          holder.rotation.y = yaw;
          pivot.position.set(...a.pivot);
          holder.add(pivot);
          const sub = new Batch();
          sub.stamp(a.parts, 0, 0, 0, 0);
          meshesFor(sub, pivot);
          g.add(holder);
          c.anims.push({ pivot, rate: a.rate, kind: a.kind, phase: (b.id * 1.37) % 6.28 });
        }
      }
    }
    meshesFor(batch, g);
    group.add(g);
    c.group = g;
  };

  return {
    group,
    chunks,
    sync(budget = 2) {
      let n = 0;
      for (const i of [...world.dirty.buildings]) {
        world.dirty.buildings.delete(i);
        build(i);
        if (++n >= budget) return;
      }
    },
    buildAll() { for (let i = 0; i < chunks.length; i++) build(i); world.dirty.buildings.clear(); },
    /** Turns the sails, once a frame. */
    update(dt, wind = 1) {
      for (const c of chunks) for (const a of c.anims) if (a.kind === 'spin') a.pivot.rotation.z += a.rate * dt * wind;
    },
    /** Every chimney and fire worth a puff of smoke. */
    emitters() {
      return chunks.flatMap((c) => c.smoke);
    },
    lights() {
      return chunks.flatMap((c) => c.lights);
    },
    /** The scaffolding round the castle while masons work. */
    setCastleWork(on, y) {
      if (on && !scaffold.group) {
        const batch = new Batch();
        batch.stamp(scaffold.parts, 0, 0, 0, 0);
        const g = new GFX.Group();
        meshesFor(batch, g);
        g.position.set(world.size / 2, y, world.size / 2);
        scene.add(g);
        scaffold.group = g;
      } else if (!on && scaffold.group) {
        scene.remove(scaffold.group);
        freeTree(scaffold.group);
        scaffold.group = null;
      }
    },
  };
}
