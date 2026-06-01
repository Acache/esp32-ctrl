#!/usr/bin/env python3
"""
LVGL UI 自动检查器
定期从 ESP32 板子拉取截图，用视觉模型分析 UI 是否正常
完全不需要人工介入
"""
import requests, time, sys, os
from pathlib import Path

# 配置
ESP32_URL = "http://192.168.4.1"
CHECK_INTERVAL = 60  # 每 60 秒检查一次
SCREENSHOT_FILE = "/tmp/desk_mate_screenshot.bmp"
OUTPUT_DIR = Path("/Users/acache/AI_PROJ/desk-mate/screenshots")
OUTPUT_DIR.mkdir(exist_ok=True)

def download_screenshot():
    """从板子下载 BMP 截图"""
    try:
        r = requests.get(f"{ESP32_URL}/scr", timeout=10)
        if r.status_code == 200 and len(r.content) > 1000:
            with open(SCREENSHOT_FILE, 'wb') as f:
                f.write(r.content)
            return True
    except Exception as e:
        print(f"[{time.strftime('%H:%M:%S')}] 下载失败: {e}")
    return False

def check_board_online():
    """检查板子是否在线"""
    try:
        r = requests.get(f"{ESP32_URL}/", timeout=5)
        return r.status_code == 200
    except:
        return False

def convert_bmp_to_png(bmp_path, png_path):
    """BMP -> PNG 转换（用于视觉分析）"""
    try:
        from PIL import Image
        img = Image.open(bmp_path)
        img.save(png_path, 'PNG')
        return True
    except Exception as e:
        print(f"转换失败: {e}")
        return False

def save_screenshot_history():
    """保存截图到历史记录"""
    ts = time.strftime("%Y%m%d_%H%M%S")
    dst = OUTPUT_DIR / f"screenshot_{ts}.png"
    try:
        from PIL import Image
        img = Image.open(SCREENSHOT_FILE)
        img.save(dst)
        print(f"  截图已保存: {dst}")
    except Exception as e:
        print(f"保存历史截图失败: {e}")

if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "once"
    
    print(f"=== Desk-Mate UI 自动检查器 ===")
    print(f"板子地址: {ESP32_URL}")
    print(f"模式: {mode}")
    print()
    
    if mode == "once":
        # 单次检查
        if check_board_online():
            print("板子在线，下载截图...")
            if download_screenshot():
                png_path = SCREENSHOT_FILE.replace(".bmp", ".png")
                if convert_bmp_to_png(SCREENSHOT_FILE, png_path):
                    print(f"截图就绪: {png_path}")
                    # 把 PNG 路径通过环境变量传给视觉分析
                    print(f"PNG_PATH={png_path}")
        else:
            print("板子不在线")
    elif mode == "watch":
        # 持续监控
        print(f"开始监控，每 {CHECK_INTERVAL} 秒检查一次...")
        while True:
            if check_board_online():
                if download_screenshot():
                    save_screenshot_history()
            time.sleep(CHECK_INTERVAL)