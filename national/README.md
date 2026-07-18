# National (Australia-wide) grid

Nine scbinder instances tiling Australia and its seismically-relevant
surroundings in a 3x3 compass-rose layout, so the flat-earth projection
grid.c/hyp.c use stays accurate everywhere (see BINDER_NOTES.md /
README.md — a single grid across the whole continent's ~35 deg latitude
range would visibly distort at its extremes).

```
 nw  n  ne      Banda Sea/Timor/PNG margin, 0-300 km — sparse coverage
  w  c   e      craton, occasional deeper, 0-100 km  — sparse/moderate
 sw  s  se      stable craton, shallow, 0-40 km       — dense coverage
```

See COVERAGE.md for exact tile bounds and per-tile parameters. All tiles
use `velmod iasp91` and the association/location parameters tuned and
regression-verified in harness/ and harness/sweep/ — **except** the
nucleation grid reach (`dspace`/`rstack`/`r_max`), which is tiered by
expected station density (see below).

## Station-density tiering

Real AU network coverage is far from uniform: dense in the SE corner
(Vic/NSW/Tas), thinning fast moving north/west/inland. One set of
nucleation-grid parameters does not work everywhere:

| tier | tiles | dspace | rstack | r_max | basis |
|---|---|---|---|---|---|
| dense | sw, s, se | 10 km | 300 km | 400 km | original tuning, regression + playback verified |
| moderate | e | 15 km | 700 km | 1000 km | interpolated, not yet independently validated |
| sparse | nw, n, ne, w, c | 20 km | 1200 km | 2000 km | **validated** against a real missed event (below) |

**Why this exists** (found 2026-07-19): tile `nw`'s original dense-tier
config completely missed a real M6.5 offshore Broome event
(`ga2026odlbri`, -18.24/120.31, 105 km deep) that scanloc located with
78 phases. It wasn't a coverage or subscription bug — `nw` was correctly
receiving the relevant picks. The nucleation stack (`grid_stack, mhit`)
peaked at 8 against `thresh=16` and never fired, because the *closest*
usable station was already 2.96 deg (~330 km) away and full azimuthal
closure needed the Indonesian IA/GE network out to ~15-17 deg
(1600-1900 km) — all well beyond the dense tier's 300 km `rstack`.

The fix that worked was widening reach (`dspace`/`rstack`/`r_max`)
**while leaving `thresh` at 16**. The wrong fix — tried first, and worth
recording as a warning — was lowering `thresh` instead: it did make the
grid nucleate, but on a *false* solution, a dense local seismic array
(most likely the Warramunga-style AS*/W* station cluster) stacking up
30+ picks ~340 km from the truth. **Sparse regions need more reach, not
a lower bar** — a lower bar just makes local arrays easier to
false-trigger. With reach widened and thresh unchanged, `nw` reproduced
a solution 17 km from the real network answer (32 picks, 87 deg gap).

Confirmed live in production the same day: once deployed, `scbinder-nw`
caught the same Broome event as it replayed through the live pick
stream, converging to -18.196/120.232 (~10 km from the scanloc/
scautoloc reference, matched by creation-time cross-check in the DB —
better than the offline estimate above). The same run also produced two
other simultaneous nucleations, initially assumed to be false triggers
from a dense local array; a catalog cross-check instead found both land
almost exactly on real, recurring GA-cataloged seismic zones (Tennant
Creek NT swarm, and a historical epicenter near -23.6/133.9 matching
ga2019mxpehi) — i.e. plausibly genuine local seismicity concurrent in
the same pick stream, not algorithmic false positives. Treat `moderate`
and further sparse-tier refinement (e.g. validating against a second,
independent sparse-region event) as open, but the sparse tier's core
behavior — nucleate correctly on real events, don't obviously hallucinate
on noise — held up under a live production check, not just the offline
replay.

## Regenerating

```
python3 tools/gen_national_grids.py
```

Writes `binder-<tile>.d` and `scbinder-<tile>.cfg` for all 9 tiles plus
`COVERAGE.md`. Edit the constants at the top of the script (bounding
box, tile/overlap size, per-row depth ranges, `DENSITY`/`TILE_DENSITY`),
not the generated files.

## Deploying a tile

Each tile is a separate SeisComP module alias of `scbinder`:

```
seiscomp alias create scbinder-<tile> scbinder
cp national/binder-<tile>.d      ~/.seiscomp/national/    # or etc/national/ system-wide
cp national/scbinder-<tile>.cfg  ~/.seiscomp/             # or etc/ system-wide
```

Two things that are **not** obvious from a normal single-instance scbinder
setup and will silently break multi-instance operation if missed —
both are already baked into the generated `scbinder-<tile>.cfg`:

- **`connection.username`** does not default to the alias name. Every
  alias otherwise tries to connect as plain `scbinder` and the second
  one to start fails with `Client name not unique`.
- **`connection.subscriptions`** does not inherit from `scbinder.cfg`'s
  own overrides (e.g. a `~/.seiscomp/scbinder.cfg` fix for a
  L1PICK/ALPICK/RGALPICK-publishing pickers setup) — aliases read their
  own config namespace, so any such fix needs repeating per tile.

`@CONFIGDIR@` in `binder.config` resolves to whichever config directory
the running instance actually reads from (`~/.seiscomp` for a per-user
setup, `etc/` for a system-wide one) — put `binder-<tile>.d` in a
matching `national/` subdirectory there.

Verified (2026-07-19): tile `s` initializes with its site table
correctly restricted to its own grid (1949/12683 inventory channels);
tile `n` (the largest grid, 300 km depth, sparse-tier reach) initializes
cleanly in a few seconds; tile `se` reproduces a real event via `--ep`
end to end (67-phase solution, ~10 km from the reference network
solution); tile `nw`, after the density-tier fix, reproduces the real
Broome event ~17 km from the reference network solution (see above).

## Enabling

```
seiscomp enable scbinder-<tile>
seiscomp start scbinder-<tile>
```

Start with one or two tiles covering your area of interest and confirm
behavior in scolv before enabling all nine — nine simultaneous instances
means nine independent streams of incremental origins onto LOCATION.
