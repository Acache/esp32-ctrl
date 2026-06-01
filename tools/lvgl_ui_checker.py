#!/usr/bin/env python3
"""
LVGL PC 模拟器截图 + 自动视觉分析
流程：
  1. 用 SDL 窗口渲染 LVGL UI
  2. 用 screencapture 截图
  3. 用 MiniMax Vision API 分析截图内容
  4. 判断 UI 是否正常
"""
import subprocess, sys, os, time

# ===== 配置 =====
LV_SIM_BIN = "/tmp/lv_sim_build/lv_port_pc_vscode/bin/main"
SCREENSHOT_PATH = "/tmp/lvgl_ui_check.png"
LVGL_Window_TITLE = "LVGL Simulator"
LVGL_SIZE = (480, 480)

def run_lvgl_capture(demo="widgets"):
    """运行 LVGL 模拟器并截图"""
    import SDL
    SDL_Init(SDL.INIT_VIDEO)
    
    # 创建隐藏窗口（或者用虚拟显示器）
    window = SDL_CreateWindow(
        b"LVGL",
        SDL.WINDOWPOS_CENTERED, SDL.WINDOWPOS_CENTERED,
        LVGL_SIZE[0], LVGL_SIZE[1],
        SDL.WINDOW_HIDDEN | SDL.WINDOW_OPENGL
    )
    renderer = SDL_CreateRenderer(window, -1, SDL.RENDERER_SOFTWARE | SDL.RENDERER_TARGETTEXTURE)
    
    # 运行 LVGL（通过子进程，因为我们要截图）
    # 实际上 sdl_hal_init 会创建可见窗口，所以我们用子进程运行 + screencapture
    proc = subprocess.Popen(
        [LV_SIM_BIN],
        cwd=os.path.dirname(LV_SIM_BIN),
        stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    
    time.sleep(2)  # 等窗口渲染
    
    # macOS screencapture 截图
    subprocess.run([
        "screencapture", "-x", "-l", str(get_sdl_window_id(window)), SCREENSHOT_PATH
    ], check=True)
    
    proc.terminate()
    SDL_DestroyRenderer(renderer)
    SDL_DestroyWindow(window)
    SDL_Quit()


def get_sdl_window_id(window):
    """从 SDL_Window 获取窗口 ID（macOS）"""
    # SDL_GetWindowID 在 ctypes 上比较麻烦，用 ps 方式找
    return None


def screencapture_window(window_title):
    """用 screencapture 截取特定窗口"""
    # 获取窗口 ID
    window_id = subprocess.check_output([
        "osascript", "-e",
        f'tell application "System Events" to id of every window where title contains "{window_title}"'
    ], text=True).strip().split("\n")[0]
    
    output = subprocess.run([
        "screencapture", "-x", "-w", "-l", window_id, SCREENSHOT_PATH
    ], capture_output=True)
    
    return output.returncode == 0


def simple_capture():
    """直接 screencapture 全屏（假设只有一个显示器）"""
    result = subprocess.run([
        "screencapture", "-x", SCREENSHOT_PATH
    ], check=True)
    return os.path.exists(SCREENSHOT_PATH)


if __name__ == "__main__":
    print("=== LVGL UI 自动检查器 ===")
    
    # 运行模拟器
    print(f"启动模拟器: {LV_SIM_BIN}")
    proc = subprocess.Popen(
        [LV_SIM_BIN],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    
    time.sleep(3)
    
    # 截图
    print("截图...")
    subprocess.run(["screencapture", "-x", SCREENSHOT_PATH], check=True)
    proc.terminate()
    
    print(f"截图已保存: {SCREENSHOT_PATH}")
    print(f"文件大小: {os.path.getsize(SCREENSHOT_PATH)} bytes")