# -*- coding: utf-8 -*-
"""
fix_gnu_stack.py - Só é usado com o compilador "mipsel-linux-gnu" (Debian/Ubuntu/WSL/Docker)

Esse compilador coloca no ELF um cabeçalho GNU_STACK no endereço 0, e o
elf2x acaba tentando gerar um .exe de 2 GB. Aqui removemos esse cabeçalho
da tabela de program headers e depois chamamos o elf2x de verdade. O CMakeLists.txt liga isso automaticamente.
O compilador recomendado (mipsel-none-elf) não precisa disso.

Uso: fix_gnu_stack.py --elf2x /caminho/elf2x [opções do elf2x] entrada.elf saida.exe
"""
import struct
import subprocess
import sys

PT_GNU_STACK = 0x6474E551


def patch(path):
    with open(path, "r+b") as f:
        data = bytearray(f.read())
        if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
            sys.exit("não é um ELF 32-bit little-endian: " + path)
        phoff, = struct.unpack_from("<I", data, 28)
        phentsize, phnum = struct.unpack_from("<HH", data, 42)
        heads = [bytes(data[phoff + i * phentsize: phoff + (i + 1) * phentsize]) for i in range(phnum)]
        keep = [h for h in heads if struct.unpack_from("<I", h, 0)[0] != PT_GNU_STACK]
        if len(keep) != len(heads):
            for i, h in enumerate(keep):
                data[phoff + i * phentsize: phoff + (i + 1) * phentsize] = h
            struct.pack_into("<H", data, 44, len(keep))   # e_phnum
            f.seek(0)
            f.write(data)


if __name__ == "__main__":
    args = sys.argv[1:]
    if args and args[0] == "--elf2x":
        elf2x, rest = args[1], args[2:]
        for a in rest:
            if a.lower().endswith(".elf"):
                patch(a)
        sys.exit(subprocess.call([elf2x] + rest))
    for a in args:
        patch(a)
