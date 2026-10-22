import { CENTER, TICK_SECONDS } from './config.js';
import { startGame, step } from './sim/game.js';
import { createScene } from './render/scene.js';

const params = new URLSearchParams(location.search);
const host = document.getElementById('game');
const game = startGame({ seed: params.get('seed') ?? 'moat', difficulty: params.get('difficulty') ?? 'normal' });

// Development: play a bot's hand for some weeks first, to see a town.
const pre = +(params.get('bot') ?? 0);
if (pre && import.meta.env.DEV) {
  const { createBot } = await import('../test/bot.js');
  const bot = createBot(game);
  for (let w = 0; w < pre && !game.gameOver; w++) { bot.think(); step(game); }
} else if (+(params.get('week') ?? 0)) {
  for (let w = 0; w < +params.get('week'); w++) step(game);
}

const scene = await createScene({ host, game, progress: (f, text) => { document.title = `${Math.round(f * 100)}% ${text}`; } });
const { stage, camera, atmosphere } = scene;

const p = (k, d) => (params.has(k) ? +params.get(k) : d);
camera.focus(p('x', CENTER), p('z', CENTER + 3), { zoom: p('zoom', 2.4), yaw: p('yaw', 0.3), instant: true });
stage.view.aim.pitch = p('pitch', 0.95);
stage.view.step(1, true);
if (params.has('tod')) { atmosphere.setMode('hold'); atmosphere.setTime(p('tod', 0.45)); }

let speed = p('speed', 1);
let acc = 0;
stage.run((dt) => {
  if (speed > 0 && !game.gameOver) {
    acc += dt * speed;
    while (acc >= TICK_SECONDS) { acc -= TICK_SECONDS; step(game); }
  }
  scene.weekFrac = acc / TICK_SECONDS;
  scene.frame(dt);
});
globalThis.moat = { game, scene, setSpeed: (s) => { speed = s; } };
document.title = 'ready';
