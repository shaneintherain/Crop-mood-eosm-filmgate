#!/usr/bin/env python3
"""Create the Jost Medium bitmap font used by the FilmGate menus.

The camera's RBF renderer is monochrome, so glyphs are drawn without
anti-aliasing (PIL font mode "1").  Every glyph sits in a 40 px tall cell with
the middle of the capital letters at 45% of the cell height, which is where the
menu layout code expects the optical centre of the text.
"""
from pathlib import Path
import struct
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "data" / "fonts" / "source" / "jost-medium.ttf"
OUTPUT = ROOT / "data" / "fonts" / "jost-medium.rbf"

FIRST, LAST = 32, 126
HEIGHT = 40          # cell height (menu code sizes rows from this)
PIXEL_SIZE = 24      # Jost em size in pixels
MAX_WIDTH = 32
CAP_CENTER = 18      # capital letters are centred on this row (0.45 * 40)
TRACKING = 1         # extra pixel between letters


def main():
    font = ImageFont.truetype(str(SOURCE), PIXEL_SIZE)
    cap = font.getbbox("H", anchor="ls")
    cap_h = -cap[1]
    baseline = CAP_CENTER + (cap_h + 1) // 2
    bytes_per_row = (MAX_WIDTH + 7) // 8
    char_size = bytes_per_row * HEIGHT
    count = LAST - FIRST + 1
    cmap_offset = 0x74 + count

    advances = bytearray()
    cmap = bytearray()
    for cp in range(FIRST, LAST + 1):
        ch = chr(cp)
        advance = 8 if cp == 32 else max(1, round(font.getlength(ch))) + TRACKING
        img = Image.new("L", (MAX_WIDTH, HEIGHT), 0)
        d = ImageDraw.Draw(img)
        d.fontmode = "1"
        d.text((0, baseline), ch, font=font, fill=255, anchor="ls")
        px = img.load()
        rows = bytearray(char_size)
        for y in range(HEIGHT):
            for x in range(MAX_WIDTH):
                if px[x, y] >= 128:
                    rows[y * bytes_per_row + x // 8] |= 1 << (x % 8)   # LSB first
        advances.append(min(MAX_WIDTH, advance))
        cmap.extend(rows)

    name = b"Jost Medium EOSM"
    name += b"\0" * (64 - len(name))
    header = struct.pack("<II64s11i", 0x0DF00EE0, 3, name,
                         char_size, PIXEL_SIZE, HEIGHT, MAX_WIDTH,
                         FIRST, LAST, 0, 0x74, cmap_offset, 0, HEIGHT)
    assert len(header) == 0x74
    OUTPUT.write_bytes(header + bytes(advances) + bytes(cmap))
    print(f"wrote {OUTPUT} ({OUTPUT.stat().st_size} bytes), cap height {cap_h}px")


if __name__ == "__main__":
    main()
