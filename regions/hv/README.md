# scbinder-hv — Hawaii / HVO (network code HV)

Single scbinder instance covering the main Hawaiian chain for testing
against Hawaiian Volcano Observatory (`HV`) data. One instance is enough:
the grid spans 4 deg of latitude (18.5-22.5N), well inside the ~10-12 deg
limit for a single flat-earth projection (BINDER_NOTES.md), unlike
Australia's `national/` setup which needed 9 tiles for its ~35 deg span.

**Status: untuned.** The algorithm section of `binder-hv.d` is copied
verbatim from the AU dense tier (`national/binder-se.d`) — the config
that was regression-verified against real binder_ew and tuned on a live
AU playback. Only the grid bounds, depth range and crustal model are
Hawaii-specific, and those are geographic common sense, not validated.
Expect to retune `thresh` / `tstack` / `residual_cut` / `rmscut_pick`
after the first real runs. `national/README.md`'s "Station-density
tiering" section documents how AU's parameters were found (widen reach
for sparse coverage; leave `thresh` alone — lowering it made a dense
local cluster false-trigger).

## Hawaii-specific choices

| knob | value | why |
|---|---|---|
| `grdlat` / `grdlon` | 18.5-22.5N, 160.6-154.5W | whole main chain + Loihi |
| `grdz` / `zrange` | 0-60 km | Big Island mantle fault zones, Pahala deep swarm (~30-45 km) |
| reach (`dspace`/`rstack`/`r_max`) | 10 / 300 / 400 km | AU dense tier — Hawaii is compact and densely instrumented |
| velocity model | `velmod iasp91` (active) | matches national/tr; all cutoffs were tuned with it |
| | Hawaii oceanic-island `lay` model (commented) | more correct here (small aperture, oceanic crust) — switch after retuning cutoffs |
| `nearest_quake_dist 5 2` | kept | but Hawaii swarms may need it loosened — see comment in the .d |

## Offline playback (no live messaging needed)

```
scbinder --binder-config regions/hv/binder-hv.d \
         --ep picks_hv.xml --ep-output out_hv.xml \
         --no-inventory-sites   # with an explicit HV site file, see below
```

Picks need to be an EventParameters XML (waveform -> picker -> SCML, or
built from an existing HVO pick catalog). Use `--no-inventory-sites`
with an HV-only site file, or make sure the loaded inventory is HV-only:
hyp.c derives its flat-earth projection origin from the *mean* of the
site table, so stations outside Hawaii skew every location (see
README.md "Site table restricted to the association grid").

## Deploying as a live alias (later)

```
seiscomp alias create scbinder-hv scbinder
mkdir -p ~/.seiscomp/regions/hv
cp regions/hv/binder-hv.d     ~/.seiscomp/regions/hv/
cp regions/hv/scbinder-hv.cfg ~/.seiscomp/
seiscomp enable scbinder-hv
seiscomp start  scbinder-hv
```

`@CONFIGDIR@` in `binder.config` resolves to whichever config directory
the running instance reads from — keep `binder-hv.d` in a matching
`regions/hv/` subdirectory there.
