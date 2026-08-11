#!/usr/bin/env bash
# Build and run every unit test for the JUCE-independent DSP modules.
# Self-contained: needs only a C++17 compiler, no JUCE.
set -euo pipefail

cd "$(dirname "$0")"

# All modules are header-only; each test is a single translation unit.
TESTS=(
    "SourceGen:test_SourceGen.cpp"
    "GrainEngine:test_GrainEngine.cpp"
    "StepSequencer:test_StepSequencer.cpp"
    "Evolver:test_Evolver.cpp"
)

CXX="${CXX:-g++}"
CXXFLAGS="-std=c++17 -O2 -Wall -Wextra"

mkdir -p bin
PASS=0
FAIL=0

for entry in "${TESTS[@]}"; do
    name="${entry%%:*}"
    files="${entry##*:}"
    out="bin/test_${name}"
    echo "--- building test_${name} ---"
    # shellcheck disable=SC2086  # we *want* word-splitting on $files
    $CXX $CXXFLAGS $files -o "$out"
    if "$out"; then PASS=$((PASS+1)); else FAIL=$((FAIL+1)); fi
    echo ""
done

echo "================================="
echo "PASS: $PASS    FAIL: $FAIL"
echo "================================="
[ "$FAIL" -eq 0 ]
