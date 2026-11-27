#!/bin/sh
# Rebuilds the public website (GitHub Pages, branch gh-pages) from brawler/: only the files the game loads.
# Run from the repo root: sh brawler/tools/publish_pages.sh
set -e
ROOT=$(git rev-parse --show-toplevel); OUT=$(mktemp -d); cd "$ROOT/brawler"
for f in $(grep -o '<script src="[^"]*"' index.html | sed 's/<script src="//;s/"//') index.html logo.webp intro.webm titlebg.mp4 thumb.jpg; do mkdir -p "$OUT/$(dirname $f)"; cp "$f" "$OUT/$f"; done
touch "$OUT/.nojekyll"; echo "Super Family Smash website. Built from brawler/ at commit $(git rev-parse --short HEAD)." > "$OUT/VERSION.txt"
cd "$OUT"; git init -q -b gh-pages; git add -A
git -c user.name="Claude" -c user.email="noreply@anthropic.com" commit -q -m "Super Family Smash website ($(git -C "$ROOT" rev-parse --short HEAD))"
git push -q -f "$(git -C "$ROOT" remote get-url origin)" gh-pages
echo "published $(du -sh . | cut -f1)"
