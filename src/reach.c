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
	int c = level_cell_raw(cx, cz);            /* fora do mapa: sólido */
	if ((c & CELL_SOLID) && !(c & CELL_LOW)) return 0;
	return !collide_cell_has_obstacle(cx, cz);
}

/* ------------------------------------------------------------------ */
/* Checagem local                                                       */
/* ------------------------------------------------------------------ */

#define REACH_HALF (REACH_WIN / 2)
/* win_walk: 0 = ainda não avaliada, 1 = andável depois, 2 = bloqueada.
 * Avaliada só quando a inundação chega nela (preguiçosa). */
static uint8_t  win_walk[REACH_WIN * REACH_WIN];
static uint8_t  win_vis[REACH_WIN * REACH_WIN];    /* 1 = 1a inundação, 2 = 2a */
static uint8_t  win_seed[REACH_WIN * REACH_WIN];   /* 1 = semente (vizinha livre da mudança) */
static uint16_t win_queue[REACH_WIN * REACH_WIN];
static int      stats[4];                    /* REACH_OK / CUT / WINDOW / LIMIT */

/* Estado da checagem em andamento (RETOMÁVEL: reach_step avança no máximo
 * 'budget' visitas e continua de onde parou). É função só do mapa e do
 * pedido: mesmo resultado em qualquer console, quebrado em passos ou não. */
static struct {
	int      active, phase;                  /* 1 = liga as sementes; 2 = o outro lado */
	int      x0, z0, head, tail, found, need, edge0, edge1, visits;
	uint16_t seeds[4 * REACH_MAX_BLOCK];
	int      ns;
} st;

/* Leitura DIRETA da grade e dos obstáculos (sem chamada de função por
 * célula: isto roda centenas de vezes) */
static const uint8_t (*rc_cells)[MAP_MAX_W];
static const int16_t (*rc_obst)[MAP_MAX_W];
static int rc_w, rc_h;

static int walk_memo(int lx, int lz) {
	int idx = lz * REACH_WIN + lx;
	if (!win_walk[idx]) {
		int x = st.x0 + lx, z = st.z0 + lz, ok = 0;
		if ((unsigned)x < (unsigned)rc_w && (unsigned)z < (unsigned)rc_h) {
			int c = rc_cells[z][x];
			ok = (!(c & CELL_SOLID) || (c & CELL_LOW)) && rc_obst[z][x] < 0;
		}
		win_walk[idx] = ok ? 1 : 2;
	}
	return win_walk[idx] == 1;
}

static int in_list(int x, int z, const uint16_t *blk, int n) {
	uint16_t c = (uint16_t)(x | (z << 8));
	for (int i = 0; i < n; i++)
		if (blk[i] == c) return 1;
	return 0;
}

static void flood_start(uint16_t seed, int mark) {
	int sx = (seed & 0xff) - st.x0, sz = (seed >> 8) - st.z0;
	st.head = st.tail = 0;
	win_vis[sz * REACH_WIN + sx] = mark;
	win_queue[st.tail++] = (uint16_t)(sz * REACH_WIN + sx);
	if (mark == 1) st.found += win_seed[sz * REACH_WIN + sx];
}

static int finish(int r) {
	st.active = 0;
	stats[r]++;
	return r == REACH_LIMIT ? REACH_WINDOW : r;
}

/* blk: células que vão ficar bloqueadas, empacotadas x | z << 8 (o mapa vai
 * até 128). (cx, cz) = centro da janela. Devolve REACH_OK / REACH_CUT /
 * REACH_WINDOW na hora, ou REACH_PENDING (continue com reach_step). */
int reach_begin(int cx, int cz, const uint16_t *blk, int n) {
	static const int d[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	st.active = 0;
	st.x0 = cx - REACH_HALF;
	st.z0 = cz - REACH_HALF;

	/* sementes: vizinhas livres (e alcançáveis hoje) das células bloqueadas */
	st.ns = 0;
	for (int i = 0; i < n && i < REACH_MAX_BLOCK; i++) {
		int bx = blk[i] & 0xff, bz = blk[i] >> 8;
		for (int k = 0; k < 4; k++) {
			int x = bx + d[k][0], z = bz + d[k][1];
			if (!reach_get(x, z) || !reach_walkable(x, z) || in_list(x, z, blk, n)) continue;
			if (x - st.x0 < 0 || z - st.z0 < 0 || x - st.x0 >= REACH_WIN || z - st.z0 >= REACH_WIN)
				return finish(REACH_WINDOW);
			st.seeds[st.ns++] = (uint16_t)(x | (z << 8));
		}
	}
	if (st.ns <= 1) return finish(REACH_OK);

	memset(win_vis, 0, sizeof(win_vis));
	memset(win_walk, 0, sizeof(win_walk));
	memset(win_seed, 0, sizeof(win_seed));
	st.need = 0;                                   /* sementes sem repetição */
	for (int i = 0; i < st.ns; i++) {
		int idx = ((st.seeds[i] >> 8) - st.z0) * REACH_WIN + ((st.seeds[i] & 0xff) - st.x0);
		if (!win_seed[idx]) { win_seed[idx] = 1; st.need++; }
	}
	for (int i = 0; i < n; i++) {                  /* as que vão bloquear */
		int lx = (blk[i] & 0xff) - st.x0, lz = (blk[i] >> 8) - st.z0;
		if (lx >= 0 && lz >= 0 && lx < REACH_WIN && lz < REACH_WIN)
			win_walk[lz * REACH_WIN + lx] = 2;
	}
	rc_cells = level_cells();
	rc_obst = collide_obst_heads();
	rc_w = level_width();
	rc_h = level_height();
	st.visits = st.found = st.edge0 = st.edge1 = 0;
	st.phase = 1;
	st.active = 1;
	flood_start(st.seeds[0], 1);
	return REACH_PENDING;
}

/* Avança no máximo 'budget' visitas (teto total REACH_MAX_VISIT, as duas
 * inundações juntas) */
int reach_step(int budget) {
	static const int d[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	if (!st.active) return REACH_OK;
	while (budget-- > 0) {
		if (st.phase == 1 && st.found >= st.need) return finish(REACH_OK);
		if (st.head >= st.tail) {
			if (st.phase == 1) {
				/* não ligou todas. Inunda a partir de uma que faltou: se
				 * os DOIS lados encostam na borda, o caminho poderia passar
				 * por fora ("não sei": o evento não acontece ali); senão
				 * corta de verdade */
				int i;
				for (i = 1; i < st.ns; i++) {
					int x = st.seeds[i] & 0xff, z = st.seeds[i] >> 8;
					if (win_vis[(z - st.z0) * REACH_WIN + (x - st.x0)] != 1) break;
				}
				if (i >= st.ns) return finish(REACH_OK);
				st.phase = 2;
				flood_start(st.seeds[i], 2);
				continue;
			}
			return finish(st.edge0 && st.edge1 ? REACH_WINDOW : REACH_CUT);
		}
		if (++st.visits > REACH_MAX_VISIT) return finish(REACH_LIMIT);
		int c = win_queue[st.head++], lx = c % REACH_WIN, lz = c / REACH_WIN;
		if (lx == 0 || lz == 0 || lx == REACH_WIN - 1 || lz == REACH_WIN - 1) {
			if (st.phase == 1) st.edge0 = 1;
			else               st.edge1 = 1;
		}
		for (int k = 0; k < 4; k++) {
			int nx = lx + d[k][0], nz = lz + d[k][1];
			if (nx < 0 || nz < 0 || nx >= REACH_WIN || nz >= REACH_WIN) continue;
			int idx = nz * REACH_WIN + nx;
			if (win_vis[idx] || !walk_memo(nx, nz)) continue;
			win_vis[idx] = st.phase;
			win_queue[st.tail++] = (uint16_t)idx;
			if (st.phase == 1) st.found += win_seed[idx];   /* semente: uma leitura */
		}
	}
	return REACH_PENDING;
}

/* Tudo de uma vez (teste do PC) */
int reach_local_ok(int cx, int cz, const uint16_t *blk, int n) {
	int r = reach_begin(cx, cz, blk, n);
	while (r == REACH_PENDING)
		r = reach_step(1 << 30);
	return r;
}

int reach_last_visits(void) {
	return st.visits;
}

void reach_stats(int *ok, int *cut, int *window, int *limit) {
	*ok = stats[REACH_OK];
	*cut = stats[REACH_CUT];
	*window = stats[REACH_WINDOW];
	*limit = stats[REACH_LIMIT];
}
