import { ZONES, ZONE_ID } from '../catalog.js';
import { CHUNK, CHUNKS, ROAD } from '../config.js';
import { HV, SUB, idx, inb } from '../world/world.js';
import { rgb } from './mesher.js';
import { freeGeometry } from './dispose.js';

/**
 * What lies on the ground: roads (autotiled from an atlas, so every bend and
 * junction joins its neighbours), fields (which sow, grow, ripen and are cut
 * as the year goes), kitchen gardens, and the faint colour of land set aside
 * for houses and shops. All are thin quads that follow the ground.
 */

const CELL = 64;
const LIFT = 0.03;

function surface(w, h) {
  const c = document.createElement('canvas');
  c.width = w; c.height = h;
  return [c, c.getContext('2d')];
}

const seeded = (seed) => { let s = seed; return () => { s = (s * 16807) % 2147483647; return s / 2147483647; }; };

/** The roads: 16 shapes (which of N, E, S, W it joins) by 4 variants by 2 kinds. */
function paintRoads(cobble) {
  const [atlas, ctx] = surface(CELL * 16, CELL * 8);
  const m = 8, r = 12;
  const rnd = seeded(77);
  for (let type = 0; type < 2; type++) {
    for (let variant = 0; variant < 4; variant++) {
      for (let mask = 0; mask < 16; mask++) {
        const ox = mask * CELL, oy = (type * 4 + variant) * CELL;
        const path = () => {
          ctx.beginPath();
          ctx.roundRect(ox + m, oy + m, CELL - 2 * m, CELL - 2 * m, r);
          if (mask & 1) ctx.rect(ox + m, oy, CELL - 2 * m, m + r);
          if (mask & 2) ctx.rect(ox + CELL - m - r, oy + m, m + r, CELL - 2 * m);
          if (mask & 4) ctx.rect(ox + m, oy + CELL - m - r, CELL - 2 * m, m + r);
          if (mask & 8) ctx.rect(ox, oy + m, m + r, CELL - 2 * m);
        };
        // The worn verge: a soft dark edge round the road.
        ctx.save();
        ctx.beginPath(); ctx.rect(ox, oy, CELL, CELL); ctx.clip();
        if ('filter' in ctx) ctx.filter = 'blur(2px)';
        path();
        ctx.lineWidth = 5; ctx.strokeStyle = 'rgba(48,38,26,0.55)'; ctx.stroke();
        ctx.fillStyle = 'rgba(48,38,26,0.55)'; ctx.fill();
        ctx.restore();
        ctx.save();
        path();
        ctx.clip();
        if (type === 0) {
          // Beaten earth: ruts, speckle, a trodden middle.
          ctx.fillStyle = '#a58959'; ctx.fillRect(ox, oy, CELL, CELL);
          for (let k = 0; k < 150; k++) { ctx.fillStyle = rnd() < 0.5 ? 'rgba(80,62,40,0.28)' : 'rgba(210,190,150,0.3)'; ctx.fillRect(ox + rnd() * CELL, oy + rnd() * CELL, 1 + rnd() * 2, 1 + rnd()); }
          ctx.strokeStyle = 'rgba(80,60,38,0.4)'; ctx.lineWidth = 3;
          for (const f of [0.38, 0.62]) {
            if (mask & 5 || !(mask & 10)) { ctx.beginPath(); ctx.moveTo(ox + f * CELL, oy + (mask & 1 ? 0 : m + 2)); ctx.lineTo(ox + f * CELL, oy + CELL - (mask & 4 ? 0 : m + 2)); ctx.stroke(); }
            if (mask & 10) { ctx.beginPath(); ctx.moveTo(ox + (mask & 8 ? 0 : m + 2), oy + f * CELL); ctx.lineTo(ox + CELL - (mask & 2 ? 0 : m + 2), oy + f * CELL); ctx.stroke(); }
          }
        } else {
          // Setts laid in rows, a little different each.
          const sx = (variant % 2) * 128, sy = (variant >> 1) * 128;
          if (cobble) ctx.drawImage(cobble, sx, sy, 128, 128, ox, oy, CELL, CELL);
          else { ctx.fillStyle = '#9a8e7c'; ctx.fillRect(ox, oy, CELL, CELL); }
          ctx.globalCompositeOperation = 'multiply';
          ctx.fillStyle = '#b6a48a'; ctx.fillRect(ox, oy, CELL, CELL);
          ctx.globalCompositeOperation = 'source-over';
          ctx.fillStyle = 'rgba(40,30,20,0.18)';
          ctx.fillRect(ox, oy, CELL, 1.5); ctx.fillRect(ox, oy + CELL - 1.5, CELL, 1.5);
        }
        ctx.restore();
      }
    }
  }
  return atlas;
}

const STAGES = 6;

/** The fields: for each crop and each stage of its year, with the rows running either way. */
function paintFields() {
  const [atlas, ctx] = surface(CELL * 8, CELL * 8);
  const rnd = seeded(91);
  const S = CELL;
  for (let crop = 0; crop < 4; crop++) {
    for (let orient = 0; orient < 2; orient++) {
      for (let stage = 0; stage < STAGES; stage++) {
        const ox = stage * S, oy = (crop * 2 + orient) * S;
        ctx.save();
        ctx.beginPath(); ctx.rect(ox, oy, S, S); ctx.clip();
        ctx.translate(ox + S / 2, oy + S / 2);
        if (orient) ctx.rotate(Math.PI / 2);
        ctx.translate(-S / 2, -S / 2);
        const frost = stage === 5;
        const soil = frost ? '#7a6a5a' : '#6e5034';
        const rows = 12, sp = S / rows;
        const plant = (cols, w) => { for (let r = 0; r < rows; r++) { ctx.strokeStyle = cols[0]; ctx.lineWidth = w; ctx.beginPath(); ctx.moveTo(0, r * sp + sp * 0.3); ctx.lineTo(S, r * sp + sp * 0.3); ctx.stroke(); ctx.strokeStyle = cols[1]; ctx.lineWidth = w * 0.4; ctx.beginPath(); ctx.moveTo(0, r * sp + sp * 0.3 - w * 0.2); ctx.lineTo(S, r * sp + sp * 0.3 - w * 0.2); ctx.stroke(); } };
        if (crop === 0 || crop === 1) {
          ctx.fillStyle = soil; ctx.fillRect(0, 0, S, S);
          // Ploughed earth: dark troughs, pale ridges, and clods.
          for (let r = 0; r < rows; r++) {
            ctx.fillStyle = frost ? '#6e6052' : '#523a24'; ctx.fillRect(0, r * sp + sp * 0.62, S, sp * 0.38);
            ctx.fillStyle = frost ? '#9a8a7a' : '#86663f'; ctx.fillRect(0, r * sp + sp * 0.18, S, sp * 0.22);
          }
          for (let k = 0; k < 140; k++) { ctx.fillStyle = rnd() < 0.5 ? 'rgba(40,26,14,0.3)' : 'rgba(180,150,100,0.26)'; ctx.fillRect(rnd() * S, rnd() * S, 1.6, 1.2); }
          if (crop === 0) {
            if (stage === 1) for (let r = 0; r < rows; r++) for (let k = 0; k < 12; k++) { ctx.fillStyle = '#86b84e'; ctx.fillRect(k * 5.4 + (r % 2) * 2.5, r * sp + sp * 0.3, 1.6, 2.4); }
            if (stage === 2) plant(['#5e9a34', '#84c04e'], 4);
            if (stage === 3) { plant(['#cfa63e', '#ecd26e'], 5.4); for (let k = 0; k < 90; k++) { ctx.fillStyle = 'rgba(240,215,120,0.6)'; ctx.fillRect(rnd() * S, rnd() * S, 1.4, 1.4); } }
            if (stage === 4) { ctx.strokeStyle = '#c4a766'; ctx.lineWidth = 1.6; for (let r = 0; r < rows; r++) { ctx.setLineDash([4, 3]); ctx.beginPath(); ctx.moveTo(0, r * sp + sp * 0.3); ctx.lineTo(S, r * sp + sp * 0.3); ctx.stroke(); } ctx.setLineDash([]); }
          } else {
            // Vegetables: round heads in rows.
            const heads = stage === 0 ? 0 : stage === 1 ? 0.5 : stage === 2 ? 0.8 : stage === 3 ? 1 : 0.35;
            if (stage < 5) for (let r = 0; r < rows; r++) for (let k = 0; k < 7; k++) {
              const x = k * 9.4 + 4 + (r % 2) * 3, y = r * sp + sp * 0.3;
              ctx.fillStyle = stage === 3 ? (rnd() < 0.2 ? '#a64a6a' : '#8ab84e') : '#6aa23a';
              ctx.beginPath(); ctx.arc(x, y, 1.2 + 2.6 * heads, 0, 7); ctx.fill();
              ctx.fillStyle = 'rgba(210,235,150,0.5)'; ctx.beginPath(); ctx.arc(x - 0.8, y - 0.8, 0.9 * heads + 0.3, 0, 7); ctx.fill();
            }
          }
        } else if (crop === 2) {
          // Pasture: grass, greener as the year ripens, with clumps and flowers.
          const g = ['#7a9a46', '#7fa84a', '#88b650', '#7aa84a', '#a49a56', '#9aa48a'][stage];
          ctx.fillStyle = g; ctx.fillRect(0, 0, S, S);
          for (let k = 0; k < 170; k++) { ctx.fillStyle = rnd() < 0.5 ? 'rgba(40,70,20,0.28)' : 'rgba(190,220,120,0.3)'; ctx.fillRect(rnd() * S, rnd() * S, 1.5, 2.2); }
          if (stage >= 1 && stage <= 3) for (let k = 0; k < 7; k++) { ctx.fillStyle = ['#f4ecc8', '#e8c84a', '#d88aa8'][k % 3]; ctx.fillRect(rnd() * S, rnd() * S, 1.6, 1.6); }
        } else {
          // Kitchen garden: raised beds with paths between.
          ctx.fillStyle = frost ? '#9a8a78' : '#8a7458'; ctx.fillRect(0, 0, S, S);
          for (let b = 0; b < 3; b++) {
            const by = 3 + b * 20;
            ctx.fillStyle = frost ? '#6a5c4e' : '#4e3a26'; ctx.fillRect(4, by, S - 8, 15);
            const n = stage === 0 || frost ? 0 : stage;
            for (let k = 0; k < 9; k++) {
              const x = 8 + k * 6.4, y = by + 7.5;
              ctx.fillStyle = n >= 3 ? (k % 3 === 0 ? '#c45a5a' : '#7ab048') : n >= 1 ? '#6aa23a' : 'transparent';
              ctx.beginPath(); ctx.arc(x, y, 1 + n * 0.8, 0, 7); ctx.fill();
            }
          }
        }
        ctx.restore();
      }
    }
  }
  return atlas;
}

/** The stage of a field's year, 0 to 5. */
export function fieldStage(zone, crop, cal) {
  const { season, weekInSeason } = cal;
  if (zone === ZONE_ID.pasture || zone === ZONE_ID.orchard) return [2, 3, 4, 5][season] ?? 3;
  if (season === 3) return 5;
  if (season === 2) return crop >= 120 ? (weekInSeason < 7 ? 3 : 4) : crop > 20 ? 2 : 0;
  return crop < 8 ? 0 : crop < 60 ? 1 : crop < 150 ? 2 : 3;
}

export function createDecals(GFX, world, materials, scene) {
  const group = new GFX.Group();
  group.name = 'decals';
  scene.add(group);
  const roadAtlas = new GFX.CanvasTexture(paintRoads(materials.tex.cobble.map.image));
  roadAtlas.colorSpace = GFX.SRGBColorSpace;
  roadAtlas.anisotropy = 8;
  const fieldAtlas = new GFX.CanvasTexture(paintFields());
  fieldAtlas.colorSpace = GFX.SRGBColorSpace;
  fieldAtlas.anisotropy = 8;
  const roadMat = new GFX.MeshStandardMaterial({ map: roadAtlas, transparent: true, roughness: 0.95, metalness: 0, depthWrite: false });
  const fieldMat = new GFX.MeshStandardMaterial({ map: fieldAtlas, roughness: 0.95, metalness: 0, transparent: true, depthWrite: false });
  const zoneMat = new GFX.MeshBasicMaterial({ vertexColors: true, transparent: true, opacity: 0.26, depthWrite: false, toneMapped: false });
  const chunks = Array.from({ length: CHUNKS * CHUNKS }, () => ({ road: null, field: null, zone: null, fields: false }));
  let zoneBoost = 0;
  let cal = { season: 1, weekInSeason: 0 };

  /** Rows run along the longer way a field goes: 0 along x, 1 along y. */
  const runsAlong = (x, y, z) => {
    let hx = 1, hy = 1;
    for (let k = 1; k < 12 && inb(x - k, y) && world.zone[idx(x - k, y)] === z; k++) hx++;
    for (let k = 1; k < 12 && inb(x + k, y) && world.zone[idx(x + k, y)] === z; k++) hx++;
    for (let k = 1; k < 12 && inb(x, y - k) && world.zone[idx(x, y - k)] === z; k++) hy++;
    for (let k = 1; k < 12 && inb(x, y + k) && world.zone[idx(x, y + k)] === z; k++) hy++;
    return hx >= hy ? 0 : 1;
  };

  const hAt = (vx, vy) => world.h[Math.max(0, Math.min(HV - 1, vy)) * HV + Math.max(0, Math.min(HV - 1, vx))];

  /** A tile's quad as 2x2 pieces that follow the ground. `uv` is [u0, v0, u1, v1] across the whole tile. */
  const tileQuads = (out, x, y, uv, color = null) => {
    const step = 1 / SUB;
    for (let j = 0; j < SUB; j++) {
      for (let i = 0; i < SUB; i++) {
        const vx = x * SUB + i, vy = y * SUB + j;
        const pts = [[0, 0], [1, 0], [1, 1], [0, 1]].map(([a, b]) => ({ x: x + (i + a) * step, z: y + (j + b) * step, y: hAt(vx + a, vy + b) + LIFT, u: (i + a) / SUB, v: (j + b) / SUB }));
        for (const k of [0, 2, 1, 0, 3, 2]) {
          const p = pts[k];
          out.pos.push(p.x, p.y, p.z);
          out.nor.push(0, 1, 0);
          if (uv) out.uv.push(uv[0] + (uv[2] - uv[0]) * p.u, uv[1] + (uv[3] - uv[1]) * p.v);
          if (color) out.col.push(color[0], color[1], color[2]);
        }
      }
    }
  };

  const geometry = (out, withUv, withColor) => {
    if (!out.pos.length) return null;
    const g = new GFX.BufferGeometry();
    g.setAttribute('position', new GFX.BufferAttribute(new Float32Array(out.pos), 3));
    g.setAttribute('normal', new GFX.BufferAttribute(new Float32Array(out.nor), 3));
    if (withUv) g.setAttribute('uv', new GFX.BufferAttribute(new Float32Array(out.uv), 2));
    if (withColor) g.setAttribute('color', new GFX.BufferAttribute(new Float32Array(out.col), 3));
    g.computeBoundingSphere();
    return g;
  };

  const place = (slot, c, g, material, order) => {
    if (c[slot]) { group.remove(c[slot]); freeGeometry(c[slot].geometry); c[slot] = null; }
    if (!g) return;
    const mesh = new GFX.Mesh(g, material);
    mesh.renderOrder = order;
    mesh.receiveShadow = true;
    group.add(mesh);
    c[slot] = mesh;
  };

  const build = (i) => {
    const cx = i % CHUNKS, cy = Math.floor(i / CHUNKS);
    const roads = { pos: [], nor: [], uv: [] }, fields = { pos: [], nor: [], uv: [] }, zones = { pos: [], nor: [], col: [] };
    const c = chunks[i];
    let any = false;
    const roadAt = (x, y) => inb(x, y) && world.road[idx(x, y)] !== ROAD.NONE;
    for (let y = cy * CHUNK; y < (cy + 1) * CHUNK; y++) {
      for (let x = cx * CHUNK; x < (cx + 1) * CHUNK; x++) {
        const t = idx(x, y);
        const r = world.road[t];
        if (r === ROAD.DIRT || r === ROAD.COBBLE) {
          const mask = (roadAt(x, y - 1) ? 1 : 0) | (roadAt(x + 1, y) ? 2 : 0) | (roadAt(x, y + 1) ? 4 : 0) | (roadAt(x - 1, y) ? 8 : 0);
          const variant = Math.floor(((x * 73856093) ^ (y * 19349663)) >>> 0) % 4;
          const row = (r === ROAD.COBBLE ? 4 : 0) + variant;
          const u0 = (mask * CELL + 1) / (CELL * 16), u1 = ((mask + 1) * CELL - 1) / (CELL * 16);
          const v0 = 1 - (row * CELL + 1) / (CELL * 8), v1 = 1 - ((row + 1) * CELL - 1) / (CELL * 8);
          tileQuads(roads, x, y, [u0, v0, u1, v1]);
          continue;
        }
        const z = world.zone[t];
        if (!z || world.bld[t] || world.road[t]) continue;
        if (z >= ZONE_ID.wheat) {
          const name = z === ZONE_ID.wheat ? 0 : z === ZONE_ID.veg ? 1 : 2;
          const orient = runsAlong(x, y, z);
          const stage = fieldStage(z, world.crop[t], cal);
          const row = name * 2 + orient;
          const u0 = (stage * CELL + 1) / (CELL * 8), u1 = ((stage + 1) * CELL - 1) / (CELL * 8);
          const v0 = 1 - (row * CELL + 1) / (CELL * 8), v1 = 1 - ((row + 1) * CELL - 1) / (CELL * 8);
          tileQuads(fields, x, y, [u0, v0, u1, v1]);
          any = true;
        } else {
          const color = rgb(ZONES[Object.keys(ZONES)[z - 1]].color);
          tileQuads(zones, x, y, null, color);
        }
      }
    }
    // Kitchen gardens in the land behind the houses.
    if (world.cores) {
      for (const t of world.cores) {
        const x = t % world.size, y = Math.floor(t / world.size);
        if (Math.floor(x / CHUNK) !== cx || Math.floor(y / CHUNK) !== cy || world.bld[t]) continue;
        const orient = (x + y) & 1;
        const stage = [1, 3, 3, 0][cal.season] ?? 1;
        const row = 6 + orient;
        const u0 = (stage * CELL + 1) / (CELL * 8), u1 = ((stage + 1) * CELL - 1) / (CELL * 8);
        const v0 = 1 - (row * CELL + 1) / (CELL * 8), v1 = 1 - ((row + 1) * CELL - 1) / (CELL * 8);
        tileQuads(fields, x, y, [u0, v0, u1, v1]);
      }
    }
    place('road', c, geometry(roads, true, false), roadMat, 1.5);
    place('field', c, geometry(fields, true, false), fieldMat, 1.2);
    place('zone', c, geometry(zones, false, true), zoneMat, 1.8);
    c.fields = any;
  };

  return {
    group,
    sync(calendar, budget = 3) {
      cal = calendar;
      let n = 0;
      for (const i of [...world.dirty.flat]) {
        world.dirty.flat.delete(i);
        build(i);
        if (++n >= budget) break;
      }
    },
    buildAll() { for (let i = 0; i < chunks.length; i++) build(i); world.dirty.flat.clear(); },
    /** The crops change with the year: redraw the chunks with fields in them. */
    refreshFields(calendar) {
      cal = calendar;
      chunks.forEach((c, i) => { if (c.fields) world.dirty.flat.add(i); });
    },
    /** How strongly the zones show: faint, or bold while the lord is zoning. */
    setZoneBoost(b) {
      zoneBoost = b;
      zoneMat.opacity = 0.2 + 0.28 * zoneBoost;
    },
    get zoneBoost() { return zoneBoost; },
  };
}
