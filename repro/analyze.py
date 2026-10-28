"""
Tables of the paper from the logs of run_experiments.sh (and the checks of analyze.sh).
usage: analyze.py runs_folder > results.md      (also writes runs_folder/../merged_trajectories.csv)
"""
import re, glob, os, sys, json, csv, statistics as st
from scipy.stats import wilcoxon

D = sys.argv[1]
HERE = os.path.dirname(os.path.abspath(__file__))
BENCHS = os.path.join(HERE, '..', 'benchs')


def parse(f):
    t = open(f).read()
    m = re.search(r'^height: (\S+)', t, re.M)
    if not m: return None
    pts = []
    m0 = re.search(r'^compacted_height \((\S+) s\): (\S+)', t, re.M)
    if m0: pts.append((float(m0.group(1)), float(m0.group(2))))
    for m2 in re.finditer(r'^\[best\] \((\S+) s\): height (\S+)', t, re.M):
        pts.append((float(m2.group(1)), float(m2.group(2))))
    c = f[:-4] + '.check'
    checked = ('feasible: yes' in open(c).read()) if os.path.exists(c) else None
    return {'pts': pts, 'h': float(m.group(1)), 'feas': 'feasible: yes' in t, 'checked': checked, 'file': f,
            'seed': int(re.search(r'\.s(\d+)\.', os.path.basename(f)).group(1))}


def runs(cfg, key):
    return [r for r in (parse(f) for f in sorted(glob.glob('%s/%s.s*.%s.log' % (D, cfg, key)))) if r]


def h_at(r, T):
    """best height found within T seconds"""
    hs = [h for (s, h) in r['pts'] if s <= T]
    return min(hs) if hs else float('nan')


def key(inst): return inst.replace('/', '__').replace(' ', '_')


S04 = ['Stoyan et al. 2004/Example%d' % i for i in range(1, 6)]
S05 = ['Stoyan et al. 2005/Example%d' % i for i in range(1, 4)]
MER = ['Egeblad et al. 2009/Merged%d' % i for i in range(1, 6)]
HARD = S04 + S05[1:]
# best published heights (main table of the paper)
REF = {'Stoyan et al. 2004/Example2': 28.47, 'Stoyan et al. 2004/Example3': 41.30, 'Stoyan et al. 2004/Example4': 54.83,
       'Stoyan et al. 2004/Example5': 72.33, 'Stoyan et al. 2005/Example2': 19.01, 'Stoyan et al. 2005/Example3': 28.83}

print('# Results (%s)\n' % D)
hw = os.path.join(D, '..', 'hardware.txt')
if os.path.exists(hw):
    t = open(hw).read()
    for k in ('Model name', 'Socket\\(s\\)', 'Core\\(s\\) per socket', 'Thread\\(s\\) per core', 'CPU max MHz'):
        m = re.search(r'^%s:\s*(.*)$' % k, t, re.M)
        if m: print('- %s: %s' % (k.replace('\\', ''), m.group(1)))
    print()

print('## Main table (120 s, 10 seeds)\n')
print('Infeasible: according to the solver; checked: feasible according to tools/check_solution.py.\n')
print('| Instance | n | Mean | Best | Std. dev. | Infeasible | Checked |')
print('|---|---|---|---|---|---|---|')
for inst in S05 + S04 + MER + ['Liu et al. 2015/Experiment1']:
    R = runs('full', key(inst))
    if not R: continue
    hs = [r['h'] for r in R]
    c = [r['checked'] for r in R if r['checked'] is not None]
    print('| %s | %d | %.2f | %.2f | %.2f | %d | %d/%d |' % (inst, len(R), st.mean(hs), min(hs), st.pstdev(hs),
          sum(not r['feas'] for r in R), sum(c), len(c)))

print('\n## Component analysis (7 hardest instances, 120 s, 10 seeds)\n')
print('Gap: mean gap with respect to the best published height. Change: mean relative change of the height with '
      'respect to the proposed method over the paired runs (instance, seed); p: Wilcoxon signed-rank test.\n')
print('| Configuration | Gap S2004 Ex2-5 | Gap S2005 Ex2-3 | Change | p | n |')
print('|---|---|---|---|---|---|')
base = {(inst, r['seed']): r for inst in HARD for r in runs('full', key(inst))}
for name, cfg, T in (('Proposed method', 'full', None), ('Without Compact', 'nocompact', None),
                     ('sigma = 0', 'slide0', None), ('Greedy by volume', 'volgreedy', None),
                     ('No kick', 'nokick', None), ('Additive GLS weights', 'additive', None),
                     ('Time limit 24 s', 'full', 24), ('rho = 0.5 %', 'rho05', None), ('rho = 2 %', 'rho2', None),
                     ('100 passes', 'pass100', None), ('2000 passes', 'pass2000', None)):
    gap = {'2004': [], '2005': []}; ch = []; ok = True
    for inst in HARD:
        R = runs(cfg, key(inst))
        if not R: ok = False; break
        hs = [h_at(r, T) if T else r['h'] for r in R]
        if inst in REF: gap['2004' if '2004' in inst else '2005'].append((st.mean(hs)-REF[inst])/REF[inst]*100)
        for r, h in zip(R, hs):
            b = base.get((inst, r['seed']))
            if b: ch.append((h - b['h'])/b['h']*100)
    if not ok: continue
    if cfg == 'full' and T is None: c, p = '--', '--'
    else:
        c = '%+.1f%%' % st.mean(ch)
        pv = wilcoxon(ch).pvalue if any(abs(x) > 1e-12 for x in ch) else 1.0
        p = '<0.001' if pv < 0.001 else '%.3f' % pv
    print('| %s | %+.1f%% | %+.1f%% | %s | %s | %d |' % (name, st.mean(gap['2004']), st.mean(gap['2005']), c, p, len(ch)))

print('\n## Merged: mean utilization over time (3DNest at 600 s: 43.0, 42.9, 43.3, 42.0, 41.0 %)\n')
V1 = 1687.3255   # volume of the pieces of Merged1; Merged i has i times its pieces
print('| Instance | 1 s | 10 s | 60 s | 120 s |')
print('|---|---|---|---|---|')
traj = []
for i, inst in enumerate(MER, 1):
    R = runs('full', key(inst))
    if not R: continue
    W = json.load(open(os.path.join(BENCHS, inst + '.json')))['container']['size-x']
    u = lambda h: 100*i*V1/(W*W*h)
    print('| Merged%d | %s |' % (i, ' | '.join('%.1f' % st.mean(u(h_at(r, T)) for r in R) for T in (1, 10, 60, 120))))
    for r in R:
        best = 1e9
        for t, h in sorted(r['pts']):
            if h < best: best = h; traj.append('Merged%d,%d,%.3f,%.6f' % (i, r['seed'], t, h))
with open(os.path.join(D, '..', 'merged_trajectories.csv'), 'w') as f:
    f.write('instance,seed,time_s,height\n' + '\n'.join(traj) + '\n')

ref = {}
for row in csv.DictReader(open(os.path.join(HERE, 'data', 'tollenaere2025_reference.csv'))):
    ref[(row['series'], int(row['items']))] = (float(row['best_ub']), float(row['best_lb']), row['proven_optimal'] == '1')
for cfg, title in (('full', 'proposed method'), ('nocompact', 'without Compact')):
    T25 = {}
    for f in glob.glob(D + '/%s.s*.Tollenaere*.log' % cfg):
        r = parse(f)
        if not r: continue
        m = re.search(r'Instances__(\w+)__(\d+)_ITEMS', f)
        T25.setdefault((m.group(1).replace('_', ' '), int(m.group(2))), []).append(r)
    if not T25: continue
    print('\n## Tollenaere et al. 2025 (30 s, 10 seeds), %s\n' % title)
    print('| Series | Instances | Optima (T2025) | Optima reached (best run) | Optima reached (every run) | Open | Open improved | Mean gap (mean) | Mean gap (best) | Infeasible |')
    print('|---|---|---|---|---|---|---|---|---|---|')
    tot = [0]*7
    for ser in sorted({k[0] for k in T25}):
        ks = sorted(k for k in T25 if k[0] == ser)
        n = len(ks); nopt = reach = every = nopen = impr = inf = 0; gm = []; gb = []
        for k in ks:
            ub, lb, opt = ref[k]; hs = [r['h'] for r in T25[k]]; b = min(hs)
            gm.append((st.mean(hs)-ub)/ub*100); gb.append((b-ub)/ub*100); inf += sum(not r['feas'] for r in T25[k])
            if opt: nopt += 1; reach += b <= ub + 0.005; every += max(hs) <= ub + 0.005
            else: nopen += 1; impr += b < ub - 0.005
        for j, v in enumerate((n, nopt, reach, every, nopen, impr, inf)): tot[j] += v
        print('| %s | %d | %d | %d | %d | %d | %d | %+.2f%% | %+.2f%% | %d |' % (ser, n, nopt, reach, every, nopen, impr,
              st.mean(gm), st.mean(gb), inf))
    print('| Total | %d | %d | %d | %d | %d | %d | | | %d |' % tuple(tot))

ch = [(name, runs(cfg, key('Lamas-Fernandez, C. et al. 2022/Chess'))) for name, cfg in
      (('fine (t = 0.05), 1200 s', 'chess_fine'), ('coarse (t = 0.2, k-DOP), 1200 s', 'chess_coarse'),
       ('coarse 900 s + fine 300 s', 'chess_mr'))]
if any(R for _, R in ch):
    print('\n## Chess (Lamas-Fernandez et al.: best 131, mean 135.1)\n')
    print('| Decomposition | n | Mean | Best | Infeasible |')
    print('|---|---|---|---|---|')
    for name, R in ch:
        if R:
            hs = [r['h'] for r in R]
            print('| %s | %d | %.2f | %.2f | %d |' % (name, len(R), st.mean(hs), min(hs), sum(not r['feas'] for r in R)))
