// Browser player for the Moat NES ROM, on the jsnes emulator. The cartridge's battery RAM (the saved realm) is kept in localStorage.
import { NES, Controller } from 'jsnes';

const $ = (id) => document.getElementById(id);
const canvas = $('nes');
const ctx = canvas.getContext('2d');
const image = ctx.createImageData(256, 240);
const frame32 = new Uint32Array(image.data.buffer);

// --- audio: a ring buffer feeding a ScriptProcessor -----------------------------------
const BUF = 8192, MASK = BUF - 1;
const ring = [new Float32Array(BUF), new Float32Array(BUF)];
let rd = 0, wr = 0, audio = null, muted = false;
try { muted = localStorage.getItem('moat-nes-muted') === '1'; } catch { /* storage unavailable */ }

function startAudio() {
  if (audio) { audio.resume?.(); return; }
  const AC = window.AudioContext || window.webkitAudioContext;
  if (!AC) return;
  audio = new AC({ sampleRate: 44100 });
  const node = audio.createScriptProcessor(1024, 0, 2);
  node.onaudioprocess = (e) => {
    const L = e.outputBuffer.getChannelData(0), R = e.outputBuffer.getChannelData(1);
    for (let i = 0; i < L.length; i++) {
      const have = ((wr - rd) & MASK) > 0;
      L[i] = have && !muted ? ring[0][rd] : 0;
      R[i] = have && !muted ? ring[1][rd] : 0;
      if (have) rd = (rd + 1) & MASK;
    }
  };
  node.connect(audio.destination);
}

// --- the cartridge's battery RAM -------------------------------------------------------
const SAVE_KEY = 'moat-nes-sram-v1';
const sram = new Uint8Array(0x2000);
let dirty = false;
function loadSram() {
  try {
    const text = localStorage.getItem(SAVE_KEY);
    if (!text) return;
    const bin = atob(text);
    if (bin.length !== sram.length) return;
    for (let i = 0; i < bin.length; i++) sram[i] = bin.charCodeAt(i);
    for (let i = 0; i < sram.length; i++) nes.cpu.mem[0x6000 + i] = sram[i];
  } catch { /* nothing saved, or storage unavailable */ }
}
function flushSram() {
  if (!dirty) return;
  dirty = false;
  try {
    let s = '';
    for (let i = 0; i < sram.length; i += 0x1000) s += String.fromCharCode.apply(null, sram.subarray(i, i + 0x1000));
    localStorage.setItem(SAVE_KEY, btoa(s));
  } catch { /* storage full or unavailable */ }
}
setInterval(flushSram, 2000);
addEventListener('pagehide', flushSram);

// --- emulator --------------------------------------------------------------------------
const nes = new NES({
  onFrame(buffer) {
    for (let i = 0; i < 256 * 240; i++) frame32[i] = 0xFF000000 | buffer[i];
    ctx.putImageData(image, 0, 0);
  },
  onAudioSample(l, r) {
    if (((wr + 1) & MASK) === rd) return;
    ring[0][wr] = l; ring[1][wr] = r; wr = (wr + 1) & MASK;
  },
  onBatteryRamWrite(address, value) {
    if (address >= 0x6000 && address < 0x8000) { sram[address - 0x6000] = value; dirty = true; }
  },
  sampleRate: 44100,
});

let running = false, paused = false, last = 0, acc = 0;
const FRAME_MS = 1000 / 60.0988;
function loop(t) {
  requestAnimationFrame(loop);
  if (paused) { last = t; return; }
  acc += Math.min(100, t - last); last = t;
  let n = 0;
  while (acc >= FRAME_MS && n < 4) { nes.frame(); acc -= FRAME_MS; n++; }
}

async function loadRom(source) {
  const bytes = source instanceof ArrayBuffer ? source : await (await fetch(source)).arrayBuffer();
  let s = '';
  const u8 = new Uint8Array(bytes);
  for (let i = 0; i < u8.length; i += 0x8000) s += String.fromCharCode.apply(null, u8.subarray(i, i + 0x8000));
  nes.loadROM(s);
  nes.ppu.clipToTvSize = false;     // the whole 256x240 picture, not a TV's cropped one
  loadSram();
}

function begin() {
  startAudio();
  $('start').hidden = true;
  if (!running) { running = true; last = performance.now(); requestAnimationFrame(loop); }
}

// --- input -----------------------------------------------------------------------------
const KEYS = {
  ArrowLeft: 'LEFT', ArrowRight: 'RIGHT', ArrowUp: 'UP', ArrowDown: 'DOWN',
  KeyA: 'LEFT', KeyD: 'RIGHT', KeyW: 'UP', KeyS: 'DOWN',
  KeyZ: 'A', Space: 'A', KeyX: 'B', Enter: 'START', ShiftRight: 'SELECT', ShiftLeft: 'SELECT', Tab: 'SELECT',
};
function press(name, down) {
  const btn = Controller['BUTTON_' + name];
  if (down) nes.buttonDown(1, btn); else nes.buttonUp(1, btn);
}
addEventListener('keydown', (e) => {
  if (e.code === 'KeyM' && !e.repeat) { toggleMute(); return; }
  const b = KEYS[e.code];
  if (!b) return;
  e.preventDefault();
  if (!running) begin();
  press(b, true);
});
addEventListener('keyup', (e) => { const b = KEYS[e.code]; if (b) { e.preventDefault(); press(b, false); } });
for (const btn of document.querySelectorAll('#pad button')) {
  const b = btn.dataset.b;
  btn.addEventListener('pointerdown', (e) => { e.preventDefault(); if (!running) begin(); press(b, true); });
  for (const ev of ['pointerup', 'pointerleave', 'pointercancel']) btn.addEventListener(ev, () => press(b, false));
}
addEventListener('blur', () => { for (const b of ['LEFT', 'RIGHT', 'UP', 'DOWN', 'A', 'B', 'START', 'SELECT']) press(b, false); });

// --- toolbar ---------------------------------------------------------------------------
function toggleMute() {
  muted = !muted;
  try { localStorage.setItem('moat-nes-muted', muted ? '1' : '0'); } catch { /* storage unavailable */ }
  $('mute').textContent = muted ? 'Sound off' : 'Sound on';
}
$('mute').textContent = muted ? 'Sound off' : 'Sound on';
$('mute').addEventListener('click', toggleMute);
$('go').addEventListener('click', begin);
$('pause').addEventListener('click', () => {
  paused = !paused;
  $('pause').textContent = paused ? 'Resume' : 'Pause';
  if (audio) { if (paused) audio.suspend(); else audio.resume(); }
});
$('reset').addEventListener('click', () => { nes.reset?.(); });
$('forget').addEventListener('click', () => {
  try { localStorage.removeItem(SAVE_KEY); } catch { /* storage unavailable */ }
  sram.fill(0); for (let i = 0; i < sram.length; i++) nes.cpu.mem[0x6000 + i] = 0;
  nes.reset?.();
});
$('fs').addEventListener('click', () => document.querySelector('.screen').requestFullscreen?.());
$('load').addEventListener('click', () => $('file').click());
$('file').addEventListener('change', async (e) => {
  const f = e.target.files[0];
  if (f) { await loadRom(await f.arrayBuffer()); begin(); }
});

loadRom(import.meta.env.BASE_URL + 'moat.nes').then(() => {
  for (let i = 0; i < 90; i++) nes.frame();        // the title behind the Play button
}).catch((err) => { $('start').querySelector('p').textContent = 'Could not load the ROM: ' + err.message; });
