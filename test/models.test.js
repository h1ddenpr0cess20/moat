import { test } from 'node:test';
import assert from 'node:assert/strict';
import { BUILDINGS } from '../src/catalog.js';
import { Mesher } from '../src/render/mesher.js';
import { modelFor } from '../src/render/models/index.js';

/**
 * Two faces in one plane, facing the same way and over each other, are drawn
 * by whichever the depth buffer happens to prefer at each pixel, so they
 * flicker as the camera moves. These tests keep every building's faces apart.
 */

const sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2];

/** The area two convex polygons share (Sutherland-Hodgman). */
function overlap(subject, clipper) {
  let out = subject;
  for (let i = 0; i < clipper.length && out.length; i++) {
    const a = clipper[i], b = clipper[(i + 1) % clipper.length];
    const inside = (p) => (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0]) >= -1e-12;
    const meet = (p, q) => {
      const a1 = q[1] - p[1], b1 = p[0] - q[0], c1 = a1 * p[0] + b1 * p[1];
      const a2 = b[1] - a[1], b2 = a[0] - b[0], c2 = a2 * a[0] + b2 * a[1];
      const det = a1 * b2 - a2 * b1;
      return [(b2 * c1 - b1 * c2) / det, (a1 * c2 - a2 * c1) / det];
    };
    const from = out;
    out = [];
    for (let j = 0; j < from.length; j++) {
      const p = from[j], q = from[(j + 1) % from.length];
      if (inside(q)) { if (!inside(p)) out.push(meet(p, q)); out.push(q); } else if (inside(p)) out.push(meet(p, q));
    }
  }
  let s = 0;
  for (let i = 0; i < out.length; i++) { const p = out[i], q = out[(i + 1) % out.length]; s += p[0] * q[1] - q[0] * p[1]; }
  return Math.abs(s) / 2;
}

/** A triangle counter-clockwise in its own plane's two main axes. */
const flat = (tri, u, v) => {
  const p = [tri.A, tri.B, tri.C].map((q) => [q[u], q[v]]);
  const s = (p[1][0] - p[0][0]) * (p[2][1] - p[0][1]) - (p[2][0] - p[0][0]) * (p[1][1] - p[0][1]);
  return s > 0 ? p : [p[0], p[2], p[1]];
};

/** Every pair of same-facing faces in a model that lie within `gap` of one plane and share more than `area` of it. Lit windows and flames are added to the picture, so they do not fight. */
function fights(template, { gap = 0.0035, area = 0.004 } = {}) {
  const tris = [];
  for (const [mat, p] of Object.entries(template)) {
    for (let i = 0; i < p.count; i += 3) {
      const A = [p.pos[i * 3], p.pos[i * 3 + 1], p.pos[i * 3 + 2]], B = [p.pos[i * 3 + 3], p.pos[i * 3 + 4], p.pos[i * 3 + 5]], C = [p.pos[i * 3 + 6], p.pos[i * 3 + 7], p.pos[i * 3 + 8]];
      const raw = cross(sub(B, A), sub(C, A));
      const len = Math.hypot(...raw);
      if (len < 1e-9) continue;
      const n = raw.map((x) => x / len);
      tris.push({ mat, A, B, C, n, d: dot(n, A) });
    }
  }
  const buckets = new Map();
  for (const t of tris) {
    const key = t.n.map((x) => Math.round(x * 20)).join(',');
    if (!buckets.has(key)) buckets.set(key, []);
    buckets.get(key).push(t);
  }
  const found = [];
  for (const list of buckets.values()) {
    list.sort((a, b) => a.d - b.d);
    for (let i = 0; i < list.length; i++) {
      for (let j = i + 1; j < list.length && list[j].d - list[i].d < gap; j++) {
        const a = list[i], b = list[j];
        if (dot(a.n, b.n) < 0.999 || a.mat === b.mat && (a.mat === 'glow' || a.mat === 'flame')) continue;
        const axes = [0, 1, 2].sort((x, y) => Math.abs(a.n[y]) - Math.abs(a.n[x]));
        const [u, v] = [axes[1], axes[2]];
        const shared = overlap(flat(a, u, v), flat(b, u, v));
        if (shared > area) found.push(`${a.mat}/${b.mat} ${shared.toFixed(3)} near ${a.A.map((x) => x.toFixed(2)).join(',')}`);
      }
    }
  }
  return found;
}

test('every face of a beam points out of it', () => {
  for (const [a, b] of [[[0, 0, 0], [0, 1, 0]], [[0, 0, 0], [1, 0, 0]], [[0, 0, 0], [0, 0, 1]], [[0, 0, 0], [1, 1, 0.3]]]) {
    const m = new Mesher();
    m.beam('wood', a, b, 0.1, '#fff');
    const p = m.finish().wood;
    const mid = a.map((x, k) => (x + b[k]) / 2);
    for (let i = 0; i < p.count; i += 3) {
      const c = [0, 1, 2].map((k) => (p.pos[i * 3 + k] + p.pos[i * 3 + 3 + k] + p.pos[i * 3 + 6 + k]) / 3 - mid[k]);
      assert.ok(dot([p.nor[i * 3], p.nor[i * 3 + 1], p.nor[i * 3 + 2]], c) > 0, `a face of the beam ${a} to ${b} faces inward`);
    }
  }
});

test('a ring faces up', () => {
  const m = new Mesher();
  m.ring('stone', 0, 0, 0, 0.5, 1, 12, '#fff');
  const p = m.finish().stone;
  for (let i = 0; i < p.count; i++) assert.ok(p.nor[i * 3 + 1] > 0.99);
});

test('no two faces of a building lie over each other in one plane', () => {
  const found = [];
  for (const type of Object.keys(BUILDINGS)) {
    for (const level of [1, 2, 3]) {
      for (const variant of [0, 1, 2, 3]) {
        for (const f of fights(modelFor(type, { level, variant }).parts)) found.push(`${type} (level ${level}, variant ${variant}): ${f}`);
      }
    }
    for (const progress of [0.1, 0.4, 0.7, 0.9]) {
      for (const f of fights(modelFor(type, { building: true, progress }).parts)) found.push(`${type} going up (${progress}): ${f}`);
    }
  }
  assert.deepEqual([...new Set(found)], []);
});
