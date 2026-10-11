# Histórico do projeto e decisões

Resumo da conversa em que o projeto foi criado (06/10/2026), para dar
contexto a quem continuar — pessoa ou agente de IA. Ordem cronológica.

## Objetivo do dono

Marcos quer um **projeto-base de jogo 3D de PS1, de uma fase**, para estudar
mexendo: aplicar modelos próprios feitos no Blender, mapas, poderes, armas,
customizações, e ver tudo funcionando como um jogo de PS1 em emulador ou
console real (CD-R).

## Linha do tempo

1. **Base do jogo (v1).** Escolha do **PSn00bSDK** (livre, moderno, CMake)
   em vez do SDK oficial da Sony (proprietário). Arena 3D com câmera em
   terceira pessoa, inimigos, 4 armas, 3 poderes, gemas, caixas, skins.
   Mapa definido em texto (`level.c`), jogo orientado a dados (`data.c`).
   Exportador do Blender escrito em Python (`tools/blender_export_psx.py`),
   que funciona como biblioteca, operador de menu e CLI.
2. **Validação.** Compilado com o GCC `mipsel-linux-gnu` do Ubuntu. Descoberto
   que esse GCC gera `PT_GNU_STACK` e o `elf2x` produzia um .exe de 2 GB →
   criado `tools/fix_gnu_stack.py`. Testado no **Mednafen** com **OpenBIOS**
   (compilado do repositório `pcsx-redux/nugget`, modo `psexe` com o .exe
   embutido), com capturas de tela automáticas via Xvfb + xdotool.
3. **Ajustes após testes:** câmera colidindo com paredes (aproxima e sobe),
   build `Release` (`-O2`), descarte por frustum, **passo fixo de 60 Hz**
   (o jogo ficava em câmera lenta quando caía para 30 FPS).
4. **Ambiente do Marcos (v2).** Windows + WSL Ubuntu + Docker. Criado o script
   `./dev` (build no Docker com a pasta montada no mesmo caminho — erros
   clicáveis no VS Code), `dev.conf`, configuração do VS Code, e o **pipeline
   automático de assets**: PNG em `assets/textures/` vira TIM com alocação de
   VRAM (`tools/build_textures.py`); `models/*.h` e texturas são declarados em
   `build/gen/assets_gen.h` sem editar o CMake. `./dev models` exporta
   `.blend` em lote usando o Blender do Windows (`tools/export_blend.py`).
   Criado `assets/blender/exemplos.blend` com todos os modelos na coleção PSX.
   Objetos de cenário por tabela (`prop_defs` + dígitos no mapa).
5. **Primeiro uso real.** Build funcionou no WSL do Marcos. O emulador ainda
   não estava instalado; orientado a usar o PCSX-Redux (não precisa de BIOS).
   Observação: a pasta do usuário no Windows é `C:\Users\Marcos  Nobre`
   (**dois espaços**); usar aspas nos caminhos.
6. **Personagens, colisão e 2 jogadores (v3).** Tela de **seleção de
   personagem** (Robô com skins, Cavaleiro, Batedor; atributos em
   `character_defs`). **Colisão por círculos** entre jogadores, inimigos e
   cenário (`collision.c`), com regra de "só bloqueia se aproxima" para nunca
   travar e "pulo passa por cima". **2 jogadores locais** (porta 2), câmera
   que enquadra o grupo, respawn cooperativo.
7. **Online.** Pesquisado: o FAQ de netplay do libretro diz que PS1 não
   funciona; o Batocera não lista núcleos de PS1. Solução documentada:
   **Parsec** (streaming; o convidado vira o controle 2). Modo link pela
   porta serial (SIO1 do PCSX-Redux por TCP / cabo link) ficou como
   **proposta** no README — não implementado porque não dava para testar.
8. **Documentação para o GitHub.** `README.md` técnico, `docs/GUIA.md` com
   receitas, `.gitattributes` (LF obrigatório para o `dev` rodar no Bash).
   Licença ainda **não definida** (README diz "todos os direitos reservados").

## Decisões e motivos

| Decisão | Motivo |
|---|---|
| PSn00bSDK | livre, mantido, CMake, roda em hardware real |
| GCC do Ubuntu no Docker (não `mipsel-none-elf`) | instalável por `apt`, reprodutível; exige o `fix_gnu_stack.py` |
| Modelos como `.h` embutidos | simples para aprender; sem leitura de CD ainda |
| Mapa em texto | editar fase sem ferramenta; geração automática de geometria e colisão |
| Tabelas em `data.c` | mudar o jogo sem mexer no motor |
| Passo fixo 60 Hz | velocidade de jogo independente do FPS; base para um futuro lockstep |
| Colisão por círculos | barata, sem travar, suficiente para arena |
| Docker montando no mesmo caminho do host | caminhos de erro iguais dentro e fora; VS Code abre com clique |

## Como foi testado (e como testar sem o Windows)

No ambiente onde o projeto foi criado: `mednafen` (apt) + Xvfb + OpenBIOS
compilado com o mesmo GCC MIPS (`make PREFIX=mipsel-linux-gnu
FORMAT=elf32-tradlittlemips BOOT_MODE=psexe EMBED_PSEXE=arena.exe` em
`nugget/openbios`, após trocar `elf32-littlemips` por `elf32-tradlittlemips`
nos Makefiles), copiado como `scph5501.bin` em `~/.mednafen/firmware/`,
rodando `mednafen -force_module psx -psx.bios_sanity 0 build/arena.cue`,
teclas enviadas com `xdotool` e capturas com F9. Útil se alguém quiser
automatizar testes visuais.

## Ideias pendentes (do roteiro de estudo)

- Segunda fase; som (`psxspu` / CD-DA); assets lidos do
  CD (`psxcd`); animação de personagem; recorde no memory card; modo link
  pela serial; CI no GitHub Actions; arquivo `LICENSE`.

## Registro de mudanças

- 2026-10-06 — v1 a v3 e documentação, como descrito acima.
- 2026-10-06 — Inimigo que atira (`ATIRADOR`, letra `A`): `BULLET.owner`,
  tabela `enemy_weapon_defs` separada (para não cair nos itens `W`), colunas
  `arma`/`alcance` em `enemy_defs`, IA com alcance, recuo, linha de visão e
  aviso piscando de 20 passos antes do tiro. Dois `A` no mapa da fase 1.
- 2026-10-06 — Etapa 00 (preparação): `src/rng.c` (xorshift32) com `g.seed`/
  `g.rng` para a lógica e `fx_range` para efeitos; `rand()` da libc removido;
  `game_reset(seed)` semeia antes do `level_load` (o `memset` apagava
  `g.frame`); `DEBUG_FIXED_SEED` em `config.h`; overlay L2 com `SEED` e
  `POLIS`; `CREDITS.md`; seção "Roteiro" e regras de determinismo no `CLAUDE.md`.
- 2026-10-06 — Etapa 01a (efeitos sonoros): `tools/wav2vag.py` (encoder
  SPU-ADPCM em Python puro, `--selftest` com SNR), pipeline
  `assets/sounds/*.wav` → `sfx_<nome>` no CMake, `src/sound.c` (vozes 0–1
  para loops, 2–23 com prioridade de `sound_defs`, som posicional pela
  câmera), sons provisórios por `tools/make_placeholder_sounds.py`, passos
  sincronizados com o balanço do modelo (~341 unidades, não 256), rosnado de
  inimigo fora da tela, vento e grilos em loop, `SPU xxxK/512K` no L2.
  **Correção do manual (R8):** a RAM do SPU gasta **~12,6 KB por segundo a
  22 050 Hz** (16 bytes a cada 28 amostras), não 8 KB/s; os 450 KB do
  orçamento dão ~35 s a 22 kHz ou ~70 s a 11 kHz. Música (CD-DA) fica para a
  etapa 01b. Obs.: o inimigo `ATIRADOR` registrado acima não está no código
  atual (`enemies.c` não atira), então só o tiro do jogador tem som.
- 2026-10-06 — Etapa 02 (fases e objetivos): `LEVEL_DEF` em `src/levels.c`
  (R3 ampliada: texturas, céu, faixa de música sem reprodução, objetivo,
  parâmetro, `max_enemies`, texto); `src/objective.c` com `OBJ_KILL_ALL`,
  `OBJ_REACH_EXIT`, `OBJ_COLLECT`, `OBJ_SURVIVE`; letras novas `X` (saída:
  pilar + anel, abre ao cumprir o objetivo), `Q` (item de missão) e `S`
  (reforço do SURVIVE: a cada 8 s, até `max_enemies` vivos, longe dos
  jogadores e fora da câmera); `STATE_INTRO` (R12) e `STATE_END`;
  progressão leva pontos, vida, energia, armas e poder (caído volta com
  50%); game over recomeça a fase; `DEBUG_START_LEVEL`. 4 fases de teste
  (ARENA, CORREDORES, RELIQUIAS, CERCO). Música CD-DA (01b) adiada para
  depois da etapa 10. Medição L2 pendente (teste do Marcos).
- 2026-10-06 — Etapa 03 (clima e lanterna): névoa por profundidade da GTE
  (DQA/DQB por macro própria, `ncds`/`dpcs`, faces além do `fog_far`
  descartadas, cor da névoa = céu), escuridão por fase (`fog_near/far` e luz
  ambiente em `LEVEL_DEF`, lua azulada em `config.h`), lanterna no Select
  (bateria 0–1000, pisca e apaga, pilha `L` +400 e 10% de drop dos
  inimigos; cone `POLY_G3` aditivo entre `DR_TPAGE`; objetos no cone com
  névoa 1,6× mais longe), overlay `FOG near/far`, `DEBUG_FOG_TUNING`,
  L2 + START pula de fase (o overlay passou a alternar ao soltar o L2).
  Clarear vértices do chão (opcional) não foi feito: o chão é um quad de cor
  única por célula. Desempenho no pior ponto de cada fase (L2):

  | Fase | Antes: FPS / POLIS / RAM GPU | Depois: FPS / POLIS / RAM GPU |
  |---|---|---|
  | 1 ARENA | 60 / 354 / 14 584 | a medir |
  | 2 CORREDORES | 30 / 937 / 35 464 | a medir |
  | 3 RELIQUIAS | 30 / 518 / 20 796 | a medir |
  | 4 CERCO | 30 / 531 / 22 056 | a medir |
- 2026-10-07 — Etapa 04a (grade grande e blocos sob demanda): grade de
  até 128×128 (1 byte por célula: `CELL_*` + `CELL_SOLID`); geometria em
  blocos de 8×8 num cache de 36 — o 3×3 em volta de cada jogador é montado
  no mesmo quadro, o resto do 5×5 do grupo numa fila de 1 bloco/quadro,
  cache centrado no jogador (girar a câmera não muda nada); `render_chunk`
  com tabela única de 405 vértices, luz embutida nas cores e névoa com
  `dpct`/`dpcs` (`POLY_GT4`); variação de cor por vértice e textura girada
  por hash; texturas terra/folhas/raízes/mata (`make_forest_textures.py`);
  mata densa com 3 m; fase 5 `FLORESTA (TESTE)` feita por código (validada
  compilando o gerador no host: tudo alcançável, ≤ 208 quads por bloco);
  L2 + direcional teleporta para 4 pontos fixos; cone da lanterna em
  coordenadas relativas (o mapa passa de 32 767). RAM: 707 KB = 723 750 bytes
  (text 82 KB, data 115 KB, bss 510 KB); antes 532 KB. Medições (L2, lanterna ligada):

  | Ponto (fase 5) | FPS | POLIS | RAM GPU | BLOCOS |
  |---|---|---|---|---|
  | ← início | a medir | | | |
  | → cruzamento | a medir | | | |
  | ↑ canto denso | a medir | | | |
  | ↓ clareira | a medir | | | |
  | fase 2 CORREDORES (pior ponto da etapa 03) | a medir | | | |
- 2026-10-07 — Correção da 04a: blocos desenhados na origem, desencontrados
  da colisão. As macros de carga da GTE leem memória dentro de um `asm` sem
  declarar isso; em `load_translation` o GCC apagou a soma da translação e
  todos os blocos (e o cone da lanterna) iam para a origem do mundo.
  Confirmado no assembly (`ctc2 $5..$7` carregando a translação da câmera
  sem a origem do bloco). Correção: `GTE_BARRIER()` (asm com clobber
  `"memory"`) antes das cargas de dados recém-escritos (`load_translation`,
  cone, `render_set_light`, cor em `render_mesh`) e `R·p` em inteiros no
  lugar do `ApplyMatrixLV`. Dados verificados compilando `level.c` no host:
  nas 5 fases todo quad de chão cai em célula livre e todo topo em célula
  sólida. Novo atalho L2 + Select: marcadores da grade de colisão.
- 2026-10-07 — Etapa 04b (árvores em massa): `tools/make_tree_models.py`
  gera pinheiro (22 faces, 3,5 m), árvore seca (24) e tronco caído (22, 3 m),
  e as imagens planas 32×64 de 4 bits renderizadas dos próprios modelos.
  Árvores fora de `g`, 8 bytes cada, ordenadas por bloco; desenho só no 5×5
  do grupo: modelo até profundidade 1800 (`TREE_LOD_DIST`), imagem plana
  além, nada depois da névoa (`render_billboard`, com `GTE_BARRIER`).
  Colisão provisória: o tronco marca a célula sólida (tronco caído: 3
  células, deitado em X ou Z). Fase 5: moitas de mata viram árvores (5%,
  16% no canto nordeste), fileira na frente da borda, 15 troncos; hash com
  semente (`FOREST_TEST_SEED`; etapa 05 deriva de `g.seed`). Validado no
  host: 883 árvores, tudo alcançável, trilhas e pontos de teleporte livres.
  RAM: 738 KB (755 450 bytes). Medições (L2, lanterna ligada, fase 5):

  | Ponto | FPS mín/méd | POLIS máx | RAM GPU máx | ARVORES modelo/plana |
  |---|---|---|---|---|
  | ← início | 30 / 30 | 506 | 24 828 | 9 / 6 |
  | → cruzamento | 30 / 30 | 431 | 21 108 | 3 / 5 |
  | ↑ canto denso | 30 / 30 | 760 | 32 808 | 27 / 45 |
  | ↓ clareira | 30 / 30 | 523 | 24 444 | 3 / 4 |

  (Medido em 2026-10-10 com `./dev bench`, DuckStation Linux, sem inimigos
  extras; ver a linha "antes da 04c" abaixo.)
- 2026-10-10 — Benchmark automático: `DEBUG_BENCHMARK` (fase 5, semente
  fixa, 4 pontos do teleporte, ~3 s sem e com 8 inimigos extras, linhas
  `BENCH ...` na TTY) e `./dev bench` (build em `build-bench/`, cópia
  portátil do DuckStation Linux, espera `BENCH FIM`, mostra a tabela).
  Determinístico (duas execuções idênticas); ~5 s com velocidade ilimitada.
  Isolamento: o DuckStation ignora `XDG_DATA_HOME` se `~/.local/share/duckstation`
  existir; por isso a cópia portátil. Os primeiros testes rodaram com a
  configuração real e somaram ~5 min ao `playtime.dat` do usuário (o save
  state criado foi apagado; `settings.ini` intacto).
  **Depois da 04b / antes da 04c** (`col` = testes de colisão por passo):

  | Ponto | Inimigos extra/total | FPS mín/méd | POLIS máx | RAM GPU máx | COL máx/méd | Árvores mod/plana |
  |---|---|---|---|---|---|---|
  | início | 0 / 11 | 30 / 30 | 506 | 24 828 | 220 / 204 | 9 / 6 |
  | início | 8 / 19 | 20 / 21 | 692 | 28 732 | 605 / 525 | 9 / 6 |
  | cruzamento | 0 / 11 | 30 / 30 | 431 | 21 108 | 232 / 210 | 3 / 5 |
  | cruzamento | 8 / 19 | 30 / 30 | 619 | 25 056 | 574 / 478 | 3 / 5 |
  | canto denso | 0 / 11 | 30 / 30 | 760 | 32 808 | 242 / 224 | 27 / 45 |
  | canto denso | 5 / 16 | 20 / 20 | 878 | 35 288 | 428 / 384 | 27 / 45 |
  | clareira | 0 / 11 | 30 / 30 | 523 | 24 444 | 242 / 217 | 3 / 4 |
  | clareira | 8 / 19 | 20 / 28 | 708 | 28 336 | 537 / 474 | 3 / 4 |

  Com 8 inimigos perto o FPS cai para 20 em 3 dos 4 pontos, com POLIS baixo
  (< 900): o gargalo é CPU (colisão + IA + desenho), não a GPU. No canto
  denso só 5 dos 8 nasceram (o resto cairia em árvore).
- 2026-10-10 — Etapa 04c (colisão em grade espacial): `collision.c` deixa de
  percorrer todos os objetos — obstáculos fixos (troncos em pé, props) e
  entidades (2 jogadores + 24 inimigos) em listas por célula da grade do mapa
  (32 KB + 16 KB, sem malloc), `collide_track()` em toda posição definida
  direto. Mesmas regras (só bloqueia se aproxima, desliza, pulo passa por
  cima de pessoas). Árvore em pé = círculo do tronco; tronco caído = células
  baixas (`CELL_LOW`): pula-se por cima (jogador e inimigos, `ENEMY_HOP`),
  tiros param, visão passa. Tiros pela grade (menor índice, como antes).
  `collide_line_of_sight()` (DDA em inteiros) pronta para a IA da 07.
  Testes no PC (`collision.c` + `level.c` compilados no host): 27 casos de
  visão, bloqueio, tronco redondo, pulo sobre tronco, tiros e grade, todos
  ok. Corrigido o README, que descrevia `BULLET.owner` e atiradores
  inexistentes. RAM: 799 KB (818 634 bytes); sobram ~430 KB do orçamento de
  1,2 MB. `./dev bench` **depois da 04c** (determinístico):

  | Ponto | Inimigos extra/total | FPS mín/méd | POLIS máx | COL máx/méd (antes) | Grade erros |
  |---|---|---|---|---|---|
  | início | 0 / 11 | 30 / 30 | 506 | 2 / 0 (220 / 204) | 0 |
  | início | 8 / 19 | **30** / 30 (antes 20) | 692 | 57 / 32 (605 / 525) | 0 |
  | cruzamento | 0 / 11 | 30 / 50 | 430 | 6 / 2 (232 / 210) | 0 |
  | cruzamento | 8 / 19 | 30 / 30 | 615 | 81 / 53 (574 / 478) | 0 |
  | canto denso | 0 / 11 | 30 / 30 | 760 | 4 / 2 (242 / 224) | 0 |
  | canto denso | 7 / 18 | 20 / 20 | 925 | 54 / 33 (428 / 384, com 5) | 0 |
  | clareira | 0 / 11 | 30 / 30 | 498 | 10 / 2 (242 / 217) | 0 |
  | clareira | 8 / 19 | **30** / 30 (antes 20) | 682 | 75 / 44 (537 / 474) | 0 |

  A colisão caiu ~10x. O canto denso com inimigos continua em 20 FPS, mas
  agora o gargalo é o desenho (27 árvores-modelo + 45 imagens planas + 7
  inimigos extras; antes nasciam só 5 porque a árvore ocupava a célula
  inteira) — primeira alavanca: `TREE_LOD_DIST`.
- 2026-10-10 — Etapa 04d (câmera na mata): câmera visual sai de `g` para
  `camera.c` (`g.cam_yaw` fica, é lógica); reforço do SURVIVE passa a usar o
  centro do grupo + `g.cam_yaw`. Câmera nunca atravessa troncos
  (`collide_camera_clear`: só troncos em pé e mata alta; começa da
  distância anterior, ~2 testes/quadro; garantia de puxar o olho na hora;
  7 casos testados no PC). Aproxima na mata densa (`CAM_DENSE_DIST` 540).
  Árvores que tapam ficam a 50% por ~0,3 s: os modelos de árvore não têm
  textura, então a mistura vale no polígono todo sem depender do bit STP;
  a imagem plana tem textura e CLUT sem STP (só o quase-preto 0x8000),
  por isso árvore marcada é sempre desenhada como modelo. 2 jogadores: o
  enquadramento vence a aproximação. Bench: pontos `canto_tapado` (50%
  ativo) e `canto_2j`, custo da câmera (`cam_*`), perfil do quadro por
  linhas (timer 1) e por categoria, variantes `./dev bench -D`. RAM: 804 KB
  (823 402 bytes), sobram ~425 KB.

  **Antes da 04d** (= fim da 04c, 4 pontos):

  | Ponto | +inimigos | FPS mín/méd | POLIS máx | COL máx |
  |---|---|---|---|---|
  | início | 0 / 8 | 30/30 / 30/30 | 506 / 692 | 2 / 57 |
  | cruzamento | 0 / 8 | 30/50 / 30/30 | 430 / 615 | 6 / 81 |
  | canto denso | 0 / 7 | 30/30 / 20/20 | 760 / 925 | 4 / 54 |
  | clareira | 0 / 8 | 30/30 / 30/30 | 498 / 682 | 10 / 75 |

  **Primeira medição depois da 04d** (padrões antigos: `TREE_LOD_DIST` 1800,
  névoa 3200, sem limite entre jogadores; CPU em linhas, 30 FPS = ~525):

  | Ponto | +ini | FPS | POLIS | 50% | CPU | espera GPU |
  |---|---|---|---|---|---|---|
  | canto denso | 0 / 7 | 30 / **20** | 789 / 946 | 0 | 461 / **599** | 0 |
  | canto 2 jogadores | 0 / 3 | **15** / **15** | 1823 / 1892 | 1 | 822 / 869 | 0 |
  (os outros pontos ficaram em 30 FPS)

  **CPU ou GPU?** A espera pela GPU é 0 em todos os pontos: o gargalo é a
  **CPU** (no emulador). Canto denso +7 (599 linhas): lógica 98, desenho dos
  blocos 214, árvores 160, resto (inimigos/itens) 102, câmera/HUD 25. No
  canto com 2 jogadores, os blocos sozinhos gastam 585 (câmera recuada e
  alta vê muito mais chão; GPU RAM 93 KB de 96 KB).

  **Varredura** (canto denso com inimigos extras; `./dev bench -D`):

  | Variante | FPS | CPU | árvores modelo/plana |
  |---|---|---|---|
  | `TREE_LOD_DIST` 1800 (atual) | 20 | 599 | 28 / 39 |
  | 1500 | 20 | 576 | 22 / 42 |
  | 1300 | 20 | 557 | 18 / 46 |
  | 1100 | 20 | 554 | 17 / 47 |
  | 900 | 30 | 506 | 15 / 48 |
  | 1300 + névoa 2800 | 30 | 498 | 18 / 45 |
  | 1300 + canto denso 12% (era 16%) | 30 | 494 | 12 / 37 |
  | 1300 + névoa 2600 | 30 | 472 | 18 / 36 (2 jogadores: 20 FPS) |
  | 1500 + névoa 2800 | 20 | 552 | 22 / 41 |

  Padrões **não** alterados nesta etapa (decisão do Marcos).
- 2026-10-10 — Decisão (04d): `TREE_LOD_DIST` 1800 → **1300** e névoa da
  fase 5 3200 → **2800** como padrão; densidade do mapa de teste mantida.
  **Folga pequena:** o canto denso com inimigos fica em ~498 de ~525
  linhas de CPU. A **etapa 10 precisa otimizar a CPU**, principalmente o
  desenho dos blocos (~214 linhas) e das árvores (~160).
- 2026-10-10 — 04d, ajustes finais: **`MAX_PLAYER_SEPARATION` = 896**
  (lógica em `player.c`: o eixo que afastaria além do limite não anda; de
  lado e de volta, livre). Escolha pelo `./dev bench -D` no `canto_2j` com
  inimigos, jogador 2 colocado no limite em 8 direções: 896 dá 30 FPS em
  todas (pior CPU 499, ≥ 62% do buffer de primitivas livre); 960 falha em 1
  direção; valores menores também falham em 1–2 direções por causa da folga
  de CPU do canto denso, não da distância. Antes disso, a câmera ao
  aproximar por tronco ficava quase vertical (até ~1 800 de altura, 15 FPS
  com 2 jogadores): agora desliza na própria linha de visão e sobe no
  máximo `CAM_BLOCK_RAISE` (120). **Buffer de primitivas:** a proteção
  contra estouro já existia, mas cortava jogadores e HUD (desenhados por
  último); agora há reserva de 16 KB para eles, blocos e árvores são
  desenhados do perto para o longe e o cortado é o mais distante (aviso
  `CORTE` no L2). Estresse com buffer de 40 KB: sem travar, máximo 26 KB
  usados. RAM: 805 KB (824 470 bytes), sobram ~423 KB.

  **Depois da 04d — tabela final** (padrões: `TREE_LOD_DIST` 1300, névoa da
  fase 5 2800, separação 896):

  | Ponto | +ini | FPS mín/méd | POLIS máx | RAM GPU máx | CPU | lógica | 50% | corte |
  |---|---|---|---|---|---|---|---|---|
  | início | 0 / 8 | 30/30 / 30/30 | 391 / 577 | 19 956 / 23 860 | 274 / 380 | 34 / 65 | 0 | 0 |
  | cruzamento | 0 / 8 | 60/60 / 30/30 | 384 / 569 | 18 880 / 22 764 | 232 / 368 | 18 / 73 | 0 | 0 |
  | canto denso | 0 / 7 | 30/30 / **30/30** | 610 / 769 | 27 228 / 30 572 | 410 / **504** | 35 / 64 | 0 | 0 |
  | clareira | 0 / 8 | 30/30 / 30/30 | 410 / 593 | 19 572 / 23 420 | 283 / 394 | 38 / 75 | 0 | 0 |
  | canto tapado | 0 / 3 | 30/30 / 30/30 | 463 / 533 | 21 692 / 23 160 | 335 / 373 | 35 / 45 | 1 | 0 |
  | canto 2 jogadores (sep. 859) | 0 / 3 | **30/30** / **30/30** | 623 / 696 | 27 172 / 28 704 | 411 / 446 | 37 / 44 | 1 | 0 |

  Atenção: o canto denso com inimigos fica em 504 de ~525 linhas. Perto do
  limite o jogo "trava" em 20 FPS (um quadro longo faz o próximo rodar 3
  passos de lógica em vez de 2); a **etapa 10 precisa abrir folga na CPU**
  (blocos ~203, árvores ~118 linhas).
- 2026-10-10 — 04d, conferência antes do merge: a separação de 941 (> 896)
  na tabela era o **bench furando a regra** (colocava o jogador 2 direto,
  num círculo exato, e a regra mede com `dist2d`, que dá até ~6% a mais);
  corrigido: o bench usa a mesma medida (agora 859). No jogo normal todo
  movimento passa por `move()` → `keep_together()`, então quem está dentro
  do limite não sai. O limite só vale entre jogadores vivos e ativos
  (`player_alive`); o começo da fase (`free_spot_near`, ≤ ~362) e o
  renascimento (ao lado do parceiro, ~283) já ficam dentro do limite.
- 2026-10-10 — Etapa 04e (integração): a floresta substitui a arena. As 4
  fases da etapa 02 viram florestas com `build_forest` (gerador da fase 5
  parametrizado: tamanho, semente fixa por fase, densidade, trilhas,
  clareiras, troncos caídos) e a névoa/luz da etapa 03: CACADA (64×64, 12
  inimigos), TRAVESSIA (128×128, oeste → leste), RELIQUIAS (64×64, 4 Q +
  saída), CERCO (64×64, 4 pontos S em células livres na borda da clareira).
  Tudo colocado com `level_place_safe` (célula livre mais próxima até 6;
  senão não coloca). Teste no PC: 0 falhas, tudo alcançável do início
  (tronco caído passável), nada na mata, nas 5 fases; fase 5 idêntica.
  Fase 5 fora da sequência (`num_levels` = 4). Título com câmera mais baixa
  (a floresta vista de cima era lenta). Removidos (nada mais usava): mapas
  de texto, `load_text_map`, `CELL_FLOOR`/`CELL_WALL`, campos
  `map`/`floor`/`wall`, `floor.png`/`wall.png` → **RAM −17 300 bytes, VRAM
  −16 KB**. RAM total: **790 KB** (808 586 bytes); sobram ~438 KB do
  orçamento de 1,2 MB. Bench: pior momento de cada fase.

  **Antes** (fim da 04d): só os 6 pontos da fase 5, todos a 30 FPS
  (canto denso com inimigos: 504 linhas).

  **Depois** (CPU em linhas, 30 FPS = ~525):

  | Ponto | Inimigos (natural / pior) | FPS | POLIS máx | CPU | lógica |
  |---|---|---|---|---|---|
  | fase 5 (6 pontos) | — | 30 em todos | — | ≤ 504 | — |
  | f1_pior (CACADA) | 12 / 12 juntos | 30 / 30 | 564 / 774 | 335 / 436 | 41 / 60 |
  | f2_pior (TRAVESSIA) | 14 / 14 juntos | 30 / 30 | 452 / 753 | 303 / 446 | 45 / 70 |
  | f3_pior (RELIQUIAS) | 8 / 8 juntos | 30 / 30 | 447 / 587 | 291 / 354 | 27 / 35 |
  | f4_pior (CERCO) | 3 / 11 (+8 reforços) | 30 / 30 | 527 / 710 | 277 / 390 | 15 / 55 |
  | f4_pior_2j (CERCO, 2 jogadores) | 4 / 12 | 30 / **20** | 759 / 941 | 401 / **544** | 20 / 85 |

  (tabela acima: névoa da fase 4 ainda em 3 400)

  **Abaixo de 30 FPS:** só o CERCO com 2 jogadores e os 8 reforços (544:
  blocos 225 — névoa 3 400, a maior das fases —, resto 187 = 12 inimigos +
  2 jogadores/cones/sombras). Opções medidas (`./dev bench -D`), todas a 30
  FPS: névoa da fase 4 3 000 → 478; 2 800 → 470; reforços 6 → 485; névoa
  3 000 + 6 reforços → 452; `MAX_PLAYER_SEPARATION` 768 → 489. **Padrões
  não alterados** (decisão do Marcos).
- 2026-10-10 — Decisão (04e): névoa da fase 4 (CERCO) 3 400 → **3 000**;
  mantidos 8 reforços e `MAX_PLAYER_SEPARATION` 896. Bench só das fases 1–4
  (`./dev bench -D BENCH_FROM=6`). **Depois — tabela final** (CPU em linhas,
  30 FPS = ~525):

  | Ponto | Inimigos (natural / pior) | FPS | POLIS máx | CPU | lógica |
  |---|---|---|---|---|---|
  | f1_pior (CACADA) | 12 / 12 juntos | 30 / 30 | 564 / 774 | 335 / 435 | 41 / 59 |
  | f2_pior (TRAVESSIA) | 14 / 14 juntos | 30 / 30 | 452 / 753 | 303 / 446 | 45 / 70 |
  | f3_pior (RELIQUIAS) | 8 / 8 juntos | 30 / 30 | 447 / 587 | 291 / 354 | 27 / 35 |
  | f4_pior (CERCO) | 3 / 11 (+8 reforços) | 60 / 30 | 470 / 654 | 247 / 375 | 7 / 56 |
  | f4_pior_2j (CERCO, 2 jogadores) | 4 / 12 | 30 / 30 | 669 / 851 | 360 / **480** | 18 / 60 |

  Todas as fases a 30 FPS no pior momento. **A folga de CPU continua sendo
  a prioridade da etapa 10**: canto denso da fase 5 em 504 e CERCO com 2
  jogadores em 480 de ~525 linhas (o maior custo segue sendo o desenho dos
  blocos e das árvores).
- 2026-10-11 — Etapa 05 (geração procedural): `src/mapgen.c` gera as
  florestas das fases 1–4 a cada partida a partir de `g.seed` (semente da
  fase = `g.seed ^ ((fase+1) * 0x9E3779B1)`, RNG local; `g.rng` intacto).
  Ruído de valor inteiro para a densidade, trilhas por passeio aleatório,
  clareiras, troncos, `P/X/Q/S`, inimigos por distância do início e itens
  pelos orçamentos da 04e (`GEN_PARAMS` em `levels.c`). Validação por BFS
  (tudo alcançável, tronco caído passável, nada em sólido); inválido →
  semente + 1 até `MAPGEN_TRIES` (16); senão **plano B** = floresta fixa da
  04e. Fase 5 continua fixa. Game over recomeça com o **mesmo mapa**; vencer
  sorteia semente nova. Overlay L2: `MAPA <semente> T<tentativas> CARGA <ms>`
  ou `MAPA PLANO B (FIXO)`. `./dev mapgen-test [N] [--bench] [-D]` testa no PC.

  **Teste no PC, 1000 sementes por fase:** 0 inválidos (plano B 0 vezes),
  0 problemas na checagem independente, 200 sementes geradas 2× com o mesmo
  hash da grade (determinístico). Tentativas: f1 973×1 + 27×2; f2 988×1 +
  12×2; f3 991×1 + 8×2 + 1×3; f4 988×1 + 12×2. Densidade (árvores por mil
  células) mín/méd/máx: f1 15/20/24, f2 24/26/29, f3 12/14/17, f4 12/12/14.
  Maior bloco: 5 árvores. Tempo no PC: méd 0,25 ms (64×64), 0,92 ms
  (128×128), máx 2,3 ms.

  **Tetos escolhidos pelo bench.** Medido com as 5 sementes de pior janela
  3×3 (árvores+mata) e as 5 com mais árvores na janela, câmera e todos os
  inimigos no centro dela. Achado: **árvore perto da câmera é o que pesa**
  (modelo completo + raízes no chão sobem `desenho_blocos` e
  `desenho_arvores`); mata quase não pesa. Com teto só de "peso" (40) as
  janelas de árvores chegavam a f1 498, f2 496, f3 397, f4 416. Ficou: teto
  de **árvores por janela 3×3** por fase (`GEN_TREES_F1..F4` = 16/20/12/10)
  e **5 árvores por bloco 8×8** (`GEN_CHUNK_CAP`). Também medida a floresta
  **fixa da 04e no seu próprio canto mais denso** (mesmo critério;
  `f{N}_fixo_*` no bench): f1 450, f2 494, f3 389, f4 415 — o bench da 04e
  não passava por esses cantos.

  **Conflito registrado (f4 e densidade):** o limite estrito da f4 (375 +
  5% = 394) não foi alcançável: mesmo com 8 árvores por janela (abaixo de
  2%) o canto de árvores deu 406. Ficou em 10 árvores → **415**, igual ao
  canto mais denso da própria floresta fixa da 04e (415) e longe de 504.
  Para caber nos limites, f3 e f4 ficaram com densidade média de 1,2–1,7%
  (abaixo da faixa pedida de 2–9%); f1 1,5–2,4%, f2 2,4–2,9%.

  **Antes** (04e, florestas fixas, pior ponto medido na 04e) e **depois**
  (05, pior entre 10 sementes por fase), CPU em linhas (30 FPS = ~525),
  natural / todos os inimigos juntos:

  | Fase | Limite (04e +5%) | Antes: CPU | Depois: CPU (ponto) | Depois: POLIS máx | FPS | Fixa 04e no canto mais denso |
  |---|---|---|---|---|---|---|
  | f1 CACADA | 458 | 335 / 435 | 375 / **450** (arv5) | 785 | 30 | 450 |
  | f2 TRAVESSIA | 468 | 303 / 446 | 320 / **465** (arv1) | 772 | 30 | 494 |
  | f3 RELIQUIAS | 372 | 291 / 354 | 338 / **369** (arv2) | 651 | 30 | 389 |
  | f4 CERCO | 394 | 247 / 375 | 302 / **415** (arv3) | 743 | 30 | 415 |
  | f4 2 jogadores | 504 | 360 / 480 | 304 / **415** (pior1) | 691 | 30 | — |
  | fase 5 (fixa) | — | canto denso 504 | 504 (igual) | 769 | 30 | — |

  **Carga no console** (DuckStation, `BENCH carga`): antes f1 160, f2 379,
  f3 140, f4 145, f5 416 ms; depois (gerada) 64×64 232–259 ms, 128×128
  749–766 ms; plano B igual à 04e. Abaixo de 1 s: geração **não** dividida
  entre quadros; `start_level` desenha um quadro `CARREGANDO...` (só
  desenho, não muda `g`) que fica na tela enquanto a CPU gera.

  **RAM:** 808 586 → 820 066 bytes (**+11 480**, ~11,2 KB: código do
  gerador e ~1,2 KB de tabelas). O BFS (marcas 16 KB + fila 32 KB) usa a
  memória do cache de blocos, livre durante a carga (`level_scratch`):
  0 byte novo. Total ~801 KB de 1,2 MB.

  **Modo link (anotação para a etapa 11, não implementado):** a semente vem
  de `g.frame` na seleção, diferente em cada console. No modo link os dois
  consoles precisam combinar a mesma semente na conexão (um envia, o outro
  usa); o gerador já é determinístico dado `g.seed`.
- 2026-10-11 — Etapa 05, mata alta (pedido do Marcos: "a floresta ficou rala
  para um terror"). Critério novo, que **substitui** o limite por fase da 04e +
  5%: 0 inválidos em 1000 sementes, tudo alcançável e o bench das piores
  sementes ≤ 504 linhas a 30 FPS. Os tetos de árvores não mudaram
  (16/20/12/10 por janela, 5 por bloco). Gerador: manchas de mata
  (`blob_noise`, oitavas de 16 e 8 células) + paredes coladas na faixa das
  trilhas (limiar 40 mais baixo: corredores), 2 passes de suavização
  (autômato celular, só bits de `gmark`), `GEN_THICKET_F1..F4` = 150 e
  `GEN_WALLS` = 40. Ruído com shifts em vez de divisões (o MIPS do PS1 leva
  ~36 ciclos por divisão) e inundação com `walkable()` calculado uma vez por
  célula: a carga da fase 2 ficou em 0,9 s mesmo com mais trabalho.

  **Achados do bench:** (1) encher os bolsões fechados com mata criava
  tufos soltos entre as árvores (3–4 lados expostos cada, até 16 quads por
  célula): a fase 2 caía para 20 FPS até com 4% de mata. Agora os bolsões
  ficam como estão e itens/inimigos só nascem em células alcançáveis
  (`GM_REACH`). (2) Teto de "contorno por janela 3x3" testado e descartado:
  as laterais aparecem até a névoa, bem além da janela; não convergia e
  deixava a carga lenta. (3) O canto denso da fase 5 está no limite (504):
  um contador no laço de desenho bastou para cair a 20 FPS, e a lógica sobe
  1,5× (3 passos por quadro). `./dev mapgen-test` agora para se a
  compilação falhar (antes rodava o binário velho em silêncio).

  **Densidade (1000 sementes por fase), antes → depois:**

  | Fase | Árvores ‰ mín/méd/máx | Mata alta no interior ‰ mín/méd/máx | Tentativas máx |
  |---|---|---|---|
  | f1 CACADA | 15/20/24 → 11/18/23 | 0/0/0 → 6/200/592 | 2 → 3 |
  | f2 TRAVESSIA | 24/26/29 → 18/23/27 | 0/0/0 → 88/224/509 | 2 → 2 |
  | f3 RELIQUIAS | 12/14/17 → 10/13/17 | 0/0/0 → 5/167/458 | 3 → 3 |
  | f4 CERCO | 12/12/14 → 11/12/14 | 0/0/0 → 0/213/578 | 2 → 3 |

  (árvores caem um pouco porque a mata ocupa células; os tetos são os mesmos)

  **Bench, piores sementes (CPU em linhas, todos os inimigos juntos):**

  | Ponto | Antes | Depois | FPS depois |
  |---|---|---|---|
  | f1 (pior de 10 sementes) | 450 | 470 | 30 |
  | f2 | 465 | 494 | 30 |
  | f3 | 369 | 400 | 30 |
  | f4 | 415 | 452 | 30 |
  | f4 2 jogadores | 415 | 423 | 30 |
  | fase 5 canto denso (fixa) | 504 | 504 | 30 |

  **Carga no console:** 64×64 232–259 → 276–335 ms; 128×128 749–766 →
  883–929 ms (< 1 s; quadro `CARREGANDO...`). **RAM:** 820 066 → 823 122
  bytes (+3 056).
- 2026-10-11 — Etapa 06 (eventos: a floresta muda). `src/events.c`: tabela
  `EVENT_DEF` por fase (gatilho → ação: aleatório a cada 45–90 s, tempo,
  área, N-ésimo item, N mortes, chave), fila com prioridade, estado todo em
  `g.ev`; nada nos primeiros 30 s e no máximo 1 evento que muda o mapa a
  cada 30 s (a chave abre na hora). Ações: **névoa avança** (volta em ~40 s;
  `spawn_ok` passa a ler `g.ev.fog_far`), **árvore cai** (pinheiro à frente
  do grupo; estalo, tombo, tremor; tronco pulável, copa sólida, só BITS das
  células — nenhum bloco remontado; copa no próprio modelo deitado
  `TREE_FALLEN`), **mata fecha atrás** (pinheiros novos fora da visão, nos
  tetos da 05), **onda** (repõe inimigos até os do começo da fase),
  **silêncio** (vento e grilos param 12 s), **caminho se abre** (o gerador
  põe um anel de raízes `CELL_BARRIER` em volta de uma clareira com prêmio
  — 94/99/65/0% dos mapas das fases 1–4 — e uma chave `K`; abrir remonta o
  bloco em fatias). Sons sintetizados (`tools/make_event_sounds.py`).
  Atalhos: overlay L2 aberto + L2 + □/○/×/L1/R1/R2; L2 + → vai para a
  frente da barreira.

  **Alcance:** `src/reach.c` — 1 bit por célula (da inundação do gerador,
  também nas florestas fixas) e checagem LOCAL antes de bloquear células
  (janela 33×33, mesma regra de andável da 05: tronco caído passa, árvore
  nova bloqueia). Retomável (40 visitas por passo, teto 600, determinístico).

  **Teste no PC** (`./dev events-test 1000`; 12 ações por partida, jogador
  e câmera sorteados, BFS completo depois de cada uma): **0 objetivos
  inalcançáveis, ninguém preso (jogador/inimigo/item), determinístico**.
  Checagens locais por fase (ok / corta / **janela** / teto de visitas):
  f1 13 011 / 432 / **25** / 51; f2 9 253 / 250 / **6** / 85; f3 16 348 /
  412 / **12** / 65; f4 16 754 / 341 / **25** / 54 — cancelados pela
  janela: 68 de ~57 600 (0,12%); pelo teto: 255 (0,44%). Aconteceram
  (pedidos forçados): árvore cai 57–82%, mata fecha 52–96% (fase 2 menos:
  janelas perto do teto de árvores), caminho abre 92/99/68/0%.

  **Passo 0 (medida):** remontar um bloco inteiro custa 62–70 linhas nas
  florestas fixas e 100–187 nas geradas (contorno da mata alta); no quadro
  da remontagem os piores pontos iam a 530–604 (20 FPS). Em fatias, com
  orçamento adaptativo (folga do quadro anterior até 500, máx. 12 linhas):
  +17 linhas no pior caso. Achado: andar pela floresta já remonta 1 bloco
  por quadro (streaming) com o mesmo custo — candidato a usar as fatias na
  etapa 10.

  **Bench do evento** (cada ponto: controle de 180 quadros sem evento, e
  depois cada evento; pior quadro, CPU em linhas):

  | Evento | Pior quadro (pontos válidos) | Pior acréscimo sobre o controle | FPS |
  |---|---|---|---|
  | árvore cai | 512 | +39 | 30 |
  | mata fecha | 513 | +23 | 30 |
  | caminho abre | 505 | +24 | 30 |
  | onda | 499 | +15 | 30 |

  Quadros normais (multidão, 3 s): média ≤ 497, máximo 504. **Achado:**
  3 pontos (canto denso da fase 5, f2_arv3, f2_fixo_arv) já passam de 515
  e caem a 20 FPS **sem evento nenhum** depois de ~3 s de multidão parada
  (o bench antigo media só 3 s); fica para a folga de CPU da etapa 10.
  Caminho até aqui: a busca fazia até 40 pegadas e uma checagem inteira
  por passo (lógica a 234–425 linhas); o tremor movia a câmera (+100
  polígonos) e depois a saída de vídeo (perdia um quadro) — ficou o offset
  de desenho (`DRAWENV.ofs`), custo zero.

  **RAM:** 823 122 → 875 218 bytes (**+52 096**: código ~19 KB, 3 sons
  ~17 KB — também ~17 KB de SPU —, buffer da remontagem em fatias 7,7 KB,
  alcance + janela ~7,6 KB). Total ~855 KB de 1,2 MB.
- 2026-10-11 — Etapa 10a (folga de CPU, antes da 07). **Medição** (bench:
  multidão parada por 10 s com série por segundo; perfil de lógica dos
  inimigos/jogadores e desenho dos inimigos; pontos `f{N}_andar` que
  percorrem o caminho do início até a saída): **nada cresce com o tempo**.
  A queda a 20 FPS dos 3 pontos era **biestável**: em ~500 linhas, um único
  quadro > 525 (remontagem, printf) faz rodar 3 passos de lógica (+35 linhas)
  e o jogo não volta mais. Andar montava blocos inteiros de até 193 linhas
  num quadro (f3 528, 20 FPS). Nos piores pontos (f2, ~490): blocos ~205,
  desenho dos inimigos ~133 (sombras 33), lógica dos inimigos ~70 (colisão
  ~56), árvores ~45.

  **`./dev replay-test`** (nova ferramenta): partida roteirizada de 60 s por
  fase, hash de `g` + mapa a cada 10 s. Conferida: desenho mais pesado dá
  IGUAL (não depende do FPS), lógica mudada dá DIFERENTE. Achou um bug: a
  busca da árvore que cai copiava lixo da pilha para `g.ev.fall` (corrigido;
  referência regravada, comportamento igual).

  **Otimizações** (todas com replay IGUAL): anel externo de blocos montado
  em fatias pelo construtor único (teto adaptativo 440); sub-blocos 4×4
  descartados fora da tela (conferido: 0 quads visíveis pulados); colisão
  com leitura direta da grade e da entidade (lógica 67 → 54); sombra sem
  rotação direto da vista e sombra simples (1 quad) além de 800 da câmera
  (visual, aprovado); matriz direto de `isin/icos` para giro só no eixo
  vertical (idêntica ao `RotMatrix` em 107 mil conferências). **Medido e
  descartado:** pré-descarte de faces de costas em C (+15..18 linhas: a GTE
  é mais rápida).

  **Antes → depois** (pior quadro, CPU em linhas; 30 FPS ≈ 525):

  | Fase | Medida | Antes | Depois |
  |---|---|---|---|
  | f1 | multidão 10 s / andar / eventos | 478 / 509 / 496 | 409 / 333 / 433 |
  | f2 | multidão 10 s / andar / eventos | 505 / 459 / 572 (20 FPS) | 450 / 307 / 471 |
  | f2 | remontagem / controle | 539 (20) / 540 (20) | 446 / 445 |
  | f3 | multidão 10 s / andar / eventos | 417 / 528 (20) / 430 | 366 / 325 / 399 |
  | f4 | multidão 10 s / andar / eventos | 442 / 420 / 447 | 374 / 292 / 391 |
  | f4 | 2 jogadores | 428 | 375 |
  | fase 5 | multidão / eventos / remontagem | 512 / 606 (20) / 553 (20) | 461 / 478 / 462 |

  Meta cumprida: fases 1–4 ≤ 450 nos quadros normais e ≤ 480 nos de evento,
  sempre 30 FPS; fase 5 a 30 FPS. **Proposta medida, não aplicada:**
  `MAX_TICKS_PER_FRAME` 4 → 2. Sob sobrecarga artificial
  (`TREE_LOD_DIST=3000`), tira o f2_fixo_arv da armadilha (20 → 30 FPS, tempo
  real); quando nem 2 passos cabem (canto denso), o jogo vai a 67% da
  velocidade (câmera lenta) em vez de 20 FPS em tempo real. No modo link:
  continua determinístico (lógica por passo), mas o "tempo" do jogo deixa de
  acompanhar o relógio quando um console sobrecarrega (no lockstep, o mais
  lento já dita o ritmo). RAM: 875 218 → 877 874 bytes (+2 656).
- 2026-10-11 — Decisão (10a): **`MAX_TICKS_PER_FRAME` = 2** (antes 4). Por
  quê: com 4, um único quadro acima de ~525 linhas fazia rodar 3 passos de
  lógica no seguinte (+35 linhas) e o jogo podia ficar preso em 20 FPS
  (biestável). Com 2, ele volta a 30 FPS sozinho; se a cena não couber nem
  com 2 passos, o jogo fica em **câmera lenta** (ex.: 67% da velocidade a
  20 FPS) em vez de travar em 20 FPS em tempo real. Timers, sobrevivência e
  eventos contam em passos, então atrasam junto. `./dev replay-test`: IGUAL
  (a lógica é por passo). Bench dos 14 pontos mais pesados: iguais aos de
  antes (±5 linhas), todos a 30 FPS, pior 460 (fase 5) e 450 (f2_fixo_arv).
