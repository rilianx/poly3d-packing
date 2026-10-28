#!/bin/bash
# Verifies the solutions of the main table with the independent checker (tools/check_solution.py, no C++ code) and writes the tables
# to repro/out/results.md (and the trajectories of Merged to repro/out/merged_trajectories.csv).
# usage: JOBS=14 repro/analyze.sh
set -e
JOBS=${JOBS:-14}
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/.." && pwd)
OUT=${OUT:-$HERE/out/runs}
PY=${PY:-$HERE/deps/venv/bin/python}
cd "$REPO/benchs"
ls "$OUT"/full.s*.Stoyan*.sol "$OUT"/full.s*.Egeblad*.sol "$OUT"/full.s*.Liu*.sol 2>/dev/null | while read s; do
  [ -s "${s%.sol}.check" ] || echo "$s"; done > "$OUT/../check_jobs.txt"
echo "verifying $(wc -l < "$OUT/../check_jobs.txt") solutions"
check(){ s=$1; b=$(basename "$s" .sol); inst=$(echo "$b" | sed 's/^[a-z_0-9]*\.s[0-9]*\.//; s/__/\//g; s/_/ /g').json
         "$PY" "$REPO/tools/check_solution.py" "$inst" "$s" . > "${s%.sol}.check" 2>&1 || true; }
export -f check; export PY REPO
xargs -P "$JOBS" -d '\n' -I{} bash -c 'check "$1"' _ {} < "$OUT/../check_jobs.txt"
"$PY" "$HERE/analyze.py" "$OUT" > "$OUT/../results.md"
echo "tables in $(cd "$OUT/.." && pwd)/results.md"
