import { h } from './dom.js';

/**
 * The title: over the land, the name, and the way in. A new realm is a seed
 * (the same seed is always the same land) and how hard the raiders come.
 */
export function createTitle({ seed, difficulty, canContinue, onBegin, onContinue, onNew }) {
  const seedInput = h('input', { type: 'text', value: seed, maxlength: 24, 'aria-label': 'Seed', spellcheck: 'false' });
  let level = difficulty;
  const seg = h('span.seg', {}, ...[['peaceful', 'Peaceful'], ['normal', 'Normal'], ['hard', 'Hard']].map(([id, name]) => h('button', { class: id === level ? 'on' : '', onclick: (e) => { level = id; e.target.parentNode.querySelectorAll('button').forEach((b) => b.classList.toggle('on', b === e.target)); } }, name)));
  const dice = () => { seedInput.value = Math.random().toString(36).slice(2, 8); };
  const begin = () => (seedInput.value.trim() === seed && level === difficulty && !canContinue ? onBegin() : onNew(seedInput.value.trim() || seed, level));
  const el = h('div.title-screen', {},
    h('h1', {}, 'MOAT'),
    h('div.tag-line', {}, 'A castle on an island, a hamlet at its gate. Raise a town of donut blocks, dig the moat wider, and claim the land.'),
    h('div.panel.title-card', {},
      canContinue ? h('button.btn.gold', { onclick: onContinue }, 'Continue your realm') : null,
      h('button.btn' + (canContinue ? '' : '.gold'), { onclick: begin }, canContinue ? 'A new realm' : 'Begin'),
      h('label', {}, 'Land (seed)', h('span', {}, seedInput, h('button.btn.small', { title: 'A different land', onclick: dice }, '⟲'))),
      h('label', {}, 'Raiders', seg)));
  return { el, remove() { el.remove(); } };
}
