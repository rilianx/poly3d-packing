#!/usr/bin/env python3
"""
Independent check of a solution (ESICUP format) for convex pieces or unions of convex parts.

It does not use the C++ code of the project. Each piece is a union of convex parts (scipy hulls):
* if benchs/decompositions/<piece path>.obj exists, the parts are its consecutive closed shells
  (or its o/g groups);
* otherwise, the piece is the convex hull of ALL the vertices of its file (OBJ or STL).
The piece is aligned to the minimum of the vertices of its original mesh (itemOrigin = AlignToMinimum). For each pair of
parts of different pieces, an LP finds the deepest point of their intersection (max delta s.t.
A1 x + delta <= b1, A2 x + delta <= b2, with unit normals); if delta > eps the pieces overlap.

usage: check_solution.py instance.json solution.json benchs_folder [eps]
requires: numpy, scipy
"""
import json, sys, os, itertools
import numpy as np
from scipy.spatial import ConvexHull
from scipy.optimize import linprog

inst_file, sol_file, root = sys.argv[1], sys.argv[2], sys.argv[3]
eps = float(sys.argv[4]) if len(sys.argv) > 4 else 1e-6
I = json.load(open(inst_file)); S = json.load(open(sol_file))
W, D = I['container']['size-x'], I['container']['size-y']

def read_obj(path):
    V, F, groups = [], [], [0]
    for l in open(path):
        l = l.split('#')[0].split()
        if not l: continue
        if l[0] == 'v': V.append([float(x) for x in l[1:4]])
        elif l[0] == 'f':
            f = [int(t.split('/')[0]) for t in l[1:]]
            F.append([k-1 if k > 0 else len(V)+k for k in f])
        elif l[0] in ('o', 'g') and len(F) > groups[-1]: groups.append(len(F))
    return np.array(V), F, groups

def read_stl_vertices(path):
    data = open(path, 'rb').read()
    n = int.from_bytes(data[80:84], 'little') if len(data) >= 84 else -1
    if n >= 0 and len(data) == 84 + 50*n:   # binary
        rec = np.frombuffer(data[84:], dtype=np.dtype([('n', '<f4', 3), ('v', '<f4', (3, 3)), ('a', '<u2')]), count=n)
        return rec['v'].reshape(-1, 3).astype(float)
    return np.array([[float(x) for x in l.split()[1:4]] for l in data.decode().splitlines() if l.strip().startswith('vertex')])

def shells(F):
    """splits the faces into consecutive closed shells (each edge used by exactly two faces)"""
    out, cnt, start, open_ = [], {}, 0, 0
    for j, f in enumerate(F):
        for k in range(len(f)):
            e = tuple(sorted((f[k], f[(k+1) % len(f)])))
            c = cnt.get(e, 0) + 1; cnt[e] = c
            open_ += 1 if c == 1 else -1 if c == 2 else 0
        if open_ == 0: out.append((start, j+1)); start = j+1; cnt = {}
    return out if start == len(F) else None

def load_parts(rel):
    dec = root + '/decompositions/' + rel.rsplit('.', 1)[0] + '.obj'
    if os.path.exists(dec):
        V, F, groups = read_obj(dec)
        rng = [(groups[k], groups[k+1] if k+1 < len(groups) else len(F)) for k in range(len(groups))] if len(groups) > 1 else shells(F)
        if rng is None: sys.exit('could not split into parts: ' + dec)
        parts = [V[sorted(set(i for f in F[a:b] for i in f))] for a, b in rng]
    else:
        if rel.lower().endswith('.stl'): V = read_stl_vertices(root + '/' + rel)
        elif rel.lower().endswith('.obj'): V, F, _ = read_obj(root + '/' + rel)
        else: sys.exit('unsupported format: ' + rel)
        parts = [V]
    #the ESICUP position (AlignToMinimum) refers to the minimum of the original mesh
    if rel.lower().endswith('.stl'): M = read_stl_vertices(root + '/' + rel)
    else: M = read_obj(root + '/' + rel)[0]
    lo = M.min(axis=0)
    return [P - lo for P in parts]

pieces = {}
for it in I['item-types']:
    pieces[it['path'].split('/')[-1]] = load_parts(it['path'])

placed = []   # per item: list of parts (transformed vertices)
for it in S['items']:
    M = np.array(it['transformation']['matrix'])
    placed.append([(np.hstack([V, np.ones((len(V), 1))]) @ M)[:, :3] for V in pieces[it['name']]])

outside = max(max(-P.min(), P[:, 0].max() - W, P[:, 1].max() - D) for Q in placed for P in Q)
height = max(P[:, 2].max() for Q in placed for P in Q)

def halfspaces(P):
    E = ConvexHull(P).equations
    n = np.linalg.norm(E[:, :3], axis=1)
    return E[:, :3] / n[:, None], -E[:, 3] / n

H = [[halfspaces(P) for P in Q] for Q in placed]
box = [(np.min([P.min(axis=0) for P in Q], axis=0), np.max([P.max(axis=0) for P in Q], axis=0)) for Q in placed]
worst, pair = 0.0, None
for i, j in itertools.combinations(range(len(placed)), 2):
    if np.any(box[i][1] <= box[j][0]) or np.any(box[j][1] <= box[i][0]): continue
    for (A1, b1) in H[i]:
        for (A2, b2) in H[j]:
            A = np.vstack([np.hstack([A1, np.ones((len(A1), 1))]), np.hstack([A2, np.ones((len(A2), 1))])])
            r = linprog(c=[0, 0, 0, -1], A_ub=A, b_ub=np.concatenate([b1, b2]),
                        bounds=[(None, None)] * 3 + [(None, 1e3)], method='highs')
            if r.status == 0 and r.x[3] > worst: worst, pair = r.x[3], (i, j)

ok = outside <= eps and worst <= eps
print('items: %d' % len(placed))
print('height: %.10g' % height)
print('max_outside: %.3g' % outside)
print('max_intersection_depth: %.3g%s' % (worst, '' if pair is None else '  (pieces %d and %d)' % pair))
print('feasible: %s' % ('yes' if ok else 'no'))
sys.exit(0 if ok else 2)
