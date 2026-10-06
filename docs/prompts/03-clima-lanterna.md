# Etapa 03 — Clima: escuridão, névoa e lanterna

> Leia antes: `CLAUDE.md`, `docs/ROADMAP.md`, manual capítulo 4 (GTE, OT).

## Objetivo

Dar ao jogo o clima de terror do PS1: visão curta, escuridão que engole o
fundo e uma lanterna com bateria como recurso.

## Tarefas

1. **Névoa por profundidade (depth cueing da GTE)** em `render.c`:
   - Configure a cor distante (`gte_SetFarColor`) e os coeficientes de
     profundidade (registradores DQA/DQB da GTE) a partir de `fog_near` e
     `fog_far` (unidades de mundo).
   - Use as variantes com depth cue nas faces iluminadas (`gte_ncds`/`gte_ncdt`
     ou interpolação equivalente) e aplique também nas faces sem luz e
     texturizadas (cor modulada pela profundidade).
   - Faces além de `fog_far` não são desenhadas (economia de polígonos);
     `DRAW_DIST` passa a acompanhar `fog_far`.
   - Parâmetros por fase em `LEVEL_DEF` (cor da névoa, near, far, luz ambiente).
2. **Escuridão**: luz ambiente baixa, luz direcional fraca e azulada (lua);
   céu escuro. Valores ajustáveis em `config.h` e por fase.
3. **Lanterna** (jogador, liga/desliga com um botão livre — proponha qual):
   - Visual barato e autêntico: **cone de luz no chão** (polígono
     semitransparente aditivo à frente do jogador, com textura de gradiente)
     + aumento local de `fog_far` quando ligada (vê-se mais longe no cone).
   - Opcional, se o orçamento permitir: clarear vértices do chão dentro do
     cone (meça o custo antes de manter).
   - **Bateria**: campo `battery` no `PLAYER` (0–1000), gasta ligada, item de
     pilha recarrega; quando acaba, a lanterna pisca e apaga. HUD com barra.
4. **Teste A/B**: L2 mostra fog near/far; um atalho de depuração (só em build
   de debug) para ajustar near/far ao vivo.

## Fora do escopo

Mapa grande, sombras dinâmicas, luz pontual real por pixel.

## Critérios de aceite

- `./dev build` limpo; ~30 FPS no pior ponto (anote no HISTORICO o FPS e os
  polígonos antes/depois).
- O fundo desaparece suavemente no preto; com a lanterna se enxerga melhor à frente.
- Bateria acaba, pisca e apaga; pilha recarrega.
- 2 jogadores: cada um com sua lanterna.

## Ao terminar

Peça ao Marcos para comparar com capturas antigas e sugerir ajustes de near/far.
