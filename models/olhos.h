/* Gerado por tools/blender_export_psx.py a partir de: tools/make_default_assets.py
 * 8 vertices, 2 faces. Nao edite a mao: exporte de novo.
 */
#ifndef OLHOS_MESH_H
#define OLHOS_MESH_H
#include "mesh.h"

static const SVECTOR olhos_verts[8] = {
	{ 24, 5, 0, 0 },
	{ 12, 5, 0, 0 },
	{ 12, -5, 0, 0 },
	{ 24, -5, 0, 0 },
	{ -12, 5, 0, 0 },
	{ -24, 5, 0, 0 },
	{ -24, -5, 0, 0 },
	{ -12, -5, 0, 0 },
};

static const SVECTOR olhos_norms[1] = {
	{ 0, 0, 4096, 0 },
};

static const MESH_FACE olhos_faces[2] = {
	/* { v0,v1,v2,v3 }, r,g,b, flags, material, 0, { u0,v0,u1,v1,u2,v2,u3,v3 }, normal */
	{ { 0, 3, 1, 2 }, 255, 235, 150, 0x05, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
	{ { 4, 7, 5, 6 }, 255, 235, 150, 0x05, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0 }, 0 },
};

const MESH olhos_mesh = {
	8, 2, olhos_verts, olhos_norms, olhos_faces, 25
};

#endif
