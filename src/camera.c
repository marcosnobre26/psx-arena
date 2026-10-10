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

/* Grupo de jogadores visíveis para a câmera */
static int grp_n, grp_x[MAX_PLAYERS], grp_z[MAX_PLAYERS];
static int cx, cy, cz;             /* centro do grupo */
static int extra;                  /* recuo para caber os dois jogadores */
static int clear_dist = CAM_DIST;  /* última distância sem tronco no caminho */
static int base_dist = CAM_DIST;   /* distância "normal" (menor na mata densa), suavizada */
static int cam_tests, cam_tests_last, cam_cells_last;

static int collect_group(void) {
	int only_alive = players_alive() > 0;
	int minx = 0x7fffffff, maxx = -0x7fffffff, minz = 0x7fffffff, maxz = -0x7fffffff;
	grp_n = 0;
	cx = cy = cz = 0;
	for (int i = 0; i < MAX_PLAYERS; i++) {
		const PLAYER *p = &g.players[i];
		if (!p->active || (only_alive && !player_alive(p))) continue;
		grp_x[grp_n] = p->pos.vx;
		grp_z[grp_n] = p->pos.vz;
		grp_n++;
		cx += p->pos.vx; cy += p->pos.vy; cz += p->pos.vz;
		if (p->pos.vx < minx) minx = p->pos.vx;
		if (p->pos.vx > maxx) maxx = p->pos.vx;
		if (p->pos.vz < minz) minz = p->pos.vz;
		if (p->pos.vz > maxz) maxz = p->pos.vz;
	}
	if (grp_n == 0) return 0;
	cx /= grp_n; cy /= grp_n; cz /= grp_n;
	extra = (grp_n > 1) ? dist2d(maxx - minx, maxz - minz) * 2 / 3 : 0;
	if (extra > 1400) extra = 1400;
	look.vx = cx;
	look.vy = (cy >> 1) - CAM_LOOK_UP;
	look.vz = cz;
	return 1;
}

/* Olho na distância d, na direção (s, c) atrás do grupo. Mais perto que o
 * desejado = mais alto (fica mais "de cima"), para ninguém sumir. */
static void eye_at(int d, int want_dist, int s, int c, VECTOR *out) {
	out->vx = cx - ((s * d) >> 12);
	out->vz = cz - ((c * d) >> 12);
	/* altura proporcional à distância-base (mesmo ângulo na mata densa) */
	out->vy = (cy >> 1) - CAM_HEIGHT * base_dist / CAM_DIST - extra * 3 / 4 - (want_dist - d) * 3 / 4;
}

/* Distância-base pela densidade da mata em volta do grupo: de CAM_DIST
 * (aberto) até CAM_DENSE_DIST (fechado), em 'ticks' passos suaves */
static void update_base(int ticks, int snap) {
	int dens = level_tree_density(cx, cz, 4 * TILE_SIZE);
	if (dens > CAM_DENSE_FULL) dens = CAM_DENSE_FULL;
	/* 2 jogadores afastados: o ENQUADRAMENTO vence a aproximação da mata
	 * (os dois precisam caber na tela). Com 'extra' >= CAM_FRAME_FULL a
	 * densidade deixa de aproximar; as árvores que tapam ficam a 50%. */
	int keep = extra >= CAM_FRAME_FULL ? 0 : CAM_FRAME_FULL - extra;
	int target = CAM_DIST - (CAM_DIST - CAM_DENSE_DIST) * dens / CAM_DENSE_FULL * keep / CAM_FRAME_FULL;
	if (snap) {
		base_dist = target;
		return;
	}
	for (int t = 0; t < ticks; t++)          /* ~1/16 por passo: ~1 s de transição */
		base_dist += (target - base_dist + (target > base_dist ? 15 : -15)) / 16;
}

/* Todos os jogadores visíveis daquele ponto (só troncos em pé e o que é
 * mais alto que a câmera atrapalham)? */
static int view_clear(int ex, int ez) {
	cam_tests++;
	for (int i = 0; i < grp_n; i++)
		if (!collide_camera_clear(grp_x[i], grp_z[i], ex, ez))
			return 0;
	return 1;
}

/* Distância desejada sem tronco no caminho. Começa da do quadro anterior
 * (não do máximo): livre -> tenta afastar UM passo; bloqueada -> aproxima
 * até liberar. Assim são ~1-2 testes por quadro em vez de ~13. */
static int find_clear(int want_dist, int s, int c, VECTOR *want) {
	int d = clear_dist;
	if (d > want_dist) d = want_dist;
	if (d < CAM_MIN_DIST) d = CAM_MIN_DIST;
	eye_at(d, want_dist, s, c, want);
	if (view_clear(want->vx, want->vz)) {
		if (d < want_dist) {
			int up = d + CAM_STEP > want_dist ? want_dist : d + CAM_STEP;
			VECTOR t;
			eye_at(up, want_dist, s, c, &t);
			if (view_clear(t.vx, t.vz)) { d = up; *want = t; }
		}
	} else {
		while (d > CAM_MIN_DIST) {
			d -= CAM_STEP;
			if (d < CAM_MIN_DIST) d = CAM_MIN_DIST;
			eye_at(d, want_dist, s, c, want);
			if (view_clear(want->vx, want->vz)) break;
		}
	}
	clear_dist = d;
	return d;
}

static int want_distance(void) {
	return base_dist + extra;
}

/* Visual: uma vez por quadro; 'ticks' = passos de lógica deste quadro */
void camera_view_update(int ticks) {
	cam_tests_last = cam_tests;
	cam_cells_last = collide_seg_cells_take();
	cam_tests = 0;
	if (!collect_group())
		return;

	int s = isin(g.cam_yaw), c = icos(g.cam_yaw);
	VECTOR want;
	update_base(ticks, 0);
	find_clear(want_distance(), s, c, &want);

	for (int t = 0; t < ticks; t++) {      /* anda 1/4 do caminho por passo */
		eye.vx += (want.vx - eye.vx) >> 2;
		eye.vy += (want.vy - eye.vy) >> 2;
		eye.vz += (want.vz - eye.vz) >> 2;
	}

	/* árvores que tapam alguém ficam semitransparentes (~0,3 s de memória) */
	level_mark_occluders(eye.vx, eye.vz, grp_x, grp_z, grp_n, ticks);

	/* Garantia: o olho suavizado nunca fica com tronco no caminho. Se ficou
	 * (girou rápido, aproximou), puxa NA HORA para o ponto livre na mesma
	 * direção, sem esperar a suavização. Começa da distância livre atual,
	 * no máximo alguns passos. */
	if (!view_clear(eye.vx, eye.vz)) {
		int ex = eye.vx - cx, ez = eye.vz - cz;
		int len = dist2d(ex, ez);
		int k = len < clear_dist ? len : clear_dist;
		for (int n = 0; len > 0 && n < 16; n++, k -= CAM_STEP) {
			if (k < CAM_MIN_DIST) k = CAM_MIN_DIST;
			int px = cx + ex * k / len, pz = cz + ez * k / len;
			if (view_clear(px, pz) || k == CAM_MIN_DIST) {
				eye.vx = px;
				eye.vz = pz;
				break;
			}
		}
	}
}

/* Coloca a câmera direto no lugar (nascer, recomeçar, teleporte) */
void camera_snap(void) {
	if (!collect_group())
		return;
	int s = isin(g.cam_yaw), c = icos(g.cam_yaw);
	update_base(0, 1);
	clear_dist = CAM_MIN_DIST;           /* sobe de baixo até onde der livre */
	VECTOR want = { 0 };
	for (int k = 0; k < 16 && clear_dist < want_distance(); k++) {
		int before = clear_dist;
		find_clear(want_distance(), s, c, &want);
		if (clear_dist == before) break; /* não dá para afastar mais */
	}
	find_clear(want_distance(), s, c, &want);
	eye = want;
}

/* Custo do quadro anterior: testes de visão e células percorridas */
void camera_stats(int *tests, int *cells) {
	*tests = cam_tests_last;
	*cells = cam_cells_last;
}

void camera_apply(void) {
	render_set_camera(&eye, &look);
}

const VECTOR *camera_eye(void) {
	return &eye;
}
