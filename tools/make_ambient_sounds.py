#!/usr/bin/env python3
"""
make_ambient_sounds.py - Sons sintetizados do AMBIENTE (etapa 09). Só visual/som.

Uso:
    python3 tools/make_ambient_sounds.py        # grava em assets/sounds/

    asas-11k.wav     bando levantando voo: muitas batidas de asa
    corvo-11k.wav    grito rouco de pássaro (de longe)
    galho-11k.wav    galho estalando no escuro (alguém pisou)
    folhas-11k.wav   folhas mexendo: bicho correndo / vulto passando
    respiro-11k.wav  respiração baixa e lenta (olhos no escuro)

Feitos no projeto (nada no CREDITS.md). 11 025 Hz. Semente fixa: rodar de
novo gera arquivos idênticos.
"""
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_placeholder_sounds import save, lowpass, env, TAU  # noqa: E402

rng = random.Random(909)
RATE = 11025


def noise(n):
    return [rng.uniform(-1, 1) for _ in range(n)]


def asas():
    n = int(0.9 * RATE)
    out = [0.0] * n
    for _ in range(26):                                  # batidas de asa espalhadas
        p = rng.randrange(n - 700)
        burst = lowpass(noise(600), 0.35)
        for k in range(600):
            out[p + k] += burst[k] * math.sin(math.pi * k / 600) * rng.uniform(0.4, 1.0)
    e = env(n, 20, RATE * 0.45)
    return [out[i] * e[i] for i in range(n)]


def corvo():
    n = int(0.45 * RATE)
    out, ph = [], 0.0
    rough = lowpass(noise(n), 0.5)
    for i in range(n):
        f = 520 - 260 * i / n + 40 * math.sin(TAU * 30 * i / RATE)
        ph += f / RATE
        out.append((2 * (ph % 1.0) - 1) * 0.5 + rough[i] * 0.6)
    e = env(n, int(0.02 * RATE), RATE * 0.25)
    return lowpass([out[i] * e[i] for i in range(n)], 0.5)


def galho():
    n = int(0.28 * RATE)
    out = [0.0] * n
    for p in (0, int(0.05 * RATE), int(0.09 * RATE)):    # 3 estalos secos
        for k in range(300):
            if p + k < n:
                out[p + k] += rng.uniform(-1, 1) * math.exp(-k / 40)
    return lowpass(out, 0.8)


def folhas():
    n = int(0.5 * RATE)
    x = lowpass(noise(n), 0.55)
    return [x[i] * (0.5 + 0.5 * math.sin(TAU * 9 * i / RATE)) * math.sin(math.pi * i / n) for i in range(n)]


def respiro():
    n = int(1.2 * RATE)
    x = lowpass(noise(n), 0.08)
    out = []
    for i in range(n):
        t = i / n
        a = math.sin(math.pi * min(1.0, t / 0.45)) if t < 0.45 else 0.6 * math.sin(math.pi * (t - 0.45) / 0.55)
        out.append(x[i] * a + 0.2 * math.sin(TAU * 70 * i / RATE) * a)
    return out


def main():
    print("Sons do ambiente (etapa 09):")
    for name, fn in (("asas", asas), ("corvo", corvo), ("galho", galho), ("folhas", folhas),
                     ("respiro", respiro)):
        save(name + "-11k", fn(), RATE)


if __name__ == "__main__":
    main()
