#!/usr/bin/env python3
"""将 LVGL 快照的 RGB888 raw 数据转换为 BMP"""
import struct
import sys

def rgb888_to_bmp(rgb888_path, output_path, width=480, height=480):
    with open(rgb888_path, 'rb') as f:
        data = f.read()
    
    expected = width * height * 3
    print(f"Read {len(data)} bytes (expected {expected})")
    
    row_size = ((width * 24 + 31) // 32) * 4
    px_size = row_size * height
    bmp_size = 54 + px_size
    
    # BMP header
    header = bytearray(54)
    header[0:2] = b'BM'
    struct.pack_into('<I', header, 2, bmp_size)
    struct.pack_into('<I', header, 10, 54)
    struct.pack_into('<I', header, 14, 40)
    struct.pack_into('<h', header, 18, width)
    struct.pack_into('<h', header, 22, height)
    struct.pack_into('<h', header, 26, 1)   # planes
    struct.pack_into('<h', header, 28, 24)  # bits per pixel
    struct.pack_into('<I', header, 30, 0)   # compression (none)
    
    # Pixel data (bottom-up, BGR)
    pixels = bytearray(px_size)
    for y in range(height):
        src_row = y * width * 3
        dst_row = (height - 1 - y) * row_size
        for x in range(width):
            b = data[src_row + x * 3]
            g = data[src_row + x * 3 + 1]
            r = data[src_row + x * 3 + 2]
            pixels[dst_row + x * 3]     = b
            pixels[dst_row + x * 3 + 1] = g
            pixels[dst_row + x * 3 + 2] = r
    
    with open(output_path, 'wb') as f:
        f.write(header)
        f.write(pixels)
    print(f"BMP saved: {output_path} ({bmp_size} bytes)")

if __name__ == '__main__':
    # 模拟 raw RGB888 数据
    import os
    rgb888_path = sys.argv[1] if len(sys.argv) > 1 else 'rgb888.raw'
    output_path = sys.argv[2] if len(sys.argv) > 2 else 'output.bmp'
    w = int(sys.argv[3]) if len(sys.argv) > 3 else 480
    h = int(sys.argv[4]) if len(sys.argv) > 4 else 480
    rgb888_to_bmp(rgb888_path, output_path, w, h)
