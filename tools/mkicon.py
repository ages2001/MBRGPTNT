#!/usr/bin/env python3
"""Draw the MBRGPTNT icon and write it as a Windows .ico.

The icon is a hard disk whose front shows a partition map (three volumes in
the manager's colors) and an activity LED. Entries:
  16x16 and 32x32 at 4 bits (VGA palette) - NT 3.1 / 3.5 / 4.0 on 16 colors
  16x16, 32x32 and 48x48 at 32 bits with alpha - Windows XP and later
usage: mkicon.py OUT.ico [PREVIEW.png]"""
import struct, sys
from PIL import Image, ImageDraw

VGA = [(0,0,0),(128,0,0),(0,128,0),(128,128,0),(0,0,128),(128,0,128),(0,128,128),(192,192,192),
       (128,128,128),(255,0,0),(0,255,0),(255,255,0),(0,0,255),(255,0,255),(0,255,255),(255,255,255)]

BLUE, GREEN, PURPLE = (54,118,176), (46,160,110), (116,96,214)


def draw16():
    """16 x 16, drawn pixel by pixel so it stays sharp"""
    im = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([0, 2, 15, 14], radius=2, fill=(66, 76, 92, 255))
    d.rectangle([1, 3, 14, 13], fill=(214, 220, 228, 255))
    d.rectangle([1, 10, 14, 13], fill=(188, 196, 207, 255))
    d.rectangle([2, 4, 13, 9], fill=(40, 50, 66, 255))
    d.rectangle([3, 5, 6, 8], fill=BLUE+(255,))
    d.rectangle([7, 5, 9, 8], fill=GREEN+(255,))
    d.rectangle([10, 5, 12, 8], fill=PURPLE+(255,))
    d.point([(3, 5), (4, 5), (5, 5), (6, 5)], fill=(110, 166, 216, 255))
    d.point([(7, 5), (8, 5), (9, 5)], fill=(96, 206, 156, 255))
    d.point([(10, 5), (11, 5), (12, 5)], fill=(164, 146, 250, 255))
    d.rectangle([11, 11, 12, 12], fill=(60, 200, 90, 255))
    d.point([(3, 11), (5, 11), (7, 11)], fill=(120, 130, 146, 255))
    return im


def draw(size):
    """Render at 8x and scale down."""
    if size <= 16:
        return draw16()
    S = 8
    W = size * S
    im = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    u = W / 32.0                      # design grid: 32 units
    def R(x0, y0, x1, y1):
        return [round(x0*u), round(y0*u), round(x1*u)-1, round(y1*u)-1]
    # drop shadow
    d.rounded_rectangle(R(2, 5, 32, 30), radius=3.6*u, fill=(0, 0, 0, 60))
    # body: light metal, darker lower part
    d.rounded_rectangle(R(0.5, 3.5, 30.5, 28.5), radius=3.6*u, fill=(66, 76, 92, 255))
    d.rounded_rectangle(R(1.6, 4.6, 29.4, 27.4), radius=2.8*u, fill=(220, 225, 233, 255))
    d.rectangle(R(1.6, 19, 29.4, 24), fill=(194, 201, 211, 255))
    d.rounded_rectangle(R(1.6, 21, 29.4, 27.4), radius=2.8*u, fill=(194, 201, 211, 255))
    d.rectangle(R(1.6, 19, 29.4, 19.6), fill=(170, 178, 190, 255))
    # partition map window
    d.rectangle(R(4, 7, 27, 17), fill=(40, 50, 66, 255))
    x = 5
    for w, c in ((9, BLUE), (6, GREEN), (7, PURPLE)):
        x1 = x + w - (0 if x + w >= 26 else 0.8)
        d.rectangle(R(x, 8, x1, 16), fill=c+(255,))
        d.rectangle(R(x, 8, x1, 9.4), fill=tuple(min(255, v+50) for v in c)+(255,))
        x += w
    # grille and LED
    for gx in (5, 8, 11, 14):
        d.rectangle(R(gx, 22, gx+1.6, 25), fill=(118, 128, 144, 255))
    d.ellipse(R(22.5, 21.2, 27, 25.7), fill=(20, 110, 40, 255))
    d.ellipse(R(23.3, 22, 26.2, 24.9), fill=(96, 226, 116, 255))
    return im.resize((size, size), Image.LANCZOS)


def draw32_vga():
    """32 x 32 in the 16 VGA colors, drawn without smoothing"""
    im = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    K, DG, LG, W = (0,0,0,255), (128,128,128,255), (192,192,192,255), (255,255,255,255)
    d.rounded_rectangle([0, 3, 30, 28], radius=3, fill=K)
    d.rounded_rectangle([1, 4, 29, 27], radius=2, fill=LG)
    d.line([(2, 4), (28, 4)], fill=W); d.line([(1, 5), (1, 18)], fill=W)
    d.line([(1, 19), (29, 19)], fill=DG)
    d.rectangle([4, 7, 26, 16], fill=K)
    for x0, x1, c, h in ((5, 12, (0,0,255,255), (0,255,255,255)), (14, 18, (0,128,0,255), (0,255,0,255)),
                         (20, 25, (128,0,128,255), (255,0,255,255))):
        d.rectangle([x0, 8, x1, 15], fill=c)
        d.line([(x0, 8), (x1, 8)], fill=h)
    for gx in (5, 8, 11, 14):
        d.rectangle([gx, 22, gx+1, 24], fill=DG)
    d.ellipse([23, 21, 26, 24], fill=(0,128,0,255))
    d.rectangle([24, 22, 25, 23], fill=(0,255,0,255))
    return im


def draw16_vga():
    im = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 3, 15, 13], fill=(0,0,0,255))
    d.rectangle([1, 4, 14, 12], fill=(192,192,192,255))
    d.rectangle([2, 5, 13, 9], fill=(0,0,0,255))
    d.rectangle([3, 6, 6, 8], fill=(0,0,255,255))
    d.rectangle([7, 6, 9, 8], fill=(0,128,0,255))
    d.rectangle([10, 6, 12, 8], fill=(128,0,128,255))
    d.point([(3, 11), (5, 11), (7, 11)], fill=(128,128,128,255))
    d.rectangle([11, 11, 12, 11], fill=(0,255,0,255))
    return im


def nearest(c):
    return min(range(16), key=lambda i: sum((a-b)**2 for a, b in zip(c, VGA[i])))


def and_mask(im):
    w, h = im.size
    row = ((w + 31) // 32) * 4
    out = bytearray()
    for y in range(h - 1, -1, -1):
        bits = bytearray(row)
        for x in range(w):
            if im.getpixel((x, y))[3] < 128:
                bits[x // 8] |= 0x80 >> (x % 8)
        out += bits
    return bytes(out)


def entry4(im):
    w, h = im.size
    hdr = struct.pack("<IiiHHIIiiII", 40, w, h*2, 1, 4, 0, 0, 0, 0, 16, 0)
    pal = b"".join(struct.pack("BBBB", b, g, r, 0) for r, g, b in VGA)
    row = ((w * 4 + 31) // 32) * 4
    px = bytearray()
    for y in range(h - 1, -1, -1):
        line = bytearray(row)
        for x in range(w):
            r, g, b, a = im.getpixel((x, y))
            i = nearest((r, g, b)) if a >= 128 else 0
            line[x // 2] |= (i << 4) if x % 2 == 0 else i
        px += line
    return hdr + pal + bytes(px) + and_mask(im)


def entry32(im):
    w, h = im.size
    hdr = struct.pack("<IiiHHIIiiII", 40, w, h*2, 1, 32, 0, 0, 0, 0, 0, 0)
    px = bytearray()
    for y in range(h - 1, -1, -1):
        for x in range(w):
            r, g, b, a = im.getpixel((x, y))
            px += bytes((b, g, r, a))
    return hdr + bytes(px) + and_mask(im)


def main():
    imgs = {s: draw(s) for s in (16, 32, 48)}
    entries = [(16, 4, entry4(draw16_vga())), (32, 4, entry4(draw32_vga())),
               (16, 32, entry32(imgs[16])), (32, 32, entry32(imgs[32])), (48, 32, entry32(imgs[48]))]
    out = struct.pack("<HHH", 0, 1, len(entries))
    off = 6 + 16 * len(entries)
    for s, bpp, data in entries:
        out += struct.pack("<BBBBHHII", s, s, 16 if bpp == 4 else 0, 0, 1, bpp, len(data), off)
        off += len(data)
    for _, _, data in entries:
        out += data
    open(sys.argv[1], "wb").write(out)
    if len(sys.argv) > 2:
        sheet = Image.new("RGBA", (16*8 + 32*8 + 48*8 + 40, 48*8), (255, 255, 255, 255))
        x = 0
        for s in (16, 32, 48):
            sheet.alpha_composite(imgs[s].resize((s*8, s*8), Image.NEAREST), (x, 0)); x += s*8 + 20
        sheet.save(sys.argv[2])


main()
