#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Mustafa Çomoğlu
"""Compare binder_ew ring output with scbinder --ep-output.

Usage: compare.py <ring_capture.log> <scbinder_out.xml> <event.xml>

Matches quake update streams by binder quake id (identical on both sides:
both start from next_id 1) and reports per-update differences in origin
time, epicenter, depth and pick count, plus the final pick-association
sets (via the injection sequence numbers).
"""

import re
import sys
import xml.etree.ElementTree as ET
from datetime import datetime, timezone

KM_PER_DEG = 111.195


def parse_ew(path):
    """-> quakes: qid -> [ {ot, lat, lon, z, rms, npix} ] (incl. kills),
          links: qid -> set(seq)"""
    quakes, links = {}, {}
    expect = None
    for line in open(path, errors='replace'):
        m = re.search(r'Received .*<type:\s*(\d+)>', line)
        if m:
            expect = int(m.group(1))
            continue
        if expect == 101:
            f = line.split()
            if len(f) == 12:
                qid = int(f[2])
                t = datetime.strptime(f[3][:14], '%Y%m%d%H%M%S').replace(
                        tzinfo=timezone.utc).timestamp() + float(f[3][14:])
                quakes.setdefault(qid, []).append({
                    'ot': t, 'lat': float(f[4]), 'lon': float(f[5]),
                    'z': float(f[6]), 'rms': float(f[7]), 'npix': int(f[11])})
        elif expect == 102:
            f = line.split()
            if len(f) == 5:
                qid, seq = int(f[0]), int(f[3])
                for s in links.values():
                    s.discard(seq)          # pick moved to another quake
                links.setdefault(qid, set()).add(seq)
        expect = None
    return quakes, links


def localname(t):
    return t.rsplit('}', 1)[-1]


def iso2epoch(s):
    s = s.rstrip('Z')
    if '.' not in s:
        s += '.0'
    return datetime.strptime(s, '%Y-%m-%dT%H:%M:%S.%f').replace(
        tzinfo=timezone.utc).timestamp()


def pick_seq_map(event_xml):
    picks = []
    for el in ET.parse(event_xml).getroot().iter():
        if localname(el.tag) != 'pick':
            continue
        t = None
        for c in el.iter():
            if localname(c.tag) == 'value':
                t = c.text
                break
        picks.append((t, el.attrib.get('publicID')))
    picks.sort()
    return {pid: i + 1 for i, (_, pid) in enumerate(picks)}


def parse_sc(path, seqmap):
    """-> quakes: qid -> [ {ot, lat, lon, z, rms, npix, seqs} ]"""
    quakes = {}
    for org in ET.parse(path).getroot().iter():
        if localname(org.tag) != 'origin':
            continue
        rec, qid, seqs = {}, None, set()
        for el in org:
            name = localname(el.tag)
            if name == 'time':
                rec['ot'] = iso2epoch(next(c.text for c in el.iter()
                                           if localname(c.tag) == 'value'))
            elif name in ('latitude', 'longitude', 'depth'):
                v = float(next(c.text for c in el.iter()
                               if localname(c.tag) == 'value'))
                rec[{'latitude': 'lat', 'longitude': 'lon', 'depth': 'z'}[name]] = v
            elif name == 'quality':
                for c in el:
                    if localname(c.tag) == 'usedPhaseCount':
                        rec['npix'] = int(c.text)
                    elif localname(c.tag) == 'standardError':
                        rec['rms'] = float(c.text)
            elif name == 'comment':
                cid = txt = None
                for c in el:
                    if localname(c.tag) == 'id':
                        cid = c.text
                    elif localname(c.tag) == 'text':
                        txt = c.text
                if cid == 'binder:quake':
                    qid = int(txt)
            elif name == 'arrival':
                for c in el:
                    if localname(c.tag) == 'pickID':
                        seq = seqmap.get(c.text)
                        if seq:
                            seqs.add(seq)
        rec['seqs'] = seqs
        quakes.setdefault(qid, []).append(rec)
    return quakes


def main():
    cap, scxml, event = sys.argv[1:4]
    ew, ewlinks = parse_ew(cap)
    sc = parse_sc(scxml, pick_seq_map(event))

    # scbinder coalesces the several bndr_quake2k() calls fired while one
    # pick is processed into a single origin, so it has FEWER snapshots
    # than the EW quake2k stream. Semantics compared per quake:
    #   1. every SC snapshot must match an EW snapshot with the same npix
    #      (in order) within tolerance,
    #   2. the final surviving state must match,
    #   3. the final pick-association (link) sets must be identical.
    qids = sorted(set(ew) | set(sc))
    print(f"{'qid':>4} {'state':>9} | {'snapshots':>10} {'final dOT(s)':>12} "
          f"{'dEpi(km)':>8} {'dZ(km)':>7} {'npix EW/SC':>10}  links")
    identical = True
    for qid in qids:
        e_all = ew.get(qid, [])
        e = [u for u in e_all if u['npix'] >= 4]
        s = sc.get(qid, [])
        killed = bool(e_all) and e_all[-1]['npix'] == 0

        # every SC snapshot must match an EW snapshot of the same npix,
        # in order, within tolerance
        def tolmatch(a, b):
            dot = b['ot'] - a['ot']
            depi = (((b['lat']-a['lat'])**2 +
                     ((b['lon']-a['lon'])*0.79)**2) ** .5) * KM_PER_DEG
            return abs(dot) < 0.05 and depi < 1.0 and abs(b['z']-a['z']) < 0.11

        matched, j = 0, 0
        for b in s:
            # in-order search first; binder's npix is not monotonic
            # (dup churn), so fall back to any-order search
            hit = None
            for jj in range(j, len(e)):
                if e[jj]['npix'] == b['npix'] and tolmatch(e[jj], b):
                    hit = jj
                    break
            if hit is None:
                hit = next((jj for jj in range(len(e))
                            if e[jj]['npix'] == b['npix']
                            and tolmatch(e[jj], b)), None)
            if hit is not None:
                matched += 1
                j = hit + 1
        ok = matched == len(s)
        snap = f"{matched}/{len(s)}"

        state = 'killed' if killed else 'survives'
        dot = depi = dz = float('nan')
        npx, linktxt = '', ''

        if killed and not s:
            # transient quake: nucleated and killed while processing one
            # pick - scbinder's per-pick coalescing correctly emits nothing
            state = 'transient'
            linktxt = 'n/a (coalesced away)'
        elif not killed:
            # surviving quake: final state and links must be identical
            if e and s:
                a, b = e[-1], s[-1]
                dot = b['ot'] - a['ot']
                depi = (((b['lat']-a['lat'])**2 +
                         ((b['lon']-a['lon'])*0.79)**2) ** .5) * KM_PER_DEG
                dz = b['z'] - a['z']
                npx = f"{a['npix']}/{b['npix']}"
                ok = ok and abs(dot) < 0.05 and depi < 1.0 and abs(dz) < 0.11 \
                     and a['npix'] == b['npix']
            else:
                ok = False
                npx = f"{e[-1]['npix'] if e else '-'}/{s[-1]['npix'] if s else '-'}"
            ewset = ewlinks.get(qid, set())
            scset = s[-1]['seqs'] if s else set()
            if ewset == scset:
                linktxt = f'identical ({len(ewset)})'
            else:
                ok = False
                linktxt = (f'DIFF EW-only:{sorted(ewset-scset)} '
                           f'SC-only:{sorted(scset-ewset)}')
        else:
            # killed later in life: SC snapshots checked above; final link
            # state is post-mortem on the EW side, not comparable
            linktxt = 'n/a (killed)'

        if not ok:
            identical = False
        print(f"{qid:>4} {state:>9} | {snap:>10} {dot:>12.3f} {depi:>8.2f} "
              f"{dz:>7.2f} {npx:>10}  {linktxt}{'' if ok else '   <-- DIFF'}")

    print()
    print("RESULT: IDENTICAL (within tolerance)" if identical
          else "RESULT: DIFFERENCES FOUND")
    return 0 if identical else 2


if __name__ == '__main__':
    sys.exit(main())
