#!/usr/bin/env python3
"""
make_title.py - Logotipo da tela de título "MATA ESCURA" (etapa 10).

Uso:
    python3 tools/make_title.py      # grava assets/textures/titulo-4bit.png

Uma fonte de pixels 5x7 feita à mão (só as letras do nome), ampliada 3x,
cor de osso com sombra vermelho-escura e alguns "escorridos" sob as letras.
4 bits (poucas cores, fundo transparente). Feito no projeto.
"""
import os

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), "assets", "textures", "titulo-4bit.png")

GLYPHS = {
    "M": ["10001", "11011", "10101", "10101", "10001", "10001", "10001"],
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "C": ["01111", "10000", "10000", "10000", "10000", "10000", "01111"],
    "U": ["10001", "10001", "10001", "10001", "10001", "10001", "01110"],
    "R": ["11110", "10001", "10001", "11110", "10100", "10010", "10001"],
    " ": ["00000"] * 7,
}
BONE = (215, 205, 180, 255)
SHADOW = (90, 10, 14, 255)
DRIP = (140, 18, 22, 255)
S = 3                                   # ampliação


def main():
    text = "MATA ESCURA"
    w = len(text) * 6 * S + 2 * S
    w = (w + 7) // 8 * 8                # largura múltipla de 8 (ver build_textures.py)
    h = 7 * S + 4 * S + 2 * S
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    px = im.load()

    def dot(x, y, c):
        for dy in range(S):
            for dx in range(S):
                if 0 <= x + dx < w and 0 <= y + dy < h:
                    px[x + dx, y + dy] = c

    drips = {2: 3, 6: 2, 13: 4, 19: 2, 26: 3, 33: 2, 40: 3, 47: 2, 55: 3, 60: 2}   # coluna -> comprimento
    for layer, c, off in ((0, SHADOW, S), (1, BONE, 0)):
        for i, ch in enumerate(text):
            g = GLYPHS[ch]
            for gy in range(7):
                for gx in range(5):
                    if g[gy][gx] == "1":
                        dot((i * 6 + gx) * S + off, gy * S + off, c)
    # escorridos: de alguns pixels da base das letras para baixo
    for col, n in drips.items():
        x = col * S
        base = 7 * S
        if x < w and px[x, base - 1][3]:
            for k in range(n):
                dot(x, base + k * S // 2, DRIP)
    im.save(OUT)
    print("Logotipo: %s (%dx%d)" % (os.path.relpath(OUT, os.path.dirname(HERE)), w, h))


if __name__ == "__main__":
    main()
