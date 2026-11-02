#!/usr/bin/env python3
"""
Generate neutral PS Vita LiveArea artwork for the public release.

This script deliberately does NOT use the game's icon, portrait, logo or any
other copyrighted asset.  It only draws gradients, geometric lines and the
game title in a plain font (nominative use to identify the port).  The result
is safe to commit and to ship inside the loader-only VPK.

Outputs (8-bit indexed PNG):
  extras/livearea/icon0.png    128x128
  extras/livearea/pic0.png     960x544
  extras/livearea/bg0.png      840x500
  extras/livearea/startup.png  280x158

Pillow is required: pip install pillow
"""
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
LIVE = ROOT / "extras" / "livearea"

FONT_IMPACT = "C:/Windows/Fonts/impact.ttf"
FONT_BOLD = "C:/Windows/Fonts/arialbd.ttf"

GOLD = (230, 191, 112)
GOLD_DARK = (160, 112, 44)
RED = (150, 38, 28)
CREAM = (244, 234, 210)
INK = (10, 7, 6)


def load_font(path, size):
    try:
        return ImageFont.truetype(path, size)
    except Exception:
        return ImageFont.truetype(FONT_BOLD, size)


def gradient_bg(w, h, top=(52, 28, 18), bottom=(7, 5, 4)):
    img = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(1, h - 1)
        c = tuple(int(top[i] * (1 - t) + bottom[i] * t) for i in range(3))
        d.line([(0, y), (w, y)], fill=c)
    return img


def centered(draw, text, font, y, fill, w, shadow=True, stroke=0,
             stroke_fill=INK):
    tw = draw.textlength(text, font=font)
    x = (w - tw) / 2
    if shadow:
        draw.text((x + 2, y + 2), text, font=font, fill=(0, 0, 0, 180),
                  stroke_width=stroke, stroke_fill=stroke_fill)
    draw.text((x, y), text, font=font, fill=fill,
              stroke_width=stroke, stroke_fill=stroke_fill)


def save_indexed(img, path):
    img = img.convert("RGB")
    q = img.quantize(colors=256, method=Image.Quantize.MEDIANCUT,
                     dither=Image.Dither.FLOYDSTEINBERG)
    q.save(path, format="PNG", optimize=True)
    return q


def make_icon():
    w = h = 128
    img = gradient_bg(w, h, (58, 30, 18), (8, 5, 4))
    d = ImageDraw.Draw(img)
    d.rectangle([4, 4, w - 5, h - 5], outline=GOLD_DARK, width=2)
    d.rectangle([10, 10, w - 11, h - 11], outline=(90, 40, 24), width=1)
    f = load_font(FONT_IMPACT, 54)
    centered(d, "BL", f, 26, GOLD, w, shadow=True, stroke=2,
             stroke_fill=INK)
    fs = load_font(FONT_BOLD, 10)
    centered(d, "PS VITA", fs, 92, CREAM, w, shadow=False)
    save_indexed(img, LIVE / "icon0.png")


def make_pic0():
    w, h = 960, 544
    img = gradient_bg(w, h)
    d = ImageDraw.Draw(img)
    # Original decorative frame/lines only.
    d.rectangle([24, 24, w - 25, h - 25], outline=(70, 30, 20), width=2)
    d.line([(90, 402), (870, 402)], fill=RED, width=3)
    d.line([(90, 410), (870, 410)], fill=GOLD_DARK, width=1)

    f1 = load_font(FONT_IMPACT, 112)
    centered(d, "BRUCE LEE", f1, 118, GOLD, w, shadow=True, stroke=3,
             stroke_fill=INK)
    f2 = load_font(FONT_BOLD, 30)
    centered(d, "D R A G O N   W A R R I O R", f2, 260, CREAM, w)
    f3 = load_font(FONT_BOLD, 22)
    centered(d, "PS VITA PORT", f3, 330, GOLD, w)
    f4 = load_font(FONT_BOLD, 14)
    centered(d, "UNOFFICIAL FAN PORT", f4, 440, (190, 170, 140), w,
             shadow=False)
    save_indexed(img, LIVE / "pic0.png")


def make_bg0():
    w, h = 840, 500
    img = gradient_bg(w, h)
    d = ImageDraw.Draw(img)
    d.rectangle([20, 20, w - 21, h - 21], outline=(70, 30, 20), width=2)
    d.line([(80, 372), (760, 372)], fill=RED, width=3)
    d.line([(80, 380), (760, 380)], fill=GOLD_DARK, width=1)

    f1 = load_font(FONT_IMPACT, 98)
    centered(d, "BRUCE LEE", f1, 108, GOLD, w, shadow=True, stroke=3,
             stroke_fill=INK)
    f2 = load_font(FONT_BOLD, 26)
    centered(d, "D R A G O N   W A R R I O R", f2, 234, CREAM, w)
    f3 = load_font(FONT_BOLD, 19)
    centered(d, "PS VITA PORT", f3, 300, GOLD, w)
    f4 = load_font(FONT_BOLD, 12)
    centered(d, "UNOFFICIAL FAN PORT", f4, 408, (190, 170, 140), w,
             shadow=False)
    save_indexed(img, LIVE / "bg0.png")


def make_startup():
    w, h = 280, 158
    img = gradient_bg(w, h, (56, 28, 17), (9, 6, 5))
    d = ImageDraw.Draw(img)
    d.rectangle([5, 5, w - 6, h - 6], outline=GOLD_DARK, width=1)
    f1 = load_font(FONT_IMPACT, 38)
    centered(d, "BRUCE LEE", f1, 22, GOLD, w, shadow=True, stroke=2,
             stroke_fill=INK)
    f2 = load_font(FONT_BOLD, 11)
    centered(d, "DRAGON WARRIOR", f2, 70, CREAM, w)
    f3 = load_font(FONT_BOLD, 10)
    centered(d, "PS VITA PORT", f3, 104, GOLD, w)
    f4 = load_font(FONT_BOLD, 8)
    centered(d, "UNOFFICIAL", f4, 126, (190, 170, 140), w, shadow=False)
    save_indexed(img, LIVE / "startup.png")


def make_loading():
    """Raw RGB565 960x544 loading screen used by video_show_loading_screen()."""
    src = Image.open(LIVE / "pic0.png").convert("RGB")
    if src.size != (960, 544):
        src = src.resize((960, 544), Image.Resampling.LANCZOS)
    buf = bytearray()
    for r, g, b in src.getdata():
        v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        buf += struct.pack("<H", v)
    out = ROOT / "extras" / "loading.rgb565"
    out.write_bytes(buf)
    print(f"loading.rgb565: 960x544 RGB565 {len(buf)} bytes")


def main():
    LIVE.mkdir(parents=True, exist_ok=True)
    make_icon()
    make_pic0()
    make_bg0()
    make_startup()
    make_loading()
    for name in ("icon0.png", "pic0.png", "bg0.png", "startup.png"):
        p = LIVE / name
        im = Image.open(p)
        print(f"{name}: {im.size} {im.mode} {p.stat().st_size} bytes")


if __name__ == "__main__":
    main()
