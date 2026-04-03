#!/usr/bin/env bash
# =============================================================
# view.sh — Launch orbit-viewer with an interactively picked
#           CSV file from the results/ directory.
#
# Usage:
#   ./view.sh                  # interactive picker
#   ./view.sh results/foo.csv  # pass file directly, skip picker
# =============================================================

RESULTS_DIR="./results"
VIEWER="./build/bin/orbit-viewer"

# ── Sanity checks ─────────────────────────────────────────────
if [ ! -f "$VIEWER" ]; then
    echo "❌ orbit-viewer not found at $VIEWER"
    echo "   Run: cmake --build build --parallel"
    exit 1
fi

if [ ! -d "$RESULTS_DIR" ]; then
    echo "❌ Results directory not found: $RESULTS_DIR"
    echo "   Run a simulation first."
    exit 1
fi

# ── If a file was passed directly, use it ─────────────────────
if [ -n "$1" ]; then
    if [ ! -f "$1" ]; then
        echo "❌ File not found: $1"
        exit 1
    fi
    echo "🚀 Launching viewer with: $1"
    exec "$VIEWER" "$1"
fi

# ── Interactive picker ─────────────────────────────────────────

# Only show CSV files, not eclipse logs
mapfile -t FILES < <(find "$RESULTS_DIR" -maxdepth 1 -name "*.csv" ! -name "*eclipse*" | sort)

if [ ${#FILES[@]} -eq 0 ]; then
    echo "❌ No CSV files found in $RESULTS_DIR"
    echo "   Run a simulation first."
    exit 1
fi

echo ""
echo "Available simulations:"
echo "──────────────────────"

for i in "${!FILES[@]}"; do
    # Show index, filename, and file size
    SIZE=$(du -sh "${FILES[$i]}" 2>/dev/null | cut -f1)
    BASENAME=$(basename "${FILES[$i]}")
    printf "  [%d] %-40s %s\n" "$((i+1))" "$BASENAME" "$SIZE"
done

echo ""

while true; do
    read -rp "Select a file [1-${#FILES[@]}]: " choice

    # Validate input is a number
    if ! [[ "$choice" =~ ^[0-9]+$ ]]; then
        echo "   Please enter a number."
        continue
    fi

    # Validate range
    if [ "$choice" -lt 1 ] || [ "$choice" -gt "${#FILES[@]}" ]; then
        echo "   Please enter a number between 1 and ${#FILES[@]}."
        continue
    fi

    SELECTED="${FILES[$((choice-1))]}"
    break
done

echo ""
echo "🚀 Launching viewer with: $SELECTED"
echo ""
exec "$VIEWER" "$SELECTED"