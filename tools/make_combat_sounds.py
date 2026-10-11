#!/usr/bin/env python3
"""
make_combat_sounds.py - Sons sintetizados do combate (etapa 07).

Uso:
    python3 tools/make_combat_sounds.py        # grava em assets/sounds/

    espada-11k.wav    golpe de espada: sopro agudo e curto
    machado-11k.wav   golpe de machado: sopro grave + pancada
    arco-11k.wav      arco soltando: corda vibrando
    flecha-11k.wav    flecha batendo: estalo seco
    fogo-11k.wav      bola de fogo: sopro + crepitar
    clarao-11k.wav    clarão: zumbido que sobe
    pisada-11k.wav    passo do grande: baque grave
    grito-11k.wav     inimigo que começa a perseguir: grito rouco

Feitos no projeto (nada no CREDITS.md). 11 025 Hz (metade da RAM do SPU).
Semente fixa: rodar de novo gera arquivos idênticos.
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_placeholder_sounds import save, lowpass, env, TAU  # noqa: E402

rng = random.Random(707)
RATE = 11025


def noise(n):
    return [rng.uniform(-1, 1) for _ in range(n)]


def whoosh(seconds, cut):
    n = int(seconds * RATE)
    x = noise(n)
    out, y = [], 0.0
    for i in range(n):
        a = cut * (0.3 + 0.7 * math.sin(math.pi * i / n))     # abre e fecha
        y += a * (x[i] - y)
        out.append(y * math.sin(math.pi * i / n))
    return out


def espada():
    return whoosh(0.22, 0.6)


def machado():
    w = whoosh(0.30, 0.25)
    n = len(w)
    thump = [math.sin(TAU * 90 * i / RATE) * math.exp(-i / (RATE * 0.05)) for i in range(n)]
    start = int(0.18 * RATE)
    return [w[i] + (thump[i - start] * 1.2 if i >= start else 0) for i in range(n)]


def arco():
    n = int(0.35 * RATE)
    e = env(n, 5, RATE * 0.08)
    return [math.sin(TAU * (180 - 60 * i / n) * i / RATE) * e[i] +
            0.3 * math.sin(TAU * 360 * i / RATE) * e[i] for i in range(n)]


def flecha():
    n = int(0.15 * RATE)
    x = lowpass(noise(n), 0.7)
    e = env(n, 2, RATE * 0.02)
    return [x[i] * e[i] + 0.5 * math.sin(TAU * 140 * i / RATE) * e[i] for i in range(n)]


def fogo():
    w = whoosh(0.5, 0.3)
    n = len(w)
    crack = [0.0] * n
    for _ in range(14):
        p = rng.randrange(n - 200)
        for k in range(120):
            crack[p + k] += rng.uniform(-1, 1) * math.exp(-k / 30)
    return [w[i] + 0.5 * crack[i] for i in range(n)]


def clarao():
    n = int(0.6 * RATE)
    out, ph = [], 0.0
    for i in range(n):
        f = 200 + 1400 * (i / n) ** 2
        ph += f / RATE
        out.append((2 * (ph % 1.0) - 1) * math.sin(math.pi * i / n))
    return lowpass(out, 0.5)


def pisada():
    n = int(0.45 * RATE)
    thump = [math.sin(TAU * (55 - 20 * i / n) * i / RATE) for i in range(n)]
    e = env(n, 15, RATE * 0.10)
    dirt = lowpass(noise(n), 0.15)
    return [thump[i] * e[i] + 0.4 * dirt[i] * e[i] for i in range(n)]


def grito():
    n = int(0.7 * RATE)
    out, ph = [], 0.0
    rough = lowpass(noise(n), 0.4)
    for i in range(n):
        f = 320 + 120 * math.sin(TAU * 7 * i / RATE) - 150 * i / n
        ph += f / RATE
        saw = 2 * (ph % 1.0) - 1
        out.append(saw * 0.6 + rough[i] * 0.5)
    e = env(n, int(0.04 * RATE), RATE * 0.35)
    return lowpass([out[i] * e[i] for i in range(n)], 0.45)


def main():
    print("Sons do combate (etapa 07):")
    for name, fn in (("espada", espada), ("machado", machado), ("arco", arco), ("flecha", flecha),
                     ("fogo", fogo), ("clarao", clarao), ("pisada", pisada), ("grito", grito)):
        save(name + "-11k", fn(), RATE)


if __name__ == "__main__":
    main()
