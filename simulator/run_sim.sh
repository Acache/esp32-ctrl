#!/bin/bash
# Desk-Mate 模拟器 — 一键启动
# 用法:
#   ./run_sim.sh          弹出窗口，鼠标交互
#   ./run_sim.sh 截图 [页] 截图模式

set -e

SIM_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SIM_DIR/lv_port_pc_vscode/build"
BIN_DIR="$SIM_DIR/lv_port_pc_vscode/bin"

cd "$BUILD_DIR"

# 自动重新编译（检测源码变化）
if [ ! -f "$BIN_DIR/main" ] || [ "$SIM_DIR/lv_port_pc_vscode/src/main.c" -nt "$BIN_DIR/main" ]; then
    echo "📦 编译模拟器..."
    cmake .. -DCMAKE_C_FLAGS="-I/opt/homebrew/include" \
             -DCMAKE_EXE_LINKER_FLAGS="-L/opt/homebrew/lib" 2>&1 | tail -2
    make -j$(sysctl -n hw.logicalcpu) 2>&1 | tail -3
    cp "$BIN_DIR/main" "$BIN_DIR/desk-mate-sim"
    echo "✅ 编译完成"
fi

cd "$SIM_DIR/lv_port_pc_vscode"

if [ "$1" = "截图" ] || [ "$1" = "screenshot" ]; then
    PAGE="${2:-0}"
    OUTFILE="/tmp/deskmate_page${PAGE}.bmp"
    export LVGL_PAGE="$PAGE"
    export LVGL_SCREENSHOT="$OUTFILE"
    ./bin/main
    echo "📸 截图: $OUTFILE"
    python3 -c "
from PIL import Image
img = Image.open('$OUTFILE')
img.save('${OUTFILE%.bmp}.png')
print(f'  PNG: ${OUTFILE%.bmp}.png ({img.size[0]}x{img.size[1]})')
" 2>/dev/null || true
else
    echo "🖥️  Desk-Mate 模拟器"
    echo "  关闭窗口或 Ctrl+C 退出"
    echo ""
    # 前台运行，窗口会自动弹出
    ./bin/main
fi
