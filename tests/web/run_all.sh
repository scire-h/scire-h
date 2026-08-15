#!/usr/bin/env bash
# Run every unit test for the ZURE web app.
# These tests are self-contained -- they need only Node (no browser, no
# npm install). Each one slices the code it exercises straight out of
# zure.html, so there is never a second copy to drift out of sync.
set -euo pipefail

cd "$(dirname "$0")"

command -v node >/dev/null || { echo "node not found"; exit 1; }

TESTS=(
    "tempo:test_tempo.js"       # timing math + catch-up/glide state machine
    "voices:test_voices.js"     # 808/909 voices against a strict Web Audio stub
    "recorder:test_recorder.js" # WAV/AIFF encoders re-parsed with independent readers
)

PASS=0
FAIL=0

for entry in "${TESTS[@]}"; do
    name="${entry%%:*}"
    file="${entry##*:}"
    echo "--- running test_${name} ---"
    if node "$file"; then
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
