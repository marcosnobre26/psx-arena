/*
 * mapgen.c - Geração procedural da floresta (etapa 05)
 *
 * Cada fase "gerada" (LEVEL_DEF.gen) monta uma floresta diferente a cada
 * partida, a partir da semente da partida (g.seed): a MESMA semente gera
 * SEMPRE o mesmo mapa (os dois consoles do modo link precisam disso).
 *
 * Regras de determinismo:
 *   - só um RNG LOCAL (rng.c), semeado com g.seed misturado ao número da
 *     fase; o g.rng da lógica não é consumido pela geração (é salvo e
 *     restaurado a cada tentativa: a partida começa igual qualquer que seja
 *     o número de tentativas);
 *   - nada de tempo, VSync ou rand() da libc; só inteiros.
 *
 * Passos de uma tentativa:
 *   1. chão em manchas + borda de mata alta;
 *   2. pontos de interesse conforme o objetivo (início, saída, relíquias,
 *      clareira do cerco) e clareiras sorteadas;
 *   3. trilhas: caminhada aleatória com tendência ao alvo, do início até
 *      cada ponto de interesse;
 *   3b. mata alta no interior: manchas por ruído + "paredes" coladas na
 *      faixa das trilhas (corredores), suavizadas (sem pontas nem células
 *      soltas: o contorno da mata é o que custa desenhar);
 *   4. árvores pela densidade de um ruído de valor (3 oitavas), com TETO por
 *      bloco 8x8 e por janela de 3x3 blocos (o que a câmera vê): vários
 *      blocos cheios vizinhos formariam um canto pesado demais;
 *   5. troncos caídos; inundação a partir do início (o que é alcançável);
 *      inimigos e itens pelo orçamento da fase, só em células alcançáveis
 *      (inimigos longe do início, os mais fortes mais longe ainda);
 *   6. validação: nada em sólido e tudo alcançável a partir do início
 *      (tronco caído passável: dá para pular).
 * Se a validação falhar, tenta de novo com semente + 1 (até MAPGEN_TRIES);
 * se todas falharem, usa a floresta fixa da fase (LEVEL_DEF.build, plano B).
 */
#include <string.h>
#include "game.h"
#ifndef HOST_TEST
#include <hwregs_c.h>
#define LINE_TIMER() TIMER_VALUE(1)        /* perfil (linhas de vídeo) */
#else
#define LINE_TIMER() 0
#endif

/* Marcas de trabalho por célula (16 KB): trilha, perto de trilha, clareira
 * e "visitado" da busca de alcance; e a fila da busca (32 KB). Não são
 * vetores novos: REAPROVEITAM a memória do cache de blocos (level_scratch),
 * que está livre durante a geração (os blocos só são montados depois). */
#define GM_TRAIL   1
#define GM_NEAR    2
#define GM_CLEAR   4
#define GM_VISIT   8
#define GM_REACH  16          /* alcançável do início (fechar bolsões) */
#define GM_THICK  32          /* mata (durante a geração da mata alta) */
#define GM_NEXT   64          /* suavização: estado do próximo passo */
#define GM_BLOCK 128          /* inundação: célula não andável (calculada 1 vez) */
static uint8_t  (*gmark)[MAP_MAX_W];
static uint16_t *bfs_queue;

#define MAX_CB  (MAP_MAX_W / CHUNK_CELLS)      /* blocos por lado (16) */
static int16_t chunk_trees[MAX_CB][MAX_CB];   /* árvores por bloco */
static int16_t chunk_wt[MAX_CB][MAX_CB];      /* peso: árvore = 4, célula de mata = 1 */
static int16_t chunk_sides[MAX_CB][MAX_CB];   /* lados expostos da mata interior */

static MAPGEN_STATS stats;
static DEBUG_POINT  gen_pts[4];               /* pontos de teleporte do mapa gerado */
static DEBUG_POINT  tree_pt;                  /* janela com mais árvores (bench) */
static int          gen_active;               /* o mapa atual foi gerado (não plano B) */

static RNG gr;                                 /* RNG LOCAL da geração */
static int W, H;

/* ------------------------------------------------------------------ */
/* Utilidades                                                           */
/* ------------------------------------------------------------------ */

static uint32_t mhash(uint32_t seed, int x, int z) {
	uint32_t h = seed ^ ((uint32_t)x * 374761393u + (uint32_t)z * 668265263u);
	h = (h ^ (h >> 13)) * 1274126177u;
	return h ^ (h >> 16);
}

static int rnd(int lo, int hi) {               /* inteiro em [lo, hi] */
	return rng_range(&gr, lo, hi);
}

static int inside(int x, int z, int margin) {
	return x >= margin && z >= margin && x < W - margin && z < H - margin;
}

/* Ruído de valor em inteiros: rede de valores 0..255 a cada 2^shift
 * células, interpolada com curva suave (3t^2 - 2t^3). Tudo em potências de
 * 2 (shifts no lugar de divisões): o MIPS do PS1 leva ~36 ciclos por
 * divisão, e isto roda para cada célula do mapa. */
static int smooth256(int t) {                  /* t: 0..256 */
	return (t * t * (768 - 2 * t)) >> 16;
}

static int noise_octave(uint32_t seed, int x, int z, int shift) {
	int gx = x >> shift, gz = z >> shift, mask = (1 << shift) - 1;
	int fx = smooth256((x & mask) << (8 - shift)), fz = smooth256((z & mask) << (8 - shift));
	int a = mhash(seed, gx, gz) & 255,     b = mhash(seed, gx + 1, gz) & 255;
	int c = mhash(seed, gx, gz + 1) & 255, d = mhash(seed, gx + 1, gz + 1) & 255;
	int ab = a + (((b - a) * fx) >> 8), cd = c + (((d - c) * fx) >> 8);
	return ab + (((cd - ab) * fz) >> 8);
}

/* 0..255: 3 oitavas (16, 8 e 4 células), pesos 4:2:1 */
static int density_noise(uint32_t seed, int x, int z) {
	return (4 * noise_octave(seed, x, z, 4) + 2 * noise_octave(seed ^ 0x51u, x, z, 3) +
	        noise_octave(seed ^ 0xa3u, x, z, 2)) / 7;
}

/* 0..255: manchas de mata (oitavas de 16 e 8 células, pesos 2:1) */
static int blob_noise(uint32_t seed, int x, int z) {
	return (2 * noise_octave(seed, x, z, 4) + noise_octave(seed ^ 0x3du, x, z, 3)) / 3;
}

/* Suavização (autômato celular): célula com 5+ vizinhos de mata vira mata,
 * com 3 ou menos vira chão. Tira pontas e células soltas: o que custa na
 * GPU/CPU é o CONTORNO da mata (cada lado exposto = 4 quads de 3 m); o
 * miolo de uma mancha custa o mesmo que o chão (1 quad de topo).
 * Trabalha só nos bits de gmark (rápido no console); level_set_cell uma vez
 * no fim. Entrada: GM_THICK = mata (a borda já vem marcada). */
static void smooth_thicket(int passes) {
	const int E = FOREST_EDGE;
	uint8_t (*m)[MAP_MAX_W] = gmark;
	for (int p = 0; p < passes; p++) {
		for (int z = E + 1; z < H - E - 1; z++) {
			const uint8_t *up = m[z - 1], *mid = m[z], *dn = m[z + 1];
			for (int x = E + 1; x < W - E - 1; x++) {
				if (mid[x] & (GM_NEAR | GM_CLEAR)) continue;
				int n = (up[x - 1] & GM_THICK) + (up[x] & GM_THICK) + (up[x + 1] & GM_THICK) +
				        (mid[x - 1] & GM_THICK) + (mid[x + 1] & GM_THICK) +
				        (dn[x - 1] & GM_THICK) + (dn[x] & GM_THICK) + (dn[x + 1] & GM_THICK);
				n >>= 5;                          /* GM_THICK = 32 */
				if (n >= 5) m[z][x] |= GM_NEXT;
				else if (n > 3 && (mid[x] & GM_THICK)) m[z][x] |= GM_NEXT;
			}
		}
		for (int z = E + 1; z < H - E - 1; z++)
			for (int x = E + 1; x < W - E - 1; x++) {
				if (m[z][x] & (GM_NEAR | GM_CLEAR)) continue;
				m[z][x] = (m[z][x] & ~(GM_THICK | GM_NEXT)) | ((m[z][x] & GM_NEXT) ? GM_THICK : 0);
			}
	}
}

/* ------------------------------------------------------------------ */
/* Peças do mapa                                                        */
/* ------------------------------------------------------------------ */

static void carve_clearing(int cx, int cz, int r) {
	for (int z = cz - r; z <= cz + r; z++)
		for (int x = cx - r; x <= cx + r; x++) {
			if (!inside(x, z, FOREST_EDGE)) continue;
			int dx = x - cx, dz = z - cz;
			if (dx * dx + dz * dz > r * r) continue;
			gmark[z][x] |= GM_CLEAR;
			if (!(gmark[z][x] & GM_TRAIL))
				level_set_cell(x, z, CELL_LEAVES);
		}
}

static void carve_trail_cell(int x, int z) {
	for (int dz = -2; dz <= 2; dz++)
		for (int dx = -2; dx <= 2; dx++) {
			int nx = x + dx, nz = z + dz;
			if (!inside(nx, nz, FOREST_EDGE)) continue;
			gmark[nz][nx] |= GM_NEAR;
			if (dx >= -1 && dx <= 1 && dz >= -1 && dz <= 1) {
				gmark[nz][nx] |= GM_TRAIL;
				level_set_cell(nx, nz, (mhash(stats.seed, nx, nz) % 9 == 0) ? CELL_ROOTS : CELL_DIRT);
			}
		}
}

/* Trilha de (x, z) até (tx, tz): passos de 1 célula, 3 em 4 vezes na
 * direção do alvo (no eixo que falta mais), senão para o lado. */
static void walk_trail(int x, int z, int tx, int tz) {
	int lo = FOREST_EDGE + 1;
	for (int steps = 0; steps < 4 * (W + H) && (x != tx || z != tz); steps++) {
		carve_trail_cell(x, z);
		int dx = tx - x, dz = tz - z;
		int adx = dx < 0 ? -dx : dx, adz = dz < 0 ? -dz : dz;
		if (rnd(0, 3) != 0) {                         /* rumo ao alvo */
			if (adx > adz || (adx == adz && rnd(0, 1)))
				x += dx > 0 ? 1 : -1;
			else
				z += dz > 0 ? 1 : -1;
		} else {                                      /* desvio para o lado */
			if (adx > adz) z += rnd(0, 1) ? 1 : -1;
			else           x += rnd(0, 1) ? 1 : -1;
		}
		if (x < lo) x = lo;
		if (z < lo) z = lo;
		if (x >= W - lo) x = W - lo - 1;
		if (z >= H - lo) z = H - lo - 1;
	}
	carve_trail_cell(tx, tz);
}

/* Peso de uma janela de 3x3 blocos centrada em (bx, bz) */
static int window_weight(int bx, int bz) {
	int w = 0;
	for (int z = bz - 1; z <= bz + 1; z++)
		for (int x = bx - 1; x <= bx + 1; x++)
			if (x >= 0 && z >= 0 && x < MAX_CB && z < MAX_CB)
				w += chunk_wt[z][x];
	return w;
}

/* Árvores numa janela de 3x3 blocos centrada em (bx, bz) */
static int window_tree_count(int bx, int bz) {
	int t = 0;
	for (int z = bz - 1; z <= bz + 1; z++)
		for (int x = bx - 1; x <= bx + 1; x++)
			if (x >= 0 && z >= 0 && x < MAX_CB && z < MAX_CB)
				t += chunk_trees[z][x];
	return t;
}

/* Lados expostos de mata interior numa janela de 3x3 blocos em (bx, bz) */
static int window_sides(int bx, int bz) {
	int t = 0;
	for (int z = bz - 1; z <= bz + 1; z++)
		for (int x = bx - 1; x <= bx + 1; x++)
			if (x >= 0 && z >= 0 && x < MAX_CB && z < MAX_CB)
				t += chunk_sides[z][x];
	return t;
}

/* Cabe mais uma árvore no bloco (bx, bz) sem estourar o teto do bloco nem o
 * de nenhuma janela 3x3 que o contém? */
static int tree_fits(const GEN_PARAMS *gp, int bx, int bz) {
	if (chunk_trees[bz][bx] >= gp->chunk_cap)
		return 0;
	for (int z = bz - 1; z <= bz + 1; z++)
		for (int x = bx - 1; x <= bx + 1; x++)
			if (window_weight(x, z) + 4 > gp->window_cap * 4 ||
			    window_tree_count(x, z) + 1 > gp->window_trees)
				return 0;
	return 1;
}

static void add_tree(const GEN_PARAMS *gp, int x, int z) {
	uint32_t h = mhash(stats.seed ^ 0x7a3u, x, z);
	int type = (h % 100) < 65 ? TREE_PINE : TREE_DEAD;
	const TREE_DEF *d = &tree_defs[type];
	int wx = x * TILE_SIZE + TILE_SIZE / 2 + (int)((h >> 8) % 81) - 40;
	int wz = z * TILE_SIZE + TILE_SIZE / 2 + (int)((h >> 16) % 81) - 40;
	int scale = d->scale_min + (int)((h >> 4) % (uint32_t)(d->scale_max - d->scale_min + 1));
	level_add_tree(type, wx, wz, (h >> 20) & 4095, scale);
	chunk_trees[z / CHUNK_CELLS][x / CHUNK_CELLS]++;
	chunk_wt[z / CHUNK_CELLS][x / CHUNK_CELLS] += 4;
	(void)gp;
}

/* Célula livre aleatória numa região, longe do início (dmin células) */
static int random_cell(int x0, int z0, int x1, int z1, int sx, int sz, int dmin, int *ox, int *oz) {
	for (int k = 0; k < 64; k++) {
		int x = rnd(x0, x1), z = rnd(z0, z1);
		if (!inside(x, z, FOREST_EDGE + 1) || level_cell_solid(x, z)) continue;
		if (!(gmark[z][x] & GM_REACH)) continue;    /* bolsão sem acesso */
		int dx = x - sx, dz = z - sz;
		if (dx * dx + dz * dz < dmin * dmin) continue;
		*ox = x; *oz = z;
		return 1;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* Uma tentativa                                                        */
/* ------------------------------------------------------------------ */

static int walkable(int x, int z);
static void store_reach(void);
static void fixed_reach(void);

/* Inundação a partir de (sx, sz) pelas células andáveis; marca 'bit' em
 * gmark. walkable() é calculado UMA vez por célula (bit GM_BLOCK), não a
 * cada vizinho visitado: no console isso corta o tempo pela metade. */
static void flood(int sx, int sz, int bit) {
	for (int z = 0; z < H; z++)
		for (int x = 0; x < W; x++) {
			if (walkable(x, z)) gmark[z][x] &= ~GM_BLOCK;
			else                gmark[z][x] |= GM_BLOCK;
		}
	if (gmark[sz][sx] & GM_BLOCK) return;
	uint16_t *queue = bfs_queue;
	int head = 0, tail = 0;
	gmark[sz][sx] |= bit;
	queue[tail++] = sz * MAP_MAX_W + sx;
	while (head < tail) {
		int c = queue[head++], x = c % MAP_MAX_W, z = c / MAP_MAX_W;
		static const int d[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
		for (int k = 0; k < 4; k++) {
			int nx = x + d[k][0], nz = z + d[k][1];
			if ((unsigned)nx >= (unsigned)W || (unsigned)nz >= (unsigned)H) continue;
			if (gmark[nz][nx] & (GM_BLOCK | bit)) continue;
			gmark[nz][nx] |= bit;
			queue[tail++] = nz * MAP_MAX_W + nx;
		}
	}
}

/* Mata alta do interior: máscara pelo ruído + paredes coladas nas trilhas,
 * suavizada; mede os lados expostos por bloco (o que custa desenhar: cada
 * lado = 4 quads de 3 m), para o teste do PC e a escolha das piores
 * sementes do bench. */
static void build_thicket(const GEN_PARAMS *gp, uint32_t nseed) {
	const int E = FOREST_EDGE;
	for (int z = 0; z < H; z++)
		for (int x = 0; x < W; x++)
			if (!inside(x, z, E)) gmark[z][x] |= GM_THICK;   /* borda já é mata */
	for (int z = E + 1; z < H - E - 1; z++)
		for (int x = E + 1; x < W - E - 1; x++) {
			if (gmark[z][x] & (GM_NEAR | GM_CLEAR)) continue;
			int edge = (gmark[z - 1][x] | gmark[z + 1][x] | gmark[z][x - 1] | gmark[z][x + 1] |
			            gmark[z - 1][x - 1] | gmark[z - 1][x + 1] | gmark[z + 1][x - 1] |
			            gmark[z + 1][x + 1]) & GM_NEAR;   /* vizinho da faixa da trilha? */
			int n = blob_noise(nseed, x, z);
			if (n >= gp->thicket || (edge && n >= gp->thicket - gp->walls))
				gmark[z][x] |= GM_THICK;
		}
	smooth_thicket(2);
	for (int z = E + 1; z < H - E - 1; z++)
		for (int x = E + 1; x < W - E - 1; x++) {
			if (!(gmark[z][x] & GM_THICK)) continue;
			level_set_cell(x, z, CELL_THICKET);
			chunk_sides[z / CHUNK_CELLS][x / CHUNK_CELLS] +=
				!(gmark[z][x + 1] & GM_THICK) + !(gmark[z][x - 1] & GM_THICK) +
				!(gmark[z + 1][x] & GM_THICK) + !(gmark[z - 1][x] & GM_THICK);
		}
}

static void generate(const GEN_PARAMS *gp) {
	const uint32_t seed = stats.seed;
	const int E = FOREST_EDGE;
	level_begin(W, H);
	memset(gmark, 0, MAP_MAX_W * MAP_MAX_H);
	memset(chunk_trees, 0, sizeof(chunk_trees));
	memset(chunk_wt, 0, sizeof(chunk_wt));
	memset(chunk_sides, 0, sizeof(chunk_sides));

	/* 1) chão em manchas de 4x4 + borda de mata alta */
	for (int z = 0; z < H; z++)
		for (int x = 0; x < W; x++) {
			int type;
			switch (mhash(seed, x >> 2, z >> 2) % 10) {
			case 0: case 1: type = CELL_ROOTS;  break;
			case 2:         type = CELL_DIRT;   break;
			default:        type = CELL_LEAVES; break;
			}
			if (!inside(x, z, E)) {
				type = CELL_THICKET;
				chunk_wt[z / CHUNK_CELLS][x / CHUNK_CELLS] += 1;
			}
			level_set_cell(x, z, type);
		}

	/* 2) pontos de interesse conforme o objetivo */
	int sx, sz, ex = -1, ez = -1;
	int qx[4], qz[4], nq = 0;
	int m = E + 4;                                    /* longe da borda */
	switch (gp->layout) {
	case GEN_CROSS:                                   /* oeste -> leste */
		sx = m;         sz = rnd(H / 4, H * 3 / 4);
		ex = W - m - 1; ez = rnd(H / 4, H * 3 / 4);
		break;
	case GEN_COLLECT:                                 /* relíquia por quadrante, saída no meio */
		sx = rnd(W / 3, W * 2 / 3); sz = H - m - 1;
		for (int q = 0; q < 4; q++) {
			int qx0 = (q & 1) ? W / 2 + 4 : m, qz0 = (q & 2) ? H / 2 + 4 : m;
			qx[q] = rnd(qx0, qx0 + W / 2 - m - 8);
			qz[q] = rnd(qz0, qz0 + H / 2 - m - 8);
			if (q >= 2 && qz[q] > H - m - 10) qz[q] = H - m - 10;   /* não em cima do início */
		}
		nq = 4;
		ex = rnd(W / 2 - 6, W / 2 + 6); ez = rnd(H / 2 - 6, H / 2 + 6);
		break;
	case GEN_SURVIVE:                                 /* clareira grande no meio */
		sx = rnd(W / 2 - 4, W / 2 + 4); sz = rnd(H / 2 - 4, H / 2 + 4);
		break;
	default:                                          /* GEN_HUNT: começa numa borda */
		if (rnd(0, 1)) { sx = rnd(m, W - m - 1); sz = rnd(0, 1) ? m : H - m - 1; }
		else           { sz = rnd(m, H - m - 1); sx = rnd(0, 1) ? m : W - m - 1; }
		break;
	}

	/* clareiras: as dos pontos de interesse + as sorteadas */
	int ccx[16], ccz[16], nc = 0;
	carve_clearing(sx, sz, gp->layout == GEN_SURVIVE ? gp->survive_r : 3);
	if (ex >= 0) carve_clearing(ex, ez, 3);
	for (int q = 0; q < nq; q++) carve_clearing(qx[q], qz[q], 4);
	int nclear = rnd(gp->clear_min, gp->clear_max);
	for (int k = 0; k < nclear && nc < 16; k++) {
		int r = rnd(gp->clear_rmin, gp->clear_rmax);
		int x = rnd(m + r, W - m - r - 1), z = rnd(m + r, H - m - r - 1);
		carve_clearing(x, z, r);
		ccx[nc] = x; ccz[nc] = z; nc++;
	}

	/* 3) trilhas do início até cada ponto de interesse e cada clareira */
	if (ex >= 0) walk_trail(sx, sz, ex, ez);
	for (int q = 0; q < nq; q++) walk_trail(sx, sz, qx[q], qz[q]);
	for (int k = 0; k < nc; k++) walk_trail(sx, sz, ccx[k], ccz[k]);

	/* 3b) mata alta no interior: manchas pelo ruído e "paredes" coladas na
	 * faixa das trilhas (limiar mais baixo ali), o que forma corredores.
	 * Nunca na trilha, perto dela ou em clareira. */
	if (gp->thicket < 256)
		build_thicket(gp, seed ^ 0x7c1u);

	/* 4) árvores: densidade do ruído (dens_min..dens_max por mil), nunca nas
	 * trilhas/perto delas/clareiras, respeitando os tetos */
	for (int z = E; z < H - E; z++)
		for (int x = E; x < W - E; x++) {
			if ((gmark[z][x] & (GM_NEAR | GM_CLEAR)) || level_cell_solid(x, z)) continue;
			int dens = gp->dens_min + (gp->dens_max - gp->dens_min) * density_noise(seed, x, z) / 255;
			int edge = x == E || z == E || x == W - E - 1 || z == H - E - 1;
			int want = edge ? ((x + z) & 1) == 0 : (int)(mhash(seed, x, z) % 1000) < dens;
			if (want && tree_fits(gp, x / CHUNK_CELLS, z / CHUNK_CELLS))
				add_tree(gp, x, z);
		}

	/* 5a) troncos caídos (deitados em X ou Z; 3 células livres) */
	for (int k = 0, placed = 0; k < 400 && placed < gp->logs; k++) {
		int x = rnd(E + 2, W - E - 3), z = rnd(E + 2, H - E - 3), along_z = rnd(0, 1);
		int ok = 1;
		for (int i = -2; i <= 2 && ok; i++) {
			int cx = x + (along_z ? 0 : i), cz = z + (along_z ? i : 0);
			if ((gmark[cz][cx] & (GM_NEAR | GM_CLEAR)) || level_cell_solid(cx, cz) ||
			    collide_cell_has_obstacle(cx, cz))
				ok = 0;
		}
		if (!ok) continue;
		level_add_tree(TREE_LOG, x * TILE_SIZE + TILE_SIZE / 2, z * TILE_SIZE + TILE_SIZE / 2,
		               (along_z ? 1024 : 0) + rnd(-64, 64),
		               tree_defs[TREE_LOG].scale_min + rnd(0, 399));
		chunk_wt[z / CHUNK_CELLS][x / CHUNK_CELLS] += 4;
		placed++;
	}

	/* 5) o que é alcançável do início: inundação com a mesma regra da
	 * validação (mata e tronco em pé bloqueiam, tronco caído passa), DEPOIS
	 * das árvores e troncos, que também fecham passagens. Itens e inimigos
	 * só são sorteados em células marcadas (random_cell). Os bolsões
	 * fechados ficam como estão: enchê-los de mata criava tufos soltos entre
	 * as árvores, com 3-4 lados expostos cada (caro de desenhar). */
	flood(sx, sz, GM_REACH);

	/* peso da mata interior nas janelas: 1 por LADO EXPOSTO (o que custa
	 * desenhar). Só DEPOIS das árvores, para o teto de peso (GEN_WINDOW_CAP)
	 * continuar valendo para árvores + borda como antes; entra nas medidas
	 * (pior janela do bench) */
	for (int z = E; z < H - E; z++)
		for (int x = E; x < W - E; x++) {
			if (!(gmark[z][x] & GM_THICK)) continue;
			int sides = !(gmark[z][x + 1] & GM_THICK) + !(gmark[z][x - 1] & GM_THICK) +
			            !(gmark[z + 1][x] & GM_THICK) + !(gmark[z - 1][x] & GM_THICK);
			chunk_wt[z / CHUNK_CELLS][x / CHUNK_CELLS] += sides;   /* 1 por lado exposto */
		}

	/* 5b) início, saída, relíquias, pontos de reforço */
	level_place_safe('P', sx, sz);
	if (ex >= 0) level_place_safe('X', ex, ez);
	for (int q = 0; q < nq; q++) level_place_safe('Q', qx[q], qz[q]);
	if (gp->layout == GEN_SURVIVE) {
		int r = gp->survive_r - 2;                    /* perto da borda da clareira */
		static const int dir[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
		for (int k = 0; k < 4; k++)
			level_place_safe('S', sx + dir[k][0] * r * 7 / 10, sz + dir[k][1] * r * 7 / 10);
	}

	/* 5c) inimigos: longe do início; os mais fortes mais longe ainda */
	int far = W > H ? W : H;
	for (int k = 0; k < gp->enemies; k++) {
		int x, z;
		if (!random_cell(E, E, W - E - 1, H - E - 1, sx, sz, gp->enemy_min_dist, &x, &z))
			continue;
		int dx = x - sx, dz = z - sz;
		int frac = dist2d(dx, dz) * 100 / far;        /* 0..~140 */
		int r = rnd(0, 99);
		char ch = (r < frac / 4) ? 'B' : (r < 20 + frac / 3) ? 'F' : 'E';
		level_place_safe(ch, x, z);
	}

	/* 5d) itens pelo orçamento (perto de onde se anda: qualquer lugar livre) */
	static const char item_ch[GEN_ITEM_TYPES] = { 'H', 'N', 'L', 'W', 'G', 'C' };
	for (int t = 0; t < GEN_ITEM_TYPES; t++)
		for (int k = 0; k < gp->items[t]; k++) {
			int x, z;
			if (random_cell(E, E, W - E - 1, H - E - 1, sx, sz, 3, &x, &z))
				level_place_safe(item_ch[t], x, z);
		}

	/* pontos de teleporte: início, centro, janela mais densa, saída/relíquia */
	gen_pts[0].cx = sx; gen_pts[0].cz = sz; gen_pts[0].yaw = 0;
	gen_pts[1].cx = W / 2; gen_pts[1].cz = H / 2; gen_pts[1].yaw = 512;
	gen_pts[3].cx = ex >= 0 ? ex : (nc ? ccx[0] : W / 2);
	gen_pts[3].cz = ex >= 0 ? ez : (nc ? ccz[0] : H / 2);
	gen_pts[3].yaw = 2048;
}

/* Tudo alcançável a partir do início e nada em sólido? (BFS na grade;
 * tronco caído passável, tronco em pé e mata não) */
static int walkable(int x, int z) {
	if (x < 0 || z < 0 || x >= W || z >= H) return 0;
	if (level_cell_solid(x, z) && !level_cell_low(x, z)) return 0;
	return !collide_cell_has_obstacle(x, z);
}

static int validate(void) {
	char ch; int cx, cz;
	if (level_place_failures(0, &ch, &cx, &cz) > 0)
		return 0;                                     /* algo ficou sem lugar */
	int sx = g.spawn_x / TILE_SIZE, sz = g.spawn_z / TILE_SIZE;
	if (!walkable(sx, sz)) return 0;
	flood(sx, sz, GM_VISIT);                    /* marcas: bit VISIT da grade gmark */
	int n = level_placed(0, &ch, &cx, &cz);
	for (int i = 0; i < n; i++) {
		level_placed(i, &ch, &cx, &cz);
		if (ch == 'C') continue;                     /* caixa ocupa a própria célula */
		if (!(gmark[cz][cx] & GM_VISIT)) return 0;
	}
	return 1;
}

/* Medidas do mapa pronto (teste do PC e bench) */
static void measure(void) {
	int trees = 0, interior = 0, best = -1;
	for (int bz = 0; bz < MAX_CB; bz++)
		for (int bx = 0; bx < MAX_CB; bx++) {
			trees += chunk_trees[bz][bx];
			if (chunk_trees[bz][bx] > stats.max_chunk) stats.max_chunk = chunk_trees[bz][bx];
			if (bx * CHUNK_CELLS >= W || bz * CHUNK_CELLS >= H) continue;
			int w = window_weight(bx, bz);
			if (w > best) {
				best = w;
				stats.win_bx = bx; stats.win_bz = bz;
			}
			if (window_sides(bx, bz) > stats.win_sides) stats.win_sides = window_sides(bx, bz);
			int t = window_tree_count(bx, bz);
			if (t > stats.win_trees) {
				stats.win_trees = t;
				stats.wt_bx = bx; stats.wt_bz = bz;
			}
		}
	int thick = 0;
	for (int z = FOREST_EDGE; z < H - FOREST_EDGE; z++)
		for (int x = FOREST_EDGE; x < W - FOREST_EDGE; x++) {
			interior++;
			thick += level_cell_type(x, z) == CELL_THICKET;
		}
	stats.thicket_permil = interior ? thick * 1000 / interior : 0;
	stats.trees = trees;
	stats.dens_permil = interior ? trees * 1000 / interior : 0;
	stats.win_weight = best;
}

/* Ponto livre e alcançável perto do centro do bloco (bx, bz) */
static void free_point_near(int bx, int bz, DEBUG_POINT *out) {
	int cx = bx * CHUNK_CELLS + CHUNK_CELLS / 2, cz = bz * CHUNK_CELLS + CHUNK_CELLS / 2;
	for (int r = 0; r < 12; r++)
		for (int dz = -r; dz <= r; dz++)
			for (int dx = -r; dx <= r; dx++) {
				int x = cx + dx, z = cz + dz;
				if (walkable(x, z) && (gmark[z][x] & GM_VISIT) &&
				    !collide_solid_at(x * TILE_SIZE + TILE_SIZE / 2, z * TILE_SIZE + TILE_SIZE / 2, PLAYER_RADIUS + 16)) {
					out->cx = x; out->cz = z; out->yaw = 0;
					return;
				}
			}
	*out = gen_pts[0];
}

/* Pior ponto do bench: centro da janela mais densa (árvores e mata); e o
 * centro da janela com mais árvores */
static void set_worst_point(void) {
	free_point_near(stats.win_bx, stats.win_bz, &gen_pts[2]);
	free_point_near(stats.wt_bx, stats.wt_bz, &tree_pt);
}

/* Limpa o que uma tentativa criou em g (o resto de g não é tocado) */
static void reset_entities(void) {
	memset(g.enemies, 0, sizeof(g.enemies));
	memset(g.pickups, 0, sizeof(g.pickups));
	memset(g.crates, 0, sizeof(g.crates));
	memset(g.props, 0, sizeof(g.props));
	g.enemies_left = 0;
	g.gems_total = 0;
	g.quest_total = 0;
	g.has_exit = 0;
	g.num_spawn_pts = 0;
	g.spawn_x = g.spawn_z = 0;
}

/* ------------------------------------------------------------------ */
/* Entrada                                                              */
/* ------------------------------------------------------------------ */

/* Monta a fase 'index' (chamado pelo level_load). Devolve 1 se gerou, 0 se
 * usou o plano B (floresta fixa). */
int mapgen_build(int index) {
	const LEVEL_DEF *ld = &level_defs[index];
	const GEN_PARAMS *gp = ld->gen;
	const RNG saved = g.rng;                          /* a geração não muda o g.rng */
	uint32_t base = g.seed ^ ((uint32_t)(index + 1) * 0x9E3779B1u);

	memset(&stats, 0, sizeof(stats));
	uint8_t *scratch = level_scratch(MAP_MAX_W * MAP_MAX_H * 3);   /* 16 KB + 32 KB */
	gmark = (uint8_t (*)[MAP_MAX_W])scratch;
	bfs_queue = (uint16_t *)(scratch + MAP_MAX_W * MAP_MAX_H);
	W = gp->w;
	H = gp->h;
	for (int t = 0; t < MAPGEN_TRIES; t++) {
		level_reset_objects();
		reset_entities();
		g.rng = saved;
		stats.seed = base + t;
		stats.tries = t + 1;
		rng_seed(&gr, stats.seed);
		uint16_t t0 = LINE_TIMER();
		generate(gp);
		uint16_t t1 = LINE_TIMER();
		int ok = validate();
		stats.gen_lines += (uint16_t)(t1 - t0);
		stats.val_lines += (uint16_t)(LINE_TIMER() - t1);
		if (ok) {
			store_reach();                   /* GM_VISIT da validação */
			measure();
			set_worst_point();
			gen_active = 1;
			return 1;
		}
	}
	/* plano B: a floresta fixa da fase (já validada no teste do PC) */
	level_reset_objects();
	reset_entities();
	g.rng = saved;
	ld->build();
	fixed_reach();
	stats.fallback = 1;
	gen_active = 0;
	return 0;
}

const DEBUG_POINT *mapgen_tree_point(void) {
	return &tree_pt;
}

const DEBUG_POINT *mapgen_worst_point(void) {
	return &gen_pts[2];
}

#ifdef DEBUG_BENCHMARK
int bench_force_fixed;
#endif

const MAPGEN_STATS *mapgen_stats(void) {
	return &stats;
}

/* Pontos de teleporte da fase atual (gerados ou os fixos da tabela) */
const DEBUG_POINT *mapgen_points(int index) {
	if (level_defs[index].gen && gen_active)
		return gen_pts;
	return level_defs[index].debug_pts;
}

/* O último level_load foi gerado? (fases fixas e plano B: 0) */
int mapgen_active(void) {
	return gen_active;
}

/* Mapa de alcance da partida (reach.c) a partir das marcas GM_VISIT da
 * última inundação: os eventos da etapa 06 o usam */
static void store_reach(void) {
	reach_clear();
	for (int z = 0; z < H; z++)
		for (int x = 0; x < W; x++)
			if (gmark[z][x] & GM_VISIT) reach_set(x, z, 1);
}

/* Floresta fixa (plano B ou fase de teste): só a inundação para o mapa de
 * alcance (memória de rascunho, livre antes dos blocos serem montados) */
static void fixed_reach(void) {
	uint8_t *scratch = level_scratch(MAP_MAX_W * MAP_MAX_H * 3);
	gmark = (uint8_t (*)[MAP_MAX_W])scratch;
	bfs_queue = (uint16_t *)(scratch + MAP_MAX_W * MAP_MAX_H);
	memset(gmark, 0, MAP_MAX_W * MAP_MAX_H);
	W = level_width();
	H = level_height();
	int sx = g.spawn_x / TILE_SIZE, sz = g.spawn_z / TILE_SIZE;
	if (walkable(sx, sz)) flood(sx, sz, GM_VISIT);
	store_reach();
}

void mapgen_mark_fixed(void) {
	gen_active = 0;
	memset(&stats, 0, sizeof(stats));
	fixed_reach();
#ifdef DEBUG_BENCHMARK
	/* bench: mede a floresta fixa com o MESMO critério do gerador (janela
	 * mais pesada e janela com mais árvores), para comparar de forma justa */
	memset(gmark, 0, MAP_MAX_W * MAP_MAX_H);
	memset(chunk_trees, 0, sizeof(chunk_trees));
	memset(chunk_wt, 0, sizeof(chunk_wt));
	int x, z, type;
	for (int i = 0; (type = level_tree_get(i, &x, &z)) >= 0; i++) {
		int bx = x / (CHUNK_CELLS * TILE_SIZE), bz = z / (CHUNK_CELLS * TILE_SIZE);
		if (type != TREE_LOG) chunk_trees[bz][bx]++;
		chunk_wt[bz][bx] += 4;
	}
	for (int cz = 0; cz < H; cz++)
		for (int cx = 0; cx < W; cx++)
			if (level_cell_type(cx, cz) == CELL_THICKET)
				chunk_wt[cz / CHUNK_CELLS][cx / CHUNK_CELLS] += 1;
	validate();
	measure();
	set_worst_point();
#endif
}
