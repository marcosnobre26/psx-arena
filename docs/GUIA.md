# Guia prático — como modificar o PS1 Arena

Receitas passo a passo para mudar o jogo. Para arquitetura, ambiente e
compilação, veja o [README](../README.md).

Depois de qualquer mudança: `./dev run` (compila e abre no emulador) ou
**Ctrl+Shift+B** no VS Code.

**Unidades:** 256 = 1 metro = 1 bloco do mapa. **Ângulos:** 4096 = 360°.
**Cores:** 0–255. Não existe `float`: `ONE` (4096) representa 1.0.

---

## Sumário

1. [Controles](#1-controles)
2. [Ajustar números](#2-ajustar-números)
3. [Criar uma fase e escolher o objetivo](#3-criar-uma-fase-e-escolher-o-objetivo)
4. [Modelar no Blender](#4-modelar-no-blender)
5. [Novo personagem jogável](#5-novo-personagem-jogável)
6. [Texturas](#6-texturas)
7. [Objetos de cenário](#7-objetos-de-cenário)
8. [Nova arma](#8-nova-arma)
9. [Novo poder](#9-novo-poder)
10. [Novo inimigo](#10-novo-inimigo)
11. [Nova skin](#11-nova-skin)
12. [Números aleatórios](#12-números-aleatórios)
13. [Sons e música](#13-sons-e-música)
14. [Emuladores](#14-emuladores)
15. [Gravar em CD e jogar no console](#15-gravar-em-cd-e-jogar-no-console)
16. [Roteiro de estudo](#16-roteiro-de-estudo)

---

## 1. Controles

| Botão | Ação |
|---|---|
| D-pad / analógico esquerdo | andar (relativo à câmera) |
| **X** | pular — no ar você passa por cima dos inimigos |
| **Quadrado** (segure) | atirar, com mira automática no inimigo à frente |
| **Triângulo** | trocar de arma |
| **Círculo** | usar poder (gasta energia) |
| **R2** | trocar de poder |
| **L1 / R1** ou analógico direito | girar a câmera |
| **Select** | liga/desliga a **lanterna** (a skin muda só na tela de seleção) |
| **Start** | pausar |
| **L2** (soltar) | depuração: névoa, memória do SPU, semente, polígonos, FPS, memória de primitivas, posição |
| **L2 + Start** | (com o overlay aberto) pula para a próxima fase |
| **L2 + direcional** | (com o overlay aberto) teleporta para os 4 pontos fixos da fase de teste |
| **L2 + Select** | (com o overlay aberto) mostra a grade de colisão: quadrado em cada célula sólida perto do jogador 1 |

**Tela de seleção:** esquerda/direita escolhe, **Select** troca a skin,
**X** confirma, **Círculo** desfaz (ou volta ao título). O **controle 2**
entra apertando **Start**; com dois jogadores, a partida começa quando os
dois confirmam.

**Objetivo:** derrotar todos os inimigos. Gemas dão pontos e energia; caixas
podem soltar vida ou energia; o item branco (`W`) libera uma arma nova. Com
2 jogadores, quem cai volta em 5 s ao lado do parceiro; se os dois caírem,
é game over. Na tela final, **Start** joga de novo e **Select** volta à
seleção.

---

## 2. Ajustar números

[`src/config.h`](../src/config.h) concentra as constantes globais: câmera,
gravidade, campo de visão, distância de desenho, limites de objetos, tempo de
respawn. Atributos de cada personagem (vida, velocidade, pulo) ficam na
tabela `character_defs` em [`src/data.c`](../src/data.c).

Exemplos:
- `CAM_DIST 820` → `1200`: câmera mais afastada.
- `GRAVITY 3` → `2`: pulos mais "flutuantes".
- `FOV_H 160` → `200`: mais zoom (campo de visão menor).
- Na linha do `ROBO` em `character_defs`, `pulo 34` → `50`: pulo bem mais alto.

---

## 3. Criar uma fase e escolher o objetivo

As fases ficam em [`src/levels.c`](../src/levels.c): um mapa em texto e uma
linha na tabela `level_defs`. **A ordem da tabela é a ordem do jogo.**

**1. Desenhe o mapa** (vetor de strings terminado em `NULL`):

```c
static const char *const map_ponte[] = {
	"################",
	"#P....E.....#..#",
	"#..####..Q..#X.#",
	"################",
	NULL
};
```

| Caractere | Significado | Caractere | Significado |
|---|---|---|---|
| `#` | parede | `.` | chão |
| `P` | início dos jogadores | `C` | caixa destrutível |
| `G` | gema (pontos) | `H` | vida |
| `N` | energia | `W` | arma nova |
| `X` | **saída** da fase | `Q` | **item de missão** |
| `S` | **ponto de reforço** (objetivo SURVIVE) | `L` | **pilha** da lanterna |
| espaço | vazio | | |
| `E` `B` `F` | inimigos (`enemy_defs`) | `1`…`9` | objetos de cenário (`prop_defs`) |

Regras: todas as linhas com o mesmo tamanho, máximo 128×128. Chão, paredes
(com 2 "andares" de textura) e colisão são gerados a partir do texto. O
segundo jogador nasce na primeira célula livre ao lado do `P`. Ao criar
inimigo ou cenário novo, **não use letras reservadas**: `# . P C G H N W X
Q S L` e `A` (reservada para o ATIRADOR).

**2. Acrescente a linha na tabela:**

```c
/* nome      mapa
 *   chão          parede       céu=névoa   névoa near/far  luz ambiente  música
 *   objetivo      parâmetro  máx. inimigos  texto do objetivo */
{ "PONTE",   map_ponte,
  &tex_floor_t, &tex_wall_t, 10, 12, 24,  1300, 3200,     40, 40, 56,   0,
  OBJ_COLLECT,  0,         0,             "PEGUE O ITEM E SAIA" },
```

| Objetivo | Vence quando | Parâmetro | HUD |
|---|---|---|---|
| `OBJ_KILL_ALL` | não sobra inimigo | — | `INIMIGOS 5` |
| `OBJ_REACH_EXIT` | um jogador vivo chega ao `X` (**obrigatório** ter `X`) | — | `VA ATE A SAIDA` |
| `OBJ_COLLECT` | o grupo junta N itens `Q` | N (0 = todos do mapa) | `ITENS 2/4` |
| `OBJ_SURVIVE` | o tempo acaba com alguém vivo | segundos | `SOBREVIVA 0:45` |

- **Saída:** se o mapa tem `X`, cumprir o objetivo **abre** a saída (o anel
  acende e gira) e a fase termina quando alguém chega nela. Sem `X`, termina
  na hora.
- **Reforços (SURVIVE):** a cada 8 s (`REINFORCE_TIME`), se houver menos
  inimigos vivos que o "máx. inimigos" da fase, nasce um inimigo num `S`
  longe dos jogadores (`REINFORCE_MIN_DIST`) e fora da visão da câmera.
- **Texto do objetivo:** aparece na introdução da fase. ASCII, sem acentos,
  até ~38 caracteres.
- **Música:** o campo existe, mas a reprodução (CD-DA) é da etapa 01b, adiada.
- **Progressão:** vencer leva pontos, vida, energia, armas e poder para a
  próxima fase (jogador caído volta com 50% da vida). Game over recomeça a
  fase com o estado de quando ela começou. Depois da última: `FIM DE JOGO`.

**3. Testar a fase sem jogar as anteriores:** em `config.h`, descomente
`#define DEBUG_START_LEVEL 3` e troque o número pela fase (1 = primeira).
O jogo novo, ao sair da seleção, começa direto nela. Comente de novo antes
do commit.

Mapas grandes com muitas paredes custam desempenho: confira o FPS com **L2**
no ponto mais pesado. **Atalho:** com o overlay L2 aberto, segure **L2** e
aperte **Start** para pular para a próxima fase (levando o progresso).

### Como o mundo é montado

**Grade.** O mapa é uma grade de até **128×128 células** (`MAP_MAX_W/H`);
1 célula = 1 metro = 256 unidades. Cada célula é **1 byte**: o tipo nos
bits 0–5 e o bit 7 (`CELL_SOLID`) diz se bloqueia. Tipos (`game.h`):

| Tipo | O que é | Sólido | Altura |
|---|---|---|---|
| `CELL_VOID` | nada (sem chão) | sim | — |
| `CELL_FLOOR` | chão dos mapas de texto (`.`), textura da fase | não | — |
| `CELL_WALL` | parede dos mapas de texto (`#`), textura da fase | sim | 1,5 m |
| `CELL_DIRT` / `CELL_LEAVES` / `CELL_ROOTS` | chão de terra / folhas / raízes | não | — |
| `CELL_THICKET` | mata densa | sim | 3 m |

**Mapa por código.** Em vez do texto, a fase pode ter `map = NULL` e uma
função em `LEVEL_DEF.build` que chama `level_begin(w, h)`,
`level_set_cell(x, z, CELL_...)` (a solidez sai do tipo) e
`level_place('E', x, z)` (a mesma legenda do texto: `P`, `X`, inimigos,
itens...). Veja `build_test_forest()` em `levels.c` (fase 5).

**Blocos sob demanda.** A geometria não existe para o mapa inteiro: o chão
e as paredes são montados em **blocos de 8×8 células**, num cache de
`CHUNK_SLOTS` (36) blocos:

- o **3×3 em volta de cada jogador** é montado no mesmo quadro, sempre
  (nunca falta chão perto de quem joga, nem girando a câmera nem correndo);
- o resto do **5×5 em volta do grupo** entra numa fila de 1 bloco por
  quadro (`CHUNK_BUILDS_PER_FRAME`), do mais perto ao mais longe — eles
  ficam dentro da névoa, então a montagem aos poucos não aparece;
- sem lugar livre, o bloco mais distante é reaproveitado.

O overlay **L2** mostra `BLOCOS montados/36 +montados_neste_quadro`; se
aparecer `CHEIO!`, algum bloco passou de 256 quads (`CHUNK_MAX_QUADS`).

**Visual.** A luz de cada face já vem gravada na cor dos vértices (as
normais são fixas), com uma variação sutil de cor por vértice e a textura
do chão girada por célula (tudo por hash da posição: sempre igual). Na
tela só se aplica a névoa. Texturas da floresta: `tools/make_forest_textures.py`.

**Mudou o mapa durante o jogo?** `level_set_cell()` e depois
`level_invalidate(x, z)`: o bloco da célula é remontado.

**Conferir a colisão:** com o overlay aberto, **L2 + Select** desenha um
quadrado em cima de cada célula sólida em volta do jogador 1 (raio
`COLMARK_RADIUS`), na altura do topo: vermelho = parede/mata, laranja =
caixa, roxo = vazio. Desenho e colisão batem quando cada parede tem o seu
quadrado vermelho exatamente no topo.

**Medir sempre no mesmo lugar (fase 5):** com o overlay aberto, segure
**L2** e aperte o direcional: ← início, → cruzamento das trilhas, ↑ canto
denso (pior caso), ↓ maior clareira. A câmera fica sempre na mesma direção.

### Clima: névoa, luz e lanterna

**Névoa** (por fase, colunas `névoa near/far` em `level_defs`): até `near`
tudo aparece normal; entre `near` e `far` a cor vai para a cor do céu; além
de `far` nada é desenhado (economiza polígonos). Os valores são de
profundidade a partir da câmera, que fica a ~1000 do jogador: **near abaixo
de ~1100 enevoa o próprio jogador**. A névoa da GTE cresce em 1/z: logo
depois do `near` ela já engrossa rápido.

- Use **céu escuro**: em faces com textura a cor só *escurece* a textura,
  então uma névoa clara (cinza, branca) não funciona nelas.
- **Luz ambiente** (0–255 por canal) também é por fase. A luz direcional
  ("lua", fraca e azulada) fica em `config.h` (`MOON_R/G/B`).
- **Ajuste ao vivo:** descomente `#define DEBUG_FOG_TUNING` em `config.h`.
  No jogo, abra o overlay (**L2**), segure **R2** e use o direcional:
  cima/baixo = `far` ±100, direita/esquerda = `near` ±50. A linha `FOG` do
  overlay mostra os valores; anote os bons e passe para `levels.c`.

**Lanterna** (Select liga/desliga, cada jogador a sua):
- Cone de luz no chão à frente do jogador; inimigos, itens e cenário **dentro
  do cone** são vistos 1,6× mais longe (`LANTERN_FOG_MUL`, `LANTERN_RANGE`).
- Bateria 0–1000 (barra `LT`): ligada gasta ~1 a cada 6 passos (~100 s).
  Abaixo de 15% a luz falha às vezes; zerada, pisca 1 s e apaga.
- **Pilha** (`L` no mapa, ou 10% de chance ao matar um inimigo) recarrega 400.
- A bateria passa de fase e não recarrega quando o jogador cai.
- Ajustes em `config.h`, bloco "Lanterna" (alcance, ângulo, cor, consumo).

---

## 4. Modelar no Blender

Regras do PS1 que o exportador espera:

- **Poucos polígonos:** 50 a 500 faces por personagem era o padrão da época.
- **Escala:** 1 metro no Blender = 256 unidades. Personagens: 1,2 a 1,8 m.
- **Frente** do modelo = vista *Front* (Numpad 1), olhando para **−Y**.
- **Origem nos pés.** A posição do objeto é ignorada; rotação e escala valem.
- **Cada material vira uma cor** (a *Base Color* do Principled BSDF). A ordem
  dos slots (0, 1, 2…) importa: o jogo pode trocar essas cores por paletas.
- Material com nome terminado em `_UNLIT` (ex.: `Olhos_UNLIT`) ignora a luz
  e parece brilhar.
- Faces com mais de 4 vértices são trianguladas automaticamente.
- Coordenadas cabem em 16 bits: no máximo ~120 m a partir da origem.

**Fluxo recomendado:** use [`assets/blender/exemplos.blend`](../assets/blender/).
Ele já tem todos os modelos do jogo na coleção **PSX**. Crie ou edite objetos
dentro dessa coleção, salve e rode:

```bash
./dev models   # cada objeto da coleção PSX vira models/<nome>.h
```

Nome do objeto → arquivo → símbolo em C: `Heroi Azul` → `models/heroi_azul.h`
→ `heroi_azul_mesh`. Propriedades opcionais no objeto (*Custom Properties*):
`psx_name` (outro nome em C) e `psx_scale` (escala diferente de 256).

**Exportar só um objeto pela interface:** aba *Scripting* → *Open* →
`tools/blender_export_psx.py` → *Run Script* → selecione o objeto →
*File → Export → PS1 Mesh (.h)* → salve em `models/`.

**Pela linha de comando** (sem abrir o Blender):
```bash
blender meu.blend --background --python tools/blender_export_psx.py -- \
    --object Heroi --out models/heroi.h
```

---

## 5. Novo personagem jogável

1. Modele o personagem no `exemplos.blend`, coleção **PSX** (ex.: objeto `heroi`).
2. `./dev models` → gera `models/heroi.h`.
3. Em `src/data.c`, adicione uma linha em `character_defs`:
   ```c
   /* nome     descrição            modelo        tamanho skins tex   vida vel pulo arma */
   { "HEROI",  "MEU PERSONAGEM",    &heroi_mesh,  ONE,    0,    NULL, 120, 15, 36,  1 },
   ```
   - **skins = 1** só se o modelo tiver os 5 materiais do robô, na ordem:
     corpo, cabeça, membros, visor, arma. Com `0`, usa as cores do Blender.
   - **tex**: `&tex_nome_t` se o modelo tiver textura (seção 6).
   - **arma**: índice em `weapon_defs` (0 blaster, 1 espingarda, 2 metralhadora, 3 canhão).
   - Descrição: só letras sem acento (a fonte do sistema é ASCII).
4. `./dev run` — o personagem aparece na tela de seleção.

---

## 6. Texturas

**Solte o PNG em `assets/textures/` e compile.** A conversão para TIM e a
posição na VRAM são automáticas.

| Arquivo | Formato | Símbolo no código |
|---|---|---|
| `madeira.png` | 16 bits (cores diretas) | `tex_madeira_t` |
| `madeira-8bit.png` | 8 bits, 256 cores (metade da VRAM) | `tex_madeira_t` |
| `madeira-4bit.png` | 4 bits, 16 cores (¼ da VRAM) | `tex_madeira_t` |

- Até 256×256; 64×64 ou 128×128 é o usual. Largura múltipla de 2 (8 bits) ou 4 (4 bits).
- Alpha < 50% vira transparente; preto puro vira "quase preto" (no PS1,
  preto puro é transparente).
- `./dev vram` mostra onde cada textura ficou.

Usando ao desenhar:
```c
DRAWOPT opt = { 0 };
opt.tex = &tex_madeira_t;
render_mesh(&heroi_mesh, &pos, &rot, ONE, &opt);
```

**Modelo texturizado:** faça o UV map e ligue um nó *Image Texture* no
material. O `./dev models` detecta o tamanho da imagem, copia o PNG para
`assets/textures/` e escreve no topo do `.h` qual textura usar. Use
interpolação *Closest* no nó para ver no Blender o visual "pixelado" do PS1.

Tem um `.tim` pronto de outra ferramenta? Coloque em `assets/tim/` — ele é
embutido como está, na posição de VRAM que o próprio arquivo define (cuidado
para não sobrepor as automáticas; confira com `./dev vram`).

---

## 7. Objetos de cenário

Pilares, árvores, estátuas — sem escrever código. Em `prop_defs` (`data.c`):

```c
/* letra  modelo         tamanho   sólido  textura */
{ '1',   &pillar_mesh,  ONE,      1,      NULL },
{ '3',   &arvore_mesh,  ONE,      1,      &tex_folhas_t },
```

Coloque o dígito no mapa. `sólido = 1` bloqueia jogadores, inimigos e tiros;
o raio de colisão é calculado do próprio modelo.

Fluxo completo de um asset novo: modele `arvore` na coleção PSX (com a
textura `folhas.png`) → `./dev models` → linha em `prop_defs` → `3` no mapa
→ `./dev run`.

---

## 8. Nova arma

Em `weapon_defs` (`data.c`):
```c
/* nome     recarga vel  dano proj abert. impr. vida  tamanho  cor */
{ "LASER",       6,  70,   2,   1,    0,    0,   30,  ONE/2,  { 255, 40, 40 } },
```
- **recarga**: quadros entre tiros (60 = 1 s). **proj**: projéteis por disparo.
- **abert.**: abertura do leque (ângulo). **impr.**: imprecisão aleatória.
- Cada `W` do mapa libera a próxima arma da lista que o jogador ainda não tem.

---

## 9. Novo poder

1. Em `src/powers.c`:
   ```c
   void power_super_pulo(PLAYER *p) {
       p->vy = -70;
       p->on_ground = 0;
   }
   ```
2. Em `src/data.c`, declare `void power_super_pulo(PLAYER *p);` e adicione
   `{ "SUPER PULO", 15, power_super_pulo },` em `power_defs`.

O parâmetro `p` é quem usou o poder — funciona igual para o jogador 1 e o 2.
**R2** já alterna para o poder novo e **Círculo** usa.

---

## 10. Novo inimigo

Em `enemy_defs` (`data.c`), uma linha com uma **letra nova**, e use a letra no mapa:
```c
/* nome    letra vida vel dano tamanho pontos modelo       paleta (corpo, chifres, olhos) */
{ "CHEFE", 'K',  60,  3,  40,  ONE*3,  2000, &grunt_mesh, { {255,40,40}, {60,0,0}, {255,255,0} } },
```
Para um modelo seu, troque `&grunt_mesh`. A paleta troca as cores por slot
de material (como as skins).

---

## 11. Nova skin

Uma linha em `skin_defs` (`data.c`) com 5 cores: corpo, cabeça,
braços/pernas, visor, arma. Vale para personagens com `skins = 1`.

---

## 12. Números aleatórios

Existem **dois** geradores (ver `src/rng.c`), e a escolha importa por causa
do futuro modo link: os dois consoles precisam sortear os mesmos números.

| Para quê | Use | Exemplo |
|---|---|---|
| **Lógica** (algo que muda `g`: IA, drops, spawns, dano) | `rand_range(lo, hi)` ou `rng_next(&g.rng)` | `e->think = rand_range(40, 140);` |
| **Só visual** (partículas, pássaros, piscadas) | `fx_range(lo, hi)` | `jitter = fx_range(-4, 4);` |

- **Nunca** use `rand()`/`srand()` da libc.
- A semente da partida fica em `g.seed` e aparece no overlay **L2**
  (`SEED`). `game_reset(seed)` semeia `g.rng` antes de carregar a fase.
- Para repetir sempre a mesma partida (testes, caça a bugs), descomente
  `#define DEBUG_FIXED_SEED 1234` em `config.h`. Com as mesmas entradas,
  os inimigos devem se mover igual em toda partida. Comente de novo depois.

---

## 13. Sons e música

### Efeitos sonoros

**Solte o WAV em `assets/sounds/` e compile.** A conversão para VAG
(formato do chip de som, o SPU) é automática (`tools/wav2vag.py`).

| Arquivo | Resultado | Símbolo no código |
|---|---|---|
| `porta.wav` | 22 050 Hz, toca uma vez | `sfx_porta` |
| `porta-11k.wav` | 11 025 Hz (metade da memória; bom para graves e ruídos) | `sfx_porta` |
| `chuva-loop.wav` | repete sem parar (para ambiente) | `sfx_chuva` |
| `chuva-loop-11k.wav` | os dois | `sfx_chuva` |

- WAV **PCM 16 bits** (ou 8), qualquer taxa; estéreo vira mono sozinho.
  No Audacity: *Arquivo → Exportar → WAV, PCM 16 bits com sinal*.
- Memória: o SPU tem 512 KB (orçamento do jogo: 450 KB). Conta rápida:
  **~12,6 KB por segundo a 22 kHz**, ~6,3 KB/s a 11 kHz. O overlay **L2**
  mostra o uso (`SPU xxxK/512K`). Cada som também ocupa o mesmo tanto na RAM
  principal (vai embutido no EXE).
- Loops: corte o WAV num ponto em que o fim emende no começo sem estalo.
  Ideal: número de amostras múltiplo de 28.
- Confira a qualidade sem abrir o jogo:
  `python3 tools/wav2vag.py --selftest assets/sounds/porta.wav` (SNR acima de
  ~20 dB é aceitável para efeito).

Tocando no código (som é só efeito colateral: nunca mude `g` por causa dele):
```c
sound_play(&sfx_porta, 0x3000);                     /* sem posição (menus) */
sound_play_at(&sfx_porta, x, z, 0x3000);            /* no mundo: volume e lado */
sound_loop_start(0, &sfx_chuva, 0x1400);            /* vozes 0 e 1: ambiente */
sound_loop_stop(0);
```
Volume: 0..`0x3fff`. Os volumes dos eventos atuais ficam em `config.h`
(`VOL_PASSO`, `VOL_TIRO`...). Distância audível: `SOUND_NEAR`/`SOUND_FAR`.

**Prioridade e variação de tom:** uma linha em `sound_defs` (`data.c`).
São 22 vozes para efeitos; quando todas estão ocupadas, um som novo só
corta uma voz de prioridade menor ou igual à dele. A variação de tom (em %)
evita que sons repetidos (passos) soem "metralhados".

**Trocar um som provisório por um real:** os sons atuais foram gerados por
`tools/make_placeholder_sounds.py`. Para trocar, apague o provisório e
coloque o seu com o **mesmo nome-base** (ex.: apague `tiro-11k.wav`, coloque
`tiro.wav`). O símbolo `sfx_tiro` continua o mesmo, então o código não muda.
Não rode o script de provisórios de novo depois disso (ele recria os arquivos).

**Onde achar sons livres:** [freesound.org](https://freesound.org) (filtre
por licença CC0 ou CC-BY), [opengameart.org](https://opengameart.org),
[sonniss.com/gameaudiogdc](https://sonniss.com/gameaudiogdc) (pacotes
liberados para jogos), [kenney.nl](https://kenney.nl/assets) (CC0). **Todo
som de terceiros vai para o `CREDITS.md`** (autor, link, licença); CC-BY
exige o crédito. Nada tirado de jogos comerciais.

### Música

Faixas de CD-DA (`assets/music/`): chegam na etapa 01b.

---

## 14. Emuladores

| Emulador | Arquivo | BIOS | Observações |
|---|---|---|---|
| **PCSX-Redux** | `.exe` ou `.cue` | não precisa (OpenBIOS) | ótimo depurador de VRAM/GPU |
| **DuckStation** (PC/Android) | `.exe` ou `.cue` | precisa | o mais preciso |
| **ePSXe** (PC/Android) | `.cue` | precisa | |
| **RetroArch** | `.cue` | Beetle/SwanStation precisam | núcleos Beetle PSX, SwanStation, PCSX ReARMed |
| **Mednafen** | `.cue` | precisa | use `-force_module psx` (o CD não tem licença) |

- Mantenha `arena.bin` e `arena.cue` na mesma pasta.
- Emuladores com BIOS simulado (HLE) podem falhar com homebrew; prefira BIOS real.
- 2 jogadores: configure um controle na **porta 2** do emulador.

Configurar o `./dev run` (arquivo `dev.conf`):
```bash
# PCSX-Redux
EMULATOR="/mnt/c/Emuladores/pcsx-redux/pcsx-redux.exe"
EMULATOR_ARGS="-run -loadexe"
RUN_FILE="exe"

# DuckStation
EMULATOR="/mnt/c/Emuladores/duckstation/duckstation-qt-x64-ReleaseLTCG.exe"
EMULATOR_ARGS=""
RUN_FILE="cue"
```

---

## 15. Gravar em CD e jogar no console

1. Grave o **`arena.cue`** (não o `.bin` sozinho) num **CD-R** (o PS1 não lê
   CD-RW). No Windows: ImgBurn → *Write image file to disc*, em 4x a 8x.
2. O console precisa aceitar discos não oficiais: **modchip** ou **softmod**
   (*Tonyhax International*, *FreePSXBoot*, *Unirom*).
3. Consoles japoneses e alguns PAL com modchip exigem o arquivo de licença da
   Sony no disco. Ele não pode ser distribuído; se precisar, extraia de um
   jogo seu com `dumpsxiso` e ative a linha `<license>` no `iso.xml`.
4. Em consoles PAL o jogo roda em 50 Hz (um pouco mais lento).

Para testar sem gastar CD: **Unirom** + cabo serial/USB envia o `arena.exe`
do PC para o console em segundos (ferramenta `nops`).

---

## 16. Roteiro de estudo

Do mais fácil ao mais difícil:

1. Mudar números no `config.h` e nos atributos dos personagens.
2. Redesenhar o mapa.
3. Criar uma arma, uma skin e um poder.
4. Modelar um personagem no Blender e colocá-lo na seleção.
5. Fazer uma textura e aplicar num modelo com UV.
6. Um inimigo que **atira** (reaproveite `weapons.c` com um campo "dono").
7. Uma **fase nova** (mapa + linha em `level_defs`, `levels.c`) com outro objetivo.
8. **Som**: biblioteca `psxspu` do PSn00bSDK (exemplos em `examples/sound`)
   ou música CD-DA no `iso.xml`.
9. Carregar modelos e texturas **do CD** (`psxcd`) em vez de embutir no executável.
10. **Animação** de personagem: quadros de vértices ou partes separadas.
11. Recorde no **memory card**.
12. Modo **link** pela porta serial (veja "Rede" no README).
