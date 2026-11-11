#!/bin/sh
#
# Idempotently apply temporary SukiSU patches against the KernelSU submodule.
# Pinned SukiSU commit: 850149efa429d9076696c9ccef2336302d50d955
# NOTE: If the submodule pin changes, verify and update the patches!

set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
KSU_DIR="$ROOT_DIR/KernelSU"
PATCHES_DIR="$ROOT_DIR/patches/sukisu"

PINNED_SUKISU_COMMIT="850149efa429d9076696c9ccef2336302d50d955"

if [ ! -d "$KSU_DIR" ]; then
    echo "ERROR: KernelSU submodule directory not found at $KSU_DIR" >&2
    exit 1
fi

CURRENT_PIN="$(git -C "$KSU_DIR" rev-parse HEAD 2>/dev/null || echo "unknown")"
if [ "$CURRENT_PIN" != "$PINNED_SUKISU_COMMIT" ]; then
    echo "WARNING: KernelSU pin is $CURRENT_PIN, expected $PINNED_SUKISU_COMMIT." >&2
    echo "         Patches were authored for $PINNED_SUKISU_COMMIT and may need updating." >&2
fi

if [ ! -d "$PATCHES_DIR" ]; then
    echo "No SukiSU patches directory found at $PATCHES_DIR, skipping."
    exit 0
fi

found=0
for patch in "$PATCHES_DIR"/*.patch; do
    [ -f "$patch" ] || continue
    found=1
    patch_name="$(basename "$patch")"
    echo "Checking patch: $patch_name"

    # Idempotency check: if reverse apply succeeds, patch is already applied
    if git -C "$KSU_DIR" apply --reverse --check "$patch" >/dev/null 2>&1; then
        echo "  --> Patch $patch_name is already applied. Skipping."
        continue
    fi

    # Forward apply check
    if git -C "$KSU_DIR" apply --check "$patch" >/dev/null 2>&1; then
        echo "  --> Applying patch $patch_name..."
        git -C "$KSU_DIR" apply "$patch"
        echo "  --> Patch $patch_name applied successfully."
    else
        echo "ERROR: Patch $patch_name cannot be applied cleanly to KernelSU submodule!" >&2
        echo "       Target HEAD: $CURRENT_PIN (expected: $PINNED_SUKISU_COMMIT)" >&2
        echo "       Neither forward nor reverse apply succeeded." >&2
        exit 1
    fi
done

if [ "$found" -eq 0 ]; then
    echo "No patches found in $PATCHES_DIR, skipping."
    exit 0
fi

echo "All SukiSU patches applied cleanly."
