#!/usr/bin/env python3
"""Turns the host tests' PPM screenshots into PNG (doubled, like the calculator seen up close)."""
import pathlib
import sys

from PIL import Image

d = pathlib.Path(sys.argv[1])
n = 0
for p in sorted(d.glob("*.ppm")):
    Image.open(p).resize((640, 480), Image.NEAREST).save(p.with_suffix(".png"))
    n += 1
print(f"{n} screenshots converted to PNG in {d}")
