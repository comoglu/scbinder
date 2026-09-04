#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Mustafa Çomoğlu
"""SCML picks -> Earthworm TYPE_PICK_SCNL messages + EW site file.

Usage: sc2ewpick.py <event.xml> <sites_out.d> <picks_out.txt>

Reads picks from an scxmldump EventParameters XML, resolves channel
coordinates from the SeisComP MySQL inventory, and writes:
  * sites_out.d  - kom-format "site" commands for binder_ew (@include)
  * picks_out.txt- one TYPE_PICK_SCNL text message per line, time-sorted:
      8 <modid> <instid> <seq> STA.CHA.NET.LOC <fm><wt> YYYYMMDDHHMMSS.mmm 0 0 0

The pick quality <wt> uses the same uncertainty mapping as scbinder
(binder.weightThresholds default 0.1/0.2/0.4/0.8 s) so both sides see
identical weights. Sequence numbers start at 1 in feed order, matching
scbinder's internal seq, so link comparisons line up 1:1.
"""

import subprocess
import sys
import xml.etree.ElementTree as ET

WEIGHT_THRESHOLDS = [0.1, 0.2, 0.4, 0.8]
MODID  = 1
INSTID = 255   # INST_UNKNOWN


def localname(tag):
    return tag.rsplit('}', 1)[-1]


def find_child(el, name):
    for c in el:
        if localname(c.tag) == name:
            return c
    return None


def child_text(el, *path):
    for name in path:
        el = find_child(el, name)
        if el is None:
            return None
    return el.text


def load_picks(path):
    root = ET.parse(path).getroot()
    picks = []
    for ep in root.iter():
        if localname(ep.tag) != 'pick':
            continue
        t = child_text(ep, 'time', 'value')
        wf = find_child(ep, 'waveformID')
        if t is None or wf is None:
            continue
        unc = child_text(ep, 'time', 'uncertainty')
        pol = child_text(ep, 'polarity')
        picks.append({
            'time': t,
            'unc':  float(unc) if unc is not None else None,
            'pol':  pol,
            'net':  wf.attrib.get('networkCode', ''),
            'sta':  wf.attrib.get('stationCode', ''),
            'cha':  wf.attrib.get('channelCode', ''),
            'loc':  wf.attrib.get('locationCode', ''),
            'id':   ep.attrib.get('publicID', ''),
        })
    picks.sort(key=lambda p: p['time'])
    return picks


def weight(unc):
    if unc is None:
        return 0
    for i, thr in enumerate(WEIGHT_THRESHOLDS):
        if unc <= thr:
            return i
    return 4


def first_motion(pol):
    return {'positive': 'U', 'negative': 'D', 'undecidable': '?'}.get(pol, '?')


def ew_time(iso):
    # 2026-07-05T03:46:57.8774Z -> 20260705034657.877
    date, t = iso.rstrip('Z').split('T')
    hms, frac = (t.split('.') + ['0'])[:2]
    ms = (frac + '000')[:3]
    return date.replace('-', '') + hms.replace(':', '') + '.' + ms


def channel_coords():
    """NSLC -> (lat, lon, elev) from the SeisComP inventory tables."""
    q = ("SELECT N.code, S.code, L.code, C.code, "
         "L.latitude, L.longitude, L.elevation "
         "FROM Network N "
         "JOIN Station S ON S._parent_oid = N._oid "
         "JOIN SensorLocation L ON L._parent_oid = S._oid "
         "JOIN Stream C ON C._parent_oid = L._oid;")
    out = subprocess.run(
        ['mysql', '-u', 'sysop', '-psysop', 'seiscomp', '-N', '-B', '-e', q],
        capture_output=True, text=True, check=True).stdout
    coords = {}
    for line in out.splitlines():
        f = line.split('\t')
        if len(f) != 7:
            continue
        net, sta, loc, cha, lat, lon, elev = f
        coords[(net, sta, loc, cha)] = (float(lat), float(lon), float(elev))
    return coords


def write_ms_truncated(event_xml, out_xml):
    """Copy of the event XML with pick times truncated to milliseconds —
    the precision TYPE_PICK_SCNL can carry — so scbinder --ep sees the
    same input binder_ew does."""
    ET.register_namespace('', 'http://geofon.gfz-potsdam.de/ns/seiscomp3-schema/0.14')
    tree = ET.parse(event_xml)
    for pick in tree.getroot().iter():
        if localname(pick.tag) != 'pick':
            continue
        for tel in pick:
            if localname(tel.tag) != 'time':
                continue
            for v in tel:
                if localname(v.tag) == 'value' and '.' in (v.text or ''):
                    base, frac = v.text.rstrip('Z').split('.')
                    v.text = f"{base}.{(frac + '000')[:3]}Z"
    tree.write(out_xml, xml_declaration=True, encoding='UTF-8')


def main():
    event_xml, sites_out, picks_out = sys.argv[1:4]
    picks = load_picks(event_xml)
    coords = channel_coords()
    write_ms_truncated(event_xml, picks_out.rsplit('.', 1)[0] + '_ms.xml')

    lines, sites, missing = [], {}, []
    seq = 0
    for p in picks:
        key = (p['net'], p['sta'], p['loc'], p['cha'])
        if key not in coords:
            missing.append('.'.join(key))
            continue
        seq += 1
        loc = p['loc'] if p['loc'] else '--'
        scnl = f"{p['sta']}.{p['cha']}.{p['net']}.{loc}"
        lines.append(f"8 {MODID} {INSTID} {seq} {scnl} "
                     f"{first_motion(p['pol'])}{weight(p['unc'])} "
                     f"{ew_time(p['time'])} 0 0 0")
        lat, lon, elev = coords[key]
        sites[scnl] = (f"site {p['sta']} {p['net']} {p['cha']} {loc} "
                       f"{lat:.6f} {lon:.6f} {elev:.1f}")

    with open(sites_out, 'w') as f:
        f.write('\n'.join(sorted(sites.values())) + '\n')
    with open(picks_out, 'w') as f:
        f.write('\n'.join(lines) + '\n')

    print(f"{len(lines)} picks written ({len(sites)} channels)"
          + (f", {len(missing)} skipped (no coords): "
             + ', '.join(sorted(set(missing))[:5]) if missing else ''))


if __name__ == '__main__':
    main()
