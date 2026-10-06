/*
 * input.c - Leitura dos controles das portas 1 e 2 (digital ou DualShock)
 *
 * O driver da BIOS lê os dois controles sozinho a cada VSync e escreve
 * num buffer para cada porta. Nos bits de botão, 0 = apertado (lógica
 * invertida), por isso o "~".
 *
 * Jogando ONLINE por netplay (RetroArch), o jogador remoto aparece aqui
 * simplesmente como o controle da porta 2 — o jogo não precisa saber.
 */
#include <psxapi.h>
#include <psxpad.h>
#include "game.h"

static uint8_t  pad_buf[MAX_PLAYERS][34];
static uint16_t prev_held[MAX_PLAYERS];

void input_init(void) {
	InitPAD(pad_buf[0], 34, pad_buf[1], 34);
	StartPAD();
	ChangeClearPAD(0);
}

static int stick(uint8_t v) {
	int s = (int)v - 128;
	return (s > -24 && s < 24) ? 0 : s;   /* zona morta */
}

void input_update(INPUT in[MAX_PLAYERS]) {
	for (int i = 0; i < MAX_PLAYERS; i++) {
		PADTYPE *pad = (PADTYPE *)pad_buf[i];
		INPUT   *n   = &in[i];

		n->connected = (pad->stat == 0);
		n->held = 0;
		n->lx = n->ly = n->rx = n->ry = 0;

		if (n->connected) {
			/* 0x4 = digital, 0x5 = analógico (stick), 0x7 = DualShock */
			if (pad->type == 0x4 || pad->type == 0x5 || pad->type == 0x7)
				n->held = ~pad->btn;
			else
				n->connected = 0;
			if (pad->type == 0x5 || pad->type == 0x7) {
				n->lx = stick(pad->ls_x);
				n->ly = stick(pad->ls_y);
				n->rx = stick(pad->rs_x);
				n->ry = stick(pad->rs_y);
			}
		}

		n->pressed = n->held & ~prev_held[i];
		prev_held[i] = n->held;
	}
}
