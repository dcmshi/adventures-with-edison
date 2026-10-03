"""Compares a port capture with a screenshot of the original.

    screendiff.py PORT.bmp ORIGINAL.png [OUT.png]

Prints the box around the differences and how many pixels differ (by more
than a small tolerance, for the original's colour rounding). OUT shows the
port on top and, below, the original darkened with the differing pixels
in red.
"""
import sys

from PIL import Image, ImageChops


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    port = Image.open(sys.argv[1]).convert("RGB")
    original = Image.open(sys.argv[2]).convert("RGB")
    diff = ImageChops.difference(port, original)
    mask = diff.convert("L").point(lambda v: 255 if v > 24 else 0)
    count = sum(1 for v in mask.getdata() if v)
    print(f"box {diff.getbbox()}, {count} pixels differ")
    if len(sys.argv) > 3:
        w, h = port.size
        out = Image.new("RGB", (w, 2 * h))
        out.paste(port, (0, 0))
        red = Image.new("RGB", (w, h), (255, 0, 0))
        out.paste(Image.composite(red, original.point(lambda v: v // 3), mask), (0, h))
        out.save(sys.argv[3])
    return 0


if __name__ == "__main__":
    sys.exit(main())
