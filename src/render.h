/*
 * render.h - Desenho 3D com a GTE (o "co-processador de geometria" do PS1)
 *
 * Fluxo de um quadro:
 *   render_set_camera(...)          // define de onde olhamos
 *   render_mesh(...) várias vezes   // envia modelos
 *   hud_print(...)                  // textos 2D por cima
 *   render_end_frame()              // espera o VSync e troca os buffers
 */
#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include <psxgte.h>
#include <psxgpu.h>
#include "mesh.h"

/* Textura já enviada para a VRAM */
typedef struct {
	uint16_t tpage;     /* página de textura (posição + modo de cor) */
	uint16_t clut;      /* paleta (texturas 4/8 bits) */
	uint8_t  u0, v0;    /* deslocamento da textura dentro da página */
	uint16_t w, h;      /* tamanho em texels */
} TEXTURE;

/* Opções de desenho de um modelo (pode passar NULL para o padrão) */
#define DRAW_UNLIT      0x01  /* sem iluminação: usa a cor pura */
#define DRAW_SEMITRANS  0x02  /* semitransparente (50%) */
#define DRAW_FLASH      0x04  /* pinta tudo de branco (efeito de dano) */
#define DRAW_NOCULL     0x08  /* desenha as duas faces do polígono */
#define DRAW_FIXEDFOG   0x10  /* ignora a lanterna (blocos do mapa) */

typedef struct {
	const CVECTOR *palette;  /* troca a cor por material: palette[face.mat] */
	int            npalette;
	const TEXTURE *tex;      /* textura para faces com UV */
	int            flags;    /* DRAW_* */
	int            zbias;    /* negativo = desenha "mais à frente" */
} DRAWOPT;

/* ---- Blocos do mapa (geometria montada em level.c) ----
 * Todo vértice de um bloco de CHUNK_CELLS x CHUNK_CELLS células é um ponto
 * da grade, em CHUNK_LAYERS alturas: por isso uma única tabela de vértices
 * (relativos à origem do bloco) serve para todos, e cada quad guarda só
 * índices. A luz já vem embutida na cor de cada vértice (as normais do
 * chão e das paredes são fixas); no desenho só se aplica a névoa. */
#define CHUNK_CELLS      8
#define CHUNK_LAYERS     5                      /* alturas: 0, 1, 2, 3, 4 x WALL_SLICE */
#define CHUNK_GRID       (CHUNK_CELLS + 1)
#define CHUNK_VERTS      (CHUNK_GRID * CHUNK_GRID * CHUNK_LAYERS)
#define CHUNK_MAX_QUADS  256

typedef struct {
	uint16_t v[4];        /* índices na tabela de vértices, em ordem "Z" */
	CVECTOR  c[4];        /* cor de cada vértice (luz embutida) */
	uint8_t  tex;         /* índice na tabela de texturas do mundo */
	uint8_t  uv;          /* 0 = chão (64x64), 1 = fatia de parede (64x48) */
	uint8_t  rot;         /* chão: rotação da textura 0..3 (variedade) */
	uint8_t  corners;     /* canto da textura de cada vértice (2 bits cada) */
	int8_t   zbias;
} CHUNK_QUAD;

typedef struct {
	VECTOR      origin;   /* canto (x0, 0, z0) do bloco no mundo */
	int         nquads;
	CHUNK_QUAD  quads[CHUNK_MAX_QUADS];
} CHUNK_GEOM;

void render_init(void);
void render_load_texture(const uint32_t *tim, TEXTURE *out);

void render_set_camera(const VECTOR *eye, const VECTOR *target);
const VECTOR *render_camera_pos(void);

/* pos: posição no mundo; rot: rotação (4096 = 360°); scale: ONE = 1.0 */
void render_mesh(const MESH *m, const VECTOR *pos, const SVECTOR *rot,
                 int scale, const DRAWOPT *opt);

/* Retângulo 2D na tela (barras de vida, etc) */
void render_rect(int x, int y, int w, int h, int r, int g, int b, int semitrans);

/* Texto na tela (fonte embutida do PSn00bSDK, só ASCII sem acentos) */
void hud_print(int x, int y, const char *fmt, ...);

void render_end_frame(void);

/* Névoa por profundidade: some no fundo entre near e far (z da câmera).
 * A cor da névoa também vira a cor de fundo. */
void render_set_fog(int near, int far, int r, int g, int b);
int  render_fog_near(void);
int  render_fog_far(void);
/* Luz ambiente (0..255) + cor da luz direcional (ONE = 1.0 por canal) */
void render_set_light(int amb_r, int amb_g, int amb_b, int sun_r, int sun_g, int sun_b);
/* Lanternas acesas neste quadro: objetos no cone são vistos mais longe */
void render_set_lanterns(int n, const VECTOR *pos, const int *angle);
/* Blocos do mapa: tabela de vértices, texturas e desenho */
void render_chunk_setup(const SVECTOR *verts, const TEXTURE *const *texs, int ntex);
void render_chunk(const CHUNK_GEOM *c);
/* Cor de uma face com normal fixa, com a luz atual (ambiente + lua) */
void render_bake_light(const SVECTOR *normal, int r, int g, int b, CVECTOR *out);

/* Depuração: quadrado plano colorido (não conta em POLIS) */
void render_marker(int x, int y, int z, int half, int r, int g, int b);

/* Cone de luz aditivo no chão (cor da ponta; a borda vai a preto) */
void render_light_cone(int x, int z, int angle, int r, int g, int b);

/* Cor de fundo (céu): muda por fase */
void render_set_clear_color(int r, int g, int b);

/* Quantos bytes de primitivas foram usados no último quadro (debug) */
int render_stats_bytes(void);
int render_stats_polys(void);

#endif
