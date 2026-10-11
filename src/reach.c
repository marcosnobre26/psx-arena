/*
 * reach.c - Alcance durante a partida (etapa 06)
 *
 * 1) Mapa de ALCANCE: 1 bit por célula, "dá para chegar aqui a partir do
 *    início". Gravado na carga a partir da inundação do gerador (mapgen.c),
 *    também nas florestas fixas. Os eventos o mantêm: célula que fica
 *    bloqueada perde o bit; a barreira de raízes aberta soma a região de trás.
 *    Inimigos de onda e árvores novas só usam células com o bit.
 *
 * 2) Checagem LOCAL antes de um evento bloquear células: todas as células
 *    livres vizinhas da mudança precisam continuar ligadas ENTRE SI por um
 *    caminho dentro de uma janela de REACH_WIN x REACH_WIN em volta. Se
 *    continuam, nenhum caminho do mapa se perde (quem passava pelas células
 *    bloqueadas dá a volta), então jogador, saída, relíquias e itens seguem
 *    alcançáveis. É a mesma regra de "andável" do BFS da 05 (tronco caído
 *    passa; tronco em pé, mata e sólido bloqueiam). Custa ~1 000 células em
 *    vez das 16 384 do mapa: cabe num passo de lógica.
 *    Se o caminho precisaria sair da janela, a resposta é "não sei" e o
 *    evento não acontece ali (conservador).
 */
#include <string.h>
#include "game.h"

static uint8_t reach_bits[MAP_MAX_H][MAP_MAX_W / 8];

void reach_clear(void) {
	memset(reach_bits, 0, sizeof(reach_bits));
}

void reach_set(int cx, int cz, int on) {
	if (cx < 0 || cz < 0 || cx >= MAP_MAX_W || cz >= MAP_MAX_H) return;
	if (on) reach_bits[cz][cx >> 3] |= 1 << (cx & 7);
	else    reach_bits[cz][cx >> 3] &= ~(1 << (cx & 7));
}

int reach_get(int cx, int cz) {
	if (cx < 0 || cz < 0 || cx >= MAP_MAX_W || cz >= MAP_MAX_H) return 0;
	return (reach_bits[cz][cx >> 3] >> (cx & 7)) & 1;
}

/* Andável como no BFS da 05: não sólido (ou sólido BAIXO: tronco caído) e
 * sem obstáculo (tronco em pé, prop) */
int reach_walkable(int cx, int cz) {
	if (cx < 0 || cz < 0 || cx >= level_width() || cz >= level_height()) return 0;
	if (level_cell_solid(cx, cz) && !level_cell_low(cx, cz)) return 0;
	return !collide_cell_has_obstacle(cx, cz);
}

/* ------------------------------------------------------------------ */
/* Checagem local                                                       */
/* ------------------------------------------------------------------ */

#define REACH_HALF (REACH_WIN / 2)
static uint8_t  win_vis[REACH_WIN * REACH_WIN];
static uint16_t win_queue[REACH_WIN * REACH_WIN];
static int      stats[3];                    /* REACH_OK / CUT / WINDOW */

static int in_list(int x, int z, const uint16_t *blk, int n) {
	uint16_t c = (uint16_t)(x | (z << 8));
	for (int i = 0; i < n; i++)
		if (blk[i] == c) return 1;
	return 0;
}

/* Andável DEPOIS da mudança (as células da lista ficam bloqueadas) */
static int walk_after(int x, int z, const uint16_t *blk, int n) {
	return reach_walkable(x, z) && !in_list(x, z, blk, n);
}

/* Inunda a janela a partir de 'seed' marcando 'mark'; devolve 1 se
 * encostou na borda da janela (o componente pode continuar lá fora) */
static int flood_win(uint16_t seed, int mark, int x0, int z0, const uint16_t *blk, int n) {
	static const int d[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	int head = 0, tail = 0, edge = 0;
	int sx = (seed & 0xff) - x0, sz = (seed >> 8) - z0;
	win_vis[sz * REACH_WIN + sx] = mark;
	win_queue[tail++] = (uint16_t)(sz * REACH_WIN + sx);
	while (head < tail) {
		int c = win_queue[head++], lx = c % REACH_WIN, lz = c / REACH_WIN;
		if (lx == 0 || lz == 0 || lx == REACH_WIN - 1 || lz == REACH_WIN - 1)
			edge = 1;
		for (int k = 0; k < 4; k++) {
			int nx = lx + d[k][0], nz = lz + d[k][1];
			if (nx < 0 || nz < 0 || nx >= REACH_WIN || nz >= REACH_WIN) continue;
			int idx = nz * REACH_WIN + nx;
			if (win_vis[idx] || !walk_after(x0 + nx, z0 + nz, blk, n)) continue;
			win_vis[idx] = mark;
			win_queue[tail++] = (uint16_t)idx;
		}
	}
	return edge;
}

/* blk: células que vão ficar bloqueadas, empacotadas x | z << 8 (o mapa vai
 * até 128). (cx, cz) = centro da janela. Devolve REACH_OK, REACH_CUT ou
 * REACH_WINDOW. */
int reach_local_ok(int cx, int cz, const uint16_t *blk, int n) {
	static const int d[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	const int x0 = cx - REACH_HALF, z0 = cz - REACH_HALF;

	/* sementes: vizinhas livres (e alcançáveis hoje) das células bloqueadas */
	uint16_t seeds[4 * REACH_MAX_BLOCK];
	int ns = 0;
	for (int i = 0; i < n && i < REACH_MAX_BLOCK; i++) {
		int bx = blk[i] & 0xff, bz = blk[i] >> 8;
		for (int k = 0; k < 4; k++) {
			int x = bx + d[k][0], z = bz + d[k][1];
			if (!reach_get(x, z) || !walk_after(x, z, blk, n)) continue;
			if (x - x0 < 0 || z - z0 < 0 || x - x0 >= REACH_WIN || z - z0 >= REACH_WIN) {
				stats[REACH_WINDOW]++;
				return REACH_WINDOW;
			}
			seeds[ns++] = (uint16_t)(x | (z << 8));
		}
	}
	if (ns <= 1) { stats[REACH_OK]++; return REACH_OK; }

	memset(win_vis, 0, sizeof(win_vis));
	int edge0 = flood_win(seeds[0], 1, x0, z0, blk, n);
	for (int i = 1; i < ns; i++) {
		int x = seeds[i] & 0xff, z = seeds[i] >> 8;
		if (win_vis[(z - z0) * REACH_WIN + (x - x0)] == 1) continue;
		/* não chegou. Se os DOIS lados ficam fechados dentro da janela, corta
		 * de verdade; se algum encosta na borda, o caminho poderia passar por
		 * fora e a resposta é "não sei" (o evento não acontece ali) */
		int edge1 = flood_win(seeds[i], 2, x0, z0, blk, n);
		int r = (edge0 && edge1) ? REACH_WINDOW : REACH_CUT;
		stats[r]++;
		return r;
	}
	stats[REACH_OK]++;
	return REACH_OK;
}

void reach_stats(int *ok, int *cut, int *window) {
	*ok = stats[REACH_OK];
	*cut = stats[REACH_CUT];
	*window = stats[REACH_WINDOW];
}
