#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")"

if [[ -x "/Applications/CMake.app/Contents/bin/cmake" ]]; then
  CMAKE="/Applications/CMake.app/Contents/bin/cmake"
elif command -v cmake >/dev/null 2>&1; then
  CMAKE="$(command -v cmake)"
else
  echo "CMake not found. Install CMake.app in /Applications or add cmake to PATH."
  exit 1
fi

ARCH="$(uname -m)"
if [[ "${1:-}" == "--universal" ]]; then
  ARCH="arm64;x86_64"
fi

echo "Using CMake: $CMAKE"
echo "Architectures: $ARCH"

rm -rf build-mac

"$CMAKE" -S . -B build-mac \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="$ARCH"

"$CMAKE" --build build-mac --config Release -j "$(sysctl -n hw.ncpu)"

APP="build-mac/SDNASuperAmp_artefacts/Release/Standalone/SonicDNA SuperAmp.app"
VST3="build-mac/SDNASuperAmp_artefacts/Release/VST3/SonicDNA SuperAmp.vst3"

if [[ -d "$APP" ]]; then
  codesign --force --deep --sign - "$APP" || true
fi
if [[ -d "$VST3" ]]; then
  codesign --force --deep --sign - "$VST3" || true
fi

echo
echo "BUILD COMPLETE"
echo "Standalone: $APP"
echo "VST3:       $VST3"
