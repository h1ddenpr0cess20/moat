/**
 * The numbers the whole game agrees on. One tile is one unit of the engine's
 * world; the map is a grid of wards, and a ward is what the moat is dug round.
 */

/** Tiles along one side of a ward, wards along one side of the map, and so tiles along the map. */
export const WARD = 30;
export const WARDS = 7;
export const SIZE = WARD * WARDS;
export const VERTS = SIZE + 1;
export const TILES = SIZE * SIZE;

/** The ward the castle stands in, and where in the map its middle is. */
export const HOME_WARD = (WARDS - 1) / 2;
export const CENTER = SIZE / 2;

/** Standing water, in world units up; and what the ground round a moat is cut to. */
export const WATER_LEVEL = 0;
/** The moat's half width (water), and how far from its middle nothing may be built (the banks). */
export const MOAT_HALF = 1.6;
export const MOAT_BANK = 2.8;
/** The castle's island: half its side (the courtyard), the inner moat's two edges, and the bank beyond. */
export const CASTLE = Object.freeze({ half: 3, moatIn: 3.4, moatOut: 4.7, bank: 5.4 });

/** The calendar: a tick is a week; four to a month, twelve months to a year. */
export const WEEKS_PER_MONTH = 4;
export const MONTHS_PER_YEAR = 12;
export const WEEKS_PER_YEAR = WEEKS_PER_MONTH * MONTHS_PER_YEAR;
/** Seconds of real time a week takes at the normal pace. */
export const TICK_SECONDS = 3.2;
export const MONTHS = ['March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December', 'January', 'February'];
export const SEASONS = ['Spring', 'Summer', 'Autumn', 'Winter'];

/** The ground is painted a chunk at a time; a chunk is a ward. */
export const CHUNK = WARD;
export const CHUNKS = WARDS;

/** The four ways a building can face (toward its street): north is -y, east +x, south +y, west -x. */
export const DIRS = Object.freeze([[0, -1], [1, 0], [0, 1], [-1, 0]]);

/** What a tile can be made of. */
export const GROUND = Object.freeze({ GRASS: 0, ROCK: 1, ORE: 2, WATER: 3, CLIFF: 4, SAND: 5 });

/** What can lie on a tile besides the ground. */
export const ROAD = Object.freeze({ NONE: 0, DIRT: 1, COBBLE: 2, BRIDGE: 3, SPAN: 4 });
