#!/usr/bin/env python3
"""
analisar_partida.py - Resumo legível de um registro de partida (etapa 07b).

Uso:
    ./dev analisar-partida              # o registro mais novo de logs/
    ./dev analisar-partida ARQUIVO      # um registro específico

Lê as linhas PLAYLOG que o jogo (build com DEBUG_PLAYLOG, o ./dev run)
grava no TTY: INICIO (fase, semente, personagem), RESUMO a cada 30 s e
FIM_FASE (vitória, morte ou pulou). Mostra, por fase, o que interessa para
o balanceamento: vida, munição/mana achada x usada x sobrando, golpes por
arma, mortes por tipo e arma, itens que ficaram no mapa, fugas do grande.
"""
import glob
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def parse(line):
    """'PLAYLOG RESUMO fase=1 t=30 ...' -> ('RESUMO', {'fase': '1', ...})"""
    parts = line.split()
    kind = parts[1] if len(parts) > 1 else ""
    kv = dict(re.findall(r"(\w+)=(\S+)", line))
    return kind, kv


def num(d, k):
    try:
        return int(d.get(k, 0))
    except ValueError:
        return 0


def with_prefix(d, prefix):
    return {k[len(prefix):]: num(d, k) for k in d if k.startswith(prefix)}


def show_phase(start, rows, end):
    last = end or (rows[-1] if rows else None)
    head = "Fase %s  (semente %s, %s)" % (start.get("fase", "?"), start.get("semente", "?"),
                                          start.get("personagem", "?"))
    print("=" * len(head))
    print(head)
    print("=" * len(head))
    if not last:
        print("  (sem dados: a fase terminou antes de 30 s)\n")
        return
    result = {"vitoria": "VENCEU", "morte": "MORREU", "pulou": "PULOU (atalho)"}.get(
        last.get("motivo", ""), "em andamento (sem FIM_FASE)")
    t = num(last, "t")
    print("  Resultado: %s em %d:%02d" % (result, t // 60, t % 60))
    print("  Vida: final %d, mínima %d, dano recebido %d, mortes %d" %
          (num(last, "vida"), num(last, "vida_min"), num(last, "dano"), num(last, "mortes")))
    killers = with_prefix(last, "morto_por_")
    if killers:
        print("  Morto por: " + ", ".join("%s %d" % (k, v) for k, v in killers.items()))
    print("  Curas: magia %d, itens de vida %d" % (num(last, "curas_magia"), num(last, "curas_item")))
    print()
    print("  %-10s %8s %8s %8s" % ("Recurso", "achado", "usado", "sobra"))
    for name, pre in (("flechas", "flechas"), ("balas", "balas"), ("mana", "mana")):
        print("  %-10s %8d %8d %8d" % (name, num(last, pre + "_achada" if pre == "mana" else pre + "_achadas"),
                                       num(last, pre + "_usada" if pre == "mana" else pre + "_usadas"),
                                       num(last, pre + "_sobra")))
    print("  flechas recolhidas do chão: %d" % num(last, "flechas_recolhidas"))
    mana = {k: v for k, v in with_prefix(last, "mana_").items()
            if k not in ("achada", "usada", "sobra")}
    if mana:
        print("  mana por magia: " + ", ".join("%s %d" % (k, v) for k, v in mana.items()))
    uses = with_prefix(last, "usos_")
    if uses:
        print("  golpes/tiros por arma: " + ", ".join("%s %d" % (k, v) for k, v in uses.items()))
    kills = with_prefix(last, "mortos_")
    if kills:
        print("  inimigos mortos (tipo_arma): " + ", ".join("%s %d" % (k, v) for k, v in kills.items()))
    print("  inimigos ainda vivos: %d   fugas do grande: %d" %
          (num(last, "inimigos_vivos"), num(last, "fugas_do_grande")))
    print("  lanterna: pilha final %d, mínima %d, pilhas pegas %d" %
          (num(last, "pilha"), num(last, "pilha_min"), num(last, "pilhas")))
    left = with_prefix(last, "nao_pegos_")
    if left:
        print("  ficaram no mapa: " + ", ".join("%s %d" % (k, v) for k, v in left.items()))
    if rows:
        print()
        print("  %5s %5s %8s %6s %6s %6s" % ("t", "vida", "vida_min", "flech", "balas", "mana"))
        for r in rows + ([end] if end else []):
            print("  %5s %5d %8d %6d %6d %6d" % (r.get("t", "?"), num(r, "vida"), num(r, "vida_min"),
                                                num(r, "flechas_sobra"), num(r, "balas_sobra"),
                                                num(r, "mana_sobra")))
    print()


def main():
    if len(sys.argv) > 1:
        path = sys.argv[1]
    else:
        logs = sorted(glob.glob(os.path.join(ROOT, "logs", "partida-*.txt")))
        if not logs:
            print("Nenhum registro em logs/. Jogue com ./dev run e feche o emulador.")
            return 1
        path = logs[-1]
    print("Registro: %s\n" % os.path.relpath(path, ROOT))
    start, rows, phases = None, [], 0
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            i = line.find("PLAYLOG ")
            if i < 0:
                continue
            kind, kv = parse(line[i:])
            if kind == "INICIO":
                if start is not None:
                    show_phase(start, rows, None)
                start, rows = kv, []
                phases += 1
            elif kind == "RESUMO" and start is not None:
                rows.append(kv)
            elif kind == "FIM_FASE" and start is not None:
                show_phase(start, rows, kv)
                start, rows = None, []
    if start is not None:
        show_phase(start, rows, None)
    if phases == 0:
        print("Nenhuma fase registrada (o jogo chegou a começar uma fase?).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
