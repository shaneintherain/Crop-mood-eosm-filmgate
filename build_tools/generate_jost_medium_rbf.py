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
FIRST, LAST = 32, 131   # ASCII plus the ML symbol codes 0x80..0x83
TRACKING = 1         # extra pixel between letters

# name -> (file, rbf title, cell height, Jost em size, max glyph width, cap-centre row)
PROFILES = {
    # menus: 40 px cell (menu code sizes rows from this), caps centred at 0.45 * 40
    "menu": ("jost-medium.rbf", b"Jost Medium EOSM", 40, 24, 32, 18),
    # live-view info bars: 28 px cell, the same height as the Canon-style font it replaces
    "info": ("jost-info.rbf", b"Jost Info EOSM", 28, 25, 28, 13),
    # replaces the 23 px stock font (FONT_MED): the top info bar and other small text
    "small": ("jost-small.rbf", b"Jost Small EOSM", 23, 21, 24, 10),
}


# ML uses single bytes 0x80..0x83 inside strings as symbols; the stock fonts draw
# little pictures for them.  Here they are drawn from Jost text (None = blank).
SYMBOLS = {0x80: None,      # ISO sign: left blank, the number alone reads fine
           0x81: "f/",      # aperture
           0x82: "1/",      # shutter reciprocal
           0x83: "\u00b0"}  # degree sign


def build(profile):
    fname, title, HEIGHT, PIXEL_SIZE, MAX_WIDTH, CAP_CENTER = PROFILES[profile]
    OUTPUT = ROOT / "data" / "fonts" / fname
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
        ch = SYMBOLS.get(cp, chr(cp)) if cp >= 127 else chr(cp)
        if ch is None or cp == 127:
            ch = ""
        advance = 8 if cp == 32 else (0 if not ch else max(1, round(font.getlength(ch))) + TRACKING)
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

    name = title
    name += b"\0" * (64 - len(name))
    header = struct.pack("<II64s11i", 0x0DF00EE0, 3, name,
                         char_size, PIXEL_SIZE, HEIGHT, MAX_WIDTH,
                         FIRST, LAST, 0, 0x74, cmap_offset, 0, HEIGHT)
    assert len(header) == 0x74
    OUTPUT.write_bytes(header + bytes(advances) + bytes(cmap))
    print(f"wrote {OUTPUT} ({OUTPUT.stat().st_size} bytes), cap height {cap_h}px")


def main():
    for profile in PROFILES:
        build(profile)


if __name__ == "__main__":
    main()
