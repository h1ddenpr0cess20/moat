// Headless test driver for the ROM (jsnes): runs a script of button presses and writes screenshots (PPM) and RAM peeks.
// usage: node shot.mjs <rom> <outdir> '<json script>'
//   script: { "frames": N, "press": [[frame, "A", holdFrames], ...], "shots": [frame, ...], "peek": ["label", ...] }
import { NES, Controller as C } from 'jsnes';
import fs from 'fs';
import path from 'path';
const [rom, out, scriptText = '{}'] = process.argv.slice(2);
const script = JSON.parse(scriptText);
fs.mkdirSync(out, { recursive: true });
let fb;
const nes = new NES({ onFrame(b) { fb = b.slice(); }, onAudioSample() {} });
nes.loadROM(fs.readFileSync(rom, 'binary'));
nes.ppu.clipToTvSize = false;
const lblPath = path.join(path.dirname(rom), 'build', 'moat.lbl');
const L = {};
if (fs.existsSync(lblPath)) for (const l of fs.readFileSync(lblPath, 'utf8').split('\n')) { const m = l.match(/al ([0-9A-F]+) \.(\w+)/); if (m) L[m[2]] = parseInt(m[1], 16); }
const BT = { A: C.BUTTON_A, B: C.BUTTON_B, SELECT: C.BUTTON_SELECT, START: C.BUTTON_START, UP: C.BUTTON_UP, DOWN: C.BUTTON_DOWN, LEFT: C.BUTTON_LEFT, RIGHT: C.BUTTON_RIGHT };
const ppm = () => { const b = Buffer.alloc(256 * 240 * 3); for (let i = 0; i < 256 * 240; i++) { const c = fb[i]; b[i * 3] = c & 255; b[i * 3 + 1] = (c >> 8) & 255; b[i * 3 + 2] = (c >> 16) & 255; } return Buffer.concat([Buffer.from('P6\n256 240\n255\n'), b]); };
const downs = new Map();
const frames = script.frames ?? 60;
const peek = (name, len = 1) => { const a = L[name] ?? L['_' + name]; const M = nes.cpu.mem; if (a === undefined) return null; return len === 1 ? M[a] : Array.from(M.slice(a, a + len)); };
for (let f = 0; f <= frames; f++) {
  for (const [at, btn, hold = 3] of script.press ?? []) {
    if (at === f) { nes.buttonDown(1, BT[btn]); downs.set(btn + ':' + f, f + hold); }
  }
  for (const [k, until] of [...downs]) if (until <= f) { nes.buttonUp(1, BT[k.split(':')[0]]); downs.delete(k); }
  nes.frame();
  if ((script.shots ?? []).includes(f)) fs.writeFileSync(path.join(out, `f${String(f).padStart(5, '0')}.ppm`), ppm());
  for (const [at, name, len] of script.peeks ?? []) if (at === f) console.log(`frame ${f} ${name} =`, peek(name, len ?? 1));
}
if (script.dump) { const M = nes.cpu.mem; for (const [name, len] of script.dump) console.log(name, JSON.stringify(peek(name, len))); }
