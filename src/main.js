import * as GFX from 'madness-engine/gfx';
import { createStage } from 'madness-engine/stage';
import { defineView } from 'madness-engine/view';
import { CENTER } from './config.js';
import { startGame } from './sim/game.js';
import { createMaterials, colorLeaves } from './render/materials.js';
import { createTerrain } from './render/terrain.js';
import { createWater } from './render/water.js';
import { createSkyDomes, lightAt, paintDay, paintReflections } from './render/sky.js';
import { heightAt } from './world/world.js';

const params = new URLSearchParams(location.search);
const host = document.getElementById('game');
const game = startGame({ seed: params.get('seed') ?? 'moat' });
const { world } = game;

const viewDef = defineView({ home: { yaw: 0.0, pitch: 0.95, zoom: 2.2 }, limits: { pitch: [0.28, 1.5], zoom: [0.2, 22] } });
const sunDir = new GFX.Vector3(-0.3, 1, 0.6).normalize();
const day = paintDay();
const stage = await createStage(host, {
  lens: { distance: 12, fov: 34 }, view: viewDef.createView(), sky: day, room: paintReflections(day),
  lights: { key: { from: sunDir }, fill: null }, shadowSize: 2048, backdrop: '#9fb8c8', label: 'moat',
});
stage.camera.far = 900;
const materials = createMaterials(GFX);
const terrain = createTerrain(GFX, world, materials, stage.scene);
const water = createWater(GFX, world, materials, stage.scene);
const domes = createSkyDomes(GFX, stage.scene);
water.build();
const t0 = performance.now();
await terrain.paintAll([CENTER, CENTER]);
console.log('painted ground in', Math.round(performance.now() - t0), 'ms');

const season = +(params.get('season') ?? 1);
const tod = +(params.get('tod') ?? 0.42);
colorLeaves(materials, GFX, season, 0.3);
terrain.setSeason(season, 0.3, season === 3 ? 0.85 : 0);
const L = lightAt(tod, 0.95);
sunDir.set(...L.dir);
stage.key.color.setRGB(...L.key);
stage.key.intensity = L.keyIntensity;
stage.hemi.color.setRGB(...L.hemiSky);
stage.hemi.groundColor.setRGB(...L.hemiGround);
stage.hemi.intensity = L.hemiIntensity;
domes.set(L.dusk, L.night);

stage.target.set(CENTER, heightAt(world, CENTER, CENTER), CENTER + 4);
stage.view.aim.zoom = +(params.get('zoom') ?? 2.6);
stage.view.aim.pitch = +(params.get('pitch') ?? 0.9);
stage.view.aim.yaw = +(params.get('yaw') ?? 0.4);
stage.view.step(1, true);
stage.run((dt) => { water.update(dt); });
globalThis.moat = { game, stage, world, terrain, water, materials };
document.title = 'ready';
