#!/usr/bin/env bash
set -e

IMAGE="../firmware/wifistepper/wifistepper.ino.generic.bin"
DATA="../firmware/wifistepper/data/"

VERSION=1
OUTPUT="wifistepper.image.$VERSION.bin"

echo "Checking environment"
[ -f "$IMAGE" ] || ( echo "Error: Image does not exist" >&2; exit 1 )
[ -d "$DATA" ] || ( echo "Error: Data directory does not exist" >&2; exit 1 )

python writeheader.py $VERSION "$OUTPUT"
python appendimage.py "$IMAGE" "$OUTPUT"
for fname in $(find "$DATA" -type f -printf '%P\n'); do
    python appenddata.py "$DATA$fname" "/$fname" "$OUTPUT"
done
python appendmd5.py "$OUTPUT"

echo "Image file written to $OUTPUT"
echo "Done."

