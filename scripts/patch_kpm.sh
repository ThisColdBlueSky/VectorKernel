#!/bin/sh
set -eu

# Target Image path must be provided
if [ $# -lt 1 ]; then
    echo "Usage: $0 <path-to-uncompressed-Image>" >&2
    exit 1
fi

IMAGE="$1"

if [ ! -f "$IMAGE" ]; then
    echo "Error: Kernel image not found at '$IMAGE'" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
TOOLS_DIR="$ROOT_DIR/tools/kpm"
mkdir -p "$TOOLS_DIR"

KP_VERSION="0.14.0"
BASE_URL="https://github.com/ShirkNeko/SukiSU_KernelPatch_patch/releases/download/${KP_VERSION}"

KPIMG_SHA256="ab020f31ca1c058603917e46d8a6691fbce0dd5cdc135a9cd8dc780e89472716"
KPTOOLS_LINUX_SHA256="a6b912ca964d35427d7fbcd87273abac604970e9746fa0613362be971f07259d"
KPTOOLS_ANDROID_SHA256="3129cafd6fd0d9fdd6375b1b760c215cd11f94b3ce6eb13dc437a04d24d99244"

# Determine host architecture
HOST_ARCH="$(uname -m)"
case "$HOST_ARCH" in
    x86_64|amd64)
        KPTOOLS_BIN="kptools-linux"
        EXPECTED_KPTOOLS_SHA256="$KPTOOLS_LINUX_SHA256"
        ;;
    aarch64|arm64)
        KPTOOLS_BIN="kptools-android"
        EXPECTED_KPTOOLS_SHA256="$KPTOOLS_ANDROID_SHA256"
        ;;
    *)
        echo "Error: Unsupported host architecture for kptools: $HOST_ARCH" >&2
        exit 1
        ;;
esac

KPIMG_PATH="$TOOLS_DIR/kpimg"
KPTOOLS_PATH="$TOOLS_DIR/$KPTOOLS_BIN"

download_and_verify() {
    file_path="$1"
    url="$2"
    expected_sha="$3"

    if [ -f "$file_path" ]; then
        actual_sha=$(sha256sum "$file_path" 2>/dev/null | cut -d' ' -f1)
        if [ "$actual_sha" = "$expected_sha" ]; then
            return 0
        fi
        echo "Checksum mismatch for $(basename "$file_path"), re-downloading..."
        rm -f "$file_path"
    fi

    echo "Downloading $(basename "$file_path") from $url..."
    if command -v curl >/dev/null 2>&1; then
        curl -sSL -f -o "$file_path" "$url"
    elif command -v wget >/dev/null 2>&1; then
        wget -q -O "$file_path" "$url"
    else
        echo "Error: Neither curl nor wget found in PATH." >&2
        exit 1
    fi

    actual_sha=$(sha256sum "$file_path" | cut -d' ' -f1)
    if [ "$actual_sha" != "$expected_sha" ]; then
        echo "Error: Checksum verification failed for $(basename "$file_path")!" >&2
        echo "Expected: $expected_sha" >&2
        echo "Actual:   $actual_sha" >&2
        rm -f "$file_path"
        exit 1
    fi
}

download_and_verify "$KPIMG_PATH" "$BASE_URL/kpimg" "$KPIMG_SHA256"
download_and_verify "$KPTOOLS_PATH" "$BASE_URL/$KPTOOLS_BIN" "$EXPECTED_KPTOOLS_SHA256"
chmod +x "$KPTOOLS_PATH"

echo "=== Patching Kernel Image with KernelPatch (KPM) ==="
echo "Target Image: $IMAGE ($(stat -c%s "$IMAGE") bytes)"
echo "KPTools:      $KPTOOLS_PATH ($($KPTOOLS_PATH -v))"
echo "KPImg:        $KPIMG_PATH ($($KPTOOLS_PATH -v -k "$KPIMG_PATH"))"

PATCHED_IMAGE="${IMAGE}.kpm_patched"
rm -f "$PATCHED_IMAGE"

"$KPTOOLS_PATH" -p -i "$IMAGE" -k "$KPIMG_PATH" -o "$PATCHED_IMAGE"

if [ ! -f "$PATCHED_IMAGE" ] || [ ! -s "$PATCHED_IMAGE" ]; then
    echo "Error: Patched image was not generated or is empty!" >&2
    exit 1
fi

mv "$PATCHED_IMAGE" "$IMAGE"
echo "Successfully patched kernel with KPM: $IMAGE ($(stat -c%s "$IMAGE") bytes)"
