/*
 * mesh.h - Formato de modelo 3D usado pelo jogo
 *
 * Os modelos em models/*.h são gerados pelo script
 * tools/blender_export_psx.py a partir do Blender, neste formato.
 *
 * Convenção de eixos do PS1 (diferente do Blender!):
 *   X = direita, Y = PARA BAIXO, Z = para frente
 * O exportador já faz a conversão: o "Z para cima" do Blender vira -Y,
 * e a frente do modelo (-Y no Blender, a vista "Front") vira +Z.
 */
#ifndef MESH_H
#define MESH_H

#include <stdint.h>
#include <psxgte.h>

#define FACE_QUAD       0x01    /* face de 4 vértices (senão, triângulo) */
#define FACE_TEXTURED   0x02    /* face tem coordenadas UV */
#define FACE_UNLIT      0x04    /* ignora a luz (cor "emissiva") */

typedef struct {
	uint16_t v[4];      /* índices dos vértices (v[3] só em quads)
	                       quads usam a ordem do PS1: v0 v1 / v2 v3 em "Z" */
	uint8_t  r, g, b;   /* cor da face (em faces texturizadas: 128 = neutro) */
	uint8_t  flags;     /* FACE_* */
	uint8_t  mat;       /* índice do material no Blender (para trocar cores) */
	uint8_t  pad;
	uint8_t  uv[8];     /* u0,v0, u1,v1, u2,v2, u3,v3 (em texels) */
	uint16_t n;         /* índice da normal da face */
} MESH_FACE;

typedef struct {
	uint16_t        nverts;
	uint16_t        nfaces;
	const SVECTOR   *verts;
	const SVECTOR   *norms;
	const MESH_FACE *faces;
	int16_t         radius; /* raio aproximado, usado para descarte */
} MESH;

#endif
