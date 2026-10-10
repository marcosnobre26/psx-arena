/*
 * camera.c - Câmera do jogo
 *
 * Duas partes, de propósito separadas:
 *
 *   camera_turn()          LÓGICA: gira g.cam_yaw pelos controles (L1/R1,
 *                          analógico direito). Fica em g porque a lógica
 *                          usa: o direcional do jogador é convertido para o
 *                          mundo pelo ângulo da câmera. No modo link os dois
 *                          consoles precisam do mesmo ângulo.
 *
 *   camera_view_update()   VISUAL: posição do olho, ponto de mira e
 *                          suavização. Fica AQUI (static), fora de g: não
 *                          muda a partida, então cada console pode ter a sua
 *                          sem quebrar o determinismo. Roda uma vez por
 *                          quadro, depois da lógica, repetindo a suavização
 *                          pelo número de passos do quadro (igual a 30 ou 60 FPS).
 *
 * Quem precisa saber onde está o olho (som posicional, rosnado) usa
 * camera_eye(). Nada da lógica deve ler a câmera visual.
 */
#include <psxpad.h>
#include "game.h"

static VECTOR eye;          /* olho (suavizado) */
static VECTOR look;         /* ponto de mira (centro do grupo) */

/* Lógica: gira a câmera pelos controles (chamado no game_tick, na partida) */
void camera_turn(void) {
	for (int i = 0; i < MAX_PLAYERS; i++) {          /* qualquer jogador gira */
		if (!g.players[i].active) continue;
		if (g.in[i].held & PAD_L1) g.cam_yaw -= CAM_TURN_SPEED;
		if (g.in[i].held & PAD_R1) g.cam_yaw += CAM_TURN_SPEED;
		g.cam_yaw += g.in[i].rx / 3;
	}
	g.cam_yaw &= 4095;
}

/* Calcula para onde a câmera quer ir. Devolve 0 se não há ninguém. */
static int camera_target(VECTOR *want) {
	/* centro dos jogadores vivos (se ninguém vivo, de todos) e o quanto
	 * estão afastados: a câmera recua para mostrar os dois */
	int n = 0, cx = 0, cy = 0, cz = 0;
	int minx = 0x7fffffff, maxx = -0x7fffffff, minz = 0x7fffffff, maxz = -0x7fffffff;
	int only_alive = players_alive() > 0;
	for (int i = 0; i < MAX_PLAYERS; i++) {
		const PLAYER *p = &g.players[i];
		if (!p->active || (only_alive && !player_alive(p))) continue;
		cx += p->pos.vx; cy += p->pos.vy; cz += p->pos.vz; n++;
		if (p->pos.vx < minx) minx = p->pos.vx;
		if (p->pos.vx > maxx) maxx = p->pos.vx;
		if (p->pos.vz < minz) minz = p->pos.vz;
		if (p->pos.vz > maxz) maxz = p->pos.vz;
	}
	if (n == 0) return 0;
	cx /= n; cy /= n; cz /= n;
	int extra = (n > 1) ? dist2d(maxx - minx, maxz - minz) * 2 / 3 : 0;
	if (extra > 1400) extra = 1400;

	/* Recua a câmera atrás do grupo. Se ela cair dentro de uma parede,
	 * aproxima e SOBE (fica mais "de cima"), assim ninguém some atrás de
	 * uma parede. */
	int s = isin(g.cam_yaw), c = icos(g.cam_yaw);
	int want_dist = CAM_DIST + extra;
	int dist;
	for (dist = want_dist; dist > CAM_MIN_DIST; dist -= 40) {
		want->vx = cx - ((s * dist) >> 12);
		want->vz = cz - ((c * dist) >> 12);
		if (!level_blocked(want->vx, want->vz, 64))
			break;
	}
	want->vy = (cy >> 1) - CAM_HEIGHT - extra * 3 / 4 - (want_dist - dist) * 3 / 4;

	look.vx = cx;
	look.vy = (cy >> 1) - CAM_LOOK_UP;
	look.vz = cz;
	return 1;
}

/* Visual: uma vez por quadro; 'ticks' = passos de lógica deste quadro */
void camera_view_update(int ticks) {
	VECTOR want;
	if (!camera_target(&want))
		return;
	for (int t = 0; t < ticks; t++) {      /* anda 1/4 do caminho por passo */
		eye.vx += (want.vx - eye.vx) >> 2;
		eye.vy += (want.vy - eye.vy) >> 2;
		eye.vz += (want.vz - eye.vz) >> 2;
	}
}

/* Coloca a câmera direto no lugar (nascer, recomeçar, teleporte) */
void camera_snap(void) {
	VECTOR want;
	if (camera_target(&want))
		eye = want;
}

void camera_apply(void) {
	render_set_camera(&eye, &look);
}

const VECTOR *camera_eye(void) {
	return &eye;
}
