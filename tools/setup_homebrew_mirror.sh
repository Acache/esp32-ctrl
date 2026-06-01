#!/bin/bash
# Homebrew 中科大镜像配置脚本 (2025-06)
# 来源: https://mirrors.ustc.edu.cn

set -e

echo "=== Homebrew 中科大镜像配置 ==="

CONF_FILE="$HOME/.zshrc"

echo "检测到 shell: $SHELL"
echo "配置文件: $CONF_FILE"

# 中科大镜像
cat >> "$CONF_FILE" << 'EOF'

# ===== Homebrew 国内镜像加速 =====
export HOMEBREW_API_DOMAIN="https://mirrors.ustc.edu.cn/homebrew-bottles/api"
export HOMEBREW_BOTTLE_DOMAIN="https://mirrors.ustc.edu.cn/homebrew-bottles"
export HOMEBREW_BREW_GIT_REMOTE="https://mirrors.ustc.edu.cn/brew.git"
export HOMEBREW_CORE_GIT_REMOTE="https://mirrors.ustc.edu.cn/homebrew-core.git"
export HOMEBREW_PIP_INDEX_URL="https://pypi.tuna.tsinghua.edu.cn/simple"
export HOMEBREW_NO_ENV_HINTS=1
# ===== Homebrew 镜像配置结束 =====
EOF

echo "配置文件已更新: $CONF_FILE"
echo ""
echo "=== 测试 brew version ==="
brew --version