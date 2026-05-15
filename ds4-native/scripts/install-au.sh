#!/usr/bin/env bash
# install-au.sh -- unzip a CI-produced AU bundle and put it where
# Logic Pro / GarageBand / MainStage will find it.
#
# Usage:
#   ./install-au.sh ULT-SOUND-DS-4M-AU.zip
#   ./install-au.sh                          # tries ./ULT-SOUND-DS-4M-AU.zip
#
# Notes:
#   * GitHub-downloaded files are quarantined by Gatekeeper -- we strip
#     that attribute so Logic doesn't refuse to scan the AU.
#   * The AU is ad-hoc signed by the CI workflow; that's enough for
#     Logic to load it locally but won't satisfy notarisation, which
#     is fine for personal use.

set -euo pipefail

ZIP="${1:-ULT-SOUND-DS-4M-AU.zip}"

if [ ! -f "$ZIP" ]; then
    echo "Zip not found: $ZIP" >&2
    echo "Download from the GitHub Actions run -> Artifacts -> ULT-SOUND-DS-4M-macOS" >&2
    exit 1
fi

DEST="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$DEST"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "Unzipping $ZIP ..."
unzip -q "$ZIP" -d "$TMP"

BUNDLE=$(find "$TMP" -name "*.component" -type d -maxdepth 3 | head -1)
if [ -z "$BUNDLE" ]; then
    echo "No .component found inside $ZIP" >&2
    exit 1
fi

BASE="$(basename "$BUNDLE")"
echo "Installing $BASE -> $DEST/"
rm -rf "$DEST/$BASE"
cp -R "$BUNDLE" "$DEST/"

echo "Stripping quarantine attribute ..."
xattr -dr com.apple.quarantine "$DEST/$BASE" 2>/dev/null || true

echo
echo "Done. Restart Logic Pro to let it re-scan AUs."
echo "Look under: Plug-in Manager -> AU Instruments -> ULT-SOUND Clone -> ULT-SOUND DS-4M"
