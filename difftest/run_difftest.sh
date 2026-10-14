#!/bin/bash
##########
#
# run_difftest.sh -- the C <-> Java differential test for SolTime (soltime.cpp vs SolTime.java).
#
# Builds both sides into a scratch directory (default /tmp/soltime_difftest, or $1), runs the
# same queries through each, and compares (compare.pl).  Low priority, per the house rule:  nice,
# plus the idle I/O class where ionice exists (Linux only).
#
######
set -e
here="$(cd "$(dirname "$0")" && pwd)"
work="${1:-/tmp/soltime_difftest}"
mkdir -p "$work/classes"
low=(nice -n 19)
if command -v ionice > /dev/null 2>&1; then low+=(ionice -c 3); fi

nice -n 19 clang++ -std=c++11 -O0 -Wall -o "$work/difftest" "$here/difftest.cpp" "$here/../soltime.cpp" -lm
nice -n 19 javac --release 8 -Xlint:-options -d "$work/classes" \
    "$here/../android/app/src/main/java/com/lebeli/soltime/SolTime.java" \
    "$here/../android/app/src/main/java/com/lebeli/soltime/SolarCalendar.java" "$here/DiffTest.java"

"${low[@]}" "$work/difftest" "$here/../data/zones.tsv" > "$work/c.txt"
"${low[@]}" java -cp "$work/classes" DiffTest "$here/../data/zones.tsv" > "$work/java.txt"
perl "$here/compare.pl" "$work/c.txt" "$work/java.txt"
