#!/usr/bin/env python3
"""
Desk-Mate 屏幕自动查看器
通过 WiFi 从 ESP32-C6 获取实时截图

用法:
  python3 screenshot_wifi.py [IP地址]
  
默认 IP: 192.168.4.1 (ESP32 AP 模式)

自动每 3 秒刷新一次，截图保存在当前目录。
"""

import sys
import io
import time
import os
import requests
from PIL import Image

ESP32_IP = sys.argv[1] if len(sys.argv) > 1 else "192.168.4.1"
URL = f"http://{ESP32_IP}/scr"
SAVE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "screenshots")

os.makedirs(SAVE_DIR, exist_ok=True)

print(f"Desk-Mate 屏幕查看器")
print(f"URL: {URL}")
print(f"截图保存: {SAVE_DIR}")
print("按 Ctrl+C 停止\n")

count = 0

try:
    while True:
        try:
            r = requests.get(URL, timeout=10)
            if r.status_code == 200 and r.content[:2] == b'BM':
                # 保存
                path = os.path.join(SAVE_DIR, f"latest.bmp")
                with open(path, "wb") as f:
                    f.write(r.content)
                
                # 也保存带时间戳的副本
                ts = time.strftime("%H%M%S")
                path_ts = os.path.join(SAVE_DIR, f"{ts}.bmp")
                with open(path_ts, "wb") as f:
                    f.write(r.content)
                
                size_kb = len(r.content) / 1024
                print(f"[{count}] 截图获取: {size_kb:.0f}KB | {path_ts}")
                count += 1
            else:
                print(f"[{count}] 非 BMP 响应: status={r.status_code}")
        except Exception as e:
            print(f"[{count}] 错误: {e}")
        
        time.sleep(3)

except KeyboardInterrupt:
    print("\n停止")
