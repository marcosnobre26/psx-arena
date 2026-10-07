#!/usr/bin/env python3
"""
make_placeholder_sounds.py - Gera sons PROVISÓRIOS sintetizados.

Uso:
    python3 tools/make_placeholder_sounds.py          # grava em assets/sounds/

São só para o jogo ter som enquanto não há sons reais: ruído filtrado,
senos e dentes de serra. Para trocar um deles, substitua o WAV de mesmo
nome em assets/sounds/ (veja docs/GUIA.md -> "Sons e música").

Escolhas de memória (RAM do SPU: ~12,6 KB por segundo a 22 050 Hz):
    efeitos graves/ruidosos e o vento usam -11k (metade da memória);
    sons com agudos (item, grilos) ficam em 22 050 Hz.
Loops têm duração múltipla de 28 amostras (um bloco ADPCM), para não
sobrar um pedaço de silêncio no ponto em que repetem.

Semente fixa: rodar de novo gera arquivos idênticos.
"""
import math
import os
import random
import struct
import wave

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "sounds")
rng = random.Random(2026)
TAU = 2 * math.pi


def save(name, samples, rate):
    """Normaliza para ~-3 dB e grava WAV 16 bits mono."""
    peak = max(1e-9, max(abs(x) for x in samples))
    data = [int(round(x / peak * 23000)) for x in samples]
    path = os.path.join(OUT, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(struct.pack(f"<{len(data)}h", *data))
    print(f"  {name}.wav  {len(data) / rate:.2f} s  {rate} Hz")


def noise(n):
    return [rng.uniform(-1, 1) for _ in range(n)]


def lowpass(x, a):
    """Filtro passa-baixa de um polo (a perto de 0 = mais abafado)."""
    out, y = [], 0.0
    for s in x:
        y += a * (s - y)
        out.append(y)
    return out


def env(n, attack, decay):
    """Envelope: sobe em 'attack' amostras e cai exponencialmente."""
    return [min(1.0, i / max(1, attack)) * math.exp(-i / decay) for i in range(n)]


def loop_len(seconds, rate):
    return int(seconds * rate) // 28 * 28


# ----------------------------------------------------------------------
def passo(variant):
    r = 11025
    n = int(0.16 * r)
    cut = 0.18 if variant == 1 else 0.24          # duas "solas" diferentes
    x = lowpass(noise(n), cut)
    thump = [math.sin(TAU * (70 + 15 * variant) * i / r) for i in range(n)]
    e = env(n, 20, 260)
    return [(a * 0.8 + b * 0.5) * k for a, b, k in zip(x, thump, e)], r


def tiro():
    r = 11025
    n = int(0.35 * r)
    x = lowpass(noise(n), 0.5)
    boom = [math.sin(TAU * (110 * math.exp(-i / 900)) * i / r) for i in range(n)]
    e = env(n, 5, 700)
    return [(a + b * 0.7) * k for a, b, k in zip(x, boom, e)], r


def acerto():
    r = 11025
    n = int(0.12 * r)
    x = lowpass(noise(n), 0.35)
    tone = [math.sin(TAU * 220 * i / r) for i in range(n)]
    e = env(n, 3, 240)
    return [(a + b * 0.6) * k for a, b, k in zip(x, tone, e)], r


def dor():
    r = 11025
    n = int(0.32 * r)
    out, ph = [], 0.0
    for i in range(n):
        f = 320 - 140 * i / n                     # "uh" descendo
        ph += f / r
        saw = 2 * (ph % 1) - 1
        out.append(saw)
    out = lowpass(out, 0.3)
    e = env(n, 80, 1400)
    return [a * k for a, k in zip(out, e)], r


def morte():
    r = 11025
    n = int(0.65 * r)
    x = lowpass(noise(n), 0.25)
    out, ph = [], 0.0
    for i in range(n):
        ph += (180 * math.exp(-i / 2500)) / r
        out.append(math.sin(TAU * ph))
    e = env(n, 30, 2200)
    return [(a * 0.7 + b) * k for a, b, k in zip(x, out, e)], r


def item():
    r = 22050
    n = int(0.28 * r)
    half = n // 2
    out = []
    for i in range(n):
        f = 880 if i < half else 1320             # duas notas subindo
        t = i - (0 if i < half else half)
        out.append(math.sin(TAU * f * i / r) * math.exp(-t / 2500))
    return out, r


def rosnado():
    r = 11025
    n = int(0.9 * r)
    x = lowpass(noise(n), 0.08)
    out, ph = [], 0.0
    for i in range(n):
        ph += (62 + 6 * math.sin(TAU * 3 * i / r)) / r
        saw = 2 * (ph % 1) - 1
        trem = 0.6 + 0.4 * math.sin(TAU * 11 * i / r)   # "rrr" tremido
        out.append(saw * trem)
    out = lowpass(out, 0.15)
    e = env(n, 900, 3500)
    return [(a + b * 1.5) * k for a, b, k in zip(out, x, e)], r


def vento():
    r = 11025
    n = loop_len(4.0, r)
    # ruído "circular": os últimos 0,5 s se misturam com o início, então
    # o fim emenda no começo sem estalo
    fade = r // 2
    raw = noise(n + fade)
    x = raw[:n]
    for i in range(fade):
        k = i / fade
        x[i] = raw[n + i] * (1 - k) + raw[i] * k
    x = lowpass(lowpass(x, 0.05), 0.08)
    # rajadas: 2 ciclos completos no loop (periódico, emenda certinho)
    return [s * (0.55 + 0.45 * math.sin(TAU * 2 * i / n)) for i, s in enumerate(x)], r


def grilos():
    r = 22050
    n = loop_len(2.0, r)
    out = [0.0] * n
    for start in (0.10, 0.24, 0.38, 1.05, 1.19, 1.33, 1.62):   # trinados
        s0 = int(start * r)
        for i in range(int(0.07 * r)):
            if s0 + i < n:
                pulse = 0.5 + 0.5 * math.sin(TAU * 60 * i / r)
                out[s0 + i] += math.sin(TAU * 4400 * i / r) * pulse * math.sin(math.pi * i / (0.07 * r))
    bg = lowpass(noise(n), 0.02)
    return [a * 0.8 + b * 0.15 for a, b in zip(out, bg)], r


SOUNDS = [
    ("passo1-11k", lambda: passo(1)),
    ("passo2-11k", lambda: passo(2)),
    ("tiro-11k", tiro),
    ("acerto-11k", acerto),
    ("dor-11k", dor),
    ("morte-11k", morte),
    ("item", item),
    ("rosnado-11k", rosnado),
    ("vento-loop-11k", vento),
    ("grilos-loop", grilos),
]


def main():
    os.makedirs(OUT, exist_ok=True)
    print("Sons provisórios em assets/sounds/:")
    for name, fn in SOUNDS:
        samples, rate = fn()
        save(name, samples, rate)


if __name__ == "__main__":
    main()
