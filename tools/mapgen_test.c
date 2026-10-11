/*
 * mapgen_test.c - Testa o gerador de florestas no PC (./dev mapgen-test).
 *
 * Compila o código REAL do jogo (mapgen.c, level.c, levels.c, collision.c,
 * rng.c, data.c) para o PC; o que é desenho/hardware vira função vazia
 * aqui. Para cada fase gerada e N sementes:
 *   - mapas inválidos depois das novas tentativas (plano B) e tentativas;
 *   - checagem INDEPENDENTE: tudo alcançável a partir do início (tronco caído
 *     passável) e nada em sólido;
 *   - mesma semente -> mesmo mapa (gera duas vezes e compara o resumo);
 *   - densidade (árvores por mil células) mínima/média/máxima, maior bloco,
 *     janela 3x3 mais pesada; tempo de geração no PC (referência);
 *   - as 5 sementes com a PIOR janela de cada fase (para o ./dev bench).
 * Uso: tools/mapgen_test [N]   (padrão 1000)
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
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
static VECTOR campos;
const VECTOR *render_camera_pos(void) { return &campos; }
void prof_begin(int s) {}
void prof_end(int s) {}
/* ---- entidades: só contamos (o mapa já registra o que importa) ---- */
static int n_enemies, n_pickups, n_crates;
void enemy_spawn(int t, int x, int z) { n_enemies++; rng_next(&g.rng); rng_next(&g.rng); }
void pickup_spawn(int t, int x, int z) { n_pickups++; }
void crate_spawn(int cx, int cz) { n_crates++; level_set_solid(cx, cz, 1); }
void prop_spawn(int d, int cx, int cz) {}
int  player_alive(const PLAYER *p) { return p->active && p->hp > 0; }
int  enemy_radius(const ENEMY *e) { return (90 * enemy_defs[e->type].scale) >> 12; }

/* ---- checagem independente ---- */
static unsigned char seen[MAP_MAX_H][MAP_MAX_W];
static int qx[MAP_MAX_W * MAP_MAX_H], qz[MAP_MAX_W * MAP_MAX_H];
static int walk(int x, int z) {
	if (x < 0 || z < 0 || x >= level_width() || z >= level_height()) return 0;
	if (level_cell_solid(x, z) && !level_cell_low(x, z)) return 0;
	return !collide_cell_has_obstacle(x, z);
}
static int check_map(int lv, uint32_t seed) {
	memset(seen, 0, sizeof(seen));
	int sx = g.spawn_x / 256, sz = g.spawn_z / 256, h = 0, t = 0, bad = 0;
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
	char ch; int cx, cz;
	int n = level_placed(0, &ch, &cx, &cz);
	for (int i = 0; i < n; i++) {
		level_placed(i, &ch, &cx, &cz);
		if (ch == 'C') continue;
		if (!seen[cz][cx] || level_cell_type(cx, cz) == CELL_THICKET) {
			if (bad < 3) printf("    PROBLEMA fase %d semente %08x: '%c' em (%d,%d)\n", lv + 1, seed, ch, cx, cz);
			bad++;
		}
	}
	return bad;
}
static uint32_t grid_hash(void) {
	uint32_t h = 2166136261u;
	for (int z = 0; z < level_height(); z++)
		for (int x = 0; x < level_width(); x++) {
			int v = level_cell_type(x, z) * 4 + (level_cell_solid(x, z) ? 1 : 0) + (level_cell_low(x, z) ? 2 : 0)
			      + (collide_cell_has_obstacle(x, z) ? 64 : 0);
			h = (h ^ (uint32_t)v) * 16777619u;
		}
	char ch; int cx, cz, n = level_placed(0, &ch, &cx, &cz);
	for (int i = 0; i < n; i++) { level_placed(i, &ch, &cx, &cz); h = (h ^ (uint32_t)(ch * 65536 + cz * 256 + cx)) * 16777619u; }
	return h;
}
static void load(int lv, uint32_t seed) {
	memset(&g, 0, sizeof(g));
	g.seed = seed;
	rng_seed(&g.rng, seed);
	n_enemies = n_pickups = n_crates = 0;
	level_load(lv);
}

int main(int argc, char **argv) {
	int N = argc > 1 ? atoi(argv[1]) : 1000;
	int total_problems = 0;
	printf("Gerador de florestas: %d sementes por fase (MAPGEN_TRIES = %d)\n", N, MAPGEN_TRIES);
	for (int lv = 0; lv < num_levels; lv++) {
		if (!level_defs[lv].gen) continue;
		int fallback = 0, problems = 0, nondet = 0, max_tries = 0, tries_hist[5] = { 0 };
		int dmin = 1 << 30, dmax = 0, dsum = 0, cmax = 0, wmax = 0, wsum = 0;
		double tsum = 0, tmax = 0;
		struct { int w; uint32_t s; } top[5] = { { -1, 0 } };
		for (int i = 0; i < 5; i++) top[i].w = -1;
		for (int k = 0; k < N; k++) {
			uint32_t seed = 0x1000u + (uint32_t)k * 7919u;
			clock_t c0 = clock();
			load(lv, seed);
			double ms = (double)(clock() - c0) * 1000.0 / CLOCKS_PER_SEC;
			tsum += ms; if (ms > tmax) tmax = ms;
			const MAPGEN_STATS *st = mapgen_stats();
			fallback += st->fallback;
			if (st->tries > max_tries) max_tries = st->tries;
			tries_hist[st->tries > 4 ? 4 : st->tries - 1]++;
			problems += check_map(lv, seed);
			if (!st->fallback) {
				if (st->dens_permil < dmin) dmin = st->dens_permil;
				if (st->dens_permil > dmax) dmax = st->dens_permil;
				dsum += st->dens_permil;
				if (st->max_chunk > cmax) cmax = st->max_chunk;
				if (st->win_weight > wmax) wmax = st->win_weight;
				wsum += st->win_weight;
				for (int i = 0; i < 5; i++)                 /* 5 piores janelas */
					if (st->win_weight > top[i].w) {
						for (int j = 4; j > i; j--) top[j] = top[j - 1];
						top[i].w = st->win_weight; top[i].s = seed;
						break;
					}
			}
			uint32_t h1 = grid_hash();
			if (k < 200) {                                  /* mesma semente -> mesmo mapa */
				load(lv, seed);
				if (grid_hash() != h1) nondet++;
			}
		}
		int ok = N - fallback;
		printf("fase %d %-10s %3dx%-3d  invalidos(plano B) %d  tentativas 1:%d 2:%d 3:%d 4+:%d (max %d)  problemas %d  nao-deterministicos %d\n",
		       lv + 1, level_defs[lv].name, level_defs[lv].gen->w, level_defs[lv].gen->h, fallback,
		       tries_hist[0], tries_hist[1], tries_hist[2], tries_hist[3] + tries_hist[4], max_tries, problems, nondet);
		printf("        densidade (arvores/mil) min %d med %d max %d   maior bloco %d arvores   janela 3x3 (arvores eq.) max %d med %d\n",
		       dmin, ok ? dsum / ok : 0, dmax, cmax, wmax / 4, ok ? wsum / ok / 4 : 0);
		printf("        tempo no PC: med %.2f ms max %.2f ms (referencia; o do console vem do ./dev bench)\n", tsum / N, tmax);
		printf("        5 piores janelas (para o bench): ");
		for (int i = 0; i < 5; i++) printf("0x%08x(%d) ", top[i].s, top[i].w / 4);
		printf("\n");
		total_problems += problems + nondet + fallback;
	}
	printf("%s\n", total_problems ? "ATENCAO: ha problemas acima" : "TUDO CERTO: 0 invalidos, 0 problemas, deterministico");
	return 0;
}
