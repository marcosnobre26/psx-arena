# -*- coding: utf-8 -*-
"""
png2tim.py - Converte uma imagem (PNG, JPG...) para TIM, o formato de textura do PS1

Requer Pillow:  pip install pillow

Uso:
  python3 tools/png2tim.py minha.png assets/tim/minha.tim --x 832 --y 256
  python3 tools/png2tim.py minha.png assets/tim/minha.tim --x 832 --y 256 --bpp 8

  --x/--y  posição na VRAM onde a textura será carregada.
           Cuidado para não sobrepor as texturas automáticas (veja ./dev vram).
           Use x múltiplo de 64 para cada textura ficar no início de uma
           "texture page" (o código assume isso).
  --bpp    16 (padrão, cores diretas), 8 (256 cores) ou 4 (16 cores).
           8 e 4 bits ocupam bem menos VRAM: na VRAM, uma textura de 8 bits
           ocupa metade da largura, e de 4 bits, um quarto. A paleta (CLUT)
           é colocada logo abaixo, na linha --clut-y (padrão 480).

Regras do PS1:
  - Tamanho máximo útil: 256x256. Prefira potências de 2 (32, 64, 128).
  - Pixel 100% preto (0,0,0) é TRANSPARENTE no PS1. Este script troca o
    preto puro por "quase preto" e usa transparência só onde o PNG tiver
    alpha < 128.

NORMALMENTE VOCÊ NÃO PRECISA DESTE SCRIPT: basta colocar o PNG em
assets/textures/ e compilar (tools/build_textures.py faz tudo sozinho).
Use este só se quiser controlar a posição na VRAM manualmente:
  1. Gere o .tim em assets/tim/  (ex.: assets/tim/minha.tim)
  2. Compile: ele vira tex_minha_t no código, como os outros.
  3. Ao desenhar: DRAWOPT opt = {0}; opt.tex = &tex_minha_t;
"""
import argparse
import struct
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow não encontrado. Instale com:  pip install pillow")


def to15(r, g, b, a=255, transparent_ok=True):
    if a < 128 and transparent_ok:
        return 0x0000                       # transparente
    v = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)
    if v == 0:
        v = 0x8000                          # preto opaco (bit STP ligado)
    return v


def pixels(im):
    """Lista de pixels linha a linha (compatível com Pillow antigo e novo)."""
    px = im.load()
    w, h = im.size
    return [px[x, y] for y in range(h) for x in range(w)]


def main():
    ap = argparse.ArgumentParser(description="PNG -> TIM (PlayStation 1)")
    ap.add_argument("entrada")
    ap.add_argument("saida")
    ap.add_argument("--x", type=int, required=True, help="posição X na VRAM")
    ap.add_argument("--y", type=int, required=True, help="posição Y na VRAM")
    ap.add_argument("--bpp", type=int, default=16, choices=(4, 8, 16))
    ap.add_argument("--clut-x", type=int, default=None, help="X da paleta (padrão = --x)")
    ap.add_argument("--clut-y", type=int, default=480, help="Y da paleta (padrão 480)")
    a = ap.parse_args()

    img = Image.open(a.entrada).convert("RGBA")
    w, h = img.size
    if w > 256 or h > 256:
        print("Aviso: texturas maiores que 256x256 não cabem numa página de textura.")

    out = bytearray()
    if a.bpp == 16:
        out += struct.pack("<II", 0x10, 0x02)
        px = [to15(*p) for p in pixels(img)]
        out += struct.pack("<IHHHH", 12 + w * h * 2, a.x, a.y, w, h)
        for v in px:
            out += struct.pack("<H", v)
    else:
        colors = 256 if a.bpp == 8 else 16
        ppw = 2 if a.bpp == 8 else 4        # pixels por "palavra" de 16 bits
        if w % ppw:
            sys.exit("Para %d bits, a largura precisa ser múltipla de %d." % (a.bpp, ppw))
        alpha = img.getchannel("A")
        q = img.convert("RGB").quantize(colors=colors - 1, method=Image.Quantize.MEDIANCUT)
        pal = q.getpalette()[: (colors - 1) * 3]
        # índice 0 = transparente; os outros deslocados em +1
        clut = [0x0000] + [to15(pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2], 255, False)
                           for i in range(len(pal) // 3)]
        clut += [0x8000] * (colors - len(clut))
        idx = [0 if al < 128 else (i + 1) for i, al in zip(pixels(q), pixels(alpha))]

        out += struct.pack("<II", 0x10, 0x08 | (1 if a.bpp == 8 else 0))
        cx = a.x if a.clut_x is None else a.clut_x
        out += struct.pack("<IHHHH", 12 + colors * 2, cx, a.clut_y, colors, 1)
        for v in clut:
            out += struct.pack("<H", v)
        words = w // ppw
        out += struct.pack("<IHHHH", 12 + words * h * 2, a.x, a.y, words, h)
        for y in range(h):
            for xw in range(words):
                v = 0
                for k in range(ppw):
                    p = idx[y * w + xw * ppw + k]
                    v |= (p & (0xFF if a.bpp == 8 else 0xF)) << (k * a.bpp)
                out += struct.pack("<H", v)

    with open(a.saida, "wb") as f:
        f.write(out)
    print("OK: %s (%dx%d, %d bits) em VRAM (%d,%d)" % (a.saida, w, h, a.bpp, a.x, a.y))


if __name__ == "__main__":
    main()
