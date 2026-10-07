/*
 * level.c - A fase: mapa em texto, geração da geometria e colisão
 *
 * Os mapas e a aparência de cada fase ficam em levels.c (tabela
 * level_defs, com a legenda dos caracteres). Aqui fica o "motor": ler o
 * texto, criar os objetos, gerar a geometria e responder à colisão.
 *
 * A câmera começa olhando do 'P' para o centro da fase.
 */
#include <string.h>
#include "game.h"

#define MAX_W       32
#define MAX_H       32
#define CHUNK       4       /* geometria agrupada em blocos de 4x4 células */
#define MAX_CHUNKS  ((MAX_W / CHUNK) * (MAX_H / CHUNK))
#define MAX_LV_VERTS 9000
#define MAX_LV_FACES 2600

static char    grid[MAX_H][MAX_W];
static uint8_t solid[MAX_H][MAX_W];
static int     lw, lh;

/* Geometria gerada a partir do mapa */
static SVECTOR   lv_verts[MAX_LV_VERTS];
static MESH_FACE lv_faces[MAX_LV_FACES];
static int       nv, nf;

static const SVECTOR lv_norms[5] = {
	{ 0, -ONE, 0, 0 },   /* 0: para cima */
	{ 0, 0, -ONE, 0 },   /* 1: norte (-Z) */
	{ 0, 0,  ONE, 0 },   /* 2: sul   (+Z) */
	{ -ONE, 0, 0, 0 },   /* 3: oeste (-X) */
	{  ONE, 0, 0, 0 },   /* 4: leste (+X) */
};

typedef struct {
	MESH   mesh;
	VECTOR pos;
} CHUNK_MESH;

static CHUNK_MESH floor_chunks[MAX_CHUNKS], wall_chunks[MAX_CHUNKS];
static int        n_floor_chunks, n_wall_chunks;

/* ------------------------------------------------------------------ */

static int is_wall(int cx, int cz) {
	if (cx < 0 || cz < 0 || cx >= lw || cz >= lh) return 1;
	return grid[cz][cx] == '#';
}

static int is_void(int cx, int cz) {
	if (cx < 0 || cz < 0 || cx >= lw || cz >= lh) return 1;
	return grid[cz][cx] == ' ';
}

/* Adiciona um quad. p[] em ordem circular (qualquer sentido); a função
 * acerta o sentido para que a face fique virada para a normal 'n' e
 * converte para a ordem em "Z" que o PS1 usa. */
static void add_quad(const VECTOR *center, const VECTOR p[4], int n,
                     const uint8_t uv[8], int r, int gg, int b) {
	if (nv + 4 > MAX_LV_VERTS || nf + 1 > MAX_LV_FACES)
		return;

	int order[4] = { 0, 1, 2, 3 };
	/* normal pelo sentido dos vértices: (p1-p0) x (p2-p0) */
	int ax = p[1].vx - p[0].vx, ay = p[1].vy - p[0].vy, az = p[1].vz - p[0].vz;
	int bx = p[2].vx - p[0].vx, by = p[2].vy - p[0].vy, bz = p[2].vz - p[0].vz;
	int wx = (ay * bz - az * by) >> 8;
	int wy = (az * bx - ax * bz) >> 8;
	int wz = (ax * by - ay * bx) >> 8;
	int d  = wx * lv_norms[n].vx + wy * lv_norms[n].vy + wz * lv_norms[n].vz;
	if (d > 0) {               /* a GTE quer o sentido oposto ao da normal */
		order[1] = 3; order[3] = 1;
	}
	/* ordem circular a,b,c,d -> ordem do PS1 a,b,d,c */
	static const int zorder[4] = { 0, 1, 3, 2 };

	MESH_FACE *f = &lv_faces[nf++];
	for (int k = 0; k < 4; k++) {
		int i = order[zorder[k]];
		lv_verts[nv].vx = p[i].vx - center->vx;
		lv_verts[nv].vy = p[i].vy - center->vy;
		lv_verts[nv].vz = p[i].vz - center->vz;
		f->v[k] = nv++ ;
		f->uv[k * 2]     = uv[i * 2];
		f->uv[k * 2 + 1] = uv[i * 2 + 1];
	}
	f->r = r; f->g = gg; f->b = b;
	f->flags = FACE_QUAD | FACE_TEXTURED;
	f->mat = 0;
	f->n = n;
}

static void begin_chunk(CHUNK_MESH *c, int ccx, int ccz, int *vbase, int *fbase) {
	c->pos.vx = (ccx * CHUNK * TILE_SIZE) + (CHUNK * TILE_SIZE / 2);
	c->pos.vy = -WALL_HEIGHT / 2;
	c->pos.vz = (ccz * CHUNK * TILE_SIZE) + (CHUNK * TILE_SIZE / 2);
	*vbase = nv;
	*fbase = nf;
}

/* Converte os índices da chunk para relativos ao seu início */
static int end_chunk(CHUNK_MESH *c, int vbase, int fbase) {
	if (nf == fbase)
		return 0;
	for (int i = fbase; i < nf; i++)
		for (int k = 0; k < 4; k++)
			lv_faces[i].v[k] -= vbase;
	c->mesh.nverts = nv - vbase;
	c->mesh.nfaces = nf - fbase;
	c->mesh.verts  = &lv_verts[vbase];
	c->mesh.norms  = lv_norms;
	c->mesh.faces  = &lv_faces[fbase];
	c->mesh.radius = (CHUNK * TILE_SIZE * 3) / 4 + WALL_HEIGHT / 2;
	return 1;
}

static void build_geometry(void) {
	static const uint8_t uv_full[8] = { 0,0, 0,63, 63,63, 63,0 };
	static const uint8_t uv_wall[8] = { 0,47, 63,47, 63,0, 0,0 }; /* 64x48 texels */
	const int T = TILE_SIZE, H = WALL_HEIGHT, HH = WALL_HEIGHT / 2;

	nv = nf = 0;
	n_floor_chunks = n_wall_chunks = 0;

	for (int ccz = 0; ccz * CHUNK < lh; ccz++)
	for (int ccx = 0; ccx * CHUNK < lw; ccx++) {
		int vb, fb;

		/* --- chão --- */
		CHUNK_MESH *c = &floor_chunks[n_floor_chunks];
		begin_chunk(c, ccx, ccz, &vb, &fb);
		for (int cz = ccz * CHUNK; cz < (ccz + 1) * CHUNK && cz < lh; cz++)
		for (int cx = ccx * CHUNK; cx < (ccx + 1) * CHUNK && cx < lw; cx++) {
			if (is_wall(cx, cz) || is_void(cx, cz)) continue;
			int x0 = cx * T, x1 = x0 + T, z0 = cz * T, z1 = z0 + T;
			VECTOR p[4] = { { x0, 0, z0 }, { x0, 0, z1 }, { x1, 0, z1 }, { x1, 0, z0 } };
			int shade = ((cx + cz) & 1) ? 128 : 116;   /* leve xadrez */
			add_quad(&c->pos, p, 0, uv_full, shade, shade, shade);
		}
		n_floor_chunks += end_chunk(c, vb, fb);

		/* --- paredes --- */
		c = &wall_chunks[n_wall_chunks];
		begin_chunk(c, ccx, ccz, &vb, &fb);
		for (int cz = ccz * CHUNK; cz < (ccz + 1) * CHUNK && cz < lh; cz++)
		for (int cx = ccx * CHUNK; cx < (ccx + 1) * CHUNK && cx < lw; cx++) {
			if (!is_wall(cx, cz)) continue;
			int x0 = cx * T, x1 = x0 + T, z0 = cz * T, z1 = z0 + T;

			/* topo */
			VECTOR top[4] = { { x0, -H, z0 }, { x0, -H, z1 }, { x1, -H, z1 }, { x1, -H, z0 } };
			add_quad(&c->pos, top, 0, uv_full, 100, 100, 100);

			/* laterais: só onde o vizinho não é parede; 2 "andares" */
			for (int y = 0; y < H; y += HH) {
				int ya = -y, yb = -(y + HH);
				if (!is_wall(cx, cz - 1)) {
					VECTOR q[4] = { { x0, ya, z0 }, { x1, ya, z0 }, { x1, yb, z0 }, { x0, yb, z0 } };
					add_quad(&c->pos, q, 1, uv_wall, 128, 128, 128);
				}
				if (!is_wall(cx, cz + 1)) {
					VECTOR q[4] = { { x1, ya, z1 }, { x0, ya, z1 }, { x0, yb, z1 }, { x1, yb, z1 } };
					add_quad(&c->pos, q, 2, uv_wall, 128, 128, 128);
				}
				if (!is_wall(cx - 1, cz)) {
					VECTOR q[4] = { { x0, ya, z1 }, { x0, ya, z0 }, { x0, yb, z0 }, { x0, yb, z1 } };
					add_quad(&c->pos, q, 3, uv_wall, 128, 128, 128);
				}
				if (!is_wall(cx + 1, cz)) {
					VECTOR q[4] = { { x1, ya, z0 }, { x1, ya, z1 }, { x1, yb, z1 }, { x1, yb, z0 } };
					add_quad(&c->pos, q, 4, uv_wall, 128, 128, 128);
				}
			}
		}
		n_wall_chunks += end_chunk(c, vb, fb);
	}
}

/* ------------------------------------------------------------------ */

static int cur_level = 0;      /* índice em level_defs (para level_draw) */

void level_load(int index) {
	const LEVEL_DEF *ld = &level_defs[index];
	const char *const *map = ld->map;
	cur_level = index;
	level_apply_look();

	memset(grid, ' ', sizeof(grid));
	memset(solid, 0, sizeof(solid));
	lw = lh = 0;

	for (int z = 0; z < MAX_H && map[z]; z++) {
		int len = strlen(map[z]);
		if (len > MAX_W) len = MAX_W;
		if (len > lw) lw = len;
		for (int x = 0; x < len; x++)
			grid[z][x] = map[z][x];
		lh = z + 1;
	}

	/* Lê as letras e cria os objetos */
	for (int z = 0; z < lh; z++)
	for (int x = 0; x < lw; x++) {
		char ch = grid[z][x];
		int wx = x * TILE_SIZE + TILE_SIZE / 2;
		int wz = z * TILE_SIZE + TILE_SIZE / 2;

		solid[z][x] = (ch == '#' || ch == ' ');

		switch (ch) {
		case 'P': g.spawn_x = wx; g.spawn_z = wz;  break;   /* jogadores nascem aqui */
		case 'C': crate_spawn(x, z);               break;
		case 'G': pickup_spawn(PICK_GEM, wx, wz);  break;
		case 'H': pickup_spawn(PICK_HEALTH, wx, wz); break;
		case 'N': pickup_spawn(PICK_ENERGY, wx, wz); break;
		case 'W': pickup_spawn(PICK_WEAPON, wx, wz); break;
		case 'Q': pickup_spawn(PICK_QUEST, wx, wz);  break;   /* item de missão */
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
					prop_spawn(t, x, z);
			break;
		}
	}

	build_geometry();
}

/* Névoa, céu e luz da fase atual (também chamado ao voltar dos menus) */
void level_apply_look(void) {
	const LEVEL_DEF *ld = &level_defs[cur_level];
	render_set_fog(ld->fog_near, ld->fog_far, ld->sky_r, ld->sky_g, ld->sky_b);
	render_set_light(ld->amb_r, ld->amb_g, ld->amb_b, MOON_R, MOON_G, MOON_B);
}

void level_draw(void) {
	DRAWOPT fo = { 0 }, wo = { 0 };
	fo.flags = DRAW_FIXEDFOG;   /* blocos grandes: não usam a névoa da lanterna */
	wo.flags = DRAW_FIXEDFOG;
	fo.tex = level_defs[cur_level].floor;
	fo.zbias = 6;               /* chão sempre atrás dos objetos sobre ele */
	wo.tex = level_defs[cur_level].wall;

	for (int i = 0; i < n_floor_chunks; i++)
		render_mesh(&floor_chunks[i].mesh, &floor_chunks[i].pos, NULL, ONE, &fo);
	for (int i = 0; i < n_wall_chunks; i++)
		render_mesh(&wall_chunks[i].mesh, &wall_chunks[i].pos, NULL, ONE, &wo);
}

/* ------------------------------------------------------------------ */
/* Colisão                                                             */
/* ------------------------------------------------------------------ */

int level_cell_solid(int cx, int cz) {
	if (cx < 0 || cz < 0 || cx >= lw || cz >= lh) return 1;
	return solid[cz][cx];
}

void level_set_solid(int cx, int cz, int s) {
	if (cx < 0 || cz < 0 || cx >= lw || cz >= lh) return;
	solid[cz][cx] = s;
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
