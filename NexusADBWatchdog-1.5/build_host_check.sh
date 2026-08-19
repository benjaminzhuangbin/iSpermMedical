#!/usr/bin/env bash
# Host-side sanity compile (NOT the device binary).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
OUT="$ROOT/release/watchdog-host"
mkdir -p "$ROOT/release"
gcc -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Os \
  -I"$ROOT/include" \
  "$ROOT"/src/*.c \
  -o "$OUT"
echo "[OK] Host test binary: $OUT"
"$OUT" -h || true
