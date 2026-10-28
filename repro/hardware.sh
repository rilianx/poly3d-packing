#!/bin/bash
# Records the hardware and software of the machine.
# usage: repro/hardware.sh [file]   (default repro/out/hardware.txt)
OUT=${1:-$(dirname "$0")/out/hardware.txt}
mkdir -p "$(dirname "$OUT")"
{
echo "# date: $(date -Is)"
echo "# host: $(hostname)"
echo; echo "## CPU (lscpu)"; lscpu 2>/dev/null
echo; echo "## cores"
echo "logical (nproc): $(nproc)"
phys=$(lscpu -p=Core,Socket 2>/dev/null | grep -v '^#' | sort -u | wc -l)
echo "physical (distinct core,socket): $phys"
echo; echo "## frequency per core (MHz, now)"; grep MHz /proc/cpuinfo | awk '{print $4}' | sort -n | uniq -c
for g in /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor /sys/devices/system/cpu/intel_pstate/no_turbo /sys/devices/system/cpu/cpufreq/boost; do
  [ -r "$g" ] && echo "$g: $(cat $g)"
done
echo; echo "## memory"; head -3 /proc/meminfo; free -h 2>/dev/null
echo; echo "## system"; cat /etc/os-release 2>/dev/null | grep -E '^(PRETTY_NAME|VERSION)='; uname -a
echo; echo "## current load"; uptime
echo; echo "## compilers and tools"
g++ --version 2>/dev/null | head -1; cmake --version 2>/dev/null | head -1; python3 --version 2>/dev/null
echo; echo "## repository"; git -C "$(dirname "$0")" log -1 --format='%H %s' 2>/dev/null
git -C "$(dirname "$0")" status --short 2>/dev/null | head
} > "$OUT" 2>&1
echo "hardware in $OUT"
grep -E '^(Model name|Socket|Core|Thread|CPU max MHz|CPU\(s\)|logical|physical)' "$OUT"
