# scbinder-tr — Turkey / KOERI (network code KO)

Single scbinder instance covering Turkey and its seismically-relevant
margins (Aegean, Cyprus, Caucasus/Iran border) for testing against
Kandilli Observatory (KOERI, network code `KO`) data. One instance is
enough here — the grid spans 9 deg of latitude (34-43N), inside the
~10-12 deg limit noted in BINDER_NOTES.md/README.md for a single
flat-earth projection, unlike Australia's `national/` setup which needed
9 tiles for its ~35 deg span.

**Status: untuned.** `binder-tr.d`'s reach parameters (`dspace`, `rstack`,
`r_max`) are borrowed from the AU "moderate" density tier as a plausible
starting point — they have not been validated against real KO station
density or a real Turkish/Aegean event the way the AU tiles were (see
`national/README.md`). Expect to retune after the first real test runs;
that README's "Station-density tiering" section documents how AU's
sparse-tile parameters were found (widen reach for sparse coverage,
leave `thresh` alone — lowering `thresh` instead made a dense local
cluster false-trigger).

## Deploying

```
seiscomp alias create scbinder-tr scbinder
cp regions/tr/binder-tr.d      ~/.seiscomp/regions/tr/   # or etc/regions/tr/ system-wide
cp regions/tr/scbinder-tr.cfg  ~/.seiscomp/               # or etc/ system-wide
seiscomp enable scbinder-tr
seiscomp start scbinder-tr
```

`@CONFIGDIR@` in `binder.config` resolves to whichever config directory
the running instance reads from — put `binder-tr.d` in a matching
`regions/tr/` subdirectory there.

## Offline testing (no live messaging needed)

For a first pass against downloaded KOERI data (e.g. `koeri_fdsnws.mseed`
+ picks from it), skip the alias/messaging setup entirely and use the
`--ep` replay mode documented in the top-level README:

```
scbinder --binder-config regions/tr/binder-tr.d --ep picks_tr.xml --ep-output out_tr.xml
```

Picks need converting to EventParameters XML first (waveform → picker →
SCML, or hand-build from an existing KOERI pick catalog). Use
`--no-inventory-sites` with an explicit KO site file if you want
byte-faithful behavior without pulling a full global inventory into the
site-table mean (see README.md's "Site table restricted to the
association grid" note — hyp.c derives its projection origin from the
site table mean, so extra stations outside the region skew every
location).
