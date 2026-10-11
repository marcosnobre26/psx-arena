/*
 * events_test.c - Testa os eventos da etapa 06 no PC (./dev events-test).
 *
 * Compila o código REAL (mapgen.c, level.c, levels.c, collision.c, reach.c,
 * events.c, rng.c, data.c, mathutil.c); desenho e som viram funções vazias,
 * e inimigos/itens são criados de verdade em g (como no jogo).
 *
 * Para cada fase gerada e N sementes: põe o jogador em células alcançáveis
 * sorteadas, câmera em direção sorteada, e dispara TODAS as ações várias
 * vezes (como o atalho de depuração: sem as regras de ritmo). Depois de cada
 * ação, um BFS COMPLETO (independente da checagem local) confere:
 *   - do jogador E do início: saída, relíquias, itens, inimigos alcançáveis
 *     (o prêmio atrás das raízes só depois de a barreira abrir);
 *   - ninguém (jogador/inimigo) em célula sólida nem dentro de árvore;
 * e conta quantos eventos aconteceram, quantos foram cancelados e por quê
 * (inclusive "a rota sairia da janela de 33x33"). Determinismo: a mesma
 * semente com a mesma sequência é rodada 2 vezes e os resumos comparados.
 * Uso: tools/events_test [N]   (padrão 300)
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "game.h"

GAME g;

/* ---- o que o PC não tem: desenho, som, hardware ---- */
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
void show_message(const char *t, int f) {}
void noise_emit(int x, int z, int cells) {}
void effect_spawn(int t, int x, int z, int r, int d, int cr, int cg, int cb) {}
void sound_play_at(const SOUND *s, int x, int z, int v) {}
int isin(int a) { return (int)lround(sin(a * 2 * M_PI / 4096.0) * 4096); }
int icos(int a) { return (int)lround(cos(a * 2 * M_PI / 4096.0) * 4096); }

/* ---- entidades de verdade em g ---- */
int player_alive(const PLAYER *p) { return p->active && p->hp > 0; }
int enemy_radius(const ENEMY *e) { return (90 * enemy_defs[e->type].scale) >> 12; }
void enemy_spawn(int type, int x, int z) {
	for (int i = 0; i < MAX_ENEMIES; i++) {
		ENEMY *e = &g.enemies[i];
		if (e->active) continue;
		memset(e, 0, sizeof(*e));
		e->active = 1; e->type = type; e->hp = 1;
		e->pos.vx = x; e->pos.vz = z;
		e->angle = rng_next(&g.rng) & 4095;      /* mesmos sorteios do jogo */
		rand_range(30, 120);
		g.enemies_left++;
		collide_track(e);
		return;
	}
}
void pickup_spawn(int type, int x, int z) {
	for (int i = 0; i < MAX_PICKUPS; i++) {
		PICKUP *p = &g.pickups[i];
		if (p->active) continue;
		p->active = 1; p->type = type; p->pos.vx = x; p->pos.vz = z;
		if (type == PICK_QUEST) g.quest_total++;
		return;
	}
}
void crate_spawn(int cx, int cz) {
	for (int i = 0; i < MAX_CRATES; i++)
		if (!g.crates[i].active) {
			g.crates[i].active = 1; g.crates[i].cx = cx; g.crates[i].cz = cz;
			level_set_solid(cx, cz, 1);
			return;
		}
}
void prop_spawn(int d, int cx, int cz) {}

/* ---- checagem COMPLETA (BFS no mapa todo) ---- */
static unsigned char seen[MAP_MAX_H][MAP_MAX_W];
static int qx[MAP_MAX_W * MAP_MAX_H], qz[MAP_MAX_W * MAP_MAX_H];
static int walk(int x, int z) {
	if (x < 0 || z < 0 || x >= level_width() || z >= level_height()) return 0;
	if (level_cell_solid(x, z) && !level_cell_low(x, z)) return 0;
	return !collide_cell_has_obstacle(x, z);
}
static void bfs(int sx, int sz) {
	memset(seen, 0, sizeof(seen));
	if (!walk(sx, sz)) return;
	int h = 0, t = 0;
	seen[sz][sx] = 1; qx[t] = sx; qz[t++] = sz;
	while (h < t) {
		int x = qx[h], z = qz[h++];
		static const int d[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
		for (int k = 0; k < 4; k++) {
			int nx = x + d[k][0], nz = z + d[k][1];
			if (!walk(nx, nz) || seen[nz][nx]) continue;
			seen[nz][nx] = 1; qx[t] = nx; qz[t++] = nz;
		}
	}
}
static int behind_barrier(int cx, int cz) {
	int bx, bz, br;
	if (g.ev.barrier_open || !mapgen_barrier(&bx, &bz, &br)) return 0;
	return (cx - bx) * (cx - bx) + (cz - bz) * (cz - bz) <= (br + 1) * (br + 1);
}
static const char *fail_what;
/* O vetor de árvores continua ordenado por bloco (inserção/remoção locais) */
static int trees_index_ok(void) {
	int x, z, expect = 0;
	for (int bz = 0; bz < MAP_MAX_H / CHUNK_CELLS; bz++)
		for (int bx = 0; bx < MAP_MAX_W / CHUNK_CELLS; bx++) {
			int start, n = level_chunk_trees(bx, bz, &start);
			if (start != expect) return 0;
			for (int i = start; i < start + n; i++) {
				if (level_tree_get(i, &x, &z) < 0) return 0;
				if (x / (CHUNK_CELLS * TILE_SIZE) != bx || z / (CHUNK_CELLS * TILE_SIZE) != bz) return 0;
			}
			expect += n;
		}
	return level_tree_get(expect, &x, &z) < 0;     /* nenhuma sobrando */
}
static int check_all(void) {
	if (!trees_index_ok()) { fail_what = "indice das arvores"; return 0; }
	const PLAYER *p = &g.players[0];
	int px = p->pos.vx / TILE_SIZE, pz = p->pos.vz / TILE_SIZE;
	if (!walk(px, pz)) { fail_what = "jogador em solido/arvore"; return 0; }
	for (int i = 0; i < MAX_ENEMIES; i++)
		if (g.enemies[i].active && !walk(g.enemies[i].pos.vx / TILE_SIZE, g.enemies[i].pos.vz / TILE_SIZE)) {
			fail_what = "inimigo em solido/arvore"; return 0;
		}
	for (int from = 0; from < 2; from++) {
		bfs(from ? g.spawn_x / TILE_SIZE : px, from ? g.spawn_z / TILE_SIZE : pz);
		if (g.has_exit && !seen[g.exit_z / TILE_SIZE][g.exit_x / TILE_SIZE]) { fail_what = "saida"; return 0; }
		for (int i = 0; i < MAX_PICKUPS; i++) {
			const PICKUP *k = &g.pickups[i];
			if (!k->active) continue;
			int cx = k->pos.vx / TILE_SIZE, cz = k->pos.vz / TILE_SIZE;
			if (behind_barrier(cx, cz)) continue;
			if (!seen[cz][cx]) { fail_what = k->type == PICK_QUEST ? "reliquia" : "item"; return 0; }
		}
		for (int i = 0; i < MAX_ENEMIES; i++) {
			const ENEMY *e = &g.enemies[i];
			if (e->active && !seen[e->pos.vz / TILE_SIZE][e->pos.vx / TILE_SIZE]) { fail_what = "inimigo"; return 0; }
		}
	}
	return 1;
}

static uint32_t state_hash(void) {
	uint32_t h = 2166136261u;
	for (int z = 0; z < level_height(); z++)
		for (int x = 0; x < level_width(); x++) {
			int v = level_cell_type(x, z) * 4 + (level_cell_solid(x, z) ? 1 : 0) + (level_cell_low(x, z) ? 2 : 0)
			      + (collide_cell_has_obstacle(x, z) ? 64 : 0) + reach_get(x, z) * 128;
			h = (h ^ (uint32_t)v) * 16777619u;
		}
	int x, z, t;
	for (int i = 0; (t = level_tree_get(i, &x, &z)) >= 0; i++)
		h = (h ^ (uint32_t)(t * 7 + x * 131 + z * 65599)) * 16777619u;
	for (int i = 0; i < MAX_ENEMIES; i++)
		if (g.enemies[i].active) h = (h ^ (uint32_t)(g.enemies[i].pos.vx * 3 + g.enemies[i].pos.vz)) * 16777619u;
	h = (h ^ g.rng.s) * 16777619u;
	return h;
}

/* Jogador numa célula alcançável sorteada (rng próprio do teste) */
static uint32_t trng = 1;
static int trand(int n) { trng = trng * 1103515245u + 12345u; return (int)((trng >> 8) % (uint32_t)n); }
static void place_player(void) {
	PLAYER *p = &g.players[0];
	for (int t = 0; t < 500; t++) {
		int cx = trand(level_width()), cz = trand(level_height());
		if (!reach_get(cx, cz) || !walk(cx, cz)) continue;
		p->pos.vx = cx * TILE_SIZE + TILE_SIZE / 2;
		p->pos.vz = cz * TILE_SIZE + TILE_SIZE / 2;
		collide_track(p);
		break;
	}
	g.cam_yaw = trand(4096);
}

static void load(int lv, uint32_t seed) {
	memset(&g, 0, sizeof(g));
	g.seed = seed;
	rng_seed(&g.rng, seed);
	level_load(lv);
	PLAYER *p = &g.players[0];
	p->active = 1; p->hp = 100; p->index = 0;
	p->pos.vx = g.spawn_x; p->pos.vz = g.spawn_z;
	g.num_players = 1;
	collide_track(p);
	events_start();
	g.ev.keys = 1;                               /* para a barreira abrir */
}

/* Machado (etapa 07): corta todas as árvores caídas registradas e a
 * barreira de raízes, golpe a golpe, como o jogador faria */
#define ACT_CHOP 99
static int chop_all(void) {
	int did = 0;
	for (int k = 0; k < 4; k++)
		while (g.ev.fallen[k].active)
			did |= events_chop(g.ev.fallen[k].cells[0] & 0xff, g.ev.fallen[k].cells[0] >> 8);
	for (int z = 0; z < level_height() && !g.ev.barrier_open; z++)
		for (int x = 0; x < level_width() && !g.ev.barrier_open; x++)
			if (level_cell_type(x, z) == CELL_BARRIER)
				for (int t = 0; t < 10 && !g.ev.barrier_open; t++)
					did |= events_chop(x, z);
	return did;
}

/* Uma ação, como o atalho de depuração; roda os passos até acabar */
static int run_action(int a) {
	if (a == ACT_CHOP) return chop_all();
	int last = g.ev.last;
	g.ev.last = -1;
	if (a == EV_WAVE)                            /* a onda repõe: "mata" 2 antes */
		for (int i = 0, k = 0; i < MAX_ENEMIES && k < 2; i++)
			if (g.enemies[i].active) {
				g.enemies[i].active = 0;
				collide_track(&g.enemies[i]);
				g.enemies_left--;
				k++;
			}
	events_debug(a);
	for (int t = 0; t < 400; t++) {
		events_update();
		if (g.ev.nq == 0 && g.ev.job.action < 0 && g.ev.fall.timer == 0) break;
	}
	int happened = g.ev.last == a;
	if (!happened) g.ev.last = last;
	return happened;
}

static const int seq[] = { EV_TREE_FALL, EV_CLOSE_BEHIND, EV_WAVE, EV_TREE_FALL, EV_OPEN_PATH,
                           EV_CLOSE_BEHIND, EV_FOG, EV_TREE_FALL, EV_SILENCE, EV_CLOSE_BEHIND,
                           EV_TREE_FALL, EV_WAVE, ACT_CHOP };
#define NSEQ ((int)(sizeof(seq) / sizeof(seq[0])))

static int run_seed(int lv, uint32_t seed, int *done, int *tried, int *fails, uint32_t *hash) {
	load(lv, seed);
	trng = seed * 2654435761u + 1;
	for (int i = 0; i < NSEQ; i++) {
		place_player();
		int a = seq[i] == ACT_CHOP ? NUM_EV_ACTIONS : seq[i];
		tried[a]++;
		done[a] += run_action(seq[i]);
		if (!check_all()) {
			if (*fails < 5)
				printf("    FALHA fase %d semente %08x depois de %s: %s inalcancavel/preso\n",
				       lv + 1, seed, seq[i] == ACT_CHOP ? "MACHADO" : event_name(seq[i]), fail_what);
			(*fails)++;
			break;
		}
	}
	*hash = state_hash();
	return 0;
}

int main(int argc, char **argv) {
	int N = argc > 1 ? atoi(argv[1]) : 300, total_fail = 0, total_nondet = 0;
	printf("Eventos: %d sementes por fase, %d acoes por partida (todas as acoes)\n", N, NSEQ);
	for (int lv = 0; lv < num_levels; lv++) {
		if (!level_defs[lv].gen) continue;
		int done[NUM_EV_ACTIONS + 1] = { 0 }, tried[NUM_EV_ACTIONS + 1] = { 0 }, fails = 0, nondet = 0;
		int ok0, cut0, win0, lim0, ok1, cut1, win1, lim1;
		reach_stats(&ok0, &cut0, &win0, &lim0);
		for (int k = 0; k < N; k++) {
			uint32_t seed = 0x2000u + (uint32_t)k * 7919u, h1, h2;
			run_seed(lv, seed, done, tried, &fails, &h1);
			if (k < 50) {                            /* determinismo */
				int d2[NUM_EV_ACTIONS + 1] = { 0 }, t2[NUM_EV_ACTIONS + 1] = { 0 }, f2 = 0;
				run_seed(lv, seed, d2, t2, &f2, &h2);
				if (h1 != h2) nondet++;
			}
		}
		reach_stats(&ok1, &cut1, &win1, &lim1);
		printf("fase %d %-10s  falhas (objetivo inalcancavel/preso) %d  nao-deterministicos %d\n",
		       lv + 1, level_defs[lv].name, fails, nondet);
		printf("        aconteceram:");
		for (int a = 0; a < NUM_EV_ACTIONS; a++)
			printf("  %s %d/%d", event_name(a), done[a], tried[a]);
		printf("  MACHADO %d/%d", done[NUM_EV_ACTIONS], tried[NUM_EV_ACTIONS]);
		printf("\n        checagens locais: ok %d  corta %d  janela %d  teto de visitas %d\n",
		       ok1 - ok0, cut1 - cut0, win1 - win0, lim1 - lim0);
		total_fail += fails;
		total_nondet += nondet;
	}
	printf(total_fail || total_nondet ? "ATENCAO: ha falhas acima\n"
	                                  : "TUDO CERTO: 0 objetivos inalcancaveis, ninguem preso, deterministico\n");
	return total_fail || total_nondet;
}
