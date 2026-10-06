# Etapa 01 — Som: efeitos, passos, ambiente e música

> Leia antes: `CLAUDE.md`, `docs/ROADMAP.md`, manual capítulo 16 (receita R8).
> Exemplos do SDK: `/opt/psn00bsdk/share/psn00bsdk/examples/sound/vagsample`
> (no contêiner: `./dev shell`).

## Objetivo

Dar ao jogo efeitos sonoros, sons de ambiente e música, com um pipeline de
assets tão simples quanto o das texturas: **soltar um WAV numa pasta e compilar**.

## Tarefas

### 1. Pipeline de sons (sem depender de ferramentas externas)

- Crie `tools/wav2vag.py`: converte WAV PCM 16 bits (mono; se vier estéreo,
  mixa) para VAG (cabeçalho de 48 bytes big-endian + SPU-ADPCM), com
  reamostragem para a taxa escolhida e suporte a **loop** (flags de loop nos
  blocos ADPCM) quando o nome terminar em `-loop` (ex.: `vento-loop.wav`).
  Implemente o encoder ADPCM do SPU em Python puro (5 filtros, escolha do
  melhor filtro/shift por bloco de 28 amostras).
- Nomes: `assets/sounds/<nome>[-loop][-11k|-22k].wav` (padrão 22 050 Hz).
- No `CMakeLists.txt`, como as texturas: glob `CONFIGURE_DEPENDS` → gera
  `build/sounds/<nome>.vag` → `psn00bsdk_target_incbin` como `snd_<nome>` →
  declarações em `build/gen/assets_gen.h` (`extern SOUND sfx_<nome>;`) e uma
  função gerada `assets_load_sounds()` que carrega todos no SPU.
- Gere sons provisórios por script (`tools/make_placeholder_sounds.py`, com
  ruído/ondas sintetizadas): passo, tiro, acerto, dor, morte de inimigo,
  pegar item, vento-loop, grilos-loop. Eles serão trocados por sons reais.

### 2. Módulo de som (`src/sound.c`, `src/sound.h`)

- Baseie-se na receita R8, **com a correção `be32()`** (sem `__builtin_bswap32`).
- Gerenciamento de vozes: 24 vozes; vozes 0–1 reservadas para loops de
  ambiente; as demais em rodízio com prioridade (um som novo de prioridade
  menor não corta um maior).
- `sound_play(const SOUND*, int vol)` e **`sound_play_at(const SOUND*, x, z, vol)`**:
  volume pela distância à câmera e pan esquerda/direita pelo ângulo relativo à
  câmera (`cam_yaw`). Inaudível além de ~3 000 unidades.
- `sound_loop_start(voice, SOUND*, vol)` / `sound_loop_stop(voice)` para ambiente.
- Volume mestre de efeitos e de música (serão usados no menu da etapa 08).

### 3. Ligar os sons ao jogo

- Passos do jogador a partir de `walk_anim` (um passo a cada ~256 unidades
  andadas), alternando 2 variações.
- Tiro, acerto, dor do jogador, morte de inimigo, coleta de item.
- Inimigos emitem um som posicional ocasional quando estão perto e fora da
  tela (base para o terror).
- **Importante:** som é efeito colateral do desenho/áudio, não muda `g`. Pode
  ser disparado da lógica, mas não pode alterar estado nem consumir `g.rng`
  (use `fx_rng` para variações).

### 4. Música por CD-DA

- `assets/music/faseNN.wav` (44 100 Hz, estéreo) → faixas de áudio no
  `iso.xml` (gere a lista no CMake, como os sons).
- `music_play(track)` / `music_stop()` em `src/music.c` usando a `psxcd`:
  leia o TOC (`CdGetToc`), posicione no início da faixa (`CdlSetloc`) e
  `CdlPlay` **sem** o parâmetro de faixa (nem todo emulador suporta). Ligue
  `CdlModeDA` e repita a faixa quando terminar (modo de relatório ou
  verificação periódica de posição).
- Uma faixa de título e uma por fase. Gere músicas provisórias (drone
  sintetizado de 20–30 s) pelo script de placeholders.
- Atenção: enquanto a música CD-DA toca, não dá para ler dados do CD (hoje
  não lemos nada, então está ok; documente a limitação).

### 5. Documentação

- `docs/GUIA.md`: seção "Sons e música" (como converter, nomes, loop, onde
  achar sons livres, registrar no `CREDITS.md`).
- `README.md`: remover "Sem áudio" das limitações.

## Fora do escopo

Música sequenciada (MIDI), streaming XA, sons 3D com reverberação.

## Critérios de aceite

- `./dev build` limpo; trocar um WAV em `assets/sounds/` e recompilar troca o som.
- O jogo **não trava** ao iniciar (lição da R8) e continua a ~30 FPS.
- Passos, tiros e ambiente audíveis no PCSX-Redux e no DuckStation; música
  toca e repete no título e na fase.
- Uso da RAM do SPU impresso no overlay L2 (`SPU xxxK/512K`).

## Ao terminar

Diga ao Marcos exatamente o que ouvir em cada tela e onde trocar os sons
provisórios pelos reais.
