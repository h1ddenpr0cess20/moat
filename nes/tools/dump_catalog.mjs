// Dumps the game's catalog (src/catalog.js) as JSON for the NES asset script: one source of truth for costs, crews and rates.
import { BUILDINGS, RESOURCES, ZONES, CASTLE_LEVELS, RANKS, START, ROADS } from '../../src/catalog.js';
import { PRICES } from '../../src/sim/state.js';
console.log(JSON.stringify({ BUILDINGS, RESOURCES, ZONES, CASTLE_LEVELS, RANKS, START, ROADS, PRICES }));
