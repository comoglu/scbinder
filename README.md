# scbinder — Earthworm's binder_ew associator as a SeisComP module

`scbinder` is a port of Earthworm's phase associator **binder_ew**
(Carl Johnson's grid-stacking associator, v1.0.43 2024-11-02) to SeisComP.
Networks migrating from Earthworm get the identical association algorithm
inside a SeisComP pipeline; SeisComP users get an open binder-style
associator (the existing options being scautoloc's loosely binder-inspired
GridSearch and gempa's closed-source scanloc).

Earthworm is MIT-licensed. The algorithm core is included **verbatim**:

| Directory | Contents | Origin |
|---|---|---|
| `core/` | grid.c (nucleation), bind.c (association), hyp.c/l1.c/ingelada.c/linear.c (L1 locator), sample.c/assess.c (WLE resampling assessment) | `earthworm/src/seismic_processing/binder_ew/`, unmodified |
| `ewlib/` | kom.c (command parser), tlay.c (layered travel times), site.c (station table), geo_to_km.c, brent.c/mnbrak.c, chron3.c, time_ew.c + headers | `earthworm/src/libsrc/util/` + `include/`, unmodified |
| `app/` | SeisComP `Client::Application`, replaces binder_ew.c | new code |

Keeping `core/` and `ewlib/` byte-identical to upstream Earthworm means
future binder_ew fixes can be pulled in by copying files.

## How the Earthworm plumbing maps

| Earthworm | scbinder |
|---|---|
| transport ring, TYPE_PICK_SCNL in | messaging PICK group, `DataModel::Pick` |
| TYPE_QUAKE2K / TYPE_LINK out | `DataModel::Origin` + `Arrival`s to LOCATION |
| site file (site.c) | SeisComP inventory, fed through verbatim site.c |
| binder_ew.d (kom.c) | same file format — see below |
| quake_id.d / EventIdFile | kept; default `@ROOTDIR@/var/run/scbinder/quake_id.d` |
| logit | SeisComP logging |
| heartbeat/statmgr | SeisComP messaging framework |

Magnitudes: binder never computed any (that was eqproc/hyp2000/localmag
downstream). scamp/scmag take over naturally.

## Configuration

`scbinder.cfg`:

```
# binder_ew-style command file (required)
binder.config = @CONFIGDIR@/binder.d

# optional
binder.eventIdFile = @ROOTDIR@/var/run/scbinder/quake_id.d
binder.defaultPickWeight = 0        # EW quality 0-4 for picks w/o uncertainty
binder.weightThresholds = 0.1, 0.2, 0.4, 0.8   # sec; uncertainty -> weight 0-3
```

`binder.config` is a **binder_ew.d command file** — an existing Earthworm
configuration works unchanged: transport commands (MyModuleId, RingName,
LogFile, HeartbeatInt, GetPicksFrom, BufferRing*) are recognized and
skipped, everything else (grid, model, association, location, assessment
commands) is processed by the verbatim `*_com()` parsers. `site`/`site_file`
commands still work and add to the inventory-derived station table.
See `config/binder_example.d`.

Pick attribute mapping (the one real semantic gap between the systems):

- **weight 0-4**: derived from the SC pick's time uncertainty via
  `binder.weightThresholds`, else `binder.defaultPickWeight`.
- **first motion**: from `Pick.polarity`.
- **amp** (for `s_to_p_amp_ratio`): not yet mapped — leave that command
  unset.

## Build

```
mkdir build && cd build
cmake .. [-DSEISCOMP_ROOT=/opt/seiscomp]
make
```

## Offline playback / validation

```
scbinder --binder-config config/binder.d --ep picks.xml -d mysql://...
```

feeds all picks of an EventParameters XML (e.g. from `scxmldump -P`)
through the associator in time order and logs the resulting origins
without sending anything — the basis for regression against a real
binder_ew run on the same pick set.

First replay (ga2026ndzjfe, 2026-07-05 western Victoria, 70 picks,
generic 3-layer model, 10 km grid): nucleates and incrementally relocates
a cluster at -39.17/143.31 vs the GA solution -38.72/143.28 with a 254°
gap — the machinery works; solution quality tracks model/grid tuning.

## Multi-region / national coverage

scbinder's grid and L1 locator both use a single flat-earth projection
with one fixed `cos(latitude)` scale for the whole grid, so one instance
is a **local/regional** associator — accurate to a continent-sized area
only if that area stays within roughly 10-12 deg of latitude. For wider
coverage (e.g. all of Australia, ~35 deg of latitude), run **multiple
scbinder instances as SeisComP aliases**, one per region, each with its
own `binder.d` grid — not one instance with a huge grid. See
`national/README.md` for a generator script and a working 9-tile
Australia+surrounds layout (regression-tested: a real event reproduced
end to end with a ~10 km residual against the reference network
solution). True teleseismic distances are out of scope regardless of
tiling — that's what a spherical global associator (e.g. scautoloc) is
for; scbinder's phase set (`P S Pn Sn Pg Sg`) and association windows
are built for regional moveouts, not `PKP`/depth phases beyond ~100 deg.

## Status / roadmap

- [x] Core + support libs compile verbatim (zero diff vs Earthworm)
- [x] Messaging mode: PICK in, Origin out (LOCATION)
- [x] Inventory-backed site table, EW "--" location convention
- [x] binder_ew.d config compatibility incl. `@` includes + EventIdFile
- [x] Offline `--ep` playback (`--ep-output` writes the origins as SCML)
- [x] **Regression-verified against real binder_ew** (harness/): the same
      70-pick event through binder_ew (Earthworm rings, startstop) and
      scbinder --ep produces identical nucleations, kills, and final
      associations (68/68 picks, dOT 0.001 s, dEpi 0.00 km).
      Reproduce: `harness/sc2ewpick.py`, `harness/run_binder_ew.sh`,
      `scbinder --ep out/picks_ew_ms.xml --no-inventory-sites`,
      `harness/compare.py`. Fidelity requires: identical site tables
      (--no-inventory-sites + the generated sites_ew.d), ms-truncated
      pick times (TYPE_PICK_SCNL carries only ms).
- [x] Site table restricted to the association grid + margin. This is
      correctness, not tuning: hyp.c derives its flat-earth projection
      origin from the MEAN of the site table, so a worldwide inventory
      skews every location and residual. `--no-inventory-sites` gives
      byte-faithful EW behavior with an EW site file.
- [x] scconfig description XML (installed to etc/descriptions)
- [x] Travel-time backends: verbatim tlay (default) or SeisComP tables
      via `velmod iasp91|ak135|<iface>:<model>` (app/ttime_sc.cpp
      dispatcher; tlay.c symbols renamed at compile time, source verbatim).
      NB: cutoffs (tstack, rmscut_pick, residual_cut) are model-dependent.
- [ ] Side-by-side regression harness vs binder_ew (same picks in both)
- [ ] Amplitude mapping for `s_to_p_amp_ratio`
- [ ] Channel-orientation from inventory dip/azimuth (currently EW
      channel-code heuristics: no_P_on_Horiz, ChannelNumberMap, ...)
- [ ] scconfig description XML
- [ ] binder_max `grid_joint()` (MAXEL, Sheen et al. 2016) as alternate
      nucleation backend (`binder_max/` sources, same adapter)

## Attribution

binder_ew was written by Carl Johnson with decades of contributions from
the Earthworm community (L. Dietz, P. Friberg and others); see the
revision histories preserved in the sources. Earthworm is distributed
under the MIT license (Copyright (c) 2022 Seismic Software).
SeisComP port: Mustafa Comoglu.
