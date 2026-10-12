#!/usr/bin/env python3
"""
make_menu_icons.py - Ícones do menu e a textura do mapa (etapa 08).

Uso:
    python3 tools/make_menu_icons.py

Saída (em assets/textures/, entram sozinhas no build):
    icones_menu-4bit.png  192x16: 12 ícones de 16x16 numa folha, na ordem
                          [itens] bandagem, kit médico, poção, pilha,
                          [armas] espada, machado, arco, bola de fogo, clarão,
                                  cura, pistola, espingarda
                          (o menu.c usa a posição: item k -> x = 16*(k-1);
                           arma w -> x = 64 + 16*w)
    mapa_menu-4bit.png    128x128 vazia: só RESERVA o lugar na VRAM. O jogo
                          troca os pixels e a paleta em tempo real (aba Mapa).

4 bits = 16 cores por textura (uma paleta para a folha toda). Transparente
= alfa 0 (vira a cor 0 da paleta). Feitos no projeto: nada no CREDITS.md.
"""
import os

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(os.path.dirname(HERE), "assets", "textures")

WHITE = (235, 235, 225, 255)
RED = (220, 50, 50, 255)
DKRED = (130, 25, 30, 255)
CREAM = (230, 215, 180, 255)
BLUE = (70, 140, 255, 255)
DKBLUE = (30, 60, 140, 255)
GREEN = (120, 230, 100, 255)
GRAY = (150, 155, 165, 255)
DARK = (55, 50, 50, 255)
WOOD = (150, 105, 60, 255)
ORANGE = (255, 150, 40, 255)
YELLOW = (255, 230, 120, 255)
STEEL = (200, 210, 225, 255)


def icon(draw_fn):
    im = Image.new("RGBA", (16, 16), (0, 0, 0, 0))
    draw_fn(ImageDraw.Draw(im))
    return im


def bandagem(d):
    d.rectangle([2, 5, 13, 10], fill=CREAM, outline=DARK)
    d.line([6, 5, 6, 10], fill=DARK)
    d.line([9, 5, 9, 10], fill=DARK)


def kit(d):
    d.rectangle([2, 4, 13, 13], fill=WHITE, outline=DARK)
    d.rectangle([6, 2, 9, 4], outline=DARK)
    d.rectangle([7, 6, 8, 11], fill=RED)
    d.rectangle([5, 8, 10, 9], fill=RED)


def pocao(d):
    d.rectangle([6, 1, 9, 4], fill=CREAM)
    d.ellipse([3, 4, 12, 14], fill=BLUE, outline=DKBLUE)
    d.point([6, 7], fill=WHITE)


def pilha(d):
    d.rectangle([4, 3, 11, 14], fill=GREEN, outline=DARK)
    d.rectangle([6, 1, 9, 3], fill=GRAY)
    d.line([7, 6, 7, 11], fill=DARK)
    d.line([5, 8, 9, 8], fill=DARK)


def espada(d):
    d.line([3, 12, 12, 3], fill=STEEL, width=2)
    d.line([3, 9, 6, 12], fill=WOOD, width=2)


def machado(d):
    d.line([4, 14, 10, 2], fill=WOOD, width=2)
    d.polygon([(8, 2), (14, 3), (13, 8), (9, 6)], fill=STEEL)


def arco(d):
    d.arc([2, 1, 12, 15], 270, 90, fill=WOOD, width=2)
    d.line([7, 1, 7, 15], fill=CREAM)


def fogo(d):
    d.ellipse([3, 5, 12, 14], fill=ORANGE)
    d.ellipse([5, 8, 10, 13], fill=YELLOW)
    d.polygon([(5, 6), (8, 1), (11, 6)], fill=ORANGE)


def clarao(d):
    for a, b in (((8, 1), (8, 15)), ((1, 8), (15, 8)), ((3, 3), (13, 13)), ((13, 3), (3, 13))):
        d.line([a, b], fill=YELLOW)
    d.ellipse([5, 5, 11, 11], fill=WHITE)


def cura(d):
    d.rectangle([6, 2, 9, 13], fill=GREEN)
    d.rectangle([2, 6, 13, 9], fill=GREEN)


def pistola(d):
    d.rectangle([2, 4, 13, 7], fill=GRAY, outline=DARK)
    d.rectangle([3, 7, 6, 13], fill=DARK)


def espingarda(d):
    d.rectangle([1, 5, 14, 7], fill=GRAY)
    d.rectangle([9, 7, 14, 11], fill=WOOD)


ICONS = [bandagem, kit, pocao, pilha,
         espada, machado, arco, fogo, clarao, cura, pistola, espingarda]


def main():
    os.makedirs(OUT, exist_ok=True)
    sheet = Image.new("RGBA", (16 * len(ICONS), 16), (0, 0, 0, 0))
    for k, fn in enumerate(ICONS):
        sheet.paste(icon(fn), (16 * k, 0))
    sheet.save(os.path.join(OUT, "icones_menu-4bit.png"))
    mapa = Image.new("RGBA", (128, 128), (8, 8, 10, 255))   # só reserva a VRAM
    mapa.save(os.path.join(OUT, "mapa_menu-4bit.png"))
    print("Ícones do menu e reserva do mapa em assets/textures/")


if __name__ == "__main__":
    main()
