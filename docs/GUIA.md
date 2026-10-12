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
8. [Criar uma arma de cada tipo](#8-criar-uma-arma-de-cada-tipo)
9. [Magias (os antigos poderes)](#9-magias-os-antigos-poderes)
10. [Criar um inimigo com percepção](#10-criar-um-inimigo-com-percepção)
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
| **Quadrado** | atacar com a arma na mão. Arco: **segure** para carregar e **solte** para atirar |
| **Triângulo** | próxima arma (corpo a corpo, arco, magias, armas de fogo que você tem) |
| **R2** (segure) | andar devagar e **em silêncio** (os inimigos não ouvem seus passos) |
| **Círculo** | cura rápida: usa uma **bandagem** (ou, sem bandagem, o **kit médico**) do inventário. A magia CURA nunca entra no atalho |
| **L1 / R1** ou analógico direito | girar a câmera |
| **Select** | liga/desliga a **lanterna** (a skin muda só na tela de seleção) |
| **Start** | abre o **menu** (o jogo para): Mapa, Objetivo, Estatísticas (fase e total), Armas, Itens, Opções |
| **L2** (soltar) | depuração: névoa, memória do SPU, semente, polígonos, FPS, memória de primitivas, posição |
| **L2 duas vezes** (rápido) | página **AMBIENTE** do overlay (etapa 09); outro toque fecha. Nela: **L2 + Quadrado** bando levanta voo, **+ Círculo** olhos no escuro, **+ X** vulto, **+ Triângulo** salto entre troncos, **+ R1** galho estalando, **+ L1** lua à frente da câmera, **+ R2** bando no céu (alterna: horizonte / rasante) |
| **L2 + Start** | (com o overlay aberto) pula para a próxima fase |
| **L2 + direcional** | (com o overlay aberto) teleporta para os 4 pontos da fase (← início, → frente da barreira de raízes, se houver, ↑ canto mais denso, ↓ saída) |
| **L2 + Quadrado / Círculo / X / L1 / R1 / R2** | (com o overlay aberto) dispara na hora: névoa avança / árvore cai / caminho abre (mesmo sem chave) / mata fecha atrás / onda / silêncio |
| **L2 + Select** | (com o overlay aberto) mostra a grade de colisão: quadrado em cada célula sólida perto do jogador 1 |
| **L2 + Triângulo** | (com o overlay aberto) nascem 8 inimigos em volta do jogador 1, até 24 (para medir); `COL` no overlay = testes de colisão no último passo |

**Tela de seleção:** esquerda/direita escolhe, **Select** troca a skin,
**X** confirma, **Círculo** desfaz (ou volta ao título). O **controle 2**
entra apertando **Start**; com dois jogadores, a partida começa quando os
dois confirmam.

**Tela de título:** cima/baixo escolhe **NOVO JOGO**, **CONTINUAR** (o save
do memory card) ou **CREDITOS**; **X** ou **Start** confirma. **Start no
controle 2** em NOVO JOGO já entra com 2 jogadores.

**Menu do START:** **L1/R1** (ou esquerda/direita) troca a aba, cima/baixo
escolhe a linha, **X** usa o item (aba Itens) ou equipa a arma (aba Armas),
**Quadrado** larga o item no chão, **Círculo** ou **Start** sai. Com 2
jogadores, quem abriu controla e **Select** mostra o inventário do outro.
O menu volta na última aba aberta.

**Inventário (8 espaços):** só ocupam espaço bandagem (+25 de vida, até 3
por espaço), kit médico (+60, 1), poção de mana (+40, 2) e pilha da lanterna
(2). Relíquias, chaves, gemas, armas, flechas e balas não ocupam. Inventário
cheio: o item fica no chão. Item largado só volta para quem largou depois
que ele se afasta. O inventário passa de fase e não se perde ao cair.

**Objetivo:** o da fase. Gemas dão pontos e mana; caixas (quebram com
golpe ou projétil) podem soltar bandagem ou poção; `W` dá a próxima arma (arco,
pistola, machado, espingarda), `V`
flechas e `M` balas. A mana volta sozinha bem devagar só até 25%. Com
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

As fases ficam em [`src/levels.c`](../src/levels.c): uma **função que monta
a floresta** e uma linha na tabela `level_defs`. Desde a etapa 05 as fases
1–4 são **geradas** a cada partida (`GEN_PARAMS`, ver "Fase gerada" abaixo);
a função fixa vira o **plano B** e a fase 5 (teste) continua fixa. **A ordem da tabela é a
ordem do jogo**; as `num_levels` primeiras são a sequência normal (a fase de
teste fica depois, só para `DEBUG_START_LEVEL 5` e o `./dev bench`).

**1. Parâmetros da floresta e posicionamento:**

```c
static const CLEARING f9_clear[] = { { 16, 16, 5 }, { 47, 47, 6 } };   /* x, z, raio */
static const FOREST_PARAMS f9_forest = {
	64, 64,          /* tamanho (até 128x128) */
	0xf0e57009u,     /* semente: floresta própria (etapa 05: derivada de g.seed) */
	50, 0,           /* árvores por mil células; e no canto nordeste (0 = igual) */
	31, 31,          /* coluna/linha das trilhas */
	f9_clear, NCLEAR(f9_clear),
	6,               /* troncos caídos */
};
static void build_f9(void) {
	build_forest(&f9_forest);
	static const THING t[] = {
		{ 'P', 8, 31 }, { 'X', 55, 31 }, { 'Q', 16, 16 },
		{ 'E', 47, 47 }, { 'H', 20, 31 }, { 'C', 10, 10 },
	};
	PLACE_ALL(t);           /* cada uma na célula livre mais próxima */
}
static const DEBUG_POINT f9_pts[4] = { { 8, 31, 1024 }, { 31, 31, 512 }, { 47, 47, 0 }, { 16, 16, 0 } };
```

`PLACE_ALL` usa `level_place_safe`: a coisa vai para a célula livre mais
próxima (até `PLACE_SEARCH` = 6 células) sem tronco, mata ou tronco caído no
círculo dela; **se não achar, não coloca** (o teste do PC acusa). O jogador 2
nasce ao lado do `P`, dentro do `MAX_PLAYER_SEPARATION`.

| Letra | Significado | Letra | Significado |
|---|---|---|---|
| `P` | início dos jogadores | `C` | caixa destrutível |
| `G` | gema (pontos) | `H` | vida |
| `N` | energia | `W` | arma nova |
| `X` | **saída** da fase | `Q` | **item de missão** |
| `S` | **ponto de reforço** (SURVIVE; em célula livre, nunca na mata) | `L` | **pilha** da lanterna |
| `E` `F` `B` | inimigos: rastejador, caçador, grande (`enemy_defs`) | `1`…`9` | objetos de cenário (`prop_defs`) |
| `K` | **chave** (abre a barreira de raízes; etapa 06) | `V` | feixe de **flechas** (etapa 07) |
| `M` | **balas** (etapa 07) | | |

Ao criar inimigo ou cenário novo, **não use letras reservadas**: `P C G H N
W X Q S L K V M` e as dos inimigos `E F B`.

**2. Acrescente a linha na tabela** (antes da fase de teste) e aumente
`num_levels`:

```c
/* nome
 *   céu=névoa  névoa near/far  luz ambiente  música
 *   objetivo   parâmetro  máx. inimigos  texto do objetivo
 *   função fixa (plano B)  pontos de teleporte  parâmetros do gerador (NULL = fixa) */
{ "PONTE",
  10, 12, 24,  1300, 2800,  40, 40, 56,  0,
  OBJ_COLLECT,  0,  0,  "PEGUE O ITEM E SAIA",
  build_f9, f9_pts, &f9_gen },
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
`#define DEBUG_START_LEVEL 3` e troque o número pela fase (1 = primeira; 5 =
fase de teste).
O jogo novo, ao sair da seleção, começa direto nela. Comente de novo antes
do commit.

Florestas densas e névoa longa custam CPU: meça com `./dev bench` (ele mede
o pior momento de cada fase: todos os inimigos juntos em volta do jogador). **Atalho:** com o overlay L2 aberto, segure **L2** e
aperte **Start** para pular para a próxima fase (levando o progresso).

### Fase gerada (etapa 05)

O gerador ([`src/mapgen.c`](../src/mapgen.c)) monta uma floresta nova a cada
partida a partir de `g.seed` (sorteada na seleção). Cada fase usa a semente
`g.seed ^ ((fase+1) * 0x9E3779B1)`; **a mesma semente dá sempre o mesmo
mapa** (no PC e no console). Vencer gera semente nova para a próxima fase;
**game over recomeça com o mesmo mapa**.

```c
static const GEN_PARAMS f9_gen = {
	64, 64,          /* tamanho em células (até 128x128) */
	GEN_COLLECT,     /* onde ficam início/saída/itens: GEN_HUNT, GEN_CROSS, GEN_COLLECT, GEN_SURVIVE */
	20, 90,          /* árvores por mil células (o ruído varia entre os dois) */
	GEN_CHUNK_CAP,   /* teto de árvores por bloco 8x8 (5) */
	GEN_WINDOW_CAP,  /* teto de "peso" numa janela 3x3 blocos (árvore 1, mata 1/4 por célula) */
	12,              /* teto de ÁRVORES numa janela 3x3 blocos: o que mais pesa na CPU */
	2, 4, 3, 5,      /* clareiras: quantidade mín/máx, raio mín/máx */
	0,               /* GEN_SURVIVE: raio da clareira do cerco */
	6,               /* troncos caídos */
	8, 10,           /* inimigos; distância mínima do início (células) */
	{ 1, 1, 2, 0, 1, 2 },   /* vida, energia, pilha, arma, gema, caixa */
	150,             /* mata alta: limiar do ruído 0..255 (menor = mais mata; 256 = nenhuma) */
	40,              /* paredes: junto das trilhas o limiar é 40 mais baixo (corredores) */
};
```

**Mata alta (terror).** Manchas de mata de 3 m pelo ruído e paredes coladas
nas trilhas, que viram corredores. Uma suavização tira pontas e células
soltas. O que custa desenhar é o **contorno**: cada lado exposto são 4 quads
de 3 m, e o miolo custa o mesmo que o chão. Por isso o gerador **não** enche
bolsões fechados com mata (isso criava tufos soltos entre as árvores);
itens e inimigos só são sorteados em células alcançáveis do início.

**Mapa sempre jogável.** Depois de gerar, uma busca em largura (BFS) a partir
do `P` confere que saída, relíquias, itens e inimigos são alcançáveis (tronco
caído é passável) e que nada ficou em sólido. Se falhar, tenta `semente + 1`
até `MAPGEN_TRIES` (16) vezes; se todas falharem, usa a floresta fixa
(**plano B**). O overlay L2 mostra `MAPA <semente> T<tentativas> CARGA
<ms>` ou `MAPA PLANO B (FIXO)`.

**Testar no PC (sem emulador):**

```bash
./dev mapgen-test 1000            # 1000 sementes por fase: inválidos, tentativas,
                                  # densidade, janela mais densa, tempo, determinismo
./dev mapgen-test 1000 --bench    # idem e grava src/bench_seeds.h (piores sementes)
./dev mapgen-test 50 -D MAPGEN_TRIES=0   # força o plano B em tudo
```

**Ajustar densidade/tamanho:** mude o `GEN_PARAMS` da fase (mata alta:
`GEN_THICKET_F1..F4` e `GEN_WALLS` em `levels.c`). Os tetos por
janela (`GEN_TREES_F1..F4`) foram escolhidos pelo bench: o pior mapa de cada
fase não pode passar do pior caso da 04e. Para testar outro valor **sem
editar**, rode os dois com o mesmo `-D` (as piores sementes dependem do teto):

```bash
./dev mapgen-test 1000 --bench -D GEN_TREES_F4=12
./dev bench -D GEN_TREES_F4=12
git checkout src/bench_seeds.h    # se desistir do valor
```

**Forçar uma semente:** em `config.h`, descomente `#define DEBUG_FIXED_SEED
0x12345678u` (todas as partidas usam essa semente; o overlay L2 mostra
`SEED`). Para reproduzir um mapa visto no jogo, use o `SEED` do overlay.
Comente de novo antes do commit.

### Como criar um evento (etapa 06)

A floresta muda durante a partida ([`src/events.c`](../src/events.c)).
Cada fase tem uma tabela `EVENT_DEF` em `levels.c`: **gatilho → ação**.

```c
static const EVENT_DEF f9_events[] = {
	/* gatilho      ação             prio  p1  p2  arg */
	{ EVT_RANDOM,  0,                1,    45, 90, EV_ALL },   /* a cada 45-90 s, uma ação da máscara */
	{ EVT_PICKUP,  EV_CLOSE_BEHIND,  3,     2,  0, 0 },        /* pegou a 2a relíquia: a mata fecha */
	{ EVT_AREA,    EV_FOG,           3, AREA_EXIT, 24, 50 },   /* a 24 células da saída: névoa a 50% */
	{ EVT_KILLS,   EV_SILENCE,       2,     6,  0, 0 },        /* derrotou 6: silêncio */
	{ EVT_TIME,    EV_WAVE,          2,    90,  0, 3 },        /* aos 90 s: onda de 3 */
	{ EVT_KEY,     EV_OPEN_PATH,     9,     0,  0, 0 },        /* chave na barreira de raízes */
};
/* ... e na linha da fase em level_defs: ..., &f9_gen, EVENTS(f9_events) }, */
```

| Ação | O que faz | `arg` |
|---|---|---|
| `EV_FOG` | a névoa desce até `arg`% em 5 s e volta em ~40 s | % (0 = 55) |
| `EV_TREE_FALL` | um pinheiro à frente do grupo estala (~1 s) e cai atravessado: tronco pulável, copa sólida; a tela treme | — |
| `EV_CLOSE_BEHIND` | pinheiros novos nascem atrás do grupo, fora da visão (respeita os tetos da 05) | árvores (0 = 5) |
| `EV_WAVE` | inimigos fora da visão, até o número do começo da fase | inimigos (0 = 4) |
| `EV_SILENCE` | o vento e os grilos param por 12 s | — |
| `EV_OPEN_PATH` | a chave abre o anel de raízes em volta do prêmio | — |

**Regras (valem para qualquer evento novo):**

- É **lógica**: depende só de `g.in`, do tempo em passos e de `g.rng`.
  Visual (tremor, árvore tombando, névoa desenhada, som) só **lê** `g.ev`.
- Ritmo: nada nos primeiros 30 s; no máximo 1 evento que muda o mapa a cada
  30 s (`EV_GRACE`, `EV_MAP_COOLDOWN`). A chave abre na hora.
- Antes de bloquear células: **checagem de alcance local**
  (`reach_begin`/`reach_step`, janela de 33×33): todas as vizinhas livres
  da mudança continuam ligadas entre si. Se não, o evento tenta outro lugar
  ou não acontece. Nada cai em cima de jogador, inimigo, item, caixa, saída,
  início ou ponto S.
- Mudar só **bits** de colisão (`level_set_flags`) não remonta o bloco;
  mudar o **tipo** da célula remonta em fatias (`level_rebuild_chunk`).
- Busca de lugar anda em passos (orçamento por contagem, não por tempo):
  nenhum quadro passa de ~515 linhas.

**Testar:** `./dev events-test 1000` (simula todas as ações em 1000
sementes por fase: 0 objetivos inalcançáveis, ninguém preso, determinismo)
e `./dev bench` (mede o quadro de cada evento contra um controle sem evento).

### Como o mundo é montado

**Grade.** O mapa é uma grade de até **128×128 células** (`MAP_MAX_W/H`);
1 célula = 1 metro = 256 unidades. Cada célula é **1 byte**: o tipo nos
bits 0–5 e o bit 7 (`CELL_SOLID`) diz se bloqueia. Tipos (`game.h`):

| Tipo | O que é | Sólido | Altura |
|---|---|---|---|
| `CELL_VOID` | nada (sem chão) | sim | — |
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

**Árvores.** Ficam fora do pool de objetos: até `MAX_TREES` (1024), 8
bytes cada, guardadas **por bloco** — o desenho só percorre os blocos do
5×5 em volta do grupo. Por árvore: perto (profundidade da câmera <
`TREE_LOD_DIST`, 1800) desenha o **modelo**; mais longe, a **imagem plana**
(um quad virado para a câmera); depois da névoa, nada. O tronco caído não
tem imagem plana: de longe some. O overlay L2 mostra `ARVORES total
MODELO n PLANA n`.

- Um mapa por código coloca árvores com `level_add_tree(TREE_PINE, x, z,
  ângulo, escala)` (posição no mundo, ângulo 0–4095, escala `ONE` = 1,0),
  **depois** de preencher as células (`level_set_cell` zera a solidez).
- **Colisão:** árvore em pé = **círculo do tronco** (`trunk_r` em
  `tree_defs` × escala), na grade espacial: o jogador contorna suave. Tronco
  caído = 3 células sólidas **baixas** ao longo do comprimento (por isso só
  deitado em X ou Z, ângulo perto de 0/1024/2048/3072): dá para **pular por
  cima** (jogador e inimigos), os tiros param nele e a visão passa.
- **Semente:** a posição das árvores vem de um hash **com semente**. A fase
  de teste usa `FOREST_TEST_SEED` (fixa, para medir sempre no mesmo cenário);
  a geração da etapa 05 deve usar uma semente derivada de `g.seed`.

**Trocar os modelos das árvores pelos seus (Blender).** O jogo procura
nomes fixos:

| Arquivo | Símbolo | O que é |
|---|---|---|
| `models/pinheiro.h` | `pinheiro_mesh` | árvore 1 |
| `models/arvore_seca.h` | `arvore_seca_mesh` | árvore 2 |
| `models/tronco.h` | `tronco_mesh` | tronco caído |
| `assets/textures/pinheiro_bb-4bit.png` | `tex_pinheiro_bb_t` | imagem plana da árvore 1 |
| `assets/textures/seca_bb-4bit.png` | `tex_seca_bb_t` | imagem plana da árvore 2 |

Respeite:
- **Origem no pé** da árvore (centro da base do tronco, no chão, Z = 0 no
  Blender). O jogo coloca a origem na posição da árvore.
- **Escala:** 1 unidade do Blender = 1 metro = 256 no jogo. Atuais: pinheiro
  3,5 m, árvore seca 3 m, tronco 3 m de comprimento. A variação de tamanho
  vem de `scale_min/scale_max` em `tree_defs` (`data.c`).
- **Tronco caído deitado ao longo do eixo X**, centrado na origem (a
  colisão marca a célula do meio e as duas vizinhas em X).
- **Tronco fino o bastante para caber numa célula** (diâmetro < ~0,6 m), porque
  a colisão é a célula inteira.
- **Faces: 20 a 40** (são centenas de árvores; a de perto é a cara).
  Sem textura: a cor de cada face vem do material (o exportador usa a cor
  do material); nomes de material livres.
- **Imagens planas:** PNG 32×64 com transparência (alpha), fundo
  transparente. O quadro representa **2 m × 4 m**, com o pé da árvore no
  meio da borda de baixo. Se mudar o tamanho do quadro, mude `bb_w/bb_h`
  em `tree_defs`. Dica: renderize o seu modelo de lado no Blender
  (ortográfica, fundo transparente) e reduza para 32×64.
- Exporte com `./dev models` (coleção `PSX` do `.blend`) usando esses nomes.
  **Não rode mais** `tools/make_tree_models.py` depois disso: ele sobrescreve
  os cinco arquivos.

**Colisão (grade espacial).** Cada célula sabe quais obstáculos fixos
(troncos, pilares) e quais entidades (jogadores, inimigos) estão nela; os
testes olham só as células vizinhas. Ao escrever código que muda a posição
de alguém **direto** (sem `collide_move`), chame `collide_track(p)` logo
depois — nascer, morrer, teleporte. Para objetos sólidos novos num mapa por
código: `collide_add_obstacle(x, z, raio)` (o raio é limitado para caber
na célula). Linha de visão: `collide_line_of_sight(x0, z0, x1, z1)`.

**Conferir a colisão:** com o overlay aberto, **L2 + Select** mostra, em
volta do jogador 1 (raio `COLMARK_RADIUS`):

| Cor | O quê |
|---|---|
| vermelho (no topo) | parede/mata |
| laranja | caixa (célula) ou tronco em pé (quadrado do tamanho do raio do círculo) |
| amarelo (baixo) | tronco caído (célula baixa) |
| roxo (no chão) | vazio |
| verde/vermelho sobre um inimigo | linha de visão do jogador 1 até ele livre/bloqueada |

Desenho e colisão batem quando cada parede tem o seu quadrado vermelho
exatamente no topo e cada tronco tem o seu quadrado laranja no pé.

**Câmera na mata.** A câmera tem duas partes: o **ângulo** (`g.cam_yaw`,
girado por L1/R1 e analógico direito) é lógica de jogo e fica em `g`, porque
o direcional do jogador gira com ele; **posição, distância e suavização**
são só visuais e ficam em `camera.c` (fora de `g`). Regras:

- **Nunca atravessa troncos:** testa a linha jogador → olho pela grade
  (`collide_camera_clear`); só troncos em pé e o que é mais alto que a
  câmera (a mata de 3 m) contam — inimigos, troncos caídos, paredes baixas
  e caixas ficam por baixo. Começa da distância do quadro anterior (~2
  testes por quadro) e, se o olho suavizado ficar com tronco no caminho, ele
  é puxado na hora.
- **Mais perto na mata densa:** a distância vai de `CAM_DIST` (820) a
  `CAM_DENSE_DIST` (540) conforme as árvores em ~4 células
  (`CAM_DENSE_FULL`), em ~1 s; a altura acompanha.
- **Árvore que tapa o jogador fica a 50%** por ~0,3 s (`TREE_OCCLUDE_TICKS`).
  Funciona porque os modelos de árvore **não têm textura**: em polígonos
  **com** textura o PS1 só mistura os texels cuja cor na CLUT tem o bit STP
  — se você trocar os modelos por versões texturizadas, essa transparência
  precisa de uma segunda CLUT com STP ligado.
- **2 jogadores:** o enquadramento vence a aproximação da mata
  (`CAM_FRAME_FULL`): os dois precisam caber na tela. Eles não podem se
  afastar mais que `MAX_PLAYER_SEPARATION` (896, regra de jogo em
  `player.c`): quem tenta passar do limite não anda naquela direção, mas
  anda de lado ou volta.
- Ao aproximar por causa de um tronco, a câmera **desliza na própria linha
  de visão** (mesmo ângulo) e sobe no máximo `CAM_BLOCK_RAISE`; se subisse
  muito, viraria uma vista de cima que enxerga chão demais (lento).
- **Buffer de primitivas** (`PACKET_LEN`, 96 KB): 16 KB ficam reservados
  para inimigos, jogadores, efeitos e HUD (`PACKET_RESERVE`); blocos e
  árvores são desenhados do mais perto para o mais longe e, se o buffer
  apertar, o mais distante deixa de ser desenhado — o L2 pisca
  `CORTE n!`. Nunca estoura.
- Overlay L2: `CAM testes/células` = custo da câmera no quadro (separado do
  `COL`), `ARV ... TRANSP n` = árvores a 50%, `CPU n GPU n FOLGA n (linhas)`
  = perfil do quadro (ver abaixo).

**Benchmark automático (`./dev bench`).** Mede sem ninguém abrir o jogo:

1. compila com `DEBUG_BENCHMARK` numa pasta separada (`build-bench/`), então
   o build normal nunca sai com o modo ligado;
2. o jogo vai sozinho para a fase 5 (semente fixa) e passa pelos 4 pontos do
   teleporte; em cada um mede ~3 s sem e depois com `DEBUG_SPAWN_COUNT` (8)
   inimigos extras (o jogador fica invulnerável só nesse modo);
3. cada medição sai por `printf` na TTY:
   `BENCH ponto=inicio inimigos_extra=0 ... fps_min=30 fps_med=30 polis_max=506 ... col_max=220 ...`
   e no fim `BENCH FIM`;
4. o `./dev bench` roda uma **cópia portátil** do DuckStation Linux
   (`build-bench/duckstation`, criada na primeira vez a partir de
   `DUCKSTATION_LINUX`), com a TTY no console e velocidade ilimitada; espera
   o `BENCH FIM` (até `BENCH_TIMEOUT` s), fecha o emulador e mostra a tabela.
   Resultado bruto em `build-bench/bench.txt`.

**Variantes:** `./dev bench -D TREE_LOD_DIST=1300 -D FOREST_FOG_FAR=2800`
compila numa pasta à parte (`build-bench-var/`) com essas definições, sem
mexer no `config.h` — para comparar valores antes de mudar o padrão. Dá
para variar tudo que estiver em `#ifndef` (`TREE_LOD_DIST`,
`FOREST_FOG_FAR`, `FOREST_DENSE`, `FOREST_DENSE_NE`...).

**Perfil (colunas do bench, em linhas de vídeo ≈ 63,5 µs; 30 FPS = até ~525
por quadro):** `cpu` = do começo do quadro até entregar a lista à GPU;
`gpu_espera` = tempo esperando a GPU terminar o quadro anterior (ela
trabalha em paralelo; espera grande = gargalo de preenchimento); `logica` =
os passos de `game_tick`; `desenho_blocos/arvores/resto` = partes do
desenho. Se `gpu_espera` ≈ 0 e `cpu` > 525, o gargalo é a CPU. (O
DuckStation estima o tempo da GPU; confirme no console real.)

O FPS vem dos retraços **emulados** (`VSync`), então velocidade ilimitada não
muda o resultado (o benchmark inteiro leva ~5 s). Com semente fixa e sem
controle, ele é **determinístico**: duas execuções dão o mesmo número, e dá
para comparar etapas. São valores do emulador — boa estimativa, não o console.
A sua pasta do DuckStation não é tocada (o comando confere e avisa).

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

### Conferir que a lógica não mudou (`./dev replay-test`)

Toda mudança que **não deveria** mudar o jogo (otimização, limpeza) e toda
mudança na lógica (inimigos, jogador, colisão, eventos, objetivo, gerador)
passa por:

```bash
./dev replay-test            # REPLAY IGUAL / REPLAY DIFERENTE
./dev replay-test --save     # grava a referência nova (só se a mudança foi de propósito)
```

Ele roda uma partida **roteirizada** de 60 s em cada fase (1–5): o jogador
anda em direções que mudam a cada 4 s, atira, pula, usa poder, troca de
arma, liga a lanterna, gira a câmera, e os eventos são forçados em passos
fixos (árvore cai aos 15 s, mata fecha 25 s, onda 35 s, raízes 40 s, névoa
45 s, silêncio 50 s). A cada 10 s imprime o **hash** de `g` mais o mapa
(células, árvores, alcance) e compara com `tools/replay_ref.txt`.

As entradas dependem só do **número do passo**, então o resultado não
depende do FPS: um teste com desenho mais pesado (`-D TREE_LOD_DIST=3000`)
dá IGUAL; uma mudança de lógica (`-D REACH_MAX_VISIT=60`) dá DIFERENTE.
Se der DIFERENTE sem você ter mudado a lógica de propósito, há um bug de
determinismo (algo visual, de tempo ou de VSync entrando na lógica).

O replay não passa por tudo (no build dele o jogador não leva dano, não pega
arma nem quebra caixa, e só joga com o Robô). Por isso há um segundo teste,
com o robô jogando 24 partidas (2 sementes × 4 fases × 3 personagens):

```bash
./dev robo-test           # hash de g no fim de cada partida = tools/robo_ref.txt?
./dev robo-test --save    # mudança de lógica de propósito: grava a nova referência
```

Ao mexer na lógica (inclusive números de `data.c`), rode os dois.

### Jogar com o registro de partida (balanceamento)

```bash
./dev run                 # build com registro + DuckStation Linux (seus controles)
# jogue; FECHE o emulador ao terminar
./dev analisar-partida    # resumo do registro mais novo (logs/partida-AAAAMMDD-HHMM.txt)
./dev run --sem-registro  # o build normal, no emulador do dev.conf
./dev economia 1000       # sem jogar: munição/mana/cura x inimigos de 1000 mapas por fase
```

O registro sai a cada 30 s de jogo e no fim de cada fase (vitória, morte
ou pulo de fase): vida mínima e final, quem te matou, curas, mana por
magia, munição achada/usada/sobrando, flechas recolhidas, golpes por arma,
inimigos mortos por tipo e arma, fugas do grande, itens não pegos e pilha.
Ele não muda a lógica (os contadores ficam fora de `g`).

### O robô joga por você (balanceamento sem jogar)

```bash
./dev robo 20                      # 20 sementes x 4 fases x 3 personagens = 240 partidas (~2 min)
./dev robo 20 -D ROBOT_SKILL=80    # jogador "bom" (padrão 50 = mediano; 0..100)
./dev analisar-partida --robo      # resumo de novo (o mais novo de logs/robo-*.txt)
./dev analisar-partida --detalhe logs/robo-....txt   # + cada partida
```

O robô é um controle: lê o estado do jogo e aperta botões (`src/robot.c`).
Anda pelos caminhos da grade, explora até ver a saída/as relíquias, pega
itens e flechas por perto, luta com a melhor arma (de longe se tiver
munição; contra o GRANDE, bate e recua), cura abaixo de 35% de vida, foge
do grande com a vida baixa, liga a lanterna no escuro. Para não ser
perfeito demais, só vê o que uma pessoa veria, reage com atraso, erra a
mira e às vezes não recua (`ROBOT_SKILL`). Cada partida começa do zero
(sem o que sobrou da fase anterior), como no `./dev economia`, e usa as
mesmas sementes: um mapa apertado na economia pode ser conferido no robô.

Use antes e depois de mexer numa tabela (`data.c`, itens das fases) e
compare: vitórias/mortes por fase e personagem, quem mata, quanto sobra de
munição e mana. O ajuste fino é sempre no teste jogando de verdade.

Limites do robô: não anda agachado (R2), não usa o clarão a não ser contra
o grande, abre caixa com o golpe (ou arco/bola de fogo de longe), não
pula por cima de inimigos.

### Menu, inventário e capturas (etapa 08)

**Criar um item de inventário:** acrescente `IT_<NOME>` no fim do enum em
`game.h`, uma linha em `item_defs` (`data.c`: nome, descrição, quantos por
espaço, efeito, qual `PICK_*` o representa no chão), o efeito em
`inv_use` (`inventory.c`) e um ícone 16x16 em `tools/make_menu_icons.py`
(a folha `icones_menu-4bit.png`: item k fica em x = 16·(k−1)). Para ele
aparecer no mapa, use um `PICK_*` com letra em `level_place` (`level.c`) e
a quantidade por fase em `GEN_PARAMS.items` (`levels.c`; tipos novos
sempre no FIM, para não mudar os outros itens dos mapas).

**O menu para o jogo de verdade:** com ele aberto nenhum `*_update` roda e
nem `g.frame` anda. Prova (abre o menu várias vezes e navega sem usar
nada; tem que dar IGUAL):

```bash
./dev replay-test -D REPLAY_MENU=1
./dev robo-test -D ROBOT_MENU=1
```

**Capturas de tela sem abrir o emulador:**

```bash
./dev capturas    # o robô joga 75 s da f3 e fotografa o jogo e cada aba
```

As imagens ficam em `docs/capturas/08-*.png`. O jogo copia a tela da VRAM
(`StoreImage`) e a manda pelo TTY em hexadecimal; `tools/capturas.py`
monta os PNGs. Funciona igual num console de verdade.

**Desempenho do menu:** `./dev bench -D BENCH_MENU=1` mede cada aba no pior
mapa da f2, com 10 inimigos em volta e o mapa todo visto.

### Floresta viva: céu, pássaros e animais (etapa 09)

Tudo isto é **só visual e som** (`src/ambient.c`): não muda a partida.

| Evento | O que se vê / ouve | Quando |
|---|---|---|
| Céu e lua | degradê do alto (escuro) ao horizonte (claro de luar); a lua de relance (nunca sob o HUD) | sempre |
| Bando no céu | 5–8 pássaros em silhueta cruzam a faixa clara do horizonte (a 6–8 m) **ou** passam rasantes pela ponta iluminada da lanterna | a cada 40–90 s (meio a meio) |
| Bando levanta voo | pássaros saem de uma árvore, com asas e grito | ao chegar perto de uma árvore de pouso ou depois de um barulho forte (ataque, tiro) perto |
| Olhos | um par de olhos no escuro a 5–8 m; some com a lanterna (1 em 3 vezes encara um instante antes) | a cada 25–60 s |
| Vulto | uma figura pálida e alta (~2,4 m), meio dissolvida na névoa, desliza de lado olhando para você, em ~1,2 s | a cada 45–90 s |
| Salto | um bicho de olhos brilhantes pula de um tronco para outro | a cada 35–70 s, se houver 2 árvores perto |
| Galho | só o som de um galho estalando atrás de você | a cada 20–50 s |

Para forçar cada um: L2 duas vezes (página AMBIENTE) e os botões da
tabela de controles. Fotos: `docs/capturas/09-*.png` (`./dev capturas`).

**Criar um evento de ambiente:**

1. Modelo (poucas faces) em `tools/make_ambient_models.py` e som em
   `tools/make_ambient_sounds.py` (ou do Blender/de terceiros, com o
   `CREDITS.md`); rode os dois scripts.
2. Em `ambient.c`: um `AM_<NOME>` no enum, o intervalo em `every_min/max`
   (segundos), o estado numa struct dentro de `am`, a função que começa
   (`start_...`, devolve 0 se não deu: fica esperando), o andar em
   `ambient_update` e o desenho em `ambient_draw`.
3. Regras: sorteios com `fx_range` (nunca `g.rng`), nada escreve em `g`;
   som com `sound_play_at`. Silhueta no céu ou olhos no escuro: `DRAW_NOFOG`
   (+ `DRAW_UNLIT`). Se o evento precisar mudar o jogo (ex.: assustar
   inimigos), ele vira lógica com `g.rng` — converse antes.
4. Um atalho na página AMBIENTE (`ambient_debug`, `debug_input` em
   `main.c`) e, se quiser, uma foto em `robot_shots`.
5. Confira: `./dev replay-test` e `./dev robo-test` IGUAL; `./dev bench`
   (pontos `fN_ambiente` e a linha `BENCH ambiente`: linhas e polígonos).

**Freio:** se o quadro anterior passou de 420 linhas (`AMB_CPU_LIMIT`), o
evento que ia começar **espera** (até 5 s) e aparece quando aliviar. O céu
sempre aparece.

### Memory card, opções e título (etapa 10)

**O que é salvo:** o **começo da fase atual** — fase, semente (o mesmo
mapa), personagens e tudo o que você levou para ela (vida, mana, armas,
munição, pilha, inventário, pontos, estatísticas da campanha) e o volume
dos efeitos. Salva **sozinho ao vencer** cada fase e pela linha **SALVAR
JOGO** da aba OPCOES. **CONTINUAR** no título recomeça essa fase igual.
Save corrompido ou de outra versão: aparece **SAVE INVALIDO** e nada muda.

**Mudar o que vai no save** (um campo novo em `CARRY`, `PSTATS`,
`PROGRESS` ou `SAVE_DATA` no `main.c`): some 1 em `SAVE_VERSION`
(`config.h`) e rode `./dev save-test`. Sem isso, um save antigo seria lido
com os campos no lugar errado.

**Opções:** aba OPCOES do menu. Setas mudam o volume dos efeitos (10
passos; vai no save); X em SALVAR JOGO salva. Ali só L1/R1 trocam de aba.
O volume da música entra com o CD-DA (etapa 01b).

**Créditos:** a tela de créditos é gerada do `CREDITS.md` no build
(`tools/credits_gen.py`). Asset de terceiros novo? Acrescente a linha na
tabela do `CREDITS.md` e ele aparece no jogo. Nome e versão: `GAME_NAME` e
`GAME_VERSION` em `game.h`; logotipo: `tools/make_title.py`.

**Testes desta parte** (sem abrir janela):

```bash
./dev save-test      # salva, bagunça, carrega: hash de g IGUAL; save ruim recusado
./dev maratona 120   # 2 h de jogo com o robô desenhando: travou? pools? memória?
./dev release        # build do zero + dist/mata-escura-v0.9.0.{cue,bin,exe,zip}
```

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

## 8. Criar uma arma de cada tipo

As armas ficam em `weapon_defs` (`data.c`), **na ordem do enum `W_*`**
(`game.h`). O **tipo** decide como ela age (`weapons.c`); a linha só tem
números. Copie uma linha do mesmo tipo:

```c
/*  nome      tipo      munição     magia       corta custo recarga dano alcance arco janela  vel proj aber barulho tamanho  cor            modelo */
/* corpo a corpo: golpe em arco à frente, janela de acerto em passos, silencioso */
{ "LANCA",    WK_MELEE, AMMO_NONE,  MAGIC_NONE, 0, 0, 30, 3, 320, 300, 8, 12,  0, 0, 0,  0, ONE,  { 200,200,210 }, &lanca_mesh },
/* arco: segurar carrega (30% -> 100% de dano e velocidade); 1 flecha por tiro */
{ "BESTA",    WK_BOW,   AMMO_ARROW, MAGIC_NONE, 0, 1, 40, 6,  60,   0, 0,  0, 80, 1, 0,  2, ONE,  { 160,140,100 }, &arco_mesh },
/* magia: gasta mana (custo); a magia é MAGIC_FIRE/FLASH/HEAL */
{ "FOGO MAIOR",WK_MAGIC,AMMO_MANA,  MAGIC_FIRE, 0,40, 50, 8,  45,   0, 0,  0, 30, 1, 0,  8, ONE*2,{ 255,100, 20 }, NULL },
/* arma de fogo: bala por tiro, mira automática curta, MUITO barulho */
{ "REVOLVER", WK_GUN,   AMMO_BULLET,MAGIC_NONE, 0, 1, 24, 6,  40,   0, 0,  0, 70, 1, 0, 18, ONE*2/3,{255,255,120 }, &pistola_mesh },
```

- **alcance**: corpo a corpo = distância do golpe; projétil = passos de vida.
- **arco** (meia abertura do golpe, 4096 = 360°) e **janela** (passos do
  golpe em que ele acerta) só valem no corpo a corpo. **corta** = machado
  (copa de árvore caída, raízes).
- **barulho**: raio em células que os inimigos ouvem (0 = silencioso).
- Acrescente o nome no enum `W_*` (mesma posição) e, se for achável no mapa,
  na lista de `PICK_WEAPON` em `items.c`.
- Modelo na mão: `models/<nome>.h` (Blender, coleção `PSX`, ou
  `tools/make_combat_models.py`); origem na mão, apontando para −Y do
  Blender. Nomes atuais: `espada`, `machado`, `arco`, `pistola`,
  `espingarda`, `flecha`.
- **Rode `./dev replay-test`**: arma nova muda a lógica (regrave com
  `--save` se for de propósito) e meça o ponto `f{N}_combate` no bench.

---

## 9. Magias (os antigos poderes)

Os poderes viraram armas do tipo `WK_MAGIC` (etapa 07): **bola de fogo**
(projétil), **clarão** (atordoa e assusta quem está em volta) e **cura**.
Todos os personagens começam com as três; a **mana** é a munição (poções
`N`, gemas). Uma magia nova de efeito diferente: acrescente um `MAGIC_*` e
um `case` em `magic_cast()` (`weapons.c`), recebendo `PLAYER *p`.

---

## 10. Criar um inimigo com percepção

Em `enemy_defs` (`data.c`), uma linha com uma **letra nova**:

```c
/* nome      letra vida vel dano tamanho pontos modelo         paleta (corpo, sombra, olhos)          visão audição foge_luz pesado */
{ "VULTO",   'U',   5,   9,  12, ONE,     200, &cacador_mesh, { {40,40,50}, {20,20,25}, {255,0,0} },  9,    120,   1,       0 },
```

- **visão** (células) no escuro; com a lanterna do jogador ligada vale o
  dobro, até a névoa. Só vê no cone à frente (se ainda não persegue) e com
  **linha de visão** livre (árvores, mata e troncos tapam).
- **audição** = % do raio de um barulho que ele ouve (tiro 16–22 células,
  arco 3, passo 3, golpe de machado em madeira 8; corpo a corpo e R2 nada).
- **foge_luz** = foge da lanterna apontada para ele; **pesado** = passos
  que se ouvem de longe e só aparece a menos de 6 células.
- Estados (`enemies.c`): vagar → investigar o barulho → perseguir quem viu
  → procurar onde viu por último → vagar. Apanhou = persegue.
- A visão é testada a cada 8 passos por inimigo (escalonada): é o que deixa
  a IA barata. Meça a lógica no bench (`f{N}_combate`) e rode o replay.
- Modelo: `models/<nome>.h` com 3 materiais (corpo, sombra, olhos); mantenha
  ~20–30 faces (cada inimigo é desenhado todo quadro).

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
