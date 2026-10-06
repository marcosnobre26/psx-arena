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
3. [Editar o mapa](#3-editar-o-mapa)
4. [Modelar no Blender](#4-modelar-no-blender)
5. [Novo personagem jogável](#5-novo-personagem-jogável)
6. [Texturas](#6-texturas)
7. [Objetos de cenário](#7-objetos-de-cenário)
8. [Nova arma](#8-nova-arma)
9. [Novo poder](#9-novo-poder)
10. [Novo inimigo](#10-novo-inimigo)
11. [Nova skin](#11-nova-skin)
12. [Emuladores](#12-emuladores)
13. [Gravar em CD e jogar no console](#13-gravar-em-cd-e-jogar-no-console)
14. [Roteiro de estudo](#14-roteiro-de-estudo)

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
| **Select** | trocar a skin (personagens com skins) |
| **Start** | pausar |
| **L2** | depuração: FPS, memória de primitivas, posição |

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

## 3. Editar o mapa

O mapa é texto, em [`src/level.c`](../src/level.c):

```c
"########################",
"#P.......#......C......#",
"#........#..E.......G..#",
```

| Caractere | Significado | Caractere | Significado |
|---|---|---|---|
| `#` | parede | `.` | chão |
| `P` | início dos jogadores | `C` | caixa destrutível |
| `G` | gema | `H` | vida |
| `N` | energia | `W` | arma nova |
| `E` `B` `F` | inimigos (`enemy_defs`) | espaço | vazio |
| `1`…`9` | objetos de cenário (`prop_defs`) | | |

Regras: todas as linhas com o mesmo tamanho, máximo 32×32. Chão, paredes
(com 2 "andares" de textura) e colisão são gerados a partir do texto. O
segundo jogador nasce na primeira célula livre ao lado do `P`.

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

## 12. Emuladores

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

## 13. Gravar em CD e jogar no console

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

## 14. Roteiro de estudo

Do mais fácil ao mais difícil:

1. Mudar números no `config.h` e nos atributos dos personagens.
2. Redesenhar o mapa.
3. Criar uma arma, uma skin e um poder.
4. Modelar um personagem no Blender e colocá-lo na seleção.
5. Fazer uma textura e aplicar num modelo com UV.
6. Um inimigo que **atira** (reaproveite `weapons.c` com um campo "dono").
7. Uma **segunda fase** (novo mapa em `level_maps` + `level_load(1)` ao vencer).
8. **Som**: biblioteca `psxspu` do PSn00bSDK (exemplos em `examples/sound`)
   ou música CD-DA no `iso.xml`.
9. Carregar modelos e texturas **do CD** (`psxcd`) em vez de embutir no executável.
10. **Animação** de personagem: quadros de vértices ou partes separadas.
11. Recorde no **memory card**.
12. Modo **link** pela porta serial (veja "Rede" no README).
