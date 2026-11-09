# scbinder-tr — Turkey / KOERI (Kandilli Observatory, network code KO)
#
# STATUS: untuned starting point, not yet validated against real KO data.
# Unlike national/*.d (regression-verified against binder_ew + a live
# missed-event fix, see national/README.md), these numbers are a first
# guess: grid covers Turkey + the Aegean/E. Mediterranean margin, reach
# parameters (dspace/rstack/r_max) borrowed from the AU "moderate" density
# tier as a reasonable starting point pending real testing against KO
# picks. Expect to retune dspace/rstack/thresh/tstack/residual_cut once
# you see real KO stack behavior — see national/README.md's "Station-
# density tiering" section for how that tuning was done for AU (widen
# reach for sparse coverage, don't lower thresh).

# Log grid stacking attempts
log_stack
hypcode 7

# Crustal model — unused while velmod iasp91 is active below; kept as a
# tlay fallback (switch by commenting the velmod line out).
lay   0.0  5.8
lay  20.0  6.5
lay  40.0  8.0
psratio 1.73

# Travel-time backend: Turkey + Aegean margin spans ~1900 km east-west
# (24-46E), well past the ~1000 km threshold where the flat-earth tlay
# model starts to distort — use the global libtau table instead.
velmod iasp91

# Association grid: 34-43N covers mainland Turkey, Aegean Sea, Cyprus
# and the Iran/Caucasus border; 24-46E from the Aegean to the eastern
# border. 9 deg of latitude, well inside the ~10-12 deg single-instance
# accuracy limit (see top-level README.md) - no tiling needed.
dspace   15.0
grdlat   34.0  43.0
grdlon   24.0  46.0
grdz      0.0 150.0
rstack  700.0
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
r_max  1000.0
zrange    1.0 150.0
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

# Don't nucleate a second event within 5 km and 2 s of an existing one
nearest_quake_dist 5 2
