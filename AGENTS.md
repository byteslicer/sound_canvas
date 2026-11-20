# AGENTS.md

Guidance for agents and contributors working in this repository.

## Project

Sound Canvas is an audio-plugin suite built with **JUCE 8** and CMake (C++20).
It builds two plugins, each as **VST3 + Standalone**:

- `SoundCanvasSynth` — image/wavetable-driven synth
- `SoundCanvasControl` — image → MIDI control module rack

## Layout

- `src/core`, `src/synth`, `src/control`, `src/dsp`, `src/gui` — source
- `src/tests/SoundCanvasTests.cpp` — console test suite
- `CMakeLists.txt` — single CMake project for everything
- `scripts/` — per-platform build scripts
- `.github/workflows/` — CI and release automation

JUCE is consumed from `external/JUCE` when present (local dev). `external/` is
git-ignored, so CI falls back to `FetchContent` and downloads **JUCE 8.0.4**.
Do not commit the JUCE tree.

## Build

Artifacts are staged into `dist/<platform>/`.

```bash
# Linux (host)
bash scripts/build_linux.sh            # -> dist/linux

# Linux, also builds the test target
bash scripts/build_all.sh

# macOS (must run on macOS; Xcode/CLT required)
bash scripts/build_macos.sh            # universal arm64;x86_64 -> dist/macos
CMAKE_OSX_ARCHITECTURES=arm64 bash scripts/build_macos.sh   # arm64 only

# Windows (PowerShell; Visual Studio generator auto-detected)
pwsh -File scripts/build_windows.ps1   # -> dist/windows
```

Tests are built when `BUILD_SOUNDCANVAS_TESTS=ON` (the default for non-cross
builds). Run the console suite with an optional snapshot directory:

```bash
build-linux/SoundCanvasTests_artefacts/Release/SoundCanvasTests [snapshotDir]
```

## CI

- `build-macos.yml`, `build-linux.yml`, `build-windows.yml` are **reusable**
  workflows (`workflow_call`) plus manual `workflow_dispatch`. They only build
  and upload a per-platform artifact; they are **not** triggered on push/PR.
- `release.yml` runs on push to `main`.

Runner notes:

- macOS must use `macos-15-intel`. `macos-14`/`macos-latest` (arm64) are not
  supported on this account and `macos-13` is retired. Intel clang still
  cross-compiles `arm64`, so universal binaries work.
- Windows: do **not** hard-code a Visual Studio generator version (the
  `windows-latest` image no longer ships VS 2022). Let CMake auto-detect it and
  pass `-A x64`.

## Releases

Releases are automated from **Conventional Commits** via
[`release-please`](https://github.com/googleapis/release-please) (manifest mode,
config in `release-please-config.json`, state in
`.release-please-manifest.json`).

Flow:

1. Merge changes to `main` using Conventional Commit messages.
2. `release.yml` opens/updates a `chore(main): release X.Y.Z` PR that updates
   `CHANGELOG.md`, `version.txt`, and `CMakeLists.txt`.
3. Merging that PR creates the `vX.Y.Z` tag and GitHub Release; the build jobs
   then run and **attach** the artifacts to that release (plus `SHA256SUMS.txt`).

Commit → bump:

| Commit type | Bump |
| --- | --- |
| `fix:` | patch |
| `feat:` | minor |
| `feat!:` / `BREAKING CHANGE:` | major |
| `chore:` `ci:` `build:` `docs:` `refactor:` `test:` | none |

Version sources (kept in sync by release-please; do not hand-edit):

- `version.txt` — canonical version
- `CMakeLists.txt` — `project(... VERSION x.y.z ...) # x-release-please-version`

JUCE reads `PROJECT_VERSION`, so plugin versions follow automatically.

**Artifacts are only built at release time** (or by manually dispatching a
`build-*.yml` workflow). Current release assets per platform:
`SoundCanvas-{macOS,linux,windows}-vX.Y.Z.zip`.

## Conventions & gotchas

- Use Conventional Commit messages — they drive the release/versioning.
- macOS bundles are **ad-hoc signed only** (no Apple Developer identity / not
  notarized); users may need `xattr -dr com.apple.quarantine` after unzipping.
- No AU/AUv3 format is built (`FORMATS VST3 Standalone` in `CMakeLists.txt`).
- On NixOS, `nix-shell`/`shell.nix` provides the Linux build dependencies.
