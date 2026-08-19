#!/usr/bin/env bash
# Build armeabi-v7a nexus_su and copy into APK assets + factory/release.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NDK="${ANDROID_NDK_HOME:-${NDK_ROOT:-/home/ubuntu/android/android-ndk-r21e}}"
OUT_ASSETS="$ROOT/app/src/main/assets/native/armeabi-v7a"
OUT_FACTORY="$ROOT/factory"
OUT_RELEASE="$ROOT/release"

if [[ ! -d "$NDK" ]]; then
  echo "ERROR: NDK not found at $NDK" >&2
  exit 1
fi

export NDK_PROJECT_PATH="$ROOT/native/nexus_su"
export NDK_OUT="$ROOT/native/nexus_su/obj"
export NDK_LIBS_OUT="$ROOT/native/nexus_su/libs"
export APP_BUILD_SCRIPT="$ROOT/native/nexus_su/Android.mk"
export NDK_APPLICATION_MK="$ROOT/native/nexus_su/Application.mk"

rm -rf "$NDK_OUT" "$NDK_LIBS_OUT"
"$NDK/ndk-build" -C "$ROOT/native/nexus_su" \
  NDK_PROJECT_PATH="$ROOT/native/nexus_su" \
  APP_BUILD_SCRIPT="$ROOT/native/nexus_su/Android.mk" \
  NDK_APPLICATION_MK="$ROOT/native/nexus_su/Application.mk" \
  NDK_OUT="$NDK_OUT" \
  NDK_LIBS_OUT="$NDK_LIBS_OUT"

BIN="$NDK_LIBS_OUT/armeabi-v7a/nexus_su"
if [[ ! -f "$BIN" ]]; then
  echo "ERROR: build produced no binary at $BIN" >&2
  exit 1
fi

mkdir -p "$OUT_ASSETS" "$OUT_FACTORY" "$OUT_RELEASE"
cp -f "$BIN" "$OUT_ASSETS/nexus_su"
cp -f "$BIN" "$OUT_FACTORY/nexus_su"
cp -f "$BIN" "$OUT_RELEASE/nexus_su"
chmod 755 "$OUT_ASSETS/nexus_su" "$OUT_FACTORY/nexus_su" "$OUT_RELEASE/nexus_su"

file "$BIN" || true
echo "OK: nexus_su -> assets + factory + release"
ls -la "$OUT_ASSETS/nexus_su" "$OUT_FACTORY/nexus_su"
