/*
 * data.c - Tabelas do jogo: PERSONAGENS, ARMAS, INIMIGOS, SKINS e CENÁRIO
 *
 * Quer uma arma nova? Copie uma linha de weapon_defs do mesmo TIPO (corpo a
 * corpo, arco, magia, fogo) e mude os números (GUIA: "Criar uma arma").
 * Quer um inimigo novo? Adicione uma linha em enemy_defs com uma letra
 * nova e use essa letra no mapa (level.c).
 * (Os antigos "poderes" viraram magias: armas do tipo WK_MAGIC.)
 *
 * Ângulos: 4096 = 360 graus. Cores: 0..255.
 */
#include "game.h"
#include "models.h"

/* ------------------------------------------------------------------ */
/* PERSONAGENS — aparecem na tela de seleção, nesta ordem.             */
/* Para usar um modelo seu: exporte do Blender (ex.: models/heroi.h)   */
/* e adicione uma linha com &heroi_mesh. use_skins = 1 só se o modelo  */
/* tiver os 5 materiais do robô (corpo, cabeça, membros, visor, arma). */
/* ------------------------------------------------------------------ */
const CHARACTER_DEF character_defs[] = {
	/* nome         descrição                      modelo        tamanho skins tex   vida vel pulo arma       arma 2  flechas */
	/* ... mat. arma embutida (some no jogo), mão (x, y, z no espaço do modelo: Y para baixo) */
	{ "ROBO",       "EQUILIBRADO. ESPADA",         &player_mesh, ONE,    1,    NULL, 100, 14, 34,  W_SWORD,   -1,     0,  4, {  87, -159, 26 } },
	{ "CAVALEIRO",  "LENTO, MUITA VIDA, MACHADO",  &knight_mesh, ONE,    0,    NULL, 160, 10, 28,  W_AXE,     -1,     0,  3, { 110, -179, 77 } },
	{ "BATEDOR",    "RAPIDO, ESPADA E ARCO",       &scout_mesh,  ONE,    0,    NULL,  70, 19, 42,  W_SWORD,   W_BOW,  6,  4, {  64, -138, 26 } },
};
const int num_characters = sizeof(character_defs) / sizeof(character_defs[0]);

/* ------------------------------------------------------------------ */
/* ARMAS — a primeira já começa liberada; as outras vêm dos itens "W" */
/* ------------------------------------------------------------------ */
const WEAPON_DEF weapon_defs[] = {
	/* A ORDEM é a do enum W_* (game.h). Números de ESCASSEZ (etapa 07):
	 * corpo a corpo é a base; munição e mana são raras.
	 *  nome           tipo       munição      magia        corta custo recarga dano alcance arco  janela   vel  proj aber barulho tamanho    cor              modelo na mão */
	{ "ESPADA",        WK_MELEE,  AMMO_NONE,   MAGIC_NONE,  0,  0,  22,  2,  240,  700,  5, 10,   0,  0,  0,  0,  ONE,       { 200, 210, 230 }, &espada_mesh },
	{ "MACHADO",       WK_MELEE,  AMMO_NONE,   MAGIC_NONE,  1,  0,  40,  5,  260,  500, 12, 18,   0,  0,  0,  0,  ONE,       { 170, 120,  80 }, &machado_mesh },
	{ "ARCO",          WK_BOW,    AMMO_ARROW,  MAGIC_NONE,  0,  1,  18,  4,   50,    0,  0,  0,  64,  1,  0,  3,  ONE*3/4,   { 220, 190, 120 }, &arco_mesh },
	{ "BOLA DE FOGO",  WK_MAGIC,  AMMO_MANA,   MAGIC_FIRE,  0, 25,  30,  4,   45,    0,  0,  0,  36,  1,  0,  6,  ONE*3/2,   { 255, 140,  40 }, NULL },
	{ "CLARAO",        WK_MAGIC,  AMMO_MANA,   MAGIC_FLASH, 0, 35,  60,  1,  900,    0,  0,  0,   0,  0,  0,  8,  ONE,       { 230, 240, 255 }, NULL },
	{ "CURA",          WK_MAGIC,  AMMO_MANA,   MAGIC_HEAL,  0, 40,  60, 35,  600,    0,  0,  0,   0,  0,  0,  0,  ONE,       {  80, 255, 120 }, NULL },
	{ "PISTOLA",       WK_GUN,    AMMO_BULLET, MAGIC_NONE,  0,  1,  16,  4,   40,    0,  0,  0,  64,  1,  0, 16,  ONE*2/3,   { 255, 255, 120 }, &pistola_mesh },
	{ "ESPINGARDA",    WK_GUN,    AMMO_BULLET, MAGIC_NONE,  0,  1,  44,  3,   22,    0,  0,  0,  52,  5, 90, 22,  ONE*3/4,   { 255, 150,  60 }, &espingarda_mesh },
};
const int num_weapons = sizeof(weapon_defs) / sizeof(weapon_defs[0]);

/* ------------------------------------------------------------------ */
/* INIMIGOS — todos usam o modelo grunt com tamanho/cores diferentes. */
/* Troque .mesh por um modelo seu exportado do Blender!                */
/* Paleta: [0] corpo, [1] chifres, [2] olhos (materiais do modelo)    */
/* ------------------------------------------------------------------ */
const ENEMY_DEF enemy_defs[] = {
	/* Inimigos de TERROR (etapa 07): assustam mais do que enxameiam.
	 *  nome          letra vida vel dano tamanho pontos modelo            paleta (corpo, sombra, olhos)                       visão audição foge_luz pesado */
	{ "RASTEJADOR", 'E',   8,   4,  14, ONE,     100, &rastejador_mesh, { { 110, 120,  95 }, {  50,  55,  45 }, { 255, 220,  80 } },  4,  70, 0, 0 },
	{ "CACADOR",    'F',   3,  11,   9, ONE,     150, &cacador_mesh,    { {  70,  70,  85 }, {  30,  30,  40 }, { 255,  60,  60 } },  7, 100, 1, 0 },
	{ "GRANDE",     'B',  25,   6,  30, ONE,     400, &grande_mesh,     { { 120,  90,  80 }, {  60,  40,  35 }, { 255, 255, 255 } },  5,  60, 0, 1 },
};
const int num_enemy_types = sizeof(enemy_defs) / sizeof(enemy_defs[0]);

/* ------------------------------------------------------------------ */
/* SKINS do jogador (botão SELECT troca). Uma cor por material:        */
/* corpo, cabeça, braços/pernas, visor, arma                           */
/* ------------------------------------------------------------------ */
const SKIN_DEF skin_defs[] = {
	{ "AZUL",     { {  60, 120, 220 }, { 230, 230, 240 }, {  70,  70,  80 }, {  80, 255, 240 }, { 200,  60,  40 } } },
	{ "VERMELHO", { { 210,  50,  50 }, { 240, 220, 200 }, {  60,  50,  50 }, { 255, 230,  80 }, {  60,  60,  60 } } },
	{ "SELVA",    { {  70, 120,  60 }, { 160, 150, 100 }, {  60,  70,  40 }, { 255, 140,  40 }, {  90,  80,  50 } } },
	{ "DOURADO",  { { 230, 180,  50 }, { 250, 240, 200 }, { 120,  90,  40 }, {  90, 200, 255 }, { 250, 250, 250 } } },
	{ "SOMBRA",   { {  40,  40,  50 }, {  70,  70,  80 }, {  25,  25,  30 }, { 255,  30,  30 }, { 120,  20,  20 } } },
};
const int num_skins = sizeof(skin_defs) / sizeof(skin_defs[0]);

/* ------------------------------------------------------------------ */
/* OBJETOS DE CENÁRIO — use o dígito no mapa (level.c).               */
/* Para um modelo seu: exporte para models/meu.h e use &meu_mesh.      */
/* ------------------------------------------------------------------ */
const PROP_DEF prop_defs[] = {
	/* letra  modelo         tamanho   sólido  textura */
	{ '1',   &pillar_mesh,  ONE,      1,      NULL },
	{ '2',   &crate_mesh,   ONE/2,    0,      &tex_crate_t },  /* caixinha decorativa */
};
const int num_props = sizeof(prop_defs) / sizeof(prop_defs[0]);

/* ------------------------------------------------------------------ */
/* SONS — prioridade e variação de tom de cada assets/sounds/*.wav.    */
/* Prioridade: um som novo só corta uma voz de prioridade <= à dele    */
/* quando as 22 vozes de efeito estão ocupadas. Sons fora da tabela:   */
/* prioridade 1, sem variação.                                         */
/* ------------------------------------------------------------------ */
const SOUND_DEF sound_defs[] = {
	/* som            prio  variação de tom (%) */
	{ &sfx_passo1,    1,    8  },
	{ &sfx_passo2,    1,    8  },
	{ &sfx_rosnado,   1,    10 },
	{ &sfx_tiro,      2,    5  },
	{ &sfx_acerto,    2,    10 },
	{ &sfx_item,      2,    0  },
	{ &sfx_dor,       3,    6  },
	{ &sfx_morte,     3,    8  },
	{ &sfx_estalo,    3,    6  },       /* eventos (etapa 06): árvore vai cair */
	{ &sfx_queda,     3,    4  },       /* ... árvore bateu no chão */
	{ &sfx_raizes,    3,    0  },       /* ... barreira de raízes abriu */
	{ NULL }
};

/* ------------------------------------------------------------------ */
/* ÁRVORES — modelos de tools/make_tree_models.py (ou do Blender, com  */
/* os mesmos nomes). De perto: modelo; além de TREE_LOD_DIST: imagem   */
/* plana (bb); depois da névoa: nada. bb_w/bb_h = quadro da imagem no  */
/* mundo (2 x 4 m no script).                                          */
/* ------------------------------------------------------------------ */
const TREE_DEF tree_defs[NUM_TREE_TYPES] = {
	/* modelo            imagem plana          bb_w  bb_h   escala mín/máx          células  tronco copa */
	{ &pinheiro_mesh,    &tex_pinheiro_bb_t,   512, 1024,  ONE * 85 / 100, ONE * 130 / 100, 1,  36,  240 },
	{ &arvore_seca_mesh, &tex_seca_bb_t,       512, 1024,  ONE * 85 / 100, ONE * 120 / 100, 1,  44,  200 },
	{ &tronco_mesh,      NULL,                 0,   0,     ONE * 90 / 100, ONE * 110 / 100, 3,  0,   0   },
	/* árvore que caiu (evento): o pinheiro deitado; a colisão (tronco baixo,
	 * copa sólida) é marcada pelo events.c nas células, sem mudar o tipo */
	{ &pinheiro_mesh,    NULL,                 0,   0,     ONE * 85 / 100, ONE * 130 / 100, 0,  0,   0,  -1024 },
};
