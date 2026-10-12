/*
 * economy_test.c - Economia de recursos por fase, no PC (./dev economia).
 *
 * Gera os mapas REAIS (mapgen.c etc.) de N sementes por fase e compara,
 * para 1 jogador e cada personagem inicial:
 *   DANO disponível  = flechas + balas + mana (bola de fogo) + corpo a corpo
 *                      num "tempo razoável"
 *   contra a VIDA total dos inimigos (fase 4: + reforços em 60 s);
 *   CURA disponível  = itens de vida + o que caixas e inimigos soltam
 *   contra o DANO esperado (cuidadoso: cada inimigo acerta 1 vez; médio: 2).
 * Mostra mínimo/média/máximo por fase e as sementes mais apertadas.
 *
 * Números do JOGO vêm das tabelas (weapon_defs, enemy_defs, config.h);
 * as PREMISSAS (aprovadas pelo Marcos, etapa 07b) estão logo abaixo.
 */
#include <stdio.h>
#include <string.h>
#include "game.h"

/* ---- premissas ---- */
static const int PHASE_SECONDS[4] = { 6 * 60, 8 * 60, 6 * 60, 60 };  /* tempo-alvo de cada fase */
#define MELEE_TIME_PCT   40      /* corpo a corpo ocupa até 40% do tempo */
#define MELEE_HIT_PCT    50      /* e acerta 50% dos golpes */
#define RANGED_HIT_PCT   70      /* flechas, balas, bola de fogo: 70% acertam */
#define ARROW_CHARGE_PCT 75      /* flecha média com 75% de carga */
#define DROP_KILL_PCT    100     /* % dos inimigos mortos (para os itens soltos) */

GAME g;
/* ---- o que o PC não tem ---- */
void render_set_fog(int a, int b, int c, int d, int e) {}
void render_set_light(int a, int b, int c, int d, int e, int f) {}
void render_chunk_setup(const SVECTOR *v, const TEXTURE *const *t, int n) {}
void render_bake_light(const SVECTOR *n, int r, int gg, int b, CVECTOR *o) {}
void render_chunk(const CHUNK_GEOM *c) {}
int  render_mesh(const MESH *m, const VECTOR *p, const SVECTOR *r, int s, const DRAWOPT *o) { return 1; }
int  render_billboard(int x, int z, int w, int h, const TEXTURE *t, const CVECTOR *c) { return 1; }
int  render_view_depth(int x, int y, int z) { return 0; }
int  render_bytes_used(void) { return 0; }
void render_stats_time(int *a, int *b, int *c) { *a = *b = *c = 0; }
static VECTOR campos;
const VECTOR *render_camera_pos(void) { return &campos; }
void prof_begin(int s) {}
void prof_end(int s) {}
void enemy_spawn(int t, int x, int z) { rng_next(&g.rng); rng_next(&g.rng); }
int  pickup_spawn(int t, int x, int z) { return 1; }
void crate_spawn(int cx, int cz) { level_set_solid(cx, cz, 1); }
void prop_spawn(int d, int cx, int cz) {}
int  player_alive(const PLAYER *p) { return p->active && p->hp > 0; }
int  enemy_radius(const ENEMY *e) { return (90 * enemy_defs[e->type].scale) >> 12; }

/* Um mapa: quantos de cada letra */
typedef struct { int n[128]; } COUNT;
static void count_map(COUNT *c) {
	memset(c, 0, sizeof(*c));
	char ch; int cx, cz, n = level_placed(0, &ch, &cx, &cz);
	for (int i = 0; i < n; i++) {
		level_placed(i, &ch, &cx, &cz);
		c->n[(unsigned char)ch]++;
	}
}

static int enemy_type_of(char ch) {
	for (int t = 0; t < num_enemy_types; t++)
		if (enemy_defs[t].map_char == ch) return t;
	return -1;
}

typedef struct { double avail, need, ratio, heal_margin1, heal_margin2, ranged_ratio; } RESULT;

/* Economia de um mapa para um personagem */
static RESULT economy(int lv, const COUNT *c, int character) {
	const CHARACTER_DEF *cd = &character_defs[character];
	RESULT r;
	int secs = PHASE_SECONDS[lv];

	/* inimigos: vida e dano por contato */
	double hp = 0, dmg1 = 0, n_enemies = 0;
	for (int ch = 0; ch < 128; ch++) {
		int t = enemy_type_of((char)ch);
		if (t < 0 || !c->n[ch]) continue;
		hp += c->n[ch] * enemy_defs[t].hp;
		dmg1 += c->n[ch] * enemy_defs[t].damage;
		n_enemies += c->n[ch];
	}
	if (level_defs[lv].objective == OBJ_SURVIVE) {     /* reforços em 60 s, tipo ao acaso */
		double extra = (double)level_defs[lv].obj_param * 60 / REINFORCE_TIME;
		double avg_hp = 0, avg_dmg = 0;
		for (int t = 0; t < num_enemy_types; t++) { avg_hp += enemy_defs[t].hp; avg_dmg += enemy_defs[t].damage; }
		avg_hp /= num_enemy_types; avg_dmg /= num_enemy_types;
		hp += extra * avg_hp; dmg1 += extra * avg_dmg; n_enemies += extra;
	}

	/* munição: inicial + mapa + W (na ordem arco, machado, pistola, espingarda) + soltos */
	uint32_t owned = (1u << cd->weapon) | (cd->weapon2 >= 0 ? 1u << cd->weapon2 : 0);
	double arrows = cd->arrows + c->n['V'] * ARROW_PICKUP;
	double bullets = c->n['M'] * BULLET_PICKUP;
	static const int order[4] = { W_BOW, W_AXE, W_PISTOL, W_SHOTGUN };
	for (int k = 0; k < c->n['W']; k++) {
		int w = -1;
		for (int j = 0; j < 4 && w < 0; j++)
			if (!(owned & (1u << order[j]))) w = order[j];
		if (w >= 0) owned |= 1u << w;
		if (w == W_BOW || w < 0) arrows += 4;
		if (w == W_PISTOL || w == W_SHOTGUN || w < 0) bullets += 3;
	}
	double kills = n_enemies * DROP_KILL_PCT / 100.0;
	arrows += kills * 0.06 * ARROW_PICKUP;               /* 6% soltam flechas */
	if (!(owned & (1u << W_BOW))) arrows = 0;            /* sem arco, flecha não serve */
	if (!(owned & ((1u << W_PISTOL) | (1u << W_SHOTGUN)))) bullets = 0;

	/* mana: inicial + poções + gemas + soltos (inimigos 8%, caixas 35%) + volta lenta */
	const int pot = item_defs[IT_POTION].amount;           /* N = poção de mana (etapa 08) */
	double mana = PLAYER_MAX_ENERGY / 2 + c->n['N'] * pot + c->n['G'] * 15
	            + kills * 0.08 * pot + c->n['C'] * 0.35 * pot
	            + (double)secs * 60 / ENERGY_REGEN_DELAY * 0.5;   /* metade do tempo abaixo de 25% */

	/* dano à distância */
	const WEAPON_DEF *bow = &weapon_defs[W_BOW], *fire = &weapon_defs[W_FIREBALL];
	const WEAPON_DEF *gun = &weapon_defs[(owned & (1u << W_SHOTGUN)) ? W_SHOTGUN : W_PISTOL];
	double arrow_dmg = bow->damage * ARROW_CHARGE_PCT / 100.0;
	double bullet_dmg = gun->damage * gun->pellets * (gun->pellets > 1 ? 0.6 : 1.0);   /* leque: 60% dos chumbos */
	double ranged = (arrows * arrow_dmg + bullets * bullet_dmg + mana / fire->cost * fire->damage)
	              * RANGED_HIT_PCT / 100.0;

	/* corpo a corpo no tempo razoável (a melhor arma de corpo a corpo que tem) */
	const WEAPON_DEF *mw = &weapon_defs[(owned & (1u << W_AXE)) ? W_AXE : W_SWORD];
	double melee = (double)secs * MELEE_TIME_PCT / 100.0 * 60.0 / mw->cooldown
	             * mw->damage * MELEE_HIT_PCT / 100.0;

	r.avail = ranged + melee;
	r.need = hp;
	r.ratio = hp > 0 ? r.avail / hp : 99;
	r.ranged_ratio = hp > 0 ? ranged / hp : 99;

	/* cura: itens de vida + soltos (inimigos 12%, caixas 35%) contra o dano esperado */
	/* etapa 08: H = bandagem, J = kit médico; os soltos de inimigo/caixa são bandagens */
	const int band = item_defs[IT_BANDAGE].amount, kit = item_defs[IT_MEDKIT].amount;
	double heal = c->n['H'] * band + c->n['J'] * kit + kills * 0.12 * band + c->n['C'] * 0.35 * band;
	double life = cd->max_hp + heal;
	r.heal_margin1 = life - dmg1;          /* cuidadoso: cada inimigo acerta 1 vez */
	r.heal_margin2 = life - 2 * dmg1;      /* médio: 2 vezes */
	return r;
}

static void load(int lv, uint32_t seed) {
	memset(&g, 0, sizeof(g));
	g.seed = seed;
	rng_seed(&g.rng, seed);
	level_load(lv);
}

int main(int argc, char **argv) {
	int N = argc > 1 ? atoi(argv[1]) : 1000;
	printf("Economia de recursos: %d sementes por fase, 1 jogador\n", N);
	printf("Premissas: corpo a corpo %d%% do tempo com %d%% de acerto; distância %d%% de acerto;\n"
	       "           flecha com %d%% de carga; tempo-alvo f1 %d s, f2 %d s, f3 %d s, f4 %d s\n",
	       MELEE_TIME_PCT, MELEE_HIT_PCT, RANGED_HIT_PCT, ARROW_CHARGE_PCT,
	       PHASE_SECONDS[0], PHASE_SECONDS[1], PHASE_SECONDS[2], PHASE_SECONDS[3]);
	printf("dano/vida = dano disponível / vida total dos inimigos (>1 = sobra)\n"
	       "distancia = só munição e mana (sem corpo a corpo)\n"
	       "cura = vida máxima + curas - dano esperado (cuidadoso 1 acerto / médio 2 por inimigo)\n\n");
	static COUNT counts[4][2000];
	static uint32_t seeds[4][2000];
	if (N > 2000) N = 2000;
	for (int lv = 0; lv < num_levels; lv++)
		for (int k = 0; k < N; k++) {
			seeds[lv][k] = 0x3000u + (uint32_t)k * 7919u;
			load(lv, seeds[lv][k]);
			count_map(&counts[lv][k]);
		}
	for (int ch = 0; ch < num_characters; ch++) {
		printf("== %s ==\n", character_defs[ch].name);
		printf("%-14s %-17s %-17s %-19s %-19s\n", "fase", "dano/vida", "distancia",
		       "cura cuidadoso", "cura medio");
		printf("%-14s %-17s %-17s %-19s %-19s\n", "", "min  med  max", "min  med  max",
		       "min   med   max", "min   med   max");
		for (int lv = 0; lv < num_levels; lv++) {
			double mn[4] = { 1e9, 1e9, 1e9, 1e9 }, mx[4] = { -1e9, -1e9, -1e9, -1e9 }, sm[4] = { 0 };
			struct { double v; uint32_t s; } tight[5];
			for (int i = 0; i < 5; i++) tight[i].v = 1e9;
			for (int k = 0; k < N; k++) {
				RESULT r = economy(lv, &counts[lv][k], ch);
				double v[4] = { r.ratio, r.ranged_ratio, r.heal_margin1, r.heal_margin2 };
				for (int j = 0; j < 4; j++) {
					if (v[j] < mn[j]) mn[j] = v[j];
					if (v[j] > mx[j]) mx[j] = v[j];
					sm[j] += v[j];
				}
				for (int i = 0; i < 5; i++)                 /* as 5 mais apertadas (dano/vida) */
					if (r.ratio < tight[i].v) {
						for (int j = 4; j > i; j--) tight[j] = tight[j - 1];
						tight[i].v = r.ratio; tight[i].s = seeds[lv][k];
						break;
					}
			}
			printf("f%d %-11s %4.1f %4.1f %4.1f   %4.1f %4.1f %4.1f   %5.0f %5.0f %5.0f   %5.0f %5.0f %5.0f\n",
			       lv + 1, level_defs[lv].name, mn[0], sm[0] / N, mx[0], mn[1], sm[1] / N, mx[1],
			       mn[2], sm[2] / N, mx[2], mn[3], sm[3] / N, mx[3]);
			printf("   mais apertadas:");
			for (int i = 0; i < 5; i++) printf(" %08x(%.2f)", tight[i].s, tight[i].v);
			printf("\n");
		}
		printf("\n");
	}
	return 0;
}
