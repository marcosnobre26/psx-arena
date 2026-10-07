# PS1 Arena - projeto base de jogo 3D para PlayStation 1 (PSn00bSDK)
#
# Normalmente você compila com:   ./dev build
# (ou, sem o script:  cmake --preset default  e  cmake --build ./build)
#
# ASSETS AUTOMÁTICOS — não precisa editar este arquivo para adicionar:
#   src/*.c                 código novo
#   models/<nome>.h         modelo exportado do Blender   -> <nome>_mesh
#   assets/textures/*.png   textura (vira .TIM sozinha)   -> tex_<nome>_t
#   assets/tim/*.tim        textura .TIM pronta            -> tex_<nome>_t
#   assets/sounds/*.wav     som (vira .VAG sozinho)        -> sfx_<nome>
#                           sufixos: -loop (repete), -11k (11 025 Hz)
# Tudo isso é declarado em build/gen/assets_gen.h (incluído pelo game.h).
#
# Saídas em ./build:  arena.exe  e  arena.bin/.cue

cmake_minimum_required(VERSION 3.21)

project(
	psx-arena
	LANGUAGES    C ASM
	VERSION      1.0.0
	DESCRIPTION  "Projeto base de jogo 3D para PlayStation 1"
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(GEN_DIR ${PROJECT_BINARY_DIR}/gen)
set(TEX_DIR ${PROJECT_BINARY_DIR}/textures)
set(SND_DIR ${PROJECT_BINARY_DIR}/sounds)

# CONFIGURE_DEPENDS: arquivos novos são percebidos sozinhos na próxima build
file(GLOB _sources  CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.c)
file(GLOB _models   CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/models/*.h)
file(GLOB _pngs     CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/assets/textures/*.png)
file(GLOB _rawtims  CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/assets/tim/*.tim)
file(GLOB _wavs     CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/assets/sounds/*.wav)
list(SORT _models)
list(SORT _pngs)
list(SORT _rawtims)
list(SORT _wavs)

# Correção necessária apenas com o compilador mipsel-linux-gnu (Ubuntu,
# WSL, Docker): passa o ELF por tools/fix_gnu_stack.py antes do elf2x.
if(PSN00BSDK_TARGET MATCHES "linux")
	set(ELF2X ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/fix_gnu_stack.py --elf2x ${ELF2X})
endif()

psn00bsdk_add_executable(arena GPREL ${_sources} ${GEN_DIR}/assets_gen.c)

target_include_directories(arena PRIVATE
	${PROJECT_SOURCE_DIR}/src
	${PROJECT_SOURCE_DIR}/models
	${GEN_DIR}
)
target_compile_options(arena PRIVATE -Wall -Wno-unused-function -Wno-comment)

# ---------------------------------------------------------------------------
# Texturas: PNG -> TIM (tools/build_textures.py escolhe a posição na VRAM)
# ---------------------------------------------------------------------------
set(_tex_names)
set(_tex_tims)
foreach(_png ${_pngs})
	get_filename_component(_stem ${_png} NAME_WE)
	string(REGEX REPLACE "-(4|8|16)[bB][iI][tT][sS]?$" "" _name "${_stem}")
	string(MAKE_C_IDENTIFIER "${_name}" _name)
	list(APPEND _tex_names ${_name})
	list(APPEND _tex_tims  ${TEX_DIR}/${_name}.tim)
endforeach()

if(_pngs)
	add_custom_command(
		OUTPUT  ${_tex_tims}
		COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/build_textures.py --out ${TEX_DIR} ${_pngs}
		DEPENDS ${_pngs} ${PROJECT_SOURCE_DIR}/tools/build_textures.py
		COMMENT "Convertendo texturas PNG -> TIM"
		VERBATIM
	)
endif()

foreach(_tim ${_rawtims})
	get_filename_component(_name ${_tim} NAME_WE)
	string(MAKE_C_IDENTIFIER "${_name}" _name)
	list(APPEND _tex_names ${_name})
	list(APPEND _tex_tims  ${_tim})
endforeach()

foreach(_name _tim IN ZIP_LISTS _tex_names _tex_tims)
	psn00bsdk_target_incbin(arena PRIVATE tex_${_name} ${_tim})
endforeach()

# ---------------------------------------------------------------------------
# Sons: WAV -> VAG (tools/wav2vag.py; o nome do arquivo diz taxa e loop).
# Um comando por arquivo: trocar um WAV reconverte só ele.
# ---------------------------------------------------------------------------
set(_snd_names)
foreach(_wav ${_wavs})
	get_filename_component(_stem ${_wav} NAME_WE)
	string(REGEX REPLACE "(-([lL][oO][oO][pP]|11[kK]|22[kK]))+$" "" _name "${_stem}")
	string(MAKE_C_IDENTIFIER "${_name}" _name)
	if(_name IN_LIST _snd_names)
		message(FATAL_ERROR "Dois sons viram o mesmo nome 'sfx_${_name}': confira assets/sounds/")
	endif()
	list(APPEND _snd_names ${_name})
	add_custom_command(
		OUTPUT  ${SND_DIR}/${_name}.vag
		COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/wav2vag.py ${_wav} ${SND_DIR}/${_name}.vag
		DEPENDS ${_wav} ${PROJECT_SOURCE_DIR}/tools/wav2vag.py
		COMMENT "Convertendo som ${_stem}.wav -> ${_name}.vag"
		VERBATIM
	)
	psn00bsdk_target_incbin(arena PRIVATE snd_${_name} ${SND_DIR}/${_name}.vag)
endforeach()

# ---------------------------------------------------------------------------
# Gera build/gen/assets_gen.h e assets_gen.c com tudo declarado
# ---------------------------------------------------------------------------
set(_h "/* GERADO AUTOMATICAMENTE pelo CMakeLists.txt - nao edite. */\n")
string(APPEND _h "#ifndef ASSETS_GEN_H\n#define ASSETS_GEN_H\n#include \"render.h\"\n#include \"mesh.h\"\n#include \"sound.h\"\n\n")
string(APPEND _h "/* Texturas (assets/textures/*.png e assets/tim/*.tim) */\n")
set(_c "/* GERADO AUTOMATICAMENTE pelo CMakeLists.txt - nao edite. */\n#include \"assets_gen.h\"\n\n")
set(_load "void assets_load_textures(void) {\n")
foreach(_name ${_tex_names})
	string(APPEND _h "extern TEXTURE tex_${_name}_t;\n")
	string(APPEND _c "extern const uint32_t tex_${_name}[];\nTEXTURE tex_${_name}_t;\n")
	string(APPEND _load "\trender_load_texture(tex_${_name}, &tex_${_name}_t);\n")
endforeach()
string(APPEND _load "}\n")
string(APPEND _h "\n/* Carrega todas as texturas na VRAM (chamado no inicio do main) */\nvoid assets_load_textures(void);\n\n")
string(APPEND _c "\n${_load}\n")

string(APPEND _h "/* Sons (assets/sounds/*.wav) */\n")
set(_load "void assets_load_sounds(void) {\n")
foreach(_name ${_snd_names})
	string(APPEND _h "extern SOUND sfx_${_name};\n")
	string(APPEND _c "extern const uint32_t snd_${_name}[];\nSOUND sfx_${_name};\n")
	string(APPEND _load "\tsound_load(&sfx_${_name}, snd_${_name});\n")
endforeach()
string(APPEND _load "}\n")
string(APPEND _h "\n/* Carrega todos os sons na RAM do SPU (chamado no inicio do main) */\nvoid assets_load_sounds(void);\n\n")
string(APPEND _h "/* Modelos 3D (models/*.h) */\n")
string(APPEND _c "\n${_load}\n/* Modelos: cada models/X.h define \"const MESH X_mesh\" */\n")
foreach(_m ${_models})
	get_filename_component(_name ${_m} NAME_WE)
	string(MAKE_C_IDENTIFIER "${_name}" _name)
	string(APPEND _h "extern const MESH ${_name}_mesh;\n")
	string(APPEND _c "#include \"${_m}\"\n")
endforeach()
string(APPEND _h "\n#endif\n")

file(CONFIGURE OUTPUT ${GEN_DIR}/assets_gen.h CONTENT "${_h}" @ONLY)
file(CONFIGURE OUTPUT ${GEN_DIR}/assets_gen.c CONTENT "${_c}" @ONLY)

# ---------------------------------------------------------------------------
# Imagem de CD (arena.bin + arena.cue)
# ---------------------------------------------------------------------------
psn00bsdk_add_cd_image(
	iso      # nome do alvo
	arena    # nome dos arquivos de saída
	iso.xml  # configuração do mkpsxiso
	DEPENDS arena system.cnf
)
