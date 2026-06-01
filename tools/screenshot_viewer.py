#!/usr/bin/env python3
"""
Desk-Mate ESP32 Screen Viewer
================================
通过串口从 ESP32-C6 获取 LVGL 屏幕图像的 PC 端查看器。

工作原理:
  1. 向 ESP32 串口发送 'S' 命令
  2. ESP32 将屏幕图像分块 (每次约 48KB = 50行 RGB565)
  3. 每块前附 6 字节头: [type(1)][row_start(2)][row_count(1)][len(2)]
  4. PC 端接收所有块, 组合成完整 BMP 并显示

依赖:
  pip install pillow pyserial

用法:
  python3 screenshot_viewer.py [port]
  
  按空格刷新, Esc退出
"""

import sys
import struct
import time
import io
import threading
from pathlib import Path

try:
    import serial
except ImportError:
    print("请安装 pyserial: pip install pyserial")
    sys.exit(1)

try:
    from PIL import Image
except ImportError:
    print("请安装 Pillow: pip install Pillow")
    sys.exit(1)


# ============ 协议常量 ============
CMD_SCREENSHOT = ord('S')
RESP_HEADER_SIZE = 6  # type(1) + row_start(2) + row_count(1) + data_len(2)
SCREEN_W = 480
SCREEN_H = 480

RESP_TYPE_HEADER = 0x01  # 块头: rows=0 表示最后一个块之前无新数据
RESP_TYPE_DATA = 0x02    # 像素数据块
RESP_TYPE_DONE = 0x03    # 完成
RESP_TYPE_ERROR = 0xFF   # 错误


class ScreenshotCollector:
    """收集并组装屏幕截图"""
    
    def __init__(self):
        self.rows = {}  # row_index -> rgb565_data (bytes, 960 bytes each)
        self.max_row = 0
    
    def reset(self):
        self.rows = {}
        self.max_row = 0
    
    def add_block(self, row_start, row_count, data):
        """添加一个数据块"""
        for i in range(row_count):
            row_idx = row_start + i
            offset = i * SCREEN_W * 2
            self.rows[row_idx] = data[offset:offset + SCREEN_W * 2]
            if row_idx > self.max_row:
                self.max_row = row_idx
    
    def is_complete(self):
        """是否收集到 480 行"""
        return len(self.rows) >= SCREEN_H
    
    def to_bmp(self):
        """转换为 BMP bytes"""
        if not self.is_complete():
            return None
        
        row_size = ((SCREEN_W * 24 + 31) // 32) * 4
        px_size = row_size * SCREEN_H
        total_size = 54 + px_size
        
        buf = bytearray(total_size)
        
        # BMP header
        buf[0:2] = b'BM'
        struct.pack_into('<I', buf, 2, total_size)
        struct.pack_into('<I', buf, 10, 54)
        struct.pack_into('<I', buf, 14, 40)
        struct.pack_into('<i', buf, 18, SCREEN_W)
        struct.pack_into('<i', buf, 22, SCREEN_H)  # 负数: 从上到下
        struct.pack_into('<H', buf, 26, 1)
        struct.pack_into('<H', buf, 28, 24)
        struct.pack_into('<I', buf, 30, 0)  # no compression
        
        # 像素数据: BMP 从底部开始 (y=H-1 to 0)
        for y in range(SCREEN_H):
            src_row = self.rows.get(y)
            if src_row is None:
                # 缺失行用黑色
                continue
            dst_offset = 54 + (SCREEN_H - 1 - y) * row_size
            for x in range(SCREEN_W):
                rgb565 = struct.unpack_from('<H', src_row, x * 2)[0]
                b = (rgb565 & 0x1F) << 3
                g = (rgb565 >> 5) & 0xFC  # 6 bits, align to 8
                r = (rgb565 >> 11) << 3
                buf[dst_offset + x * 3] = b | (b >> 5)
                buf[dst_offset + x * 3 + 1] = g | (g >> 6)
                buf[dst_offset + x * 3 + 2] = r | (r >> 5)
        
        return bytes(buf)


class ESP32ScreenshotClient:
    """ESP32 截图客户端"""
    
    def __init__(self, port, baud=2000000):
        self.port = port
        self.baud = baud
        self.ser = None
    
    def connect(self):
        """连接并尝试高波特率"""
        # 先用 115200 连接
        self.ser = serial.Serial(self.port, 115200, timeout=2)
        time.sleep(0.3)
        
        # 尝试切换到 2M 波特率
        self.ser.write(b'B')  # 板端收到 B 后切换波特率
        time.sleep(0.1)
        self.ser.baudrate = self.baud
        self.ser.reset_input_buffer()
        
        print(f"连接 {self.port} @ {self.baud} baud")
        return True
    
    def capture(self):
        """发起截图并收集完整数据"""
        collector = ScreenshotCollector()
        collector.reset()
        
        # 清空缓冲
        self.ser.reset_input_buffer()
        
        # 发送命令
        self.ser.write(bytes([CMD_SCREENSHOT]))
        self.ser.flush()
        
        time.sleep(0.1)
        
        # 读取响应
        timeout_start = time.time()
        while not collector.is_complete():
            if time.time() - timeout_start > 15:
                raise TimeoutError(f"超时, 仅收集到 {len(collector.rows)}/{SCREEN_H} 行")
            
            # 读取头
            header = self.ser.read(6)
            if len(header) < 6:
                continue
            
            resp_type, row_start, row_count, data_len = struct.unpack('<BHBH', header)
            
            if resp_type == RESP_TYPE_DONE:
                break
            
            if resp_type == RESP_TYPE_ERROR:
                msg = self.ser.read(data_len)
                raise Exception(f"板端错误: {msg}")
            
            if resp_type == RESP_TYPE_DATA:
                # 读取数据
                data = self.ser.read(data_len)
                if len(data) < data_len:
                    continue
                collector.add_block(row_start, row_count, data)
        
        return collector.to_bmp()
    
    def close(self):
        if self.ser:
            self.ser.close()


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/cu.usbmodem1101"
    
    client = ESP32ScreenshotClient(port)
    
    try:
        client.connect()
        print("连接成功! 发送 'S' 命令...")
        
        bmp_data = client.capture()
        if bmp_data:
            output_path = "screenshot.bmp"
            with open(output_path, "wb") as f:
                f.write(bmp_data)
            print(f"截图保存至: {output_path} ({len(bmp_data)/1024:.1f} KB)")
            
            # 显示
            img = Image.open(io.BytesIO(bmp_data))
            img.show()
        else:
            print("截图失败")
    
    except Exception as e:
        print(f"错误: {e}")
        import traceback
        traceback.print_exc()
    finally:
        client.close()


if __name__ == "__main__":
    main()
