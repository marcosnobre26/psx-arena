#!/usr/bin/env python3
"""
make_ambient_models.py - Modelos e textura do AMBIENTE (etapa 09). Só visual.

Uso:
    python3 tools/make_ambient_models.py

Saída (nomes FIXOS: para trocar por um modelo do Blender, exporte um objeto
com o mesmo nome na coleção PSX; o código não muda):
    models/passaro_a.h   pássaro, asas para CIMA   (2 triângulos)
    models/passaro_b.h   pássaro, asas para BAIXO  (2 triângulos)
    models/vulto.h       silhueta alta e magra que cruza a trilha ao longe (~12 faces)
    models/bicho.h       bicho pequeno que salta entre troncos (~10 faces, olhos que brilham)
    models/olhos.h       par de olhos que brilham no escuro (2 quads, sem luz)
    assets/textures/lua-4bit.png   a lua, 32x32 (fundo transparente)

Coordenadas do Blender em metros, Z para cima, frente para -Y (como em
make_combat_models.py). Silhuetas: cores escuras, quase pretas. Feitos no
projeto: nada no CREDITS.md.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from make_default_assets import Shape  # noqa: E402

BLACK = (12, 12, 14)
DARK = (30, 26, 24)
FUR = (120, 95, 70)
EYE = (255, 235, 150)


def make_passaro(up):
    """V de 2 triângulos (as asas); o corpo é a aresta do meio."""
    s = Shape(["Asa"])
    z = 0.12 if up else -0.10
    nose, tail = (0, -0.12, 0), (0, 0.10, 0)
    for side in (-1, 1):
        tip = (side * 0.35, 0.02, z)
        s.poly([nose, tip, tail], BLACK, 0, outward=(0, 0, 1))
    return s


def make_vulto():
    """Alto e magro, ombros caídos: lê-se como 'alguém' de longe."""
    s = Shape(["Corpo"])
    s.box(-0.16, -0.10, 0.0, 0.16, 0.10, 1.55, BLACK, 0)          # corpo (5)
    s.box(-0.11, -0.12, 1.55, 0.11, 0.08, 1.85, BLACK, 0)         # cabeça (5)
    s.poly([(-0.16, 0, 1.5), (-0.30, 0, 0.8), (-0.24, 0, 0.75)], BLACK, 0, outward=(0, -1, 0))
    s.poly([(0.16, 0, 1.5), (0.30, 0, 0.8), (0.24, 0, 0.75)], BLACK, 0, outward=(0, -1, 0))
    return s


def make_bicho():
    """Corpo, cabeça, cauda e OLHOS que brilham (no escuro é o que se vê)."""
    s = Shape(["Pelo", "Olhos"])
    s.box(-0.07, -0.14, 0.0, 0.07, 0.10, 0.12, FUR, 0)            # corpo (5)
    s.poly([(-0.05, -0.14, 0.04), (0.05, -0.14, 0.04), (0, -0.24, 0.08)], DARK, 0, outward=(0, -1, 0))
    s.poly([(-0.04, 0.10, 0.08), (0.04, 0.10, 0.08), (0, 0.30, 0.22)], DARK, 0, outward=(0, 1, 0))
    s.poly([(0.04, 0.10, 0.08), (-0.04, 0.10, 0.08), (0, 0.30, 0.22)], DARK, 0, outward=(0, -1, 0))
    for x in (-0.045, 0.045):                                      # olhos (2), sem luz
        s.poly([(x - 0.03, -0.145, 0.06), (x + 0.03, -0.145, 0.06), (x + 0.03, -0.145, 0.12),
                (x - 0.03, -0.145, 0.12)], EYE, 1, outward=(0, -1, 0), unlit=True)
    return s


def make_olhos():
    s = Shape(["Olhos"])
    for x in (-0.07, 0.07):
        s.poly([(x - 0.025, 0, -0.02), (x + 0.025, 0, -0.02), (x + 0.025, 0, 0.02), (x - 0.025, 0, 0.02)],
               EYE, 0, outward=(0, -1, 0), unlit=True)
    return s


def make_lua(path):
    from PIL import Image, ImageDraw
    im = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.ellipse([2, 2, 29, 29], fill=(205, 210, 220, 255))
    for (x, y, r, c) in ((10, 11, 4, 170), (19, 17, 5, 180), (13, 21, 3, 165), (21, 8, 2, 175)):
        d.ellipse([x - r, y - r, x + r, y + r], fill=(c, c + 4, c + 12, 255))
    im.save(path)


def main():
    print("Modelos do ambiente (etapa 09):")
    make_passaro(True).save("passaro_a")
    make_passaro(False).save("passaro_b")
    make_vulto().save("vulto")
    make_bicho().save("bicho")
    make_olhos().save("olhos")
    tex = os.path.join(os.path.dirname(HERE), "assets", "textures", "lua-4bit.png")
    make_lua(tex)
    print("  " + os.path.relpath(tex, os.path.dirname(HERE)))


if __name__ == "__main__":
    main()
