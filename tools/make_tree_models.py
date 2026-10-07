#!/usr/bin/env python3
"""
make_tree_models.py - Gera os modelos das árvores e as imagens planas (LOD).

Uso:
    python3 tools/make_tree_models.py

Saída:
    models/pinheiro.h       pinheiro (tronco + 3 cones de copa)      -> pinheiro_mesh
    models/arvore_seca.h    árvore seca retorcida (sem folhas)       -> arvore_seca_mesh
    models/tronco.h         tronco caído (deitado ao longo do eixo X) -> tronco_mesh
    assets/textures/pinheiro_bb-4bit.png   imagem plana do pinheiro (32x64)
    assets/textures/seca_bb-4bit.png       imagem plana da árvore seca (32x64)

Os modelos usam o mesmo construtor (Shape) e o mesmo conversor do
exportador do Blender que make_default_assets.py, mas em arquivo separado
para não regenerar os modelos antigos.

As imagens planas são RENDERIZADAS a partir dos próprios modelos: vista de
lado (ortográfica), sombreamento simples, fundo transparente. Assim a árvore
de longe tem a mesma silhueta e as mesmas cores da árvore de perto. O
quadro da imagem é BB_W x BB_H metros com a base do modelo na borda de
baixo; o jogo desenha a imagem com esse mesmo tamanho (tree_defs, data.c).

Coordenadas do Blender (em metros): Z para cima, origem no pé da árvore.
"""
import math
import os
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

from make_default_assets import Shape, cross, sub  # noqa: E402

BB_W, BB_H = 2.0, 4.0          # quadro da imagem plana, em metros
IMG_W, IMG_H = 32, 64          # pixels (4 bits: largura múltipla de 4)

BARK = (92, 64, 40)
NEEDLE = (38, 74, 40)
DEAD = (112, 98, 84)
DEAD_DARK = (84, 72, 62)
LOG = (96, 70, 46)
LOG_END = (156, 124, 84)


def frustum(s, b, rb, t, rt, sides, rgb, mat, phase=0.0, caps=False):
    """Tronco de cone de 'sides' lados entre o centro b (raio rb) e t (raio rt)."""
    ring_b = [(b[0] + math.cos(phase + 2 * math.pi * i / sides) * rb,
               b[1] + math.sin(phase + 2 * math.pi * i / sides) * rb, b[2]) for i in range(sides)]
    ring_t = [(t[0] + math.cos(phase + 2 * math.pi * i / sides) * rt,
               t[1] + math.sin(phase + 2 * math.pi * i / sides) * rt, t[2]) for i in range(sides)]
    center = ((b[0] + t[0]) / 2, (b[1] + t[1]) / 2, (b[2] + t[2]) / 2)
    for i in range(sides):
        j = (i + 1) % sides
        s.poly([ring_b[i], ring_b[j], ring_t[j], ring_t[i]], rgb, mat, center=center)


def cone(s, base, r, tip, sides, rgb, mat, phase=0.0):
    ring = [(base[0] + math.cos(phase + 2 * math.pi * i / sides) * r,
             base[1] + math.sin(phase + 2 * math.pi * i / sides) * r, base[2]) for i in range(sides)]
    center = ((base[0] + tip[0]) / 2, (base[1] + tip[1]) / 2, (base[2] + tip[2]) / 2)
    for i in range(sides):
        s.poly([ring[i], ring[(i + 1) % sides], tip], rgb, mat, center=center)


def shade(rgb, k):
    return tuple(max(0, min(255, int(c * k))) for c in rgb)


# ----------------------------------------------------------------------------
# Modelos
# ----------------------------------------------------------------------------

def make_pinheiro():
    s = Shape(["Tronco", "Copa"])
    frustum(s, (0, 0, 0), 0.14, (0, 0, 1.2), 0.10, 4, BARK, 0, phase=math.pi / 4)   # 4 faces
    cone(s, (0, 0, 0.85), 0.95, (0, 0, 2.15), 6, shade(NEEDLE, 0.90), 1)            # 6
    cone(s, (0, 0, 1.60), 0.75, (0, 0, 2.85), 6, NEEDLE, 1, phase=0.5)              # 6
    cone(s, (0, 0, 2.30), 0.50, (0, 0, 3.50), 6, shade(NEEDLE, 1.12), 1)            # 6
    return s


def make_arvore_seca():
    s = Shape(["Madeira"])
    a, b, c = (0, 0, 0), (0.16, 0.05, 1.40), (-0.08, 0.14, 2.55)
    frustum(s, a, 0.17, b, 0.11, 4, DEAD_DARK, 0, phase=0.3)                        # 4
    frustum(s, b, 0.11, c, 0.06, 4, DEAD, 0, phase=0.6)                             # 4
    cone(s, c, 0.06, (0.10, 0.22, 3.05), 4, DEAD, 0)                                # 4 (ponta)
    # galhos: pirâmides finas de 3 lados saindo do tronco
    branches = [
        ((0.10, 0.03, 1.05), (0.85, -0.10, 1.75)),
        ((0.14, 0.06, 1.55), (-0.70, 0.30, 2.20)),
        ((0.02, 0.10, 2.00), (0.55, 0.65, 2.70)),
        ((-0.05, 0.12, 2.30), (-0.55, -0.45, 2.95)),
    ]
    for base, tip in branches:
        cone(s, base, 0.09, tip, 3, DEAD, 0)                                        # 4 x 3
    return s


def make_tronco():
    s = Shape(["Casca", "Corte"])
    r, half, n = 0.22, 1.5, 8
    ring = [(math.cos(2 * math.pi * i / n + math.pi / 8) * r, math.sin(2 * math.pi * i / n + math.pi / 8) * r)
            for i in range(n)]
    for i in range(n):                                                              # 8 lados
        j = (i + 1) % n
        y0, z0 = ring[i]
        y1, z1 = ring[j]
        s.poly([(-half, y0, z0 + r), (half, y0, z0 + r), (half, y1, z1 + r), (-half, y1, z1 + r)],
               shade(LOG, 0.9 + 0.2 * (i % 2)), 0, center=(0, 0, r))
    for sx in (-half, half):                                                        # tampas: 3 quads cada
        pts = [(sx, y, z + r) for y, z in ring]
        out = (1 if sx > 0 else -1, 0, 0)
        for q in ([0, 1, 2, 3], [0, 3, 4, 7], [4, 5, 6, 7]):
            s.poly([pts[k] for k in q], LOG_END, 1, outward=out)
    cone(s, (0.5, 0.0, 2 * r - 0.02), 0.07, (0.75, -0.15, 0.80), 4, LOG, 0)         # tocos de galho
    cone(s, (-0.7, 0.1, 2 * r - 0.04), 0.06, (-0.95, 0.45, 0.62), 4, LOG, 0)
    return s


# ----------------------------------------------------------------------------
# Imagem plana: renderiza o modelo de lado (olhando para +Y do Blender)
# ----------------------------------------------------------------------------

def render_billboard(shape, path):
    W, H = IMG_W * 4, IMG_H * 4                  # 4x4 amostras por pixel (suaviza)
    color = [[None] * W for _ in range(H)]
    depth = [[1e9] * W for _ in range(H)]
    light = (-0.45, -0.55, 0.70)                 # luz vindo da frente-esquerda, de cima
    ln = math.sqrt(sum(c * c for c in light))
    light = tuple(c / ln for c in light)

    def to_px(p):
        return ((p[0] + BB_W / 2) / BB_W * W, (1 - p[2] / BB_H) * H)

    for pts, rgb, _mat, _uvs, _unlit in shape.polys:
        n = cross(sub(pts[1], pts[0]), sub(pts[2], pts[0]))
        nl = math.sqrt(sum(c * c for c in n)) or 1
        n = tuple(c / nl for c in n)
        lam = max(0.0, sum(a * b for a, b in zip(n, light)))
        k = 0.45 + 0.75 * lam
        col = shade(rgb, k)
        tris = [pts] if len(pts) == 3 else [[pts[0], pts[1], pts[2]], [pts[0], pts[2], pts[3]]]
        for tri in tris:
            p = [to_px(v) for v in tri]
            d = [v[1] for v in tri]              # profundidade: Y do Blender (frente = -Y)
            x0, x1 = max(0, int(min(q[0] for q in p))), min(W - 1, int(max(q[0] for q in p)) + 1)
            y0, y1 = max(0, int(min(q[1] for q in p))), min(H - 1, int(max(q[1] for q in p)) + 1)
            (ax, ay), (bx, by), (cx, cy) = p
            den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
            if abs(den) < 1e-9:
                continue
            for y in range(y0, y1 + 1):
                for x in range(x0, x1 + 1):
                    px, py = x + 0.5, y + 0.5
                    w0 = ((by - cy) * (px - cx) + (cx - bx) * (py - cy)) / den
                    w1 = ((cy - ay) * (px - cx) + (ax - cx) * (py - cy)) / den
                    w2 = 1 - w0 - w1
                    if w0 < 0 or w1 < 0 or w2 < 0:
                        continue
                    z = w0 * d[0] + w1 * d[1] + w2 * d[2]
                    if z < depth[y][x]:
                        depth[y][x] = z
                        color[y][x] = col

    # reduz 4x4 -> 1 pixel: cobertura suficiente = opaco (média das cores)
    raw = bytearray()
    for y in range(IMG_H):
        raw.append(0)
        for x in range(IMG_W):
            cols = [color[y * 4 + j][x * 4 + i] for j in range(4) for i in range(4)]
            cols = [c for c in cols if c]
            if len(cols) >= 5:                   # 5 de 16 amostras: galhos finos não somem
                r = sum(c[0] for c in cols) // len(cols)
                g = sum(c[1] for c in cols) // len(cols)
                b = sum(c[2] for c in cols) // len(cols)
                raw += bytes((max(r, 8), max(g, 8), max(b, 8), 255))
            else:
                raw += bytes((0, 0, 0, 0))

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", IMG_W, IMG_H, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)
    print("  %s: %dx%d (RGBA, vira 4 bits no build)" % (os.path.relpath(path, ROOT), IMG_W, IMG_H))


def main():
    print("Modelos das árvores:")
    pin = make_pinheiro().save("pinheiro")
    seca = make_arvore_seca().save("arvore_seca")
    make_tronco().save("tronco")
    print("Imagens planas (LOD):")
    tex = os.path.join(ROOT, "assets", "textures")
    render_billboard(pin, os.path.join(tex, "pinheiro_bb-4bit.png"))
    render_billboard(seca, os.path.join(tex, "seca_bb-4bit.png"))


if __name__ == "__main__":
    main()
