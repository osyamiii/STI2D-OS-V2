#!/usr/bin/env python3
"""Icones blanches (masques 4 bits, format de gfx_mask) des cartes de STI2D-Play.
Usage : sti2d_assets.py launcher/assets/sti2d launcher/src/sti2d_assets.c"""
import math, os, sys
from PIL import Image, ImageDraw, ImageFilter

SRC, OUT = sys.argv[1:3]
S = 4  # sur-echantillonnage

def canvas(w, h):
    im = Image.new("L", (w * S, h * S)); return im, ImageDraw.Draw(im)

def star(w=46, h=44):
    im, d = canvas(w, h)
    cx, cy, R, r = w * S / 2, h * S / 2 + 2 * S, 22 * S, 9 * S
    d.polygon([(cx + (R if i % 2 == 0 else r) * math.sin(i * math.pi / 5),
                cy - (R if i % 2 == 0 else r) * math.cos(i * math.pi / 5)) for i in range(10)], fill=255)
    return im

def people(w=56, h=40):
    im, d = canvas(w, h)
    for cx, s in ((14, .78), (42, .78), (28, 1.0)):
        cx *= S
        d.ellipse((cx - 7 * s * S, (13 - 7 * s) * S + (1 - s) * 8 * S, cx + 7 * s * S, (13 + 7 * s) * S + (1 - s) * 8 * S), fill=255)
        d.rounded_rectangle((cx - 12 * s * S, (24 + (1 - s) * 6) * S, cx + 12 * s * S, 40 * S), int(9 * s * S), fill=255)
    for cx in (14, 42):  # separe les silhouettes
        pass
    return im

def gamepad(w=56, h=36):
    im, d = canvas(w, h)
    d.rounded_rectangle((2 * S, 6 * S, 54 * S, 32 * S), 13 * S, fill=255)
    d.ellipse((2 * S, 14 * S, 22 * S, 36 * S), fill=255)
    d.ellipse((34 * S, 14 * S, 54 * S, 36 * S), fill=255)
    d.rectangle((11 * S, 15 * S, 14 * S, 26 * S), fill=0)   # croix
    d.rectangle((7 * S, 19 * S, 18 * S, 22 * S), fill=0)
    for x, y in ((40, 16), (46, 21), (40, 26), (34, 21)):    # boutons
        d.ellipse(((x - 2.5) * S, (y - 2.5) * S, (x + 2.5) * S, (y + 2.5) * S), fill=0)
    return im

def matrices(w=60, h=46):
    im = Image.open(os.path.join(SRC, "matrices.jpeg")).convert("RGB")
    W, H = im.size
    im = im.crop((int(W * .31), int(H * .25), int(W * .71), int(H * .58)))  # le [M] blanc
    px = im.load()
    L = Image.new("L", im.size); lp = L.load()
    for y in range(im.size[1]):
        for x in range(im.size[0]):
            r, g, b = px[x, y]
            lp[x, y] = max(0, min(255, (min(r, g, b) - 150) * 3))
    return L.resize((w * S, h * S), Image.Resampling.LANCZOS)

def pack(name, im):
    im = im.resize((im.size[0] // S, im.size[1] // S), Image.Resampling.LANCZOS)
    w, h = im.size
    v = [min(15, (p + 8) // 17) for p in im.getdata()]
    if len(v) % 2: v.append(0)
    data = bytes(v[i] | (v[i + 1] << 4) for i in range(0, len(v), 2))
    rows = ",".join(str(b) for b in data)
    return (f"static const uint8_t {name}_data[{len(data)}] = {{{rows}}};\n"
            f"const np_icon_t sti_icon_{name} = {{{w}, {h}, {name}_data}};\n")

with open(OUT, "w") as f:
    f.write("/* Genere par tools/sti2d_assets.py. */\n#include \"gfx.h\"\n\n")
    for n, fn in (("star", star), ("people", people), ("gamepad", gamepad), ("matrices", matrices)):
        f.write(pack(n, fn()))
print("ok", OUT)
