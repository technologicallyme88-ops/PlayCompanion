"""Generate an automatic contact sheet of the *compiled* Companion sprites.

The preview uses the same PNG decoder/crop/threshold path as the firmware sprite
generator, so it shows what the 1-bit X4 Pro art will actually look like rather
than simply displaying the source PNGs.
"""

Import("env")  # noqa: F821
from pathlib import Path
import importlib.util
import struct
import zlib

PROJECT = Path(env.subst("$PROJECT_DIR"))
GEN = PROJECT / "scripts" / "gen_companion_sprites.py"
OUT = PROJECT / "build_artifacts" / "companion-preview.png"

spec = importlib.util.spec_from_file_location("companion_sprite_gen", GEN)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

# Tiny 3x5 uppercase font: enough for concise row/column labels.
FONT = {
    'A':[0b010,0b101,0b111,0b101,0b101], 'B':[0b110,0b101,0b110,0b101,0b110],
    'C':[0b011,0b100,0b100,0b100,0b011], 'D':[0b110,0b101,0b101,0b101,0b110],
    'E':[0b111,0b100,0b110,0b100,0b111], 'F':[0b111,0b100,0b110,0b100,0b100],
    'G':[0b011,0b100,0b101,0b101,0b011], 'H':[0b101,0b101,0b111,0b101,0b101],
    'I':[0b111,0b010,0b010,0b010,0b111], 'J':[0b001,0b001,0b001,0b101,0b010],
    'K':[0b101,0b101,0b110,0b101,0b101], 'L':[0b100,0b100,0b100,0b100,0b111],
    'M':[0b101,0b111,0b111,0b101,0b101], 'N':[0b101,0b111,0b111,0b111,0b101],
    'O':[0b010,0b101,0b101,0b101,0b010], 'P':[0b110,0b101,0b110,0b100,0b100],
    'Q':[0b010,0b101,0b101,0b011,0b001], 'R':[0b110,0b101,0b110,0b101,0b101],
    'S':[0b011,0b100,0b010,0b001,0b110], 'T':[0b111,0b010,0b010,0b010,0b010],
    'U':[0b101,0b101,0b101,0b101,0b111], 'V':[0b101,0b101,0b101,0b101,0b010],
    'W':[0b101,0b101,0b111,0b111,0b101], 'X':[0b101,0b101,0b010,0b101,0b101],
    'Y':[0b101,0b101,0b010,0b010,0b010], 'Z':[0b111,0b001,0b010,0b100,0b111],
    '0':[0b111,0b101,0b101,0b101,0b111], '1':[0b010,0b110,0b010,0b010,0b111],
    '2':[0b110,0b001,0b010,0b100,0b111], '3':[0b110,0b001,0b010,0b001,0b110],
    '4':[0b101,0b101,0b111,0b001,0b001], '5':[0b111,0b100,0b110,0b001,0b110],
    '6':[0b011,0b100,0b110,0b101,0b010], '7':[0b111,0b001,0b010,0b010,0b010],
    '8':[0b010,0b101,0b010,0b101,0b010], '9':[0b010,0b101,0b011,0b001,0b110],
    ' ':[0,0,0,0,0], '-':[0,0,0b111,0,0], '/':[0b001,0b001,0b010,0b100,0b100],
}


def write_png(path, width, height, rgb):
    def chunk(kind, payload):
        return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff)
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        raw.extend(rgb[y])
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    path.parent.mkdir(parents=True, exist_ok=True)
    data = bytes(png)
    if path.exists() and path.read_bytes() == data:
        return False
    path.write_bytes(data)
    return True


def setpx(canvas, w, h, x, y, black=True):
    if 0 <= x < w and 0 <= y < h:
        v = 0 if black else 255
        i = x * 3
        canvas[y][i:i+3] = bytes((v,v,v))


def text(canvas, w, h, x, y, s, scale=2):
    for ch in s.upper():
        rows = FONT.get(ch, FONT[' '])
        for yy, bits in enumerate(rows):
            for xx in range(3):
                if bits & (1 << (2-xx)):
                    for sy in range(scale):
                        for sx in range(scale):
                            setpx(canvas,w,h,x+xx*scale+sx,y+yy*scale+sy)
        x += 4*scale


def main():
    sprites = PROJECT / "src" / "companion" / "sprites"
    order = [ln.strip() for ln in (sprites / "order.txt").read_text(encoding="utf-8").splitlines() if ln.strip() and not ln.startswith('#')]
    companions=[]
    for stem in order:
        name, kind, poses, quotes = mod.parse_grid_file(str(sprites / f"{stem}.grid"))
        poses = mod.apply_png_assets(stem, poses, str(PROJECT / "assets" / "companions"))
        companions.append((name,poses))

    scale=4; cell_w=160; cell_h=150; left=118; top=30
    width=left+len(mod.MOODS)*cell_w
    height=top+len(companions)*cell_h
    canvas=[bytearray([255]*(width*3)) for _ in range(height)]
    for c,mood in enumerate(mod.MOODS):
        text(canvas,width,height,left+c*cell_w+8,8,mood[:10],1)
    for r,(name,poses) in enumerate(companions):
        y0=top+r*cell_h
        text(canvas,width,height,4,y0+8,name[:13],1)
        for c,mood in enumerate(mod.MOODS):
            rows=poses[mood]
            x0=left+c*cell_w+(cell_w-mod.WIDTH*scale)//2
            sy0=y0+22
            for y,row in enumerate(rows):
                for x,cell in enumerate(row):
                    if mod.dither_ink(cell,x,y):
                        for yy in range(scale):
                            for xx in range(scale):
                                setpx(canvas,width,height,x0+x*scale+xx,sy0+y*scale+yy)
    changed=write_png(OUT,width,height,canvas)
    if changed:
        print(f"Companion preview generated: {OUT}")

main()
