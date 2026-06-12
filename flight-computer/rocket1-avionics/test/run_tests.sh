#!/usr/bin/env bash
# =============================================================================
# test/run_tests.sh — build and run every host-side test
#   ./run_tests.sh            build + run everything
# Requires: g++ (C++17). Run from the test/ directory.
# =============================================================================
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p out

CXX=${CXX:-g++}
FLAGS="-std=c++17 -Wall -Wextra -Werror -O2"

echo "== build: logic tests =="
$CXX $FLAGS sim_flight.cpp -o out/sim_flight

echo "== build: firmware smoke test (the unmodified .ino against mocks) =="
$CXX $FLAGS -DHOST_TEST -Imocks mocks/mocks.cpp firmware_smoke.cpp \
  -o out/firmware_smoke

echo
echo "== run: logic tests =="
./out/sim_flight out/nominal_sim.csv

echo
echo "== run: firmware smoke tests =="
for mode in ok alt_addr sd_seq sd_fail baro_fail accel_fail; do
  ./out/firmware_smoke "$mode"
done

echo
echo "ALL TESTS PASSED"
