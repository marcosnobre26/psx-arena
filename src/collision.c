/*
 * collision.c - Colisão por GRADE ESPACIAL (nada de percorrer todos os objetos)
 *
 * Cada coisa ocupa um CÍRCULO no plano X/Z, ou uma célula sólida do mapa:
 *   - paredes, mata, caixas, troncos caídos -> células sólidas (level.c)
 *   - troncos de árvore em pé, props sólidos -> OBSTÁCULOS FIXOS (círculos)
 *   - jogadores e inimigos                   -> ENTIDADES (círculos que andam)
 *
 * A grade espacial é a própria grade do mapa (1 célula = 256 unidades):
 *   - obst_head[cz][cx]: lista dos obstáculos fixos daquela célula. Um
 *     obstáculo sempre cabe INTEIRO na sua célula (o raio é limitado na hora
 *     de registrar), então basta olhar as células que o círculo consultado toca;
 *   - ent_head[cz][cx]: lista das entidades cujo CENTRO está na célula. Como
 *     uma entidade pode passar da própria célula, a busca amplia pelo maior
 *     raio de entidade (ent_max_r).
 * Tudo em vetores estáticos (sem malloc), fora de g, mas calculado só a partir
 * das posições em g: é refeito em collide_reset() (início do level_load) e
 * atualizado por collide_track() sempre que uma posição muda.
 *
 * Para mover algo:   collide_move(&pos, dx, dz, raio, eu_mesmo, COL_ALL);
 * O movimento é testado separadamente em X e em Z, então quem bate numa
 * parede de lado "desliza" nela em vez de parar. collide_move também
 * atualiza a célula da entidade na grade.
 *
 * Regras que evitam travamentos (as mesmas de antes da grade):
 *   - "self" é o próprio objeto, ignorado no teste;
 *   - se dois círculos já estão sobrepostos (ex.: um empurrão), o movimento
 *     que os AFASTA é permitido; só é bloqueado o que os aproxima;
 *   - quem está alto no ar (pulando) passa por cima de jogadores e inimigos;
 *   - células "baixas" (tronco caído): quem está acima de LOW_CLEAR passa por
 *     cima, e quem já está sobre ela pode sair (pulou e caiu em cima).
 */
#include <stdlib.h>
#include "game.h"

#define JUMP_CLEAR  150     /* altura do pulo que passa por cima de alguém */
#define LOW_CLEAR   100     /* altura que passa por cima de um tronco caído */

/* ------------------------------------------------------------------ */
/* Contador de testes (depuração: overlay L2 "COL" e ./dev bench)        */
/* ------------------------------------------------------------------ */
static int col_tests, col_tests_last;

void collide_tick_begin(void) {
	col_tests_last = col_tests;
	col_tests = 0;
}

int collide_stats(void)     { return col_tests_last; }
int collide_tests_now(void) { return col_tests; }

/* ------------------------------------------------------------------ */
/* Grade: obstáculos fixos                                              */
/* ------------------------------------------------------------------ */
#define MAX_OBST    (MAX_TREES + MAX_PROPS)

typedef struct {
	uint16_t x, z;          /* centro no mundo */
	uint16_t r;             /* raio */
	int16_t  next;          /* próximo na mesma célula (-1 = fim) */
} OBST;

static OBST    obst[MAX_OBST];
static int     num_obst;
static int16_t obst_head[MAP_MAX_H][MAP_MAX_W];      /* 32 KB */

/* ------------------------------------------------------------------ */
/* Grade: entidades (ids 0..1 = jogadores, 2..25 = inimigos)             */
/* ------------------------------------------------------------------ */
#define ENT_ENEMY0  MAX_PLAYERS
#define MAX_ENT     (MAX_PLAYERS + MAX_ENEMIES)

static int8_t  ent_head[MAP_MAX_H][MAP_MAX_W];       /* 16 KB */
static int8_t  ent_next[MAX_ENT];
static int16_t ent_cx[MAX_ENT], ent_cz[MAX_ENT];     /* célula atual (-1 = fora) */
static int     ent_max_r;                            /* maior raio de entidade */

/* Qual id da grade é este ponteiro? (jogador, inimigo ou -1 = outra coisa) */
static int ent_id(const void *self) {
	const PLAYER *p = (const PLAYER *)self;
	const ENEMY  *e = (const ENEMY *)self;
	if (p >= &g.players[0] && p < &g.players[MAX_PLAYERS])
		return p - &g.players[0];
	if (e >= &g.enemies[0] && e < &g.enemies[MAX_ENEMIES])
		return ENT_ENEMY0 + (e - &g.enemies[0]);
	return -1;
}

/* A entidade existe para a colisão? Onde está? Qual o raio? */
static int ent_info(int id, int *x, int *z, int *y, int *r) {
	if (id < ENT_ENEMY0) {
		const PLAYER *p = &g.players[id];
		if (!player_alive(p)) return 0;
		*x = p->pos.vx; *z = p->pos.vz; *y = p->pos.vy; *r = PLAYER_RADIUS;
	} else {
		const ENEMY *e = &g.enemies[id - ENT_ENEMY0];
		if (!e->active) return 0;
		*x = e->pos.vx; *z = e->pos.vz; *y = e->pos.vy; *r = enemy_radius(e);
	}
	return 1;
}

static void ent_unlink(int id) {
	if (ent_cx[id] < 0) return;
	int8_t *pp = &ent_head[ent_cz[id]][ent_cx[id]];
	while (*pp >= 0 && *pp != id)
		pp = &ent_next[(int)*pp];
	if (*pp == id)
		*pp = ent_next[id];
	ent_cx[id] = ent_cz[id] = -1;
}

/* Coloca a entidade na célula certa (ou tira, se morreu/sumiu). Chame sempre
 * que a posição for definida direto: nascer, morrer, respawn, teleporte. */
void collide_track(const void *self) {
	int id = ent_id(self);
	if (id < 0) return;
	int x, z, y, r;
	if (!ent_info(id, &x, &z, &y, &r)) {
		ent_unlink(id);
		return;
	}
	int cx = x / TILE_SIZE, cz = z / TILE_SIZE;
	if (cx < 0) cx = 0;
	if (cz < 0) cz = 0;
	if (cx >= MAP_MAX_W) cx = MAP_MAX_W - 1;
	if (cz >= MAP_MAX_H) cz = MAP_MAX_H - 1;
	if (cx == ent_cx[id] && cz == ent_cz[id])
		return;                                   /* mesma célula: nada a fazer */
	ent_unlink(id);
	ent_next[id] = ent_head[cz][cx];
	ent_head[cz][cx] = id;
	ent_cx[id] = cx;
	ent_cz[id] = cz;
}

/* Começo do level_load, antes de o mapa criar inimigos e objetos */
void collide_reset(void) {
	num_obst = 0;
	for (int z = 0; z < MAP_MAX_H; z++)
		for (int x = 0; x < MAP_MAX_W; x++) {
			obst_head[z][x] = -1;
			ent_head[z][x] = -1;
		}
	for (int i = 0; i < MAX_ENT; i++) {
		ent_next[i] = -1;
		ent_cx[i] = ent_cz[i] = -1;
	}
	ent_max_r = PLAYER_RADIUS;
	for (int t = 0; t < num_enemy_types; t++) {
		int r = (90 * enemy_defs[t].scale) >> 12;     /* mesmo cálculo de enemy_radius */
		if (r > ent_max_r) ent_max_r = r;
	}
}

/* Registra um obstáculo fixo (tronco em pé, prop sólido). O raio é limitado
 * para o círculo caber inteiro na célula do centro: assim as buscas só
 * precisam olhar as células que tocam. */
void collide_add_obstacle(int x, int z, int r) {
	if (num_obst >= MAX_OBST || x < 0 || z < 0 ||
	    x >= MAP_MAX_W * TILE_SIZE || z >= MAP_MAX_H * TILE_SIZE)
		return;
	int cx = x / TILE_SIZE, cz = z / TILE_SIZE;
	int x0 = cx * TILE_SIZE, z0 = cz * TILE_SIZE;
	int lim = x - x0;
	if (x0 + TILE_SIZE - 1 - x < lim) lim = x0 + TILE_SIZE - 1 - x;
	if (z - z0 < lim) lim = z - z0;
	if (z0 + TILE_SIZE - 1 - z < lim) lim = z0 + TILE_SIZE - 1 - z;
	if (r > lim) r = lim;
	if (r < 1) r = 1;
	OBST *o = &obst[num_obst];
	o->x = x; o->z = z; o->r = r;
	o->next = obst_head[cz][cx];
	obst_head[cz][cx] = num_obst++;
}

int collide_cell_has_obstacle(int cx, int cz) {
	if (cx < 0 || cz < 0 || cx >= MAP_MAX_W || cz >= MAP_MAX_H) return 0;
	return obst_head[cz][cx] >= 0;
}

/* ------------------------------------------------------------------ */
/* Testes                                                              */
/* ------------------------------------------------------------------ */

/* Testa um círculo (x,z,r) contra outro (ox,oz,orad).
 * Bloqueia só se vai sobrepor E está se aproximando (de from_x/from_z). */
static int circle_blocks(int x, int z, int r, int ox, int oz, int orad,
                         int from_x, int from_z) {
	col_tests++;
	int min = r + orad;
	int dx = x - ox, dz = z - oz;
	if (dx >= min || dx <= -min || dz >= min || dz <= -min)
		return 0;                                   /* longe: descarte rápido */
	int d2 = dx * dx + dz * dz;
	if (d2 >= min * min)
		return 0;
	int fx = from_x - ox, fz = from_z - oz;
	return d2 < fx * fx + fz * fz;                  /* aproximando? bloqueia */
}

/* O quadrado da célula (cx,cz) encosta no círculo (x,z,r)? */
static int cell_touches(int cx, int cz, int x, int z, int r) {
	int x0 = cx * TILE_SIZE, z0 = cz * TILE_SIZE;
	return x + r > x0 && x - r < x0 + TILE_SIZE && z + r > z0 && z - r < z0 + TILE_SIZE;
}

/* Células sólidas do mapa. Células baixas (tronco caído) não bloqueiam quem
 * está alto (y < -LOW_CLEAR) nem quem já estava sobre elas (para sair). */
static int cells_block(int x, int z, int y, int r, int from_x, int from_z) {
	if (x - r < 0 || z - r < 0) return 1;
	int x0 = (x - r) / TILE_SIZE, x1 = (x + r) / TILE_SIZE;
	int z0 = (z - r) / TILE_SIZE, z1 = (z + r) / TILE_SIZE;
	for (int cz = z0; cz <= z1; cz++)
		for (int cx = x0; cx <= x1; cx++) {
			if (!level_cell_solid(cx, cz)) continue;
			if (level_cell_low(cx, cz) &&
			    (y < -LOW_CLEAR || cell_touches(cx, cz, from_x, from_z, r)))
				continue;
			return 1;
		}
	return 0;
}

/* Obstáculos fixos nas células que o círculo toca */
static int obstacles_block(int x, int z, int r, int from_x, int from_z) {
	int x0 = (x - r) / TILE_SIZE, x1 = (x + r) / TILE_SIZE;
	int z0 = (z - r) / TILE_SIZE, z1 = (z + r) / TILE_SIZE;
	if (x0 < 0) x0 = 0;
	if (z0 < 0) z0 = 0;
	if (x1 >= MAP_MAX_W) x1 = MAP_MAX_W - 1;
	if (z1 >= MAP_MAX_H) z1 = MAP_MAX_H - 1;
	for (int cz = z0; cz <= z1; cz++)
		for (int cx = x0; cx <= x1; cx++)
			for (int i = obst_head[cz][cx]; i >= 0; i = obst[i].next)
				if (circle_blocks(x, z, r, obst[i].x, obst[i].z, obst[i].r, from_x, from_z))
					return 1;
	return 0;
}

int collide_blocked(int x, int z, int y, int radius, const void *self, int mask,
                    int from_x, int from_z) {
	if (cells_block(x, z, y, radius, from_x, from_z))
		return 1;
	if (obstacles_block(x, z, radius, from_x, from_z))
		return 1;
	if (!(mask & (COL_PLAYERS | COL_ENEMIES)))
		return 0;

	/* entidades: o centro de quem pode encostar está até radius + ent_max_r */
	int reach = radius + ent_max_r;
	int x0 = (x - reach) / TILE_SIZE, x1 = (x + reach) / TILE_SIZE;
	int z0 = (z - reach) / TILE_SIZE, z1 = (z + reach) / TILE_SIZE;
	if (x0 < 0) x0 = 0;
	if (z0 < 0) z0 = 0;
	if (x1 >= MAP_MAX_W) x1 = MAP_MAX_W - 1;
	if (z1 >= MAP_MAX_H) z1 = MAP_MAX_H - 1;
	int me = ent_id(self);
	for (int cz = z0; cz <= z1; cz++)
		for (int cx = x0; cx <= x1; cx++)
			for (int id = ent_head[cz][cx]; id >= 0; id = ent_next[id]) {
				if (id == me) continue;
				if (!(mask & (id < ENT_ENEMY0 ? COL_PLAYERS : COL_ENEMIES))) continue;
				int ox, oz, oy, orad;
				if (!ent_info(id, &ox, &oz, &oy, &orad)) continue;
				if (abs(oy - y) > JUMP_CLEAR) continue;     /* um está no ar */
				if (circle_blocks(x, z, radius, ox, oz, orad, from_x, from_z))
					return 1;
			}
	return 0;
}

int collide_move(VECTOR *pos, int dx, int dz, int radius, const void *self, int mask) {
	int hit = 0;
	if (dx) {
		if (!collide_blocked(pos->vx + dx, pos->vz, pos->vy, radius, self, mask, pos->vx, pos->vz))
			pos->vx += dx;
		else
			hit = 1;
	}
	if (dz) {
		if (!collide_blocked(pos->vx, pos->vz + dz, pos->vy, radius, self, mask, pos->vx, pos->vz))
			pos->vz += dz;
		else
			hit = 1;
	}
	collide_track(self);                 /* mudou de célula? atualiza a grade */
	return hit;
}

/* Há algo sólido (célula ou obstáculo fixo) no círculo? Para tiros e para
 * achar lugar livre. Aqui o tronco caído conta (o tiro para nele). */
int collide_solid_at(int x, int z, int r) {
	if (x - r < 0 || z - r < 0) return 1;
	int x0 = (x - r) / TILE_SIZE, x1 = (x + r) / TILE_SIZE;
	int z0 = (z - r) / TILE_SIZE, z1 = (z + r) / TILE_SIZE;
	for (int cz = z0; cz <= z1; cz++)
		for (int cx = x0; cx <= x1; cx++) {
			if (level_cell_solid(cx, cz))
				return 1;
			if (cx >= MAP_MAX_W || cz >= MAP_MAX_H) continue;
			for (int i = obst_head[cz][cx]; i >= 0; i = obst[i].next) {
				col_tests++;
				int min = r + obst[i].r;
				int ddx = x - obst[i].x, ddz = z - obst[i].z;
				if (abs(ddx) < min && abs(ddz) < min && ddx * ddx + ddz * ddz < min * min)
					return 1;
			}
		}
	return 0;
}

/* Inimigo atingido em (x,z): caixa de ((90 * escala) + pad) em volta do
 * centro. Entre vários, o de MENOR índice (o mesmo resultado de quando o
 * tiro percorria a lista inteira em ordem). */
ENEMY *collide_enemy_at(int x, int z, int pad) {
	int reach = ent_max_r + pad;
	int x0 = (x - reach) / TILE_SIZE, x1 = (x + reach) / TILE_SIZE;
	int z0 = (z - reach) / TILE_SIZE, z1 = (z + reach) / TILE_SIZE;
	if (x0 < 0) x0 = 0;
	if (z0 < 0) z0 = 0;
	if (x1 >= MAP_MAX_W) x1 = MAP_MAX_W - 1;
	if (z1 >= MAP_MAX_H) z1 = MAP_MAX_H - 1;
	int best = -1;
	for (int cz = z0; cz <= z1; cz++)
		for (int cx = x0; cx <= x1; cx++)
			for (int id = ent_head[cz][cx]; id >= 0; id = ent_next[id]) {
				if (id < ENT_ENEMY0) continue;
				int i = id - ENT_ENEMY0;
				const ENEMY *e = &g.enemies[i];
				if (!e->active || (best >= 0 && i > best)) continue;
				col_tests++;
				int r = ((90 * enemy_defs[e->type].scale) >> 12) + pad;
				int dx = e->pos.vx - x, dz = e->pos.vz - z;
				if (abs(dx) < r && abs(dz) < r)
					best = i;
			}
	return best >= 0 ? &g.enemies[best] : NULL;
}

/* ------------------------------------------------------------------ */
/* Linha de visão                                                       */
/* ------------------------------------------------------------------ */
/* A visão de (x0,z0) a (x1,z1) está livre? Percorre exatamente as células
 * que o segmento cruza (DDA em inteiros) e, em cada uma:
 *   - parede, mata ou caixa (célula sólida que não é baixa) bloqueia;
 *   - tronco caído (baixo) e chão vazio NÃO bloqueiam (dá para ver por cima);
 *   - tronco em pé: bloqueia se o segmento passa a menos de r do centro.
 * Pessoas não bloqueiam. Para a IA da etapa 07; não muda g. */
static int seg_hits_circle(int x0, int z0, int ux, int uz, int len, int ox, int oz, int r) {
	int rx = ox - x0, rz = oz - z0;
	int along = (rx * ux + rz * uz) >> 12;          /* ux,uz: direção (4096 = 1) */
	if (along < -r || along > len + r)
		return 0;
	int side = (rx * uz - rz * ux) >> 12;
	return side < r && side > -r;
}

int collide_line_of_sight(int x0, int z0, int x1, int z1) {
	int dx = x1 - x0, dz = z1 - z0;
	int adx = abs(dx), adz = abs(dz);
	/* direção unitária (4096): o comprimento cabe em 32 bits com >> 2 */
	int len = SquareRoot0((adx >> 2) * (adx >> 2) + (adz >> 2) * (adz >> 2)) << 2;
	int ux = len ? (dx * 4096) / len : 0, uz = len ? (dz * 4096) / len : 0;

	int cx = x0 / TILE_SIZE, cz = z0 / TILE_SIZE;
	int ex = x1 / TILE_SIZE, ez = z1 / TILE_SIZE;
	int sx = dx > 0 ? 1 : -1, sz = dz > 0 ? 1 : -1;

	for (int steps = 0; steps < 2 * (MAP_MAX_W + MAP_MAX_H); steps++) {
		col_tests++;
		/* esta célula bloqueia? (a de partida e a de chegada não contam
		 * paredes: quem olha e quem é olhado estão ali) */
		int endpoint = (cx == x0 / TILE_SIZE && cz == z0 / TILE_SIZE) || (cx == ex && cz == ez);
		if (!endpoint && level_cell_solid(cx, cz) && !level_cell_low(cx, cz))
			return 0;
		if (cx >= 0 && cz >= 0 && cx < MAP_MAX_W && cz < MAP_MAX_H)
			for (int i = obst_head[cz][cx]; i >= 0; i = obst[i].next)
				if (seg_hits_circle(x0, z0, ux, uz, len, obst[i].x, obst[i].z, obst[i].r))
					return 0;
		if (cx == ex && cz == ez)
			return 1;

		/* próxima célula: cruza antes a borda em X ou em Z? Compara
		 * (bx - x0)/dx com (bz - z0)/dz sem dividir (produtos < 2^31) */
		int bx = sx > 0 ? (cx + 1) * TILE_SIZE : cx * TILE_SIZE;
		int bz = sz > 0 ? (cz + 1) * TILE_SIZE : cz * TILE_SIZE;
		int tx = abs(bx - x0) * adz, tz = abs(bz - z0) * adx;
		if (adx == 0)      cz += sz;
		else if (adz == 0) cx += sx;
		else if (tx < tz)  cx += sx;
		else if (tz < tx)  cz += sz;
		else {
			/* passa exatamente pelo canto: fechado se as duas vizinhas fecham */
			int a = level_cell_solid(cx + sx, cz) && !level_cell_low(cx + sx, cz);
			int b = level_cell_solid(cx, cz + sz) && !level_cell_low(cx, cz + sz);
			if (a && b) return 0;
			cx += sx;
			cz += sz;
		}
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* Depuração                                                           */
/* ------------------------------------------------------------------ */

/* A grade está batendo com g? Devolve quantas entidades estão fora da
 * célula certa (0 = tudo certo). Usado pelo ./dev bench. */
int collide_check_grid(void) {
	int errors = 0;
	for (int id = 0; id < MAX_ENT; id++) {
		int x, z, y, r, found = 0;
		int alive = ent_info(id, &x, &z, &y, &r);
		if (!alive) {
			if (ent_cx[id] >= 0) errors++;            /* morto mas registrado */
			continue;
		}
		int cx = x / TILE_SIZE, cz = z / TILE_SIZE;
		if (cx < 0 || cz < 0 || cx >= MAP_MAX_W || cz >= MAP_MAX_H) continue;
		for (int k = ent_head[cz][cx]; k >= 0; k = ent_next[k])
			if (k == id) found = 1;
		if (!found) errors++;
	}
	return errors;
}

/* Obstáculos fixos de uma célula (para os marcadores de depuração) */
int collide_cell_obstacle(int cx, int cz, int k, int *x, int *z, int *r) {
	if (cx < 0 || cz < 0 || cx >= MAP_MAX_W || cz >= MAP_MAX_H) return 0;
	for (int i = obst_head[cz][cx]; i >= 0; i = obst[i].next, k--)
		if (k == 0) {
			*x = obst[i].x; *z = obst[i].z; *r = obst[i].r;
			return 1;
		}
	return 0;
}
