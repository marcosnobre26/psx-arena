/*
 * render.c - Motor de renderização 3D
 *
 * Conceitos importantes do PS1:
 *
 *  - VRAM: 1 MB de memória de vídeo (1024x512 pixels de 16 bits). Nela ficam
 *    as duas telas (double buffer), as texturas e a fonte.
 *
 *  - Double buffer: enquanto a GPU desenha o quadro A, montamos o quadro B.
 *    Depois trocamos. Evita "piscar" a imagem.
 *
 *  - Ordering Table (OT): o PS1 NÃO tem Z-buffer. Para que objetos de trás
 *    não apareçam na frente, cada polígono é colocado numa "gaveta" de
 *    acordo com a profundidade, e a GPU desenha da gaveta mais distante
 *    para a mais próxima (algoritmo do pintor).
 *
 *  - GTE: co-processador que faz rotação, translação, perspectiva e luz de
 *    3 vértices por vez, muito mais rápido que a CPU.
 */
#include <stdio.h>
#include <stdarg.h>
#include <inline_c.h>
#include "config.h"
#include "render.h"

/* Layout da VRAM usado por este projeto:
 *   x 0..319    tela 0        x 320..639  tela 1
 *   x 640..959  texturas (cada TIM define sua posição; veja tools/png2tim.py)
 *   x 960..     fonte do sistema (FntLoad)
 */

typedef struct {
	DISPENV  disp;
	DRAWENV  draw;
	uint32_t ot[OT_LEN];
	uint8_t  pkt[PACKET_LEN];
} FRAMEBUF;

static FRAMEBUF fb[2];
static int      cur = 0;
static uint8_t *nextpri;
static int      last_bytes = 0;
static int      polys = 0;         /* polígonos enviados à GPU neste quadro */
static int      last_polys = 0;    /* total do quadro anterior (para o HUD) */

static MATRIX   view;          /* matriz da câmera (mundo -> câmera) */
static VECTOR   cam_pos;

/* Luz direcional: cada linha aponta PARA a luz (4096 = 1.0).
 * Y negativo = vem de cima. Experimente mudar! */
static MATRIX light_dir = {{
	{ -1434, -3482, -1638 },   /* luz 1: de cima, à esquerda, pela frente */
	{     0,     0,     0 },   /* luz 2: desligada (linha zerada) */
	{     0,     0,     0 }    /* luz 3: desligada */
}};

/* Cor de cada luz: cada COLUNA é uma luz (R, G, B). Definida por
 * render_set_light() (a fase escolhe: lua azulada, sol, etc). */
static MATRIX light_color = {{
	{ ONE, 0, 0 },     /* R */
	{ ONE, 0, 0 },     /* G */
	{ ONE*9/10, 0, 0 } /* B */
}};

/* ------------------------------------------------------------------ */
/* Barreira de compilador para as macros de CARGA da GTE                 */
/* ------------------------------------------------------------------ */
/*
 * As macros que CARREGAM dados na GTE (gte_SetRotMatrix, gte_SetTransMatrix,
 * gte_SetColorMatrix, gte_ldv3, gte_ldv0...) leem a memória por dentro de um
 * asm, mas só declaram o ENDEREÇO como entrada. Para o GCC, então, "ninguém
 * lê" aquela memória: se a função acabou de escrever os dados numa variável
 * local, ele pode atrasar ou até APAGAR essas escritas (foi o bug da 04a:
 * a translação dos blocos sumia e todos eram desenhados na origem).
 * GTE_BARRIER() obriga o compilador a terminar todas as escritas antes.
 * Use sempre que montar uma matriz/vértices e carregar na GTE em seguida.
 * (As macros de GRAVAÇÃO, gte_st*, já declaram "memory" e são seguras.)
 */
#define GTE_BARRIER() __asm__ volatile ( "" ::: "memory" )

/* ------------------------------------------------------------------ */
/* Névoa por profundidade ("depth cueing" da GTE)                       */
/* ------------------------------------------------------------------ */
/*
 * A cada projeção (rtps/rtpt) a GTE calcula, de graça, um fator IR0:
 *     IR0 = (H * 65536 / z * DQA + DQB) / 4096       (limitado a 0..4096)
 * e as instruções ncds (luz + névoa) e dpcs (só névoa) misturam a cor da
 * face com a "cor distante" (FarColor) na proporção IR0/4096.
 * Escolhemos DQA/DQB para IR0 = 0 em fog_near e 4096 em fog_far. Como a
 * conta é em 1/z, a névoa engrossa mais rápido logo depois do near (no
 * meio do caminho já está em ~70%): é o visual típico do PS1.
 *
 * O SDK não tem macro para DQA/DQB (registradores de controle 27 e 28 da
 * GTE), então escrevemos a nossa: duas instruções ctc2.
 */
#define gte_SetDepthCue(dqa, dqb) __asm__ volatile ( \
	"ctc2	%0, $27;"	\
	"ctc2	%1, $28;"	\
	:					\
	: "r"( dqa ), "r"( dqb ) )

typedef struct {
	int near, far;      /* em unidades de profundidade (z da câmera) */
	int dqa, dqb;
} FOG;

static FOG fog_base;            /* névoa da fase */
static FOG fog_lamp;            /* névoa "empurrada" para objetos no cone da lanterna */
static const FOG *fog_cur;      /* qual está carregada na GTE agora */
static int draw_dist = DRAW_DIST;

static void fog_calc(FOG *f, int near, int far) {
	/* limites: near >= 256 e far - near >= 1/8 do far mantêm DQA em 16 bits
	 * e DQB em 32 bits sem precisar de contas de 64 bits (que puxariam
	 * rotinas da libgcc) */
	if (near < 256) near = 256;
	if (far < near + (near >> 3) + 64) far = near + (near >> 3) + 64;
	f->near = near;
	f->far  = far;
	int t   = (256 * near) / FOV_H;
	int dqa = -(t * far) / (far - near);
	if (dqa < -32767) dqa = -32767;
	f->dqa = dqa;
	f->dqb = -dqa * ((FOV_H * 65536) / near);
}

static inline void fog_use(const FOG *f) {
	if (fog_cur == f) return;
	gte_SetDepthCue(f->dqa, f->dqb);
	fog_cur = f;
}

void render_set_fog(int near, int far, int r, int g, int b) {
	fog_calc(&fog_base, near, far);
	fog_calc(&fog_lamp, near * LANTERN_FOG_MUL >> 4, far * LANTERN_FOG_MUL >> 4);
	fog_cur = NULL;
	fog_use(&fog_base);
	gte_SetFarColor(r, g, b);
	render_set_clear_color(r, g, b);   /* o fundo é a própria névoa */
	draw_dist = fog_lamp.far + 256;    /* além disso, nem a lanterna mostra */
}

int render_fog_near(void) { return fog_base.near; }
int render_fog_far(void)  { return fog_base.far; }

static int amb[3], sun[3];      /* cópia da luz atual, para render_bake_light */

/* Luz ambiente (0..255) e cor da luz direcional (ONE = 1.0 por canal) */
void render_set_light(int amb_r, int amb_g, int amb_b, int sun_r, int sun_g, int sun_b) {
	amb[0] = amb_r; amb[1] = amb_g; amb[2] = amb_b;
	sun[0] = sun_r; sun[1] = sun_g; sun[2] = sun_b;
	gte_SetBackColor(amb_r, amb_g, amb_b);
	light_color.m[0][0] = sun_r;
	light_color.m[1][0] = sun_g;
	light_color.m[2][0] = sun_b;
	GTE_BARRIER();
	gte_SetColorMatrix(&light_color);
}

/* A mesma conta que a GTE faz em nccs, feita uma vez na montagem do bloco:
 *   luz = ambiente*16 + cor_da_lua * max(0, direção_da_luz . normal)
 *   cor = cor_base * luz / 4096   (4096 = 1.0) */
void render_bake_light(const SVECTOR *n, int r, int g, int b, CVECTOR *out) {
	int d = (light_dir.m[0][0] * n->vx + light_dir.m[0][1] * n->vy +
	         light_dir.m[0][2] * n->vz) >> 12;
	if (d < 0) d = 0;
	int base[3] = { r, g, b }, res[3];
	for (int k = 0; k < 3; k++) {
		int l = amb[k] * 16 + ((sun[k] * d) >> 12);
		if (l > 4095 * 2) l = 4095 * 2;
		int c = (base[k] * l) >> 12;
		res[k] = c > 255 ? 255 : c;
	}
	out->r = res[0]; out->g = res[1]; out->b = res[2];
	out->cd = 0x3c;                 /* código do POLY_GT4 (a GTE copia este byte) */
}

/* ------------------------------------------------------------------ */
/* Lanternas: objetos dentro de um cone usam a névoa mais distante       */
/* ------------------------------------------------------------------ */
static struct { int x, z, s, c; } lamps[LANTERN_MAX];
static int num_lamps;

void render_set_lanterns(int n, const VECTOR *pos, const int *angle) {
	num_lamps = n > LANTERN_MAX ? LANTERN_MAX : n;
	for (int i = 0; i < num_lamps; i++) {
		lamps[i].x = pos[i].vx;
		lamps[i].z = pos[i].vz;
		lamps[i].s = isin(angle[i]);
		lamps[i].c = icos(angle[i]);
	}
}

/* (x, z) está no cone de alguma lanterna? Teste barato: distância ao
 * longo da direção (along) e para o lado (side); meio-ângulo ~30 graus
 * (tan 30 ~ 4/7). */
static int in_lantern(int x, int z) {
	for (int i = 0; i < num_lamps; i++) {
		int dx = x - lamps[i].x, dz = z - lamps[i].z;
		int along = (dx * lamps[i].s + dz * lamps[i].c) >> 12;
		if (along <= 0 || along > LANTERN_RANGE) continue;
		int side = (dx * lamps[i].c - dz * lamps[i].s) >> 12;
		if (side < 0) side = -side;
		if (side * 7 < (along + 128) * 4)
			return 1;
	}
	return 0;
}

/* ------------------------------------------------------------------ */

void render_init(void) {
	ResetGraph(0);

	SetDefDispEnv(&fb[0].disp, 0,        0, SCREEN_W, SCREEN_H);
	SetDefDrawEnv(&fb[0].draw, SCREEN_W, 0, SCREEN_W, SCREEN_H);
	SetDefDispEnv(&fb[1].disp, SCREEN_W, 0, SCREEN_W, SCREEN_H);
	SetDefDrawEnv(&fb[1].draw, 0,        0, SCREEN_W, SCREEN_H);

	for (int i = 0; i < 2; i++) {
		setRGB0(&fb[i].draw, CLEAR_R, CLEAR_G, CLEAR_B);
		fb[i].draw.isbg = 1;   /* limpa a tela a cada quadro */
		fb[i].draw.dtd  = 1;   /* dithering: o visual clássico do PS1 */
		ClearOTagR(fb[i].ot, OT_LEN);
	}

	cur     = 0;
	nextpri = fb[0].pkt;
	PutDrawEnv(&fb[0].draw);

	InitGeom();
	gte_SetGeomOffset(SCREEN_W / 2, SCREEN_H / 2);
	gte_SetGeomScreen(FOV_H);
	render_set_light(72, 72, 88, ONE, ONE, ONE * 9 / 10);
	render_set_fog(6000, 9000, CLEAR_R, CLEAR_G, CLEAR_B);   /* praticamente sem névoa */

	FntLoad(960, 0);
}

void render_load_texture(const uint32_t *tim, TEXTURE *out) {
	TIM_IMAGE t;
	GetTimInfo(tim, &t);

	LoadImage(t.prect, t.paddr);
	if (t.mode & 0x8)
		LoadImage(t.crect, t.caddr);
	DrawSync(0);

	int bpp_mode = t.mode & 0x3;          /* 0=4bit 1=8bit 2=16bit */
	int mult     = (bpp_mode == 0) ? 4 : (bpp_mode == 1) ? 2 : 1;

	out->tpage = getTPage(bpp_mode, 0, t.prect->x, t.prect->y);
	out->clut  = (t.mode & 0x8) ? getClut(t.crect->x, t.crect->y) : 0;
	out->u0    = (t.prect->x & 63) * mult;
	out->v0    = t.prect->y & 255;
	out->w     = t.prect->w * mult;
	out->h     = t.prect->h;
}

/* ------------------------------------------------------------------ */

static void cross12(const SVECTOR *a, const SVECTOR *b, VECTOR *o) {
	o->vx = ((a->vy * b->vz) - (a->vz * b->vy)) >> 12;
	o->vy = ((a->vz * b->vx) - (a->vx * b->vz)) >> 12;
	o->vz = ((a->vx * b->vy) - (a->vy * b->vx)) >> 12;
}

void render_set_camera(const VECTOR *eye, const VECTOR *target) {
	/* Monta uma matriz "LookAt": os eixos da câmera em coordenadas do mundo */
	VECTOR  t;
	SVECTOR zaxis, xaxis, yaxis;
	SVECTOR up = { 0, -ONE, 0 };   /* "para cima" no PS1 é Y negativo */

	cam_pos = *eye;

	setVector(&t, target->vx - eye->vx, target->vy - eye->vy, target->vz - eye->vz);
	VectorNormalS(&t, &zaxis);
	cross12(&zaxis, &up, &t);
	VectorNormalS(&t, &xaxis);
	cross12(&zaxis, &xaxis, &t);
	VectorNormalS(&t, &yaxis);

	view.m[0][0] = xaxis.vx; view.m[0][1] = xaxis.vy; view.m[0][2] = xaxis.vz;
	view.m[1][0] = yaxis.vx; view.m[1][1] = yaxis.vy; view.m[1][2] = yaxis.vz;
	view.m[2][0] = zaxis.vx; view.m[2][1] = zaxis.vy; view.m[2][2] = zaxis.vz;

	VECTOR neg = { -eye->vx, -eye->vy, -eye->vz }, tr;
	ApplyMatrixLV(&view, &neg, &tr);
	TransMatrix(&view, &tr);
}

const VECTOR *render_camera_pos(void) {
	return &cam_pos;
}

/* ------------------------------------------------------------------ */

/* Descarta polígonos com vértices fora dos limites que a GPU aceita */
static inline int bad_xy(int x, int y) {
	return (x < -600 || x > SCREEN_W + 600 || y < -400 || y > SCREEN_H + 400);
}

void render_mesh(const MESH *m, const VECTOR *pos, const SVECTOR *rot,
                 int scale, const DRAWOPT *opt) {
	static const DRAWOPT defopt = { 0 };
	if (!opt) opt = &defopt;

	/* Descarte rápido: longe demais ou atrás da câmera */
	int dx = pos->vx - cam_pos.vx, dy = pos->vy - cam_pos.vy, dz = pos->vz - cam_pos.vz;
	int r  = (m->radius * scale) >> 12;
	if (dx > draw_dist + r || dx < -draw_dist - r ||
	    dz > draw_dist + r || dz < -draw_dist - r)
		return;
	/* Frustum culling: posição do centro no espaço da câmera.
	 * Se a esfera do modelo está toda fora do campo de visão, nem processa. */
	int vz = (view.m[2][0] * dx + view.m[2][1] * dy + view.m[2][2] * dz) >> 12;
	if (vz < NEAR_Z - r)
		return;                                  /* atrás da câmera */

	/* névoa deste modelo: a da fase, ou a "empurrada" se estiver no cone
	 * de uma lanterna (os blocos do mapa ficam de fora: são grandes demais
	 * para um teste por objeto e "acenderiam" em degraus) */
	const FOG *fog = (!(opt->flags & DRAW_FIXEDFOG) && num_lamps &&
	                  in_lantern(pos->vx, pos->vz)) ? &fog_lamp : &fog_base;
	if (vz - r > fog->far)
		return;                                  /* inteiro dentro da névoa */
	fog_use(fog);
	int vx = (view.m[0][0] * dx + view.m[0][1] * dy + view.m[0][2] * dz) >> 12;
	int lim = (vz * (SCREEN_W / 2)) / FOV_H + ((r * 3) >> 1);
	if (vx > lim || vx < -lim)
		return;                                  /* fora pelos lados */
	int vy = (view.m[1][0] * dx + view.m[1][1] * dy + view.m[1][2] * dz) >> 12;
	lim = (vz * (SCREEN_H / 2)) / FOV_H + ((r * 3) >> 1);
	if (vy > lim || vy < -lim)
		return;                                  /* fora por cima/baixo */

	/* Matriz do modelo = rotação + escala + posição */
	MATRIX model, mv, lmtx;
	SVECTOR zero = { 0 };
	RotMatrix(rot ? (SVECTOR *)rot : &zero, &model);
	MulMatrix0(&light_dir, &model, &lmtx);   /* luz no espaço do modelo */
	if (scale != ONE) {
		VECTOR s = { scale, scale, scale };
		ScaleMatrix(&model, &s);
	}
	model.t[0] = pos->vx; model.t[1] = pos->vy; model.t[2] = pos->vz;
	CompMatrixLV(&view, &model, &mv);        /* mundo -> câmera */

	gte_SetRotMatrix(&mv);
	gte_SetTransMatrix(&mv);
	gte_SetLightMatrix(&lmtx);

	uint32_t *ot   = fb[cur].ot;
	uint8_t  *end  = fb[cur].pkt + PACKET_LEN - 64;
	int       lit  = !(opt->flags & DRAW_UNLIT);
	int       semi = (opt->flags & DRAW_SEMITRANS) ? 1 : 0;

	for (int i = 0; i < m->nfaces; i++) {
		const MESH_FACE *f = &m->faces[i];
		int p, otz;
		int32_t z0, z1, z2, z3 = NEAR_Z;
		int quad = f->flags & FACE_QUAD;
		int tex  = (f->flags & FACE_TEXTURED) && opt->tex;

		if (nextpri >= end)
			break;   /* sem memória para mais primitivas neste quadro */

		gte_ldv3(&m->verts[f->v[0]], &m->verts[f->v[1]], &m->verts[f->v[2]]);
		gte_rtpt();                 /* gira, move e projeta 3 vértices */
		gte_nclip();                /* sentido da face (frente/costas) */
		gte_stopz(&p);
		if (p == 0 || (p < 0 && !(opt->flags & DRAW_NOCULL)))
			continue;               /* face de costas: não desenha */

		gte_stsz3(&z0, &z1, &z2);
		if (z0 < NEAR_Z || z1 < NEAR_Z || z2 < NEAR_Z)
			continue;
		if (z0 > fog->far && z1 > fog->far && z2 > fog->far)
			continue;               /* sumiu na névoa: economiza a primitiva */

		/* Cor da face (ou da paleta do material) */
		uint8_t cr = f->r, cg = f->g, cb = f->b;
		if (opt->palette && f->mat < opt->npalette) {
			cr = opt->palette[f->mat].r;
			cg = opt->palette[f->mat].g;
			cb = opt->palette[f->mat].b;
		}
		if (opt->flags & DRAW_FLASH) {
			cr = cg = cb = tex ? 255 : 250;
		}

		/* Todos os tipos de polígono começam com o mesmo cabeçalho
		 * (tag + r,g,b,code + x0,y0), então podemos tratá-los juntos. */
		POLY_FT4 *pr = (POLY_FT4 *)nextpri;
		DVECTOR  *xy[4];

		if (tex) {
			if (quad) { setPolyFT4(pr); }
			else      { setPolyFT3((POLY_FT3 *)pr); }
		} else {
			if (quad) { setPolyF4((POLY_F4 *)pr); }
			else      { setPolyF3((POLY_F3 *)pr); }
		}

		/* posições dos campos x,y dentro de cada tipo de primitiva */
		if (tex) {
			xy[0] = (DVECTOR *)&pr->x0; xy[1] = (DVECTOR *)&pr->x1;
			xy[2] = (DVECTOR *)&pr->x2; xy[3] = (DVECTOR *)&pr->x3;
		} else {
			POLY_F4 *pf = (POLY_F4 *)pr;
			xy[0] = (DVECTOR *)&pf->x0; xy[1] = (DVECTOR *)&pf->x1;
			xy[2] = (DVECTOR *)&pf->x2; xy[3] = (DVECTOR *)&pf->x3;
		}

		gte_stsxy3(xy[0], xy[1], xy[2]);

		if (quad) {
			gte_ldv0(&m->verts[f->v[3]]);
			gte_rtps();
			gte_stsxy(xy[3]);
			gte_stsz(&z3);
			if (z3 < NEAR_Z)
				continue;
			gte_avsz4();
		} else {
			gte_avsz3();
		}
		gte_stotz(&otz);

		otz += opt->zbias;
		if (otz < 2) otz = 2;          /* 0 e 1 são reservados para o HUD */
		if (otz >= OT_LEN) continue;

		if (bad_xy(xy[0]->vx, xy[0]->vy) || bad_xy(xy[1]->vx, xy[1]->vy) ||
		    bad_xy(xy[2]->vx, xy[2]->vy) || (quad && bad_xy(xy[3]->vx, xy[3]->vy)))
			continue;

		if (semi)
			setSemiTrans(pr, 1);

		setRGB0(pr, cr, cg, cb);
		if (!(opt->flags & DRAW_FLASH)) {
			/* IR0 (fator de névoa) veio da última projeção, de um vértice
			 * desta face: a névoa é por face, como a cor. */
			GTE_BARRIER();          /* setRGB0 acima escreveu na memória que gte_ldrgb lê */
			gte_ldrgb(&pr->r0);
			if (lit && !(f->flags & FACE_UNLIT)) {
				/* luz + névoa: cor * (ambiente + luz . normal) -> FarColor */
				gte_ldv0(&m->norms[f->n]);
				gte_ncds();
			} else {
				gte_dpcs();         /* sem luz: só mistura com a névoa */
			}
			gte_strgb(&pr->r0);
		}

		if (tex) {
			const TEXTURE *t = opt->tex;
			pr->tpage = t->tpage;
			pr->clut  = t->clut;
			pr->u0 = t->u0 + f->uv[0]; pr->v0 = t->v0 + f->uv[1];
			pr->u1 = t->u0 + f->uv[2]; pr->v1 = t->v0 + f->uv[3];
			pr->u2 = t->u0 + f->uv[4]; pr->v2 = t->v0 + f->uv[5];
			if (quad) {
				pr->u3 = t->u0 + f->uv[6]; pr->v3 = t->v0 + f->uv[7];
				addPrim(ot + otz, pr);
				nextpri += sizeof(POLY_FT4);
			} else {
				addPrim(ot + otz, pr);
				nextpri += sizeof(POLY_FT3);
			}
		} else {
			addPrim(ot + otz, pr);
			nextpri += quad ? sizeof(POLY_F4) : sizeof(POLY_F3);
		}
		polys++;   /* só chega aqui quem passou por todos os descartes */
	}
}

/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Blocos do mapa                                                      */
/* ------------------------------------------------------------------ */

static const SVECTOR       *chunk_verts;
static const TEXTURE *const *chunk_texs;
static int                  chunk_ntex;

void render_chunk_setup(const SVECTOR *verts, const TEXTURE *const *texs, int ntex) {
	chunk_verts = verts;
	chunk_texs  = texs;
	chunk_ntex  = ntex;
}

/* Carrega na GTE a matriz mundo->câmera de algo só transladado para (x,y,z).
 * Assim os vértices podem ser pequenos (relativos a esse ponto) mesmo num
 * mapa de 32 768 unidades, que estouraria um SVECTOR (16 bits). */
static void load_translation(int x, int y, int z) {
	/* câmera = R * (v + p) + T = R * v + (R * p + T). R * p em inteiros:
	 * |R| <= 4096 e |p| <= 32768 cabem em 32 bits (sem ApplyMatrixLV). */
	MATRIX m = view;
	for (int i = 0; i < 3; i++)
		m.t[i] = view.t[i] + ((view.m[i][0] * x + view.m[i][1] * y + view.m[i][2] * z) >> 12);
	GTE_BARRIER();                  /* sem isto o GCC apagava a soma acima */
	gte_SetRotMatrix(&m);
	gte_SetTransMatrix(&m);
}

/* Cantos da textura (u, v): chão 64x64 e fatia de parede 64x48 */
static const uint8_t chunk_uv[2][4][2] = {
	{ { 0, 0 }, { 0, 63 }, { 63, 63 }, { 63, 0 } },
	{ { 0, 47 }, { 63, 47 }, { 63, 0 }, { 0, 0 } },
};

#define CHUNK_HALF    (CHUNK_CELLS * TILE_SIZE / 2)
#define CHUNK_RADIUS  (CHUNK_HALF * 3 / 2 + THICKET_HEIGHT / 2)   /* esfera que cobre o bloco */

void render_chunk(const CHUNK_GEOM *c) {
	if (!c->nquads || !chunk_verts)
		return;

	/* descarte do bloco inteiro: distância, névoa e campo de visão */
	int r  = CHUNK_RADIUS;
	int dx = c->origin.vx + CHUNK_HALF - cam_pos.vx;
	int dy = -THICKET_HEIGHT / 2 - cam_pos.vy;
	int dz = c->origin.vz + CHUNK_HALF - cam_pos.vz;
	if (dx > draw_dist + r || dx < -draw_dist - r || dz > draw_dist + r || dz < -draw_dist - r)
		return;
	int vz = (view.m[2][0] * dx + view.m[2][1] * dy + view.m[2][2] * dz) >> 12;
	if (vz < NEAR_Z - r || vz - r > fog_base.far)
		return;
	int vx = (view.m[0][0] * dx + view.m[0][1] * dy + view.m[0][2] * dz) >> 12;
	int lim = (vz * (SCREEN_W / 2)) / FOV_H + ((r * 3) >> 1);
	if (vx > lim || vx < -lim)
		return;

	fog_use(&fog_base);              /* blocos não usam a névoa da lanterna */
	load_translation(c->origin.vx, 0, c->origin.vz);

	uint32_t *ot  = fb[cur].ot;
	uint8_t  *end = fb[cur].pkt + PACKET_LEN - 64;
	const int far = fog_base.far;

	for (int i = 0; i < c->nquads; i++) {
		const CHUNK_QUAD *q = &c->quads[i];
		int p, otz;
		int32_t z0, z1, z2, z3;

		if (nextpri >= end)
			break;

		gte_ldv3(&chunk_verts[q->v[0]], &chunk_verts[q->v[1]], &chunk_verts[q->v[2]]);
		gte_rtpt();
		gte_nclip();
		gte_stopz(&p);
		if (p <= 0)
			continue;                    /* de costas */
		gte_stsz3(&z0, &z1, &z2);
		if (z0 < NEAR_Z || z1 < NEAR_Z || z2 < NEAR_Z)
			continue;
		if (z0 > far && z1 > far && z2 > far)
			continue;                    /* sumiu na névoa */

		POLY_GT4 *pr = (POLY_GT4 *)nextpri;
		gte_stsxy3(&pr->x0, &pr->x1, &pr->x2);
		gte_ldv0(&chunk_verts[q->v[3]]);
		gte_rtps();
		gte_stsxy(&pr->x3);
		gte_stsz(&z3);
		if (z3 < NEAR_Z)
			continue;
		gte_avsz4();
		gte_stotz(&otz);
		otz += q->zbias;
		if (otz < 2) otz = 2;
		if (otz >= OT_LEN) continue;
		if (bad_xy(pr->x0, pr->y0) || bad_xy(pr->x1, pr->y1) ||
		    bad_xy(pr->x2, pr->y2) || bad_xy(pr->x3, pr->y3))
			continue;

		/* névoa nas 4 cores (o fator IR0 é o do último vértice projetado):
		 * dpct faz 3 cores de uma vez, dpcs a quarta */
		gte_ldrgb3(&q->c[0], &q->c[1], &q->c[2]);
		gte_dpct();
		gte_strgb3(&pr->r0, &pr->r1, &pr->r2);
		gte_ldrgb(&q->c[3]);
		gte_dpcs();
		gte_strgb(&pr->r3);
		setPolyGT4(pr);

		const TEXTURE *t = chunk_texs[q->tex < chunk_ntex ? q->tex : 0];
		const uint8_t (*uv)[2] = chunk_uv[q->uv];
		int cm = q->corners, rot = q->rot;
		pr->tpage = t->tpage;
		pr->clut  = t->clut;
		pr->u0 = t->u0 + uv[((cm     ) + rot) & 3][0]; pr->v0 = t->v0 + uv[((cm     ) + rot) & 3][1];
		pr->u1 = t->u0 + uv[((cm >> 2) + rot) & 3][0]; pr->v1 = t->v0 + uv[((cm >> 2) + rot) & 3][1];
		pr->u2 = t->u0 + uv[((cm >> 4) + rot) & 3][0]; pr->v2 = t->v0 + uv[((cm >> 4) + rot) & 3][1];
		pr->u3 = t->u0 + uv[((cm >> 6) + rot) & 3][0]; pr->v3 = t->v0 + uv[((cm >> 6) + rot) & 3][1];

		addPrim(ot + otz, pr);
		nextpri += sizeof(POLY_GT4);
		polys++;
	}
}

/* Marcador de depuração: quadrado plano (horizontal) de lado 2*half em
 * (x, y, z), sem luz e sem névoa, puxado para a frente na OT. Não entra
 * na contagem de POLIS (é só para conferir a colisão). */
void render_marker(int x, int y, int z, int half, int r, int g, int b) {
	SVECTOR v[4] = {
		{ -half, 0, -half, 0 }, { half, 0, -half, 0 },
		{ -half, 0,  half, 0 }, { half, 0,  half, 0 },    /* ordem "Z" */
	};
	if (nextpri + sizeof(POLY_F4) >= fb[cur].pkt + PACKET_LEN - 64)
		return;
	load_translation(x, y, z);
	GTE_BARRIER();
	POLY_F4 *p = (POLY_F4 *)nextpri;
	int32_t z0, z1, z2, z3;
	int otz;
	gte_ldv3(&v[0], &v[1], &v[2]);
	gte_rtpt();
	gte_stsz3(&z0, &z1, &z2);
	if (z0 < NEAR_Z || z1 < NEAR_Z || z2 < NEAR_Z)
		return;
	gte_stsxy3(&p->x0, &p->x1, &p->x2);
	gte_ldv0(&v[3]);
	gte_rtps();
	gte_stsxy(&p->x3);
	gte_stsz(&z3);
	if (z3 < NEAR_Z)
		return;
	gte_avsz4();
	gte_stotz(&otz);
	otz -= 8;                                /* na frente do topo da parede */
	if (otz < 2 || otz >= OT_LEN)
		return;
	if (bad_xy(p->x0, p->y0) || bad_xy(p->x1, p->y1) || bad_xy(p->x2, p->y2) || bad_xy(p->x3, p->y3))
		return;
	setPolyF4(p);
	setRGB0(p, r, g, b);
	addPrim(fb[cur].ot + otz, p);
	nextpri += sizeof(POLY_F4);
}

/* Cone de luz da lanterna no chão: um leque de triângulos POLY_G3 em modo
 * ADITIVO, com a ponta clara e a borda preta (somar preto não muda nada,
 * então o degradê some suavemente sem precisar de textura).
 *
 * Cuidado com o modo de mistura: polígonos sem textura usam o modo da
 * última "texture page" ativa. Por isso cada triângulo vai na OT entre
 * dois DR_TPAGE: um liga o modo aditivo antes dele e outro volta ao modo
 * normal (50%) depois, senão as sombras (pretas, 50%) passariam a ser
 * somadas e sumiriam. Dentro de uma entrada da OT, o último addPrim é o
 * primeiro desenhado; daí a ordem invertida abaixo. */
void render_light_cone(int x, int z, int angle, int r, int g, int b) {
	/* vértices relativos ao jogador (o mapa grande estouraria 16 bits) */
	SVECTOR v[LANTERN_SEGS + 2];
	v[0].vx = (isin(angle) * 40) >> 12;          /* ponta logo à frente do pé */
	v[0].vy = -4;
	v[0].vz = (icos(angle) * 40) >> 12;
	for (int k = 0; k <= LANTERN_SEGS; k++) {
		int a = (angle - LANTERN_HALF + (2 * LANTERN_HALF * k) / LANTERN_SEGS) & 4095;
		v[k + 1].vx = (isin(a) * LANTERN_RANGE) >> 12;
		v[k + 1].vy = -4;
		v[k + 1].vz = (icos(a) * LANTERN_RANGE) >> 12;
	}
	load_translation(x, 0, z);
	GTE_BARRIER();                  /* v[] acabou de ser escrito: gte_ldv3 lê da memória */

	uint32_t *ot  = fb[cur].ot;
	uint8_t  *end = fb[cur].pkt + PACKET_LEN - 64;
	uint16_t add  = getTPage(0, 1, 0, 0) | (fb[cur].draw.dtd << 9);   /* abr 1 = somar */
	uint16_t half = getTPage(0, 0, 0, 0) | (fb[cur].draw.dtd << 9);   /* abr 0 = 50% */

	for (int k = 1; k <= LANTERN_SEGS; k++) {
		int32_t z0, z1, z2;
		int otz;
		DVECTOR xy[3];
		if (nextpri + sizeof(POLY_G3) + 2 * sizeof(DR_TPAGE) >= end)
			return;
		gte_ldv3(&v[0], &v[k], &v[k + 1]);
		gte_rtpt();
		gte_stsz3(&z0, &z1, &z2);
		if (z0 < NEAR_Z || z1 < NEAR_Z || z2 < NEAR_Z)
			continue;
		/* Posição na OT: o triângulo é comprido, e pela profundidade média
		 * as células do chão perto da ponta seriam desenhadas por cima dele.
		 * Usamos o vértice mais próximo menos meia célula (a GTE converte
		 * para a escala da OT, igual às outras faces). */
		int zn = z0 < z1 ? z0 : z1;
		if (z2 < zn) zn = z2;
		zn -= TILE_SIZE / 2;
		if (zn < NEAR_Z) zn = NEAR_Z;
		gte_ldsz3(zn, zn, zn);
		gte_avsz3();
		gte_stotz(&otz);
		if (otz < 2 || otz >= OT_LEN) continue;
		gte_stsxy3(&xy[0], &xy[1], &xy[2]);
		if (bad_xy(xy[0].vx, xy[0].vy) || bad_xy(xy[1].vx, xy[1].vy) || bad_xy(xy[2].vx, xy[2].vy))
			continue;

		DR_TPAGE *after = (DR_TPAGE *)nextpri;     /* desenhado por último */
		setDrawTPage(after, 0, 1, half);
		addPrim(ot + otz, after);
		nextpri += sizeof(DR_TPAGE);

		POLY_G3 *p = (POLY_G3 *)nextpri;
		setXY3(p, xy[0].vx, xy[0].vy, xy[1].vx, xy[1].vy, xy[2].vx, xy[2].vy);
		setPolyG3(p);
		setSemiTrans(p, 1);
		setRGB0(p, r, g, b);                       /* ponta: clara */
		setRGB1(p, 0, 0, 0);                       /* borda: preta (some) */
		setRGB2(p, 0, 0, 0);
		addPrim(ot + otz, p);
		nextpri += sizeof(POLY_G3);

		DR_TPAGE *before = (DR_TPAGE *)nextpri;    /* desenhado primeiro */
		setDrawTPage(before, 0, 1, add);
		addPrim(ot + otz, before);
		nextpri += sizeof(DR_TPAGE);
		polys++;
	}
}

void render_rect(int x, int y, int w, int h, int r, int g, int b, int semitrans) {
	if (nextpri + sizeof(TILE) >= fb[cur].pkt + PACKET_LEN)
		return;
	TILE *t = (TILE *)nextpri;
	setTile(t);
	setXY0(t, x, y);
	setWH(t, w, h);
	setRGB0(t, r, g, b);
	if (semitrans)
		setSemiTrans(t, 1);
	addPrim(fb[cur].ot + 1, t);
	nextpri += sizeof(TILE);
}

void hud_print(int x, int y, const char *fmt, ...) {
	char buf[96];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	/* cada letra vira um sprite de 8x8 (~16 bytes); garante espaço */
	if (nextpri + 24 * 96 >= fb[cur].pkt + PACKET_LEN)
		return;
	nextpri = (uint8_t *)FntSort(fb[cur].ot, nextpri, x, y, buf);
}

/* ------------------------------------------------------------------ */

void render_end_frame(void) {
	last_bytes = nextpri - fb[cur].pkt;
	last_polys = polys;
	polys = 0;

	DrawSync(0);   /* espera a GPU terminar o quadro anterior */
	VSync(0);      /* espera o retraço vertical (60 Hz NTSC) */

	PutDispEnv(&fb[cur].disp);
	PutDrawEnv(&fb[cur].draw);
	SetDispMask(1);
	DrawOTag(fb[cur].ot + OT_LEN - 1);   /* manda a GPU desenhar a OT */

	cur ^= 1;
	nextpri = fb[cur].pkt;
	ClearOTagR(fb[cur].ot, OT_LEN);
}

void render_set_clear_color(int r, int g, int b) {
	for (int i = 0; i < 2; i++)
		setRGB0(&fb[i].draw, r, g, b);
}

int render_stats_bytes(void) {
	return last_bytes;
}

/* Polígonos de modelos (render_mesh) no último quadro; o HUD não conta.
 * Orçamento do roteiro: ~2 500 por quadro. */
int render_stats_polys(void) {
	return last_polys;
}
