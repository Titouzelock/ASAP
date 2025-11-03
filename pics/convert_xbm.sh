#!/usr/bin/env bash
set -euo pipefail

# Batch convert SVG/PNG icons to 30x30 1-bit XBM.
# Usage:
#   bash pics/convert_xbm.sh              # convert files under pics/
#   bash pics/convert_xbm.sh path/to/dir  # convert in a different folder
#
# Optional env overrides:
#   SIZE=30x30 THRESHOLD=55% bash pics/convert_xbm.sh

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TARGET_DIR="${1:-${script_dir}}"
SIZE="${SIZE:-30x30}"
THRESHOLD="${THRESHOLD:-55%}"

# Pick ImageMagick entrypoint
if command -v magick >/dev/null 2>&1; then
  IM=magick
elif command -v convert >/dev/null 2>&1; then
  IM=convert
else
  echo "Error: ImageMagick (magick/convert) not found in PATH" >&2
  exit 1
fi

shopt -s nullglob
count=0
for f in "${TARGET_DIR}"/*.{svg,SVG,png,PNG}; do
  base="${f%.*}"
  out="${base}.xbm"
  "$IM" "$f" \
    -resize "$SIZE" \
    -gravity center -background none -extent "$SIZE" \
    -colorspace Gray -threshold "$THRESHOLD" -monochrome \
    "xbm:${out}"
  echo "Wrote ${out}"
  count=$((count+1))
done

if [[ $count -eq 0 ]]; then
  echo "No .svg or .png files found under ${TARGET_DIR}" >&2
fi

