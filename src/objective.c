/*
 * objective.c - Objetivo da fase, saída (X) e reforços (S)
 *
 * Cada fase (levels.c) escolhe um objetivo:
 *   OBJ_KILL_ALL    derrotar todos os inimigos
 *   OBJ_REACH_EXIT  chegar à saída (célula X do mapa)
 *   OBJ_COLLECT     juntar N itens de missão (Q); obj_param = N (0 = todos)
 *   OBJ_SURVIVE     sobreviver obj_param segundos; chegam reforços pelos S
 *
 * Regra da saída: se o mapa tem um X, cumprir o objetivo ABRE a saída e a
 * fase termina quando um jogador vivo chega nela. Sem X, a fase termina
 * assim que o objetivo é cumprido. (Em OBJ_REACH_EXIT a saída já começa
 * aberta, então o mapa precisa ter um X.)
 *
 * Tudo vale para o grupo: os itens contam para os dois jogadores, e basta
 * um deles chegar à saída.
 */
#include <stdio.h>
#include "game.h"
#include "models.h"

static const LEVEL_DEF *cur_def(void) {
	return &level_defs[g.level];
}

/* Chamado no fim do game_reset, depois de o mapa criar os objetos */
void objective_start(void) {
	const LEVEL_DEF *ld = cur_def();
	g.quest_need = ld->obj_param > 0 ? ld->obj_param : g.quest_total;
	g.survive_left = ld->objective == OBJ_SURVIVE ? ld->obj_param * 60 : 0;
	g.reinforce_timer = REINFORCE_TIME;
	g.exit_open = 0;
}

/* O objetivo em si (sem contar a saída) foi cumprido? */
static int goal_done(void) {
	switch (cur_def()->objective) {
	case OBJ_KILL_ALL:   return g.enemies_left == 0;
	case OBJ_REACH_EXIT: return 1;
	case OBJ_COLLECT:    return g.quest >= g.quest_need;
	case OBJ_SURVIVE:    return g.survive_left == 0;
	}
	return 0;
}

static int player_at_exit(void) {
	for (int i = 0; i < MAX_PLAYERS; i++) {
		const PLAYER *p = &g.players[i];
		if (player_alive(p) &&
		    dist2d(p->pos.vx - g.exit_x, p->pos.vz - g.exit_z) < EXIT_RADIUS)
			return 1;
	}
	return 0;
}

/* Um ponto S serve para reforço se estiver longe de todos os jogadores,
 * fora do que a câmera vê e sem inimigo em cima. */
static int spawn_ok(int x, int z) {
	for (int i = 0; i < MAX_PLAYERS; i++) {
		const PLAYER *p = &g.players[i];
		if (player_alive(p) && dist2d(p->pos.vx - x, p->pos.vz - z) < REINFORCE_MIN_DIST)
			return 0;
	}
	int dx = x - g.cam_pos.vx, dz = z - g.cam_pos.vz;
	int rel = angle_diff(g.cam_yaw, angle_of(dx, dz));
	if (rel > -700 && rel < 700 && dist2d(dx, dz) < render_fog_far())
		return 0;                                   /* a câmera veria nascer */
	for (int i = 0; i < MAX_ENEMIES; i++) {
		const ENEMY *e = &g.enemies[i];
		if (e->active && dist2d(e->pos.vx - x, e->pos.vz - z) < 200)
			return 0;
	}
	return 1;
}

/* SURVIVE: a cada REINFORCE_TIME passos chega um inimigo num S válido,
 * respeitando o limite de inimigos vivos da fase. Usa g.rng (é lógica). */
static void reinforcements(void) {
	if (--g.reinforce_timer > 0)
		return;
	g.reinforce_timer = REINFORCE_TIME;
	if (g.enemies_left >= cur_def()->max_enemies || g.num_spawn_pts == 0)
		return;

	int ok[MAX_SPAWN_PTS], n = 0;
	for (int i = 0; i < g.num_spawn_pts; i++)
		if (spawn_ok(g.spawn_pts[i].x, g.spawn_pts[i].z))
			ok[n++] = i;
	if (n == 0) {
		g.reinforce_timer = 60;                     /* tenta de novo em 1 s */
		return;
	}
	int s = ok[rng_range(&g.rng, 0, n - 1)];
	enemy_spawn(rng_range(&g.rng, 0, num_enemy_types - 1), g.spawn_pts[s].x, g.spawn_pts[s].z);
}

/* Um passo de lógica. Devolve 1 quando a fase foi vencida. */
int objective_update(void) {
	if (cur_def()->objective == OBJ_SURVIVE && g.survive_left > 0) {
		g.survive_left--;
		reinforcements();
	}

	int done = goal_done();
	if (!g.has_exit)
		return done;

	if (done && !g.exit_open) {
		g.exit_open = 1;
		if (cur_def()->objective != OBJ_REACH_EXIT) {
			show_message("SAIDA ABERTA!", 120);
			effect_spawn(FX_BURST, g.exit_x, g.exit_z, 400, 30, 120, 220, 255);
		}
	}
	/* "porta de luz": anéis subindo enquanto a saída está aberta */
	if (g.exit_open && (g.frame & 31) == 0)
		effect_spawn(FX_HEAL, g.exit_x, g.exit_z, 180, 40, 120, 220, 255);

	return g.exit_open && player_at_exit();
}

/* Linha do objetivo para o HUD (ASCII). Só lê g. */
const char *objective_text(void) {
	static char buf[24];
	const LEVEL_DEF *ld = cur_def();
	if (g.has_exit && g.exit_open)
		return "VA ATE A SAIDA";
	switch (ld->objective) {
	case OBJ_KILL_ALL:
		snprintf(buf, sizeof(buf), "INIMIGOS %d", g.enemies_left);
		return buf;
	case OBJ_COLLECT:
		snprintf(buf, sizeof(buf), "ITENS %d/%d", g.quest, g.quest_need);
		return buf;
	case OBJ_SURVIVE: {
		int s = (g.survive_left + 59) / 60;
		snprintf(buf, sizeof(buf), "SOBREVIVA %d:%02d", s / 60, s % 60);
		return buf;
	}
	}
	return "";
}

/* Saída: um pilar (marco de pedra) com um anel em cima. Fechada: anel
 * cinza e parado. Aberta: anel claro girando e pulsando. */
void exit_draw(void) {
	if (!g.has_exit)
		return;
	VECTOR pos = { g.exit_x, 0, g.exit_z };
	render_mesh(&pillar_mesh, &pos, NULL, ONE * 3 / 4, NULL);

	static const CVECTOR closed = { 90, 90, 100 };
	CVECTOR open;
	int pulse = isin((g.frame * 96) & 4095);           /* -4096..4096 */
	open.r = 100 + (pulse * 40 >> 12);
	open.g = 200 + (pulse * 40 >> 12);
	open.b = 255;

	DRAWOPT opt = { 0 };
	opt.npalette = 1;
	opt.flags = DRAW_UNLIT | DRAW_NOCULL;
	opt.palette = g.exit_open ? &open : &closed;
	VECTOR rp = { g.exit_x, -540, g.exit_z };      /* em cima do pilar */
	SVECTOR rot = { 1024, g.exit_open ? (g.frame * 64) & 4095 : 0, 0 };   /* anel em pé */
	render_mesh(&ring_mesh, &rp, &rot, ONE * 5 / 8, &opt);
}
