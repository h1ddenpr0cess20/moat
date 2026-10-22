import { test } from 'node:test';
import assert from 'node:assert/strict';
import { GOODS } from '../src/catalog.js';
import { demolish, headcount, setTax, startGame, step } from '../src/sim/game.js';
import { createBot } from './bot.js';

test('every starting hamlet has a connected farm, granary and woodcutter', () => {
  for (const seed of ['moat', 'oxmere', 'a1', 'b2', 'karst']) {
    const game = startGame({ seed });
    for (const type of ['farmstead', 'granary', 'woodcutter']) {
      const b = [...game.world.buildings.values()].find((x) => x.type === type);
      assert.ok(b, `${seed} has a ${type}`);
      assert.ok(b.connected, `${seed}: the ${type} is joined to the castle by a road`);
    }
  }
});

test('a bot plays three years without a crack: numbers stay finite and stores never go negative', () => {
  const game = startGame({ seed: 'c3' });
  const bot = createBot(game);
  for (let w = 0; w < 144 && !game.gameOver; w++) {
    bot.think();
    step(game);
    for (const g of GOODS) assert.ok(Number.isFinite(game.stock[g]) && game.stock[g] >= 0, `week ${w}: ${g} is ${game.stock[g]}`);
    assert.ok(Number.isFinite(game.happiness) && game.happiness >= 0 && game.happiness <= 100);
  }
  assert.ok(headcount(game) > 10, 'the town is alive');
});

test('taxes: heavier rates bring in more coin and weigh on the mood', () => {
  const a = startGame({ seed: 'moat' }), b = startGame({ seed: 'moat' });
  setTax(a, 0.05);
  setTax(b, 0.25);
  for (let w = 0; w < 6; w++) { step(a); step(b); }
  assert.ok(b.report.taxes > a.report.taxes);
  assert.ok(b.report.happinessTarget < a.report.happinessTarget);
});

test('a town with no food does not grow', () => {
  const game = startGame({ seed: 'moat' });
  game.stock.grain = 0; game.stock.produce = 0; game.stock.meat = 0;
  const before = headcount(game);
  for (let w = 0; w < 8; w++) step(game);
  assert.ok(headcount(game) <= before);
});

test('the clearing tool fells trees and breaks rock, and the stores gain by it', () => {
  const game = startGame({ seed: 'moat' });
  const { world } = game;
  let tree = null, rock = null;
  for (let i = 0; i < world.tree.length && !(tree && rock); i++) {
    if (!world.realm[i] || world.bld[i] || world.road[i] || world.zone[i]) continue;
    if (!tree && world.tree[i] > 0) tree = i;
    if (!rock && world.ground[i] === 1 && world.amount[i] > 0) rock = i;
  }
  assert.ok(tree !== null, 'there are trees in the realm');
  const wood = game.stock.wood;
  assert.ok(demolish(game, tree % world.size, Math.floor(tree / world.size)).ok);
  assert.equal(world.tree[tree], 0);
  assert.ok(game.stock.wood > wood);
  if (rock !== null) {
    const stone = game.stock.stone;
    assert.ok(demolish(game, rock % world.size, Math.floor(rock / world.size)).ok);
    assert.ok(game.stock.stone > stone);
  }
  assert.equal(demolish(game, 3, 3).ok, false, 'nothing is cleared outside the realm');
});
