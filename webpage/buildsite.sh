#!/usr/bin/env bash
set -e

echo "Cleaning environment"
rm -rf build/ publish/

echo "Running jekyll build"
JEKYLL_ENV=production bundle exec jekyll build --config _config.yml,_config_production.yml --trace

echo "Running post-process build"
gulp postprocess
