#!/usr/bin/env python3
"""
analisar_partida.py - Resumo legível do registro de partida (etapa 07b).

Uso:
    ./dev analisar-partida              # o registro mais novo de logs/ (partida ou robô)
    ./dev analisar-partida ARQUIVO      # um registro específico
    ./dev analisar-partida --robo       # o registro mais novo do robô (logs/robo-*.txt)
    ./dev analisar-partida --detalhe ARQUIVO   # robô: também cada partida

Lê as linhas PLAYLOG que o jogo (build com DEBUG_PLAYLOG: ./dev run e
./dev robo) grava no TTY: INICIO (fase, semente, personagem), RESUMO a cada
30 s e FIM_FASE (vitoria, morte, pulou, tempo). Cada registro vem em várias
linhas (o DuckStation corta linhas longas): as "PLAYLOG MAIS ..." completam
a anterior.

Partida de uma pessoa: mostra cada fase (vida, munição/mana achada x usada
x sobrando, golpes por arma, mortes, itens que ficaram no mapa).
Robô (linhas ROBO ...): agrega por fase e personagem (vitórias/mortes,
causa das mortes, tempo, sobra/falta de recurso, quem mais causou dano).
"""
import glob
import os
import re
import sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CHARS = ["ROBO", "CAVALEIRO", "BATEDOR"]


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


def read_matches(path):
    """Lista de partidas: {'start', 'rows', 'end', 'robo'} (registros já juntos)."""
    matches, cur, last = [], None, None
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            j = line.find("ROBO MAIS")
            if j >= 0:                              # continuação do relatório do robô
                for m in reversed(matches):
                    if m["robo"] is not None:
                        m["robo"].update(parse(line[j:])[1])
                        break
                continue
            j = line.find("ROBO FIM_PARTIDA")
            if j >= 0:
                _, kv = parse(line[j:])
                # o relatório do robô vem logo depois do FIM_FASE da mesma partida
                for m in reversed(matches):
                    if m["start"].get("fase") == kv.get("fase") and m["robo"] is None:
                        m["robo"] = kv
                        break
                continue
            i = line.find("PLAYLOG ")
            if i < 0:
                continue
            kind, kv = parse(line[i:])
            if kind == "MAIS":
                if last is not None:
                    last.update(kv)
                continue
            if kind == "INICIO":
                if cur is not None:
                    matches.append(cur)
                cur = {"start": kv, "rows": [], "end": None, "robo": None}
                last = None
            elif kind == "RESUMO" and cur is not None:
                cur["rows"].append(kv)
                last = kv
            elif kind == "FIM_FASE" and cur is not None:
                cur["end"] = kv
                last = kv
                matches.append(cur)
                cur = None
    if cur is not None:
        matches.append(cur)
    return matches


# ---------------------------------------------------------------------------
# Uma partida (pessoa jogando)
# ---------------------------------------------------------------------------
def show_phase(m):
    start, rows, end = m["start"], m["rows"], m["end"]
    last = end or (rows[-1] if rows else None)
    head = "Fase %s  (semente %s, %s)" % (start.get("fase", "?"), start.get("semente", "?"),
                                          start.get("personagem", "?"))
    print("=" * len(head))
    print(head)
    print("=" * len(head))
    if not last:
        print("  (sem dados: a fase terminou antes de 30 s)\n")
        return
    result = {"vitoria": "VENCEU", "morte": "MORREU", "pulou": "PULOU (atalho)",
              "tempo": "TEMPO ESGOTADO"}.get(last.get("motivo", ""), "em andamento (sem FIM_FASE)")
    t = num(last, "t")
    print("  Resultado: %s em %d:%02d" % (result, t // 60, t % 60))
    print("  Vida: final %d, mínima %d, dano recebido %d, mortes %d" %
          (num(last, "vida"), num(last, "vida_min"), num(last, "dano"), num(last, "mortes")))
    killers = with_prefix(last, "morto_por_")
    if killers:
        print("  Morto por: " + ", ".join("%s %d" % (k, v) for k, v in killers.items()))
    dmg = with_prefix(last, "dano_de_")
    if dmg:
        print("  Dano por inimigo: " + ", ".join("%s %d" % (k, v) for k, v in dmg.items()))
    print("  Curas: magia %d, itens de vida %d" % (num(last, "curas_magia"), num(last, "curas_item")))
    print()
    print("  %-10s %8s %8s %8s" % ("Recurso", "achado", "usado", "sobra"))
    for name, a, u in (("flechas", "flechas_achadas", "flechas_usadas"),
                       ("balas", "balas_achadas", "balas_usadas"),
                       ("mana", "mana_achada", "mana_usada")):
        print("  %-10s %8d %8d %8d" % (name, num(last, a), num(last, u), num(last, name + "_sobra")))
    print("  flechas recolhidas do chão: %d" % num(last, "flechas_recolhidas"))
    mana = {k: v for k, v in with_prefix(last, "mana_").items() if k not in ("achada", "usada", "sobra")}
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
    if m.get("robo"):
        r = m["robo"]
        print("  robô: presos %d, sem cura %d, sem arma de longe %d s, fugas %d" %
              (num(r, "presos"), num(r, "sem_cura"), num(r, "sem_distancia_s"), num(r, "fugas")))
    if rows:
        print()
        print("  %5s %5s %8s %6s %6s %6s" % ("t", "vida", "vida_min", "flech", "balas", "mana"))
        for r in rows + ([end] if end else []):
            print("  %5s %5d %8d %6d %6d %6d" % (r.get("t", "?"), num(r, "vida"), num(r, "vida_min"),
                                                num(r, "flechas_sobra"), num(r, "balas_sobra"),
                                                num(r, "mana_sobra")))
    print()


# ---------------------------------------------------------------------------
# Robô: agregado por fase x personagem
# ---------------------------------------------------------------------------
def avg(v):
    return sum(v) / len(v) if v else 0.0


def mmm(v):
    return "%d/%d/%d" % (min(v), round(avg(v)), max(v)) if v else "-"


def robot_summary(matches, detail):
    done = [m for m in matches if m["end"]]
    groups = defaultdict(list)
    for m in done:
        groups[(num(m["start"], "fase"), m["start"].get("personagem", "?"))].append(m)
    phases = sorted({k[0] for k in groups})
    print("Robô: %d partidas (%d sementes)\n" % (len(done), len({m["start"].get("semente") for m in done})))

    fmt = "%-4s %-10s %3s %4s %4s %5s | %-12s | %-11s | %5s %5s | %s"
    print(fmt % ("fase", "personagem", "n", "venc", "mort", "tempo", "t venceu (s)", "vida_min", "cura", "cura",
                 "morto por"))
    print(fmt % ("", "", "", "", "", "esgot", "min/med/max", "min/med/max", "magia", "item", ""))
    for f in phases:
        for c in CHARS:
            ms = groups.get((f, c))
            if not ms:
                continue
            ends = [m["end"] for m in ms]
            res = defaultdict(int)
            for e in ends:
                res[e.get("motivo", "?")] += 1
            killers = defaultdict(int)
            for e in ends:
                for k, v in with_prefix(e, "morto_por_").items():
                    killers[k] += v
            tw = [num(e, "t") for e in ends if e.get("motivo") == "vitoria"]
            print("%-4d %-10s %3d %4d %4d %5d | %-12s | %-11s | %5.1f %5.1f | %s" %
                  (f, c, len(ms), res["vitoria"], res["morte"], res["tempo"], mmm(tw),
                   mmm([num(e, "vida_min") for e in ends]),
                   avg([num(e, "curas_magia") for e in ends]), avg([num(e, "curas_item") for e in ends]),
                   ", ".join("%s %d" % kv for kv in sorted(killers.items(), key=lambda x: -x[1])) or "-"))
    print()

    print("Recursos no fim (média por partida): achado/usado/sobra")
    fmt = "%-4s %-10s | %-14s | %-14s | %-16s | %5s | %5s %6s | %s"
    print(fmt % ("fase", "personagem", "flechas", "balas", "mana", "recol", "sem", "sem", "dano recebido (média)"))
    print(fmt % ("", "", "", "", "", "flech", "cura", "dist s", "por inimigo"))
    for f in phases:
        for c in CHARS:
            ms = groups.get((f, c))
            if not ms:
                continue
            ends = [m["end"] for m in ms]
            robos = [m["robo"] or {} for m in ms]

            def trio(a, u, s):
                return "%.1f/%.1f/%.1f" % (avg([num(e, a) for e in ends]), avg([num(e, u) for e in ends]),
                                           avg([num(e, s) for e in ends]))
            dmg = defaultdict(int)
            for e in ends:
                for k, v in with_prefix(e, "dano_de_").items():
                    dmg[k] += v
            dmg_s = ", ".join("%s %d" % (k, v / len(ends)) for k, v in sorted(dmg.items(), key=lambda x: -x[1]))
            print("%-4d %-10s | %-14s | %-14s | %-16s | %5.1f | %5.2f %6.1f | %s" %
                  (f, c, trio("flechas_achadas", "flechas_usadas", "flechas_sobra"),
                   trio("balas_achadas", "balas_usadas", "balas_sobra"),
                   trio("mana_achada", "mana_usada", "mana_sobra"),
                   avg([num(e, "flechas_recolhidas") for e in ends]),
                   avg([num(r, "sem_cura") for r in robos]), avg([num(r, "sem_distancia_s") for r in robos]),
                   dmg_s or "-"))
    print()

    print("Inimigos mortos por arma (soma) e o que sobrou no mapa (média por partida)")
    for f in phases:
        for c in CHARS:
            ms = groups.get((f, c))
            if not ms:
                continue
            ends = [m["end"] for m in ms]
            kills = defaultdict(int)
            for e in ends:
                for k, v in with_prefix(e, "mortos_").items():
                    kills[k] += v
            left = defaultdict(int)
            for e in ends:
                for k, v in with_prefix(e, "nao_pegos_").items():
                    left[k] += v
            robos = [m["robo"] or {} for m in ms]
            print("  f%d %-10s mortos: %s" % (f, c, ", ".join("%s %d" % kv for kv in
                                                              sorted(kills.items(), key=lambda x: -x[1])) or "-"))
            print("  %-13s inventário no fim: bandagem %.1f kit %.1f poção %.1f pilha %.1f | curas pelo atalho %.1f" %
                  ("", avg([num(r, "inv_bandagem") for r in robos]), avg([num(r, "inv_kit") for r in robos]),
                   avg([num(r, "inv_pocao") for r in robos]), avg([num(r, "inv_pilha") for r in robos]),
                   avg([num(r, "curas_atalho") for r in robos])))
            print("  %-13s sobrou: %s | inimigos vivos %.1f | robô preso %.1f | fugas do grande %.1f" %
                  ("", ", ".join("%s %.1f" % (k, v / len(ends)) for k, v in sorted(left.items())) or "-",
                   avg([num(e, "inimigos_vivos") for e in ends]), avg([num(r, "presos") for r in robos]),
                   avg([num(r, "fugas") for r in robos])))
    print()

    # extremos: mapas em que nenhum dos 3 personagens venceu
    by_seed = defaultdict(list)
    for m in done:
        by_seed[(num(m["start"], "fase"), m["start"].get("semente"))].append(m["end"].get("motivo"))
    hard = sorted(k for k, v in by_seed.items() if v and "vitoria" not in v)
    print("Mapas em que NINGUÉM venceu (fase semente): " +
          (", ".join("f%d %s" % k for k in hard[:24]) + (" ..." if len(hard) > 24 else "") if hard else "nenhum"))
    if detail:
        print()
        for m in done:
            show_phase(m)


def main():
    args = sys.argv[1:]
    detail = "--detalhe" in args
    robo = "--robo" in args
    args = [a for a in args if a not in ("--detalhe", "--robo")]
    if args:
        path = args[0]
    else:
        pat = "robo-*.txt" if robo else "*.txt"
        logs = sorted(glob.glob(os.path.join(ROOT, "logs", pat)), key=os.path.getmtime)
        if not logs:
            print("Nenhum registro em logs/. Jogue com ./dev run (e feche o emulador) ou rode ./dev robo.")
            return 1
        path = logs[-1]
    print("Registro: %s\n" % os.path.relpath(path, ROOT))
    matches = read_matches(path)
    if not matches:
        print("Nenhuma fase registrada (o jogo chegou a começar uma fase?).")
        return 0
    if any(m["robo"] for m in matches):
        robot_summary(matches, detail)
    else:
        for m in matches:
            show_phase(m)
    return 0


if __name__ == "__main__":
    sys.exit(main())
