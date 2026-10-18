#!/usr/bin/env bash
# BeanDraw OpenHarmony 交叉编译脚本（开发大纲 build_scripts/build_ohos.sh）
#
# 前置条件：
#   - rustup target add aarch64-unknown-linux-ohos（或 ohos-sdk 提供的 target 名）
#   - OHOS NDK 工具链在 PATH
#   - Qt6 for OHOS（若 Phase 0 spike 结论为不可用，仅构建内核并切换 ArkUI 方案）
#
# 用法：./build_ohos.sh [--kernel-only]

set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUST_DIR="$ROOT/rust_core"
UI_DIR="$ROOT/qt_ui"
BUILD_DIR="$ROOT/output/ohos"
KERNEL_ONLY=0

for arg in "$@"; do
  case "$arg" in
    --kernel-only) KERNEL_ONLY=1 ;;
    *) echo "unknown arg: $arg" >&2; exit 1 ;;
  esac
done

echo "==> [1/2] Cross-compiling rust_core for aarch64-ohos"
cd "$RUST_DIR"
# OHOS target 名称以 ohos-sdk 实际为准，常见 aarch64-unknown-linux-ohos
TARGET="${OHOS_TARGET:-aarch64-unknown-linux-ohos}"
rustup target add "$TARGET" 2>/dev/null || true
cargo build --release --target "$TARGET"
if [ $? -ne 0 ]; then
  echo "ERROR: rust kernel cross-compile failed (check OHOS SDK / linker config)" >&2
  exit 1
fi
KERNEL_LIB="$RUST_DIR/target/$TARGET/release/libcraftmaster_core.a"

if [ "$KERNEL_ONLY" -eq 1 ]; then
  echo "==> Kernel-only build done: $KERNEL_LIB"
  exit 0
fi

echo "==> [2/2] Building Qt UI for OHOS (requires Qt6 for OHOS)"
mkdir -p "$BUILD_DIR"
cmake -S "$UI_DIR" -B "$BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="${OHOS_TOOLCHAIN_FILE:?set OHOS_TOOLCHAIN_FILE}" \
  -DCRAFTMASTER_RUST_LIB="$KERNEL_LIB"
cmake --build "$BUILD_DIR" --parallel

echo "==> Done. HAP packaging: use DevEco Studio or hvigor (see platform_guide.md)"
