# Reproducing the experiments

Scripts that run every experiment of the paper on another machine and regenerate its tables.

## Steps

```bash
# 0. system dependencies (Debian/Ubuntu)
sudo apt install build-essential cmake git libqhull-dev python3-venv

# 1. binaries + Python environment + Chess instance and its decompositions + smoke test (~15 min)
JOBS=14 repro/setup.sh

# 2. (optional) test of the whole pipeline with all the times divided by 60 (~15 min)
JOBS=14 TDIV=60 OUT=$PWD/repro/out/test/runs repro/run_experiments.sh
OUT=$PWD/repro/out/test/runs repro/analyze.sh

# 3. all the experiments (~3 h with 14 processes; inside tmux/screen or with nohup)
JOBS=14 nohup repro/run_experiments.sh > repro/out/run.log 2>&1 &

# 4. independent check of the solutions and tables -> repro/out/results.md
JOBS=14 repro/analyze.sh
```

`run_experiments.sh` takes groups (`chess main liu ablation sens tollenaere`) and skips the finished runs, so it can
be relaunched if interrupted.

| Group | Runs | Time per run | CPU time |
|---|---|---|---|
| main: Stoyan 2004/2005, Merged1–5 | 13 × 10 seeds | 120 s | 4.3 h |
| liu: Liu et al. 2015 Experiment1 | 10 | 120 s | 0.3 h |
| ablation: 7 instances × (no Compact, σ = 0, greedy by volume, no kick, additive weights) | 350 | 120 s | 11.7 h |
| ablation: Tollenaere et al. 2025 without Compact | 66 × 10 | 30 s | 5.5 h |
| sens: 7 instances × (ρ = 0.5 %, 2 %; 100 and 2000 passes) | 280 | 120 s | 9.3 h |
| tollenaere: Tollenaere et al. 2025 | 66 × 10 | 30 s | 5.5 h |
| chess: fine, coarse, coarse + fine | 3 × 5 seeds | 1200 s | 5 h |

Total ≈ 42 CPU hours, ≈ 3 h with 14 processes. The row "time limit 24 s" of the component analysis is taken from
the trajectories of the main runs.

## Notes

* Each run uses one thread and the time limit is CPU time (`clock()`). For comparable times, do not exceed the
  number of **physical** cores (the script warns if `JOBS` does) and do not run other loads at the same time. With
  16 cores, 14 processes leave two for the system.
* `hardware.sh` writes to `out/hardware.txt` the CPU (`lscpu`), physical and logical cores, frequencies, memory,
  operating system, compilers and the commit of the repository; `analyze.py` copies the CPU model to `results.md`.
* `analyze.sh` checks the solutions of the main table with `tools/check_solution.py`, an independent verifier in
  Python (an LP per pair of convex parts) that shares no code with the solver.
* `data/tollenaere2025_reference.csv`: best upper and lower bounds of the MILP models of Tollenaere et al.
  (Tables B.1–B.4 of their article) and whether the optimum is proven.
* The Chess meshes (GNU GPL) are not in the repository: `setup.sh` downloads them from ESICUP
  (`tools/fetch_lamas.sh`) and builds the decompositions with `tools/decompose_mesh.py` (CoACD). Qhull is used for
  the hulls of their parts; on the polyhedral instances it is not used.
* The results of the paper are in `results/` (tables, best solutions and the trajectories of Merged).
