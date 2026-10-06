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

typedef struct {
	const CVECTOR *palette;  /* troca a cor por material: palette[face.mat] */
	int            npalette;
	const TEXTURE *tex;      /* textura para faces com UV */
	int            flags;    /* DRAW_* */
	int            zbias;    /* negativo = desenha "mais à frente" */
} DRAWOPT;

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

/* Quantos bytes de primitivas foram usados no último quadro (debug) */
int render_stats_bytes(void);

#endif
