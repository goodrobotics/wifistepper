#!/usr/bin/env bash

S3BUCKET="s3://www.wifistepper.com/"
DIST="EAUDF2Y457PMS"
CACHEFILE="cache.txt"

echo "Cleaning environment"
rm -rf $CACHEFILE build/

echo "Running jekyll build"
JEKYLL_ENV=production bundle exec jekyll build --config _config.yml --trace
if [ $? -ne 0 ]; then
    echo "ERROR: Bad jekyll compilation"
    exit 1
fi

echo "Uploading files to server..."
aws s3 sync build/ $S3BUCKET --size-only --include "*.*" --delete --profile wifistepper >> $CACHEFILE
echo "Done uploading"

# Invalidate only modified files
changed=$(grep "upload\|deleted" $CACHEFILE | sed -e "s|.*upload.*to ${S3BUCKET}|/|" | sed -e "s|.*delete: ${S3BUCKET}|/|" | sed -e 's/index.html//' | sed -e 's/\(.*\).html/\1/')
num=$(cat $CACHEFILE | wc -l)
echo "Number of files changed: $num"
if [ $num -eq 0 ]; then
    echo "No changed files, nothing to invalidate"
elif [ $num -lt 10 ]; then
    echo "Using per-page invalidation method"
    echo $changed
    echo $changed | tr '\n' ' ' | xargs aws cloudfront create-invalidation --distribution-id "$DIST" --paths --profile wifistepper
else
    echo "Using bulk invalidation method (all paths)"
    aws cloudfront create-invalidation --distribution-id "$DIST" --paths "/*" --profile wifistepper
fi
