#!/usr/bin/env python3
"""
PERSONAL LiveArea builder (optional).

This uses the game icon from the user's legally obtained APK to make a personal
LiveArea.  The generated art is derived from copyrighted game material: use it
only on your own console and NEVER commit or redistribute it.

Source:
  extras/livearea/source_icon.jpg   (res/drawable/icon.jpg from the user's APK)

Outputs (git-ignored):
  extras/livearea_personal/icon0.png   128x128
  extras/livearea_personal/pic0.png    960x544
  extras/livearea_personal/bg0.png     840x500
  extras/livearea_personal/startup.png 280x158
  extras/livearea_personal/template.xml
  extras/loading_personal.rgb565       raw 960x544 RGB565

For the PUBLIC release use make_neutral_livearea.py instead.

Pillow is required: pip install pillow

Port code Copyright (C) 2026 Leandro Ferrete.
"""
import shutil
import struct
from pathlib import Path
import math
import random

from PIL import Image, ImageChops, ImageDraw, ImageEnhance, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parent.parent
LIVE = ROOT / "extras" / "livearea"
PERSONAL = ROOT / "extras" / "livearea_personal"
ICON_SRC = LIVE / "source_icon.jpg"

FONT_IMPACT = "C:/Windows/Fonts/impact.ttf"
FONT_BOLD = "C:/Windows/Fonts/arialbd.ttf"
FONT_CJK = "C:/Windows/Fonts/msyh.ttc"

# Palette
GOLD = (242, 214, 150)
GOLD_DARK = (184, 132, 54)
RED = (176, 34, 26)
RED_DARK = (96, 18, 14)
INK = (14, 9, 7)
CREAM = (247, 236, 211)


def load_font(path, size, index=0):
    try:
        return ImageFont.truetype(path, size, index=index)
    except Exception:
        return ImageFont.truetype(FONT_BOLD, size)


def gradient_bg(w, h, top=(58, 31, 19), bottom=(7, 5, 4)):
    img = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(1, h - 1)
        c = tuple(int(top[i] * (1 - t) + bottom[i] * t) for i in range(3))
        d.line([(0, y), (w, y)], fill=c)
    return img


def paper_texture(icon, w, h):
    # A brown patch from the icon's upper-right corner, tiled/scaled and blurred.
    patch = icon.crop((172, 0, 256, 84)).resize((w, h), Image.Resampling.LANCZOS)
    patch = patch.filter(ImageFilter.GaussianBlur(14))
    patch = ImageEnhance.Brightness(patch).enhance(0.48)
    patch = ImageEnhance.Contrast(patch).enhance(0.85)
    return patch


def dragon_watermark(w, h):
    layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    size = int(h * 0.98)
    font = load_font(FONT_CJK, size)
    text = "龍"
    bbox = d.textbbox((0, 0), text, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    x = int(w * 0.02) - bbox[0]
    y = int(h * 0.015) - bbox[1]
    d.text((x, y), text, font=font, fill=(112, 25, 18, 118),
           stroke_width=max(1, h // 300), stroke_fill=(160, 52, 24, 90))
    return layer.filter(ImageFilter.GaussianBlur(1.2))


def portrait_mask(size):
    w, h = size
    mask = Image.new("L", size, 0)
    d = ImageDraw.Draw(mask)
    d.rectangle([int(w * 0.18), -int(h * 0.05), w + 40, int(h * 0.78)], fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(max(12, int(h * 0.055))))
    # extra bottom fade
    fade = Image.linear_gradient("L").rotate(180, expand=True).resize(size)
    mask = ImageChops.multiply(mask, fade)
    return mask


def add_portrait(base, icon, target_h, x_frac, y_frac):
    w, h = base.size
    src = icon.crop((0, 0, 256, 200))
    ph = int(target_h)
    pw = int(src.width * ph / src.height)
    portrait = src.resize((pw, ph), Image.Resampling.LANCZOS)
    portrait = ImageEnhance.Contrast(portrait).enhance(1.08)
    portrait = ImageEnhance.Sharpness(portrait).enhance(1.12)
    port = portrait.convert("RGBA")
    mask = portrait_mask((pw, ph))
    x = int(w * x_frac)
    y = int(h * y_frac)
    base.paste(port, (x, y), mask)
    return base


def bottom_shade(base, start_frac=0.52, strength=190):
    w, h = base.size
    overlay = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(overlay)
    start = int(h * start_frac)
    for y in range(start, h):
        a = int(strength * (y - start) / max(1, h - start))
        d.line([(0, y), (w, y)], fill=(0, 0, 0, a))
    return Image.alpha_composite(base, overlay)


def vignette(base, strength=0.50):
    w, h = base.size
    vig = Image.radial_gradient("L").resize((w, h), Image.Resampling.BICUBIC)
    vig = Image.eval(vig, lambda p: 255 - int(p * strength))
    return ImageChops.multiply(base, Image.merge("RGB", (vig, vig, vig)))


def draw_spaced(draw, xy, text, font, fill, spacing, stroke_width=0, stroke_fill=None):
    x, y = xy
    for ch in text:
        draw.text((x, y), ch, font=font, fill=fill,
                  stroke_width=stroke_width, stroke_fill=stroke_fill)
        x += draw.textlength(ch, font=font) + spacing
    return x


def add_title(base, title_size, sub_size, x_frac, y_frac, max_width=None):
    w, h = base.size
    d = ImageDraw.Draw(base)
    title_font = load_font(FONT_IMPACT, title_size)
    sub_font = load_font(FONT_BOLD, sub_size)

    title = "BRUCE LEE"
    sub = "DRAGON WARRIOR"
    x = int(w * x_frac)
    y = int(h * y_frac)
    bbox = d.textbbox((0, 0), title, font=title_font, stroke_width=max(2, title_size // 16))
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    if max_width and tw > max_width:
        title_size = int(title_size * max_width / tw)
        title_font = load_font(FONT_IMPACT, title_size)
        bbox = d.textbbox((0, 0), title, font=title_font, stroke_width=max(2, title_size // 16))
        tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]

    stroke = max(2, title_size // 13)
    # red offset shadow
    d.text((x + stroke, y + stroke), title, font=title_font,
           fill=(70, 8, 6), stroke_width=stroke, stroke_fill=(40, 4, 3))
    # main title
    d.text((x, y), title, font=title_font, fill=CREAM,
           stroke_width=stroke, stroke_fill=INK)
    # gold inner line effect: draw a slightly smaller gold text
    d.text((x, y), title, font=title_font, fill=GOLD)

    line_y = y + th + int(title_size * 0.08)
    d.line([(x + 2, line_y), (x + int(tw * 0.58), line_y)], fill=RED, width=max(2, title_size // 26))
    d.line([(x + 2, line_y + max(2, title_size // 26) + 2),
            (x + int(tw * 0.30), line_y + max(2, title_size // 26) + 2)],
           fill=GOLD_DARK, width=1)

    sub_y = line_y + int(sub_size * 0.65)
    draw_spaced(d, (x + 2, sub_y), sub, sub_font, GOLD,
                spacing=max(1, sub_size // 5), stroke_width=max(1, sub_size // 14),
                stroke_fill=INK)


def save_indexed(img, path):
    img = img.convert("RGB")
    q = img.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG)
    q.save(path, format="PNG", optimize=True)
    return q


def make_pic0(icon):
    w, h = 960, 544
    base = gradient_bg(w, h).convert("RGBA")
    base = Image.alpha_composite(base, paper_texture(icon, w, h).convert("RGBA"))
    base = Image.alpha_composite(base, dragon_watermark(w, h))
    base = add_portrait(base, icon, int(h * 1.08), 0.34, -0.11)
    base = bottom_shade(base, 0.42, 205).convert("RGB")
    base = vignette(base, 0.52).convert("RGBA")
    add_title(base, 98, 28, 0.055, 0.665, max_width=int(w * 0.50))
    save_indexed(base, PERSONAL / "pic0.png")


def make_bg0(icon):
    w, h = 840, 500
    base = gradient_bg(w, h).convert("RGBA")
    base = Image.alpha_composite(base, paper_texture(icon, w, h).convert("RGBA"))
    base = Image.alpha_composite(base, dragon_watermark(w, h))
    base = add_portrait(base, icon, int(h * 1.08), 0.33, -0.12)
    base = bottom_shade(base, 0.48, 195).convert("RGB")
    base = vignette(base, 0.50).convert("RGBA")
    add_title(base, 86, 25, 0.055, 0.665, max_width=int(w * 0.52))
    save_indexed(base, PERSONAL / "bg0.png")


def make_startup(icon):
    w, h = 280, 158
    base = gradient_bg(w, h, (54, 28, 16), (8, 5, 4)).convert("RGBA")
    base = Image.alpha_composite(base, paper_texture(icon, w, h).convert("RGBA"))

    # portrait on the left, title on the right
    src = icon.crop((0, 0, 256, 200))
    ph = 150
    pw = int(src.width * ph / src.height)
    portrait = src.resize((pw, ph), Image.Resampling.LANCZOS)
    portrait = ImageEnhance.Contrast(portrait).enhance(1.06)
    mask = Image.new("L", (pw, ph), 0)
    md = ImageDraw.Draw(mask)
    md.rectangle([6, 4, pw - 2, ph - 8], fill=255)
    mask = mask.filter(ImageFilter.GaussianBlur(10))
    base.paste(portrait.convert("RGBA"), (-8, 4), mask)

    # right-side dark panel
    panel = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    pd = ImageDraw.Draw(panel)
    pd.rectangle([145, 0, w, h], fill=(8, 5, 4, 150))
    base = Image.alpha_composite(base, panel)

    d = ImageDraw.Draw(base)
    tf = load_font(FONT_IMPACT, 30)
    sf = load_font(FONT_BOLD, 9)
    x = 154
    y = 35
    d.text((x + 2, y + 2), "BRUCE LEE", font=tf, fill=(60, 6, 4), stroke_width=2, stroke_fill=(30, 3, 2))
    d.text((x, y), "BRUCE LEE", font=tf, fill=CREAM, stroke_width=2, stroke_fill=INK)
    d.text((x, y), "BRUCE LEE", font=tf, fill=GOLD)
    d.line([(x + 1, y + 34), (x + 78, y + 34)], fill=RED, width=2)
    draw_spaced(d, (x + 1, y + 42), "DRAGON", sf, GOLD, spacing=2, stroke_width=1, stroke_fill=INK)
    draw_spaced(d, (x + 1, y + 56), "WARRIOR", sf, GOLD, spacing=2, stroke_width=1, stroke_fill=INK)
    base = vignette(base.convert("RGB"), 0.42).convert("RGBA")
    save_indexed(base, PERSONAL / "startup.png")


def make_icon(icon):
    im = icon.resize((128, 128), Image.Resampling.LANCZOS)
    im = ImageEnhance.Contrast(im).enhance(1.05)
    im = ImageEnhance.Sharpness(im).enhance(1.15)
    save_indexed(im, PERSONAL / "icon0.png")


def make_loading():
    """Raw RGB565 960x544 loading screen for the personal build."""
    src = Image.open(PERSONAL / "pic0.png").convert("RGB")
    if src.size != (960, 544):
        src = src.resize((960, 544), Image.Resampling.LANCZOS)
    buf = bytearray()
    for r, g, b in src.getdata():
        v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        buf += struct.pack("<H", v)
    out = ROOT / "extras" / "loading_personal.rgb565"
    out.write_bytes(buf)
    print(f"loading_personal.rgb565: 960x544 RGB565 {len(buf)} bytes")


def main():
    if not ICON_SRC.exists():
        raise SystemExit(f"Missing source icon: {ICON_SRC}")
    PERSONAL.mkdir(parents=True, exist_ok=True)
    icon = Image.open(ICON_SRC).convert("RGB")
    if icon.size != (256, 256):
        icon = icon.resize((256, 256), Image.Resampling.LANCZOS)

    make_pic0(icon)
    make_bg0(icon)
    make_startup(icon)
    make_icon(icon)
    make_loading()

    tpl = LIVE / "template.xml"
    if tpl.exists():
        shutil.copy2(tpl, PERSONAL / "template.xml")

    for name in ("icon0.png", "pic0.png", "bg0.png", "startup.png"):
        p = PERSONAL / name
        im = Image.open(p)
        print(f"{name}: {im.size} {im.mode} {p.stat().st_size} bytes")


if __name__ == "__main__":
    main()
