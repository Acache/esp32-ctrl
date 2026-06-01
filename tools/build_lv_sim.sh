#!/bin/bash
# LVGL PC 模拟器自动构建脚本
# 用法: bash build_simulator.sh

set -e

WORKDIR="$HOME/AI_PROJ/desk-mate/tools/lv_simulator"
SIMDIR="$WORKDIR/lv_port_linux"

echo "=== LVGL PC Simulator Setup ==="

# 1. 安装依赖
echo "[1/5] Installing dependencies..."
if ! command -v cmake &>/dev/null; then
    brew install cmake sdl2 2>&1 | tail -5 || true
fi

# 2. 克隆 LVGL 模拟器
echo "[2/5] Cloning LVGL simulator..."
mkdir -p "$WORKDIR"
if [ -d "$SIMDIR/.git" ]; then
    echo "Already cloned, pulling..."
    git -C "$SIMDIR" pull
else
    git clone -b release/v8.3 --depth 1 https://github.com/lvgl/lv_port_linux.git "$SIMDIR"
fi

# 3. 配置 CMake
echo "[3/5] Configuring CMake..."
cd "$SIMDIR"
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

# 4. 编译
echo "[4/5] Building..."
make -j$(nproc)

echo "[5/5] Done! Binary: $SIMDIR/build/bin/lv_sim"
echo ""
echo "Usage:"
echo "  cd $SIMDIR/build"
echo "  ./bin/main"
echo ""
echo "Then capture screenshot:"
echo "  # Use VNC or framebuffer to capture the window"