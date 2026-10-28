#!/bin/bash
# Prepares everything needed to reproduce the experiments of the paper:
#   1. poly3d and poly3d_check (with Qhull) in <repo>/build-repro
#   2. Python environment in repro/deps/venv (numpy, scipy, trimesh, rtree, coacd)
#   3. Chess instance (ESICUP) and its conservative decompositions (decompositions_t05 and decompositions_C)
#   4. smoke test
# usage: JOBS=14 repro/setup.sh
# requires (Debian/Ubuntu): sudo apt install build-essential cmake git libqhull-dev python3-venv
set -e
JOBS=${JOBS:-14}
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/.." && pwd)
DEPS=$HERE/deps
mkdir -p "$DEPS" "$HERE/out"

echo "== 0. system dependencies"
miss=""
for c in g++ cmake make git python3; do command -v $c >/dev/null || miss="$miss $c"; done
[ -f /usr/include/libqhull_r/libqhull_r.h ] || miss="$miss libqhull-dev"
python3 -c 'import venv, ensurepip' 2>/dev/null || miss="$miss python3-venv"
if [ -n "$miss" ]; then
  echo "missing:$miss"
  echo "install with: sudo apt install build-essential cmake git libqhull-dev python3-venv"
  exit 1
fi
"$HERE/hardware.sh" "$HERE/out/hardware.txt"

echo "== 1. poly3d"
cmake -S "$REPO" -B "$REPO/build-repro" -DCMAKE_BUILD_TYPE=Release | grep -E 'Qhull' || true
cmake --build "$REPO/build-repro" -j $JOBS > "$DEPS/build.log"
grep -q PLP_WITH_QHULL "$REPO/build-repro/CMakeFiles/plp.dir/flags.make" || { echo "built without Qhull"; exit 1; }
(cd "$REPO/build-repro" && ctest > "$DEPS/ctest.log") || { echo "tests failed (see $DEPS/ctest.log)"; exit 1; }
echo "binaries in $REPO/build-repro"

echo "== 2. Python"
[ -x "$DEPS/venv/bin/python" ] || python3 -m venv "$DEPS/venv"
"$DEPS/venv/bin/pip" install -q --upgrade pip
"$DEPS/venv/bin/pip" install -q numpy scipy trimesh rtree coacd==1.0.14
"$DEPS/venv/bin/pip" freeze | grep -iE 'numpy|scipy|trimesh|rtree|coacd' > "$HERE/out/python-packages.txt"

echo "== 3. Chess (Lamas-Fernandez et al. 2022)"
B=$REPO/benchs
CH="Lamas-Fernandez, C. et al. 2022"
[ -f "$B/$CH/Chess.json" ] || bash "$REPO/tools/fetch_lamas.sh"
PIECES=()
for p in bishop king knight pawn queen rook; do PIECES+=("$CH/Chess/classic_$p.stl"); done
dec(){ #folder and options of decompose_mesh.py (skipped if it exists)
  local d=$1; shift
  if [ ! -f "$B/$d/$CH/Chess/classic_rook.obj" ]; then
    "$DEPS/venv/bin/python" "$REPO/tools/decompose_mesh.py" "$B" --dec $d "$@" "${PIECES[@]}"
  fi
}
dec decompositions_t05 --threshold 0.05 --kdop 0 --merge 0   # fine: 6-20 parts per piece
dec decompositions_C --threshold 0.2 --kdop 26 --hullfaces 16 --merge 0   # coarse: 1-4 parts

echo "== 4. smoke test"
cd "$B"
"$REPO/build-repro/poly3d" "Stoyan et al. 2005/Example1.json" -t 3 --seed 1 -o "$HERE/out/smoke.sol" | grep -E '^(feasible|height):'
"$REPO/build-repro/poly3d_check" "Stoyan et al. 2005/Example1.json" -s "$HERE/out/smoke.sol" -q | grep -E '^feasible'
"$DEPS/venv/bin/python" "$REPO/tools/check_solution.py" "Stoyan et al. 2005/Example1.json" "$HERE/out/smoke.sol" . | tail -1
echo "done. next step: JOBS=$JOBS $HERE/run_experiments.sh"
