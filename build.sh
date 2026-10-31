#!/usr/bin/env bash
# Vector Kernel Build Script for POCO X4 Pro 5G (veux / peux)
# Supports Normal, KernelSU, and Unified dual-mode AnyKernel3 packages.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

VARIANT="both"
JOBS="$(nproc 2>/dev/null || echo 4)"
BUILD_DATE="$(date -u +%Y%m%d)"
DEFCONFIG="veux_defconfig"
OUT_BASE="$ROOT_DIR/out"
DIST_DIR="$ROOT_DIR/dist"

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Options:
  --normal       Build standard non-KSU kernel only
  --ksu          Build KernelSU-enabled kernel only
  --both         Build both standard & KSU kernels + unified package (default)
  -j, --jobs N   Number of parallel make jobs (default: $JOBS)
  --clean        Clean output directories before building
  -h, --help     Show this help message
EOF
    exit 0
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --normal)
            VARIANT="normal"
            shift
            ;;
        --ksu)
            VARIANT="ksu"
            shift
            ;;
        --both)
            VARIANT="both"
            shift
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        --clean)
            rm -rf "$OUT_BASE" "$DIST_DIR"
            echo "Cleaned build directories."
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

# Check / Setup Clang & Toolchain
if [ -d "$ROOT_DIR/llvm/bin" ]; then
    export PATH="$ROOT_DIR/llvm/bin:$PATH"
fi

if ! command -v clang >/dev/null 2>&1; then
    echo "Clang not found in PATH or $ROOT_DIR/llvm/bin."
    echo "Downloading ClangBuiltLinux LLVM 22.1.7 from cdn.kernel.org..."
    mkdir -p "$ROOT_DIR/llvm"
    curl -sSL "https://cdn.kernel.org/pub/tools/llvm/files/llvm-22.1.7-x86_64.tar.xz" | tar -xJ --strip-components=1 -C "$ROOT_DIR/llvm"
    export PATH="$ROOT_DIR/llvm/bin:$PATH"
fi

export ARCH=arm64
export SUBARCH=arm64
export KBUILD_BUILD_USER="${KBUILD_BUILD_USER:-ThisColdBlueSky}"
export KBUILD_BUILD_HOST="${KBUILD_BUILD_HOST:-$(hostname 2>/dev/null || echo android)}"

mkdir -p "$DIST_DIR"

compile_flavor() {
    local flavor="$1"
    local out_dir="$OUT_BASE/$flavor"
    echo "=========================================="
    echo "Building Vector Kernel: flavor=$flavor"
    echo "=========================================="

    mkdir -p "$out_dir"
    
    # 1. Defconfig
    make O="$out_dir" ARCH=arm64 CC=clang LLVM=1 LLVM_IAS=1 \
        CROSS_COMPILE=aarch64-linux-gnu- CROSS_COMPILE_ARM32=arm-linux-gnueabi- \
        "$DEFCONFIG"

    echo "--> Enabling ThinLTO (CONFIG_THINLTO=y)"
    bash "$ROOT_DIR/scripts/config" --file "$out_dir/.config" -e THINLTO

    # 2. Toggle KSU
    if [ "$flavor" = "ksu" ]; then
        echo "--> Enabling KernelSU (CONFIG_KSU=y)"
        bash "$ROOT_DIR/scripts/config" --file "$out_dir/.config" -e KSU
    else
        echo "--> Disabling KernelSU (CONFIG_KSU not set)"
        bash "$ROOT_DIR/scripts/config" --file "$out_dir/.config" -d KSU
    fi

    make O="$out_dir" ARCH=arm64 CC=clang LLVM=1 LLVM_IAS=1 \
        CROSS_COMPILE=aarch64-linux-gnu- CROSS_COMPILE_ARM32=arm-linux-gnueabi- \
        olddefconfig

    # 3. Build Kernel & DTBs
    make -j"$JOBS" O="$out_dir" ARCH=arm64 CC=clang LLVM=1 LLVM_IAS=1 \
        CROSS_COMPILE=aarch64-linux-gnu- CROSS_COMPILE_ARM32=arm-linux-gnueabi- \
        KCFLAGS="-Wno-error" \
        Image dtbs

    # 4. Verify outputs
    if [ ! -f "$out_dir/arch/arm64/boot/Image" ]; then
        echo "Error: Image not generated for $flavor!"
        exit 1
    fi

    local dtb_path
    dtb_path=$(find "$out_dir/arch/arm64/boot/dts" -name "veux.dtb" | head -n 1)
    if [ -z "$dtb_path" ] || [ ! -f "$dtb_path" ]; then
        echo "Error: veux.dtb not generated for $flavor!"
        exit 1
    fi

    # 5. Package AnyKernel3 Zip
    local zip_name
    if [ "$flavor" = "ksu" ]; then
        zip_name="Vector-veux-${BUILD_DATE}-KSU.zip"
    else
        zip_name="Vector-veux-${BUILD_DATE}-Standard.zip"
    fi

    local pkg_dir="$out_dir/ak3_pkg"
    rm -rf "$pkg_dir"
    mkdir -p "$pkg_dir"
    cp -r "$ROOT_DIR/anykernel"/* "$pkg_dir"/
    cp "$out_dir/arch/arm64/boot/Image" "$pkg_dir"/Image
    cp "$dtb_path" "$pkg_dir"/dtb

    chmod +x "$pkg_dir/anykernel.sh" \
             "$pkg_dir/META-INF/com/google/android/update-binary" \
             "$pkg_dir"/tools/* 2>/dev/null || true

    (cd "$pkg_dir" && zip -r9 "$DIST_DIR/$zip_name" * -x ".git*" "README.md")
    (cd "$DIST_DIR" && sha256sum "$zip_name" > "$zip_name.sha256")

    echo "==> Successfully packaged: $DIST_DIR/$zip_name"
}

if [ "$VARIANT" = "normal" ] || [ "$VARIANT" = "both" ]; then
    compile_flavor "normal"
fi

if [ "$VARIANT" = "ksu" ] || [ "$VARIANT" = "both" ]; then
    compile_flavor "ksu"
fi

if [ "$VARIANT" = "both" ]; then
    echo "=========================================="
    echo "Generating Unified Dual-Mode Package"
    echo "=========================================="
    if command -v bsdiff >/dev/null 2>&1; then
        bsdiff "$OUT_BASE/normal/arch/arm64/boot/Image" \
               "$OUT_BASE/ksu/arch/arm64/boot/Image" \
               "$DIST_DIR/ksu.bdf"

        local_unified_dir="$OUT_BASE/unified_pkg"
        rm -rf "$local_unified_dir"
        mkdir -p "$local_unified_dir"
        cp -r "$ROOT_DIR/anykernel"/* "$local_unified_dir"/
        cp "$OUT_BASE/normal/arch/arm64/boot/Image" "$local_unified_dir"/Image
        dtb_path=$(find "$OUT_BASE/normal/arch/arm64/boot/dts" -name "veux.dtb" | head -n 1)
        cp "$dtb_path" "$local_unified_dir"/dtb
        cp "$DIST_DIR/ksu.bdf" "$local_unified_dir"/ksu.bdf

        chmod +x "$local_unified_dir/anykernel.sh" \
                 "$local_unified_dir/META-INF/com/google/android/update-binary" \
                 "$local_unified_dir"/tools/* 2>/dev/null || true

        unified_zip="Vector-veux-${BUILD_DATE}-Unified.zip"
        (cd "$local_unified_dir" && zip -r9 "$DIST_DIR/$unified_zip" * -x ".git*" "README.md")
        (cd "$DIST_DIR" && sha256sum "$unified_zip" > "$unified_zip.sha256")
        echo "==> Successfully packaged: $DIST_DIR/$unified_zip"
    else
        echo "Warning: bsdiff not installed. Skipping Unified zip creation."
    fi
fi

echo "=========================================="
echo "All done! Built artifacts in $DIST_DIR:"
ls -lh "$DIST_DIR"
echo "=========================================="
