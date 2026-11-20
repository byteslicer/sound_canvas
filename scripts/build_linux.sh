#!/usr/bin/env bash
#
# Build Sound Canvas for Linux (VST3 + Standalone) and stage the artifacts in
# dist/linux/.
#
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

JOBS="$(nproc 2>/dev/null || echo 4)"

echo "=== Building Sound Canvas for Linux ==="
cmake -B build-linux \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SOUNDCANVAS_TESTS=OFF
cmake --build build-linux --config Release --parallel "${JOBS}"

mkdir -p dist/linux

stage() {
    local src="$1" dest="$2"
    if [ -e "$src" ]; then
        rm -rf "$dest"
        cp -R "$src" "$dest"
    else
        echo "warning: expected artifact not found: $src" >&2
    fi
}

stage "build-linux/SoundCanvasSynth_artefacts/Release/VST3/Sound Canvas Synth.vst3"       "dist/linux/SoundCanvasSynth.vst3"
stage "build-linux/SoundCanvasSynth_artefacts/Release/Standalone/Sound Canvas Synth"      "dist/linux/SoundCanvasSynth"
stage "build-linux/SoundCanvasControl_artefacts/Release/VST3/Sound Canvas Control.vst3"     "dist/linux/SoundCanvasControl.vst3"
stage "build-linux/SoundCanvasControl_artefacts/Release/Standalone/Sound Canvas Control"    "dist/linux/SoundCanvasControl"

echo "=== Linux artifacts staged in dist/linux/ ==="
ls -la dist/linux/
