#!/usr/bin/env python3
"""
make_forest_textures.py - Gera as texturas do chão e da mata da floresta.

Uso:
    python3 tools/make_forest_textures.py      # grava em assets/textures/

Saída (64x64, sufixo -8bit = 256 cores, metade da VRAM de uma 16 bits):
    terra-8bit.png    trilha de terra batida      -> tex_terra_t
    folhas-8bit.png   chão coberto de folhas      -> tex_folhas_t
    raizes-8bit.png   terra com raízes            -> tex_raizes_t
    mata-8bit.png     paredão de mata densa       -> tex_mata_t

São provisórias (desenhadas por código). Para trocar, substitua o PNG de
mesmo nome. Separado do make_default_assets.py para não regenerar os
modelos e texturas antigos. Semente fixa: sempre o mesmo resultado.

As texturas do chão "dão a volta" (as bordas emendam), porque o chão
repete a mesma textura célula após célula.
"""
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

from make_default_assets import rgb15, write_png  # noqa: E402

N = 64


def value_noise(rnd, cells):
    """Ruído suave que emenda nas bordas: grade cells x cells, interpolada."""
    grid = [[rnd.random() for _ in range(cells)] for _ in range(cells)]

    def f(x, y):
        gx, gy = x * cells / N, y * cells / N
        x0, y0 = int(gx) % cells, int(gy) % cells
        x1, y1 = (x0 + 1) % cells, (y0 + 1) % cells
        tx, ty = gx - int(gx), gy - int(gy)
        tx, ty = tx * tx * (3 - 2 * tx), ty * ty * (3 - 2 * ty)
        a = grid[y0][x0] + (grid[y0][x1] - grid[y0][x0]) * tx
        b = grid[y1][x0] + (grid[y1][x1] - grid[y1][x0]) * tx
        return a + (b - a) * ty
    return f


def tex_terra(rnd):
    n1, n2 = value_noise(rnd, 4), value_noise(rnd, 16)
    px = []
    for y in range(N):
        for x in range(N):
            v = n1(x, y) * 0.6 + n2(x, y) * 0.4
            grit = rnd.randint(-10, 10)
            pebble = 18 if rnd.random() < 0.02 else 0
            c = (92 + v * 50 + grit + pebble, 70 + v * 38 + grit + pebble, 48 + v * 24 + grit + pebble)
            px.append(rgb15(*c))
    return px


def tex_folhas(rnd):
    base = value_noise(rnd, 8)
    img = [[None] * N for _ in range(N)]
    for y in range(N):
        for x in range(N):
            v = base(x, y)
            img[y][x] = [44 + v * 30, 50 + v * 26, 26 + v * 14]
    # folhas: pequenas elipses de tons de outono/verde escuro (emendando)
    tones = [(110, 72, 30), (92, 58, 26), (70, 84, 36), (124, 92, 40), (58, 64, 28)]
    for _ in range(170):
        cx, cy = rnd.randrange(N), rnd.randrange(N)
        a = rnd.random() * math.pi
        rl, rw = rnd.uniform(2.0, 4.0), rnd.uniform(0.9, 1.6)
        col = tones[rnd.randrange(len(tones))]
        shade = rnd.uniform(0.8, 1.15)
        for dy in range(-4, 5):
            for dx in range(-4, 5):
                u = dx * math.cos(a) + dy * math.sin(a)
                w = -dx * math.sin(a) + dy * math.cos(a)
                if (u / rl) ** 2 + (w / rw) ** 2 <= 1.0:
                    img[(cy + dy) % N][(cx + dx) % N] = [col[0] * shade, col[1] * shade, col[2] * shade]
    px = []
    for y in range(N):
        for x in range(N):
            n = rnd.randint(-6, 6)
            r, g, b = img[y][x]
            px.append(rgb15(r + n, g + n, b + n))
    return px


def tex_raizes(rnd):
    base = tex_terra(random.Random(rnd.random()))
    img = [[[((v & 31) << 3), (((v >> 5) & 31) << 3), (((v >> 10) & 31) << 3)] for v in base[y * N:(y + 1) * N]]
           for y in range(N)]
    # raízes: curvas grossas e escuras atravessando a célula (emendam nas bordas)
    for _ in range(6):
        x, y = rnd.uniform(0, N), rnd.uniform(0, N)
        ang = rnd.uniform(0, 2 * math.pi)
        width = rnd.uniform(1.5, 3.2)
        for _step in range(90):
            ang += rnd.uniform(-0.25, 0.25)
            x += math.cos(ang)
            y += math.sin(ang)
            for dy in range(-3, 4):
                for dx in range(-3, 4):
                    d = math.hypot(dx, dy)
                    if d <= width:
                        k = 0.45 + 0.25 * (d / width)
                        px_ = img[int(y + dy) % N][int(x + dx) % N]
                        img[int(y + dy) % N][int(x + dx) % N] = [62 * k + 20, 44 * k + 14, 28 * k + 8] if d > width - 1 else [px_[0] * 0.5 + 30, px_[1] * 0.45 + 20, px_[2] * 0.4 + 12]
            width = max(1.0, width - 0.02)
    return [rgb15(*img[y][x]) for y in range(N) for x in range(N)]


def tex_mata(rnd):
    n1, n2 = value_noise(rnd, 4), value_noise(rnd, 16)
    px = []
    for y in range(N):
        for x in range(N):
            v = n1(x, y) * 0.5 + n2(x, y) * 0.5
            leaf = rnd.random()
            if leaf < 0.18:
                c = (34 + v * 30, 70 + v * 50, 30 + v * 20)       # folha clara
            elif leaf < 0.30:
                c = (14, 22, 12)                                  # buraco escuro
            else:
                c = (22 + v * 26, 44 + v * 40, 20 + v * 18)
            # galhos verticais escuros, como troncos finos atrás da folhagem
            if (x + int(v * 6)) % 21 in (0, 1):
                c = (40 + v * 10, 30 + v * 8, 22 + v * 6)
            px.append(rgb15(*c))
    return px


def main():
    out = os.path.join(ROOT, "assets", "textures")
    os.makedirs(out, exist_ok=True)
    rnd = random.Random(4242)
    print("Texturas da floresta:")
    write_png(os.path.join(out, "terra-8bit.png"), N, N, tex_terra(rnd))
    write_png(os.path.join(out, "folhas-8bit.png"), N, N, tex_folhas(rnd))
    write_png(os.path.join(out, "raizes-8bit.png"), N, N, tex_raizes(rnd))
    write_png(os.path.join(out, "mata-8bit.png"), N, N, tex_mata(rnd))


if __name__ == "__main__":
    main()
