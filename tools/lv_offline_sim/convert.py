#!/usr/bin/env python3
"""将 LVGL raw 截图转换为 PNG
用法: python3 convert.py <raw_file> <output_png> [width=480] [height=480]
"""
import sys, struct, os
from PIL import Image

raw_path = sys.argv[1] if len(sys.argv) > 1 else 'lvgl_screenshot.raw'
png_path = sys.argv[2] if len(sys.argv) > 2 else 'output.png'
w = int(sys.argv[3]) if len(sys.argv) > 3 else 480
h = int(sys.argv[4]) if len(sys.argv) > 4 else 480

with open(raw_path, 'rb') as f:
    data = f.read()

expected_rgb888 = w * h * 3
expected_rgb565 = w * h * 2

print(f'File: {len(data)} bytes, expected RGB888={expected_rgb888} RGB565={expected_rgb565}')

img = Image.new('RGB', (w, h))

if len(data) == expected_rgb888:
    # RGB888
    for y in range(h):
        for x in range(w):
            idx = (y * w + x) * 3
            img.putpixel((x, y), (data[idx], data[idx+1], data[idx+2]))
elif len(data) == expected_rgb565 * 1.5:
    # RGB565 with padding (3 bytes per pixel)
    for y in range(h):
        for x in range(w):
            idx = (y * w + x) * 3
            v = data[idx] << 8 | data[idx+1]  # big-endian RGB565
            r = ((v >> 11) & 0x1F) * 255 // 31
            g = ((v >> 5) & 0x3F) * 255 // 63
            b = (v & 0x1F) * 255 // 31
            img.putpixel((x, y), (r, g, b))
elif len(data) == expected_rgb565:
    # Pure RGB565
    for y in range(h):
        for x in range(w):
            idx = (y * w + x) * 2
            v = data[idx] << 8 | data[idx+1]  # big-endian
            r = ((v >> 11) & 0x1F) * 255 // 31
            g = ((v >> 5) & 0x3F) * 255 // 63
            b = (v & 0x1F) * 255 // 31
            img.putpixel((x, y), (r, g, b))
else:
    print(f'Unknown format, trying RGB888')
    if len(data) >= expected_rgb888:
        for y in range(h):
            for x in range(w):
                idx = (y * w + x) * 3
                img.putpixel((x, y), (data[idx], data[idx+1], data[idx+2]))

img.save(png_path)
print(f'Saved: {png_path}')
