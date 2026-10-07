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
./dev clean          # apaga build/
VERBOSE=1 ./dev build
```

- **Sempre rode `./dev build` depois de editar** e corrija erros/avisos novos.
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
| `levels.c` | **tabela de fases** `level_defs`: mapa em texto, texturas, céu, objetivo |
| `level.c` | lê o mapa (`level_load`), geração de chão/paredes em blocos 4×4, colisão com o mapa |
| `objective.c` | objetivo da fase (`objective_update/text`), saída `X`, reforços `S` |
| `player.c` | até 2 jogadores (`g.players[2]`, `g.in[2]`), respawn |
| `enemies.c` / `weapons.c` / `powers.c` / `items.c` | inimigos, tiros, poderes, itens/caixas/efeitos/cenário |
| `collision.c` | colisão por círculos: `collide_move()`, `collide_blocked()` |
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
- Névoa: só cores **escuras** (em faces texturizadas a cor só escurece).
  DQA/DQB são gravados por `gte_SetDepthCue` (macro própria em `render.c`).
  Primitiva aditiva sem textura (cone da lanterna) precisa de `DR_TPAGE`
  antes **e** depois, senão muda o modo de mistura das sombras.
- Controles: **Select = lanterna** (skin só na seleção); L2 alterna o overlay
  ao **soltar**; overlay aberto + L2 + START pula de fase.
- Desempenho: ~30 FPS na fase com os limites atuais; confira com **L2** no
  jogo (`SEED`, `POLIS` = polígonos enviados à GPU, FPS e bytes de primitivas;
  `PACKET_LEN` = 96 KB por buffer).

## Ao terminar uma tarefa

1. `./dev build` sem erros/avisos novos.
2. Atualize `docs/GUIA.md` (receitas) e/ou `README.md` (técnico) se mudou
   algo que o usuário ou outro dev precisa saber.
3. Acrescente uma linha em `docs/HISTORICO.md` → "Registro de mudanças".
4. Diga ao Marcos o que testar no emulador.
