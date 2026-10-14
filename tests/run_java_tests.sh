#!/bin/bash
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
work="$(mktemp -d /tmp/soltime-java-tests.XXXXXX)"
trap 'rm -rf "$work"' EXIT
src="$here/../android/app/src/main/java/com/lebeli/soltime"
# BUG_FIX_PLAN_2026-10-05.md FIX-01/FIX-03 — added 2026-10-05 16:43:53 EDT (UTC-04:00).
javac --release 8 -d "$work" "$src/SolTime.java" "$src/SolarCalendar.java" \
    "$src/LocationFixes.java" "$src/Cities.java" "$here/LocationRegression.java" "$here/ReviewRegression.java"
java -cp "$work" com.lebeli.soltime.LocationRegression
java -cp "$work" com.lebeli.soltime.ReviewRegression
