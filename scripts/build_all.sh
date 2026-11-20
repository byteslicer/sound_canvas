#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

export CMAKE_BUILD_PARALLEL_LEVEL="$(nproc)"

echo "=== Building Sound Canvas for Linux (VST3, Standalone, & Automated Tests) ==="
cmake -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux --config Release -j"$(nproc)"

mkdir -p dist/linux
if [ -d "build-linux/SoundCanvasSynth_artefacts/Release/VST3/Sound Canvas Synth.vst3" ]; then
    rm -rf "dist/linux/SoundCanvasSynth.vst3"
    cp -r "build-linux/SoundCanvasSynth_artefacts/Release/VST3/Sound Canvas Synth.vst3" "dist/linux/SoundCanvasSynth.vst3"
fi

if [ -f "build-linux/SoundCanvasSynth_artefacts/Release/Standalone/Sound Canvas Synth" ]; then
    cp "build-linux/SoundCanvasSynth_artefacts/Release/Standalone/Sound Canvas Synth" "dist/linux/SoundCanvasSynth"
fi

if [ -d "build-linux/SoundCanvasControl_artefacts/Release/VST3/Sound Canvas Control.vst3" ]; then
    rm -rf "dist/linux/SoundCanvasControl.vst3"
    cp -r "build-linux/SoundCanvasControl_artefacts/Release/VST3/Sound Canvas Control.vst3" "dist/linux/SoundCanvasControl.vst3"
fi

if [ -f "build-linux/SoundCanvasControl_artefacts/Release/Standalone/Sound Canvas Control" ]; then
    cp "build-linux/SoundCanvasControl_artefacts/Release/Standalone/Sound Canvas Control" "dist/linux/SoundCanvasControl"
fi

echo "=== Linux artifacts staged in dist/linux/ ==="
ls -la dist/linux/
