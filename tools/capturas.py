#!/usr/bin/env python3
"""
capturas.py - Monta PNGs a partir das linhas SHOT do TTY (./dev capturas, etapa 08).

Uso: python3 tools/capturas.py <log do DuckStation> <pasta de saída> <prefixo>

O jogo (build do robô com ROBOT_SHOTS) manda cada tela assim:
    SHOT INICIO <nome> <largura> <altura>
    SHOT <y> <x> <32 pixels em hex, 4 dígitos cada: cor de 15 bits do PS1>
    SHOT FIM <nome>
Cor do PS1: bits 0-4 vermelho, 5-9 verde, 10-14 azul (5 bits cada).
Grava <saída>/<prefixo><nome>.png.
"""
import os
import re
import sys

from PIL import Image


def main():
    log, out, prefix = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(out, exist_ok=True)
    img, name, saved = None, None, []
    with open(log, encoding="utf-8", errors="replace") as f:
        for raw in f:
            line = re.sub(r"\x1b\[[0-9;]*m", "", raw)
            i = line.find("SHOT ")
            if i < 0:
                continue
            parts = line[i:].split()
            if parts[1] == "INICIO":
                name, w, h = parts[2], int(parts[3]), int(parts[4])
                img = Image.new("RGB", (w, h))
            elif parts[1] == "FIM" and img is not None:
                path = os.path.join(out, prefix + name + ".png")
                img.save(path)
                saved.append(path)
                img = None
            elif img is not None and len(parts) >= 4 and parts[1].isdigit():
                y, x, hexs = int(parts[1]), int(parts[2]), parts[3]
                for k in range(len(hexs) // 4):
                    c = int(hexs[k * 4:k * 4 + 4], 16)
                    r, g, b = (c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3
                    if x + k < img.width and y < img.height:
                        img.putpixel((x + k, y), (r, g, b))
    for p in saved:
        print(p)
    if not saved:
        sys.exit("nenhuma captura encontrada no log")


if __name__ == "__main__":
    main()
