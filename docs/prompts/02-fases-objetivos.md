# Etapa 02 — Fases e objetivos

> Leia antes: `CLAUDE.md`, `docs/ROADMAP.md`, manual capítulos 13 e 14 (receitas R3 e R12).

## Objetivo

Estrutura de várias fases com objetivos diferentes, tela de introdução e
progressão. Os mapas ainda são de texto (a floresta procedural chega nas
etapas 04–05), mas a estrutura já deve servir para ela.

## Tarefas

1. **`LEVEL_DEF`** (receita R3, ampliada) em `src/levels.c` (arquivo novo):
   nome, mapa (por enquanto texto), texturas, cor do céu, faixa de música,
   **tipo de objetivo**, parâmetros do objetivo e texto do objetivo (ASCII).
2. **Tipos de objetivo** (`enum`): `OBJ_KILL_ALL`, `OBJ_REACH_EXIT` (célula
   `X` no mapa), `OBJ_COLLECT` (N itens de missão, caractere `Q` no mapa), `OBJ_SURVIVE` (sobreviver
   N segundos). Função `objective_update()` decide vitória; `objective_text()`
   devolve a linha para o HUD.
3. **Saída de fase** (`X`): modelo simples (marco de pedra/porta de luz) com
   efeito, ativa só quando o objetivo permitir.
4. **Tela de introdução** (receita R12) com nome da fase e objetivo.
5. **Progressão**: vencer → próxima fase; pontuação e armas/itens do jogador
   passam para a fase seguinte; game over → recomeça a fase atual.
6. **4 fases de teste** com os mapas atuais/variações, uma de cada tipo de
   objetivo (KILL_ALL, REACH_EXIT, COLLECT, SURVIVE).
7. HUD: linha de objetivo no lugar de "INIMIGOS n" quando fizer sentido.

## Fora do escopo

Floresta, geração procedural, salvar progresso no memory card.

## Critérios de aceite

- `./dev build` limpo.
- As 4 fases jogáveis em sequência; cada objetivo vence corretamente.
- Modo 2 jogadores continua funcionando (objetivos valem para o grupo).

## Ao terminar

Atualize `docs/GUIA.md` ("Criar uma fase e escolher o objetivo") e o
`HISTORICO.md`; diga ao Marcos como pular para uma fase específica para testar.
