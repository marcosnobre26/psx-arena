#!/usr/bin/env python3
"""
credits_gen.py - Gera build/gen/credits_gen.c a partir do CREDITS.md (etapa 10).

Chamado pelo CMake a cada build em que o CREDITS.md mudou. A tela de
créditos do jogo mostra estas linhas (ASCII sem acentos: a fonte do PS1
não tem acentos; os acentos são trocados pela letra simples).

    python3 tools/credits_gen.py CREDITS.md build/gen/credits_gen.c
"""
import re
import sys
import unicodedata

HEAD = [
    "MATA ESCURA",
    "VERSAO 0.9.0 - FLORESTA",
    "",
    "CRIADO POR",
    "MARCOS NOBRE",
    "",
    "FEITO COM",
    "PSN00BSDK (LIVRE)",
    "BLENDER",
    "",
    "MODELOS, TEXTURAS E SONS",
    "FEITOS NO PROJETO",
    "(SCRIPTS EM TOOLS/)",
    "",
    "ASSETS DE TERCEIROS",
    "(DO CREDITS.MD)",
]


def ascii_up(s):
    s = unicodedata.normalize("NFKD", s)
    s = "".join(c for c in s if not unicodedata.combining(c))
    s = s.encode("ascii", "ignore").decode()
    s = re.sub(r"[`*_\[\]]", "", s)
    return s.upper().strip()


def main():
    src, dst = sys.argv[1], sys.argv[2]
    rows = []
    for line in open(src, encoding="utf-8"):
        line = line.strip()
        if not line.startswith("|") or set(line) <= set("|-: "):
            continue
        cells = [c.strip() for c in line.strip("|").split("|")]
        if cells and cells[0].lower() == "asset":
            continue                                     # cabeçalho da tabela
        if len(cells) >= 5:
            rows.append("%s - %s" % (ascii_up(cells[0]), ascii_up(cells[2])))
            rows.append("  %s" % ascii_up(cells[4]))
    lines = HEAD + (rows if rows else ["NENHUM ATE AGORA"]) + ["", "OBRIGADO POR JOGAR"]
    lines = [l[:36] for l in lines]                      # 36 colunas cabem na tela
    with open(dst, "w") as f:
        f.write("/* GERADO por tools/credits_gen.py a partir do CREDITS.md - nao edite. */\n")
        f.write("const char *const credits_lines[] = {\n")
        for l in lines:
            f.write('\t"%s",\n' % l.replace("\\", "\\\\").replace('"', "'"))
        f.write("};\nconst int credits_count = %d;\n" % len(lines))


if __name__ == "__main__":
    main()
