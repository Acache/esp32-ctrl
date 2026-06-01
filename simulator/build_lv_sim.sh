#!/bin/bash
# 自动安装 LVGL PC 模拟器 (macOS)
# 使用中科大镜像加速

set -e
cd /tmp/lv_sim_build

echo "=== 1. 配置 Homebrew 镜像 ==="
export HOMEBREW_API_DOMAIN="https://mirrors.ustc.edu.cn/homebrew-bottles/api"
export HOMEBREW_BOTTLE_DOMAIN="https://mirrors.ustc.edu.cn/homebrew-bottles"
export HOMEBREW_BREW_GIT_REMOTE="https://mirrors.ustc.edu.cn/brew.git"
export HOMEBREW_CORE_GIT_REMOTE="https://mirrors.ustc.edu.cn/homebrew-core.git"
export HOMEBREW_NO_ENV_HINTS=1

echo "=== 2. 检查 sdl2 ==="
if ! command -v sdl2-config &>/dev/null; then
    brew install sdl2
fi
sdl2-config --cflags --libs

echo "=== 3. 克隆 lv_port_pc_vscode (官方模拟器) ==="
rm -rf lv_port_pc_vscode
git clone --depth 1 https://ghproxy.net/https://github.com/lvgl/lv_port_pc_vscode.git lv_port_pc_vscode 2>&1 | tail -3

echo "=== 4. 启用 SDL 驱动 ==="
# 修改 lv_conf.h：启用 SNAPSHOT, SDL, 禁用 DEMO_WIDGETS
CONF="/tmp/lv_sim_build/lv_port_pc_vscode/lv_conf.h"
DRV_CONF="/tmp/lv_sim_build/lv_port_pc_vscode/lv_drv_conf.h"

# 替换分辨率
sed -i.bak 's/800 * 480/480 * 480/g' $CONF $DRV_CONF main.c

# 启用 SNAPSHOT
sed -i 's/#define LV_USE_SNAPSHOT 0/#define LV_USE_SNAPSHOT 1/' $CONF

# 禁用 widgets demo，启用 benchmark 方便测试
sed -i 's/#define LV_USE_DEMO_WIDGETS        1/#define LV_USE_DEMO_WIDGETS        0/' $CONF
sed -i 's/#define LV_USE_DEMO_BENCHMARK   0/#define LV_USE_DEMO_BENCHMARK   1/' $CONF

# 启用 SDL 驱动
sed -i 's/#ifndef USE_SDL/#ifndef USE_SDL\n# define USE_SDL 1/' $DRV_CONF
sed -i 's/#define USE_SDL 0/#define USE_SDL 1/' $DRV_CONF 2>/dev/null || true

echo "=== 5. 编译 ==="
cd lv_port_pc_vscode
mkdir -p build
cd build
cmake .. -DCMAKE_C_FLAGS="-I/opt/homebrew/include" \
         -DCMAKE_EXE_LINKER_FLAGS="-L/opt/homebrew/lib" 2>&1 | tail -10

make -j$(nproc) 2>&1 | tail -5

echo "=== 6. 检查可执行文件 ==="
ls -la lv_port_pc_vscode 2>/dev/null || ls -la . | grep -E "demo|lv_sim|main"