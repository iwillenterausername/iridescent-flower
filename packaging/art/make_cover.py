#!/usr/bin/env python3
"""Fuse the two supplied portraits into a 16:9 subculture video cover."""

from __future__ import annotations

import math
import random
from pathlib import Path

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageEnhance, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[2]
OLD = ROOT / "Assets" / "portrait.jpg"
NEW = ROOT / "Assets" / "source" / "iridescent_sketch.jpg"
PIXEL_B = ROOT / "Assets" / "avatar_b_pixel.png"
PIXEL_C = ROOT / "Assets" / "avatar_c_pixel.png"
OUT = ROOT / "docs" / "art"
FONT = ROOT / "Assets" / "Fonts" / "Orbitron.ttf"

random.seed(13)
np.random.seed(13)


def font(size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(str(FONT), size)


def line_art(source: Image.Image) -> Image.Image:
    rgb = np.asarray(source.convert("RGB"), dtype=np.float32)
    alpha = np.clip((255.0 - rgb.min(axis=2)) * 1.2, 0, 255)
    inverted = np.clip((255.0 - rgb) * 1.04 + 8.0, 0, 255)
    return Image.fromarray(np.dstack([inverted, alpha]).astype(np.uint8), "RGBA")


def screen(base: Image.Image, layer: Image.Image, opacity: float = 1.0) -> Image.Image:
    base_rgb = np.asarray(base.convert("RGB"), dtype=np.float32)
    layer_rgb = np.asarray(layer.convert("RGB"), dtype=np.float32)
    out = 255.0 - (255.0 - base_rgb) * (255.0 - layer_rgb * opacity) / 255.0
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8), "RGB")


def add_scanlines(image: Image.Image, alpha: int = 34, gap: int = 4) -> Image.Image:
    overlay = Image.new("RGBA", image.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    for y in range(0, image.height, gap):
        draw.line((0, y, image.width, y), fill=(0, 0, 0, alpha), width=1)
    return Image.alpha_composite(image.convert("RGBA"), overlay)


def add_halftone(image: Image.Image, alpha: int = 30) -> Image.Image:
    overlay = Image.new("RGBA", image.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    for y in range(8, image.height, 13):
        for x in range(8, image.width, 13):
            draw.ellipse((x, y, x + 2, y + 2), fill=(205, 180, 255, alpha))
    return Image.alpha_composite(image.convert("RGBA"), overlay)


def glitch(image: Image.Image, count: int = 20, x0: int = 0, x1: int | None = None) -> Image.Image:
    arr = np.asarray(image.convert("RGBA")).copy()
    h, w = arr.shape[:2]
    x1 = w if x1 is None else x1
    for _ in range(count):
        y = random.randrange(0, h - 10)
        height = random.randrange(2, 18)
        shift = random.randrange(-55, 56)
        arr[y:y + height, x0:x1] = np.roll(arr[y:y + height, x0:x1], shift, axis=1)
    return Image.fromarray(arr, "RGBA")


def build_cover() -> Image.Image:
    W, H = 1920, 1080
    canvas = Image.new("RGB", (W, H), (7, 4, 10))

    # Blurred, oversized portrait used as the room.
    room = Image.open(OLD).convert("RGB").resize((1700, 1700), Image.Resampling.LANCZOS)
    room = room.crop((300, 250, 2000, 1470)).resize((W, H), Image.Resampling.LANCZOS)
    room = room.filter(ImageFilter.GaussianBlur(42))
    room = ImageEnhance.Brightness(room).enhance(0.30)
    room = ImageEnhance.Color(room).enhance(1.7)
    canvas.paste(room, (0, 0))

    # Purple/magenta light field.
    yy, xx = np.mgrid[0:H, 0:W]
    radial = np.clip(1.0 - np.sqrt(((xx - 1280) / W) ** 2 + ((yy - 460) / H) ** 2) * 2.1, 0, 1)
    arr = np.asarray(canvas).astype(np.float32)
    arr[..., 0] += radial * 96
    arr[..., 2] += radial * 120
    arr[..., 1] += radial * 12
    canvas = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGB")

    draw = ImageDraw.Draw(canvas, "RGBA")
    draw.ellipse((1080, 82, 1858, 860), outline=(255, 46, 136, 100), width=3)
    draw.ellipse((1108, 110, 1830, 832), outline=(216, 255, 74, 55), width=1)
    draw.arc((1080, 82, 1858, 860), 200, 330, fill=(66, 232, 255, 170), width=6)

    # Two portraits, same axis: old painted face and new line drawing.
    old = Image.open(OLD).convert("RGB").resize((800, 800), Image.Resampling.LANCZOS)
    old = ImageEnhance.Contrast(old).enhance(1.25)
    old = ImageEnhance.Color(old).enhance(1.35)
    old = old.filter(ImageFilter.UnsharpMask(radius=2, percent=140, threshold=2))

    new = line_art(Image.open(NEW).convert("RGB").crop((70, 20, 850, 750)))
    new = new.resize((1020, 960), Image.Resampling.LANCZOS)
    new = ImageEnhance.Contrast(new).enhance(1.18)

    # Pixel avatar texture, enlarged and softened, keeps the older avatar family present.
    pixel = Image.open(PIXEL_B).convert("RGB").resize((1020, 1020), Image.Resampling.NEAREST)
    pixel = pixel.filter(ImageFilter.GaussianBlur(1.2))

    face_x, face_y = 760, 52
    canvas_rgba = canvas.convert("RGBA")
    old_layer = old.convert("RGBA")
    old_layer.putalpha(112)
    old_mask = Image.new("L", (800, 800), 0)
    md = ImageDraw.Draw(old_mask)
    md.rectangle((34, 34, 766, 766), fill=255)
    old_mask = old_mask.filter(ImageFilter.GaussianBlur(28))
    old_layer.putalpha(ImageChops.multiply(old_layer.getchannel("A"), old_mask))
    canvas_rgba.alpha_composite(old_layer, (face_x + 110, face_y + 106))

    # A screened pixel ghost, then the new sketch lines.
    ghost = Image.new("RGB", (W, H), (0, 0, 0))
    ghost.paste(pixel, (face_x, face_y))
    screen_layer = screen(canvas_rgba.convert("RGB"), ghost, 0.55).convert("RGBA")
    screen_layer.putalpha(255)
    canvas_rgba = Image.alpha_composite(canvas_rgba, screen_layer)

    # Offset chromatic ghosts of the sketch.
    cyan = Image.new("RGBA", new.size, (0, 0, 0, 0))
    cyan.putalpha(new.getchannel("A").filter(ImageFilter.GaussianBlur(1.1)))
    cyan = Image.composite(
        Image.new("RGBA", new.size, (35, 245, 255, 150)),
        Image.new("RGBA", new.size, (0, 0, 0, 0)),
        cyan.getchannel("A"),
    )
    magenta = Image.new("RGBA", new.size, (0, 0, 0, 0))
    magenta.putalpha(new.getchannel("A").filter(ImageFilter.GaussianBlur(2.3)))
    magenta = Image.composite(
        Image.new("RGBA", new.size, (255, 20, 156, 95)),
        Image.new("RGBA", new.size, (0, 0, 0, 0)),
        magenta.getchannel("A"),
    )
    canvas_rgba.alpha_composite(cyan, (face_x - 4, face_y + 2))
    canvas_rgba.alpha_composite(magenta, (face_x + 5, face_y - 1))
    new_alpha = new.copy()
    new_alpha.putalpha(new.getchannel("A").point(lambda v: int(v * 0.88)))
    canvas_rgba.alpha_composite(new_alpha, (face_x, face_y))

    # Glitch slices only around the face, then a global xerox texture.
    canvas_rgba = glitch(canvas_rgba, 4, 700, 1840)

    # Left-side typography.
    draw = ImageDraw.Draw(canvas_rgba, "RGBA")
    draw.text((86, 82), "L E G U I   S T U D I O", font=font(23), fill=(216, 255, 74, 235))
    draw.text((88, 118), "AUDIO OBJECT / 001", font=font(13), fill=(205, 180, 235, 210))

    draw.text((88, 390), "IRIDESCENT", font=font(104), fill=(250, 240, 255, 245))
    draw.text((88, 492), "FLOWER", font=font(104), fill=(255, 46, 136, 245))
    draw.text((92, 615), "VST3 / MULTI-BUS / 147 PARAMS", font=font(16), fill=(66, 232, 255, 225))
    draw.text((92, 650), "PAD_EASYSTRINGS", font=font(15), fill=(216, 255, 74, 210))

    # Thin framing mark and a flower glyph.
    draw.line((88, 754, 700, 754), fill=(255, 46, 136, 170), width=2)
    draw.text((88, 786), "NO PRESET / NO RULES", font=font(18), fill=(205, 180, 235, 220))

    # Small flower symbol made from the crop.
    flower = Image.open(ROOT / "packaging" / "Installer" / "icon.png").convert("RGBA")
    flower.thumbnail((96, 96), Image.Resampling.LANCZOS)
    canvas_rgba.alpha_composite(flower, (1710, 918))

    # Print texture.
    canvas_rgba = canvas_rgba.convert("RGB")
    canvas_rgba = screen(canvas_rgba, Image.open(PIXEL_C).convert("RGB").resize((W, H), Image.Resampling.NEAREST), 0.10)
    canvas_rgba = add_halftone(canvas_rgba.convert("RGBA"), 13)
    canvas_rgba = add_scanlines(canvas_rgba, 14, 4)
    canvas_rgba = canvas_rgba.convert("RGB")

    arr = np.asarray(canvas_rgba).astype(np.float32)
    arr += np.random.normal(0, 14, (H, W, 1))
    d = np.sqrt(((xx - W / 2) / (W / 2)) ** 2 + ((yy - H / 2) / (H / 2)) ** 2)
    arr *= np.clip(1.02 - d * 0.30, 0.46, 1.0)[..., None]
    canvas_rgba = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGB")
    canvas_rgba = ImageEnhance.Contrast(canvas_rgba).enhance(1.08)
    return canvas_rgba


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    cover = build_cover()
    cover.save(OUT / "iridescent-flower-cover-1920.jpg", quality=93, optimize=True, progressive=True)
    cover.resize((1280, 720), Image.Resampling.LANCZOS).save(
        OUT / "iridescent-flower-cover-1280x720.jpg", quality=92, optimize=True, progressive=True
    )
    print(OUT / "iridescent-flower-cover-1920.jpg")


if __name__ == "__main__":
    main()
