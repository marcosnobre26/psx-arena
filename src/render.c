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

static MATRIX   view;          /* matriz da câmera (mundo -> câmera) */
static VECTOR   cam_pos;

/* Luz direcional: cada linha aponta PARA a luz (4096 = 1.0).
 * Y negativo = vem de cima. Experimente mudar! */
static MATRIX light_dir = {{
	{ -1434, -3482, -1638 },   /* luz 1: de cima, à esquerda, pela frente */
	{     0,     0,     0 },   /* luz 2: desligada (linha zerada) */
	{     0,     0,     0 }    /* luz 3: desligada */
}};

/* Cor de cada luz: cada COLUNA é uma luz (R, G, B). */
static MATRIX light_color = {{
	{ ONE, 0, 0 },     /* R */
	{ ONE, 0, 0 },     /* G */
	{ ONE*9/10, 0, 0 } /* B: luz levemente amarelada */
}};

#define AMBIENT_R 72
#define AMBIENT_G 72
#define AMBIENT_B 88

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
	gte_SetBackColor(AMBIENT_R, AMBIENT_G, AMBIENT_B);
	gte_SetColorMatrix(&light_color);

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
	if (dx > DRAW_DIST + r || dx < -DRAW_DIST - r ||
	    dz > DRAW_DIST + r || dz < -DRAW_DIST - r)
		return;
	/* Frustum culling: posição do centro no espaço da câmera.
	 * Se a esfera do modelo está toda fora do campo de visão, nem processa. */
	int vz = (view.m[2][0] * dx + view.m[2][1] * dy + view.m[2][2] * dz) >> 12;
	if (vz < NEAR_Z - r)
		return;                                  /* atrás da câmera */
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
		if (lit && !(f->flags & FACE_UNLIT) && !(opt->flags & DRAW_FLASH)) {
			/* Luz calculada pela GTE: cor * (ambiente + luz . normal) */
			gte_ldrgb(&pr->r0);
			gte_ldv0(&m->norms[f->n]);
			gte_nccs();
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
	}
}

/* ------------------------------------------------------------------ */

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

int render_stats_bytes(void) {
	return last_bytes;
}
