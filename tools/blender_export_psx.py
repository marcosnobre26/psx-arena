# -*- coding: utf-8 -*-
"""
blender_export_psx.py - Exporta um objeto do Blender para o formato MESH do jogo

Gera um arquivo models/<nome>.h com vértices, normais, faces, cores e UVs,
pronto para ser incluído em src/models.c.

COMO USAR (3 jeitos):

 1) Dentro do Blender (mais fácil):
      - Abra a aba "Scripting", clique em "Open" e escolha este arquivo.
      - Clique em "Run Script" (▶). Isso adiciona o menu
        File > Export > PS1 Mesh (.h)
      - Selecione seu objeto e use esse menu.

 2) Pela linha de comando (sem abrir a interface):
      blender meu_modelo.blend --background --python tools/blender_export_psx.py -- \
          --object Robo --name robo --out models/robo.h --scale 256

 3) Como biblioteca Python (o tools/make_default_assets.py faz isso para
    gerar os modelos padrão sem precisar do Blender).

DICAS DE MODELAGEM PARA PS1:
  - Mantenha poucos polígonos: 50 a 500 faces por modelo é o normal da época.
  - 1 metro no Blender = 256 unidades no jogo (= 1 bloco do mapa).
  - A FRENTE do modelo é a vista "Front" do Blender (olhando para -Y).
  - Deixe a origem do objeto nos PÉS do personagem.
  - Cada material vira uma cor. O índice do material (0, 1, 2...) pode ser
    trocado no jogo por uma paleta (é assim que funcionam as "skins").
  - Faces com mais de 4 vértices são trianguladas automaticamente.
  - Para textura: tenha um UV map e um nó "Image Texture" no material.
    O tamanho da imagem é detectado sozinho (ou informe --tex-size 64 64).
    Texturas do PS1 são pequenas: 64x64 ou 128x128 é o usual.
  - O nome em C é o nome do ARQUIVO: models/robo.h -> robo_mesh.
  - Coordenadas precisam caber em 16 bits: o modelo deve ter no máximo
    ~120 metros a partir da origem (com escala 256).
"""

import math
import os
import sys

# --------------------------------------------------------------------------
# Parte 1: lógica pura (não depende do Blender)
# --------------------------------------------------------------------------

ONE = 4096
FACE_QUAD, FACE_TEXTURED, FACE_UNLIT = 0x01, 0x02, 0x04


def blender_to_psx(co, scale):
    """Converte um ponto do Blender (Z para cima, frente = -Y) para o PS1
    (Y para baixo, frente = +Z). É uma rotação pura, não espelha o modelo."""
    x, y, z = co
    return (int(round(-x * scale)), int(round(-z * scale)), int(round(-y * scale)))


def normal_to_psx(n):
    x, y, z = n
    length = math.sqrt(x * x + y * y + z * z) or 1.0
    return (int(round(-x / length * ONE)), int(round(-z / length * ONE)), int(round(-y / length * ONE)))


def psx_order(indices):
    """Recebe índices em ordem anti-horária vista de fora (padrão Blender)
    e devolve na ordem esperada pelo PS1:
      - a GTE considera "de frente" a ordem inversa da do Blender;
      - quads do PS1 usam ordem em "Z" (v0 v1 / v2 v3), não circular."""
    if len(indices) == 3:
        a, b, c = indices
        return [a, c, b]
    a, b, c, d = indices
    return [a, d, b, c]


def linear_to_srgb(c):
    c = max(0.0, min(1.0, c))
    return 12.92 * c if c <= 0.0031308 else 1.055 * (c ** (1.0 / 2.4)) - 0.055


class MeshBuilder:
    """Acumula vértices/faces já no formato do PS1 e escreve o .h"""

    def __init__(self):
        self.verts = []
        self.norms = []
        self._norm_index = {}
        self.faces = []

    def add_vertex(self, psx_xyz):
        self.verts.append(tuple(psx_xyz))
        return len(self.verts) - 1

    def add_normal(self, psx_n):
        key = tuple(psx_n)
        if key not in self._norm_index:
            self._norm_index[key] = len(self.norms)
            self.norms.append(key)
        return self._norm_index[key]

    def add_face(self, blender_ccw_indices, normal_psx, rgb, mat=0, uvs=None, unlit=False):
        """blender_ccw_indices: 3 ou 4 índices em ordem anti-horária vista de
        fora (como o Blender guarda). uvs: lista de (u,v) em texels na mesma
        ordem dos índices, ou None."""
        n = len(blender_ccw_indices)
        assert n in (3, 4)
        order = psx_order(list(range(n)))
        idx = [blender_ccw_indices[i] for i in order]
        flags = FACE_QUAD if n == 4 else 0
        uv8 = [0] * 8
        if uvs is not None:
            flags |= FACE_TEXTURED
            for k, i in enumerate(order):
                u, v = uvs[i]
                uv8[k * 2] = max(0, min(255, int(round(u))))
                uv8[k * 2 + 1] = max(0, min(255, int(round(v))))
        if unlit:
            flags |= FACE_UNLIT
        while len(idx) < 4:
            idx.append(0)
        r, g, b = [max(0, min(255, int(round(c)))) for c in rgb]
        self.faces.append({
            "v": idx, "rgb": (r, g, b), "flags": flags, "mat": mat,
            "uv": uv8, "n": self.add_normal(normal_psx),
        })

    def radius(self):
        r = 0
        for x, y, z in self.verts:
            r = max(r, math.sqrt(x * x + y * y + z * z))
        return int(math.ceil(r))

    def write_header(self, path, name, source="?", note=None):
        for v in self.verts:
            for c in v:
                if c < -32768 or c > 32767:
                    raise ValueError("Vértice fora do limite de 16 bits (%d). Diminua a escala ou o modelo." % c)
        if len(self.verts) > 65535 or len(self.faces) > 65535:
            raise ValueError("Modelo grande demais.")

        guard = name.upper() + "_MESH_H"
        out = []
        out.append("/* Gerado por tools/blender_export_psx.py a partir de: %s" % source)
        out.append(" * %d vertices, %d faces. Nao edite a mao: exporte de novo." % (len(self.verts), len(self.faces)))
        if note:
            out.append(" * " + note)
        out.append(" */")
        out.append("#ifndef %s" % guard)
        out.append("#define %s" % guard)
        out.append('#include "mesh.h"')
        out.append("")
        out.append("static const SVECTOR %s_verts[%d] = {" % (name, len(self.verts)))
        for x, y, z in self.verts:
            out.append("\t{ %d, %d, %d, 0 }," % (x, y, z))
        out.append("};")
        out.append("")
        out.append("static const SVECTOR %s_norms[%d] = {" % (name, len(self.norms)))
        for x, y, z in self.norms:
            out.append("\t{ %d, %d, %d, 0 }," % (x, y, z))
        out.append("};")
        out.append("")
        out.append("static const MESH_FACE %s_faces[%d] = {" % (name, len(self.faces)))
        out.append("\t/* { v0,v1,v2,v3 }, r,g,b, flags, material, 0, { u0,v0,u1,v1,u2,v2,u3,v3 }, normal */")
        for f in self.faces:
            out.append("\t{ { %d, %d, %d, %d }, %d, %d, %d, 0x%02x, %d, 0, { %s }, %d }," % (
                f["v"][0], f["v"][1], f["v"][2], f["v"][3],
                f["rgb"][0], f["rgb"][1], f["rgb"][2], f["flags"], f["mat"],
                ", ".join(str(u) for u in f["uv"]), f["n"]))
        out.append("};")
        out.append("")
        out.append("const MESH %s_mesh = {" % name)
        out.append("\t%d, %d, %s_verts, %s_norms, %s_faces, %d" % (
            len(self.verts), len(self.faces), name, name, name, self.radius()))
        out.append("};")
        out.append("")
        out.append("#endif")
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        with open(path, "w", encoding="utf-8", newline="\n") as fp:
            fp.write("\n".join(out) + "\n")


# --------------------------------------------------------------------------
# Parte 2: integração com o Blender
# --------------------------------------------------------------------------

try:
    import bpy
    from bpy.props import StringProperty, IntProperty, BoolProperty
    from bpy_extras.io_utils import ExportHelper
    HAVE_BPY = True
except ImportError:
    HAVE_BPY = False


def _material_rgb(mat):
    """Cor base do material (sRGB 0..1). Lê o Principled BSDF se existir."""
    if mat is None:
        return (0.8, 0.8, 0.8)
    col = None
    if getattr(mat, "use_nodes", False) and mat.node_tree:
        for node in mat.node_tree.nodes:
            if node.type == "BSDF_PRINCIPLED":
                col = node.inputs["Base Color"].default_value
                break
            if node.type == "EMISSION":
                col = node.inputs["Color"].default_value
                break
    if col is None:
        col = mat.diffuse_color
    return tuple(linear_to_srgb(c) for c in col[:3])


def find_image(obj):
    """Primeira imagem (nó Image Texture) usada nos materiais do objeto."""
    for slot in obj.material_slots:
        mat = slot.material
        if mat and getattr(mat, "node_tree", None):
            for node in mat.node_tree.nodes:
                if node.type == "TEX_IMAGE" and node.image is not None:
                    return node.image
    return None


def export_object(obj, filepath, name, scale=256, tex_w=0, tex_h=0,
                  use_vertex_colors=False, apply_modifiers=True, note=None):
    if obj is None or obj.type != "MESH":
        raise ValueError("Selecione um objeto do tipo Mesh.")

    depsgraph = bpy.context.evaluated_depsgraph_get()
    src = obj.evaluated_get(depsgraph) if apply_modifiers else obj
    me = src.to_mesh()
    try:
        m3 = obj.matrix_world.to_3x3()            # rotação + escala, sem posição
        nmat = m3.inverted_safe().transposed()

        mb = MeshBuilder()
        for v in me.vertices:
            mb.add_vertex(blender_to_psx(m3 @ v.co, scale))

        uv_layer = me.uv_layers.active if (tex_w > 0 and tex_h > 0) else None

        col_attr = None
        if use_vertex_colors and hasattr(me, "color_attributes"):
            col_attr = me.color_attributes.active_color

        for poly in me.polygons:
            vidx = list(poly.vertices)
            loops = list(poly.loop_indices)
            normal = normal_to_psx(nmat @ poly.normal)

            mat = obj.material_slots[poly.material_index].material if obj.material_slots else None
            rgb = _material_rgb(mat)
            unlit = bool(mat and mat.name.upper().endswith("_UNLIT"))

            if col_attr is not None:
                acc = [0.0, 0.0, 0.0]
                for li, vi in zip(loops, vidx):
                    item = col_attr.data[li if col_attr.domain == "CORNER" else vi]
                    c = item.color_srgb if hasattr(item, "color_srgb") else item.color
                    for k in range(3):
                        acc[k] += c[k]
                rgb = tuple(a / len(loops) for a in acc)

            uvs = None
            if uv_layer is not None:
                uvs = []
                for li in loops:
                    u, v = uv_layer.data[li].uv
                    uvs.append((math.floor(u * tex_w), math.floor((1.0 - v) * tex_h)))
                uvs = [(min(u, tex_w - 1), min(v, tex_h - 1)) for (u, v) in uvs]
                face_rgb = tuple(c * 128 for c in rgb)   # 128 = textura sem alteração
            else:
                face_rgb = tuple(c * 255 for c in rgb)

            # Triangula polígonos com mais de 4 lados (em leque)
            if len(vidx) <= 4:
                parts = [list(range(len(vidx)))]
            else:
                parts = [[0, i, i + 1] for i in range(1, len(vidx) - 1)]

            for part in parts:
                mb.add_face([vidx[i] for i in part], normal, face_rgb,
                            mat=poly.material_index,
                            uvs=[uvs[i] for i in part] if uvs else None,
                            unlit=unlit)

        mb.write_header(filepath, name, source="%s (%s)" % (os.path.basename(bpy.data.filepath) or "sem nome", obj.name),
                        note=note)
        return len(mb.verts), len(mb.faces)
    finally:
        src.to_mesh_clear()


if HAVE_BPY:
    class EXPORT_OT_psx_mesh(bpy.types.Operator, ExportHelper):
        """Exporta o objeto ativo como modelo do PlayStation 1 (header C)"""
        bl_idname = "export_mesh.psx_header"
        bl_label = "Exportar PS1 Mesh"
        filename_ext = ".h"
        filter_glob: StringProperty(default="*.h", options={"HIDDEN"})

        mesh_name: StringProperty(name="Nome em C", default="",
                                  description="Nome usado no código (ex: robo -> robo_mesh). Vazio = nome do arquivo")
        scale: IntProperty(name="Escala", default=256, min=1, max=4096,
                           description="Unidades do jogo por metro do Blender")
        tex_w: IntProperty(name="Largura da textura", default=0, min=0, max=256,
                           description="0 = automático (tamanho da imagem usada no material)")
        tex_h: IntProperty(name="Altura da textura", default=0, min=0, max=256)
        use_vertex_colors: BoolProperty(name="Usar Color Attribute", default=False,
                                        description="Usa a cor pintada nos vértices em vez da cor do material")
        apply_modifiers: BoolProperty(name="Aplicar modificadores", default=True)

        def execute(self, context):
            obj = context.active_object
            # o nome em C precisa ser igual ao nome do arquivo (models/robo.h -> robo_mesh)
            name = self.mesh_name or _c_name(os.path.splitext(os.path.basename(self.filepath))[0])
            tw, th = self.tex_w, self.tex_h
            img = find_image(obj) if obj else None
            if tw == 0 and img is not None and img.size[0] > 0:
                tw, th = img.size[0], img.size[1]
            try:
                nv, nf = export_object(obj, self.filepath, name, self.scale,
                                       tw, th, self.use_vertex_colors, self.apply_modifiers)
            except Exception as e:
                self.report({"ERROR"}, str(e))
                return {"CANCELLED"}
            self.report({"INFO"}, "PS1: %d vértices, %d faces -> %s" % (nv, nf, self.filepath))
            return {"FINISHED"}

    def _menu(self, context):
        self.layout.operator(EXPORT_OT_psx_mesh.bl_idname, text="PS1 Mesh (.h)")

    def register():
        old = getattr(bpy.types, "EXPORT_MESH_OT_psx_header", None)
        if old is not None:       # permite rodar o script mais de uma vez
            try:
                bpy.utils.unregister_class(old)
            except Exception:
                pass
        bpy.utils.register_class(EXPORT_OT_psx_mesh)
        bpy.types.TOPBAR_MT_file_export.append(_menu)

    def unregister():
        bpy.types.TOPBAR_MT_file_export.remove(_menu)
        bpy.utils.unregister_class(EXPORT_OT_psx_mesh)

bl_info = {
    "name": "PS1 Mesh Exporter (PSX Arena)",
    "blender": (2, 93, 0),
    "category": "Import-Export",
    "location": "File > Export > PS1 Mesh (.h)",
}


def _c_name(s):
    out = "".join(c.lower() if c.isalnum() else "_" for c in s)
    if not out or out[0].isdigit():
        out = "m_" + out
    return out


def _cli(argv):
    import argparse
    p = argparse.ArgumentParser(description="Exporta um objeto do Blender para PS1")
    p.add_argument("--object", required=True, help="nome do objeto no .blend")
    p.add_argument("--out", required=True, help="arquivo .h de saída")
    p.add_argument("--name", default="", help="nome em C (padrão: nome do arquivo de saída)")
    p.add_argument("--scale", type=int, default=256)
    p.add_argument("--tex-size", type=int, nargs=2, default=[0, 0], metavar=("W", "H"))
    p.add_argument("--vertex-colors", action="store_true")
    a = p.parse_args(argv)
    obj = bpy.data.objects.get(a.object)
    if obj is None:
        print("Objeto '%s' não encontrado. Objetos: %s" % (a.object, [o.name for o in bpy.data.objects]))
        sys.exit(1)
    name = a.name or _c_name(os.path.splitext(os.path.basename(a.out))[0])
    tw, th = a.tex_size
    img = find_image(obj)
    if tw == 0 and img is not None and img.size[0] > 0:
        tw, th = img.size[0], img.size[1]
    nv, nf = export_object(obj, a.out, name, a.scale, tw, th, a.vertex_colors)
    print("OK: %d vértices, %d faces -> %s" % (nv, nf, a.out))


if HAVE_BPY and __name__ == "__main__":
    if "--" in sys.argv:
        _cli(sys.argv[sys.argv.index("--") + 1:])
    else:
        register()
        print("Menu 'File > Export > PS1 Mesh (.h)' adicionado.")
