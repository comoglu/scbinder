# scbinder-hv — Hawaii / HVO (Hawaiian Volcano Observatory, network code HV)
#
# STATUS: untuned starting point, not yet validated against real HV data.
# Same situation as regions/tr — the grid below is geographic common
# sense, the algorithm section is copied from the AU dense tier
# (national/binder-se.d), which is what was regression-verified against
# binder_ew and tuned on a live AU playback (harness/, national/README.md).
# Hawaii is compact and densely instrumented, so the dense-tier reach
# (dspace/rstack/r_max = 10/300/400 km) is the right starting family;
# expect to retune thresh/tstack/residual_cut/rmscut_pick once you see
# real HV stack behavior. See national/README.md "Station-density
# tiering" for how that tuning was done (widen reach for sparse gaps,
# do NOT lower thresh).

# Log grid stacking attempts
log_stack
hypcode 7

# Crustal model.
#
# Active model is `velmod iasp91` (below), matching national/*.d and
# regions/tr — every association/location cutoff in this project
# (tstack, rmscut_pick, residual_cut) was tuned with iasp91, so a first
# playback should use it too, even though iasp91 (a continental model)
# is a poor fit for an oceanic hot-spot island.
#
# The `lay` lines below are a Hawaii oceanic-island crust (thin volcanic
# pile over oceanic crust, load-thickened Moho ~15-18 km, Klein-1981
# flavour) for tlay. Hawaii's aperture is < ~600 km, well inside the
# flat-earth tlay validity range, so this is both geographically valid
# and more accurate than iasp91 here — switch to it (comment out the
# velmod line) once you have retuned the cutoffs against HV picks.
lay   0.0  3.4
lay   3.0  5.9
lay   8.0  6.8
lay  14.0  7.3
lay  18.0  8.1
psratio 1.75
velmod iasp91

# Association grid: 18.5-22.5N, 160.6-154.5W covers the whole main
# Hawaiian chain (Hawaii I., Maui Nui, Oahu, Kauai) and Loihi to the SE.
# 4 deg of latitude, well inside the ~10-12 deg single-instance limit
# (BINDER_NOTES.md) — no tiling. Depth to 60 km catches the Big Island
# mantle fault zones and the Pahala deep swarm (~30-45 km).
dspace   10.0
grdlat   18.5  22.5
grdlon -160.6 -154.5
grdz      0.0  60.0
rstack  300.0
tstack    0.6
stack   200
thresh   16
focus    20
grid_wt 0 4
grid_wt 1 3
grid_wt 2 2
grid_wt 3 1

# Associating picks with active hypocenters
rAvg_Factor 5.0
taper     0.0  1.0
taper    50.0  1.0
taper   200.0  2.5
taper   400.0  4.5
taper_OT  2.0  1.0
t_dif    -3.0  240.0

# Locating events (L1 locator)
wt 0 1.0
wt 1 0.5
wt 2 0.25
wt 3 0.125
ph P  1.0
ph Pn 0.5
ph Pg 0.5
r_max   400.0
zrange    1.0  60.0
MaxStep  25.0   5.0
MinXYZstep 0.1
MaxIter    4
MaxDeltaRms 1.0001
locate_eq  25   2
locate_eq  50   4
locate_eq  75   8
locate_eq 100  16

# Cull events based on rms; freed picks can reassociate
rmscut_pick 0.6  20
rmscut_pick 0.9  50
rmscut_pick 1.3 100

# Assessing pick groups with resampling (Ellsworth WLE)
assess_pk  6  20
maxtrial   500
maxwt      3
v_halfspace   5.0
residual_cut  3.0  10.0

# Channel heuristics
no_P_on_Horiz
no_S_on_Z
ChannelNumberMap NEZ

# Don't nucleate a second event within 5 km and 2 s of an existing one.
# NB: Hawaii swarms (Pahala, Kilauea summit collapse sequences) put many
# real events inside that window — if a playback merges obvious
# separate events, loosen or drop this.
nearest_quake_dist 5 2
