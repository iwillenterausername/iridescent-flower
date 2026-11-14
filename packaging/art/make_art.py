#!/usr/bin/env python3
"""Generate the inverted Iridescent Flower poster and the setup flower icon."""

from __future__ import annotations

import math
import random
from pathlib import Path

import numpy as np
from PIL import Image, ImageChops, ImageDraw, ImageEnhance, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "Assets" / "source" / "iridescent_sketch.jpg"
OUT = ROOT / "docs" / "art"
ICON_DIR = ROOT / "packaging" / "Installer"

random.seed(7)
np.random.seed(7)


def font(size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(str(ROOT / "Assets" / "Fonts" / "Orbitron.ttf"), size)


def line_layer(source: Image.Image) -> Image.Image:
    """Turn the white-paper sketch into an inverted transparent line layer."""
    rgb = np.asarray(source.convert("RGB"), dtype=np.float32)
    alpha = 255.0 - rgb.min(axis=2)
    alpha = np.clip(alpha * 1.15, 0, 255)

    inverted = 255.0 - rgb
    # Lift the black ink into bone white and keep the red flower as cyan.
    inverted = np.clip(inverted * 1.05 + 8.0, 0, 255)

    layer = np.dstack([inverted, alpha]).astype(np.uint8)
    return Image.fromarray(layer, "RGBA")


def add_glitch_bands(image: Image.Image, amount: int = 18) -> Image.Image:
    arr = np.asarray(image).copy()
    h, w = arr.shape[:2]

    for _ in range(amount):
        y = random.randrange(0, h - 8)
        height = random.randrange(2, 14)
        shift = random.randrange(-24, 25)
        arr[y:y + height] = np.roll(arr[y:y + height], shift, axis=1)

    return Image.fromarray(arr)


def add_scanlines(image: Image.Image, alpha: int = 24, gap: int = 4) -> Image.Image:
    overlay = Image.new("RGBA", image.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)

    for y in range(0, image.height, gap):
        draw.line((0, y, image.width, y), fill=(0, 0, 0, alpha), width=1)

    return Image.alpha_composite(image.convert("RGBA"), overlay)


def add_halftone(image: Image.Image, alpha: int = 34) -> Image.Image:
    overlay = Image.new("RGBA", image.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    step = 17
    radius = 2

    for y in range(10, image.height, step):
        for x in range(10, image.width, step):
            draw.ellipse((x, y, x + radius, y + radius), fill=(190, 160, 255, alpha))

    return Image.alpha_composite(image.convert("RGBA"), overlay)


def add_noise(image: Image.Image, amount: int = 20) -> Image.Image:
    arr = np.asarray(image.convert("RGB"), dtype=np.int16)
    noise = np.random.normal(0, amount, arr.shape[:2])[..., None]
    arr = np.clip(arr + noise, 0, 255).astype(np.uint8)
    return Image.fromarray(arr, "RGB")


def add_tape(draw: ImageDraw.ImageDraw, box, angle_text: str) -> None:
    x0, y0, x1, y1 = box
    points = [
        (x0 + random.randrange(-8, 8), y0),
        (x1 + random.randrange(-8, 8), y0 + random.randrange(-5, 5)),
        (x1 + random.randrange(-8, 8), y1),
        (x0 + random.randrange(-8, 8), y1 + random.randrange(-5, 5)),
    ]
    draw.polygon(points, fill=(214, 198, 167, 190), outline=(255, 245, 210, 120))
    draw.text((x0 + 18, y0 + 12), angle_text, font=font(15), fill=(20, 12, 18, 230))


def build_poster() -> Image.Image:
    source = Image.open(SOURCE).convert("RGB")
    size = 2000
    canvas = Image.new("RGB", (size, size), (7, 4, 11))

    # Soft colored bloom.
    yy, xx = np.mgrid[0:size, 0:size]
    radial = np.clip(1.0 - np.sqrt(((xx - size * 0.48) / size) ** 2 + ((yy - size * 0.46) / size) ** 2) * 1.8, 0, 1)
    arr = np.zeros((size, size, 3), dtype=np.float32)
    arr[..., 0] = 28 + radial * 105
    arr[..., 1] = 8 + radial * 25
    arr[..., 2] = 38 + radial * 120
    arr += np.random.normal(0, 6, (size, size, 1))
    arr = np.clip(arr, 0, 255).astype(np.uint8)
    canvas = Image.fromarray(arr, "RGB")

    draw = ImageDraw.Draw(canvas, "RGBA")

    # Large offset rings and Y2K orbit marks.
    draw.ellipse((60, 90, 1260, 1290), outline=(255, 46, 136, 55), width=3)
    draw.ellipse((170, 200, 1150, 1180), outline=(216, 255, 74, 40), width=1)
    draw.arc((90, 120, 1210, 1240), 190, 330, fill=(66, 232, 255, 120), width=5)
    draw.text((92, 72), "L E G U I   S T U D I O", font=font(24), fill=(216, 255, 74, 220))
    draw.text((92, 108), "AUDIO OBJECT / 001", font=font(14), fill=(190, 160, 220, 200))

    # Grid and registration marks.
    for x in range(0, size, 100):
        draw.line((x, 0, x, size), fill=(255, 255, 255, 9), width=1)
    for y in range(0, size, 100):
        draw.line((0, y, size, y), fill=(255, 255, 255, 9), width=1)

    # Character layer, slightly rotated and sliced.
    lines = line_layer(source)
    scale = 1280 / lines.width
    lines = lines.resize((1280, int(lines.height * scale)), Image.Resampling.LANCZOS)
    lines = lines.rotate(-2.2, resample=Image.Resampling.BICUBIC, expand=True)

    # Chromatic split.
    cyan = Image.new("RGBA", lines.size, (0, 0, 0, 0))
    cyan.putalpha(lines.getchannel("A").filter(ImageFilter.GaussianBlur(1.1)))
    cyan_col = Image.new("RGBA", lines.size, (30, 245, 255, 135))
    cyan = Image.composite(cyan_col, Image.new("RGBA", lines.size, (0, 0, 0, 0)), cyan.getchannel("A"))
    magenta = Image.new("RGBA", lines.size, (0, 0, 0, 0))
    magenta.putalpha(lines.getchannel("A").filter(ImageFilter.GaussianBlur(2.0)))
    magenta_col = Image.new("RGBA", lines.size, (255, 20, 156, 95))
    magenta = Image.composite(magenta_col, Image.new("RGBA", lines.size, (0, 0, 0, 0)), magenta.getchannel("A"))

    x = (size - lines.width) // 2 + 100
    y = 240
    canvas = canvas.convert("RGBA")
    canvas.alpha_composite(cyan, (x, y))
    canvas.alpha_composite(magenta, (x, y))
    canvas.alpha_composite(lines, (x, y))

    # A few hard glitch strips over the portrait.
    arr = np.asarray(canvas).copy()
    for _ in range(12):
        gy = random.randrange(260, 1490)
        gh = random.randrange(3, 17)
        shift = random.randrange(-35, 36)
        arr[gy:gy + gh, 260:1540] = np.roll(arr[gy:gy + gh, 260:1540], shift, axis=1)
    canvas = Image.fromarray(arr, "RGBA")

    draw = ImageDraw.Draw(canvas, "RGBA")

    # Xerox bars, tape and stickers.
    add_tape(draw, (130, 1360, 560, 1450), "MULTI-BUS / 3 x 8")
    add_tape(draw, (1420, 360, 1820, 455), "VST3 / X64")

    draw.rectangle((134, 1540, 640, 1620), fill=(8, 5, 12, 210), outline=(216, 255, 74, 190), width=2)
    draw.text((154, 1556), "IRIDESCENT FLOWER", font=font(26), fill=(245, 234, 255, 235))
    draw.text((156, 1590), "NO PRESET / NO RULES", font=font(12), fill=(255, 46, 136, 220))

    # Barcode block.
    bx, by = 1450, 1450
    draw.rectangle((bx, by, bx + 360, by + 155), fill=(235, 225, 205, 230))
    bxx = bx + 14
    for i in range(34):
        w = random.randrange(2, 8)
        draw.rectangle((bxx, by + 12, bxx + w, by + 95), fill=(10, 5, 12, 240))
        bxx += w + random.randrange(2, 6)
    draw.text((bx + 14, by + 108), "IF-001 // LEGUI STUDIO", font=font(13), fill=(10, 5, 12, 240))

    # Handwritten title stays from the source; add a small red flower echo.
    draw.text((1450, 1710), "LEGUI", font=font(86), fill=(255, 46, 136, 200))
    draw.text((1450, 1795), "STUDIO", font=font(28), fill=(216, 255, 74, 230))
    draw.text((92, 1810), "A // SIGNAL", font=font(20), fill=(66, 232, 255, 220))
    draw.text((92, 1842), "B // SIGNAL", font=font(20), fill=(255, 46, 136, 220))

    # Stars / sticker marks.
    for sx, sy, sr in [(1810, 120, 28), (120, 1760, 18), (1760, 1710, 12)]:
        pts = []
        for i in range(10):
            a = -math.pi / 2 + i * math.pi / 5
            r = sr if i % 2 == 0 else sr * 0.42
            pts.append((sx + math.cos(a) * r, sy + math.sin(a) * r))
        draw.polygon(pts, fill=(216, 255, 74, 220))

    # Print texture, halftone, scanlines, vignette.
    canvas = add_halftone(canvas, 26)
    canvas = add_scanlines(canvas, 22, 4)
    canvas = add_noise(canvas.convert("RGB"), 18).convert("RGBA")

    arr = np.asarray(canvas).astype(np.float32)
    yy, xx = np.mgrid[0:size, 0:size]
    d = np.sqrt(((xx - size / 2) / (size / 2)) ** 2 + ((yy - size / 2) / (size / 2)) ** 2)
    vignette = np.clip(1.08 - d * 0.34, 0.38, 1.0)[..., None]
    arr[..., :3] *= vignette
    canvas = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGBA")

    # Lift magenta shadows a touch.
    canvas = ImageEnhance.Color(canvas.convert("RGB")).enhance(1.16)
    canvas = ImageEnhance.Contrast(canvas).enhance(1.08)

    return canvas


def build_icon() -> Image.Image:
    """A cream sticker using the actual cropped flower, readable at 16 px."""
    source = Image.open(SOURCE).convert("RGB")
    crop = source.crop((480, 25, 760, 305)).resize((390, 390), Image.Resampling.LANCZOS)

    rgb = np.asarray(crop, dtype=np.float32)
    red = (rgb[..., 0] > 90) & (rgb[..., 0] > rgb[..., 1] * 1.18) & (rgb[..., 0] > rgb[..., 2] * 1.08)
    h, w = red.shape
    yy, xx = np.mgrid[0:h, 0:w]
    center = ((xx - w * 0.53) ** 2 + (yy - h * 0.52) ** 2) < (w * 0.32) ** 2
    keep = red | center
    alpha = np.where(keep, np.clip((255.0 - rgb.min(axis=2)) * 1.3, 0, 255), 0)
    ink = np.dstack([rgb, alpha]).astype(np.uint8)
    ink_img = Image.fromarray(ink, "RGBA")

    red_mask = Image.fromarray((red * 255).astype(np.uint8), "L")
    red_mask = red_mask.filter(ImageFilter.MaxFilter(9)).filter(ImageFilter.GaussianBlur(2))
    red_tint = Image.new("RGBA", (390, 390), (255, 31, 74, 55))

    base = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
    draw = ImageDraw.Draw(base, "RGBA")
    draw.ellipse((8, 8, 504, 504), fill=(8, 4, 14, 255), outline=(216, 255, 74, 255), width=8)
    draw.ellipse((26, 26, 486, 486), outline=(255, 46, 136, 180), width=2)

    draw.ellipse((48, 48, 464, 464), fill=(244, 235, 218, 255), outline=(255, 31, 74, 220), width=5)
    for _ in range(450):
        x = random.randrange(70, 443)
        y = random.randrange(70, 443)
        if (x - 256) ** 2 + (y - 256) ** 2 < 195 ** 2:
            draw.point((x, y), fill=(30, 20, 26, random.randrange(15, 55)))

    base.paste(red_tint, (61, 61), red_mask)
    base.alpha_composite(ink_img, (61, 61))

    draw.arc((48, 48, 464, 464), 205, 330, fill=(255, 46, 136, 255), width=6)
    pts = []
    for i in range(10):
        a = -math.pi / 2 + i * math.pi / 5
        r = 25 if i % 2 == 0 else 10
        pts.append((448 + math.cos(a) * r, 448 + math.sin(a) * r))
    draw.polygon(pts, fill=(216, 255, 74, 255))

    icon_png = base
    icon_png.save(ICON_DIR / "icon.png")
    base.save(ICON_DIR / "icon.ico", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    return base

def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    poster = build_poster()
    poster.save(OUT / "iridescent-flower-inverted.jpg", quality=92, optimize=True, progressive=True)
    poster.resize((1200, 1200), Image.Resampling.LANCZOS).save(
        OUT / "iridescent-flower-inverted-1200.jpg", quality=90, optimize=True, progressive=True
    )
    build_icon()

    print(OUT / "iridescent-flower-inverted.jpg")
    print(ICON_DIR / "icon.ico")


if __name__ == "__main__":
    main()
