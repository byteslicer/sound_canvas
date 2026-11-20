#!/usr/bin/env bash
#
# Build Sound Canvas for macOS (VST3 + Standalone) and stage the bundles in
# dist/macos/. Must be run on macOS with Xcode / command line tools installed.
#
# Override the architectures with e.g.:
#   CMAKE_OSX_ARCHITECTURES=arm64 scripts/build_macos.sh
#   CMAKE_OSX_ARCHITECTURES="arm64;x86_64" scripts/build_macos.sh   # default
#
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if [ "$(uname -s)" != "Darwin" ]; then
    echo "error: build_macos.sh must be run on macOS" >&2
    exit 1
fi

ARCHS="${CMAKE_OSX_ARCHITECTURES:-arm64;x86_64}"
JOBS="$(sysctl -n hw.ncpu 2>/dev/null || nproc)"

echo "=== Building Sound Canvas for macOS (archs: ${ARCHS}) ==="
cmake -B build-macos -G Xcode \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="${ARCHS}" \
    -DBUILD_SOUNDCANVAS_TESTS=OFF
cmake --build build-macos --config Release --parallel "${JOBS}"

mkdir -p dist/macos

stage() {
    local src="$1" dest="$2"
    if [ -e "$src" ]; then
        rm -rf "$dest"
        cp -R "$src" "$dest"
    else
        echo "warning: expected artifact not found: $src" >&2
    fi
}

stage "build-macos/SoundCanvasSynth_artefacts/Release/VST3/Sound Canvas Synth.vst3"       "dist/macos/SoundCanvasSynth.vst3"
stage "build-macos/SoundCanvasSynth_artefacts/Release/Standalone/Sound Canvas Synth.app"  "dist/macos/SoundCanvasSynth.app"
stage "build-macos/SoundCanvasControl_artefacts/Release/VST3/Sound Canvas Control.vst3"     "dist/macos/SoundCanvasControl.vst3"
stage "build-macos/SoundCanvasControl_artefacts/Release/Standalone/Sound Canvas Control.app" "dist/macos/SoundCanvasControl.app"

# Ad-hoc sign the bundles. Without an Apple Developer identity they can't be
# notarized, but ad-hoc signing lets them load on Apple Silicon.
if command -v codesign >/dev/null 2>&1; then
    for bundle in dist/macos/*.vst3 dist/macos/*.app; do
        if [ -e "$bundle" ]; then
            echo "signing: $bundle"
            codesign --force --deep --sign - "$bundle" || true
        fi
    done
fi

echo "=== macOS artifacts staged in dist/macos/ ==="
ls -la dist/macos/
