/* Gerado por tools/blender_export_psx.py a partir de: tools/make_default_assets.py
 * 6 vertices, 2 faces. Nao edite a mao: exporte de novo.
 */
#ifndef PASSARO_A_MESH_H
#define PASSARO_A_MESH_H
#include "mesh.h"

static const SVECTOR passaro_a_verts[6] = {
	{ 0, 0, 31, 0 },
	{ 0, 0, -26, 0 },
	{ 90, -31, -5, 0 },
	{ 0, 0, 31, 0 },
	{ -90, -31, -5, 0 },
	{ 0, 0, -26, 0 },
};

static const SVECTOR passaro_a_norms[2] = {
	{ -1328, -3875, 0, 0 },
	{ 1328, -3875, 0, 0 },
};

static const MESH_FACE passaro_a_faces[2] = {
	/* { v0,v1,v2,v3 }, r,g,b, flags, material, 0, { u0,v0,u1,v1,u2,v2,u3,v3 }, normal */
	{ { 0, 2, 1, 0 }, 12, 12, 14, 0x00, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
	{ { 3, 5, 4, 0 }, 12, 12, 14, 0x00, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 }, 1 },
};

const MESH passaro_a_mesh = {
	6, 2, passaro_a_verts, passaro_a_norms, passaro_a_faces, 96
};

#endif
