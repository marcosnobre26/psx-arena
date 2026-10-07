/*
 * sound.h - Efeitos sonoros (.VAG) no SPU
 *
 * Cada assets/sounds/<nome>.wav vira, no build, um "SOUND sfx_<nome>"
 * (declarado em build/gen/assets_gen.h) já carregado na RAM do SPU por
 * assets_load_sounds(). Prioridade e variação de tom de cada som ficam
 * na tabela sound_defs (data.c).
 *
 * Volumes: 0..SOUND_VOL_MAX (0x3fff, o máximo de uma voz do SPU).
 * Volumes mestre: 0..128 (128 = 100%).
 *
 * Som é EFEITO COLATERAL: pode ser disparado da lógica, mas nada aqui
 * escreve em g nem usa g.rng (as variações usam fx_range).
 */
#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

#define SOUND_VOL_MAX     0x3fff
#define SOUND_LOOP_VOICES 2         /* vozes 0 e 1: loops de ambiente */

typedef struct {
	uint32_t addr;      /* endereço na RAM do SPU (0 = não carregado) */
	int      rate;      /* taxa de amostragem (Hz) */
	int      frames;    /* duração em quadros de 1/60 s (para liberar a voz) */
	int      prio;      /* som novo só rouba voz de prioridade <= à dele */
	int      pitch_var; /* variação aleatória de tom, em % (0 = nenhuma) */
} SOUND;

void sound_init(void);
void sound_load(SOUND *s, const uint32_t *vag_file);
void sound_setup(void);                         /* aplica a tabela sound_defs */

void sound_play(const SOUND *s, int vol);                  /* sem posição */
void sound_play_at(const SOUND *s, int x, int z, int vol); /* posicional */

void sound_loop_start(int voice, const SOUND *s, int vol); /* voice 0..1 */
void sound_loop_stop(int voice);

void sound_set_volume(int sfx, int music);      /* volumes mestre 0..128 */
int  sound_spu_used(void);                      /* bytes ocupados na RAM do SPU */

#endif
