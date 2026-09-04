#!/usr/bin/env python3
"""Sanity-check STL files for printability problems that slicers report as
"floating geometry": disconnected shells, and open (non-manifold) edges.

Pure Python, no dependencies. Usage:

    python3 check_stl.py stl/*.stl        # report on each file
    python3 check_stl.py --strict ...      # exit 1 if any file has >1 shell

Two triangles are considered connected when they share a vertex (coordinates
compared after rounding to 1e-4 mm). Each shell is reported with its bounding
box and volume so a stray fragment is easy to locate in the model.
"""
import struct
import sys


def read_stl(path):
    """Return a list of triangles, each a tuple of three (x, y, z) tuples."""
    with open(path, "rb") as f:
        data = f.read()
    tris = []
    if data[:5] == b"solid" and b"facet" in data[:1024]:
        verts = []
        for line in data.decode("ascii", "replace").splitlines():
            parts = line.split()
            if len(parts) == 4 and parts[0] == "vertex":
                verts.append((float(parts[1]), float(parts[2]), float(parts[3])))
                if len(verts) == 3:
                    tris.append(tuple(verts))
                    verts = []
    else:
        (n,) = struct.unpack_from("<I", data, 80)
        off = 84
        for _ in range(n):
            vals = struct.unpack_from("<12f", data, off)
            tris.append(((vals[3], vals[4], vals[5]), (vals[6], vals[7], vals[8]), (vals[9], vals[10], vals[11])))
            off += 50
    return tris


def key(v):
    return (round(v[0], 4), round(v[1], 4), round(v[2], 4))


class DSU:
    def __init__(self, n):
        self.p = list(range(n))

    def find(self, a):
        while self.p[a] != a:
            self.p[a] = self.p[self.p[a]]
            a = self.p[a]
        return a

    def union(self, a, b):
        a, b = self.find(a), self.find(b)
        if a != b:
            self.p[b] = a


def analyse(tris):
    # vertex ids
    ids = {}
    tri_ids = []
    for t in tris:
        tri_ids.append(tuple(ids.setdefault(key(v), len(ids)) for v in t))
    dsu = DSU(len(ids))
    for a, b, c in tri_ids:
        dsu.union(a, b)
        dsu.union(a, c)
    # edge use count (manifold check: every edge should be used exactly twice)
    edges = {}
    for a, b, c in tri_ids:
        for e in ((a, b), (b, c), (c, a)):
            e = (min(e), max(e))
            edges[e] = edges.get(e, 0) + 1
    open_edges = sum(1 for n in edges.values() if n != 2)
    # per-shell stats
    shells = {}
    for t, (a, b, c) in zip(tris, tri_ids):
        root = dsu.find(a)
        s = shells.setdefault(root, {"n": 0, "vol": 0.0, "lo": [1e9] * 3, "hi": [-1e9] * 3})
        s["n"] += 1
        p, q, r = t
        # signed volume of the tetrahedron (origin, p, q, r)
        s["vol"] += (
            p[0] * (q[1] * r[2] - q[2] * r[1])
            - p[1] * (q[0] * r[2] - q[2] * r[0])
            + p[2] * (q[0] * r[1] - q[1] * r[0])
        ) / 6.0
        for v in t:
            for i in range(3):
                s["lo"][i] = min(s["lo"][i], v[i])
                s["hi"][i] = max(s["hi"][i], v[i])
    return sorted(shells.values(), key=lambda s: -abs(s["vol"])), open_edges


def main(argv):
    strict = "--strict" in argv
    paths = [a for a in argv if not a.startswith("--")]
    bad = 0
    for path in paths:
        tris = read_stl(path)
        shells, open_edges = analyse(tris)
        flag = "" if len(shells) == 1 and open_edges == 0 else "  <-- CHECK"
        print(f"{path}: {len(tris)} triangles, {len(shells)} shell(s), {open_edges} open edges{flag}")
        if len(shells) > 1 or open_edges:
            bad += 1
            for i, s in enumerate(shells):
                lo, hi = s["lo"], s["hi"]
                print(
                    f"    shell {i}: {s['n']} tris, volume {s['vol']:.1f} mm^3, "
                    f"bbox x {lo[0]:.1f}..{hi[0]:.1f}  y {lo[1]:.1f}..{hi[1]:.1f}  z {lo[2]:.1f}..{hi[2]:.1f}"
                )
    if strict and bad:
        print(f"{bad} file(s) need attention")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
