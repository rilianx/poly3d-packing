"""
Conservative convex decomposition of general meshes (the union of the parts contains the piece).

usage: decompose_mesh.py benchs [--threshold 0.1] [--kdop 26] [--dec folder] piece1 [piece2 ...]
Writes benchs/<folder>/<piece without extension>.obj (benchs/decompositions by default) with one group (o) per part.

1. CoACD (Wei et al. 2022) cuts the mesh with planes and takes the convex hull of each chunk. --threshold
   controls the allowed concavity (larger: fewer, coarser parts). Very small gaps remain between neighboring parts
   (up to ~6e-5 of the diagonal in the chess pieces); --offset moves the faces of each part outwards by at least
   --offset (2e-4 of the diagonal by default) to cover them: by scaling the part from its centroid
   (--cover scale, keeps vertices and faces) or by moving each face (--cover offset, tighter).
2. --merge a: in each part, faces whose normals differ by less than a degrees are merged into one (mean normal,
   moved out to the farthest vertex); the resulting part contains the original one and has far fewer faces
   (the nearly coplanar faces of CoACD produce NFPs with thousands of facets).
3. --kdop k (0: no): each hull is replaced by its k-DOP, the intersection of the halfspaces
   d.x <= max_v d.v for k fixed directions (6: box; 14: + vertex diagonals; 26: + edge diagonals),
   if it has fewer faces. The k-DOP contains the hull.
Reports the volume of the union (by sampling) relative to that of the mesh and checks, with interior points of the
mesh, that all of them lie inside the union.
requires: numpy, scipy, trimesh, coacd
"""
import sys, os, argparse
import numpy as np
import trimesh
from scipy.spatial import ConvexHull, HalfspaceIntersection


def kdop_dirs(k):
    d = [(1, 0, 0), (0, 1, 0), (0, 0, 1)]
    if k >= 14: d += [(1, 1, 1), (1, 1, -1), (1, -1, 1), (-1, 1, 1)]
    if k >= 26: d += [(1, 1, 0), (1, -1, 0), (1, 0, 1), (1, 0, -1), (0, 1, 1), (0, 1, -1)]
    d = np.array(d, float)
    d /= np.linalg.norm(d, axis=1)[:, None]
    return np.vstack([d, -d])


def kdop(P, k, m=0):
    """k-DOP of P, with the normals of the m largest faces of its hull as additional directions"""
    D = kdop_dirs(k)
    if m:
        h = ConvexHull(P)
        tri = P[h.simplices]
        area = np.linalg.norm(np.cross(tri[:, 1]-tri[:, 0], tri[:, 2]-tri[:, 0]), axis=1)
        D = np.vstack([D, h.equations[np.argsort(-area)[:m], :3]])
    h = np.max(P @ D.T, axis=0)
    hs = np.hstack([D, -h[:, None]])      # D.x - h <= 0
    c = P.mean(axis=0)
    V = HalfspaceIntersection(hs, c).intersections
    V = V[ConvexHull(V).vertices]
    #check: the vertices of P inside conv(V); otherwise (numerical errors), enlarge V slightly
    F = ConvexHull(V).equations
    viol = (P @ F[:, :3].T + F[:, 3]).max()
    if viol > 0:
        ctr = V.mean(axis=0)
        V = ctr + (V - ctr)*(1 + 2*viol/max(1e-12, np.linalg.norm(V - ctr, axis=1).min()))
    return V


def offset(P, delta):
    """polytope with the faces of conv(P) moved outwards by delta (contains conv(P))"""
    E = ConvexHull(P).equations.copy()
    E[:, 3] -= delta
    V = HalfspaceIntersection(E, P.mean(axis=0)).intersections
    V = V[ConvexHull(V).vertices]
    #check: the vertices of P inside conv(V) (if it fails due to numerical errors, add P)
    F = ConvexHull(V).equations
    if (P @ F[:, :3].T + F[:, 3]).max() > 0:
        Q = np.vstack([V, P]); V = Q[ConvexHull(Q).vertices]
    return V


def scale_cover(P, delta):
    """scales conv(P) from its centroid so that each face moves out by at least delta (same vertices and faces)"""
    h = ConvexHull(P)
    c = P[h.vertices].mean(axis=0)
    dist = -(h.equations[:, :3] @ c + h.equations[:, 3])      # distance from the centroid to each face (> 0)
    s = 1 + delta/max(dist.min(), 1e-12)
    return c + s*(P[h.vertices]-c)


def merge_faces(P, angle):
    """polytope containing conv(P), with faces whose normals differ by less than `angle` degrees merged: each group
    uses its mean normal (area-weighted) moved out to the farthest vertex"""
    h = ConvexHull(P)
    tri = P[h.simplices]
    area = np.linalg.norm(np.cross(tri[:, 1]-tri[:, 0], tri[:, 2]-tri[:, 0]), axis=1)
    N = h.equations[:, :3]
    order = np.argsort(-area)
    cos = np.cos(np.radians(angle))
    groups = []                              # (accumulated normal, area)
    for k in order:
        for g in groups:
            if np.dot(g[0]/np.linalg.norm(g[0]), N[k]) >= cos:
                g[0] += area[k]*N[k]; g[1] += area[k]; break
        else:
            groups.append([area[k]*N[k].copy(), area[k]])
    D = np.array([g[0]/np.linalg.norm(g[0]) for g in groups])
    hgt = np.max(P @ D.T, axis=0)
    V = HalfspaceIntersection(np.hstack([D, -hgt[:, None]]), P[h.vertices].mean(axis=0)).intersections
    V = V[ConvexHull(V).vertices]
    F = ConvexHull(V).equations
    if (P @ F[:, :3].T + F[:, 3]).max() > 1e-9:
        Q = np.vstack([V, P]); V = Q[ConvexHull(Q).vertices]
    return V


def union_volume(parts, n=200000, seed=0):
    allv = np.vstack(parts); lo, hi = allv.min(0), allv.max(0)
    pts = np.random.default_rng(seed).uniform(lo, hi, (n, 3))
    inside = np.zeros(n, bool)
    for P in parts:
        E = ConvexHull(P).equations
        inside |= np.all(pts @ E[:, :3].T + E[:, 3] <= 1e-9, axis=1)
    return inside.mean()*np.prod(hi-lo)


def contained(mesh, parts, n=40000, seed=1):
    """fraction of interior points of the mesh outside the union (must be 0)"""
    pts = trimesh.sample.volume_mesh(mesh, n)
    out = np.ones(len(pts), bool)
    for P in parts:
        E = ConvexHull(P).equations
        out &= ~np.all(pts @ E[:, :3].T + E[:, 3] <= 1e-7, axis=1)
    return out.mean(), len(pts)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('root'); ap.add_argument('pieces', nargs='+')
    ap.add_argument('--threshold', type=float, default=0.1)
    ap.add_argument('--kdop', type=int, default=26)
    ap.add_argument('--offset', type=float, default=2e-4, help='outward offset of the faces, as a fraction of the piece diagonal (covers the CoACD gaps between parts)')
    ap.add_argument('--merge', type=float, default=3.0, help='merge the faces of each part whose normals differ by less than this angle (degrees; 0: no); the resulting part contains the original one')
    ap.add_argument('--cover', choices=['scale', 'offset'], default='scale', help='how each part is enlarged: scaling from the centroid (same vertices) or moving the faces (tighter, more vertices)')
    ap.add_argument('--hullfaces', type=int, default=0, help='k-DOP: additional normals of the m largest faces of the hull')
    ap.add_argument('--dec', default='decompositions', help='output folder under benchs (to keep several resolutions)')
    a = ap.parse_args()
    import coacd
    coacd.set_log_level('error')
    for path in a.pieces:
        mesh = trimesh.load(os.path.join(a.root, path), force='mesh')
        res = coacd.run_coacd(coacd.Mesh(mesh.vertices, mesh.faces), threshold=a.threshold, preprocess_mode='off')
        parts = [np.asarray(p[0], float) for p in res]
        if a.merge > 0:
            parts = [merge_faces(P, a.merge) for P in parts]
        if a.offset > 0:
            diag = np.linalg.norm(mesh.bounds[1]-mesh.bounds[0])
            parts = [offset(P, a.offset*diag) if a.cover == 'offset' else scale_cover(P, a.offset*diag) for P in parts]
        if a.kdop:
            parts = [Q if len(ConvexHull(Q).simplices) < len(ConvexHull(P).simplices) else P for P, Q in ((P, kdop(P, a.kdop, a.hullfaces)) for P in parts)]
        nf = [len(ConvexHull(P).simplices) for P in parts]
        uv = union_volume(parts)
        frac, ns = contained(mesh, parts)
        print('%s: %d parts, faces %d-%d, union %.4g (+%.1f%%), interior points outside the union: %d of %d'
              % (path, len(parts), min(nf), max(nf), uv, 100*(uv/mesh.volume-1), int(round(frac*ns)), ns))
        out = os.path.join(a.root, a.dec, os.path.splitext(path)[0] + '.obj')
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, 'w') as f:
            f.write('# Conservative convex decomposition of "%s" (CoACD threshold %g, offset %g, k-DOP %d + %d faces; tools/decompose_mesh.py)\n'
                    % (path, a.threshold, a.offset, a.kdop, a.hullfaces))
            base = 1
            for k, P in enumerate(parts):
                h = ConvexHull(P)
                idx = sorted(set(h.vertices)); remap = {v: i for i, v in enumerate(idx)}
                f.write('o part%d\n' % (k+1))
                for v in idx: f.write('v %.17g %.17g %.17g\n' % tuple(P[v]))
                for s in h.simplices:
                    # outward orientation
                    a0, b0, c0 = P[s[0]], P[s[1]], P[s[2]]
                    nrm = np.cross(b0-a0, c0-a0)
                    if np.dot(nrm, a0 - P[idx].mean(axis=0)) < 0: s = s[[0, 2, 1]]
                    f.write('f %d %d %d\n' % tuple(remap[v]+base for v in s))
                base += len(idx)


if __name__ == '__main__':
    main()
