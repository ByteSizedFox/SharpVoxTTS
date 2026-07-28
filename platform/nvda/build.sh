#!/usr/bin/env bash
cd "$(dirname "$0")"

ADDON_NAME="sharpvoxTTS"
STAGE="build/stage"

# build the dll
make -j"$(nproc)"

rm -rf "$STAGE"
mkdir -p "$STAGE/synthDrivers/sharpvox"

cp sharpvox.dll                "$STAGE/synthDrivers/sharpvox/"
cp manifest.ini                "$STAGE/"
cp synthDrivers/sharpvox/__init__.py "$STAGE/synthDrivers/sharpvox/"

rm -f "$ADDON_NAME.nvda-addon"
(cd "$STAGE" && zip -r "../../$ADDON_NAME.nvda-addon" .)
