# Etapa 00 — Preparação

> Leia antes: `CLAUDE.md`, `docs/ROADMAP.md`, `docs/HISTORICO.md`.
> Referência de código: `docs/MANUAL-DESENVOLVEDOR.pdf` (receitas R1–R12).

## Objetivo

Preparar o projeto para as próximas etapas, sem mudar a jogabilidade.

## Tarefas

1. **CLAUDE.md**
   - Acrescente uma seção "Roteiro" apontando para `docs/ROADMAP.md` e `docs/prompts/`.
   - Acrescente as **regras de determinismo** do ROADMAP como regras do projeto.
   - Acrescente: "não use `__builtin_bswap32` nem builtins que puxem a libgcc".
2. **Gerador aleatório determinístico** (`src/rng.c` + protótipos em `game.h`):
   - xorshift32 com estado explícito: `typedef struct { uint32_t s; } RNG;`,
     `rng_seed(RNG*, uint32_t)`, `rng_next(RNG*)`, `rng_range(RNG*, lo, hi)`.
   - Dois geradores globais: `g.rng` (lógica, dentro de `GAME`) e um `fx_rng`
     estático para efeitos puramente visuais (fora de `g`).
   - `g.seed` (uint32_t) guardado em `GAME`; a partida chama `rng_seed(&g.rng, g.seed)`.
   - Troque os usos de `rand()`/`rand_range()` da **lógica** por `g.rng`. Mantenha
     `rand_range` como wrapper de `g.rng` para não espalhar mudanças.
   - Hoje a semente vem de `g.frame` ao iniciar a partida; mantenha esse
     comportamento (no modo link ela virá do handshake).
3. **CREDITS.md** na raiz: tabela vazia com colunas Asset | Arquivo | Autor | Fonte (link) | Licença.
4. **Overlay de depuração (L2)**: acrescente a semente (`SEED %08x`) e o número
   de polígonos desenhados no quadro (conte em `render_mesh`).
5. **docs/HISTORICO.md**: registre a etapa.

## Fora do escopo

Qualquer mudança de jogabilidade, visual ou de mapa.

## Critérios de aceite

- `./dev build` sem erros nem avisos novos.
- O jogo se comporta igual ao anterior.
- Com a mesma semente forçada (ex.: `g.seed = 1234` temporário), duas partidas
  com as mesmas entradas têm inimigos se movendo igual.

## Ao terminar

Diga ao Marcos o que testar no emulador (L2 mostra SEED e POLIS) e faça commits
pequenos com mensagens em português.
