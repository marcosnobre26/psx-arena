/*
 * data.c - Tabelas do jogo: PERSONAGENS, ARMAS, PODERES, INIMIGOS, SKINS e CENÁRIO
 *
 * Quer uma arma nova? Copie uma linha de weapon_defs e mude os números.
 * Quer um inimigo novo? Adicione uma linha em enemy_defs com uma letra
 * nova e use essa letra no mapa (level.c).
 * Quer um poder novo? Escreva a função em powers.c e adicione aqui.
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
	/* nome         descrição                      modelo        tamanho skins tex   vida vel pulo arma */
	{ "ROBO",       "EQUILIBRADO. TROCA DE SKIN",  &player_mesh, ONE,    1,    NULL, 100, 14, 34,  0 },
	{ "CAVALEIRO",  "LENTO, MUITA VIDA, CANHAO",   &knight_mesh, ONE,    0,    NULL, 160, 10, 28,  3 },
	{ "BATEDOR",    "RAPIDO, POUCA VIDA, METRALHA", &scout_mesh, ONE,    0,    NULL,  70, 19, 42,  2 },
};
const int num_characters = sizeof(character_defs) / sizeof(character_defs[0]);

/* ------------------------------------------------------------------ */
/* ARMAS — a primeira já começa liberada; as outras vêm dos itens "W" */
/* ------------------------------------------------------------------ */
const WEAPON_DEF weapon_defs[] = {
	/* nome            recarga vel  dano proj abert. impr. vida  tamanho    cor */
	{ "BLASTER",          10,   44,   1,   1,    0,    0,   50, ONE,       { 255, 255, 120 } },
	{ "ESPINGARDA",       32,   38,   1,   5,   90,   20,   20, ONE*3/4,   { 255, 150,  60 } },
	{ "METRALHADORA",      4,   52,   1,   1,    0,   70,   40, ONE*2/3,   { 120, 255, 255 } },
	{ "CANHAO",           50,   26,   8,   1,    0,    0,   90, ONE*5/2,   { 255,  80, 255 } },
};
const int num_weapons = sizeof(weapon_defs) / sizeof(weapon_defs[0]);

/* ------------------------------------------------------------------ */
/* PODERES — funções implementadas em powers.c                         */
/* ------------------------------------------------------------------ */
void power_shockwave(PLAYER *p);
void power_dash(PLAYER *p);
void power_heal(PLAYER *p);

const POWER_DEF power_defs[] = {
	/* nome               custo  função */
	{ "ONDA DE CHOQUE",     40,  power_shockwave },
	{ "DASH",               20,  power_dash },
	{ "CURA",               50,  power_heal },
};
const int num_powers = sizeof(power_defs) / sizeof(power_defs[0]);

/* ------------------------------------------------------------------ */
/* INIMIGOS — todos usam o modelo grunt com tamanho/cores diferentes. */
/* Troque .mesh por um modelo seu exportado do Blender!                */
/* Paleta: [0] corpo, [1] chifres, [2] olhos (materiais do modelo)    */
/* ------------------------------------------------------------------ */
const ENEMY_DEF enemy_defs[] = {
	/* nome     letra  vida vel dano tamanho  pontos modelo        paleta */
	{ "GRUNT",   'E',   3,   7,  10, ONE,      100, &grunt_mesh, { { 210,  50,  60 }, {  90,  20,  40 }, { 255, 230,  60 } } },
	{ "BRUTO",   'B',  10,   4,  25, ONE*3/2,  300, &grunt_mesh, { { 130,  60, 200 }, {  40,  20,  70 }, { 255,  80,  80 } } },
	{ "VELOZ",   'F',   2,  12,   8, ONE*3/4,  150, &grunt_mesh, { {  60, 200,  80 }, {  20,  70,  30 }, { 255, 255, 255 } } },
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
