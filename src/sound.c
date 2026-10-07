/*
 * sound.c - Toca amostras .VAG no SPU (o chip de som do PS1)
 *
 * O SPU tem 512 KB de RAM própria e 24 vozes. Cada voz toca uma amostra
 * ADPCM a partir de um endereço dessa RAM, com volume esquerdo/direito e
 * tom (pitch) próprios. Fluxo:
 *   1. no boot, sound_load() copia cada .VAG (embutido no EXE) para a RAM do SPU;
 *   2. sound_play() escolhe uma voz livre e manda tocar.
 *
 * Divisão das vozes:
 *   0..1   loops de ambiente (vento, grilos), controlados à mão;
 *   2..23  efeitos, em rodízio com prioridade.
 *
 * O SPU não avisa quando uma voz termina (o SDK não expõe o registrador
 * ENDX). Então guardamos, para cada voz, em que quadro o som acaba
 * (duração calculada no carregamento). Esse estado é só de áudio: fica
 * fora de g de propósito e não afeta o determinismo da lógica.
 */
#include <psxgpu.h>
#include <psxspu.h>
#include <hwregs_c.h>
#include "game.h"

#define NUM_VOICES   24
#define FIRST_SFX    SOUND_LOOP_VOICES
#define NUM_SFX      (NUM_VOICES - FIRST_SFX)
#define SPU_RAM_SIZE 0x80000

/* Os primeiros 4 KB do SPU são reservados e o psxspu grava uma amostra
 * vazia em 0x1000; nossas amostras começam depois disso. */
#define SPU_ALLOC_START 0x1010

typedef struct {               /* cabeçalho do arquivo .VAG (big-endian) */
	uint32_t magic, version, interleave, size, sample_rate;
	uint16_t reserved[5], channels;
	char     name[16];
} VAG_HEADER;

static uint32_t next_addr = SPU_ALLOC_START;
static int      next_sfx  = 0;           /* rodízio entre as vozes de efeito */
static int      sfx_master = 128, music_master = 128;

static struct {
	int prio;
	int end;                   /* VSync(-1) em que o som termina */
} voices[NUM_VOICES];

static const SOUND *loop_sound[SOUND_LOOP_VOICES];
static int          loop_vol[SOUND_LOOP_VOICES];

/* Os campos do cabeçalho VAG são big-endian; o PS1 é little-endian.
 * Não use __builtin_bswap32: com este GCC ele chama uma rotina da libgcc
 * compilada para outro ABI e o jogo trava no boot (lição da receita R8). */
static uint32_t be32(uint32_t v) {
	const uint8_t *b = (const uint8_t *)&v;
	return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}

void sound_init(void) {
	SpuInit();
	sound_set_volume(sfx_master, music_master);
}

void sound_load(SOUND *s, const uint32_t *vag_file) {
	const VAG_HEADER *h = (const VAG_HEADER *)vag_file;
	s->addr = 0;
	s->frames = 0;
	s->prio = 1;
	s->pitch_var = 0;
	if (h->magic != 0x70474156)                 /* "VAGp" lido como little-endian */
		return;

	int bytes = be32(h->size);
	int size  = (bytes + 63) & ~63;             /* DMA em blocos de 64 bytes */
	if (next_addr + size > SPU_RAM_SIZE)
		return;                                 /* RAM do SPU cheia: fica mudo */

	SpuSetTransferMode(SPU_TRANSFER_BY_DMA);
	SpuSetTransferStartAddr(next_addr);
	SpuWrite((const uint32_t *)(h + 1), size);
	SpuIsTransferCompleted(SPU_TRANSFER_WAIT);

	s->addr = next_addr;
	s->rate = be32(h->sample_rate);
	/* 16 bytes = 28 amostras; +1 quadro de folga */
	s->frames = (bytes / 16) * 28 * 60 / s->rate + 1;
	next_addr += size;
}

void sound_setup(void) {
	for (const SOUND_DEF *d = sound_defs; d->sound; d++) {
		d->sound->prio      = d->prio;
		d->sound->pitch_var = d->pitch_var;
	}
}

/* Liga uma voz. pitch no formato do SPU (4096 = 44 100 Hz). */
static void voice_start(int ch, const SOUND *s, int vl, int vr, int pitch) {
	SpuSetKey(0, 1 << ch);                   /* para a voz, se estiver tocando */
	SPU_CH_FREQ(ch)  = pitch;
	SPU_CH_ADDR(ch)  = getSPUAddr(s->addr);
	SPU_CH_VOL_L(ch) = vl;
	SPU_CH_VOL_R(ch) = vr;
	SPU_CH_ADSR1(ch) = 0x00ff;               /* envelope desligado */
	SPU_CH_ADSR2(ch) = 0x0000;
	SpuSetKey(1, 1 << ch);                   /* toca */
}

/* Tom com variação aleatória (fx_range: não mexe no gerador da lógica) */
static int sound_pitch(const SOUND *s) {
	int p = getSPUSampleRate(s->rate);
	if (s->pitch_var)
		p = p * (100 + fx_range(-s->pitch_var, s->pitch_var)) / 100;
	return p > 0x3fff ? 0x3fff : p;
}

/* Escolhe a voz de efeito: uma livre; senão, rouba a de menor prioridade
 * (empate: a que vai acabar primeiro). Devolve -1 se todas forem mais
 * importantes que o som novo. */
static int voice_alloc(int prio) {
	int now = VSync(-1);
	int best = -1;
	for (int k = 0; k < NUM_SFX; k++) {
		int ch = FIRST_SFX + (next_sfx + k) % NUM_SFX;
		if (voices[ch].end - now <= 0) {     /* já terminou: livre */
			best = ch;
			break;
		}
		if (best < 0 || voices[ch].prio < voices[best].prio ||
		    (voices[ch].prio == voices[best].prio && voices[ch].end < voices[best].end))
			best = ch;
	}
	if (voices[best].end - now > 0 && voices[best].prio > prio)
		return -1;
	next_sfx = (best - FIRST_SFX + 1) % NUM_SFX;
	return best;
}

static void play(const SOUND *s, int vl, int vr) {
	if (!s->addr || (vl <= 0 && vr <= 0))
		return;
	int ch = voice_alloc(s->prio);
	if (ch < 0)
		return;
	voices[ch].prio = s->prio;
	voices[ch].end  = VSync(-1) + s->frames;
	vl = (vl * sfx_master) >> 7;
	vr = (vr * sfx_master) >> 7;
	voice_start(ch, s, vl > SOUND_VOL_MAX ? SOUND_VOL_MAX : vl,
	                   vr > SOUND_VOL_MAX ? SOUND_VOL_MAX : vr, sound_pitch(s));
}

void sound_play(const SOUND *s, int vol) {
	play(s, vol, vol);
}

/* Som com posição no mundo: mais baixo com a distância até a câmera e
 * puxado para o lado em que está em relação à direção da câmera. */
void sound_play_at(const SOUND *s, int x, int z, int vol) {
	int dx = x - g.cam_pos.vx, dz = z - g.cam_pos.vz;
	int d  = dist2d(dx, dz);
	if (d >= SOUND_FAR)
		return;                              /* longe demais: inaudível */
	if (d > SOUND_NEAR)
		vol = vol * (SOUND_FAR - d) / (SOUND_FAR - SOUND_NEAR);

	/* pan: seno do ângulo relativo. +4096 = todo à direita, -4096 = à
	 * esquerda. O lado oposto cai até 25% (nunca some de todo). */
	int pan = isin((angle_of(dx, dz) - g.cam_yaw) & 4095);
	int vl = vol, vr = vol;
	if (pan > 0) vl -= (vol * pan * 3) >> 14;
	else         vr -= (vol * -pan * 3) >> 14;
	play(s, vl, vr);
}

void sound_loop_start(int voice, const SOUND *s, int vol) {
	if (voice < 0 || voice >= SOUND_LOOP_VOICES || !s->addr)
		return;
	loop_sound[voice] = s;
	loop_vol[voice]   = vol;
	vol = (vol * sfx_master) >> 7;
	voice_start(voice, s, vol, vol, getSPUSampleRate(s->rate));
}

void sound_loop_stop(int voice) {
	if (voice < 0 || voice >= SOUND_LOOP_VOICES)
		return;
	SpuSetKey(0, 1 << voice);
	loop_sound[voice] = NULL;
}

void sound_set_volume(int sfx, int music) {
	sfx_master   = sfx;
	music_master = music;
	/* loops já tocando acompanham o volume novo */
	for (int v = 0; v < SOUND_LOOP_VOICES; v++)
		if (loop_sound[v]) {
			int vol = (loop_vol[v] * sfx_master) >> 7;
			SPU_CH_VOL_L(v) = vol;
			SPU_CH_VOL_R(v) = vol;
		}
	/* música por CD-DA (etapa 01b): volume da entrada de CD do SPU */
	int cd = (music_master * 0x7fff) >> 7;
	if (cd > 0x7fff) cd = 0x7fff;
	SpuSetCommonCDVolume(cd, cd);
}

int sound_spu_used(void) {
	return next_addr;
}
