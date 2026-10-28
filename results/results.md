# Results of the runs of the paper (2× Xeon E5-2620 v4, 14 parallel runs)

- Model name: Intel(R) Xeon(R) CPU E5-2620 v4 @ 2.10GHz
- Socket(s): 2
- Core(s) per socket: 8
- Thread(s) per core: 1
- CPU max MHz: 3000.0000

## Main table (120 s, 10 seeds)

Infeasible: according to the solver; checked: feasible according to tools/check_solution.py.

| Instance | n | Mean | Best | Std. dev. | Infeasible | Checked |
|---|---|---|---|---|---|---|
| Stoyan et al. 2005/Example1 | 10 | 17.47 | 17.46 | 0.01 | 0 | 10/10 |
| Stoyan et al. 2005/Example2 | 10 | 19.16 | 19.14 | 0.01 | 0 | 10/10 |
| Stoyan et al. 2005/Example3 | 10 | 27.45 | 27.05 | 0.39 | 0 | 10/10 |
| Stoyan et al. 2004/Example1 | 10 | 53.08 | 52.10 | 0.61 | 0 | 10/10 |
| Stoyan et al. 2004/Example2 | 10 | 29.00 | 29.00 | 0.00 | 0 | 10/10 |
| Stoyan et al. 2004/Example3 | 10 | 40.00 | 39.13 | 0.49 | 0 | 10/10 |
| Stoyan et al. 2004/Example4 | 10 | 53.32 | 52.15 | 1.00 | 0 | 10/10 |
| Stoyan et al. 2004/Example5 | 10 | 66.96 | 65.13 | 1.04 | 0 | 10/10 |
| Egeblad et al. 2009/Merged1 | 10 | 16.70 | 16.56 | 0.07 | 0 | 10/10 |
| Egeblad et al. 2009/Merged2 | 10 | 21.10 | 21.01 | 0.11 | 0 | 10/10 |
| Egeblad et al. 2009/Merged3 | 10 | 22.60 | 22.01 | 0.37 | 0 | 10/10 |
| Egeblad et al. 2009/Merged4 | 10 | 24.93 | 24.12 | 0.63 | 0 | 10/10 |
| Egeblad et al. 2009/Merged5 | 10 | 27.93 | 27.07 | 0.30 | 0 | 10/10 |
| Liu et al. 2015/Experiment1 | 10 | 30.22 | 30.00 | 0.22 | 0 | 10/10 |

## Component analysis (7 hardest instances, 120 s, 10 seeds)

Gap: mean gap with respect to the best published height. Change: mean relative change of the height with respect to the proposed method over the paired runs (instance, seed); p: Wilcoxon signed-rank test.

| Configuration | Gap S2004 Ex2-5 | Gap S2005 Ex2-3 | Change | p | n |
|---|---|---|---|---|---|
| Proposed method | -2.9% | -2.0% | -- | -- | 70 |
| Without Improve | -2.6% | -1.6% | +0.4% | 0.095 | 70 |
| sigma = 0 | -2.3% | -1.7% | +0.5% | 0.190 | 70 |
| Greedy by volume | -2.9% | -1.4% | +0.3% | 0.216 | 70 |
| No kick | -2.6% | -2.1% | +0.2% | 0.294 | 70 |
| Additive GLS weights | -1.4% | -0.8% | +1.4% | <0.001 | 70 |
| Time limit 24 s | -0.1% | -1.2% | +2.3% | <0.001 | 70 |
| rho = 0.5 % | -2.8% | -2.1% | +0.1% | 0.808 | 70 |
| rho = 2 % | -2.2% | -1.5% | +0.4% | 0.060 | 70 |
| 100 passes | -1.1% | -1.1% | +1.8% | <0.001 | 70 |
| 2000 passes | -2.0% | -1.2% | +0.7% | 0.008 | 70 |

## Merged: mean utilization over time (3DNest at 600 s: 43.0, 42.9, 43.3, 42.0, 41.0 %)

| Instance | 1 s | 10 s | 60 s | 120 s |
|---|---|---|---|---|
| Merged1 | 41.2 | 44.1 | 44.7 | 44.9 |
| Merged2 | 41.9 | 43.1 | 44.7 | 44.8 |
| Merged3 | 40.5 | 44.9 | 47.5 | 47.9 |
| Merged4 | 39.1 | 43.8 | 46.8 | 47.8 |
| Merged5 | 39.0 | 43.1 | 45.9 | 45.9 |

## Tollenaere et al. 2025 (30 s, 10 seeds), proposed method

| Series | Instances | Optima (T2025) | Optima reached (best run) | Optima reached (every run) | Open | Open improved | Mean gap (mean) | Mean gap (best) | Infeasible |
|---|---|---|---|---|---|---|---|---|---|
| DIAMONDS | 9 | 4 | 4 | 2 | 5 | 5 | -2.47% | -2.91% | 0 |
| STOYAN 2004 | 19 | 9 | 8 | 3 | 10 | 9 | -2.50% | -3.02% | 0 |
| STOYAN 2005 | 19 | 9 | 7 | 5 | 10 | 9 | -2.43% | -2.92% | 0 |
| TETRAHEDRONS | 19 | 5 | 5 | 2 | 14 | 9 | -1.23% | -1.30% | 0 |
| Total | 66 | 27 | 24 | 12 | 39 | 32 | | | 0 |

## Chess (Lamas-Fernandez et al.: best 131, mean 135.1)

| Decomposition | n | Mean | Best | Infeasible |
|---|---|---|---|---|
| fine (t = 0.05), 1200 s | 5 | 151.17 | 146.25 | 0 |
| coarse (t = 0.2, k-DOP), 1200 s | 5 | 154.99 | 149.91 | 0 |
| coarse 900 s + fine 300 s | 5 | 148.36 | 138.55 | 0 |
