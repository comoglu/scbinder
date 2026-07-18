# National (Australia-wide) grid

Nine scbinder instances tiling Australia and its seismically-relevant
surroundings in a 3x3 compass-rose layout, so the flat-earth projection
grid.c/hyp.c use stays accurate everywhere (see BINDER_NOTES.md /
README.md — a single grid across the whole continent's ~35 deg latitude
range would visibly distort at its extremes).

```
 nw  n  ne      Banda Sea/Timor/PNG margin — 0-300 km
  w  c   e      craton, occasional deeper events — 0-100 km
 sw  s  se      stable craton, shallow only — 0-40 km
```

See COVERAGE.md for exact tile bounds. All tiles use `velmod iasp91` and
the association/location parameters tuned and regression-verified in
harness/ and harness/sweep/.

## Regenerating

```
python3 tools/gen_national_grids.py
```

Writes `binder-<tile>.d` and `scbinder-<tile>.cfg` for all 9 tiles plus
`COVERAGE.md`. Edit the constants at the top of the script (bounding
box, tile/overlap size, per-row depth ranges), not the generated files.

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
tile `n` (the largest grid, 300 km depth, 176x133x30 cells) initializes
cleanly in ~2s; tile `se` reproduces a real event via `--ep` end to end
(67-phase solution, ~10 km from the reference network solution).

## Enabling

Created aliases are **not** started by default. To bring a tile online:

```
seiscomp enable scbinder-<tile>
seiscomp start scbinder-<tile>
```

Start with one or two tiles covering your area of interest and confirm
behavior in scolv before enabling all nine — nine simultaneous instances
means nine independent streams of incremental origins onto LOCATION.
