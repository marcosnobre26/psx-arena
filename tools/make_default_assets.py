# -*- coding: utf-8 -*-
"""
make_default_assets.py - Gera os modelos e texturas padrão do projeto

Os modelos são montados aqui com "caixas" e formas simples, em coordenadas
do Blender (Z para cima, frente = -Y), e passam pelo MESMO conversor do
exportador do Blender. Ou seja: o resultado é idêntico ao que você teria
modelando no Blender e exportando.

Uso:  python3 tools/make_default_assets.py
Saída: models/*.h  e  assets/textures/*.png

O mesmo código também monta o assets/blender/exemplos.blend
(veja tools/make_example_blend.py), para você editar os modelos no Blender.

Você não precisa rodar isso de novo, a não ser que queira mudar os modelos
padrão por código. O caminho normal é modelar no Blender e exportar.
"""
import math
import os
import random
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

from blender_export_psx import MeshBuilder, blender_to_psx, normal_to_psx  # noqa: E402

SCALE = 256  # 1 metro = 256 unidades


# ----------------------------------------------------------------------------
# Construtor de formas (coordenadas do Blender, em metros)
# ----------------------------------------------------------------------------

def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


class Shape:
    def __init__(self, materials):
        self.mb = MeshBuilder()
        self.materials = materials   # nomes dos materiais (índice = slot)
        self.polys = []              # cópia em coordenadas do Blender

    def poly(self, pts, rgb, mat=0, outward=None, center=None, uvs=None, unlit=False):
        """pts: 3 ou 4 pontos. Garante ordem anti-horária vista de fora
        (como no Blender) usando 'outward' ou o 'center' da peça."""
        wn = cross(sub(pts[1], pts[0]), sub(pts[2], pts[0]))
        if outward is None:
            c = tuple(sum(p[i] for p in pts) / len(pts) for i in range(3))
            outward = sub(c, center)
        if dot(wn, outward) < 0:
            pts = [pts[0]] + pts[1:][::-1]
            if uvs:
                uvs = [uvs[0]] + uvs[1:][::-1]
            wn = (-wn[0], -wn[1], -wn[2])
        self.polys.append((pts, rgb, mat, uvs, unlit))
        idx = [self.mb.add_vertex(blender_to_psx(p, SCALE)) for p in pts]
        self.mb.add_face(idx, normal_to_psx(wn), rgb, mat=mat, uvs=uvs, unlit=unlit)

    def box(self, x0, y0, z0, x1, y1, z1, rgb, mat=0, tex=None, unlit=False, skip_bottom=True):
        c = ((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2)
        faces = [
            [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)],  # frente (-Y)
            [(x1, y1, z0), (x0, y1, z0), (x0, y1, z1), (x1, y1, z1)],  # trás
            [(x0, y1, z0), (x0, y0, z0), (x0, y0, z1), (x0, y1, z1)],  # esquerda
            [(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)],  # direita
            [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)],  # topo
        ]
        if not skip_bottom:
            faces.append([(x0, y1, z0), (x1, y1, z0), (x1, y0, z0), (x0, y0, z0)])
        for f in faces:
            uvs = None
            if tex:
                w, h = tex
                uvs = [(0, h - 1), (w - 1, h - 1), (w - 1, 0), (0, 0)]
            self.poly(f, rgb, mat, center=c, uvs=uvs, unlit=unlit)

    def bipyramid(self, cx, cy, z_bot, z_mid, z_top, radius, sides, rgb, mat=0, unlit=False, phase=0.0):
        ring = []
        for i in range(sides):
            a = phase + 2 * math.pi * i / sides
            ring.append((cx + math.cos(a) * radius, cy + math.sin(a) * radius, z_mid))
        top, bot = (cx, cy, z_top), (cx, cy, z_bot)
        center = (cx, cy, z_mid)
        for i in range(sides):
            p, q = ring[i], ring[(i + 1) % sides]
            self.poly([p, q, top], rgb, mat, center=center, unlit=unlit)
            self.poly([q, p, bot], rgb, mat, center=center, unlit=unlit)

    def pyramid(self, base_center, base_r, tip, sides, rgb, mat=0):
        cx, cy, cz = base_center
        ring = [(cx + math.cos(2 * math.pi * i / sides) * base_r,
                 cy + math.sin(2 * math.pi * i / sides) * base_r, cz) for i in range(sides)]
        center = ((cx + tip[0]) / 2, (cy + tip[1]) / 2, (cz + tip[2]) / 2)
        for i in range(sides):
            self.poly([ring[i], ring[(i + 1) % sides], tip], rgb, mat, center=center)

    def save(self, name):
        path = os.path.join(ROOT, "models", name + ".h")
        self.mb.write_header(path, name, source="tools/make_default_assets.py")
        print("  models/%s.h: %d vértices, %d faces" % (name, len(self.mb.verts), len(self.mb.faces)))
        return self


# ----------------------------------------------------------------------------
# Modelos
# ----------------------------------------------------------------------------
# Materiais do robô (o jogo troca essas cores pelas "skins"):
#   0 = corpo, 1 = cabeça, 2 = braços/pernas, 3 = visor, 4 = arma

def make_player():
    s = Shape(["Corpo", "Cabeca", "Membros", "Visor_UNLIT", "Arma"])
    body, head, limb, visor, gun = (60, 120, 220), (230, 230, 240), (70, 70, 80), (80, 255, 240), (200, 60, 40)
    # pernas
    s.box(-0.20, -0.09, 0.00, -0.06, 0.09, 0.42, limb, 2)
    s.box(0.06, -0.09, 0.00, 0.20, 0.09, 0.42, limb, 2)
    # pés (um pouco para frente)
    s.box(-0.21, -0.17, 0.00, -0.05, 0.10, 0.08, limb, 2)
    s.box(0.05, -0.17, 0.00, 0.21, 0.10, 0.08, limb, 2)
    # tronco
    s.box(-0.26, -0.15, 0.42, 0.26, 0.15, 0.92, body, 0)
    # mochila
    s.box(-0.18, 0.15, 0.50, 0.18, 0.26, 0.86, limb, 2)
    # braços
    s.box(-0.38, -0.08, 0.50, -0.26, 0.08, 0.90, limb, 2)
    s.box(0.26, -0.08, 0.50, 0.38, 0.08, 0.90, limb, 2)
    # arma no braço direito (do personagem = -X no Blender visto de frente)
    s.box(-0.40, -0.42, 0.56, -0.28, -0.04, 0.68, gun, 4)
    # cabeça
    s.box(-0.18, -0.16, 0.95, 0.18, 0.16, 1.25, head, 1)
    # visor (sem luz: parece que brilha)
    s.box(-0.14, -0.19, 1.05, 0.14, -0.16, 1.15, visor, 3, unlit=True)
    # antena
    s.box(0.08, -0.02, 1.25, 0.11, 0.02, 1.42, gun, 4)
    return s.save("player")


def make_knight():
    """Cavaleiro: pesado, armadura, escudo e espada. Usa as próprias cores."""
    s = Shape(["Armadura", "Detalhe", "Capa", "Espada", "Visor_UNLIT"])
    armor, trim, cape, sword, slit = (170, 175, 185), (200, 160, 50), (170, 30, 40), (220, 225, 235), (255, 200, 60)
    s.box(-0.22, -0.10, 0.00, -0.06, 0.10, 0.45, armor, 0)        # pernas
    s.box(0.06, -0.10, 0.00, 0.22, 0.10, 0.45, armor, 0)
    s.box(-0.24, -0.20, 0.00, -0.04, 0.11, 0.09, trim, 1)         # botas
    s.box(0.04, -0.20, 0.00, 0.24, 0.11, 0.09, trim, 1)
    s.box(-0.30, -0.17, 0.45, 0.30, 0.17, 0.98, armor, 0)         # tronco
    s.box(-0.31, -0.18, 0.50, 0.31, 0.18, 0.56, trim, 1)          # cinto
    s.box(-0.28, 0.17, 0.40, 0.28, 0.22, 0.96, cape, 2)           # capa
    s.box(-0.44, -0.09, 0.62, -0.30, 0.09, 0.98, armor, 0)        # braços
    s.box(0.30, -0.09, 0.62, 0.44, 0.09, 0.98, armor, 0)
    s.box(-0.50, -0.30, 0.66, -0.36, -0.06, 0.74, trim, 1)        # guarda da espada
    s.box(-0.46, -0.85, 0.67, -0.40, -0.30, 0.73, sword, 3)       # lâmina
    s.box(0.44, -0.30, 0.45, 0.50, 0.12, 1.00, trim, 1)           # escudo
    s.box(0.50, -0.24, 0.52, 0.53, 0.06, 0.93, cape, 2)           # brasão
    s.box(-0.20, -0.19, 1.00, 0.20, 0.19, 1.36, armor, 0)         # elmo
    s.box(-0.15, -0.22, 1.16, 0.15, -0.19, 1.21, slit, 4, unlit=True)  # fresta do visor
    s.box(-0.03, -0.12, 1.36, 0.03, 0.16, 1.56, cape, 2)          # penacho
    return s.save("knight")


def make_scout():
    """Batedor: leve e rápido, capuz e óculos. Usa as próprias cores."""
    s = Shape(["Roupa", "Pele", "Capuz", "Oculos_UNLIT", "Adaga"])
    cloth, skin, hood, goggles, blade = (90, 110, 70), (230, 180, 140), (60, 140, 90), (255, 230, 60), (200, 200, 210)
    s.box(-0.15, -0.07, 0.00, -0.04, 0.07, 0.42, cloth, 0)        # pernas
    s.box(0.04, -0.07, 0.00, 0.15, 0.07, 0.42, cloth, 0)
    s.box(-0.16, -0.13, 0.00, -0.03, 0.08, 0.06, (70, 50, 40), 0) # botas
    s.box(0.03, -0.13, 0.00, 0.16, 0.08, 0.06, (70, 50, 40), 0)
    s.box(-0.19, -0.11, 0.42, 0.19, 0.11, 0.82, hood, 2)          # casaco
    s.box(-0.28, -0.06, 0.48, -0.19, 0.06, 0.80, cloth, 0)        # braços
    s.box(0.19, -0.06, 0.48, 0.28, 0.06, 0.80, cloth, 0)
    s.box(-0.27, -0.40, 0.52, -0.23, -0.05, 0.56, blade, 4)       # adaga
    s.box(-0.13, -0.13, 0.84, 0.13, 0.12, 1.08, skin, 1)          # cabeça
    s.box(-0.15, -0.11, 0.98, 0.15, 0.14, 1.12, hood, 2)          # capuz
    s.pyramid((0, 0.02, 1.12), 0.17, (0, 0.10, 1.32), 4, hood, 2) # ponta do capuz
    s.box(-0.12, -0.16, 0.95, 0.12, -0.13, 1.01, goggles, 3, unlit=True)  # óculos
    s.box(-0.20, -0.13, 0.80, 0.20, 0.13, 0.86, (200, 60, 50), 0) # cachecol
    return s.save("scout")


def make_grunt():
    s = Shape(["Corpo", "Chifres", "Olhos_UNLIT"])
    red, dark, eye = (210, 50, 60), (90, 20, 40), (255, 230, 60)
    s.bipyramid(0, 0, 0.10, 0.55, 1.05, 0.42, 8, red, 0, phase=math.pi / 8)
    # chifres
    s.pyramid((-0.18, 0, 0.82), 0.08, (-0.34, 0.05, 1.25), 4, dark, 1)
    s.pyramid((0.18, 0, 0.82), 0.08, (0.34, 0.05, 1.25), 4, dark, 1)
    # olhos
    s.box(-0.20, -0.46, 0.62, -0.06, -0.36, 0.72, eye, 2, unlit=True)
    s.box(0.06, -0.46, 0.62, 0.20, -0.36, 0.72, eye, 2, unlit=True)
    return s.save("grunt")


def make_crate():
    s = Shape(["Madeira"])
    s.box(-0.5, -0.5, 0.0, 0.5, 0.5, 1.0, (128, 128, 128), 0, tex=(64, 64))
    return s.save("crate")


def make_gem():
    s = Shape(["Gema"])
    s.bipyramid(0, 0, 0.0, 0.22, 0.44, 0.17, 4, (255, 220, 40), 0)
    return s.save("gem")


def make_bullet():
    s = Shape(["Tiro_UNLIT"])
    s.bipyramid(0, 0, -0.08, 0.0, 0.08, 0.08, 4, (255, 255, 160), 0, unlit=True)
    return s.save("bullet")


def make_shadow():
    s = Shape(["Sombra_UNLIT"])
    n, r = 8, 0.5
    pts = [(math.cos(2 * math.pi * i / n) * r, math.sin(2 * math.pi * i / n) * r, 0.0) for i in range(n)]
    for i in range(1, n - 1):
        s.poly([pts[0], pts[i], pts[i + 1]], (0, 0, 0), 0, outward=(0, 0, 1), unlit=True)
    return s.save("shadow")


def make_pillar():
    s = Shape(["Pedra", "Base", "Chama_UNLIT"])
    stone, base, fire = (150, 145, 135), (95, 90, 85), (255, 170, 40)
    s.box(-0.36, -0.36, 0.00, 0.36, 0.36, 0.18, base, 1)       # base
    s.box(-0.26, -0.26, 0.18, 0.26, 0.26, 1.30, stone, 0)      # coluna
    s.box(-0.36, -0.36, 1.30, 0.36, 0.36, 1.42, base, 1)       # capitel
    s.bipyramid(0, 0, 1.42, 1.50, 1.85, 0.16, 4, fire, 2, unlit=True)  # chama
    return s.save("pillar")


def make_ring():
    s = Shape(["Anel_UNLIT"])
    n, r0, r1 = 16, 0.85, 1.0
    for i in range(n):
        a, b = 2 * math.pi * i / n, 2 * math.pi * (i + 1) / n
        p = [(math.cos(a) * r0, math.sin(a) * r0, 0), (math.cos(a) * r1, math.sin(a) * r1, 0),
             (math.cos(b) * r1, math.sin(b) * r1, 0), (math.cos(b) * r0, math.sin(b) * r0, 0)]
        s.poly(p, (120, 200, 255), 0, outward=(0, 0, 1), unlit=True)
    return s.save("ring")


# ----------------------------------------------------------------------------
# Texturas (TIM 16 bits)
# ----------------------------------------------------------------------------

def rgb15(r, g, b):
    r, g, b = [max(0, min(255, int(c))) >> 3 for c in (r, g, b)]
    v = r | (g << 5) | (b << 10)
    return v if v != 0 else 0x8000   # preto puro seria transparente no PS1


def write_png(path, w, h, pixels):
    """PNG simples sem precisar do Pillow. pixels = valores 15 bits do PS1."""
    import zlib
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            v = pixels[y * w + x]
            raw += bytes(((v & 31) << 3, ((v >> 5) & 31) << 3, ((v >> 10) & 31) << 3))

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)
    print("  %s: %dx%d" % (os.path.relpath(path, ROOT), w, h))


def tex_floor(rnd):
    px = []
    for y in range(64):
        for x in range(64):
            gx, gy = x % 32, y % 32
            grout = gx in (0, 31) or gy in (0, 31)
            n = rnd.randint(-10, 10)
            if grout:
                c = (52 + n // 2, 50 + n // 2, 58 + n // 2)
            else:
                tone = 118 if ((x // 32) + (y // 32)) % 2 == 0 else 104
                edge = 14 if (gx in (1, 2) or gy in (1, 2)) else (-12 if (gx in (29, 30) or gy in (29, 30)) else 0)
                c = (tone + n + edge, tone + 6 + n + edge, tone - 6 + n + edge)
            px.append(rgb15(*c))
    return px


def tex_wall(rnd):
    px = []
    for y in range(64):
        for x in range(64):
            row = y // 16
            off = 16 if row % 2 else 0
            bx = (x + off) % 32
            by = y % 16
            mortar = by in (0, 15) or bx in (0, 31)
            n = rnd.randint(-12, 12)
            if mortar:
                c = (70 + n // 3, 66 + n // 3, 62 + n // 3)
            else:
                brick = ((x + off) // 32 + row) % 3
                base = [(150, 78, 60), (138, 70, 56), (160, 88, 66)][brick]
                c = (base[0] + n, base[1] + n, base[2] + n)
            px.append(rgb15(*c))
    return px


def tex_crate(rnd):
    px = []
    for y in range(64):
        for x in range(64):
            n = rnd.randint(-8, 8)
            plank = (y // 8) % 2
            wood = (150 + n, 100 + n, 50 + n) if plank else (138 + n, 90 + n, 44 + n)
            if y % 8 == 0:
                wood = (90, 58, 28)
            border = x < 6 or x > 57 or y < 6 or y > 57
            diag = abs(x - y) < 4 or abs(x - (63 - y)) < 4
            if border or diag:
                wood = (176 + n, 128 + n, 70 + n)
                if x in (6, 57) or y in (6, 57):
                    wood = (80, 50, 24)
            px.append(rgb15(*wood))
    return px


MODELS = [make_player, make_knight, make_scout, make_grunt, make_crate, make_gem, make_bullet,
          make_shadow, make_ring, make_pillar]
MODEL_NAMES = ["player", "knight", "scout", "grunt", "crate", "gem", "bullet", "shadow", "ring", "pillar"]


def make_textures():
    rnd = random.Random(1234)
    out = os.path.join(ROOT, "assets", "textures")
    os.makedirs(out, exist_ok=True)
    write_png(os.path.join(out, "floor.png"), 64, 64, tex_floor(rnd))
    write_png(os.path.join(out, "wall.png"), 64, 64, tex_wall(rnd))
    write_png(os.path.join(out, "crate.png"), 64, 64, tex_crate(rnd))


def main():
    print("Gerando modelos...")
    for make in MODELS:
        make()
    print("Gerando texturas...")
    make_textures()
    print("Pronto.")


if __name__ == "__main__":
    main()
