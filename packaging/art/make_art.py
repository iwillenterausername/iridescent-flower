#!/usr/bin/env python3
"""Generate the inverted Iridescent Flower poster and the setup flower icon."""

from __future__ import annotations

import math
import random
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "Assets" / "source" / "iridescent_sketch.jpg"
OUT = ROOT / "docs" / "art"
ICON_DIR = ROOT / "packaging" / "Installer"

random.seed(7)
np.random.seed(7)


def font(size: int) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(str(ROOT / "Assets" / "Fonts" / "Orbitron.ttf"), size)


def line_layer(source: Image.Image) -> Image.Image:
    """Invert the sketch into a transparent, bone-white line layer."""
    rgb = np.asarray(source.convert("RGB"), dtype=np.float32)
    alpha = np.clip((255.0 - rgb.min(axis=2)) * 1.16, 0, 255)

    inverted = 255.0 - rgb
    inverted = np.clip(inverted * 1.04 + 7.0, 0, 255)

    layer = np.dstack([inverted, alpha]).astype(np.uint8)
    return Image.fromarray(layer, "RGBA")


def add_scanlines(image: Image.Image, alpha: int = 20, gap: int = 4) -> Image.Image:
    overlay = Image.new("RGBA", image.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)

    for y in range(0, image.height, gap):
        draw.line((0, y, image.width, y), fill=(0, 0, 0, alpha), width=1)

    return Image.alpha_composite(image.convert("RGBA"), overlay)


def add_halftone(image: Image.Image, alpha: int = 24) -> Image.Image:
    overlay = Image.new("RGBA", image.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)

    for y in range(12, image.height, 19):
        for x in range(12, image.width, 19):
            draw.ellipse((x, y, x + 2, y + 2), fill=(190, 160, 255, alpha))

    return Image.alpha_composite(image.convert("RGBA"), overlay)


def add_noise(image: Image.Image, amount: int = 17) -> Image.Image:
    arr = np.asarray(image.convert("RGB"), dtype=np.int16)
    noise = np.random.normal(0, amount, arr.shape[:2])[..., None]
    return Image.fromarray(np.clip(arr + noise, 0, 255).astype(np.uint8), "RGB")


def build_poster() -> Image.Image:
    source = Image.open(SOURCE).convert("RGB")
    size = 2000
    center = size // 2

    # Deep purple field, brightest in the middle of the ring.
    yy, xx = np.mgrid[0:size, 0:size]
    radial = np.clip(
        1.0 - np.sqrt(((xx - center) / size) ** 2 + ((yy - center * 0.94) / size) ** 2) * 1.55,
        0,
        1,
    )
    arr = np.zeros((size, size, 3), dtype=np.float32)
    arr[..., 0] = 18 + radial * 104
    arr[..., 1] = 5 + radial * 20
    arr[..., 2] = 27 + radial * 116
    arr += np.random.normal(0, 5, (size, size, 1))
    canvas = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGB")
    draw = ImageDraw.Draw(canvas, "RGBA")

    # One centered ring. This is the visual anchor for the portrait.
    draw.ellipse((150, 150, 1850, 1850), outline=(255, 46, 136, 80), width=3)
    draw.ellipse((190, 190, 1810, 1810), outline=(216, 255, 74, 45), width=1)
    draw.arc((150, 150, 1850, 1850), 205, 330, fill=(66, 232, 255, 150), width=5)
    draw.text((92, 84), "L E G U I   S T U D I O", font=font(23), fill=(216, 255, 74, 225))
    draw.text((94, 120), "AUDIO OBJECT / 001", font=font(13), fill=(190, 160, 220, 190))

    # Large portrait, centered inside the ring.
    lines = line_layer(source)
    scale = 1580 / lines.width
    lines = lines.resize((1580, int(lines.height * scale)), Image.Resampling.LANCZOS)
    lines = lines.rotate(-1.6, resample=Image.Resampling.BICUBIC, expand=True)

    x = (size - lines.width) // 2
    y = 205
    canvas = canvas.convert("RGBA")

    # Soft bloom behind the line art.
    bloom = Image.new("RGBA", lines.size, (0, 0, 0, 0))
    bloom.putalpha(lines.getchannel("A").filter(ImageFilter.GaussianBlur(26)))
    bloom_col = Image.new("RGBA", lines.size, (132, 32, 210, 46))
    bloom = Image.composite(bloom_col, Image.new("RGBA", lines.size, (0, 0, 0, 0)), bloom.getchannel("A"))
    canvas.alpha_composite(bloom, (x, y))

    # Restrained cyan/magenta offset.
    cyan = Image.new("RGBA", lines.size, (0, 0, 0, 0))
    cyan.putalpha(lines.getchannel("A").filter(ImageFilter.GaussianBlur(1.0)))
    cyan = Image.composite(
        Image.new("RGBA", lines.size, (26, 240, 255, 90)),
        Image.new("RGBA", lines.size, (0, 0, 0, 0)),
        cyan.getchannel("A"),
    )
    magenta = Image.new("RGBA", lines.size, (0, 0, 0, 0))
    magenta.putalpha(lines.getchannel("A").filter(ImageFilter.GaussianBlur(2.2)))
    magenta = Image.composite(
        Image.new("RGBA", lines.size, (255, 20, 156, 65)),
        Image.new("RGBA", lines.size, (0, 0, 0, 0)),
        magenta.getchannel("A"),
    )

    canvas.alpha_composite(cyan, (x - 4, y + 2))
    canvas.alpha_composite(magenta, (x + 5, y - 2))
    canvas.alpha_composite(lines, (x, y))

    # Only a few glitch slices; keep the face readable.
    arr = np.asarray(canvas).copy()
    for _ in range(6):
        gy = random.randrange(260, 1710)
        gh = random.randrange(2, 10)
        shift = random.randrange(-22, 23)
        arr[gy:gy + gh, 260:1740] = np.roll(arr[gy:gy + gh, 260:1740], shift, axis=1)
    canvas = Image.fromarray(arr, "RGBA")

    canvas = add_halftone(canvas, 22)
    canvas = add_scanlines(canvas, 18, 4)
    canvas = add_noise(canvas.convert("RGB"), 16).convert("RGBA")

    arr = np.asarray(canvas).astype(np.float32)
    d = np.sqrt(((xx - center) / center) ** 2 + ((yy - center) / center) ** 2)
    vignette = np.clip(1.05 - d * 0.30, 0.42, 1.0)[..., None]
    arr[..., :3] *= vignette
    canvas = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGBA")

    canvas = ImageEnhance.Color(canvas.convert("RGB")).enhance(1.14)
    canvas = ImageEnhance.Contrast(canvas).enhance(1.07)
    return canvas


def build_icon() -> Image.Image:
    """The flower itself, with no ring, star or sticker frame."""
    source = Image.open(SOURCE).convert("RGB")
    crop = source.crop((490, 25, 760, 300)).resize((440, 440), Image.Resampling.LANCZOS)

    rgb = np.asarray(crop, dtype=np.float32)
    red = (rgb[..., 0] > 90) & (rgb[..., 0] > rgb[..., 1] * 1.18) & (rgb[..., 0] > rgb[..., 2] * 1.08)
    h, w = red.shape
    yy, xx = np.mgrid[0:h, 0:w]
    center = ((xx - w * 0.52) ** 2 + (yy - h * 0.52) ** 2) < (w * 0.31) ** 2
    keep = red | center

    # Transparent flower with a soft red petal fill and the original ink.
    red_mask = Image.fromarray((red * 255).astype(np.uint8), "L")
    red_mask = red_mask.filter(ImageFilter.MaxFilter(9)).filter(ImageFilter.GaussianBlur(2))
    petal_fill = Image.new("RGBA", (440, 440), (255, 31, 74, 150))
    icon = Image.new("RGBA", (440, 440), (0, 0, 0, 0))
    icon.paste(petal_fill, (0, 0), red_mask)

    alpha = np.where(keep, np.clip((255.0 - rgb.min(axis=2)) * 1.25, 0, 255), 0)
    ink = np.dstack([rgb, alpha]).astype(np.uint8)
    icon.alpha_composite(Image.fromarray(ink, "RGBA"))

    # Add a little weight so the flower survives 16 px.
    glow = icon.filter(ImageFilter.GaussianBlur(3))
    glow.putalpha(glow.getchannel("A").point(lambda v: int(v * 0.45)))
    icon = Image.alpha_composite(glow, icon)

    icon.save(ICON_DIR / "icon.png")
    icon.save(
        ICON_DIR / "icon.ico",
        sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)],
    )
    return icon


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
