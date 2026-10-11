/*
 * level.c - O mundo: grade de células, blocos montados sob demanda e colisão
 *
 * GRADE: até MAP_MAX_W x MAP_MAX_H células (1 célula = 1 metro = 256
 * unidades). Cada célula é 1 byte: o tipo (bits 0-5: chão de terra, parede,
 * mata densa...) e o bit CELL_SOLID (7). A fase vem de um mapa em texto
 * (levels.c) ou de uma função que preenche a grade por código.
 *
 * BLOCOS: a geometria NÃO é montada para o mapa todo (128x128 células dariam
 * dezenas de milhares de polígonos e muita RAM). Ela é montada em blocos de
 * CHUNK_CELLS x CHUNK_CELLS células, guardados num cache fixo de CHUNK_SLOTS:
 *   - todo quadro, o 3x3 de blocos em volta de cada jogador vivo é montado
 *     NA HORA (nunca há buraco no chão perto de quem joga);
 *   - o resto do 5x5 em volta do grupo entra numa fila: no máximo 1 bloco
 *     por quadro, do mais perto para o mais longe (esses ficam escondidos na
 *     névoa, então a montagem gradual não aparece e não há travadinha);
 *   - quando falta lugar, o bloco mais distante do grupo é reaproveitado.
 * O cache é centrado no jogador (não na direção da câmera): girar a câmera
 * não muda nada; correr troca no máximo uma fileira por vez.
 *
 * Isso é só desenho: nada aqui muda g. A colisão usa a grade, não os blocos.
 */
#include <string.h>
#include <hwregs_c.h>
#include "game.h"

static uint8_t cells[MAP_MAX_H][MAP_MAX_W];
static int     lw, lh;              /* tamanho do mapa atual, em células */
static int     cur_level = 0;       /* índice em level_defs */

/* ------------------------------------------------------------------ */
/* Tipos de célula                                                      */
/* ------------------------------------------------------------------ */

#define CELL_OUTSIDE  0xff          /* "tipo" de fora do mapa */

static int cell_type(int cx, int cz) {
	if (cx < 0 || cz < 0 || cx >= lw || cz >= lh) return CELL_OUTSIDE;
	return cells[cz][cx] & CELL_TYPE_MASK;
}

/* Altura em fatias de WALL_SLICE. Fora do mapa conta como "muito alto":
 * assim não se monta a face de trás das paredes da borda. */
static int type_slices(int t) {
	switch (t) {
	case CELL_THICKET: return THICKET_HEIGHT / WALL_SLICE;
	case CELL_BARRIER: return BARRIER_HEIGHT / WALL_SLICE;
	case CELL_OUTSIDE: return 99;
	}
	return 0;
}

static int type_has_floor(int t) {
	return t == CELL_DIRT || t == CELL_LEAVES || t == CELL_ROOTS;
}

static int type_solid(int t) {
	return t == CELL_VOID || t == CELL_THICKET || t == CELL_BARRIER;
}

/* Textura de cada tipo (índice na tabela world_tex) */
enum { WT_DIRT, WT_LEAVES, WT_ROOTS, WT_THICKET, NUM_WT };
static const TEXTURE *world_tex[NUM_WT];

static int type_tex(int t) {
	switch (t) {
	case CELL_LEAVES:  return WT_LEAVES;
	case CELL_ROOTS:   return WT_ROOTS;
	case CELL_BARRIER: return WT_ROOTS;      /* parede de raízes */
	case CELL_THICKET: return WT_THICKET;
	}
	return WT_DIRT;
}

/* ------------------------------------------------------------------ */
/* Montagem de um bloco                                                 */
/* ------------------------------------------------------------------ */

/* Tabela única de vértices: todos os pontos da grade de um bloco, em
 * CHUNK_LAYERS alturas, relativos ao canto do bloco. */
static SVECTOR chunk_verts[CHUNK_VERTS];

static int vidx(int ix, int iz, int iy) {
	return (iy * CHUNK_GRID + iz) * CHUNK_GRID + ix;
}

static const SVECTOR normals[5] = {
	{ 0, -ONE, 0, 0 },   /* 0: para cima */
	{ 0, 0, -ONE, 0 },   /* 1: norte (-Z) */
	{ 0, 0,  ONE, 0 },   /* 2: sul   (+Z) */
	{ -ONE, 0, 0, 0 },   /* 3: oeste (-X) */
	{  ONE, 0, 0, 0 },   /* 4: leste (+X) */
};

/* Hash de inteiros (só multiplicações e XOR de 32 bits): variação visual
 * fixa por posição, sem usar nenhum gerador aleatório. */
static uint32_t hash3(int x, int y, int z) {
	uint32_t h = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ (uint32_t)z * 83492791u;
	h ^= h >> 13;
	h *= 0x5bd1e995u;
	h ^= h >> 15;
	return h;
}

static int chunk_overflow;           /* quads que não couberam (depuração) */

/* Acrescenta um quad. corner[] = 4 pontos (ix, iz, iy) da grade do bloco em
 * ordem circular; a função acerta o sentido para a face ficar virada para a
 * normal n, converte para a ordem "Z" do PS1 e calcula a cor de cada vértice
 * (cor base x variação por posição x luz). gx0/gz0 = célula do canto do bloco. */
static void add_quad(CHUNK_GEOM *c, const int corner[4][3], int n, int uvset,
                     int tex, int rot, int zbias, int r, int g, int b, int gx0, int gz0) {
	if (c->nquads >= CHUNK_MAX_QUADS) {
		chunk_overflow++;
		return;
	}
	int order[4] = { 0, 1, 2, 3 };
	const SVECTOR *p0 = &chunk_verts[vidx(corner[0][0], corner[0][1], corner[0][2])];
	const SVECTOR *p1 = &chunk_verts[vidx(corner[1][0], corner[1][1], corner[1][2])];
	const SVECTOR *p2 = &chunk_verts[vidx(corner[2][0], corner[2][1], corner[2][2])];
	int ax = p1->vx - p0->vx, ay = p1->vy - p0->vy, az = p1->vz - p0->vz;
	int bx = p2->vx - p0->vx, by = p2->vy - p0->vy, bz = p2->vz - p0->vz;
	int wx = (ay * bz - az * by) >> 8;
	int wy = (az * bx - ax * bz) >> 8;
	int wz = (ax * by - ay * bx) >> 8;
	if (wx * normals[n].vx + wy * normals[n].vy + wz * normals[n].vz > 0) {
		order[1] = 3; order[3] = 1;          /* a GTE quer o sentido oposto */
	}
	static const int zorder[4] = { 0, 1, 3, 2 };   /* circular a,b,c,d -> a,b,d,c */

	CHUNK_QUAD *q = &c->quads[c->nquads++];
	q->corners = 0;
	for (int k = 0; k < 4; k++) {
		int i = order[zorder[k]];
		int ix = corner[i][0], iz = corner[i][1], iy = corner[i][2];
		q->v[k] = vidx(ix, iz, iy);
		q->corners |= i << (2 * k);
		/* variação sutil por vértice (+-8% de brilho e um toque de tom);
		 * vértices compartilhados por células vizinhas têm a mesma cor */
		uint32_t h = hash3(gx0 + ix, iy, gz0 + iz);
		int var = 118 + (int)(h % 21);                  /* 118..138 (/128) */
		int vr = (r * var >> 7) + (int)((h >> 8) % 7) - 3;
		int vb = (b * var >> 7) + (int)((h >> 12) % 7) - 3;
		render_bake_light(&normals[n], vr, g * var >> 7, vb, &q->c[k]);
	}
	q->tex = tex;
	q->uv = uvset;
	q->rot = rot;
	q->zbias = zbias;
}

static void build_chunk_begin(CHUNK_GEOM *c, int ccx, int ccz) {
	c->origin.vx = ccx * CHUNK_CELLS * TILE_SIZE;
	c->origin.vy = 0;
	c->origin.vz = ccz * CHUNK_CELLS * TILE_SIZE;
	c->nquads = 0;
}

/* Quads de UMA célula (lx, lz) do bloco (ccx, ccz). A montagem inteira e a
 * em fatias (etapa 06) usam a mesma função: resultado idêntico. */
static void build_chunk_cell(CHUNK_GEOM *c, int ccx, int ccz, int lx, int lz) {
	const int gx0 = ccx * CHUNK_CELLS, gz0 = ccz * CHUNK_CELLS;
	{
		int cx = gx0 + lx, cz = gz0 + lz;
		int t = cell_type(cx, cz);
		if (t == CELL_OUTSIDE) return;

		/* chão */
		if (type_has_floor(t)) {
			int corner[4][3] = { { lx, lz, 0 }, { lx, lz + 1, 0 }, { lx + 1, lz + 1, 0 }, { lx + 1, lz, 0 } };
			int rot = hash3(cx, 7, cz) & 3;                     /* textura girada: menos repetição */
			add_quad(c, corner, 0, 0, type_tex(t), rot, 6, 128, 128, 128, gx0, gz0);
		}

		/* paredes e mata: topo + laterais onde o vizinho é mais baixo */
		int h = type_slices(t);
		if (h == 0) return;
		{
			int corner[4][3] = { { lx, lz, h }, { lx, lz + 1, h }, { lx + 1, lz + 1, h }, { lx + 1, lz, h } };
			add_quad(c, corner, 0, 0, type_tex(t), 0, 0, 100, 100, 100, gx0, gz0);
		}
		static const int dir[4][3] = { { 0, -1, 1 }, { 0, 1, 2 }, { -1, 0, 3 }, { 1, 0, 4 } };
		for (int d = 0; d < 4; d++) {
			int nh = type_slices(cell_type(cx + dir[d][0], cz + dir[d][1]));
			int n = dir[d][2];
			for (int s = nh; s < h; s++) {
				int a = s, b2 = s + 1;
				int x0 = lx, x1 = lx + 1, z0 = lz, z1 = lz + 1;
				int corner[4][3];
				switch (n) {
				case 1: { int q[4][3] = { { x0, z0, a }, { x1, z0, a }, { x1, z0, b2 }, { x0, z0, b2 } }; memcpy(corner, q, sizeof(q)); break; }
				case 2: { int q[4][3] = { { x1, z1, a }, { x0, z1, a }, { x0, z1, b2 }, { x1, z1, b2 } }; memcpy(corner, q, sizeof(q)); break; }
				case 3: { int q[4][3] = { { x0, z1, a }, { x0, z0, a }, { x0, z0, b2 }, { x0, z1, b2 } }; memcpy(corner, q, sizeof(q)); break; }
				default:{ int q[4][3] = { { x1, z0, a }, { x1, z1, a }, { x1, z1, b2 }, { x1, z0, b2 } }; memcpy(corner, q, sizeof(q)); break; }
				}
				add_quad(c, corner, n, 1, type_tex(t), 0, 0, 128, 128, 128, gx0, gz0);
			}
		}
	}
}

static void build_chunk(CHUNK_GEOM *c, int ccx, int ccz) {
	build_chunk_begin(c, ccx, ccz);
	for (int lz = 0; lz < CHUNK_CELLS; lz++)
		for (int lx = 0; lx < CHUNK_CELLS; lx++)
			build_chunk_cell(c, ccx, ccz, lx, lz);
}

/* ------------------------------------------------------------------ */
/* Cache de blocos                                                      */
/* ------------------------------------------------------------------ */

#define CHUNK_SIZE   (CHUNK_CELLS * TILE_SIZE)
#define MAX_CCX      (MAP_MAX_W / CHUNK_CELLS)
#define MAX_CCZ      (MAP_MAX_H / CHUNK_CELLS)

static CHUNK_GEOM slot_geom[CHUNK_SLOTS];
static int16_t    slot_cx[CHUNK_SLOTS], slot_cz[CHUNK_SLOTS];   /* -1 = livre */
static int8_t     slot_of[MAX_CCZ][MAX_CCX];                    /* -1 = não montado */
static int        ncx, ncz;                                     /* blocos no mapa */
static int        builds_this_frame, loaded_count;

static void cache_reset(void) {
	for (int i = 0; i < CHUNK_SLOTS; i++) slot_cx[i] = slot_cz[i] = -1;
	memset(slot_of, -1, sizeof(slot_of));
	loaded_count = 0;
}

static int cheb(int ax, int az, int bx, int bz) {
	int dx = ax - bx, dz = az - bz;
	if (dx < 0) dx = -dx;
	if (dz < 0) dz = -dz;
	return dx > dz ? dx : dz;
}

/* Pontos de interesse deste quadro: blocos dos jogadores vivos e do grupo */
static int pcx[MAX_PLAYERS], pcz[MAX_PLAYERS], npl;
static int fcx, fcz;                 /* bloco do centro do grupo */

static int is_must(int cx, int cz) {
	for (int i = 0; i < npl; i++)
		if (cheb(cx, cz, pcx[i], pcz[i]) <= 1) return 1;
	return 0;
}

/* Monta o bloco (cx, cz) se ainda não estiver montado */
/* Contador de linhas do PS1 (timer 1). No teste do PC (HOST_TEST) não há
 * esse hardware: vale 0. */
#ifdef HOST_TEST
#define LINE_TIMER() 0
#else
#define LINE_TIMER() TIMER_VALUE(1)
#endif

static int build_lines_max;           /* perfil: pior montagem de bloco (linhas) */

void level_build_stats(int *max_lines) {      /* zera ao ler */
	*max_lines = build_lines_max;
	build_lines_max = 0;
}

static void ensure(int cx, int cz) {
	if (cx < 0 || cz < 0 || cx >= ncx || cz >= ncz || slot_of[cz][cx] >= 0)
		return;
	int s = -1, worst = -1;
	for (int i = 0; i < CHUNK_SLOTS; i++) {
		if (slot_cx[i] < 0) { s = i; break; }                 /* livre */
		if (is_must(slot_cx[i], slot_cz[i])) continue;        /* nunca tira o do jogador */
		int d = cheb(slot_cx[i], slot_cz[i], fcx, fcz);
		if (d > worst) { worst = d; s = i; }                  /* o mais longe do grupo */
	}
	if (s < 0) return;
	if (slot_cx[s] >= 0) {
		slot_of[slot_cz[s]][slot_cx[s]] = -1;
		loaded_count--;
	}
	uint16_t t0 = LINE_TIMER();
	build_chunk(&slot_geom[s], cx, cz);
	int bl = (uint16_t)(LINE_TIMER() - t0);
	if (bl > build_lines_max) build_lines_max = bl;
	slot_cx[s] = cx;
	slot_cz[s] = cz;
	slot_of[cz][cx] = s;
	loaded_count++;
	builds_this_frame++;
}

/* Decide o que montar neste quadro (só leitura de g) */
static void stream_update(void) {
	builds_this_frame = 0;
	npl = 0;
	int sx = 0, sz = 0;
	for (int i = 0; i < MAX_PLAYERS; i++) {
		const PLAYER *p = &g.players[i];
		if (!player_alive(p)) continue;
		pcx[npl] = p->pos.vx / CHUNK_SIZE;
		pcz[npl] = p->pos.vz / CHUNK_SIZE;
		sx += p->pos.vx;
		sz += p->pos.vz;
		npl++;
	}
	if (npl) {
		fcx = (sx / npl) / CHUNK_SIZE;
		fcz = (sz / npl) / CHUNK_SIZE;
	} else {                          /* título: centra na câmera */
		const VECTOR *cp = render_camera_pos();
		fcx = cp->vx / CHUNK_SIZE;
		fcz = cp->vz / CHUNK_SIZE;
	}

	/* 1) obrigatórios: 3x3 em volta de cada jogador, já neste quadro */
	for (int i = 0; i < npl; i++)
		for (int dz = -1; dz <= 1; dz++)
			for (int dx = -1; dx <= 1; dx++)
				ensure(pcx[i] + dx, pcz[i] + dz);

	/* 2) fila: o bloco faltante mais perto do grupo dentro do raio */
	int best = -1, bx = 0, bz = 0;
	for (int dz = -CHUNK_LOAD_RADIUS; dz <= CHUNK_LOAD_RADIUS; dz++)
		for (int dx = -CHUNK_LOAD_RADIUS; dx <= CHUNK_LOAD_RADIUS; dx++) {
			int cx = fcx + dx, cz = fcz + dz;
			if (cx < 0 || cz < 0 || cx >= ncx || cz >= ncz || slot_of[cz][cx] >= 0)
				continue;
			int d = dx * dx + dz * dz;
			if (best < 0 || d < best) { best = d; bx = cx; bz = cz; }
		}
	if (best >= 0 && builds_this_frame < CHUNK_BUILDS_PER_FRAME)
		ensure(bx, bz);
}

/* Memória de rascunho para a geração do mapa: a do CACHE DE BLOCOS, que
 * está livre dentro do level_load antes de os blocos serem montados (o
 * mapgen roda nesse intervalo). Fora dele, NÃO use: o cache está em uso. */
void *level_scratch(int bytes) {
	return bytes <= (int)sizeof(slot_geom) ? (void *)slot_geom : NULL;
}

/* Mapa mudou na célula (cx, cz): o bloco dela é remontado (etapa 06) */
void level_invalidate(int cx, int cz) {
	int bx = cx / CHUNK_CELLS, bz = cz / CHUNK_CELLS;
	if (cx < 0 || cz < 0 || bx >= ncx || bz >= ncz) return;
	int s = slot_of[bz][bx];
	if (s < 0) return;
	slot_of[bz][bx] = -1;
	slot_cx[s] = slot_cz[s] = -1;
	loaded_count--;
}

/* Só os bits de colisão (CELL_SOLID / CELL_LOW) de uma célula, sem mudar o
 * tipo: o desenho do bloco não depende deles, então NÃO remonta (árvore
 * que caiu; etapa 06). flags = 0 limpa os dois. */
void level_set_flags(int cx, int cz, int flags) {
	if (cx < 0 || cz < 0 || cx >= lw || cz >= lh) return;
	int t = cells[cz][cx] & CELL_TYPE_MASK;
	cells[cz][cx] = t | flags | (type_solid(t) ? CELL_SOLID : 0);
}

/* ------------------------------------------------------------------ */
/* Remontagem EM FATIAS (etapa 06): o TIPO de uma célula mudou (barreira */
/* de raízes aberta). Montar um bloco inteiro custa 60-190 linhas e, num */
/* quadro já pesado, perde os 30 FPS. Aqui ele é montado célula a célula */
/* num buffer reserva, até REBUILD_LINES linhas por quadro; o bloco antigo */
/* continua desenhado até o novo ficar pronto (a COLISÃO já mudou na hora, */
/* na lógica: só o desenho atrasa alguns quadros).                       */
/* ------------------------------------------------------------------ */
static CHUNK_GEOM rebuild_geom;              /* buffer reserva */
static int8_t     rebuild_q[REBUILD_QUEUE][2];   /* blocos na fila */
static int        rebuild_n, rebuild_cell = -1;

void level_rebuild_chunk(int cx, int cz) {
	int bx = cx / CHUNK_CELLS, bz = cz / CHUNK_CELLS;
	if (cx < 0 || cz < 0 || bx >= ncx || bz >= ncz) return;
	for (int i = 0; i < rebuild_n; i++)
		if (rebuild_q[i][0] == bx && rebuild_q[i][1] == bz) return;
	if (rebuild_n >= REBUILD_QUEUE) {          /* fila cheia: o jeito antigo */
		level_invalidate(cx, cz);
		return;
	}
	rebuild_q[rebuild_n][0] = bx;
	rebuild_q[rebuild_n][1] = bz;
	rebuild_n++;
}

static void rebuild_step(void) {
	if (rebuild_n == 0) return;
	int bx = rebuild_q[0][0], bz = rebuild_q[0][1];
	if (slot_of[bz][bx] < 0) {                 /* não está montado: nada a fazer */
		rebuild_cell = -1;                     /* (quando montar, já vem novo) */
		goto next;
	}
	if (rebuild_cell < 0) {
		build_chunk_begin(&rebuild_geom, bx, bz);
		rebuild_cell = 0;
	}
	{
		/* orçamento ADAPTATIVO: só a folga que o quadro anterior deixou até
		 * REBUILD_CPU_CAP (no máximo REBUILD_LINES). Antes de cada célula,
		 * estima o custo pela pior célula vista: não passa do orçamento.
		 * Sem folga nenhuma por 60 quadros, monta 1 célula (não trava). */
		static int cell_cost = 4, starved;
		int pc, pg, pv;
		render_stats_time(&pc, &pg, &pv);
		int budget = REBUILD_CPU_CAP - pc;
		if (budget > REBUILD_LINES) budget = REBUILD_LINES;
		if (budget < cell_cost && ++starved < 60) return;
		starved = 0;
		uint16_t t0 = LINE_TIMER();
		do {
			uint16_t tc = LINE_TIMER();
			build_chunk_cell(&rebuild_geom, bx, bz, rebuild_cell % CHUNK_CELLS, rebuild_cell / CHUNK_CELLS);
			int c = (uint16_t)(LINE_TIMER() - tc);
			if (c > cell_cost) cell_cost = c;
			rebuild_cell++;
		} while (rebuild_cell < CHUNK_CELLS * CHUNK_CELLS &&
		         (uint16_t)(LINE_TIMER() - t0) + cell_cost <= budget);
		int bl = (uint16_t)(LINE_TIMER() - t0);
		if (bl > build_lines_max) build_lines_max = bl;
	}
	if (rebuild_cell < CHUNK_CELLS * CHUNK_CELLS) return;
	slot_geom[slot_of[bz][bx]] = rebuild_geom; /* pronto: troca (cópia de ~8 KB) */
	rebuild_cell = -1;
next:
	rebuild_n--;
	for (int i = 0; i < rebuild_n; i++) {
		rebuild_q[i][0] = rebuild_q[i + 1][0];
		rebuild_q[i][1] = rebuild_q[i + 1][1];
	}
}

void level_stats(int *loaded, int *slots, int *built, int *overflow) {
	*loaded = loaded_count;
	*slots = CHUNK_SLOTS;
	*built = builds_this_frame;
	*overflow = chunk_overflow;
}

/* ------------------------------------------------------------------ */
/* Árvores: guardadas por bloco                                         */
/* ------------------------------------------------------------------ */
/* Centenas de árvores não cabem (nem precisam) no pool de props, que a
 * colisão percorre inteiro. Aqui elas são só desenho, 8 bytes cada,
 * ordenadas por bloco: o desenho percorre só os blocos perto do grupo.
 * A colisão é pela grade: o tronco marca a célula como sólida (provisório
 * até a grade espacial da 04c). */
typedef struct {
	uint16_t x, z;          /* posição no mundo (0..32767) */
	uint8_t  type;          /* TREE_* */
	uint8_t  rot;           /* ângulo / 16 */
	uint8_t  scale;         /* escala / 64 (64 = 1.0) */
	uint8_t  occlude;       /* tapou o jogador: passos restantes a 50% (visual) */
} TREE;

static TREE     trees[MAX_TREES], tree_tmp[MAX_TREES];
static int      num_trees;
static uint16_t tree_start[MAX_CCZ][MAX_CCX], tree_count[MAX_CCZ][MAX_CCX];
static int      trees_drawn_mesh, trees_drawn_bb, trees_drawn_trans;
static int      cut_chunks, cut_trees;   /* cortados por falta de buffer (L2) */
static int      occl_ticks;     /* passos do quadro (contagem regressiva da transparência) */
static CVECTOR  bb_tint;        /* cor das imagens planas (luz da fase) */

void level_add_tree(int type, int x, int z, int angle, int scale) {
	if (num_trees >= MAX_TREES || type < 0 || type >= NUM_TREE_TYPES)
		return;
	if (x < 0 || z < 0 || x >= lw * TILE_SIZE || z >= lh * TILE_SIZE)
		return;
	TREE *t = &trees[num_trees++];
	t->x = x;
	t->z = z;
	t->type = type;
	t->rot = (angle & 4095) >> 4;
	t->scale = scale >> 6;
	t->occlude = 0;

	/* colisão: em pé = círculo do tronco na grade espacial (collision.c),
	 * redondo para o jogador contornar suave; deitado = 3 células sólidas
	 * BAIXAS ao longo do comprimento (eixo pelo ângulo arredondado): dá para
	 * pular por cima, tiros param, a visão passa */
	int cx = x / TILE_SIZE, cz = z / TILE_SIZE;
	if (tree_defs[type].cells == 0) {
		/* deitada (evento): a colisão vem das células, marcada por events.c */
	} else if (tree_defs[type].cells == 3) {
		int along_z = ((angle + 512) >> 10) & 1;   /* 1024/3072: comprido em Z */
		for (int k = -1; k <= 1; k++) {
			int lx = cx + (along_z ? 0 : k), lz = cz + (along_z ? k : 0);
			if (lx >= 0 && lz >= 0 && lx < lw && lz < lh)
				cells[lz][lx] |= CELL_SOLID | CELL_LOW;
		}
	} else {
		collide_add_obstacle(x, z, (tree_defs[type].trunk_r * scale) >> 12);
	}
}

/* Ordena por bloco (contagem + preenchimento, estável) */
static void trees_finish(void) {
	memset(tree_count, 0, sizeof(tree_count));
	for (int i = 0; i < num_trees; i++)
		tree_count[trees[i].z / CHUNK_SIZE][trees[i].x / CHUNK_SIZE]++;
	int pos = 0;
	for (int bz = 0; bz < MAX_CCZ; bz++)
		for (int bx = 0; bx < MAX_CCX; bx++) {
			tree_start[bz][bx] = pos;
			pos += tree_count[bz][bx];
		}
	uint16_t fill[MAX_CCZ][MAX_CCX];
	memcpy(fill, tree_start, sizeof(fill));
	for (int i = 0; i < num_trees; i++)
		tree_tmp[fill[trees[i].z / CHUNK_SIZE][trees[i].x / CHUNK_SIZE]++] = trees[i];
	memcpy(trees, tree_tmp, num_trees * sizeof(TREE));
}

/* ------------------------------------------------------------------ */
/* Árvores em tempo de jogo (eventos; etapa 06). Fora de g, como as do   */
/* mapa: são função determinística da semente + eventos (lógica).        */
/* ------------------------------------------------------------------ */

/* Árvore EM PÉ (círculo de tronco) na célula (cx, cz)? Devolve o índice. */
int level_standing_tree_at(int cx, int cz) {
	int bx = cx / CHUNK_CELLS, bz = cz / CHUNK_CELLS;
	if (cx < 0 || cz < 0 || bx >= MAX_CCX || bz >= MAX_CCZ) return -1;
	for (int i = tree_start[bz][bx], n = tree_count[bz][bx]; n > 0; n--, i++)
		if (tree_defs[trees[i].type].cells == 1 &&
		    trees[i].x / TILE_SIZE == cx && trees[i].z / TILE_SIZE == cz)
			return i;
	return -1;
}

void level_tree_info(int i, int *x, int *z, int *type, int *scale, int *angle) {
	*x = trees[i].x;
	*z = trees[i].z;
	*type = trees[i].type;
	*scale = trees[i].scale << 6;
	*angle = trees[i].rot << 4;
}

/* Árvores em pé no bloco (bx, bz): os tetos da 05 valem para as novas */
int level_chunk_standing(int bx, int bz) {
	if (bx < 0 || bz < 0 || bx >= MAX_CCX || bz >= MAX_CCZ) return 0;
	int n = 0;
	for (int i = tree_start[bz][bx], k = tree_count[bz][bx]; k > 0; k--, i++)
		n += tree_defs[trees[i].type].cells == 1;
	return n;
}

/* Tira a árvore i (e o obstáculo dela). Operação LOCAL: o vetor está
 * ordenado por bloco; desloca o resto uma posição e acerta o início dos
 * blocos seguintes (reordenar ~1000 árvores custava ~80 linhas). */
void level_remove_tree(int i) {
	if (i < 0 || i >= num_trees) return;
	if (tree_defs[trees[i].type].cells == 1)
		collide_remove_obstacle(trees[i].x, trees[i].z);
	int bx = trees[i].x / CHUNK_SIZE, bz = trees[i].z / CHUNK_SIZE;
	memmove(&trees[i], &trees[i + 1], (num_trees - i - 1) * sizeof(TREE));
	num_trees--;
	tree_count[bz][bx]--;
	for (int z = bz; z < MAX_CCZ; z++)
		for (int x = (z == bz ? bx + 1 : 0); x < MAX_CCX; x++)
			tree_start[z][x]--;
}

/* Depois de level_add_tree em tempo de jogo: a árvore nova (a última do
 * vetor) vai para o lugar do bloco dela, sem reordenar tudo */
void level_trees_changed(void) {
	if (num_trees == 0) return;
	TREE t = trees[num_trees - 1];
	int bx = t.x / CHUNK_SIZE, bz = t.z / CHUNK_SIZE;
	int pos = tree_start[bz][bx] + tree_count[bz][bx];   /* fim do bloco dela */
	memmove(&trees[pos + 1], &trees[pos], (num_trees - 1 - pos) * sizeof(TREE));
	trees[pos] = t;
	tree_count[bz][bx]++;
	for (int z = bz; z < MAX_CCZ; z++)
		for (int x = (z == bz ? bx + 1 : 0); x < MAX_CCX; x++)
			tree_start[z][x]++;
}

/* Árvores do bloco (bx, bz): índice da primeira e quantas */
int level_chunk_trees(int bx, int bz, int *start) {
	if (bx < 0 || bz < 0 || bx >= MAX_CCX || bz >= MAX_CCZ) { *start = 0; return 0; }
	*start = tree_start[bz][bx];
	return tree_count[bz][bx];
}

int level_tree_room(void) {
	return MAX_TREES - num_trees;
}

/* Só os blocos do 5x5 em volta do grupo, em ANÉIS do centro para fora
 * (perto -> longe): se o buffer de primitivas acabar, o cortado é o mais
 * distante. Perto = modelo, longe = imagem plana. */
static void trees_draw(void) {
	DRAWOPT opt = { 0 }, trans = { 0 };
	trans.flags = DRAW_SEMITRANS;   /* modelos sem textura: 50% no polígono todo */
	trees_drawn_mesh = trees_drawn_bb = trees_drawn_trans = 0;
	cut_trees = 0;
	const int budget = PACKET_LEN - PACKET_RESERVE;
	for (int ring = 0; ring <= CHUNK_LOAD_RADIUS; ring++)
	for (int bz = fcz - ring; bz <= fcz + ring; bz++)
		for (int bx = fcx - ring; bx <= fcx + ring; bx++) {
			if (bx != fcx - ring && bx != fcx + ring && bz != fcz - ring && bz != fcz + ring)
				continue;                    /* só a borda do anel */
			if (bx < 0 || bz < 0 || bx >= ncx || bz >= ncz)
				continue;
			/* buffer quase cheio: as árvores deste bloco (e dos mais longe)
			 * ficam de fora. Testado por bloco, não por árvore (mais barato;
			 * um bloco a mais cabe na reserva) */
			if (render_bytes_used() > budget) {
				cut_trees += tree_count[bz][bx];
				continue;
			}
			const TREE *t = &trees[tree_start[bz][bx]];
			for (int n = tree_count[bz][bx]; n > 0; n--, t++) {
				const TREE_DEF *d = &tree_defs[t->type];
				int scale = t->scale << 6;
				int depth = render_view_depth(t->x, -256, t->z);
				TREE *tw = (TREE *)t;            /* contagem regressiva da transparência */
				if (tw->occlude)
					tw->occlude = tw->occlude > occl_ticks ? tw->occlude - occl_ticks : 0;
				if (t->occlude) {
					/* tapa o jogador: SEMPRE modelo (sem textura, a 50%).
					 * A imagem plana tem textura e, nela, a semitransparência
					 * só vale nos texels com o bit STP da CLUT (aqui, quase
					 * nenhum): ela continuaria opaca. */
					VECTOR  pos = { t->x, 0, t->z };
					SVECTOR rot = { 0, t->rot << 4, 0 };
					int ok = render_mesh(d->mesh, &pos, &rot, scale, &trans);
					trees_drawn_mesh += ok;
					trees_drawn_trans += ok;
					continue;
				}
				if (d->pitch) {                  /* caída: sempre modelo (são poucas) */
					VECTOR  pos = { t->x, 0, t->z };
					SVECTOR rot = { d->pitch, t->rot << 4, 0 };
					trees_drawn_mesh += render_mesh(d->mesh, &pos, &rot, scale, &opt);
					continue;
				}
				if (depth > TREE_LOD_DIST) {
					if (d->bb)
						trees_drawn_bb += render_billboard(t->x, t->z, (d->bb_w * scale) >> 12,
						                                   (d->bb_h * scale) >> 12, d->bb, &bb_tint);
					continue;       /* sem imagem plana (tronco): some de longe */
				}
				VECTOR  pos = { t->x, 0, t->z };
				SVECTOR rot = { 0, t->rot << 4, 0 };
				trees_drawn_mesh += render_mesh(d->mesh, &pos, &rot, scale, &opt);
			}
		}
}

/* Quantas árvores em pé há a até 'radius' de (x, z), mais 1 a cada 4
 * células de mata alta: a câmera se aproxima onde a floresta fecha.
 * Percorre só os blocos que o círculo toca (visual: não muda g). */
int level_tree_density(int x, int z, int radius) {
	int n = 0, r2 = radius * radius;
	int bx0 = (x - radius) / CHUNK_SIZE, bx1 = (x + radius) / CHUNK_SIZE;
	int bz0 = (z - radius) / CHUNK_SIZE, bz1 = (z + radius) / CHUNK_SIZE;
	for (int bz = bz0; bz <= bz1; bz++)
		for (int bx = bx0; bx <= bx1; bx++) {
			if (bx < 0 || bz < 0 || bx >= ncx || bz >= ncz) continue;
			const TREE *t = &trees[tree_start[bz][bx]];
			for (int k = tree_count[bz][bx]; k > 0; k--, t++) {
				int dx = t->x - x, dz = t->z - z;
				if (tree_defs[t->type].cells == 1 && dx * dx + dz * dz < r2)
					n++;
			}
		}
	int thicket = 0, rc = radius / TILE_SIZE;
	for (int cz = z / TILE_SIZE - rc; cz <= z / TILE_SIZE + rc; cz++)
		for (int cx = x / TILE_SIZE - rc; cx <= x / TILE_SIZE + rc; cx++)
			if (cell_type(cx, cz) == CELL_THICKET)
				thicket++;
	return n + thicket / 4;
}

int level_tree_transparent(void) {
	return trees_drawn_trans;
}

/* Marca as árvores em pé cuja copa cruza algum segmento olho -> jogador
 * (no plano X/Z), mais perto do olho que o jogador. Percorre só os blocos
 * que a caixa dos segmentos toca. Visual (não muda g). */
void level_mark_occluders(int ex, int ez, const int *x, const int *z, int n, int ticks) {
	occl_ticks = ticks;
	int minx = ex, maxx = ex, minz = ez, maxz = ez;
	for (int i = 0; i < n; i++) {
		if (x[i] < minx) minx = x[i];
		if (x[i] > maxx) maxx = x[i];
		if (z[i] < minz) minz = z[i];
		if (z[i] > maxz) maxz = z[i];
	}
	const int margin = 320;              /* maior copa x escala */
	int bx0 = (minx - margin) / CHUNK_SIZE, bx1 = (maxx + margin) / CHUNK_SIZE;
	int bz0 = (minz - margin) / CHUNK_SIZE, bz1 = (maxz + margin) / CHUNK_SIZE;
	if (bx0 < 0) bx0 = 0;
	if (bz0 < 0) bz0 = 0;
	for (int bz = bz0; bz <= bz1 && bz < ncz; bz++)
		for (int bx = bx0; bx <= bx1 && bx < ncx; bx++) {
			TREE *t = &trees[tree_start[bz][bx]];
			for (int k = tree_count[bz][bx]; k > 0; k--, t++) {
				const TREE_DEF *d = &tree_defs[t->type];
				if (d->cells != 1 || !d->canopy_r)
					continue;
				int r = (d->canopy_r * (t->scale << 6)) >> 12;
				for (int i = 0; i < n; i++) {
					/* distância da copa ao segmento olho -> jogador i */
					int sx = x[i] - ex, sz = z[i] - ez;
					int len = dist2d(sx, sz);
					if (len <= 0) continue;
					int ux = (sx * 4096) / len, uz = (sz * 4096) / len;
					int rx = t->x - ex, rz = t->z - ez;
					int along = (rx * ux + rz * uz) >> 12;
					if (along <= 0 || along >= len - PLAYER_RADIUS)
						continue;            /* atrás do olho ou além do jogador */
					int side = (rx * uz - rz * ux) >> 12;
					if (side < r + PLAYER_RADIUS && side > -(r + PLAYER_RADIUS))
						t->occlude = TREE_OCCLUDE_TICKS;
				}
			}
		}
}

/* Quantos blocos e árvores ficaram de fora por falta de buffer no quadro */
int level_cut_count(void) {
	return cut_chunks + cut_trees;
}

void level_tree_stats(int *total, int *models, int *billboards) {
	*total = num_trees;
	*models = trees_drawn_mesh;
	*billboards = trees_drawn_bb;
}

/* ------------------------------------------------------------------ */
/* Carregar a fase                                                      */
/* ------------------------------------------------------------------ */

void level_begin(int w, int h) {
	if (w > MAP_MAX_W) w = MAP_MAX_W;
	if (h > MAP_MAX_H) h = MAP_MAX_H;
	lw = w;
	lh = h;
	memset(cells, CELL_VOID | CELL_SOLID, sizeof(cells));
}

void level_set_cell(int cx, int cz, int type) {
	if (cx < 0 || cz < 0 || cx >= lw || cz >= lh) return;
	cells[cz][cx] = type | (type_solid(type) ? CELL_SOLID : 0);
}

int level_cell_type(int cx, int cz) {
	int t = cell_type(cx, cz);
	return t == CELL_OUTSIDE ? CELL_VOID : t;
}

/* Cria o que uma letra do mapa representa na célula (cx, cz). Usado pelos
 * mapas de texto e pelos mapas feitos por código. */
void level_place(char ch, int cx, int cz) {
	int wx = cx * TILE_SIZE + TILE_SIZE / 2;
	int wz = cz * TILE_SIZE + TILE_SIZE / 2;
	switch (ch) {
	case 'P': g.spawn_x = wx; g.spawn_z = wz;  break;   /* jogadores nascem aqui */
	case 'C': crate_spawn(cx, cz);             break;
	case 'G': pickup_spawn(PICK_GEM, wx, wz);  break;
	case 'H': pickup_spawn(PICK_HEALTH, wx, wz); break;
	case 'N': pickup_spawn(PICK_ENERGY, wx, wz); break;
	case 'W': pickup_spawn(PICK_WEAPON, wx, wz); break;
	case 'Q': pickup_spawn(PICK_QUEST, wx, wz);  break;   /* item de missão */
	case 'L': pickup_spawn(PICK_BATTERY, wx, wz); break;  /* pilha da lanterna */
	case 'K': pickup_spawn(PICK_KEY, wx, wz);     break;  /* chave (barreira de raízes) */
	case 'X': g.has_exit = 1; g.exit_x = wx; g.exit_z = wz; break;  /* saída */
	case 'S':                                         /* ponto de reforço */
		if (g.num_spawn_pts < MAX_SPAWN_PTS) {
			g.spawn_pts[g.num_spawn_pts].x = wx;
			g.spawn_pts[g.num_spawn_pts].z = wz;
			g.num_spawn_pts++;
		}
		break;
	default:
		for (int t = 0; t < num_enemy_types; t++)
			if (enemy_defs[t].map_char == ch)
				enemy_spawn(t, wx, wz);
		for (int t = 0; t < num_props; t++)
			if (prop_defs[t].map_char == ch)
				prop_spawn(t, cx, cz);
		break;
	}
}

/* ------------------------------------------------------------------ */
/* Posicionamento seguro (mapas por código)                            */
/* ------------------------------------------------------------------ */
/* Raio de quem a letra cria: inimigo (pelo tipo), jogador ou item */
static int place_radius(char ch) {
	for (int t = 0; t < num_enemy_types; t++)
		if (enemy_defs[t].map_char == ch)
			return (90 * enemy_defs[t].scale) >> 12;
	if (ch == 'P') return PLAYER_RADIUS + 16;
	return 80;                           /* itens, saída, ponto de reforço */
}

static int place_ok(int cx, int cz, int r) {
	if (cx < 0 || cz < 0 || cx >= lw || cz >= lh) return 0;
	if (level_cell_solid(cx, cz)) return 0;          /* mata, tronco caído, vazio */
	return !collide_solid_at(cx * TILE_SIZE + TILE_SIZE / 2, cz * TILE_SIZE + TILE_SIZE / 2, r);
}

/* Lista de tudo que foi colocado e das falhas (o teste do PC confere:
 * nenhuma falha e tudo alcançável a partir do início) */
#define MAX_PLACED 96
static struct { char ch; int16_t cx, cz; } placed[MAX_PLACED], failed[8];
static int num_placed, num_failed;

/* Coloca a letra na célula livre mais próxima de (cx, cz), procurando em
 * anéis de até PLACE_SEARCH células. Nada nasce dentro de tronco, mata ou
 * tronco caído. Se não achar, NÃO coloca (registra a falha) e devolve 0. */
int level_place_safe(char ch, int cx, int cz) {
	int r = place_radius(ch);
	for (int ring = 0; ring <= PLACE_SEARCH; ring++)
		for (int dz = -ring; dz <= ring; dz++)
			for (int dx = -ring; dx <= ring; dx++) {
				if (dx != -ring && dx != ring && dz != -ring && dz != ring)
					continue;                /* só a borda do anel */
				if (!place_ok(cx + dx, cz + dz, r))
					continue;
				level_place(ch, cx + dx, cz + dz);
				if (num_placed < MAX_PLACED) {
					placed[num_placed].ch = ch;
					placed[num_placed].cx = cx + dx;
					placed[num_placed].cz = cz + dz;
					num_placed++;
				}
				return 1;
			}
	if (num_failed < 8) {
		failed[num_failed].ch = ch;
		failed[num_failed].cx = cx;
		failed[num_failed].cz = cz;
	}
	num_failed++;
	return 0;
}

int level_place_failures(int i, char *ch, int *cx, int *cz) {
	if (i >= num_failed || i >= 8) return num_failed;
	*ch = failed[i].ch; *cx = failed[i].cx; *cz = failed[i].cz;
	return num_failed;
}

int level_placed(int i, char *ch, int *cx, int *cz) {
	if (i >= num_placed) return num_placed;
	*ch = placed[i].ch; *cx = placed[i].cx; *cz = placed[i].cz;
	return num_placed;
}

/* Começo de uma montagem de fase: sem árvores, sem colocados, grade de
 * colisão vazia (o mapgen chama a cada tentativa) */
void level_reset_objects(void) {
	num_trees = 0;
	num_placed = num_failed = 0;
	collide_reset();
}

static int load_lines;                 /* tempo do último level_load (linhas) */


int level_load_lines(void) {
	return load_lines;
}

void level_load(int index) {
	uint16_t t_load = LINE_TIMER();    /* perfil: quanto a carga leva no console */
	const LEVEL_DEF *ld = &level_defs[index];
	cur_level = index;
	level_apply_look();               /* a luz entra na cor dos blocos */

#ifdef DEBUG_BENCHMARK
	if (ld->gen && !bench_force_fixed) {
#else
	if (ld->gen) {
#endif
		mapgen_build(index);          /* gera (com novas tentativas e plano B) */
	} else {
		level_reset_objects();        /* grade espacial vazia antes de criar objetos */
		ld->build();                  /* fase fixa (levels.c) */
		uint16_t t_fix = LINE_TIMER();
		mapgen_mark_fixed();          /* no bench também mede a floresta: */
		t_load += (uint16_t)(LINE_TIMER() - t_fix);   /* fora do tempo de carga */
	}
	trees_finish();

	/* textura de cada tipo de terreno */
	world_tex[WT_DIRT]    = &tex_terra_t;
	world_tex[WT_LEAVES]  = &tex_folhas_t;
	world_tex[WT_ROOTS]   = &tex_raizes_t;
	world_tex[WT_THICKET] = &tex_mata_t;

	for (int iy = 0; iy < CHUNK_LAYERS; iy++)
		for (int iz = 0; iz < CHUNK_GRID; iz++)
			for (int ix = 0; ix < CHUNK_GRID; ix++) {
				SVECTOR *v = &chunk_verts[vidx(ix, iz, iy)];
				v->vx = ix * TILE_SIZE;
				v->vy = -iy * WALL_SLICE;
				v->vz = iz * TILE_SIZE;
			}
	render_chunk_setup(chunk_verts, world_tex, NUM_WT);

	/* monta já tudo em volta do início, para o primeiro quadro estar completo */
	ncx = (lw + CHUNK_CELLS - 1) / CHUNK_CELLS;
	ncz = (lh + CHUNK_CELLS - 1) / CHUNK_CELLS;
	cache_reset();
	rebuild_n = 0;                    /* remontagens em fatias pendentes: nenhuma */
	rebuild_cell = -1;
	chunk_overflow = 0;
	fcx = g.spawn_x / CHUNK_SIZE;
	fcz = g.spawn_z / CHUNK_SIZE;
	npl = 0;
	for (int dz = -CHUNK_LOAD_RADIUS; dz <= CHUNK_LOAD_RADIUS; dz++)
		for (int dx = -CHUNK_LOAD_RADIUS; dx <= CHUNK_LOAD_RADIUS; dx++)
			ensure(fcx + dx, fcz + dz);
	load_lines = (uint16_t)(LINE_TIMER() - t_load);     /* < 65536 linhas (~4 s) */
}

/* Névoa da fase: a da tabela, ou a do evento "névoa avança" (g.ev.fog_far,
 * lógica). O near acompanha na mesma proporção. Só desenho: lê g. */
void level_apply_fog(void) {
	const LEVEL_DEF *ld = &level_defs[cur_level];
	int far = g.ev.fog_far > 0 ? g.ev.fog_far : ld->fog_far;
	render_set_fog(ld->fog_near * far / ld->fog_far, far, ld->sky_r, ld->sky_g, ld->sky_b);
}

/* Névoa, céu e luz da fase atual (também chamado ao voltar dos menus) */
void level_apply_look(void) {
	const LEVEL_DEF *ld = &level_defs[cur_level];
	level_apply_fog();
	render_set_light(ld->amb_r, ld->amb_g, ld->amb_b, MOON_R, MOON_G, MOON_B);
	/* imagens planas: a luz média de uma face de lado (sem normal própria) */
	static const SVECTOR side = { ONE * 7 / 10, 0, -ONE * 7 / 10, 0 };
	render_bake_light(&side, 128, 128, 128, &bb_tint);
	bb_tint.cd = 0;
}

void level_draw(void) {
	prof_begin(PROF_CHUNKS);
	stream_update();
	rebuild_step();
	/* do mais perto para o mais longe da câmera (inserção: são <= 36) */
	const VECTOR *cam = render_camera_pos();
	int order[CHUNK_SLOTS], dist[CHUNK_SLOTS], n = 0;
	for (int i = 0; i < CHUNK_SLOTS; i++) {
		if (slot_cx[i] < 0) continue;
		int d = dist2d(slot_geom[i].origin.vx + CHUNK_SIZE / 2 - cam->vx,
		               slot_geom[i].origin.vz + CHUNK_SIZE / 2 - cam->vz);
		int k = n++;
		while (k > 0 && dist[k - 1] > d) { dist[k] = dist[k - 1]; order[k] = order[k - 1]; k--; }
		dist[k] = d;
		order[k] = i;
	}
	cut_chunks = 0;
	const int budget = PACKET_LEN - PACKET_RESERVE - PACKET_TREE_ROOM;
	for (int k = 0; k < n; k++) {
		if (render_bytes_used() > budget) {
			cut_chunks = n - k;              /* o resto é o mais distante */
			break;
		}
		render_chunk(&slot_geom[order[k]]);
	}
	prof_end(PROF_CHUNKS);
	prof_begin(PROF_TREES);
	trees_draw();
	prof_end(PROF_TREES);
}

/* ------------------------------------------------------------------ */
/* Colisão                                                             */
/* ------------------------------------------------------------------ */

/* Byte cru da célula (tipo | CELL_LOW | CELL_SOLID); fora do mapa: sólido.
 * Uma chamada só, para a checagem de alcance (reach.c) ser barata. */
/* A grade inteira, só para LEITURA rápida (checagem de alcance) */
const uint8_t (*level_cells(void))[MAP_MAX_W] {
	return (const uint8_t (*)[MAP_MAX_W])cells;
}

int level_cell_raw(int cx, int cz) {
	if (cx < 0 || cz < 0 || cx >= lw || cz >= lh) return CELL_SOLID;
	return cells[cz][cx];
}

int level_cell_solid(int cx, int cz) {
	if (cx < 0 || cz < 0 || cx >= lw || cz >= lh) return 1;
	return cells[cz][cx] & CELL_SOLID;
}

int level_cell_low(int cx, int cz) {
	if (cx < 0 || cz < 0 || cx >= lw || cz >= lh) return 0;
	return cells[cz][cx] & CELL_LOW;
}

void level_set_solid(int cx, int cz, int s) {
	if (cx < 0 || cz < 0 || cx >= lw || cz >= lh) return;
	if (s) cells[cz][cx] |= CELL_SOLID;
	else   cells[cz][cx] &= ~(CELL_SOLID | CELL_LOW);
}

/* Um círculo de raio 'radius' em (x,z) encosta em algo sólido? */
int level_blocked(int x, int z, int radius) {
	int x0 = (x - radius) / TILE_SIZE, x1 = (x + radius) / TILE_SIZE;
	int z0 = (z - radius) / TILE_SIZE, z1 = (z + radius) / TILE_SIZE;
	if (x - radius < 0 || z - radius < 0) return 1;
	for (int cz = z0; cz <= z1; cz++)
		for (int cx = x0; cx <= x1; cx++)
			if (level_cell_solid(cx, cz))
				return 1;
	return 0;
}

/* Altura do topo da célula (unidades, positivo = para cima): 0 no chão,
 * 384 na parede, 768 na mata. Para a depuração da colisão. */
int level_cell_top(int cx, int cz) {
	return type_slices(cell_type(cx, cz)) * WALL_SLICE;
}

int level_width(void)  { return lw; }

/* Árvore i do mapa (posição de mundo); devolve o tipo, ou -1 se não existe */
int level_tree_get(int i, int *x, int *z) {
	if (i < 0 || i >= num_trees) return -1;
	*x = trees[i].x;
	*z = trees[i].z;
	return trees[i].type;
}
int level_height(void) { return lh; }
