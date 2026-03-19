#!/usr/bin/env bash
# bundle_libs.sh — copies all non-system dylibs into the .app and rewrites paths
# so the app is fully self-contained (no Homebrew needed on target machine).
set -e

APP="$1"
if [ -z "$APP" ]; then
    echo "Usage: $0 <path/to/App.app>"
    exit 1
fi

FRAMEWORKS="$APP/Contents/Frameworks"
RESOURCES="$APP/Contents/Resources"
MACOS="$APP/Contents/MacOS"

mkdir -p "$FRAMEWORKS"

# ── Helpers ────────────────────────────────────────────────────────────────

# Returns true if the path is a system library (should NOT be bundled)
is_system() {
    local p="$1"
    [[ "$p" == /usr/lib/* ]] || \
    [[ "$p" == /System/* ]] || \
    [[ "$p" == @* ]]
}

# Get the real path of a dylib install name
resolve_lib() {
    local name="$1"
    # Already an absolute path?
    if [[ "$name" == /* ]] && [ -f "$name" ]; then
        echo "$name"; return
    fi
    # Try brew prefix variations
    local base; base=$(basename "$name")
    local found; found=$(find /opt/homebrew /usr/local -name "$base" -type f 2>/dev/null | head -1)
    echo "$found"
}

PROCESSED=()
QUEUE=()

already_processed() {
    local t="$1"
    for p in "${PROCESSED[@]}"; do [ "$p" = "$t" ] && return 0; done
    return 1
}

# ── Recursively collect and copy libs ─────────────────────────────────────

collect_deps() {
    local binary="$1"
    local deps
    deps=$(otool -L "$binary" 2>/dev/null | tail -n +2 | awk '{print $1}')

    while IFS= read -r dep; do
        [ -z "$dep" ] && continue
        is_system "$dep" && continue

        local basename; basename=$(basename "$dep")
        local dest="$FRAMEWORKS/$basename"

        already_processed "$basename" && continue
        PROCESSED+=("$basename")

        # Resolve actual file path
        local src
        if [[ "$dep" == /* ]] && [ -f "$dep" ]; then
            src="$dep"
        else
            src=$(resolve_lib "$dep")
        fi

        if [ -z "$src" ] || [ ! -f "$src" ]; then
            echo "  ⚠️  Could not find: $dep (skipping)"
            continue
        fi

        echo "  + $basename"
        cp "$src" "$dest"
        chmod 755 "$dest"

        # Recurse into the copied dylib
        collect_deps "$dest"
    done <<< "$deps"
}

# ── Rewrite install names ─────────────────────────────────────────────────

rewrite_binary() {
    local binary="$1"
    local ref_prefix="$2"   # e.g. @loader_path/../Frameworks

    local deps
    deps=$(otool -L "$binary" 2>/dev/null | tail -n +2 | awk '{print $1}')

    while IFS= read -r dep; do
        [ -z "$dep" ] && continue
        is_system "$dep" && continue

        local basename; basename=$(basename "$dep")
        install_name_tool -change "$dep" "${ref_prefix}/${basename}" "$binary" 2>/dev/null || true
    done <<< "$deps"
}

rewrite_dylib_id() {
    local dylib="$1"
    local basename; basename=$(basename "$dylib")
    install_name_tool -id "@rpath/$basename" "$dylib" 2>/dev/null || true
}

# ── Main ──────────────────────────────────────────────────────────────────

echo "==> Collecting dependencies..."

# Seed from the untrunc CLI binary
UNTRUNC_BIN="$RESOURCES/untrunc"
[ -f "$UNTRUNC_BIN" ] && collect_deps "$UNTRUNC_BIN"

# Also seed from the macOS GUI binary (in case it has extra deps)
GUI_BIN=$(find "$MACOS" -maxdepth 1 -type f -perm +111 | head -1)
[ -n "$GUI_BIN" ] && collect_deps "$GUI_BIN"

echo ""
echo "==> Rewriting install names..."

# 1. Rewrite references in the untrunc CLI binary
#    (it lives in Contents/Resources, so Frameworks is one level up)
if [ -f "$UNTRUNC_BIN" ]; then
    echo "  Patching: $(basename "$UNTRUNC_BIN")"
    rewrite_binary "$UNTRUNC_BIN" "@loader_path/../Frameworks"
fi

# 2. Rewrite references in the GUI binary
if [ -n "$GUI_BIN" ]; then
    echo "  Patching: $(basename "$GUI_BIN")"
    rewrite_binary "$GUI_BIN" "@executable_path/../Frameworks"
fi

# 3. Fix each bundled dylib: update its own -id and its inter-lib references
for dylib in "$FRAMEWORKS"/*.dylib; do
    [ -f "$dylib" ] || continue
    echo "  Patching: $(basename "$dylib")"
    rewrite_dylib_id "$dylib"
    rewrite_binary "$dylib" "@loader_path"
done

# 4. Add @rpath entries so @rpath-based references resolve
if [ -f "$UNTRUNC_BIN" ]; then
    install_name_tool -add_rpath "@loader_path/../Frameworks" "$UNTRUNC_BIN" 2>/dev/null || true
fi
if [ -n "$GUI_BIN" ]; then
    install_name_tool -add_rpath "@executable_path/../Frameworks" "$GUI_BIN" 2>/dev/null || true
fi

echo ""
echo "==> Verifying untrunc can find its libraries..."
DYLD_LIBRARY_PATH="$FRAMEWORKS" \
DYLD_FALLBACK_LIBRARY_PATH="$FRAMEWORKS" \
"$UNTRUNC_BIN" -V 2>&1 && echo "  ✅ untrunc runs standalone" || echo "  ⚠️  Verification failed — check output above"

echo ""
echo "Done. Bundled $(ls "$FRAMEWORKS" | wc -l | tr -d ' ') dylibs into $FRAMEWORKS"
