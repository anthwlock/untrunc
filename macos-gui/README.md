# Untrunc — macOS GUI

A native macOS SwiftUI application for **untrunc** — drag-and-drop video repair without touching the terminal.

![macOS 13+](https://img.shields.io/badge/macOS-13%2B-blue) ![Swift 5.9](https://img.shields.io/badge/Swift-5.9-orange) ![Architecture](https://img.shields.io/badge/arch-arm64-lightgrey)

## Features

- **Drag & drop** reference file and corrupt file
- All key CLI flags exposed as checkboxes (`-s`, `-sv`, `-k`, `-dw`, `-ms`)
- Live log output streamed in real time
- "Show in Finder" button when repair completes
- Fully self-contained DMG — no Homebrew or FFmpeg needed on the target machine

## Requirements (to build)

- macOS 13 Ventura or later (Apple Silicon)
- Xcode Command Line Tools: `xcode-select --install`
- FFmpeg via Homebrew: `brew install ffmpeg`

## Build & Run

```bash
cd macos-gui
bash build.sh
open Untrunc.app
```

## Create a distributable DMG

```bash
# Also requires: brew install create-dmg
bash package_dmg.sh
# → produces Untrunc-1.0.dmg
```

The DMG bundles all FFmpeg dylibs inside the `.app` and rewrites their
install names to `@loader_path`-relative paths, so the app runs on any
macOS 13+ Apple Silicon machine with no dependencies.

## Project structure

```
macos-gui/
├── Sources/UntruncGUI/
│   ├── UntruncApp.swift      # @main SwiftUI entry point
│   ├── ContentView.swift     # Full UI (drop zones, options, log panel)
│   ├── UntruncRunner.swift   # Runs untrunc CLI as subprocess, streams output
│   └── AppIcon.icns          # App icon (regenerate with make_icon.py)
├── Package.swift             # Swift Package manifest
├── Info.plist                # macOS app bundle metadata
├── build.sh                  # Compiles untrunc + Swift app → Untrunc.app
├── bundle_libs.sh            # Bundles FFmpeg dylibs and rewrites @rpath
├── package_dmg.sh            # Full pipeline → Untrunc-1.0.dmg
├── make_icon.py              # Generates AppIcon.icns using AppKit
└── make_dmg_bg.py            # Generates DMG background image using AppKit
```
