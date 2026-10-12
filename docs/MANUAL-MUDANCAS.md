# O que mudou em relação ao manual (PDF)

O `docs/MANUAL-DESENVOLVEDOR.pdf` foi escrito no começo do projeto (arena).
O jogo virou o **MATA ESCURA** (floresta, etapas 00–10). Esta lista junta as
diferenças **registradas no `docs/HISTORICO.md`** ao longo das etapas; o PDF
não foi editado. (Observação: o texto do PDF não pôde ser extraído no
ambiente do agente, então a lista vem das anotações feitas quando cada
diferença foi encontrada.)

## Correções (o manual está errado)

| Receita / capítulo | O manual diz | O certo (medido) | Onde |
|---|---|---|---|
| R8 (som no SPU) | ~8 KB por segundo de som | **~12,6 KB/s a 22 kHz** (16 bytes a cada 28 amostras); os 450 KB dão ~35 s a 22 kHz ou ~70 s a 11 kHz | etapa 01a |

## O que o jogo faz diferente do manual

| Tema | No manual | No jogo hoje | Etapa |
|---|---|---|---|
| Mapa | arena em texto (`level.c`) | grade 128×128 gerada pela semente (`mapgen.c`), blocos 8×8 montados sob demanda; texto só no plano B e na fase de teste | 04, 05 |
| Fases (R3) | tabela simples | `LEVEL_DEF` com gerador, céu/névoa, objetivo, eventos | 02, 05, 06 |
| Estados (R12) | título, jogo, pausa | + introdução, fim, **menu do START** (o jogo para de verdade, `g.frame` não anda), créditos | 02, 08, 10 |
| Pausa | só para | **menu** com mapa, objetivo, estatísticas, armas, itens, opções | 08, 10 |
| Poderes | onda de choque, dash, cura | viraram **magias** (armas do tipo `WK_MAGIC`): bola de fogo, clarão, cura; gastam mana | 07 |
| Armas | 4 armas de tiro com mira | espada, machado, arco (segurar carrega), pistola, espingarda; mira automática só nas de fogo | 07 |
| Inimigos | perseguem sempre | percepção: visão (escuro/lanterna/linha de visão, a cada 8 passos) e audição (barulhos) | 07 |
| Colisão | círculos contra listas | **grade espacial** (`collision.c`): obstáculos e entidades por célula | 04c |
| Câmera | lógica | `g.cam_yaw` é lógica; posição/suavização são **visuais** (fora de `g`) | 04d |
| Passos por quadro | até 4 | **`MAX_TICKS_PER_FRAME` = 2** (sob sobrecarga, câmera lenta em vez de travar em 20 FPS) | 10a |
| Aleatório | `rand()` | `g.rng` (lógica, semente) e `fx_range` (visual); determinismo para o modo link | 00 |
| Memory card (R10) | receita | salva o **começo da fase** (o mapa volta pela semente), com versão e checksum; testado por `./dev save-test` | 10 |
| Testes | teste no emulador | `replay-test`, `robo-test`, `save-test`, `maratona`, `bench`, `capturas` (sem abrir janela) | 07b–10 |
| Emulador | PCSX-Redux / DuckStation no Windows | **DuckStation Linux** (cópia portátil para testes; a sua pasta só é lida) | 07b |
| Modo link (R11) | proposta | ainda proposta (etapa 11); o save fica desligado nele | — |

## Armadilhas que o manual não tinha

- `PT_GNU_STACK` do GCC do Ubuntu (`tools/fix_gnu_stack.py`).
- Macros de carga da GTE leem memória sem avisar o GCC: `GTE_BARRIER()` (04a).
- Posição de mundo em 16 bits estoura no mapa grande: `VECTOR` ou origem relativa (04).
- Textura com número ímpar de unidades de 16 bits na VRAM trava o `DrawSync` (10).
- `open()` do memory card logo no boot trava com o cartão recém-ligado (10).
- O DuckStation corta linhas longas do TTY: registros em linhas curtas (07b).
