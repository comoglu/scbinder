#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Mustafa Çomoğlu
"""Summarize sweep outputs: final state of the largest quake per variant."""
import math
import sys
import xml.etree.ElementTree as ET

REF_LAT, REF_LON = -37.4144, 146.2608   # scanloc, 179 phases
BASE = '/home/ubuntu/Projects/scbinder/harness/sweep'


def ln(t):
    return t.rsplit('}', 1)[-1]


def val(el, *names):
    cur = el
    for n in names:
        cur = next((c for c in cur if ln(c.tag) == n), None)
        if cur is None:
            return None
    return cur.text


def main():
    print(f"{'variant':<10} {'qid':>4} {'lat':>9} {'lon':>9} {'z':>5} "
          f"{'nph':>4} {'rms':>5} {'gap':>4} {'dist_km':>8}")
    for v in sys.argv[1:]:
        quakes = {}
        for org in ET.parse(f'{BASE}/{v}.out.xml').getroot().iter():
            if ln(org.tag) != 'origin':
                continue
            qid = None
            for c in org:
                if ln(c.tag) == 'comment' and val(c, 'id') is None:
                    pass
            for c in org:
                if ln(c.tag) == 'comment':
                    cid = next((x.text for x in c if ln(x.tag) == 'id'), None)
                    if cid == 'binder:quake':
                        qid = int(next(x.text for x in c if ln(x.tag) == 'text'))
            rec = {
                'lat': float(val(org, 'latitude', 'value')),
                'lon': float(val(org, 'longitude', 'value')),
                'z':   float(val(org, 'depth', 'value')),
                'nph': int(val(org, 'quality', 'usedPhaseCount') or 0),
                'rms': float(val(org, 'quality', 'standardError') or 0),
                'gap': float(val(org, 'quality', 'azimuthalGap') or 0),
            }
            quakes.setdefault(qid, []).append(rec)

        if not quakes:
            print(f"{v:<10} no quakes")
            continue
        qid, hist = max(quakes.items(), key=lambda kv: kv[1][-1]['nph'])
        f = hist[-1]
        dlat = (f['lat'] - REF_LAT) * 111.195
        dlon = (f['lon'] - REF_LON) * 111.195 * math.cos(math.radians(REF_LAT))
        dist = math.hypot(dlat, dlon)
        print(f"{v:<10} {qid:>4} {f['lat']:>9.4f} {f['lon']:>9.4f} "
              f"{f['z']:>5.1f} {f['nph']:>4} {f['rms']:>5.2f} {f['gap']:>4.0f} "
              f"{dist:>8.1f}")


if __name__ == '__main__':
    main()
