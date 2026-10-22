# Design notes

## Time and scale

* One tile is one world unit. The map is 210 × 210 tiles: 7 × 7 wards of 30 × 30. The castle stands in the middle of the middle ward.
* A tick is a week (3.2 s at normal speed). A year is 48 weeks, starting 1 March; seasons are 12 weeks.
* Heights have two vertices to a tile so that moat banks and hills come out round. `world.hBase` holds the land before any moat is cut, so that a moat can be filled in when its ward is taken.

## The moat

The moat is a distance field from the *exposed borders* of owned wards (borders with a ward that is not yet owned). The trench profile is `-0.95 + (plateau + 0.5·max(0, d − 2.5) + 0.95)·smoothstep(0.7, 3.2, d)`; tiles within 1.6 of a border are water and within 2.8 are bank (nothing may be built). Annexing a ward recomputes the field, so the old border between the two wards becomes plain again.

## A week

weather → work and farms → food and fuel → money → mood → births and deaths → construction → coverage and blocks → growth → merchants → fire → plague → raids → tribute → rank.

Labour: the garrison, then a third of the hands held for building sites, then priority-3 jobs (food, and woodcutters when timber is low), then crews up to 55%, then the rest by priority.

## Donut blocks

`src/sim/blocks.js` finds every closed loop of road enclosing 9–800 tiles (a flood fill over non-road tiles; faces touching the map edge are open land). Tiles within three of the street are the *ring*; the rest is the *core*.

* `closure = clamp((built / zoned − 0.35) / 0.55) · min(1, homes / 6)`
* `heart = 1 − Π(1 − fineᵢ)` over the distinct special buildings in the core (green 0.85, cathedral 0.9, chapel 0.6, market 0.6, castle 0.7, tavern 0.45, healer 0.45, well 0.4, watchtower 0.35, barracks 0.3; ×1.15 per extra of a kind) and the worked ground (gardens 0.3, allotments 0.36, orchard 0.42, common 0.32, field 0.25)
* `rating = round(100 · heart · closure)`, `q = rating / 100`

Effects: the core's services are lifted to `strength · (0.45 + 0.55·closure)` over every ring and core tile; homes' tax × `(1 + 0.18·q)`; beds + `12%·q`; mood `+7·q` weighted by the share of beds in blocks; shop capacity × `(1 + 0.3·q)`; fire spread × `(1 − 0.5·q)`, ignition × `(1 − 0.3·q)`; lot value `+1.4·q`; upgrade bars lowered by `8·q` happiness.

## Saves

The world is regenerated from the seed; a save keeps runs of changed cells for each map (`ground tree amount road zone crop`, and the two height maps to a thousandth), the buildings, wards, gates and the game's numbers. Autosave each season and on leaving the page (`localStorage`, key `moat.save.v1`).

## Known rough edges

* Early balance is tight on some seeds: a passive hamlet stagnates, and an active lord must find timber early (a forester's lodge plants new woods, and the starting ward has a belt of trees inside the bank).
* Rendering in a software-GL browser is slow; shader compiles are the first-use hitches.
