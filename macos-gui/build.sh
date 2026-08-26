#!/usr/bin/env bash
# build.sh — builds the Untrunc macOS GUI app
# Run from inside the macos-gui/ directory.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$SCRIPT_DIR/.build"
APP_NAME="Untrunc"
APP_DIR="$SCRIPT_DIR/$APP_NAME.app"
CONTENTS="$APP_DIR/Contents"
CLI_DST="$SCRIPT_DIR/Sources/UntruncGUI/untrunc"

# ── 1. Build the untrunc CLI from source ───────────────────────────────────
echo "==> Building untrunc CLI..."
FFMPEG_PREFIX="$(brew --prefix ffmpeg 2>/dev/null || echo /opt/homebrew/opt/ffmpeg)"
if [ ! -d "$FFMPEG_PREFIX/include" ]; then
    echo "ERROR: FFmpeg not found. Install with: brew install ffmpeg"
    exit 1
fi

make -C "$REPO_ROOT" \
    CXXFLAGS="-isystem${FFMPEG_PREFIX}/include" \
    LDFLAGS="-L${FFMPEG_PREFIX}/lib -lavformat -lavcodec -lavutil" \
    2>&1

cp "$REPO_ROOT/untrunc" "$CLI_DST"
echo "  CLI binary ready."

# ── 2. Regenerate app icon (optional, skipped if .icns already exists) ──────
ICNS="$SCRIPT_DIR/Sources/UntruncGUI/AppIcon.icns"
if [ ! -f "$ICNS" ]; then
    echo "==> Generating app icon..."
    python3 "$SCRIPT_DIR/make_icon.py"
fi

# ── 3. Build the Swift GUI with swift build ─────────────────────────────────
echo "==> Building Swift GUI..."
cd "$SCRIPT_DIR"
swift build -c release 2>&1

BINARY="$BUILD_DIR/release/UntruncGUI"
if [ ! -f "$BINARY" ]; then
    echo "ERROR: Swift build failed — binary not found."
    exit 1
fi

# ── 4. Package as .app bundle ────────────────────────────────────────────────
echo "==> Packaging $APP_NAME.app..."
rm -rf "$APP_DIR"
mkdir -p "$CONTENTS/MacOS"
mkdir -p "$CONTENTS/Resources"

cp "$BINARY"           "$CONTENTS/MacOS/UntruncGUI"
cp "$SCRIPT_DIR/Info.plist" "$CONTENTS/Info.plist"
cp "$ICNS"             "$CONTENTS/Resources/AppIcon.icns"

# Copy untrunc CLI
RESOURCE_UNTRUNC="$BUILD_DIR/release/UntruncGUI_UntruncGUI.resources/untrunc"
if [ -f "$RESOURCE_UNTRUNC" ]; then
    cp "$RESOURCE_UNTRUNC" "$CONTENTS/Resources/untrunc"
elif [ -f "$CLI_DST" ]; then
    cp "$CLI_DST" "$CONTENTS/Resources/untrunc"
fi
chmod +x "$CONTENTS/Resources/untrunc"
chmod +x "$CONTENTS/MacOS/UntruncGUI"

echo ""
echo "✅  Built: $APP_DIR"
echo "   Run:    open \"$APP_DIR\""
echo "   To create a distributable DMG: bash package_dmg.sh"
