#!/usr/bin/env python3
"""Quantize a PNG to a Pebble-safe color palette.

Pebble's color docs describe 64 supported colors. This script uses the common
4-level-per-channel RGB cube (00, 55, AA, FF) to map every non-transparent pixel
to the nearest Pebble-safe value while preserving alpha.

Usage:
  python3 tools/pebble_safe_png.py input.png output.png
  python3 tools/pebble_safe_png.py input.png --print
"""

from __future__ import annotations

import argparse
from collections import Counter
from pathlib import Path
from typing import Iterable, Tuple

from PIL import Image

PALETTE_LEVELS = (0, 85, 170, 255)
PALETTE = [(r, g, b) for r in PALETTE_LEVELS for g in PALETTE_LEVELS for b in PALETTE_LEVELS]


def nearest_palette_color(rgb: Tuple[int, int, int]) -> Tuple[int, int, int]:
    r, g, b = rgb
    best = PALETTE[0]
    best_distance = None
    for candidate in PALETTE:
        dr = candidate[0] - r
        dg = candidate[1] - g
        db = candidate[2] - b
        distance = dr * dr + dg * dg + db * db
        if best_distance is None or distance < best_distance:
            best = candidate
            best_distance = distance
    return best


def iter_visible_pixels(image: Image.Image) -> Iterable[Tuple[int, int, Tuple[int, int, int, int]]]:
    rgba = image.convert("RGBA")
    for y in range(rgba.height):
        for x in range(rgba.width):
            pixel = rgba.getpixel((x, y))
            if pixel[3] > 0:
                yield x, y, pixel


def first_visible_pixel(image: Image.Image):
    for x, y, pixel in iter_visible_pixels(image):
        return x, y, pixel
    return None


def dominant_visible_color(image: Image.Image):
    counter = Counter()
    for _, _, pixel in iter_visible_pixels(image):
        counter[pixel[:3]] += 1
    return counter.most_common(1)[0] if counter else None


def quantize_image(image: Image.Image) -> Image.Image:
    rgba = image.convert("RGBA")
    out = Image.new("RGBA", rgba.size)
    for y in range(rgba.height):
        for x in range(rgba.width):
            r, g, b, a = rgba.getpixel((x, y))
            if a == 0:
                out.putpixel((x, y), (0, 0, 0, 0))
            else:
                out.putpixel((x, y), (*nearest_palette_color((r, g, b)), a))
    return out


def format_rgb(rgb: Tuple[int, int, int]) -> str:
    return f"rgb({rgb[0]}, {rgb[1]}, {rgb[2]})"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="Input PNG path")
    parser.add_argument("output", nargs="?", type=Path, help="Output PNG path")
    parser.add_argument("--print", dest="do_print", action="store_true", help="Print color summary")
    args = parser.parse_args()

    image = Image.open(args.input)
    first = first_visible_pixel(image)
    dominant = dominant_visible_color(image)
    quantized = quantize_image(image)

    if args.output:
        quantized.save(args.output)

    if args.do_print or not args.output:
        print(f"input: {args.input}")
        if first:
          x, y, pixel = first
          safe = nearest_palette_color(pixel[:3])
          print(f"first visible: ({x}, {y}) {pixel[:3]} -> {safe}")
        else:
          print("first visible: none")
        if dominant:
            rgb, count = dominant
            safe = nearest_palette_color(rgb)
            print(f"dominant: {rgb} x{count} -> {safe}")
        else:
            print("dominant: none")
        if args.output:
            print(f"wrote: {args.output}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
