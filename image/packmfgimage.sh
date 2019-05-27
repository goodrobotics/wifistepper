#!/usr/bin/env bash
set -e

IMAGE="../firmware/mfg-wifistepper/mfg-wifistepper.ino.generic.bin"
OUTPUT="mfgtest.wifistepper.image.bin"

echo "Checking environment"
[ -f "$IMAGE" ] || ( echo "Error: Image does not exist" >&2; exit 1 )

python writeheader.py 0 "$OUTPUT"
python appendimage.py "$IMAGE" "$OUTPUT"

echo "Image file written to $OUTPUT"
echo "Done."

