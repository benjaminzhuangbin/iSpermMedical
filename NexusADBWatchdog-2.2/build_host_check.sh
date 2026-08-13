#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
gcc -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Os -I"$ROOT/include" "$ROOT"/src/*.c -o "$ROOT/release/watchdog-host"
"$ROOT/release/watchdog-host" -h || true
