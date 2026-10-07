/*
 * main.c - PS1 Arena: laço principal, câmera, HUD e telas
 *
 * Objetivo da fase: derrotar todos os inimigos da arena.
 * Gemas dão pontos e energia; caixas podem esconder itens.
 *
 * Telas:  TÍTULO -> SELEÇÃO DE PERSONAGEM -> JOGO -> (PAUSA / FIM)
 * Até 2 jogadores (controles 1 e 2). Online: veja "Jogar online" no README
 * (netplay do RetroArch: o jogador remoto vira o controle 2).
 */
#include <stdio.h>
#include <string.h>
#include <psxapi.h>
#include <psxpad.h>
#include "game.h"
#include "models.h"

GAME    g;

static int show_debug = 0;
static int fps = 60, fps_frames = 0, fps_last = 0;

void show_message(const char *text, int frames) {
	strncpy(g.message, text, sizeof(g.message) - 1);
	g.message[sizeof(g.message) - 1] = 0;
	g.message_timer = frames;
}

/* ------------------------------------------------------------------ */
/* Seleção de personagem (sobrevive entre partidas)                     */
/* ------------------------------------------------------------------ */

typedef struct {
	int joined;      /* este controle está no jogo */
	int character;   /* índice em character_defs */
	int skin;
	int ready;       /* confirmou a escolha */
} SELECTION;

static SELECTION sel[MAX_PLAYERS] = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 } };

/* Algum controle apertou um destes botões? Devolve qual (ou -1). */
static int any_pressed(uint16_t mask) {
	for (int i = 0; i < MAX_PLAYERS; i++)
		if (g.in[i].pressed & mask)
			return i;
	return -1;
}

/* ------------------------------------------------------------------ */
/* Partida                                                             */
/* ------------------------------------------------------------------ */

static void camera_update(int snap);
static VECTOR cam_look;

/* Lugar livre ao lado de (x,z) para o segundo jogador nascer */
static void free_spot_near(int *x, int *z) {
	static const int off[8][2] = { {1,0},{0,1},{-1,0},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1} };
	for (int k = 0; k < 8; k++) {
		int tx = *x + off[k][0] * TILE_SIZE, tz = *z + off[k][1] * TILE_SIZE;
		if (!level_blocked(tx, tz, PLAYER_RADIUS) && !collide_prop_at(tx, tz, PLAYER_RADIUS)) {
			*x = tx; *z = tz;
			return;
		}
	}
}

/* Começa a fase 'level'. A semente vem como parâmetro porque o memset
 * abaixo apaga g inteiro (inclusive g.frame, de onde ela costuma vir). */
static void game_reset(int level, uint32_t seed) {
	INPUT in[MAX_PLAYERS];
	memcpy(in, g.in, sizeof(in));
	memset(&g, 0, sizeof(g));
	memcpy(g.in, in, sizeof(in));

#ifdef DEBUG_FIXED_SEED
	seed = DEBUG_FIXED_SEED;
#endif
	/* semear ANTES do level_load: os inimigos sorteiam ao nascer */
	g.seed = seed;
	rng_seed(&g.rng, seed);

	g.level = level;
	level_load(level);

	/* cria os jogadores escolhidos na tela de seleção */
	int x = g.spawn_x, z = g.spawn_z;
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (!sel[i].joined) continue;
		PLAYER *p = &g.players[i];
		p->index     = i;
		p->active    = 1;
		p->character = sel[i].character;
		p->skin      = sel[i].skin;
		if (g.num_players > 0)
			free_spot_near(&x, &z);
		player_spawn(p, x, z);
		g.num_players++;
	}

	/* câmera começa olhando do início para o centro da fase */
	g.cam_yaw = angle_of(level_width() * TILE_SIZE / 2 - g.spawn_x,
	                     level_height() * TILE_SIZE / 2 - g.spawn_z);
	for (int i = 0; i < MAX_PLAYERS; i++)
		g.players[i].angle = g.cam_yaw;
	camera_update(1);
	objective_start();
}

/* ------------------------------------------------------------------ */
/* Progressão entre fases                                              */
/* ------------------------------------------------------------------ */

/* O que passa de uma fase para a outra. Fica fora de g (como sel[]),
 * porque o memset do game_reset apaga g. É o "retrato" de quando a fase
 * começou: vencer atualiza o retrato; o game over recomeça a fase com ele.
 * Calculado só a partir de g, então é igual nos dois consoles (modo link). */
typedef struct {
	int      valid;
	int      hp, energy, weapon, power;
	uint32_t weapons_owned;
	int      battery, lantern_on;
} CARRY;

static struct {
	int   score;
	CARRY p[MAX_PLAYERS];
} progress;

/* Guarda o estado atual (chamado ao vencer uma fase) */
static void progress_save(void) {
	progress.score = g.score;
	for (int i = 0; i < MAX_PLAYERS; i++) {
		const PLAYER *p = &g.players[i];
		CARRY *c = &progress.p[i];
		c->valid = p->active;
		if (!p->active) continue;
		c->hp            = p->hp > 0 ? p->hp : p->max_hp / 2;  /* caído volta com 50% */
		c->energy        = p->energy;
		c->weapon        = p->weapon;
		c->power         = p->power;
		c->weapons_owned = p->weapons_owned;
		c->battery       = p->battery;
		c->lantern_on    = p->lantern_on && p->battery > 0;
	}
}

/* Começa a fase pela tela de introdução, com o progresso guardado */
static void start_level(int level, uint32_t seed) {
	game_reset(level, seed);
	g.score = progress.score;
	for (int i = 0; i < MAX_PLAYERS; i++) {
		PLAYER *p = &g.players[i];
		const CARRY *c = &progress.p[i];
		if (!p->active || !c->valid) continue;
		p->hp            = c->hp < p->max_hp ? c->hp : p->max_hp;
		p->energy        = c->energy;
		p->weapon        = c->weapon;
		p->power         = c->power;
		p->weapons_owned = c->weapons_owned;
		p->battery       = c->battery;
		p->lantern_on    = c->lantern_on;
	}
	g.state = STATE_INTRO;
	g.state_timer = INTRO_TIME;
}

/* Jogo novo (saindo da seleção): zera o progresso */
static void start_new_game(void) {
	int first = 0;
#ifdef DEBUG_START_LEVEL
	first = DEBUG_START_LEVEL - 1;            /* 1 = primeira fase */
	if (first < 0) first = 0;
	if (first >= num_levels) first = num_levels - 1;
#endif
	memset(&progress, 0, sizeof(progress));
	start_level(first, g.frame);              /* tempo na seleção varia: partida diferente */
}

/* ------------------------------------------------------------------ */
/* Câmera em terceira pessoa (segue o grupo)                            */
/* ------------------------------------------------------------------ */

static void camera_update(int snap) {
	if (g.state == STATE_PLAY) {
		for (int i = 0; i < MAX_PLAYERS; i++) {   /* qualquer jogador gira */
			if (!g.players[i].active) continue;
			if (g.in[i].held & PAD_L1) g.cam_yaw -= CAM_TURN_SPEED;
			if (g.in[i].held & PAD_R1) g.cam_yaw += CAM_TURN_SPEED;
			g.cam_yaw += g.in[i].rx / 3;
		}
		g.cam_yaw &= 4095;
	}

	/* centro dos jogadores vivos (se ninguém vivo, de todos) e o quanto
	 * estão afastados: a câmera recua para mostrar os dois */
	int n = 0, cx = 0, cy = 0, cz = 0;
	int minx = 0x7fffffff, maxx = -0x7fffffff, minz = 0x7fffffff, maxz = -0x7fffffff;
	int only_alive = players_alive() > 0;
	for (int i = 0; i < MAX_PLAYERS; i++) {
		PLAYER *p = &g.players[i];
		if (!p->active || (only_alive && !player_alive(p))) continue;
		cx += p->pos.vx; cy += p->pos.vy; cz += p->pos.vz; n++;
		if (p->pos.vx < minx) minx = p->pos.vx;
		if (p->pos.vx > maxx) maxx = p->pos.vx;
		if (p->pos.vz < minz) minz = p->pos.vz;
		if (p->pos.vz > maxz) maxz = p->pos.vz;
	}
	if (n == 0) return;
	cx /= n; cy /= n; cz /= n;
	int extra = (n > 1) ? dist2d(maxx - minx, maxz - minz) * 2 / 3 : 0;
	if (extra > 1400) extra = 1400;

	/* Recua a câmera atrás do grupo. Se ela cair dentro de uma parede,
	 * aproxima e SOBE (fica mais "de cima"), assim ninguém some atrás de
	 * uma parede. */
	int s = isin(g.cam_yaw), c = icos(g.cam_yaw);
	int want_dist = CAM_DIST + extra;
	VECTOR want;
	int dist;
	for (dist = want_dist; dist > CAM_MIN_DIST; dist -= 40) {
		want.vx = cx - ((s * dist) >> 12);
		want.vz = cz - ((c * dist) >> 12);
		if (!level_blocked(want.vx, want.vz, 64))
			break;
	}
	want.vy = (cy >> 1) - CAM_HEIGHT - extra * 3 / 4 - (want_dist - dist) * 3 / 4;

	if (snap) {
		g.cam_pos = want;
	} else {   /* suaviza o movimento: anda 1/4 do caminho por quadro */
		g.cam_pos.vx += (want.vx - g.cam_pos.vx) >> 2;
		g.cam_pos.vy += (want.vy - g.cam_pos.vy) >> 2;
		g.cam_pos.vz += (want.vz - g.cam_pos.vz) >> 2;
	}

	cam_look.vx = cx;
	cam_look.vy = (cy >> 1) - CAM_LOOK_UP;
	cam_look.vz = cz;
}

/* Envia a câmera calculada para o renderizador (uma vez por quadro) */
static void camera_apply(void) {
	render_set_camera(&g.cam_pos, &cam_look);
}

/* Na tela de título a câmera gira em volta da arena */
static void camera_title(void) {
	int cx = level_width() * TILE_SIZE / 2, cz = level_height() * TILE_SIZE / 2;
	int a = (g.frame * 6) & 4095;
	VECTOR eye  = { cx + ((isin(a) * 2600) >> 12), -1900, cz + ((icos(a) * 2600) >> 12) };
	VECTOR look = { cx, 0, cz };
	render_set_camera(&eye, &look);
}

/* ------------------------------------------------------------------ */
/* HUD                                                                 */
/* ------------------------------------------------------------------ */

static void draw_bar(int x, int y, int w, int value, int max, int r, int gg, int b) {
	if (value < 0) value = 0;
	int fill = (w * value) / max;
	render_rect(x, y, fill, 6, r, gg, b, 0);        /* frente (é desenhada depois) */
	render_rect(x - 1, y - 1, w + 2, 8, 0, 0, 0, 1); /* fundo semitransparente */
}

static void draw_center(int y, const char *text) {
	hud_print(160 - (int)strlen(text) * 4, y, "%s", text);
}

/* Bloco de vida/energia/arma de um jogador, a partir de (x, y) */
static void draw_player_hud(const PLAYER *p, int x) {
	if (g.num_players > 1)
		hud_print(x, 2, "P%d %s", p->index + 1, character_defs[p->character].name);
	if (!player_alive(p)) {
		if (p->respawn_timer > 0)
			hud_print(x, 18, "CAIDO - VOLTA EM %d", p->respawn_timer / 60 + 1);
		else
			hud_print(x, 18, "CAIDO");
		return;
	}
	hud_print(x, 12, "HP");
	draw_bar(x + 20, 13, 90, p->hp, p->max_hp, 230, 50, 60);
	hud_print(x, 24, "EN");
	draw_bar(x + 20, 25, 90, p->energy, PLAYER_MAX_ENERGY, 60, 150, 255);
	/* bateria da lanterna: amarela; vermelha quando fraca; apagada = cinza */
	hud_print(x, 36, "LT");
	if (!p->lantern_on)
		draw_bar(x + 20, 37, 90, p->battery, BATTERY_MAX, 110, 110, 110);
	else if (p->battery < BATTERY_LOW)
		draw_bar(x + 20, 37, 90, p->battery, BATTERY_MAX, 230, 60, 40);
	else
		draw_bar(x + 20, 37, 90, p->battery, BATTERY_MAX, 240, 210, 80);
	hud_print(x, 48, "%s", weapon_defs[p->weapon].name);
	if (g.num_players > 1)   /* com 2 jogadores falta espaço: sem o custo */
		hud_print(x, 58, "%s", power_defs[p->power].name);
	else
		hud_print(x, 58, "%s (%d)", power_defs[p->power].name, power_defs[p->power].cost);
}

static void draw_hud(void) {
	int col = 0;
	for (int i = 0; i < MAX_PLAYERS; i++) {
		if (!g.players[i].active) continue;
		draw_player_hud(&g.players[i], col == 0 ? 12 : 196);
		col++;
	}

	hud_print(12, 224, "PONTOS %05d  GEMAS %d/%d  %s",
	          g.score, g.gems, g.gems_total, objective_text());

	if (g.message_timer > 0)
		draw_center(70, g.message);

	if (show_debug) {
		const PLAYER *p = &g.players[0];
#ifdef DEBUG_FOG_TUNING
		hud_print(12, 166, "FOG %d/%d  R2+DIR", render_fog_near(), render_fog_far());
#else
		hud_print(12, 166, "FOG %d/%d", render_fog_near(), render_fog_far());
#endif
		hud_print(12, 176, "SPU %dK/512K", sound_spu_used() / 1024);
		hud_print(12, 186, "SEED %08X  POLIS %d", (unsigned)g.seed, render_stats_polys());
		hud_print(12, 196, "FPS %d  RAM GPU %d/%d", fps, render_stats_bytes(), PACKET_LEN);
		hud_print(12, 206, "X %d Z %d ANG %d CAM %d", p->pos.vx, p->pos.vz, p->angle, g.cam_yaw);
	}
}

/* ------------------------------------------------------------------ */
/* Tela de seleção de personagem                                        */
/* ------------------------------------------------------------------ */

/* Os modelos são mostrados num "palco" longe da fase (fora da visão dela) */
#define STAGE_X  (-9000)
#define STAGE_Z  (-9000)

static int joined_count(void) {
	int n = 0;
	for (int i = 0; i < MAX_PLAYERS; i++) n += sel[i].joined;
	return n;
}

static void select_enter(void) {
	for (int i = 0; i < MAX_PLAYERS; i++)
		sel[i].ready = 0;
	g.state = STATE_SELECT;
	/* vitrine dos personagens: sem névoa e com luz clara */
	render_set_fog(MENU_FOG_NEAR, MENU_FOG_FAR, CLEAR_R, CLEAR_G, CLEAR_B);
	render_set_light(72, 72, 88, ONE, ONE, ONE * 9 / 10);
}

static void select_tick(void) {
	for (int i = 0; i < MAX_PLAYERS; i++) {
		SELECTION *s  = &sel[i];
		INPUT     *in = &g.in[i];

		if (!s->joined) {                 /* jogador 2 entra apertando START */
			if (in->pressed & (PAD_START | PAD_CROSS)) {
				s->joined = 1;
				s->ready  = 0;
			}
			continue;
		}

		if (!s->ready) {
			int n = num_characters;
			if (in->pressed & PAD_LEFT)  s->character = (s->character + n - 1) % n;
			if (in->pressed & PAD_RIGHT) s->character = (s->character + 1) % n;
			if ((in->pressed & PAD_SELECT) && character_defs[s->character].use_skins)
				s->skin = (s->skin + 1) % num_skins;
			if (in->pressed & (PAD_CROSS | PAD_START))
				s->ready = 1;
			if (in->pressed & PAD_CIRCLE) {
				if (i == 0) {                     /* P1 volta ao título */
					g.state = STATE_TITLE;
					level_apply_look();           /* o título mostra a fase com névoa */
					return;
				}
				s->joined = 0;                                     /* P2 sai */
			}
		} else if (in->pressed & PAD_CIRCLE) {
			s->ready = 0;                     /* desfaz a confirmação */
		}
	}

	/* todos que entraram confirmaram? começa! */
	int all = 1;
	for (int i = 0; i < MAX_PLAYERS; i++)
		if (sel[i].joined && !sel[i].ready) all = 0;
	if (all && sel[0].joined) {
		start_new_game();
	}
}

static void select_draw(void) {
	static const CVECTOR ring_color[MAX_PLAYERS] = { { 80, 160, 255 }, { 255, 90, 90 } };
	VECTOR eye  = { STAGE_X, -330, STAGE_Z - 1150 };
	VECTOR look = { STAGE_X, -170, STAGE_Z };
	render_set_camera(&eye, &look);

	int nj = joined_count();
	int slot = 0;

	draw_center(12, "ESCOLHA SEU PERSONAGEM");

	for (int i = 0; i < MAX_PLAYERS; i++) {
		SELECTION *s = &sel[i];
		if (!s->joined) continue;
		const CHARACTER_DEF *c = &character_defs[s->character];

		int sx = (nj == 1) ? 160 : (slot == 0 ? 80 : 240);   /* centro na tela */
		int wx = (sx - 160) * 1150 / FOV_H;                    /* mesmo ponto no mundo */
		slot++;

		/* modelo girando sobre um anel colorido */
		VECTOR  pos = { STAGE_X + wx, 0, STAGE_Z };
		SVECTOR rot = { 0, (2048 + g.frame * 14) & 4095, 0 };
		if (s->ready)
			pos.vy = -((isin((g.frame * 200) & 4095) < 0 ? -isin((g.frame * 200) & 4095)
			                                              :  isin((g.frame * 200) & 4095)) >> 7);
		player_draw_model(&pos, &rot, s->character, s->skin);

		DRAWOPT ro = { 0 };
		ro.flags = DRAW_UNLIT | DRAW_SEMITRANS | DRAW_NOCULL;
		ro.palette = &ring_color[i];
		ro.npalette = 1;
		VECTOR rp = { pos.vx, -4, pos.vz };
		render_mesh(&ring_mesh, &rp, NULL, ONE * 3 / 4, &ro);

		/* textos e atributos */
		char line[40];
		if (nj > 1) {
			snprintf(line, sizeof(line), "JOGADOR %d", i + 1);
			hud_print(sx - (int)strlen(line) * 4, 30, "%s", line);
		}
		snprintf(line, sizeof(line), s->ready ? "%s" : "< %s >", c->name);
		hud_print(sx - (int)strlen(line) * 4, 150, "%s", line);

		int bx = sx - 52;
		hud_print(bx, 164, "VIDA");  draw_bar(bx + 44, 165, 60, c->max_hp, 200, 230, 50, 60);
		hud_print(bx, 174, "VEL");   draw_bar(bx + 44, 175, 60, c->speed, 24, 80, 220, 120);
		hud_print(bx, 184, "PULO");  draw_bar(bx + 44, 185, 60, c->jump, 50, 240, 200, 60);

		snprintf(line, sizeof(line), "%s", weapon_defs[c->weapon].name);
		hud_print(sx - (int)strlen(line) * 4, 196, "%s", line);

		if (nj == 1)
			draw_center(208, c->desc);
		if (c->use_skins && !s->ready) {
			snprintf(line, sizeof(line), "SELECT: %s", skin_defs[s->skin].name);
			hud_print(sx - (int)strlen(line) * 4, nj == 1 ? 46 : 42, "%s", line);
		}
		if (s->ready && ((g.frame >> 4) & 1)) {
			hud_print(sx - 28, 136, "PRONTO!");
		}
	}

	if (!sel[1].joined)
		draw_center(222, g.in[1].connected ? "CONTROLE 2: START PARA ENTRAR"
		                                   : "LIGUE O CONTROLE 2 PARA 2 JOGADORES");
	else
		draw_center(222, "ESQ/DIR ESCOLHE  X CONFIRMA  O VOLTA");
}

/* ------------------------------------------------------------------ */
/* Mundo                                                               */
/* ------------------------------------------------------------------ */

/* Lanternas acesas neste quadro: avisa o motor (objetos no cone são vistos
 * mais longe) e desenha o cone de luz no chão de cada uma. */
static void lanterns_draw(int with_players) {
	VECTOR pos[MAX_PLAYERS] = { { 0 } };
	int    ang[MAX_PLAYERS] = { 0 }, n = 0;
	for (int i = 0; with_players && i < MAX_PLAYERS; i++) {
		const PLAYER *p = &g.players[i];
		if (!lantern_lit(p)) continue;
		pos[n] = p->pos;
		ang[n] = p->angle;
		n++;
	}
	render_set_lanterns(n, pos, ang);
	for (int i = 0; i < n; i++)
		render_light_cone(pos[i].vx, pos[i].vz, ang[i], LANTERN_R, LANTERN_G, LANTERN_B);
}

static void draw_world(int with_players) {
	lanterns_draw(with_players);   /* antes dos modelos: eles consultam as lanternas */
	level_draw();
	props_draw();
	exit_draw();
	crates_draw();
	pickups_draw();
	enemies_draw();
	bullets_draw();
	effects_draw();
	if (with_players)
		for (int i = 0; i < MAX_PLAYERS; i++)
			player_draw(&g.players[i]);
}

/* ------------------------------------------------------------------ */

/* Lógica de UM passo (1/60 de segundo). Não desenha nada. */
static void game_tick(void) {
	int who;
	switch (g.state) {
	case STATE_TITLE:
		effects_update();
		who = any_pressed(PAD_START | PAD_CROSS);
		if (who >= 0) {
			sel[0].joined = 1;
			if (who == 1) sel[1].joined = 1;   /* apertou no controle 2: entra também */
			select_enter();
		}
		break;

	case STATE_SELECT:
		select_tick();
		break;

	case STATE_PLAY:
		if (any_pressed(PAD_START) >= 0) {
			g.state = STATE_PAUSE;
			break;
		}
		for (int i = 0; i < MAX_PLAYERS; i++)
			player_update(&g.players[i], &g.in[i]);
		enemies_update();
		bullets_update();
		pickups_update();
		effects_update();
		camera_update(0);
		g.play_frames++;
		if (objective_update() && g.state == STATE_PLAY) {
			g.state = STATE_WIN;
			for (int i = 0; i < MAX_PLAYERS; i++)
				if (player_alive(&g.players[i]))
					g.score += g.players[i].hp * 10;
			g.score += g.gems * 100;
		}
		break;

	case STATE_PAUSE:
		if (any_pressed(PAD_START) >= 0)
			g.state = STATE_PLAY;
		break;

	case STATE_INTRO:                             /* nome da fase e objetivo */
		effects_update();
		camera_update(0);
		if (--g.state_timer <= 0 || any_pressed(PAD_START | PAD_CROSS) >= 0)
			g.state = STATE_PLAY;
		break;

	case STATE_WIN:
	case STATE_DEAD:
		enemies_update();
		effects_update();
		camera_update(0);
		if (any_pressed(PAD_START) >= 0) {
			if (g.state == STATE_DEAD) {              /* recomeça a fase (semente nova) */
				start_level(g.level, g.frame);
			} else if (g.level + 1 < num_levels) {    /* próxima fase, levando tudo */
				progress_save();
				start_level(g.level + 1, g.frame);
			} else {                                  /* era a última */
				progress_save();
				g.state = STATE_END;
			}
		} else if (any_pressed(PAD_SELECT) >= 0) { /* trocar de personagem */
			select_enter();
		}
		break;

	case STATE_END:
		effects_update();
		camera_update(0);
		if (any_pressed(PAD_START | PAD_CROSS) >= 0) {
			game_reset(0, 12345);                 /* fundo do título, como no boot */
			g.state = STATE_TITLE;
		}
		break;
	}

	if (g.message_timer > 0)
		g.message_timer--;
	g.frame++;
}

/* Desenha o quadro atual */
static void game_draw(void) {
	switch (g.state) {
	case STATE_TITLE:
		camera_title();
		draw_world(0);
		draw_center(60,  "P S 1   A R E N A");
		draw_center(76,  "projeto base PSn00bSDK");
		if ((g.frame >> 5) & 1)
			draw_center(150, "APERTE START");
		draw_center(200, "1 OU 2 JOGADORES");
		break;

	case STATE_SELECT:
		select_draw();
		break;

	case STATE_PLAY:
		camera_apply();
		draw_world(1);
		draw_hud();
		break;

	case STATE_PAUSE:
		camera_apply();
		draw_world(1);
		draw_hud();
		draw_center(70,  "PAUSA");
		draw_center(96,  "X pular   QUADRADO atirar");
		draw_center(108, "TRIANGULO arma  CIRCULO poder");
		draw_center(120, "R2 troca poder  L1/R1 camera");
		draw_center(132, "SELECT lanterna  L2 debug");
		break;

	case STATE_INTRO: {
		const LEVEL_DEF *ld = &level_defs[g.level];
		camera_apply();
		draw_world(1);
		render_rect(0, 64, SCREEN_W, 64, 0, 0, 0, 1);   /* faixa escura atrás do texto */
		hud_print(124, 72, "FASE %d/%d", g.level + 1, num_levels);
		draw_center(88, ld->name);
		draw_center(108, ld->obj_text);
		break;
	}

	case STATE_WIN:
	case STATE_DEAD:
		camera_apply();
		draw_world(g.state == STATE_WIN);
		draw_hud();
		if (g.state == STATE_WIN) {
			draw_center(90, "FASE COMPLETA!");
			hud_print(104, 110, "TEMPO  %02d:%02d", g.play_frames / 3600, (g.play_frames / 60) % 60);
			hud_print(104, 122, "PONTOS %05d", g.score);
		} else {
			draw_center(100, "GAME OVER");
		}
		if ((g.frame >> 5) & 1) {
			if (g.state == STATE_DEAD)
				draw_center(150, "START tentar de novo");
			else if (g.level + 1 < num_levels)
				draw_center(150, "START proxima fase");
			else
				draw_center(150, "START continuar");
		}
		draw_center(164, "SELECT trocar personagem");
		break;

	case STATE_END:
		camera_apply();
		draw_world(1);
		render_rect(0, 70, SCREEN_W, 80, 0, 0, 0, 1);
		draw_center(80, "FIM DE JOGO");
		draw_center(96, "TODAS AS FASES VENCIDAS!");
		hud_print(84, 116, "PONTOS TOTAIS %05d", g.score);   /* 19 letras x 8 px, centrado */
		if ((g.frame >> 5) & 1)
			draw_center(136, "START voltar ao titulo");
		break;
	}
}

/* ------------------------------------------------------------------ */
/* Depuração (controle 1)                                              */
/* ------------------------------------------------------------------ */

/* Pula para a próxima fase levando o progresso, como se tivesse vencido */
static void debug_skip_level(void) {
	if (g.state != STATE_PLAY && g.state != STATE_PAUSE && g.state != STATE_INTRO &&
	    g.state != STATE_WIN && g.state != STATE_DEAD)
		return;
	progress_save();
	if (g.level + 1 < num_levels)
		start_level(g.level + 1, g.frame);
	else
		g.state = STATE_END;
}

/* L2 (solto sozinho) liga/desliga o overlay. Com o overlay aberto:
 *   L2 segurado + START  -> pula de fase
 *   R2 segurado + direcional (só com DEBUG_FOG_TUNING) -> ajusta a névoa:
 *       cima/baixo = far +-100, direita/esquerda = near +-50
 * Os botões usados aqui são "comidos" para não pausar nem mover o jogador. */
static void debug_input(INPUT *in) {
	static int l2_was_held = 0, l2_combo = 0;
	int l2 = in->held & PAD_L2;

	if (l2 && show_debug && (in->pressed & PAD_START)) {
		in->pressed &= ~PAD_START;
		l2_combo = 1;
		debug_skip_level();
	}
	if (!l2 && l2_was_held) {          /* soltou o L2 */
		if (!l2_combo)
			show_debug ^= 1;
		l2_combo = 0;
	}
	l2_was_held = l2;

#ifdef DEBUG_FOG_TUNING
	if (show_debug && (in->held & PAD_R2)) {
		int near = render_fog_near(), far = render_fog_far();
		if (in->pressed & PAD_UP)    far  += 100;
		if (in->pressed & PAD_DOWN)  far  -= 100;
		if (in->pressed & PAD_RIGHT) near += 50;
		if (in->pressed & PAD_LEFT)  near -= 50;
		if (in->pressed & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT)) {
			const LEVEL_DEF *ld = &level_defs[g.level];
			render_set_fog(near, far, ld->sky_r, ld->sky_g, ld->sky_b);
		}
		in->held    &= ~(PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT | PAD_R2);
		in->pressed &= ~(PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT | PAD_R2);
	}
#endif
}

/* Loops de ambiente (vozes 0 e 1): tocam enquanto há uma partida na tela
 * (jogo, pausa, vitória, derrota) e param no título e na seleção. */
static void ambience_update(void) {
	static int playing = 0;
	int want = g.state != STATE_TITLE && g.state != STATE_SELECT;
	if (want == playing)
		return;
	playing = want;
	if (want) {
		sound_loop_start(0, &sfx_vento, VOL_VENTO);
		sound_loop_start(1, &sfx_grilos, VOL_GRILOS);
	} else {
		sound_loop_stop(0);
		sound_loop_stop(1);
	}
}

int main(void) {
	render_init();
	input_init();

	assets_load_textures();     /* todas as texturas de assets/ (gerado) */
	sound_init();
	assets_load_sounds();       /* todos os sons de assets/sounds/ para o SPU (gerado) */
	sound_setup();              /* prioridades e variações (data.c) */

	game_reset(0, 12345);       /* fundo da tela de título: sempre igual */
	g.state = STATE_TITLE;

	int last_vsync = VSync(-1);

	while (1) {
		/* PASSO FIXO: a lógica sempre roda 60 vezes por segundo, mesmo que
		 * o desenho caia para 30 ou 20 quadros. Assim o jogo não fica
		 * "em câmera lenta" quando a cena é pesada. */
		int now   = VSync(-1);
		int ticks = now - last_vsync;
		last_vsync = now;
		if (ticks < 1) ticks = 1;
		if (ticks > 4) ticks = 4;

		input_update(g.in);
		debug_input(&g.in[0]);         /* L2: overlay e atalhos (controle 1) */

		for (int t = 0; t < ticks; t++) {
			game_tick();
			for (int i = 0; i < MAX_PLAYERS; i++)
				g.in[i].pressed = 0;    /* "apertou agora" vale só 1 passo */
		}

		ambience_update();
		game_draw();
		render_end_frame();

		/* medidor de FPS (VSync(-1) conta os retraços desde o boot) */
		fps_frames++;
		if (now - fps_last >= 60) {
			fps = fps_frames * 60 / (now - fps_last);
			fps_frames = 0;
			fps_last = now;
		}
	}
	return 0;
}
