#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_NAME="Untrunc"
VERSION="1.0"
DMG_NAME="${APP_NAME}-${VERSION}.dmg"
APP_PATH="$SCRIPT_DIR/${APP_NAME}.app"
OUT_DMG="$SCRIPT_DIR/$DMG_NAME"
STAGING="$SCRIPT_DIR/.dmg-staging"
BG_IMG="/tmp/dmg_background.png"

# ── 1. Build the app ────────────────────────────────────────────────────────
echo "==> Building ${APP_NAME}.app..."
bash "$SCRIPT_DIR/build.sh"

if [ ! -d "$APP_PATH" ]; then
    echo "ERROR: ${APP_NAME}.app not found after build."
    exit 1
fi

# ── 2. Copy icon into Contents/Resources ───────────────────────────────────
ICNS_SRC="$SCRIPT_DIR/Sources/UntruncGUI/AppIcon.icns"
if [ -f "$ICNS_SRC" ]; then
    cp "$ICNS_SRC" "$APP_PATH/Contents/Resources/AppIcon.icns"
    echo "  Installed app icon."
fi

# ── 3. Bundle all non-system dylibs (FFmpeg + deps) ────────────────────────
echo "==> Bundling dylibs for self-contained distribution..."
bash "$SCRIPT_DIR/bundle_libs.sh" "$APP_PATH"

# ── 4. Ad-hoc code-sign the whole bundle (must happen AFTER bundling libs) ──
echo "==> Code-signing (ad-hoc)..."
# Sign each dylib individually first, then the outer bundle
for dylib in "$APP_PATH/Contents/Frameworks/"*.dylib; do
    codesign --force --sign - "$dylib" 2>/dev/null || true
done
codesign --force --deep --sign - "$APP_PATH" 2>&1 || true

# ── 5. Generate DMG background ─────────────────────────────────────────────
echo "==> Generating DMG background..."
python3 "$SCRIPT_DIR/make_dmg_bg.py"

# ── 6. Create the DMG ──────────────────────────────────────────────────────
echo "==> Creating ${DMG_NAME}..."
rm -f "$OUT_DMG"
rm -rf "$STAGING"

create-dmg \
    --volname "$APP_NAME" \
    --volicon "$ICNS_SRC" \
    --background "$BG_IMG" \
    --window-pos  200 120 \
    --window-size 660 400 \
    --icon-size   120 \
    --icon        "${APP_NAME}.app" 165 200 \
    --app-drop-link 495 200 \
    --no-internet-enable \
    --hdiutil-quiet \
    "$OUT_DMG" \
    "$APP_PATH"

echo ""
echo "✅  Done: $OUT_DMG"
echo "   To install: open \"$OUT_DMG\""
