#!/usr/bin/env bash
# Build and run every unit test for the JUCE-independent DSP modules.
# These tests are self-contained -- they don't need JUCE to be installed,
# only a C++17 compiler.
set -euo pipefail

cd "$(dirname "$0")"

# All tests follow the pattern: test_<Module>.cpp + ../Source/<Module>.cpp
# (Header-only modules like TPTSvf are pulled in via #include.)
TESTS=(
    "TriangleCoreVCO:test_TriangleCoreVCO.cpp ../Source/TriangleCoreVCO.cpp"
    "ExpEnvelope:test_ExpEnvelope.cpp ../Source/ExpEnvelope.cpp"
    "OTAVCA:test_OTAVCA.cpp ../Source/OTAVCA.cpp"
    "LFOSchmitt:test_LFOSchmitt.cpp ../Source/LFOSchmitt.cpp"
    "PiezoTrigger:test_PiezoTrigger.cpp ../Source/PiezoTrigger.cpp"
)

mkdir -p bin
PASS=0
FAIL=0

for entry in "${TESTS[@]}"; do
    name="${entry%%:*}"
    files="${entry##*:}"
    out="bin/test_${name}"
    echo "--- building test_${name} ---"
    # shellcheck disable=SC2086  # we *want* word-splitting on $files
    g++ -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter \
        -I../Source $files -o "$out"
    if "$out"; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
    fi
    echo
done

echo "================================="
echo "PASS: $PASS    FAIL: $FAIL"
echo "================================="
[ "$FAIL" -eq 0 ]
