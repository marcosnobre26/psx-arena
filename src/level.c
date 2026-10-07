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
	case CELL_WALL:    return WALL_HEIGHT / WALL_SLICE;
	case CELL_THICKET: return THICKET_HEIGHT / WALL_SLICE;
	case CELL_OUTSIDE: return 99;
	}
	return 0;
}

static int type_has_floor(int t) {
	return t == CELL_FLOOR || t == CELL_DIRT || t == CELL_LEAVES || t == CELL_ROOTS;
}

static int type_solid(int t) {
	return t == CELL_VOID || t == CELL_WALL || t == CELL_THICKET;
}

/* Textura de cada tipo (índice na tabela world_tex) */
enum { WT_FLOOR, WT_WALL, WT_DIRT, WT_LEAVES, WT_ROOTS, WT_THICKET, NUM_WT };
static const TEXTURE *world_tex[NUM_WT];

static int type_tex(int t) {
	switch (t) {
	case CELL_WALL:    return WT_WALL;
	case CELL_DIRT:    return WT_DIRT;
	case CELL_LEAVES:  return WT_LEAVES;
	case CELL_ROOTS:   return WT_ROOTS;
	case CELL_THICKET: return WT_THICKET;
	}
	return WT_FLOOR;
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

static void build_chunk(CHUNK_GEOM *c, int ccx, int ccz) {
	const int gx0 = ccx * CHUNK_CELLS, gz0 = ccz * CHUNK_CELLS;
	c->origin.vx = gx0 * TILE_SIZE;
	c->origin.vy = 0;
	c->origin.vz = gz0 * TILE_SIZE;
	c->nquads = 0;

	for (int lz = 0; lz < CHUNK_CELLS; lz++)
	for (int lx = 0; lx < CHUNK_CELLS; lx++) {
		int cx = gx0 + lx, cz = gz0 + lz;
		int t = cell_type(cx, cz);
		if (t == CELL_OUTSIDE) continue;

		/* chão */
		if (type_has_floor(t)) {
			int corner[4][3] = { { lx, lz, 0 }, { lx, lz + 1, 0 }, { lx + 1, lz + 1, 0 }, { lx + 1, lz, 0 } };
			int shade = 128, rot = 0;
			if (t == CELL_FLOOR)
				shade = ((cx + cz) & 1) ? 128 : 116;           /* leve xadrez (mapas de texto) */
			else
				rot = hash3(cx, 7, cz) & 3;                     /* textura girada: menos repetição */
			add_quad(c, corner, 0, 0, type_tex(t), rot, 6, shade, shade, shade, gx0, gz0);
		}

		/* paredes e mata: topo + laterais onde o vizinho é mais baixo */
		int h = type_slices(t);
		if (h == 0) continue;
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
	build_chunk(&slot_geom[s], cx, cz);
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

void level_stats(int *loaded, int *slots, int *built, int *overflow) {
	*loaded = loaded_count;
	*slots = CHUNK_SLOTS;
	*built = builds_this_frame;
	*overflow = chunk_overflow;
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

/* Mapa em texto -> grade + objetos */
static void load_text_map(const char *const *map) {
	int w = 0, h = 0;
	for (h = 0; h < MAP_MAX_H && map[h]; h++) {
		int len = strlen(map[h]);
		if (len > w) w = len;
	}
	level_begin(w, h);
	for (int z = 0; z < lh; z++) {
		int len = strlen(map[z]);
		for (int x = 0; x < len && x < lw; x++) {
			char ch = map[z][x];
			if (ch == '#')      level_set_cell(x, z, CELL_WALL);
			else if (ch == ' ') level_set_cell(x, z, CELL_VOID);
			else {
				level_set_cell(x, z, CELL_FLOOR);
				level_place(ch, x, z);
			}
		}
	}
}

void level_load(int index) {
	const LEVEL_DEF *ld = &level_defs[index];
	cur_level = index;
	level_apply_look();               /* a luz entra na cor dos blocos */

	if (ld->map)
		load_text_map(ld->map);
	else if (ld->build)
		ld->build();                  /* mapa feito por código (levels.c) */

	/* texturas do mundo: as da fase para chão/parede de texto, e as da
	 * floresta para os tipos de terreno */
	world_tex[WT_FLOOR]   = ld->floor;
	world_tex[WT_WALL]    = ld->wall;
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
	chunk_overflow = 0;
	fcx = g.spawn_x / CHUNK_SIZE;
	fcz = g.spawn_z / CHUNK_SIZE;
	npl = 0;
	for (int dz = -CHUNK_LOAD_RADIUS; dz <= CHUNK_LOAD_RADIUS; dz++)
		for (int dx = -CHUNK_LOAD_RADIUS; dx <= CHUNK_LOAD_RADIUS; dx++)
			ensure(fcx + dx, fcz + dz);
}

/* Névoa, céu e luz da fase atual (também chamado ao voltar dos menus) */
void level_apply_look(void) {
	const LEVEL_DEF *ld = &level_defs[cur_level];
	render_set_fog(ld->fog_near, ld->fog_far, ld->sky_r, ld->sky_g, ld->sky_b);
	render_set_light(ld->amb_r, ld->amb_g, ld->amb_b, MOON_R, MOON_G, MOON_B);
}

void level_draw(void) {
	stream_update();
	for (int i = 0; i < CHUNK_SLOTS; i++)
		if (slot_cx[i] >= 0)
			render_chunk(&slot_geom[i]);
}

/* ------------------------------------------------------------------ */
/* Colisão                                                             */
/* ------------------------------------------------------------------ */

int level_cell_solid(int cx, int cz) {
	if (cx < 0 || cz < 0 || cx >= lw || cz >= lh) return 1;
	return cells[cz][cx] & CELL_SOLID;
}

void level_set_solid(int cx, int cz, int s) {
	if (cx < 0 || cz < 0 || cx >= lw || cz >= lh) return;
	if (s) cells[cz][cx] |= CELL_SOLID;
	else   cells[cz][cx] &= ~CELL_SOLID;
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

int level_width(void)  { return lw; }
int level_height(void) { return lh; }
