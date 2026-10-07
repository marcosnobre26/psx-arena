#!/usr/bin/env python3
"""
wav2vag.py - Converte WAV (PCM 8/16 bits) para VAG (SPU-ADPCM do PS1).

Uso:
    python3 tools/wav2vag.py entrada.wav saida.vag
    python3 tools/wav2vag.py --selftest entrada.wav     # mede a qualidade

O nome do arquivo controla a conversão (como os sufixos das texturas):
    passo.wav          22 050 Hz, toca uma vez
    passo-11k.wav      11 025 Hz (metade da memória; bom para efeitos graves)
    vento-loop.wav     repete sem parar (flags de loop nos blocos ADPCM)
    vento-loop-11k.wav os dois

Formato SPU-ADPCM (o que o chip de som do PS1 entende):
    blocos de 16 bytes, cada um com 28 amostras de 4 bits.
    byte 0: (filtro << 4) | shift   byte 1: flags   bytes 2..15: amostras
    Cada amostra decodificada = (nibble << 12 >> shift) + previsão, onde a
    previsão vem das 2 amostras anteriores por um dos 5 filtros abaixo.
    O encoder escolhe, por bloco, o filtro/shift que erra menos.

Flags de cada bloco:
    1 = fim (para, ou pula para o início do loop se tiver o bit 2)
    2 = repetir (junto com 1: volta ao início do loop)
    4 = início do loop
    Som normal: último bloco = 1.  Loop: primeiro = 4, último = 1|2 = 3.

Memória: 16 bytes a cada 28 amostras -> ~12,6 KB por segundo a 22 050 Hz.

Só usa a biblioteca padrão do Python (sem numpy/audioop).
"""
import math
import os
import re
import struct
import sys
import wave

# Coeficientes dos 5 filtros do SPU (em 1/64): previsão = s1*k0 + s2*k1
FILTERS = [(0, 0), (60, 0), (115, -52), (98, -55), (122, -60)]

RATE_SUFFIX = {"11k": 11025, "22k": 22050}
DEFAULT_RATE = 22050


def parse_name(path):
    """Lê os sufixos do nome: devolve (taxa, loop)."""
    stem = os.path.splitext(os.path.basename(path))[0].lower()
    rate, loop = DEFAULT_RATE, False
    while True:
        m = re.search(r"-(loop|11k|22k)$", stem)
        if not m:
            break
        if m.group(1) == "loop":
            loop = True
        else:
            rate = RATE_SUFFIX[m.group(1)]
        stem = stem[: m.start()]
    return rate, loop


def read_wav(path):
    """Lê o WAV e devolve (amostras mono int16, taxa)."""
    try:
        w = wave.open(path, "rb")
    except wave.Error as e:
        sys.exit(f"{path}: WAV não suportado ({e}). Exporte como PCM 16 bits.")
    with w:
        ch, width, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if width == 2:
        data = struct.unpack(f"<{len(raw) // 2}h", raw)
    elif width == 1:                       # 8 bits é sem sinal
        data = [(b - 128) << 8 for b in raw]
    else:
        sys.exit(f"{path}: {width * 8} bits não suportado. Exporte como PCM 16 bits.")
    if ch > 1:                             # estéreo (ou mais): mixa para mono
        data = [sum(data[i:i + ch]) // ch for i in range(0, len(data), ch)]
    return list(data), rate


def resample(samples, src, dst):
    """Reamostragem por interpolação linear (suficiente para efeitos)."""
    if src == dst or not samples:
        return samples
    n = max(1, len(samples) * dst // src)
    out = []
    last = len(samples) - 1
    for i in range(n):
        pos = i * src / dst
        j = int(pos)
        f = pos - j
        a = samples[min(j, last)]
        b = samples[min(j + 1, last)]
        out.append(int(round(a + (b - a) * f)))
    return out


def clamp16(v):
    return -32768 if v < -32768 else 32767 if v > 32767 else v


def encode_block(block, s1, s2):
    """Codifica 28 amostras. Devolve (bytes, novo s1, novo s2, erro)."""
    best = None
    for f, (k0, k1) in enumerate(FILTERS):
        # estimativa do shift: o maior resíduo precisa caber em 4 bits
        p1, p2, peak = s1, s2, 0
        for x in block:
            r = x - ((p1 * k0 + p2 * k1 + 32) >> 6)
            peak = max(peak, abs(r))
            p2, p1 = p1, x
        bits = 0
        while bits < 12 and (7 << bits) < peak:
            bits += 1
        # testa a estimativa e um shift a menos (às vezes satura menos)
        for shift in {12 - bits, max(0, 11 - bits)}:
            step = 1 << (12 - shift)
            p1, p2, err, nibs = s1, s2, 0, []
            for x in block:
                pred = (p1 * k0 + p2 * k1 + 32) >> 6
                q = int(round((x - pred) / step))
                q = -8 if q < -8 else 7 if q > 7 else q
                y = clamp16(((q << 12) >> shift) + pred)   # o que o SPU vai tocar
                err += (x - y) * (x - y)
                nibs.append(q & 15)
                p2, p1 = p1, y
            if best is None or err < best[3]:
                best = ((f << 4) | shift, nibs, (p1, p2), err)
    hdr, nibs, (n1, n2), err = best
    data = bytes([hdr, 0]) + bytes(nibs[i] | (nibs[i + 1] << 4) for i in range(0, 28, 2))
    return data, n1, n2, err


def encode(samples, loop):
    """Codifica todas as amostras em blocos de 16 bytes, com as flags certas."""
    if not samples:
        samples = [0]
    pad = (-len(samples)) % 28
    samples = samples + [0] * pad
    blocks, s1, s2 = [], 0, 0
    for i in range(0, len(samples), 28):
        data, s1, s2, _ = encode_block(samples[i:i + 28], s1, s2)
        blocks.append(bytearray(data))
    if loop:
        blocks[0][1] |= 4                  # início do loop
        blocks[-1][1] |= 3                 # fim + repetir
    else:
        blocks[-1][1] |= 1                 # fim: a voz para
    return b"".join(blocks)


def decode(adpcm):
    """Decodificador (igual ao SPU) para o --selftest."""
    out, s1, s2 = [], 0, 0
    for i in range(0, len(adpcm), 16):
        hdr = adpcm[i]
        shift, (k0, k1) = hdr & 15, FILTERS[min(hdr >> 4, 4)]
        for b in adpcm[i + 2:i + 16]:
            for nib in (b & 15, b >> 4):
                q = nib - 16 if nib & 8 else nib
                y = clamp16(((q << 12) >> shift) + ((s1 * k0 + s2 * k1 + 32) >> 6))
                out.append(y)
                s2, s1 = s1, y
    return out


def vag_header(size, rate, name):
    """Cabeçalho de 48 bytes, big-endian (o PS1 é little-endian: o jogo
    converte com be32() em sound.c)."""
    name = re.sub(r"[^A-Za-z0-9_]", "_", name)[:16].encode("ascii")
    return (b"VAGp" + struct.pack(">IIII", 0x20, 0, size, rate)
            + bytes(12) + name.ljust(16, b"\0"))


def main():
    args = sys.argv[1:]
    selftest = "--selftest" in args
    args = [a for a in args if a != "--selftest"]
    if len(args) != (1 if selftest else 2):
        sys.exit(__doc__)

    src = args[0]
    rate, loop = parse_name(src)
    samples, src_rate = read_wav(src)
    samples = resample(samples, src_rate, rate)
    adpcm = encode(samples, loop)

    if selftest:
        dec = decode(adpcm)[:len(samples)]
        sig = sum(x * x for x in samples) or 1
        noise = sum((a - b) ** 2 for a, b in zip(samples, dec)) or 1
        print(f"{os.path.basename(src)}: {rate} Hz, loop={loop}, {len(samples)} amostras, "
              f"{len(adpcm)} bytes ADPCM, SNR {10 * math.log10(sig / noise):.1f} dB")
        return

    name = os.path.splitext(os.path.basename(args[1]))[0]
    os.makedirs(os.path.dirname(os.path.abspath(args[1])), exist_ok=True)
    with open(args[1], "wb") as f:
        f.write(vag_header(len(adpcm), rate, name) + adpcm)


if __name__ == "__main__":
    main()
