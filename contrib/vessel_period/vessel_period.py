#!/usr/bin/env python3
"""Compute a vessel's route circuit time with the server's own timing arithmetic.

WHAT THIS IS FOR. `transports.period` is the route clock: the server takes
`GameTime % period` as the phase of the lap. Nobody recorded where those numbers came from.
This tool runs Transport::GenerateWaypoints' timing half outside the server, so a missing
period can be computed rather than invented and an existing one can be checked.

WHAT IT CANNOT TELL YOU, stated first because the temptation runs the other way. Both
`transports.period` and the `gameobject_template` motion fields are EMULATOR data --
hand-derived numbers in the same hand-built database. Finding that they agree with each
other says only that whoever built that database computed them together. It is NOT evidence
about the client, and no conclusion about Blizzard may be drawn from it.

WHAT WAS ESTABLISHED FROM THE 5.4.8 CLIENT ITSELF, all of it negative, and worth recording
so the next person does not spend the same days on it:

    * TaxiPath.dbc carries From = To = Cost = 0 for every transport path. No cadence there.
    * TransportAnimation.dbc does not contain these vessels. It is the ELEVATOR table --
      of its 113 TransportIDs, 95 of the 98 that resolve are gameobject type 11
      ("Mesa Elevator", "Undervator", "Plunger"). Its id range covers these entries, so
      their absence is a fact and not a range artefact.
    * GameObjects.db2, the client's own per-entry gameobject record (build 18273, 1780 rows,
      ids 80..213367), contains none of them either.
    * Path.db2 / PathNode.db2, the MoP path system, span ids 3032..6891; these vessels use
      taxi paths 241..2600. No overlap.

So the client holds no link from a vessel's entry to a route, and the model does not come
from the entry at all -- the server sends GAMEOBJECT_DISPLAYID outright. What the client
does with a type-15 hull is, on the evidence available here, UNKNOWN. Do not write code that
assumes an answer.

WHAT THE FIT MODE SHOWS. Sweeping speed alone against the stored periods, 21 of 28 land
under 0.5%; sweeping speed and acceleration together, 33 of 35 do. That is a two-parameter
fit to one observation per vessel, so the pair it returns is A motion reproducing that
period, never THE vessel's motion. Treat it as such.

A CAUTION, because it is the thing that actually bites. `period` and the span of the
generated waypoint table (`m_pathTime`) must agree: Transport::Update cycles the clock over
the first and indexes positions by the second. Where they disagree the pose jumps -- at the
built-in 30 yd/s and 1 yd/s^2, Orgrim's Hammer runs a 6.85x mismatch and its pose moves in
137-yard steps. A period computed here is only correct if the server is driven with the same
speed and acceleration it was computed with.

USAGE

    vessel_period.py fit    --dbc <dir> --known <csv>   check the model against known rows
    vessel_period.py period --dbc <dir> --path <id> [--speed S] [--accel A]

`--dbc` is a directory holding TaxiPathNode.dbc (as the extractor writes it). `--known` is
`pathId,period[,name]` per line -- dump it from `transports` joined to
`gameobject_template.data0`.
"""

import argparse
import collections
import math
import os
import struct
import sys

# The server's defaults, and the reason a generic pair fits almost nothing.
DEFAULT_SPEED = 30.0
DEFAULT_ACCEL = 1.0


def read_dbc(path):
    """Return (rows, fieldCount) for a WDBC file, each row a tuple of uint32."""
    with open(path, 'rb') as handle:
        raw = handle.read()
    magic, count, fields, recsize, _ = struct.unpack('<4sIIII', raw[:20])
    if magic != b'WDBC':
        raise SystemExit('%s is not a WDBC file' % path)
    body = raw[20:20 + count * recsize]
    return [struct.unpack('<%dI' % fields, body[i * recsize:(i + 1) * recsize])
            for i in range(count)], fields


def as_float(u):
    return struct.unpack('<f', struct.pack('<I', u))[0]


def load_paths(dbc_dir):
    """pathId -> nodes, ordered by NodeIndex.

    TaxiPathNodeEntry: 0 ID, 1 PathID, 2 NodeIndex, 3 ContinentID, 4-6 x/y/z,
                       7 Flags, 8 Delay, 9 ArrivalEventID, 10 DepartureEventID.
    """
    rows, _ = read_dbc(os.path.join(dbc_dir, 'TaxiPathNode.dbc'))
    paths = collections.defaultdict(list)
    for row in rows:
        paths[row[1]].append(row)
    for nodes in paths.values():
        nodes.sort(key=lambda r: r[2])
    return paths


class Frame(object):
    __slots__ = ('cont', 'x', 'y', 'z', 'flags', 'delay',
                 'from_prev', 'since_stop', 'until_stop', 't_from', 't_to')


def path_time(nodes, speed=DEFAULT_SPEED, accel=DEFAULT_ACCEL):
    """Milliseconds for one circuit -- Transport::GenerateWaypoints, transcribed.

    Returns None for a path too short to walk.
    """
    if len(nodes) < 3:
        return None

    # The server skips the first and last node: they are arrived at by teleport.
    frames = []
    map_change = 0
    for i in range(1, len(nodes) - 1):
        if map_change:
            map_change -= 1
            continue
        if nodes[i][3] != nodes[i + 1][3]:
            map_change = 1
            continue
        f = Frame()
        f.cont = nodes[i][3]
        f.x, f.y, f.z = (as_float(nodes[i][4]), as_float(nodes[i][5]), as_float(nodes[i][6]))
        f.flags, f.delay = nodes[i][7], nodes[i][8]
        f.from_prev = f.since_stop = f.until_stop = f.t_from = f.t_to = 0.0
        frames.append(f)

    if len(frames) < 2:
        return None

    last_stop = 0 if frames[0].flags == 2 else -1
    first_stop = -1

    for i in range(1, len(frames)):
        # BOTH ENDS. Flags == 1 marks the node a vessel teleports FROM, so reading it only
        # on the node being arrived at misses every jump that stays on one continent --
        # where the map id does not change either. Grom'Gol to Undercity has one, 13,685
        # yards with flags 1 -> 0, and counting it as distance travelled makes the circuit
        # 242% too long.
        if (frames[i].flags == 1 or frames[i - 1].flags == 1
                or frames[i].cont != frames[i - 1].cont):
            frames[i].from_prev = 0.0
        else:
            frames[i].from_prev = math.dist(
                (frames[i].x, frames[i].y, frames[i].z),
                (frames[i - 1].x, frames[i - 1].y, frames[i - 1].z))
        if frames[i].flags == 2:
            if first_stop == -1:
                first_stop = i
            last_stop = i

    last_stop = max(last_stop, 0)
    first_stop = max(first_stop, 0)

    run = 0.0
    for i in range(len(frames)):
        j = (i + last_stop) % len(frames)
        run = 0.0 if frames[j].flags == 2 else run + frames[j].from_prev
        frames[j].since_stop = run
    for i in range(len(frames) - 1, -1, -1):
        j = (i + first_stop + 1) % len(frames)
        run += frames[(j + 1) % len(frames)].from_prev
        frames[j].until_stop = run
        if frames[j].flags == 2:
            run = 0.0

    accel_dist = (speed * speed) / (2.0 * accel)   # covered while winding up
    accel_time = speed / accel                     # seconds to reach speed

    for f in frames:
        f.t_from = (math.sqrt(2 * f.since_stop / accel) if f.since_stop < accel_dist
                    else ((f.since_stop - accel_dist) / speed) + accel_time)
        f.t_to = (math.sqrt(2 * f.until_stop / accel) if f.until_stop < accel_dist
                  else ((f.until_stop - accel_dist) / speed) + accel_time)
        f.t_from *= 1000
        f.t_to *= 1000

    total = frames[0].delay * 1000
    accel_ms = accel_time * 1000

    for i in range(len(frames) - 1):
        covered = 0.0
        t_from, t_to = frames[i].t_from, frames[i].t_to
        if covered < frames[i + 1].from_prev and t_to > 0:
            while covered < frames[i + 1].from_prev and t_to > 0:
                t_from += 100
                t_to -= 100
                if t_from < t_to:
                    covered = (0.5 * accel * (t_from / 1000) ** 2 if t_from <= accel_ms
                               else accel_dist + speed * ((t_from - accel_ms) / 1000))
                    covered -= frames[i].since_stop
                else:
                    covered = (0.5 * accel * (t_to / 1000) ** 2 if t_to <= accel_ms
                               else accel_dist + speed * ((t_to - accel_ms) / 1000))
                    covered = frames[i].until_stop - covered
                total += 100
            total -= 100
        total += ((100 - (int(frames[i + 1].t_to) % 100)) if frames[i + 1].t_from > frames[i + 1].t_to
                  else (int(frames[i + 1].t_to) % 100))
        total += frames[i + 1].delay * 1000

    return total


def fit(nodes, target, speeds, accels):
    """The (speed, accel) whose circuit best matches `target`. None when unwalkable."""
    best = None
    for accel in accels:
        for speed in speeds:
            got = path_time(nodes, speed, accel)
            if got is None:
                return None
            err = abs(got - target)
            if best is None or err < best[0]:
                best = (err, speed, accel, got)
    return best


def frange(lo, hi, step):
    out = []
    v = lo
    while v <= hi + 1e-9:
        out.append(round(v, 4))
        v += step
    return out


def cmd_period(args):
    paths = load_paths(args.dbc)
    nodes = paths.get(args.path)
    if not nodes:
        raise SystemExit('no path %d in TaxiPathNode.dbc' % args.path)
    got = path_time(nodes, args.speed, args.accel)
    if got is None:
        raise SystemExit('path %d has too few usable nodes' % args.path)
    print('path %d  speed %.1f  accel %.1f  ->  period %d ms (%.1f min)'
          % (args.path, args.speed, args.accel, got, got / 60000.0))


def cmd_fit(args):
    paths = load_paths(args.dbc)
    speeds = frange(args.speed_min, args.speed_max, args.step)
    accels = frange(args.accel_min, args.accel_max, args.accel_step)

    print('%-7s %-10s %-7s %-7s %-10s %-7s %s'
          % ('path', 'period', 'speed', 'accel', 'computed', 'err%', 'name'))
    print('-' * 78)

    close = 0
    total = 0
    for line in open(args.known):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        parts = [p.strip() for p in line.split(',')]
        pid, period = int(parts[0]), int(parts[1])
        name = parts[2] if len(parts) > 2 else ''
        nodes = paths.get(pid)
        if not nodes:
            print('%-7d %-10d %s' % (pid, period, '(no such path)'))
            continue
        best = fit(nodes, period, speeds, accels)
        if best is None:
            print('%-7d %-10d %s' % (pid, period, '(unwalkable)'))
            continue
        err, speed, accel, got = best
        pct = err / period * 100.0 if period else 0.0
        total += 1
        if pct <= 0.5:
            close += 1
        print('%-7d %-10d %-7.1f %-7.1f %-10d %-7.2f %s'
              % (pid, period, speed, accel, got, pct, name))

    print('-' * 78)
    print('within 0.5%%: %d of %d' % (close, total))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)

    p = sub.add_parser('period', help='compute one path\'s circuit time')
    p.add_argument('--dbc', required=True, help='directory holding TaxiPathNode.dbc')
    p.add_argument('--path', type=int, required=True, help='taxiPathId (gameobject_template.data0)')
    p.add_argument('--speed', type=float, default=DEFAULT_SPEED)
    p.add_argument('--accel', type=float, default=DEFAULT_ACCEL)
    p.set_defaults(func=cmd_period)

    f = sub.add_parser('fit', help='find the motion that reproduces known periods')
    f.add_argument('--dbc', required=True)
    f.add_argument('--known', required=True, help='csv: pathId,period[,name]')
    f.add_argument('--speed-min', type=float, default=1.0)
    f.add_argument('--speed-max', type=float, default=40.0)
    f.add_argument('--step', type=float, default=0.5)
    f.add_argument('--accel-min', type=float, default=1.0)
    f.add_argument('--accel-max', type=float, default=1.0)
    f.add_argument('--accel-step', type=float, default=0.5)
    f.set_defaults(func=cmd_fit)

    args = ap.parse_args()
    args.func(args)


if __name__ == '__main__':
    sys.exit(main())
