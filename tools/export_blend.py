# -*- coding: utf-8 -*-
"""
export_blend.py - Exporta TODOS os modelos de um .blend de uma vez

Chamado pelo "./dev models" para cada arquivo em assets/blender/*.blend.
Também pode ser usado direto:

  blender assets/blender/modelos.blend --background --python tools/export_blend.py -- \
      --models models --textures assets/textures

Quais objetos são exportados:
  - todos os objetos Mesh dentro da coleção chamada "PSX" (e subcoleções);
  - se não existir coleção "PSX", todos os objetos Mesh do arquivo.

Cada objeto vira models/<nome do objeto>.h  ->  no código: <nome>_mesh
  (o nome é convertido para minúsculas e caracteres válidos em C:
   "Robo Azul" -> robo_azul -> robo_azul_mesh)

Texturas: se o material do objeto usa um nó "Image Texture", o tamanho da
imagem é usado para os UVs e uma cópia PNG é salva em assets/textures/
(se ainda não estiver lá). No código: opt.tex = &tex_<nome da imagem>_t;

Propriedades opcionais no objeto (Object Properties > Custom Properties):
  psx_name   nome em C diferente do nome do objeto
  psx_scale  escala (padrão 256 unidades por metro)
"""
import os
import re
import shutil
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blender_export_psx as ex  # noqa: E402


def c_name(s):
    s = re.sub(r"[^A-Za-z0-9_]", "_", s.strip()).lower()
    s = re.sub(r"_+", "_", s).strip("_")
    if not s or s[0].isdigit():
        s = "m_" + s
    return s


def collect_objects():
    psx = None
    for col in bpy.data.collections:
        if col.name.upper() == "PSX":
            psx = col
            break
    objs = psx.all_objects if psx else bpy.data.objects
    return [o for o in objs if o.type == "MESH"], psx is not None


def save_texture(img, tex_dir):
    """Garante uma cópia PNG da imagem em assets/textures. Devolve o nome em C."""
    base = os.path.splitext(bpy.path.basename(img.filepath or img.name))[0] or img.name
    name = c_name(re.sub(r"-(4|8|16)bits?$", "", base, flags=re.IGNORECASE))
    # já existe uma textura com esse nome (com ou sem sufixo -Nbit)? usa ela
    if os.path.isdir(tex_dir):
        for f in os.listdir(tex_dir):
            stem = os.path.splitext(f)[0]
            if f.lower().endswith(".png") and c_name(re.sub(r"-(4|8|16)bits?$", "", stem, flags=re.I)) == name:
                return name
    os.makedirs(tex_dir, exist_ok=True)
    dst = os.path.join(tex_dir, name + ".png")
    src = bpy.path.abspath(img.filepath) if img.filepath else ""
    if src and os.path.isfile(src) and src.lower().endswith(".png"):
        shutil.copyfile(src, dst)
    else:
        # imagem empacotada no .blend ou em outro formato: salva como PNG
        copy = img.copy()
        copy.filepath_raw = dst
        copy.file_format = "PNG"
        copy.save()
        bpy.data.images.remove(copy)
    print("PSX   textura salva: %s" % dst)
    return name


def main(argv):
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("--models", required=True)
    ap.add_argument("--textures", required=True)
    a = ap.parse_args(argv)

    objs, from_col = collect_objects()
    if not objs:
        print("ERRO: nenhum objeto Mesh encontrado em %s" % bpy.data.filepath)
        return 1
    print("PSX %s: %d objeto(s)%s" % (os.path.basename(bpy.data.filepath), len(objs),
                                     " da coleção PSX" if from_col else ""))
    for obj in objs:
        name = c_name(str(obj.get("psx_name", obj.name)))
        scale = int(obj.get("psx_scale", 256))
        img = ex.find_image(obj)
        tw = th = 0
        note = None
        if img is not None and img.size[0] > 0:
            tw, th = img.size[0], img.size[1]
            tname = save_texture(img, a.textures)
            note = "Textura: opt.tex = &tex_%s_t;" % tname
        out = os.path.join(a.models, name + ".h")
        try:
            nv, nf = ex.export_object(obj, out, name, scale, tw, th, note=note)
        except Exception as e:  # noqa: BLE001
            print("ERRO %s: %s" % (obj.name, e))
            continue
        print("PSX   %-16s -> models/%s.h  (%d faces%s)" % (obj.name, name, nf,
              ", textura %dx%d" % (tw, th) if tw else ""))
    return 0


if __name__ == "__main__":
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    sys.exit(main(args))
