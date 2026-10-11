# PS1 Arena — instruções para o agente

Jogo 3D de PlayStation 1 em C (PSn00bSDK 0.24), projeto de **estudo**. O dono
(Marcos) é dev back-end sênior (PHP/Go/Node), novo em PS1, C de console e
Blender. Converse em **português do Brasil**, explique o "porquê" das
mudanças e mantenha o código didático.

Contexto completo do que já foi feito e por quê: `docs/HISTORICO.md` (leia
antes de mudanças grandes). Referência técnica: `README.md`. Receitas de
modificação: `docs/GUIA.md`.

## Ambiente

- Windows + **WSL2 Ubuntu**; o projeto fica em `~/psx-arena` (disco do Linux).
- Compilação dentro do **Docker** (imagem `psx-arena-sdk`, ver `Dockerfile`):
  Ubuntu 24.04, GCC `mipsel-linux-gnu` 12.4, CMake 3.28, Python 3 + Pillow,
  PSn00bSDK compilado de um commit fixo.
- Emulador no Windows (PCSX-Redux sem BIOS; DuckStation com BIOS),
  configurado em `dev.conf` (pessoal, fora do git).
- Blender 5.2 no Windows (`/mnt/c/Program Files/Blender Foundation/...`).

## Comandos

```bash
./dev build          # compila (Docker). Erros aparecem com caminho real do arquivo
./dev run            # compila e abre no emulador do Windows
./dev models         # exporta assets/blender/*.blend -> models/*.h (Blender do Windows)
./dev vram           # mapa das texturas na VRAM
./dev bench          # benchmark automático (FPS/POLIS/COL/CPU nos pontos da fase 5)
./dev bench -D TREE_LOD_DIST=1300   # variante sem mexer no config.h
./dev mapgen-test 1000 [--bench] [-D N=V]   # gerador no PC (N sementes/fase); --bench grava src/bench_seeds.h
./dev events-test 1000                       # eventos simulados no PC (alcance, ninguém preso, determinismo)
./dev replay-test                            # partida roteirizada: hash de g igual à referência?
./dev clean          # apaga build/
VERBOSE=1 ./dev build
```

- **Sempre rode `./dev build` depois de editar** e corrija erros/avisos novos.
- **Medição de desempenho: use `./dev bench`** (você mesmo roda; ~5 s). Ele
  compila com `DEBUG_BENCHMARK` em `build-bench/` (o build normal nunca liga o
  modo), roda uma cópia portátil do DuckStation Linux (a pasta do usuário não
  é tocada) e imprime a tabela. É determinístico: compare entre etapas e
  registre no HISTORICO. Valores de emulador, não de console real.
- Você **não consegue ver o jogo rodando** (o emulador é do Windows). Depois
  de mudanças visuais ou de jogabilidade, peça ao Marcos para testar com
  `./dev run` e descreva exatamente o que conferir.
- Não há testes automatizados; a verificação é build limpo + teste no emulador
  (lista em README → "Lista de verificação de release").

## Mapa do código (`src/`)

| Arquivo | Responsabilidade |
|---|---|
| `main.c` | laço com passo fixo de 60 Hz, estados (título, seleção, introdução, jogo, pausa, fim), progressão entre fases, câmera, HUD |
| `data.c` | **tabelas**: `character_defs`, `weapon_defs`, `power_defs`, `enemy_defs`, `skin_defs`, `prop_defs` |
| `config.h` | constantes globais |
| `game.h` | tipos, `GAME g` (estado global), protótipos |
| `levels.c` | **tabela de fases** `level_defs`: florestas fixas (`build_forest`; plano B e fase 5), `GEN_PARAMS` de cada fase gerada, céu, objetivo |
| `events.c` | **eventos** (etapa 06): tabela `EVENT_DEF` por fase, fila, ações (névoa, árvore cai, mata fecha, onda, silêncio, caminho abre), buscas em passos |
| `reach.c` | alcance durante a partida: 1 bit/célula + checagem local retomável (janela 33×33) |
| `mapgen.c` | **gerador procedural** (etapa 05): floresta por `g.seed`, validação por BFS, novas tentativas (semente + 1), plano B, medidas para o teste do PC e o bench |
| `level.c` | grade 128×128 (tipo + bit sólido), blocos 8×8 montados sob demanda num cache de 36, colisão com a grade |
| `objective.c` | objetivo da fase (`objective_update/text`), saída `X`, reforços `S` |
| `player.c` | até 2 jogadores (`g.players[2]`, `g.in[2]`), respawn |
| `enemies.c` / `weapons.c` / `powers.c` / `items.c` | inimigos, tiros, poderes, itens/caixas/efeitos/cenário |
| `collision.c` | **grade espacial**: obstáculos fixos e entidades por célula; `collide_move()`, `collide_track()`, `collide_solid_at()`, `collide_line_of_sight()` |
| `render.c` | motor 3D: GTE, Ordering Table, `render_mesh()`, HUD |
| `input.c` | controles das portas 1 e 2 |
| `sound.c` | efeitos no SPU: `sound_play`, `sound_play_at` (posicional), loops de ambiente nas vozes 0–1 |
| `rng.c` | gerador aleatório determinístico: `g.rng` (lógica) e `fx_range` (visual) |

Gerados no build (não edite): `build/gen/assets_gen.{h,c}` (declara
`<nome>_mesh` para cada `models/<nome>.h` e `tex_<nome>_t` para cada
`assets/textures/<nome>[-4bit|-8bit].png`), `build/textures/*.tim`.

## Regras do projeto (importantes)

- **Sem `float`/`double`, sem `malloc`.** Ponto fixo: `ONE = 4096` = 1.0;
  multiplicações seguidas de `>> 12`. Ângulos: 4096 = 360° (`isin/icos`).
- **Eixos do PS1:** X direita, **Y para baixo**, Z frente. Subir = Y negativo.
  Chão em Y = 0. 256 unidades = 1 metro = 1 célula do mapa.
- Entidades em **pools estáticos** com flag `active` (`MAX_*` em `config.h`).
- `*_update()` só muda estado; `*_draw()` só desenha. A câmera é calculada em
  `camera_update` (lógica) e aplicada em `camera_apply` (desenho).
- Lógica roda por passo (`game_tick`), não por quadro. Timers em passos de 1/60 s.
- Funções que agem sobre um jogador recebem `PLAYER *p` (inclusive poderes:
  `void power_x(PLAYER *p)`). Nunca use um "jogador global".
- Movimento de qualquer entidade passa por `collide_move()`.
- Texto na tela: **ASCII sem acentos** (fonte do sistema). Comentários do
  código em português, explicando o porquê.
- Ordering Table: entradas 0 e 1 são do HUD; `DRAWOPT.zbias` ajusta ordem
  (chão +6, sombras +2).
- Prefira resolver com **tabelas em `data.c`** antes de mexer no motor.
- Modelos: `models/*.h` são gerados pelo exportador — não edite à mão;
  regenere com `./dev models` (fonte: `assets/blender/exemplos.blend`, coleção
  `PSX`). O nome do arquivo define o símbolo.
- Sons: WAV em `assets/sounds/<nome>[-loop][-11k].wav` vira `sfx_<nome>`;
  prioridade/variação de tom em `sound_defs` (`data.c`), volumes em `config.h`.
  Som nunca muda `g` nem usa `g.rng`.
- Novos `.c` em `src/`, PNGs em `assets/textures/`, WAVs em `assets/sounds/` e `.h` em `models/` entram
  sozinhos no build (globs `CONFIGURE_DEPENDS`); não precisa editar o CMake.
- **Não use `__builtin_bswap32`** nem outros builtins que puxem a libgcc
  (o projeto não liga com ela; o link falha ou traz código inesperado).

### Determinismo (preparação para o modo link)

- Toda lógica que muda `g` depende só de `g.in[]` e de `g.rng` (semente
  `g.seed`). Nada de tempo, `VSync` ou outros valores locais na lógica.
- Sorteios da **lógica** (`*_update`, `game_tick`, spawns): `rand_range()` ou
  `rng_next(&g.rng)`. **Nunca** `rand()` da libc.
- Sorteios **puramente visuais** (partículas, pássaros): `fx_range()`, que usa
  um gerador separado e não afeta `g`.
- Nada de ler o controle direto do hardware dentro da lógica (use `g.in[]`).
- Todo campo novo é inicializado em `*_spawn`/reset (o `memset` de `g` zera o resto).
- **`./dev replay-test` sempre que mexer na lógica** (inimigos, jogador,
  colisão, eventos, objetivo, gerador). Partida roteirizada de 60 s por fase
  (entradas por passo, eventos forçados), hash de `g` + mapa a cada 10 s,
  comparado com `tools/replay_ref.txt`. Otimização tem que dar IGUAL. Mudança
  de lógica intencional: explique ao Marcos e regrave com `--save`.
- **Gerador de mapas:** só usa o RNG local semeado por `g.seed` + fase +
  tentativa (nunca `g.rng`, tempo ou `VSync`); deixa `g.rng` como estava.
  Mesma semente = mesmo mapa (o `./dev mapgen-test` compara o hash da grade).
- **Semente no modo link (etapa 11, não implementado):** hoje a semente vem de
  `g.frame` na seleção (cada console teria a sua). No modo link os dois
  consoles precisam combinar a mesma semente na conexão: um envia, o outro usa.

## Roteiro

O plano de evolução (arena → survival horror na floresta) está em
`docs/ROADMAP.md`, com um prompt por etapa em `docs/prompts/`. Uma etapa =
uma branch. As regras do ROADMAP (determinismo, orçamentos de desempenho,
`CREDITS.md` para assets de terceiros) valem para todas as etapas.

## Armadilhas conhecidas

- O GCC `mipsel-linux-gnu` gera `PT_GNU_STACK`; `tools/fix_gnu_stack.py`
  (ligado pelo CMake) remove antes do `elf2x`. Não remova.
- Exportador do Blender: rotação `(x,y,z)->(-x,-z,-y)` e ordem de vértices
  invertida (a GTE considera frente a ordem horária). Quads em ordem "Z".
- Faces que cruzam o plano próximo (`NEAR_Z`) são descartadas, não recortadas.
- `render_load_texture` calcula tpage/clut/u0 a partir da posição gravada
  no TIM; o empacotador (`tools/build_textures.py`) garante que nada cruze
  y = 256 nem u0+largura > 256.
- PSX não tem rede; netplay do RetroArch não funciona com PS1. Online hoje =
  Parsec. Modo link (SIO1/lockstep) é proposta não implementada (README).
- RAM do SPU: ~12,6 KB por segundo de som a 22 kHz (o manual, R8, diz 8 KB/s —
  está errado). Orçamento 450 KB; cada som também ocupa RAM principal (incbin).
- **GTE + GCC:** as macros de *carga* (`gte_SetRotMatrix`, `gte_SetTransMatrix`,
  `gte_SetColorMatrix`, `gte_ldv3`, `gte_ldrgb`...) leem memória sem avisar o
  compilador. Se a função acabou de escrever a matriz/vértices/cor, chame
  `GTE_BARRIER()` (em `render.c`) antes, senão o GCC pode apagar ou atrasar
  as escritas (bug da 04a: blocos desenhados na origem). Dados que vêm de
  uma chamada de função (ex.: `CompMatrixLV`) já estão na memória.
- Mapa grande: coordenadas do mundo vão a 32 768 → **nunca** guarde posição
  de mundo em `SVECTOR` (16 bits); use `VECTOR` ou vértices relativos a uma
  origem (`load_translation` em `render.c`).
- **2 jogadores:** `MAX_PLAYER_SEPARATION` é regra de jogo (`player.c`,
  só posições em `g`). Buffer de primitivas: o mundo para em
  `PACKET_LEN - PACKET_RESERVE` (blocos/árvores do perto para o longe);
  qualquer desenho novo de cenário deve respeitar o mesmo orçamento.
- **Câmera:** `g.cam_yaw` é lógica (fica em `g`); posição/distância/suavização
  são visuais (`camera.c`, fora de `g`). **Nenhuma lógica pode ler a câmera
  visual** (`camera_eye()` só para som/visual).
- Transparência em polígono **texturizado** só vale nos texels com o bit STP
  na CLUT (o `build_textures.py` só liga no quase-preto 0x8000). Os modelos
  de árvore não têm textura, por isso o 50% funciona neles.
- Tuning: `./dev bench -D NOME=VALOR` compara valores sem editar arquivos
  (constantes em `#ifndef`). Perfil: `cpu`/`gpu_espera`/`logica`/`desenho_*`
  em linhas (30 FPS = ~525).
- **Grade de colisão:** toda posição definida direto (sem `collide_move`) —
  nascer, morrer, respawn, teleporte — precisa de `collide_track(ent)` logo
  depois, senão a colisão fica desencontrada (`./dev bench` mostra
  `grade_erros`). Nada de percorrer `g.enemies`/`g.props` para colidir.
- Fases: tudo é colocado com `level_place_safe` (nunca dentro de tronco ou
  mata; se não achar lugar, não coloca). Fase 5 = só teste (`num_levels` = 4),
  floresta fixa (o bench compara entre etapas). Fases 1–4 são geradas
  (`GEN_PARAMS`); mudou o gerador ou um teto → rode `./dev mapgen-test 1000
  --bench` (0 inválidos, determinístico) e depois `./dev bench`. Teto de
  desempenho: cada fase ≤ pior caso da 04e + 5% e nunca > 504 linhas; se
  passar, baixe o teto de árvores (`GEN_TREES_Fn`), não mexa na névoa.
- Mata alta gerada: o custo é o **contorno** (cada lado exposto = 4 quads
  de 3 m). Não encha bolsões com mata (vira tufos soltos caros); sorteie só
  em células alcançáveis (`GM_REACH`). Ponto do bench perto de ~510 linhas
  cai para 20 FPS e a lógica sobe 1,5× (3 passos por quadro): deixe folga.
- **Eventos (etapa 06):** são lógica (só `g.in`, passos, `g.rng`); visual só
  lê `g.ev`. Toda célula que um evento bloqueia passa antes por
  `reach_begin/reach_step` (janela 33×33; fora dela = não acontece). Nada em
  cima de entidade/saída/início/S. Prefira mudar BITS (`level_set_flags`,
  não remonta) a mudar TIPO (`level_rebuild_chunk`, em fatias). Trabalho
  pesado da lógica vai em passos com orçamento por CONTAGEM (nunca tempo).
  Árvores em jogo: `level_add_tree` + `level_trees_changed` /
  `level_remove_tree` (o vetor fica ordenado por bloco). Tremor = offset de
  desenho (`render_set_shake`), nunca mexer na câmera nem na saída de vídeo.
  Bench: cada ponto mede um controle (180 quadros sem evento) e cada evento.
- Carga de fase: `start_level` desenha um quadro `CARREGANDO...` antes. Se a
  geração passar de ~1 s, divida-a entre quadros SEM rodar `game_tick` até
  o mapa ficar pronto (resultado idêntico ao de uma vez).
- Árvores: `level_add_tree` (fora de `g`, por bloco), **depois** das células;
  em pé = círculo na grade, tronco caído = células baixas (`CELL_LOW`). Posição por hash **com semente** (fase de
  teste: fixa; etapa 05: derivada de `g.seed`, nunca `g.rng`). Nomes fixos
  dos modelos: `pinheiro`, `arvore_seca`, `tronco` (+ `*_bb-4bit.png`).
- Blocos: o 3×3 em volta de cada jogador é montado no mesmo quadro (sem
  buracos); o anel externo e as remontagens dos eventos passam por UM
  construtor em fatias (`rebuild_step`, orçamento adaptativo até
  `REBUILD_CPU_CAP` = 440). Cada bloco tem 4 sub-blocos 4×4 (`sub_start`)
  descartados fora da tela/névoa — mantenha a ordem de montagem por quarto
  (`chunk_cell_order`). `level_invalidate` remonta inteiro (evite).
- Névoa: só cores **escuras** (em faces texturizadas a cor só escurece).
  DQA/DQB são gravados por `gte_SetDepthCue` (macro própria em `render.c`).
  Primitiva aditiva sem textura (cone da lanterna) precisa de `DR_TPAGE`
  antes **e** depois, senão muda o modo de mistura das sombras.
- Controles: **Select = lanterna** (skin só na seleção); L2 alterna o overlay
  ao **soltar**; overlay aberto + L2 + START pula de fase.
- **Desempenho (etapa 10a):** meta = pior quadro ≤ 450 linhas nas fases
  1–4 (multidão de 10 s, andar, remontagem) e ≤ 480 no quadro de evento,
  sempre 30 FPS; ~75 linhas de folga para as próximas etapas. Um quadro
  > 525 vira 3 passos de lógica e pode PRENDER em 20 FPS (biestável): deixe
  folga. `./dev bench -D BENCH_SUBSET=1 -D BENCH_NO_EVENTS=1` (14 piores
  pontos, ~1 min) para iterar; o completo no fim. Lições medidas: conta em
  C antes da GTE (pré-descarte de costas) é MAIS LENTA que deixar a GTE
  projetar e descartar; o caro é projetar o que está fora da tela
  (descarte por esfera em blocos/sub-blocos/modelos) e chamada de função
  por célula/vizinho em laço quente. `MAX_TICKS_PER_FRAME` = 4 (2 tira da
  armadilha dos 20 FPS, mas com sobrecarga forte o jogo fica em câmera
  lenta e muda o "tempo" do modo link: decisão do Marcos, não aplicado).
- Desempenho: ~30 FPS na fase com os limites atuais; confira com **L2** no
  jogo (`SEED`, `POLIS` = polígonos enviados à GPU, FPS e bytes de primitivas;
  `PACKET_LEN` = 96 KB por buffer).

## Ao terminar uma tarefa

1. `./dev build` sem erros/avisos novos.
2. Atualize `docs/GUIA.md` (receitas) e/ou `README.md` (técnico) se mudou
   algo que o usuário ou outro dev precisa saber.
3. Acrescente uma linha em `docs/HISTORICO.md` → "Registro de mudanças".
4. Diga ao Marcos o que testar no emulador.
