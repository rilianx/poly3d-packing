#!/bin/bash
# Runs all the experiments of the paper, JOBS processes in parallel (one per core; one thread per run).
# usage: JOBS=14 repro/run_experiments.sh [groups...]
#   groups (default all): chess main liu ablation sens tollenaere
#   main        13 instances (Stoyan 2004/2005, Merged1-5) x 10 seeds x 120 s                       130 runs
#   liu         Liu et al. 2015 Experiment1 x 10 seeds x 120 s                                        10 runs
#   ablation    7 hardest instances x 5 variants (no Compact, sigma = 0, greedy by volume, no kick,
#               additive weights) x 10 seeds x 120 s, and Tollenaere et al. 2025 without Compact       350 + 660 runs
#   sens        7 hardest instances x (rho 0.5 %, 2 %; 100 and 2000 passes) x 10 seeds x 120 s       280 runs
#   tollenaere  66 instances of Tollenaere et al. 2025 x 10 seeds x 30 s                             660 runs
#   chess       Chess x 5 seeds: fine 1200 s, coarse 1200 s, coarse 900 s + fine 300 s (multiresolution)
# Total ~42 CPU hours (~3 h with 14 processes). Finished runs (log with "height:") are skipped, so the script can
# be relaunched if interrupted. TDIV=n divides all the times by n (quick test; use another OUT).
# Results in repro/out/runs (OUT=... to change it); then repro/analyze.sh.
set -e
JOBS=${JOBS:-14}
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/.." && pwd)
BIN=${BIN:-$REPO/build-repro/poly3d}
OUT=${OUT:-$HERE/out/runs}
GROUPS_=${*:-chess main liu ablation sens tollenaere}
[ -x "$BIN" ] || { echo "$BIN does not exist (run repro/setup.sh)"; exit 1; }
mkdir -p "$OUT"
phys=$(lscpu -p=Core,Socket 2>/dev/null | grep -v '^#' | sort -u | wc -l)
[ "$phys" -gt 0 ] && [ "$JOBS" -gt "$phys" ] && echo "WARNING: JOBS=$JOBS > physical cores ($phys); the times would not be comparable"
"$HERE/hardware.sh" "$OUT/../hardware.txt" > /dev/null

# job: config|seed|time|instance|options   (log and solution: OUT/config.sSEED.KEY.{log,sol})
run(){
  IFS='|' read -r cfg s t inst opts <<< "$1"
  t=$(( (t + TDIV - 1) / TDIV )); t1=$(( (900 + TDIV - 1) / TDIV ))
  key=$(echo "${inst%.json}" | sed 's|/|__|g; s| |_|g')
  base="$OUT/$cfg.s$s.$key"
  grep -q '^height:' "$base.log" 2>/dev/null && return 0
  if [ "$cfg" = chess_mr ]; then   # coarse, then fine from that solution
    $BIN "$inst" --dec decompositions_C -t $t1 --seed $s -o "$base.stage1.sol" > "$base.stage1.log" 2>&1
    $BIN "$inst" --dec decompositions_t05 --start "$base.stage1.sol" -t $t --seed $s -o "$base.sol" > "$base.log" 2>&1
  else
    $BIN "$inst" -t $t --seed $s $opts -o "$base.sol" > "$base.log" 2>&1
  fi
}
TDIV=${TDIV:-1}
export -f run; export BIN OUT TDIV

HARD=("Stoyan et al. 2004/Example1.json" "Stoyan et al. 2004/Example2.json" "Stoyan et al. 2004/Example3.json"
      "Stoyan et al. 2004/Example4.json" "Stoyan et al. 2004/Example5.json" "Stoyan et al. 2005/Example2.json"
      "Stoyan et al. 2005/Example3.json")
ALL=("${HARD[@]}" "Stoyan et al. 2005/Example1.json" "Egeblad et al. 2009/Merged1.json" "Egeblad et al. 2009/Merged2.json"
     "Egeblad et al. 2009/Merged3.json" "Egeblad et al. 2009/Merged4.json" "Egeblad et al. 2009/Merged5.json")
CHESS="Lamas-Fernandez, C. et al. 2022/Chess.json"
T25=("Tollenaere, J. et al. 2025"/Instances/*/*.json)
has(){ [[ " $GROUPS_ " == *" $1 "* ]]; }

cd "$REPO/benchs"
LIST="$OUT/../jobs.txt"
{ # long runs first (better balance between processes)
if has chess; then for s in 1 2 3 4 5; do
  echo "chess_mr|$s|300|$CHESS|"
  echo "chess_fine|$s|1200|$CHESS|--dec decompositions_t05"
  echo "chess_coarse|$s|1200|$CHESS|--dec decompositions_C"
done; fi
if has main; then for s in $(seq 1 10); do for f in "${ALL[@]}"; do echo "full|$s|120|$f|"; done; done; fi
if has liu; then for s in $(seq 1 10); do echo "full|$s|120|Liu et al. 2015/Experiment1.json|"; done; fi
if has ablation; then for s in $(seq 1 10); do for f in "${HARD[@]}"; do
  echo "nocompact|$s|120|$f|--no_compact"; echo "slide0|$s|120|$f|--slide 0"
  echo "volgreedy|$s|120|$f|--alpha 1000 --beta 1 --gamma 0"; echo "nokick|$s|120|$f|--no_kick"
  echo "additive|$s|120|$f|--additive"
done; done; fi
if has sens; then for s in $(seq 1 10); do for f in "${HARD[@]}"; do
  echo "rho05|$s|120|$f|--rho 0.005"; echo "rho2|$s|120|$f|--rho 0.02"
  echo "pass100|$s|120|$f|--passes 100"; echo "pass2000|$s|120|$f|--passes 2000"
done; done; fi
if has tollenaere; then for s in $(seq 1 10); do for f in "${T25[@]}"; do echo "full|$s|30|$f|"; done; done; fi
if has ablation; then for s in $(seq 1 10); do for f in "${T25[@]}"; do echo "nocompact|$s|30|$f|--no_compact"; done; done; fi
} > "$LIST"
echo "$(wc -l < "$LIST") runs ($GROUPS_), $JOBS in parallel; logs in $OUT"
start=$(date +%s)
xargs -P "$JOBS" -d '\n' -I{} bash -c 'run "$1"' _ {} < "$LIST"
echo "finished in $(( ($(date +%s)-start)/60 )) min. next step: $HERE/analyze.sh"
