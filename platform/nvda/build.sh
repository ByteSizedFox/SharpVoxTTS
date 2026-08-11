#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

# Architecture to build: "32" or "64" (defaults to 64)
ARCH="${1:-64}"
case "$ARCH" in
  32|32bit|i686)    BIT=32; CXX="${CXX:-i686-w64-mingw32-g++}" ;;
  64|64bit|x86_64)  BIT=64; CXX="${CXX:-x86_64-w64-mingw32-g++}" ;;
  *)
    echo "error: unknown architecture '$ARCH' (use '32' or '64')" >&2
    exit 1
    ;;
esac

# Per-architecture build dir and dll so both can be built in the same checkout
BDIR="build${BIT}"
DLL="sharpvox-${BIT}.dll"
STAGE="${BDIR}/stage"

# Short commit hash used in the addon filename (override with SHORT_HASH=...)
SHORT_HASH="${SHORT_HASH:-$(git rev-parse --short HEAD 2>/dev/null || echo local)}"

# build the dll with the requested cross-compiler
make -j"$(nproc)" CXX="$CXX" BDIR="$BDIR" DLL="$DLL"

rm -rf "$STAGE"
mkdir -p "$STAGE/synthDrivers/sharpvox"

# the driver loads "sharpvox.dll" from the addon dir, so rename on copy
cp "$DLL"                                "$STAGE/synthDrivers/sharpvox/sharpvox.dll"
cp manifest.ini                          "$STAGE/"
cp synthDrivers/sharpvox/__init__.py     "$STAGE/synthDrivers/sharpvox/"

ADDON_NAME="SharpVoxTTS-${BIT}bit-${SHORT_HASH}"
rm -f "$ADDON_NAME.nvda-addon"
(cd "$STAGE" && zip -r "../../$ADDON_NAME.nvda-addon" .)
echo "built $ADDON_NAME.nvda-addon"
