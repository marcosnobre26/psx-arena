#!/usr/bin/env python3
"""
make_combat_models.py - Modelos PROVISÓRIOS do combate (etapa 07).

Uso:
    python3 tools/make_combat_models.py

Saída (nomes FIXOS: para trocar por um modelo do Blender, exporte um objeto
com o mesmo nome na coleção PSX; o código não muda):
    models/espada.h      espada        -> espada_mesh      (arma na mão)
    models/machado.h     machado       -> machado_mesh
    models/arco.h        arco          -> arco_mesh
    models/pistola.h     pistola       -> pistola_mesh
    models/espingarda.h  espingarda    -> espingarda_mesh
    models/flecha.h      flecha        -> flecha_mesh      (no ar e no chão)
    models/rastejador.h  inimigo lento e resistente  -> rastejador_mesh
    models/cacador.h     inimigo rápido, foge da luz -> cacador_mesh
    models/grande.h      inimigo grande e raro       -> grande_mesh

Convenções (as mesmas do exportador e de make_default_assets.py):
coordenadas do Blender em metros, Z para cima. ARMAS: origem na mão,
apontando para -Y do Blender (a frente do personagem no jogo). INIMIGOS:
origem no pé, de frente para -Y; materiais 0 = corpo, 1 = sombra/detalhe,
2 = olhos (o jogo troca as cores por tipo, paleta de enemy_defs).
Feitos no projeto: nada no CREDITS.md.
"""
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from make_default_assets import Shape  # noqa: E402
from make_tree_models import cone, frustum  # noqa: E402

STEEL = (190, 200, 215)
DARK = (60, 50, 45)
WOOD = (130, 92, 56)
LEATHER = (90, 60, 40)
IRON = (110, 115, 125)


# ---------------------------------------------------------------------------
# Armas (origem na mão, para -Y)
# ---------------------------------------------------------------------------

def make_espada():
    # enxuto (é desenhado todo quadro): sem o cabo (a mão cobre), pontas de 3 lados
    s = Shape(["Lamina", "Cabo"])
    s.box(-0.025, -0.80, -0.008, 0.025, -0.10, 0.008, STEEL, 0)                      # lâmina (5)
    cone(s, (0, -0.80, 0), 0.03, (0, -0.90, 0), 3, STEEL, 0)                         # ponta (3)
    s.box(-0.10, -0.12, -0.02, 0.10, -0.08, 0.02, IRON, 1)                           # guarda (5)
    return s


def make_machado():
    s = Shape(["Lamina", "Cabo"])
    s.box(-0.022, -0.70, -0.022, 0.022, 0.0, 0.022, WOOD, 1)                         # cabo (5)
    s.box(0.02, -0.72, -0.012, 0.20, -0.52, 0.012, STEEL, 0)                         # lâmina (5)
    return s


def make_arco():
    s = Shape(["Madeira", "Corda"])
    pts = [(0.0, -0.10, -0.55), (0.0, -0.20, -0.25), (0.0, -0.20, 0.25), (0.0, -0.10, 0.55)]
    for a, b in zip(pts, pts[1:]):
        frustum(s, a, 0.02, b, 0.02, 3, WOOD, 0)                                     # 3 x 3
    s.box(-0.004, -0.11, -0.55, 0.004, -0.09, 0.55, (220, 220, 200), 1)              # corda (5)
    return s


def make_pistola():
    s = Shape(["Metal", "Cabo"])
    s.box(-0.025, -0.28, 0.02, 0.025, 0.04, 0.08, IRON, 0)                           # cano (5)
    s.box(-0.025, -0.02, -0.12, 0.025, 0.05, 0.03, DARK, 1)                          # cabo (5)
    return s


def make_espingarda():
    s = Shape(["Metal", "Coronha"])
    s.box(-0.022, -0.75, 0.02, 0.022, -0.05, 0.065, IRON, 0)                         # cano (5)
    s.box(-0.03, -0.05, -0.04, 0.03, 0.35, 0.05, WOOD, 1)                            # coronha (5)
    return s


def make_flecha():
    s = Shape(["Haste", "Ponta"])
    s.box(-0.008, -0.30, -0.008, 0.008, 0.30, 0.008, WOOD, 0)                        # haste (5)
    cone(s, (0, -0.30, 0), 0.03, (0, -0.40, 0), 3, IRON, 1)                          # ponta (3)
    return s


# ---------------------------------------------------------------------------
# Inimigos (origem no pé, de frente para -Y; mat 0 corpo, 1 sombra, 2 olhos)
# ---------------------------------------------------------------------------

BODY = (150, 150, 150)
SHADE = (80, 80, 80)
EYE = (255, 255, 255)


def eyes(s, y, z, dx, size):
    for x in (-dx, dx):
        s.box(x - size, y - 0.01, z - size, x + size, y + 0.01, z + size, EYE, 2, unlit=True)


def make_rastejador():
    """Baixo e comprido, quatro patas: lento e resistente."""
    s = Shape(["Corpo", "Sombra", "Olhos"])
    s.box(-0.30, -0.70, 0.20, 0.30, 0.60, 0.55, BODY, 0)                             # tronco
    s.box(-0.22, -1.05, 0.25, 0.22, -0.70, 0.55, SHADE, 1)                           # cabeça
    for x in (-0.28, 0.28):
        for y in (-0.55, 0.40):
            s.box(x - 0.07, y - 0.07, 0.0, x + 0.07, y + 0.07, 0.22, SHADE, 1)       # patas
    eyes(s, -1.06, 0.45, 0.12, 0.04)
    return s


def make_cacador():
    """Alto e magro, curvado para a frente: rápido."""
    s = Shape(["Corpo", "Sombra", "Olhos"])
    for x in (-0.12, 0.12):
        s.box(x - 0.05, -0.05, 0.0, x + 0.05, 0.05, 0.85, SHADE, 1)                  # pernas
    s.box(-0.20, -0.25, 0.85, 0.20, 0.10, 1.55, BODY, 0)                             # tronco
    s.box(-0.14, -0.50, 1.40, 0.14, -0.20, 1.70, BODY, 0)                            # cabeça baixa
    for x in (-0.26, 0.26):
        s.box(x - 0.04, -0.45, 0.80, x + 0.04, -0.05, 1.45, SHADE, 1)                # braços longos
    eyes(s, -0.51, 1.58, 0.07, 0.03)
    return s


def make_grande():
    """Largo e pesado: raro, ouve-se antes de ver."""
    s = Shape(["Corpo", "Sombra", "Olhos"])
    for x in (-0.35, 0.35):
        s.box(x - 0.16, -0.16, 0.0, x + 0.16, 0.16, 0.90, SHADE, 1)                  # pernas
    s.box(-0.60, -0.40, 0.90, 0.60, 0.40, 2.10, BODY, 0)                             # tronco
    s.box(-0.28, -0.55, 2.00, 0.28, -0.05, 2.45, BODY, 0)                            # cabeça
    for x in (-0.75, 0.75):
        s.box(x - 0.15, -0.30, 0.70, x + 0.15, 0.10, 2.00, SHADE, 1)                 # braços
    eyes(s, -0.56, 2.28, 0.13, 0.05)
    return s


def main():
    print("Modelos do combate (etapa 07):")
    make_espada().save("espada")
    make_machado().save("machado")
    make_arco().save("arco")
    make_pistola().save("pistola")
    make_espingarda().save("espingarda")
    make_flecha().save("flecha")
    make_rastejador().save("rastejador")
    make_cacador().save("cacador")
    make_grande().save("grande")


if __name__ == "__main__":
    main()
