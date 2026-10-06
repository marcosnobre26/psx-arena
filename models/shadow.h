/* Gerado por tools/blender_export_psx.py a partir de: tools/make_default_assets.py
 * 18 vertices, 6 faces. Nao edite a mao: exporte de novo.
 */
#ifndef SHADOW_MESH_H
#define SHADOW_MESH_H
#include "mesh.h"

static const SVECTOR shadow_verts[18] = {
	{ -128, 0, 0, 0 },
	{ -91, 0, -91, 0 },
	{ 0, 0, -128, 0 },
	{ -128, 0, 0, 0 },
	{ 0, 0, -128, 0 },
	{ 91, 0, -91, 0 },
	{ -128, 0, 0, 0 },
	{ 91, 0, -91, 0 },
	{ 128, 0, 0, 0 },
	{ -128, 0, 0, 0 },
	{ 128, 0, 0, 0 },
	{ 91, 0, 91, 0 },
	{ -128, 0, 0, 0 },
	{ 91, 0, 91, 0 },
	{ 0, 0, 128, 0 },
	{ -128, 0, 0, 0 },
	{ 0, 0, 128, 0 },
	{ -91, 0, 91, 0 },
};

static const SVECTOR shadow_norms[1] = {
	{ 0, -4096, 0, 0 },
};

static const MESH_FACE shadow_faces[6] = {
	/* { v0,v1,v2,v3 }, r,g,b, flags, material, 0, { u0,v0,u1,v1,u2,v2,u3,v3 }, normal */
	{ { 0, 2, 1, 0 }, 0, 0, 0, 0x04, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
	{ { 3, 5, 4, 0 }, 0, 0, 0, 0x04, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
	{ { 6, 8, 7, 0 }, 0, 0, 0, 0x04, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
	{ { 9, 11, 10, 0 }, 0, 0, 0, 0x04, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
	{ { 12, 14, 13, 0 }, 0, 0, 0, 0x04, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
	{ { 15, 17, 16, 0 }, 0, 0, 0, 0x04, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
};

const MESH shadow_mesh = {
	18, 6, shadow_verts, shadow_norms, shadow_faces, 129
};

#endif
