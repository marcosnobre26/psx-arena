# -*- coding: utf-8 -*-
"""
build_textures.py - Converte TODAS as imagens de assets/textures/ em .TIM

Roda sozinho durante a compilação (o CMakeLists.txt chama este script).
Você só precisa soltar o PNG na pasta.

Regras de nome do arquivo:
    madeira.png         -> textura 16 bits (cores diretas)      símbolo: tex_madeira_t
    madeira-8bit.png    -> textura 8 bits (256 cores, metade da VRAM)
    madeira-4bit.png    -> textura 4 bits (16 cores, 1/4 da VRAM)
No código o nome é sempre o mesmo, sem o sufixo:  opt.tex = &tex_madeira_t;

Tamanho: até 256x256. Prefira 32, 64 ou 128 (potências de 2).
Pixels com alpha < 128 ficam transparentes; preto puro vira "quase preto".

A posição de cada textura na VRAM é escolhida automaticamente (área livre
x 640..959). O resultado fica em build/textures/vram.txt para conferência.

Uso manual: python3 build_textures.py --out PASTA img1.png img2.png ...
"""
import argparse
import os
import re
import struct
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow não encontrado (pip install pillow). No Docker do projeto ele já vem instalado.")

# Região da VRAM reservada para texturas (em unidades de 16 bits)
AREA_X0, AREA_X1 = 640, 960
AREA_Y0, AREA_Y1 = 0, 480        # linhas 480..511 ficam para as paletas (CLUT)
CLUT_Y0 = 480


def c_name(stem):
    """Mesmo nome que o CMake gera: tira o sufixo -Nbit e vira identificador C."""
    stem = re.sub(r"-(4|8|16)bits?$", "", stem, flags=re.IGNORECASE)
    name = re.sub(r"[^A-Za-z0-9_]", "_", stem)
    if not name or name[0].isdigit():
        name = "_" + name
    return name


def bpp_of(stem):
    m = re.search(r"-(4|8|16)bits?$", stem, flags=re.IGNORECASE)
    return int(m.group(1)) if m else 16


def to15(r, g, b, a=255, alpha_ok=True):
    if alpha_ok and a < 128:
        return 0x0000
    v = (r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)
    return v if v else 0x8000


def pixels(im):
    px = im.load()
    w, h = im.size
    return [px[x, y] for y in range(h) for x in range(w)]


class Packer:
    """Empacotador em "prateleiras" dentro da área de texturas.
    Garante que nenhuma textura cruze a linha y=256 (limite de página) e que
    u0 + largura <= 256 dentro da página."""

    def __init__(self):
        self.shelves = []   # [x_atual, y, altura, banda_fim]
        self.bands = [(AREA_Y0, 256), (256, AREA_Y1)]
        self.next_y = {b: b[0] for b in self.bands}

    def _fits_page(self, x, vw, mult):
        return (x & 63) * mult + vw * mult <= 256

    def place(self, vw, h, mult):
        # tenta prateleiras existentes
        for sh in self.shelves:
            x, y, sh_h, band_end = sh
            if h <= sh_h:
                if not self._fits_page(x, vw, mult):
                    x = (x + 63) & ~63
                if x + vw <= AREA_X1:
                    sh[0] = x + vw
                    return x, y
        # nova prateleira
        for band in self.bands:
            y = self.next_y[band]
            if y + h <= band[1]:
                self.next_y[band] = y + h
                self.shelves.append([AREA_X0 + vw, y, h, band[1]])
                return AREA_X0, y
        raise RuntimeError("VRAM cheia: texturas demais ou grandes demais.")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("images", nargs="*")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)

    items = []
    for path in a.images:
        stem = os.path.basename(path).split(".")[0]
        img = Image.open(path).convert("RGBA")
        bpp = bpp_of(stem)
        w, h = img.size
        if w > 256 or h > 256:
            sys.exit("%s: maior que 256x256 (%dx%d)." % (path, w, h))
        mult = {16: 1, 8: 2, 4: 4}[bpp]
        if w % mult:
            sys.exit("%s: para %d bits a largura precisa ser múltipla de %d." % (path, bpp, mult))
        items.append((path, c_name(stem), img, bpp, mult))

    # maiores primeiro = empacotamento melhor; nome desempata (resultado estável)
    items.sort(key=lambda it: (-it[2].size[1], it[1]))

    names = [it[1] for it in items]
    dup = {n for n in names if names.count(n) > 1}
    if dup:
        sys.exit("Texturas com o mesmo nome: %s" % ", ".join(sorted(dup)))

    packer = Packer()
    clut_x, clut_y = AREA_X0, CLUT_Y0
    report = []

    for path, name, img, bpp, mult in items:
        w, h = img.size
        vw = w // mult
        x, y = packer.place(vw, h, mult)
        out = bytearray()

        if bpp == 16:
            out += struct.pack("<II", 0x10, 0x02)
            data = [to15(*p) for p in pixels(img)]
            out += struct.pack("<IHHHH", 12 + vw * h * 2, x, y, vw, h)
            out += struct.pack("<%dH" % len(data), *data)
            report.append("%-20s %3dx%-3d 16bit  vram(%d,%d)" % (name, w, h, x, y))
        else:
            colors = 256 if bpp == 8 else 16
            if clut_x + colors > AREA_X1:
                clut_x, clut_y = AREA_X0, clut_y + 1
            if clut_y >= 512:
                sys.exit("Sem espaço para paletas (texturas de 4/8 bits demais).")
            alpha = img.getchannel("A")
            q = img.convert("RGB").quantize(colors=colors - 1)
            pal = q.getpalette()[: (colors - 1) * 3]
            clut = [0x0000] + [to15(pal[i], pal[i + 1], pal[i + 2], 255, False)
                               for i in range(0, len(pal), 3)]
            clut += [0x8000] * (colors - len(clut))
            idx = [0 if al < 128 else i + 1 for i, al in zip(pixels(q), pixels(alpha))]

            out += struct.pack("<II", 0x10, 0x08 | (1 if bpp == 8 else 0))
            out += struct.pack("<IHHHH", 12 + colors * 2, clut_x, clut_y, colors, 1)
            out += struct.pack("<%dH" % colors, *clut)
            words = []
            for yy in range(h):
                for xw in range(vw):
                    v = 0
                    for k in range(mult):
                        v |= idx[yy * w + xw * mult + k] << (k * bpp)
                    words.append(v)
            out += struct.pack("<IHHHH", 12 + vw * h * 2, x, y, vw, h)
            out += struct.pack("<%dH" % len(words), *words)
            report.append("%-20s %3dx%-3d %2dbit  vram(%d,%d) clut(%d,%d)"
                          % (name, w, h, bpp, x, y, clut_x, clut_y))
            clut_x += colors

        with open(os.path.join(a.out, name + ".tim"), "wb") as f:
            f.write(out)

    with open(os.path.join(a.out, "vram.txt"), "w") as f:
        f.write("Texturas na VRAM (gerado automaticamente)\n\n" + "\n".join(report) + "\n")


if __name__ == "__main__":
    main()
