# poly3d-packing

Overlap-minimization heuristic for the **three-dimensional irregular strip packing problem**: polyhedra
(possibly non-convex, given as unions of convex parts) are placed by translation in a container with a fixed
rectangular base so as to minimize its height.

This repository contains the code, instances, solutions and scripts of the paper

> I. Araya et al. *Fast exact overlap minimization for three-dimensional irregular strip packing of polyhedra.*
> Submitted to the European Journal of Operational Research.

## Method in brief

* The overlap between two convex parts is measured by their **penetration depth** in the no-fit polytope
  N_AB = B ⊕ (−A).
* The best position of a piece along an axis is found **exactly** by evaluating a few breakpoints of a
  piecewise function (the 3D counterpart of the directional penetration depths of Umetani et al., 2009).
* Around this move: a greedy construction, a local improvement (`Improve`), and an oscillation on the height
  where `MinimizeOverlap` (guided local search with multiplicative weights) removes the overlap after each
  decrease; restarts move one random piece to a random position.

## Build

Requirements: a C++11 compiler and CMake ≥ 3.5. No external solver is needed; Qhull (`libqhull-dev`) is
optional (robust hulls for the parts of curved meshes).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
(cd build && ctest)
```

## Usage

Paths inside the instances are relative to `benchs/`:

```bash
cd benchs
../build/poly3d "Stoyan et al. 2005/Example1.json" -t 120 --seed 1 -o sol.json
../build/poly3d_check "Stoyan et al. 2005/Example1.json" -s sol.json
../build/poly3d --help                                  # all options
```

The time limit is CPU time of a single thread. The output uses the ESICUP JSON solution format. The defaults are
the parameters of the paper; the options of the component analysis are `--no_improve`, `--slide 0`,
`--no_kick`, `--additive`, `--rho` and `--passes`.

## Contents

| Folder | |
|---|---|
| `src/` | `Placer` (Descend, Improve, exact axis move, MinimizeOverlap), `Greedy` (construction), `main.cpp` (Algorithm 1) → `poly3d`; `check.cpp` → `poly3d_check` (feasibility check); `test.cpp`; geometry (`ConvexPolytope`, `SmallLP`: the three-variable LPs of Descend), instances and solutions |
| `benchs/` | instances in ESICUP JSON format (Stoyan et al. 2004, 2005; Egeblad & Pisinger 2009; Liu et al. 2015; Tollenaere et al. 2025) and the convex decompositions of the non-convex pieces (`benchs/decompositions`) |
| `tools/` | independent checker in Python (`check_solution.py`), convex decomposition of meshes (`decompose_mesh.py`, `decompose_prism.py`), download of the Chess instance (`fetch_lamas.sh`) |
| `repro/` | scripts that reproduce every experiment of the paper (see `repro/README.md`) |
| `results/` | tables of the runs of the paper (`results.md`), hardware (`hardware.txt`), trajectories of Merged (`merged_trajectories.csv`) and the best solutions (`solutions_final/`) |

The Chess instance of Lamas-Fernandez et al. (2022) is not included because its meshes are under the GNU GPL
with their own attribution; `repro/setup.sh` downloads it from the ESICUP repository and builds its
decompositions.

## Reproducing the experiments

```bash
sudo apt install build-essential cmake git libqhull-dev python3-venv
JOBS=14 repro/setup.sh            # binaries in build-repro/, Python venv, Chess, smoke test
JOBS=14 repro/run_experiments.sh  # ~42 CPU hours (~3 h with 14 processes)
JOBS=14 repro/analyze.sh          # independent check and tables -> repro/out/results.md
```

## License

The code is distributed under the GNU General Public License v3 (`LICENSE`). The instances come from the
[ESICUP datasets](https://github.com/ESICUP/datasets) (`benchs/LICENSE`, `benchs/README_ESICUP.md`).
