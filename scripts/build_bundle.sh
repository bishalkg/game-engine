#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

app_path="build/JeetersCastle.app"
app_bin="$app_path/Contents/MacOS/JeetersCastle"
frameworks_dir="$app_path/Contents/Frameworks"

cmake --preset default
cmake --build build --preset game
cmake --build build --preset bundle

if [[ ! -x "$app_bin" ]]; then
  echo "Missing app binary: $app_bin" >&2
  exit 1
fi

if [[ ! -d "$frameworks_dir" ]]; then
  echo "Missing frameworks directory: $frameworks_dir" >&2
  exit 1
fi

echo
echo "App binary links:"
otool -L "$app_bin"

echo
echo "Bundled frameworks:"
find "$frameworks_dir" -maxdepth 1 -type f | sort

echo
echo "Deployment targets:"
otool -l "$app_bin" | rg "minos" || true
for f in "$frameworks_dir"/*; do
  [[ -f "$f" ]] || continue
  echo "FILE:$f"
  otool -l "$f" | rg "minos" || true
done
