#!/usr/bin/env bash
# Baut die Website für GitHub Pages:
#   <out>/        Homepage (site/)
#   <out>/app/    Vue-App (app/, gebaut mit Vite)
# Aufruf: scripts/build-pages.sh [Zielordner]   (Standard: _site)
# Voraussetzung: Abhängigkeiten in app/ installiert (npm ci).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/_site}"

case "$OUT" in
  "" | "/" | "$ROOT" | "$ROOT/") echo "Unsicherer Zielordner: '$OUT'" >&2; exit 1 ;;
esac

rm -rf "$OUT"
mkdir -p "$OUT"

cp -R "$ROOT/site/." "$OUT/"
find "$OUT" -name '*.test.js' -delete

(cd "$ROOT/app" && npm run build)
cp -R "$ROOT/app/dist" "$OUT/app"

echo "Website gebaut: $OUT"
