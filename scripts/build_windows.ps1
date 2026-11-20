# Build Sound Canvas for Windows (VST3 + Standalone) and stage the artifacts in
# dist/windows/. Run on Windows with Visual Studio 2022 (or the Build Tools).
$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent (Split-Path -Parent $PSCommandPath)
Set-Location $Root

$Jobs = [Environment]::ProcessorCount

Write-Host "=== Building Sound Canvas for Windows ==="
cmake -B build-windows -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_BUILD_TYPE=Release `
    -DBUILD_SOUNDCANVAS_TESTS=OFF
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build build-windows --config Release --parallel $Jobs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

New-Item -ItemType Directory -Force -Path dist/windows | Out-Null

function Stage($src, $dest) {
    if (Test-Path -LiteralPath $src) {
        if (Test-Path -LiteralPath $dest) { Remove-Item -Recurse -Force -LiteralPath $dest }
        Copy-Item -Recurse -Force -LiteralPath $src -Destination $dest
    } else {
        Write-Warning "expected artifact not found: $src"
    }
}

Stage "build-windows/SoundCanvasSynth_artefacts/Release/VST3/Sound Canvas Synth.vst3"        "dist/windows/SoundCanvasSynth.vst3"
Stage "build-windows/SoundCanvasSynth_artefacts/Release/Standalone/Sound Canvas Synth.exe"   "dist/windows/SoundCanvasSynth.exe"
Stage "build-windows/SoundCanvasControl_artefacts/Release/VST3/Sound Canvas Control.vst3"      "dist/windows/SoundCanvasControl.vst3"
Stage "build-windows/SoundCanvasControl_artefacts/Release/Standalone/Sound Canvas Control.exe" "dist/windows/SoundCanvasControl.exe"

Write-Host "=== Windows artifacts staged in dist/windows/ ==="
Get-ChildItem dist/windows
