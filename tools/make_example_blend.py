# -*- coding: utf-8 -*-
"""
make_example_blend.py - Cria assets/blender/exemplos.blend com os modelos do jogo

Monta no Blender os mesmos modelos padrão (robô, inimigo, caixa, gema...)
com materiais e UVs, dentro da coleção "PSX". Assim você pode abrir o
arquivo, editar o robô no Blender e exportar de volta com ./dev models.

Uso (só precisa rodar se quiser recriar o arquivo):
  blender --background --python tools/make_example_blend.py
"""
import os
import sys

import bpy
import bmesh

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

import make_default_assets as mda  # noqa: E402

mda.Shape.save = lambda self, name: self   # não grava .h, só monta as formas


def srgb_to_linear(c):
    c = c / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def make_material(name, rgb, image=None):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    col = tuple(srgb_to_linear(c) for c in rgb) + (1.0,)
    bsdf.inputs["Base Color"].default_value = col
    mat.diffuse_color = col
    if image is not None:
        tex = nt.nodes.new("ShaderNodeTexImage")
        tex.image = image
        tex.interpolation = "Closest"          # visual "pixelado" do PS1
        tex.location = (-300, 300)
        nt.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])
    return mat


def build(name, shape, collection, x, image=None):
    me = bpy.data.meshes.new(name)
    verts, faces, mats, uvs = [], [], [], []
    for pts, rgb, mat, puv, unlit in shape.polys:
        base = len(verts)
        verts.extend(pts)
        faces.append(list(range(base, base + len(pts))))
        mats.append(mat)
        uvs.append(puv)
    me.from_pydata(verts, [], faces)
    me.update()

    # cores dos materiais = cor da primeira face que usa cada um
    first_rgb = {}
    for pts, rgb, mat, puv, unlit in shape.polys:
        first_rgb.setdefault(mat, rgb)
    for i, mname in enumerate(shape.materials):
        rgb = first_rgb.get(i, (200, 200, 200))
        if image is not None:
            rgb = (255, 255, 255)
        me.materials.append(make_material("%s_%s" % (name, mname), rgb, image))
    for poly, m in zip(me.polygons, mats):
        poly.material_index = m

    if any(u is not None for u in uvs):
        layer = me.uv_layers.new(name="UVMap")
        w, h = image.size if image else (64, 64)
        li = 0
        for poly, puv in zip(me.polygons, uvs):
            for k in range(poly.loop_total):
                u, v = puv[k] if puv else (0, 0)
                layer.data[poly.loop_start + k].uv = ((u + 0.5) / w, 1.0 - (v + 0.5) / h)

    # junta vértices repetidos (as caixas foram montadas face a face)
    bm = bmesh.new()
    bm.from_mesh(me)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0005)
    bm.to_mesh(me)
    bm.free()

    obj = bpy.data.objects.new(name, me)
    obj.location = (x, 0, 0)          # só para organizar a cena; a posição é ignorada ao exportar
    collection.objects.link(obj)
    return obj


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    col = bpy.data.collections.new("PSX")
    bpy.context.scene.collection.children.link(col)

    crate_png = os.path.join(ROOT, "assets", "textures", "crate.png")
    crate_img = bpy.data.images.load(crate_png)

    x = 0.0
    for make, name in zip(mda.MODELS, mda.MODEL_NAMES):
        shape = make()
        build(name, shape, col, x, crate_img if name == "crate" else None)
        x += 1.5

    out_dir = os.path.join(ROOT, "assets", "blender")
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, "exemplos.blend")
    # caminho relativo para a textura (funciona em qualquer pasta/PC)
    bpy.ops.wm.save_as_mainfile(filepath=path)
    crate_img.filepath = bpy.path.relpath(crate_png)
    bpy.ops.wm.save_mainfile(filepath=path, compress=True)
    print("PSX salvo: %s" % path)


if __name__ == "__main__":
    main()
