#!/usr/bin/env python3
"""
make_event_sounds.py - Sons sintetizados dos eventos da etapa 06.

Uso:
    python3 tools/make_event_sounds.py        # grava em assets/sounds/

    estalo-11k.wav   árvore prestes a cair: estalos de madeira cada vez
                     mais juntos (~0,8 s)
    queda-11k.wav    árvore batendo no chão: baque grave + galhos (~1,0 s)
    raizes-11k.wav   barreira de raízes abrindo: rangido grave (~0,9 s)

Feitos no projeto (sem assets de terceiros: nada no CREDITS.md). 11 025 Hz:
são graves e ruidosos (metade da RAM do SPU). Semente fixa: rodar de novo
gera arquivos idênticos. Usa as funções de make_placeholder_sounds.py.
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_placeholder_sounds import save, lowpass, env, TAU  # noqa: E402

rng = random.Random(606)
RATE = 11025


def noise(n):
    return [rng.uniform(-1, 1) for _ in range(n)]


def estalo():
    """Estalos curtos (ruído filtrado com ataque seco), cada vez mais perto."""
    n = int(0.8 * RATE)
    out = [0.0] * n
    t = 0.0
    gap = 0.18
    while t < 0.75:
        start = int(t * RATE)
        size = int(0.035 * RATE)
        burst = lowpass(noise(size), 0.55)
        e = env(size, 2, size / 5)
        amp = 0.5 + 0.5 * t / 0.75
        for i in range(size):
            if start + i < n:
                out[start + i] += burst[i] * e[i] * amp
        t += gap * rng.uniform(0.7, 1.2)
        gap *= 0.8                       # acelera: a árvore vai ceder
    return out


def queda():
    """Baque grave (seno que desce) + estrondo de galhos (ruído abafado)."""
    n = int(1.0 * RATE)
    thump = [math.sin(TAU * (70 - 35 * i / n) * i / RATE) for i in range(n)]
    e1 = env(n, 30, RATE * 0.18)
    crash = lowpass(noise(n), 0.25)
    e2 = env(n, 60, RATE * 0.30)
    twigs = lowpass(noise(n), 0.7)
    e3 = env(n, 200, RATE * 0.12)
    return [thump[i] * e1[i] * 1.0 + crash[i] * e2[i] * 0.8 + twigs[i] * e3[i] * 0.3
            for i in range(n)]


def raizes():
    """Rangido: dente de serra grave com o tom oscilando, mais ruído."""
    n = int(0.9 * RATE)
    out, phase = [], 0.0
    grit = lowpass(noise(n), 0.3)
    for i in range(n):
        f = 55 + 25 * math.sin(TAU * 3.1 * i / RATE) - 20 * i / n
        phase = (phase + f / RATE) % 1.0
        saw = 2 * phase - 1
        out.append(saw * 0.6 + grit[i] * 0.5)
    e = env(n, int(0.08 * RATE), RATE * 0.45)
    return lowpass([out[i] * e[i] for i in range(n)], 0.5)


def main():
    print("Sons dos eventos (etapa 06):")
    save("estalo-11k", estalo(), RATE)
    save("queda-11k", queda(), RATE)
    save("raizes-11k", raizes(), RATE)


if __name__ == "__main__":
    main()
