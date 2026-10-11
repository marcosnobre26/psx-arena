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
#include <hwregs_c.h>
#include "game.h"
#include "models.h"

GAME    g;

static int show_debug = 0;
static int show_collision = 0;   /* depuração: marcadores nas células sólidas */
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


/* Lugar livre ao lado de (x,z) para o segundo jogador nascer */
static void free_spot_near(int *x, int *z) {
	static const int off[8][2] = { {1,0},{0,1},{-1,0},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1} };
	for (int k = 0; k < 8; k++) {
		int tx = *x + off[k][0] * TILE_SIZE, tz = *z + off[k][1] * TILE_SIZE;
		if (!collide_solid_at(tx, tz, PLAYER_RADIUS)) {
			*x = tx; *z = tz;
			return;
		}
	}
}

/* Começa a fase 'level'. A semente vem como parâmetro porque o memset
 * abaixo apaga g inteiro (inclusive g.frame, de onde ela costuma vir). */
#ifdef DEBUG_BENCHMARK
static int bench_prof_players, bench_prof_enemies;   /* lógica do quadro (linhas) */
#endif

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
	camera_snap();                 /* câmera visual direto no lugar */
	objective_start();
	events_start();
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
	int      hp, energy, weapon, arrows, bullets;
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
		c->arrows        = p->arrows;
		c->bullets       = p->bullets;
		c->weapons_owned = p->weapons_owned;
		c->battery       = p->battery;
		c->lantern_on    = p->lantern_on && p->battery > 0;
	}
}

static void draw_center(int y, const char *text);

/* Um quadro "CARREGANDO..." antes de montar a fase. A geração leva até
 * ~0,8 s no console (fase 2, 128x128); sem isto a tela ficaria parada no
 * último quadro. É só desenho: não muda g, então não afeta o determinismo.
 * A GPU mostra este quadro enquanto a CPU gera o mapa. */
static void draw_loading(void) {
	render_rect(0, 0, SCREEN_W, SCREEN_H, 0, 0, 0, 0);
	draw_center(112, "CARREGANDO...");
	render_end_frame();
}

/* Começa a fase pela tela de introdução, com o progresso guardado */
static void start_level(int level, uint32_t seed) {
	draw_loading();
	game_reset(level, seed);
	g.score = progress.score;
	for (int i = 0; i < MAX_PLAYERS; i++) {
		PLAYER *p = &g.players[i];
		const CARRY *c = &progress.p[i];
		if (!p->active || !c->valid) continue;
		p->hp            = c->hp < p->max_hp ? c->hp : p->max_hp;
		p->energy        = c->energy;
		p->weapon        = c->weapon;
		p->arrows        = c->arrows;
		p->bullets       = c->bullets;
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
	if (first >= num_level_defs) first = num_level_defs - 1;   /* 5 = fase de teste */
#endif
	memset(&progress, 0, sizeof(progress));
	start_level(first, g.frame);              /* tempo na seleção varia: partida diferente */
}

/* ------------------------------------------------------------------ */
/* Câmera em terceira pessoa (segue o grupo)                            */
/* ------------------------------------------------------------------ */

/* Na tela de título a câmera gira em volta da arena */
static void camera_title(void) {
	int cx = level_width() * TILE_SIZE / 2, cz = level_height() * TILE_SIZE / 2;
	int a = (g.frame * 6) & 4095;
	/* baixa e perto: alta (1900, raio 2600) virava vista de cima da floresta,
	 * enxergando chão demais (lento) */
	VECTOR eye  = { cx + ((isin(a) * TITLE_CAM_DIST) >> 12), -TITLE_CAM_HEIGHT,
	                cz + ((icos(a) * TITLE_CAM_DIST) >> 12) };
	VECTOR look = { cx, -120, cz };
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
	hud_print(x, 24, "MP");                   /* mana (magias) */
	draw_bar(x + 20, 25, 90, p->energy, PLAYER_MAX_ENERGY, 60, 150, 255);
	/* bateria da lanterna: amarela; vermelha quando fraca; apagada = cinza */
	hud_print(x, 36, "LT");
	if (!p->lantern_on)
		draw_bar(x + 20, 37, 90, p->battery, BATTERY_MAX, 110, 110, 110);
	else if (p->battery < BATTERY_LOW)
		draw_bar(x + 20, 37, 90, p->battery, BATTERY_MAX, 230, 60, 40);
	else
		draw_bar(x + 20, 37, 90, p->battery, BATTERY_MAX, 240, 210, 80);
	/* arma + o que ela gasta (corpo a corpo: nada) */
	const WEAPON_DEF *w = &weapon_defs[p->weapon];
	switch (w->ammo) {
	case AMMO_ARROW:  hud_print(x, 48, "%s %d", w->name, p->arrows); break;
	case AMMO_BULLET: hud_print(x, 48, "%s %d", w->name, p->bullets); break;
	case AMMO_MANA:   hud_print(x, 48, "%s (%d)", w->name, w->cost); break;
	default:          hud_print(x, 48, "%s", w->name); break;
	}
	if (p->charge > 0)                          /* carga do arco */
		draw_bar(x, 58, 60, p->charge, BOW_CHARGE, 220, 190, 120);
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
	if (g.ev.keys > 0)
		hud_print(236, 212, "CHAVE x%d", g.ev.keys);

	if (g.message_timer > 0)
		draw_center(70, g.message);

	if (show_debug) {
		const PLAYER *p = &g.players[0];
#ifdef DEBUG_FOG_TUNING
		hud_print(12, 166, "FOG %d/%d  R2+DIR", render_fog_near(), render_fog_far());
#else
		hud_print(12, 166, "FOG %d/%d", render_fog_near(), render_fog_far());
#endif
		int bl, bs, bb, bo;
		level_stats(&bl, &bs, &bb, &bo);
		hud_print(12, 156, "BLOCOS %d/%d +%d%s", bl, bs, bb, bo ? " CHEIO!" : "");
		int tt, tm, tb;
		level_tree_stats(&tt, &tm, &tb);
		if (tt)
			hud_print(12, 146, "ARV %d MOD %d PLANA %d TRANSP %d", tt, tm, tb, level_tree_transparent());
		int ct, cc;
		camera_stats(&ct, &cc);
		hud_print(12, 176, "SPU %dK  COL %d  CAM %d/%d", sound_spu_used() / 1024, collide_stats(), ct, cc);
		int pc, pg, pv;
		render_stats_time(&pc, &pg, &pv);
		hud_print(12, 136, "CPU %d GPU %d FOLGA %d (linhas)", pc, pg, pv);
		hud_print(12, 116, "EVENTO %s  PROX %ds  MAPA %ds", event_name(g.ev.last),
		          g.ev.next_random > 0 ? g.ev.next_random / 60 : 0, g.ev.map_cooldown / 60);
		/* mapa: semente usada pelo gerador (SEED + fase + tentativa), quantas
		 * tentativas e o tempo de carga; PLANO B = floresta fixa da 04e */
		const MAPGEN_STATS *ms = mapgen_stats();
		int load_ms = level_load_lines() * 635 / 10000;   /* 1 linha = 63,5 us */
		if (ms->fallback)
			hud_print(12, 126, "MAPA PLANO B (FIXO)  CARGA %dMS", load_ms);
		else if (mapgen_active())
			hud_print(12, 126, "MAPA %08X T%d  CARGA %dMS", (unsigned)ms->seed, ms->tries, load_ms);
		else
			hud_print(12, 126, "MAPA FIXO  CARGA %dMS", load_ms);
		hud_print(12, 186, "SEED %08X  POLIS %d", (unsigned)g.seed, render_stats_polys());
		if (level_cut_count() && ((g.frame >> 3) & 1))   /* pisca: buffer no limite */
			hud_print(12, 196, "FPS %d RAM GPU %d CORTE %d!", fps, render_stats_bytes(), level_cut_count());
		else
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

/* Depuração (L2 + SELECT com o overlay aberto): mostra a colisão em volta do
 * jogador 1. Se o desenho e a colisão baterem, cada parede/mata tem um
 * quadrado vermelho exatamente no topo e cada tronco tem um quadrado laranja
 * do tamanho do seu círculo de colisão.
 *   vermelho = parede/mata (célula sólida)      roxo = vazio (sem chão)
 *   laranja  = caixa (célula) ou tronco em pé (círculo, tamanho = raio)
 *   amarelo  = tronco caído (célula BAIXA: pula-se por cima, a visão passa)
 *   em cima de cada inimigo perto: VERDE = o jogador 1 o enxerga (linha de
 *   visão livre), VERMELHO = algo bloqueia a visão */
static void collision_markers_draw(void) {
	const PLAYER *p = &g.players[0];
	if (!p->active) return;
	int pcx = p->pos.vx / TILE_SIZE, pcz = p->pos.vz / TILE_SIZE;
	for (int cz = pcz - COLMARK_RADIUS; cz <= pcz + COLMARK_RADIUS; cz++)
		for (int cx = pcx - COLMARK_RADIUS; cx <= pcx + COLMARK_RADIUS; cx++) {
			if (cx < 0 || cz < 0 || cx >= level_width() || cz >= level_height())
				continue;
			int ox, oz, orad;
			for (int k = 0; collide_cell_obstacle(cx, cz, k, &ox, &oz, &orad); k++)
				render_marker(ox, -300, oz, orad, 255, 140, 0);       /* tronco: círculo */
			if (!level_cell_solid(cx, cz))
				continue;
			int x = cx * TILE_SIZE + TILE_SIZE / 2, z = cz * TILE_SIZE + TILE_SIZE / 2;
			int top = level_cell_top(cx, cz), t = level_cell_type(cx, cz);
			if (top > 0)
				render_marker(x, -top - 6, z, 80, 230, 30, 30);
			else if (t == CELL_VOID)
				render_marker(x, -6, z, 80, 150, 40, 200);
			else if (level_cell_low(cx, cz))
				render_marker(x, -130, z, 80, 240, 220, 40);           /* tronco caído */
			else
				render_marker(x, -280, z, 80, 255, 140, 0);            /* caixa */
		}
	/* linha de visão do jogador 1 até cada inimigo perto */
	for (int i = 0; i < MAX_ENEMIES; i++) {
		const ENEMY *e = &g.enemies[i];
		if (!e->active) continue;
		if (dist2d(e->pos.vx - p->pos.vx, e->pos.vz - p->pos.vz) > 12 * TILE_SIZE) continue;
		int see = collide_line_of_sight(p->pos.vx, p->pos.vz, e->pos.vx, e->pos.vz);
		render_marker(e->pos.vx, -420, e->pos.vz, 50, see ? 40 : 240, see ? 230 : 30, 40);
	}
}

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
	prof_begin(PROF_OTHER);
	events_draw();                 /* árvore caindo (etapa 06) */
	props_draw();
	exit_draw();
	crates_draw();
	pickups_draw();
	ground_arrows_draw();
	prof_begin(PROF_ENEMIES);            /* (dentro do OTHER: o bench subtrai) */
	enemies_draw();
	prof_end(PROF_ENEMIES);
	bullets_draw();
	effects_draw();
	if (with_players)
		for (int i = 0; i < MAX_PLAYERS; i++)
			player_draw(&g.players[i]);
	prof_end(PROF_OTHER);
	if (show_debug && show_collision)
		collision_markers_draw();
}

/* ------------------------------------------------------------------ */

/* Lógica de UM passo (1/60 de segundo). Não desenha nada. */
static void game_tick(void) {
	int who;
	collide_tick_begin();
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
#ifdef DEBUG_BENCHMARK
		{
			uint16_t t0 = TIMER_VALUE(1);
			for (int i = 0; i < MAX_PLAYERS; i++)
				player_update(&g.players[i], &g.in[i]);
			uint16_t t1 = TIMER_VALUE(1);
			enemies_update();
			uint16_t t2 = TIMER_VALUE(1);
			bench_prof_players += (uint16_t)(t1 - t0);
			bench_prof_enemies += (uint16_t)(t2 - t1);
		}
#else
		for (int i = 0; i < MAX_PLAYERS; i++)
			player_update(&g.players[i], &g.in[i]);
		enemies_update();
#endif
		bullets_update();
		pickups_update();
		ground_arrows_update();
		effects_update();
		events_update();                 /* a floresta muda (etapa 06) */
		camera_turn();                   /* lógica: só o ângulo (g.cam_yaw) */
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
		if (--g.state_timer <= 0 || any_pressed(PAD_START | PAD_CROSS) >= 0)
			g.state = STATE_PLAY;
		break;

	case STATE_WIN:
	case STATE_DEAD:
		enemies_update();
		effects_update();
		if (any_pressed(PAD_START) >= 0) {
			if (g.state == STATE_DEAD) {              /* recomeça a fase: MESMO mapa */
				start_level(g.level, g.seed);     /* mesma semente = mesma floresta */
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
	render_set_shake(0, 0);              /* camera_apply liga se houver tremor */
#ifndef DEBUG_FOG_TUNING
	if (g.ev.fog_far > 0 && render_fog_far() != g.ev.fog_far)
		level_apply_fog();               /* névoa do evento (lê g) */
#endif
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
		if (g.level < num_levels)
			hud_print(124, 72, "FASE %d/%d", g.level + 1, num_levels);
		else
			draw_center(72, "FASE DE TESTE");
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

/* Leva os jogadores a um dos 4 pontos fixos da fase (LEVEL_DEF.debug_pts),
 * com a câmera numa direção fixa: medições de FPS/POLIS sempre iguais. */
static void teleport_to(const DEBUG_POINT *pt) {
	int x = pt->cx * TILE_SIZE + TILE_SIZE / 2;
	int z = pt->cz * TILE_SIZE + TILE_SIZE / 2;
	int first = 1;
	for (int k = 0; k < MAX_PLAYERS; k++) {
		PLAYER *p = &g.players[k];
		if (!player_alive(p)) continue;
		int px = x, pz = z;
		if (!first)
			free_spot_near(&px, &pz);            /* o 2o jogador fica ao lado */
		p->pos.vx = px; p->pos.vy = 0; p->pos.vz = pz;
		p->vy = 0;
		p->on_ground = 1;
		p->angle = pt->yaw;
		collide_track(p);                        /* posição definida direto */
		first = 0;
	}
	g.cam_yaw = pt->yaw;
	camera_snap();                               /* câmera direto no lugar */
}

static void debug_teleport(int i) {
	const DEBUG_POINT *pts = mapgen_points(g.level);   /* do mapa gerado ou fixos */
	if (!pts || (g.state != STATE_PLAY && g.state != STATE_PAUSE))
		return;
	teleport_to(&pts[i]);
}

/* Nascem até 8 inimigos num anel em volta do jogador 1 (só onde há chão
 * livre), para medir com muita gente perto. Respeita MAX_ENEMIES. */
static void debug_spawn_enemies(void) {
	const PLAYER *p = &g.players[0];
	if (!player_alive(p) || (g.state != STATE_PLAY && g.state != STATE_PAUSE))
		return;
	for (int k = 0; k < DEBUG_SPAWN_COUNT; k++) {
		int a = (p->angle + k * (4096 / DEBUG_SPAWN_COUNT)) & 4095;
		int x = p->pos.vx + ((isin(a) * DEBUG_SPAWN_DIST) >> 12);
		int z = p->pos.vz + ((icos(a) * DEBUG_SPAWN_DIST) >> 12);
		if (collide_solid_at(x, z, 140))
			continue;
		enemy_spawn(k % num_enemy_types, x, z);
	}
}

/* L2 (solto sozinho) liga/desliga o overlay. Com o overlay aberto:
 *   L2 segurado + direcional -> teleporte para os pontos fixos da fase
 *       (esquerda, direita, cima, baixo = pontos 1 a 4)
 *   L2 segurado + START  -> pula de fase
 *   L2 segurado + SELECT -> marcadores de colisão (células sólidas)
 *   L2 segurado + TRIANGULO -> nascem 8 inimigos em volta (medição)
 *   R2 segurado + direcional (só com DEBUG_FOG_TUNING) -> ajusta a névoa:
 *       cima/baixo = far +-100, direita/esquerda = near +-50
 * Os botões usados aqui são "comidos" para não pausar nem mover o jogador. */
static void debug_input(INPUT *in) {
	static int l2_was_held = 0, l2_combo = 0;
	int l2 = in->held & PAD_L2;

	if (l2 && show_debug && (in->pressed & PAD_SELECT)) {
		in->pressed &= ~PAD_SELECT;      /* não liga/desliga a lanterna */
		l2_combo = 1;
		show_collision ^= 1;
	}
	if (l2 && show_debug && (in->pressed & PAD_TRIANGLE)) {
		in->pressed &= ~PAD_TRIANGLE;    /* não troca de arma */
		l2_combo = 1;
		debug_spawn_enemies();
	}
	if (l2 && show_debug && g.state == STATE_PLAY) {   /* eventos na hora (etapa 06) */
		static const uint16_t ev_keys[NUM_EV_ACTIONS] = {
			PAD_SQUARE, PAD_CIRCLE, PAD_CROSS, PAD_L1, PAD_R1, PAD_R2,
		};
		for (int a = 0; a < NUM_EV_ACTIONS; a++)
			if (in->pressed & ev_keys[a]) {
				in->pressed &= ~ev_keys[a];  /* não ataca/pula/troca */
				l2_combo = 1;
				events_debug(a);
			}
	}
	if (l2 && show_debug && (in->pressed & PAD_START)) {
		in->pressed &= ~PAD_START;
		l2_combo = 1;
		debug_skip_level();
	}
	if (l2 && show_debug) {
		static const uint16_t dirs[4] = { PAD_LEFT, PAD_RIGHT, PAD_UP, PAD_DOWN };
		for (int i = 0; i < 4; i++)
			if (in->pressed & dirs[i]) {
				l2_combo = 1;
				debug_teleport(i);
			}
		/* o direcional não chega ao jogo enquanto L2 está segurado */
		in->held    &= ~(PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT);
		in->pressed &= ~(PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT);
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

/* ------------------------------------------------------------------ */
/* Benchmark automático (./dev bench)                                  */
/* ------------------------------------------------------------------ */
#ifdef DEBUG_BENCHMARK
/*
 * O jogo mede sozinho: começa direto na fase de teste (semente fixa), passa
 * pelos 4 pontos do teleporte e, em cada um, mede BENCH_MEASURE quadros sem
 * e depois com DEBUG_SPAWN_COUNT inimigos extras. Cada medição vira uma
 * linha de texto na TTY (printf -> BIOS -> log do emulador), fácil de ler
 * por script:
 *   BENCH ponto=inicio inimigos=0 fps_min=30 fps_med=30 polis_max=... ...
 * e no fim "BENCH FIM". O FPS é medido pelos retraços EMULADOS (VSync), então
 * o resultado é o mesmo com o emulador em velocidade ilimitada.
 */
#define BENCH_LEVEL    4      /* fase 5 (FLORESTA TESTE) */
#define BENCH_SEED     12345
#define BENCH_SETTLE   60     /* quadros para blocos/câmera assentarem */
#define BENCH_SETTLE_E 30     /* depois de criar os inimigos */
#define BENCH_MEASURE  180    /* ~3 s a 60 quadros por segundo */

#ifndef BENCH_P2_ANGLE
#define BENCH_P2_ANGLE 1024     /* direção em que o jogador 2 é colocado (-D testa outras) */
#endif

/* Pontos medidos. Os 6 primeiros são os da fase 5 (comparáveis com as
 * etapas anteriores); depois, o PIOR MOMENTO de cada fase da sequência:
 * no 3o ponto de teleporte da fase, com o máximo de inimigos que ela
 * permite juntos em volta do jogador. Cada ponto mede 2 vezes: o estado
 * natural e depois a "multidão" (crowd). */
enum { CROWD_SPAWN8,     /* + DEBUG_SPAWN_COUNT inimigos (fase 5) */
       CROWD_GATHER,     /* todos os inimigos da fase juntados em volta */
       CROWD_SURVIVE };  /* reforços até iniciais + max_enemies (SURVIVE) */
typedef struct {
	const char *name;
	int level;          /* índice em level_defs */
	int pt;             /* ponto de teleporte (0..3) ou -1/-2 = bench_extra[0/1] */
	int crowd;
	int two;            /* 1 = com o jogador 2 no limite de distância */
	int walk;           /* 1 = anda do início até a saída (remontagem ao andar) */
	int combat;         /* 1 = multidão + o jogador atacando sem parar (etapa 07) */
} BENCH_PT;
#include "bench_seeds.h"                   /* ./dev mapgen-test --bench */

/* Fase 5 (fixa): os mesmos 6 pontos de sempre (comparáveis entre etapas) */
static const BENCH_PT bench_fixed[] = {
	{ "inicio",       4,  0, CROWD_SPAWN8,  0 },
	{ "cruzamento",   4,  1, CROWD_SPAWN8,  0 },
	{ "canto_denso",  4,  2, CROWD_SPAWN8,  0 },
	{ "clareira",     4,  3, CROWD_SPAWN8,  0 },
	{ "canto_tapado", 4, -1, CROWD_SPAWN8,  0 },
	{ "canto_2j",     4, -2, CROWD_SPAWN8,  1 },
};
#define BENCH_NFIXED ((int)(sizeof(bench_fixed) / sizeof(bench_fixed[0])))
/* Depois: fases 1-4, cada uma nas 5 sementes de PIOR janela (no ponto 2 =
 * centro da janela mais densa), e o CERCO com 2 jogadores na pior delas */
#define BENCH_POINTS (BENCH_NFIXED + 4 * 5 + 1 + 4 * 5 + 4 * 2 + 4 + 4)
#define BENCH_CROWD_VS 600    /* multidão parada: 10 s (o pior aparece depois dos 3 s) */
#define BENCH_WALK_VS  1500   /* andar: até 25 s (ou até o fim do caminho) */
#define BENCH_SECS     26
#ifdef BENCH_SUBSET                   /* -D BENCH_SUBSET: só os pontos mais pesados (iterações) */
static const int bench_subset[] = { 2, 5, 15, 26, 27, 32, 33, 34, 35, 50, 55, 56, 57, 58,
                                    59, 60, 61, 62 };
#define BENCH_RUN ((int)(sizeof(bench_subset) / sizeof(bench_subset[0])))
#define BENCH_INDEX(i) bench_subset[i]
#else
#define BENCH_RUN BENCH_POINTS
#define BENCH_INDEX(i) (i)
#endif
#define BENCH_SEED_FIXED 0xffffffffu      /* "semente" que carrega a floresta fixa da 04e */
/* eventos medidos em cada ponto, depois da multidão e da remontagem */
/* -1 = controle: a mesma janela sem evento (o quadro pesa sozinho com o tempo) */
static const int  bench_events[] = { -1, EV_TREE_FALL, EV_CLOSE_BEHIND, EV_OPEN_PATH, EV_WAVE };
static const char *const bench_event_names[] = { "ev_controle", "ev_queda", "ev_mata", "ev_raizes", "ev_onda" };
#define BENCH_NEVENTS ((int)(sizeof(bench_events) / sizeof(bench_events[0])))

/* Monta o ponto i (os gerados não cabem numa tabela fixa) */
static BENCH_PT bench_point(int i, uint32_t *seed, char *name) {
	if (i < BENCH_NFIXED) {
		*seed = BENCH_SEED;
		strcpy(name, bench_fixed[i].name);
		return bench_fixed[i];
	}
	i -= BENCH_NFIXED;
	BENCH_PT p = { 0, 0, 2, CROWD_GATHER, 0 };
	if (i < 20) {
		p.level = i / 5;
		*seed = bench_worst_seeds[i / 5][i % 5];
		sprintf(name, "f%d_pior%d", p.level + 1, i % 5 + 1);
	} else if (i == 20) {                         /* CERCO, 2 jogadores, pior semente */
		p.level = 3;
		p.two = 1;
		*seed = bench_worst_seeds[3][0];
		strcpy(name, "f4_pior1_2j");
	} else if (i >= 53) {                         /* COMBATE: multidão + ataque sem parar */
		i -= 53;
		p.level = i;
		p.combat = 1;
		*seed = bench_worst_seeds[i][0];
		sprintf(name, "f%d_combate", p.level + 1);
	} else if (i >= 49) {                         /* ANDAR: início -> saída, pior semente */
		i -= 49;
		p.level = i;
		p.pt = 0;
		p.walk = 1;
		*seed = bench_worst_seeds[i][0];
		sprintf(name, "f%d_andar", p.level + 1);
	} else if (i >= 41) {                         /* floresta FIXA da 04e, mesmo critério */
		i -= 41;
		p.level = i / 2;
		p.pt = i % 2 ? 4 : 5;                     /* 5 = mapgen_worst_point() */
		*seed = BENCH_SEED_FIXED;
		sprintf(name, "f%d_fixo_%s", p.level + 1, i % 2 ? "arv" : "pior");
	} else {                                      /* janelas com MAIS ÁRVORES */
		i -= 21;
		p.level = i / 5;
		p.pt = 4;                                 /* 4 = mapgen_tree_point() */
		*seed = bench_tree_seeds[i / 5][i % 5];
		sprintf(name, "f%d_arv%d", p.level + 1, i % 5 + 1);
	}
	if (p.level == 3) p.crowd = CROWD_SURVIVE;
	return p;
}

static const DEBUG_POINT bench_extra[2] = {
	{ 110, 24, 512 },       /* canto denso onde 2 árvores tapam o jogador (50%) */
	{ 110, 24, 512 },       /* o mesmo, com o jogador 2 no limite de distância */
};

/* Jogador 2 entra/sai só para o ponto de 2 jogadores (posição definida
 * direto: passa pela grade) */
static void bench_player2(int on) {
	PLAYER *p2 = &g.players[1];
	if (!on) {
		if (p2->active) { p2->active = 0; collide_track(p2); g.num_players = 1; }
		return;
	}
	const PLAYER *p1 = &g.players[0];
	p2->index = 1; p2->active = 1; p2->character = 1; p2->skin = 0;
	/* pior caso: o jogador 2 no LIMITE de distância (MAX_PLAYER_SEPARATION),
	 * no primeiro lugar livre do anel (ou um pouco mais perto) */
	int placed = 0;
	for (int r = MAX_PLAYER_SEPARATION - 16; r > 256 && !placed; r -= 64)
		for (int k = 0; k < 16; k++) {
			int a = (k * 256 + BENCH_P2_ANGLE) & 4095;
			int x = p1->pos.vx + ((isin(a) * r) >> 12);
			int z = p1->pos.vz + ((icos(a) * r) >> 12);
			/* mesma medida da regra (dist2d): o bench não pode furar o limite */
			if (dist2d(x - p1->pos.vx, z - p1->pos.vz) > MAX_PLAYER_SEPARATION)
				continue;
			if (!collide_solid_at(x, z, PLAYER_RADIUS + 40)) {
				player_spawn(p2, x, z);          /* chama collide_track */
				placed = 1;
				break;
			}
		}
	g.num_players = 2;
	camera_snap();
}

/* Caminho do ponto "andar": BFS na grade (só no build de bench), do
 * início até o ponto 3 da fase (saída ou clareira) */
static uint16_t walk_prev[MAP_MAX_W * MAP_MAX_H], walk_q[MAP_MAX_W * MAP_MAX_H];
static uint16_t walk_path[MAP_MAX_W * MAP_MAX_H / 4];
static int      walk_n, walk_i;

static int bench_walk_path(int sx, int sz, int tx, int tz) {
	static const int d[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
	memset(walk_prev, 0xff, sizeof(walk_prev));
	int h = 0, t = 0;
	walk_prev[sz * MAP_MAX_W + sx] = (uint16_t)(sz * MAP_MAX_W + sx);
	walk_q[t++] = (uint16_t)(sz * MAP_MAX_W + sx);
	while (h < t) {
		int c = walk_q[h++], x = c % MAP_MAX_W, z = c / MAP_MAX_W;
		if (x == tx && z == tz) break;
		for (int k = 0; k < 4; k++) {
			int nx = x + d[k][0], nz = z + d[k][1];
			if (!reach_walkable(nx, nz) || walk_prev[nz * MAP_MAX_W + nx] != 0xffff) continue;
			walk_prev[nz * MAP_MAX_W + nx] = (uint16_t)c;
			walk_q[t++] = (uint16_t)(nz * MAP_MAX_W + nx);
		}
	}
	if (walk_prev[tz * MAP_MAX_W + tx] == 0xffff) return 0;
	int n = 0;                                   /* de trás para a frente */
	for (int c = tz * MAP_MAX_W + tx; c != sz * MAP_MAX_W + sx; c = walk_prev[c])
		n++;
	if (n > (int)(sizeof(walk_path) / sizeof(walk_path[0]))) return 0;
	walk_n = n;
	for (int c = tz * MAP_MAX_W + tx; n > 0; c = walk_prev[c])
		walk_path[--n] = (uint16_t)c;
	walk_i = 0;
	return 1;
}

/* Um passo de lógica andando no caminho, na velocidade do personagem
 * (posição definida direto: passa pela grade). A câmera (g.cam_yaw, lógica)
 * vira aos poucos para alguns passos à frente, como um jogador faria. */
static int bench_walk_tick(void) {
	PLAYER *p = &g.players[0];
	int speed = character_defs[p->character].speed;
	while (walk_i < walk_n) {
		int c = walk_path[walk_i];
		int tx = (c % MAP_MAX_W) * TILE_SIZE + TILE_SIZE / 2, tz = (c / MAP_MAX_W) * TILE_SIZE + TILE_SIZE / 2;
		int dx = tx - p->pos.vx, dz = tz - p->pos.vz, d = dist2d(dx, dz);
		if (d <= speed) { walk_i++; continue; }
		p->pos.vx += dx * speed / d;
		p->pos.vz += dz * speed / d;
		p->angle = angle_of(dx, dz);
		collide_track(p);
		int ahead = walk_path[walk_i + 6 < walk_n ? walk_i + 6 : walk_n - 1];
		int ax = (ahead % MAP_MAX_W) * TILE_SIZE + TILE_SIZE / 2 - p->pos.vx;
		int az = (ahead / MAP_MAX_W) * TILE_SIZE + TILE_SIZE / 2 - p->pos.vz;
		int want = angle_of(ax, az), diff = angle_diff(g.cam_yaw, want);
		if (diff > 24) diff = 24;
		if (diff < -24) diff = -24;
		g.cam_yaw = (g.cam_yaw + diff) & 4095;
		return 1;
	}
	return 0;                                    /* chegou */
}

/* Uma fatia de 1 s da medição (série: o que cresce com o tempo) */
typedef struct {
	int16_t cpu, logic, en_log, pl_log, en_draw, chunks, trees, other, col, polis, ticks, frames, build, builds;
	int16_t logic_ticks;                      /* passos de lógica rodados (60 = tempo real) */
} BENCH_SEC;

enum { BENCH_TELEPORT, BENCH_WAIT, BENCH_MEASURING, BENCH_DONE };

static struct {
	int      phase, point, timer, extras, stage;
	uint32_t spawned;               /* bits: inimigos criados pelo benchmark */
	uint32_t seed;                  /* semente do ponto atual */
	BENCH_PT cur;                   /* ponto atual */
	char     name[24];
	int      frames, vsyncs, fps_min;
	int      polis_max, polis_sum, ram_max;
	int      col_max, col_sum, col_ticks;
	int      tree_m_max, tree_b_max;
	int      grid_err;              /* entidades fora da célula certa (deve ser 0) */
	int      cam_max, cam_sum, cam_cells_max;   /* custo da câmera por quadro */
	int      trans_max;                         /* árvores a 50% (tapando o jogador) */
	int      cpu_max, cpu_sum, gpu_max, gpu_sum;  /* perfil do quadro, em linhas */
	int      logic_sum, logic_last;               /* parte da CPU gasta na lógica */
	int      p_chunks, p_trees, p_other;          /* desenho por categoria (soma) */
	int      build_max;                           /* pior montagem de bloco (linhas) */
	int      dirty;                               /* eventos mexeram no mapa: recarregar */
	int      logic_max;                           /* pior quadro de lógica (linhas) */
	int      ev_job, ev_impact, ev_check;         /* eventos: pior passo da busca / impacto / checagem */
	int      cut_max;                             /* blocos+árvores cortados (buffer) */
	int      col_frame;                           /* testes de colisão: pior passo do quadro */
	int      walk_done, last_build, tick_count;
	BENCH_SEC sec[BENCH_SECS];                    /* série por segundo */
} bench;
static int bench_combat_t;                    /* passo do roteiro de combate */

/* Lugar livre para um inimigo num anel em volta do jogador 1 */
static int bench_ring_spot(int k, int n, int *x, int *z) {
	const PLAYER *p = &g.players[0];
	for (int r = 640 + (k % 3) * 128; r >= 320; r -= 64) {
		int a = (k * 4096 / n + p->angle) & 4095;
		*x = p->pos.vx + ((isin(a) * r) >> 12);
		*z = p->pos.vz + ((icos(a) * r) >> 12);
		if (!collide_solid_at(*x, *z, 140))
			return 1;
	}
	return 0;
}

/* Fases 1-3: todos os inimigos da fase vêm para perto do jogador (posição
 * definida direto: passa pela grade). Devolve quantos foram juntados. */
static int bench_gather(void) {
	int n = 0, k = 0;
	for (int i = 0; i < MAX_ENEMIES; i++) n += g.enemies[i].active;
	for (int i = 0; i < MAX_ENEMIES; i++) {
		ENEMY *e = &g.enemies[i];
		int x, z;
		if (!e->active || !bench_ring_spot(k++, n ? n : 1, &x, &z)) continue;
		e->pos.vx = x; e->pos.vz = z; e->pos.vy = 0; e->vy = 0;
		collide_track(e);
	}
	return n;
}

/* Fase 4: reforços até o máximo que ela permite (iniciais + max_enemies) */
static void bench_survive_fill(void) {
	int alive = 0;
	for (int i = 0; i < MAX_ENEMIES; i++) alive += g.enemies[i].active;
	int want = alive + level_defs[g.level].max_enemies, k = 0;
	if (want > MAX_ENEMIES) want = MAX_ENEMIES;
	for (int tries = 0; alive < want && tries < 32; tries++) {
		int x, z;
		if (!bench_ring_spot(k, want, &x, &z)) { k++; continue; }
		enemy_spawn(k % num_enemy_types, x, z);
		alive++;
		k++;
	}
}

#ifdef BENCH_REPLAY
/* ------------------------------------------------------------------ */
/* Partida ROTEIRIZADA com hash (./dev replay-test): entradas sintéticas */
/* que dependem só do número do passo, eventos forçados em passos fixos, */
/* e o hash de g (+ mapa: células, árvores, alcance) a cada 10 s. Mesma  */
/* semente + mesmas entradas = mesmo hash, qualquer que seja o FPS: é a  */
/* prova de que uma otimização não mudou a lógica.                       */
/* ------------------------------------------------------------------ */
#define REPLAY_TICKS   3600                /* 60 s por fase */
#define REPLAY_HASH_AT 600                 /* hash a cada 10 s */
static struct { int level, t, done; } rp;
static const int replay_levels[] = { 0, 1, 2, 3, 4 };
#define REPLAY_NLEVELS ((int)(sizeof(replay_levels) / sizeof(replay_levels[0])))

static uint32_t fnv(uint32_t h, const void *p, int n) {
	const uint8_t *b = (const uint8_t *)p;
	for (int i = 0; i < n; i++) h = (h ^ b[i]) * 16777619u;
	return h;
}

static uint32_t replay_hash(void) {
	uint32_t h = fnv(2166136261u, &g, sizeof(g));
	const uint8_t (*c)[MAP_MAX_W] = level_cells();
	for (int z = 0; z < level_height(); z++)
		h = fnv(h, c[z], level_width());
	int x, z, t;
	for (int i = 0; (t = level_tree_get(i, &x, &z)) >= 0; i++) {
		int v[3] = { t, x, z };
		h = fnv(h, v, sizeof(v));
	}
	for (int zz = 0; zz < level_height(); zz++)
		for (int xx = 0; xx < level_width(); xx++)
			h = (h ^ (uint32_t)reach_get(xx, zz)) * 16777619u;
	return h;
}

static void replay_begin_level(void) {
	int lv = replay_levels[rp.level];
	start_level(lv, lv == 4 ? BENCH_SEED : 0x51ED0000u + (uint32_t)lv);
	g.state = STATE_PLAY;
	rp.t = 0;
}

/* Entradas do passo rp.t (antes de CADA game_tick) */
static void replay_input(void) {
	if (rp.done) return;
	static const int8_t dirs[][2] = { { 0, -127 }, { 90, -90 }, { -127, 0 }, { 0, -127 },
	                                  { 127, 30 }, { -60, -110 }, { 0, 127 }, { 110, -60 } };
	const int t = rp.t;
	INPUT *in = &g.in[0];
	memset(&g.in[0], 0, sizeof(g.in));
	in->connected = 1;
	const int8_t *d = dirs[(t / 240) % 8];          /* direção muda a cada 4 s */
	in->lx = d[0];
	in->ly = d[1];
	if ((t % 90) < 40)          in->held |= PAD_SQUARE;     /* ataca (segurar/soltar: arco carrega) */
	if ((t % 600) >= 300 && (t % 600) < 360) in->held |= PAD_R1;   /* gira a câmera */
	if ((t % 480) >= 400)       in->held |= PAD_R2;         /* anda devagar (silêncio) */
	if (t % 150 == 0)           in->pressed |= PAD_CROSS;   /* pula */
	if (t % 300 == 150)         in->pressed |= PAD_TRIANGLE;/* troca de arma (passa por todas) */
	if (t % 900 == 450)         in->pressed |= PAD_SELECT;  /* lanterna */
	in->held |= in->pressed;
	switch (t) {                                    /* eventos forçados */
	case 900:  events_debug(EV_TREE_FALL);    break;
	case 1500: events_debug(EV_CLOSE_BEHIND); break;
	case 2100: events_debug(EV_WAVE);         break;
	case 2400: events_debug(EV_OPEN_PATH);    break;
	case 2700: events_debug(EV_FOG);          break;
	case 3000: events_debug(EV_SILENCE);      break;
	}
}

/* Depois de CADA game_tick */
static void replay_after_tick(void) {
	if (rp.done) return;
	rp.t++;
	if (rp.t % REPLAY_HASH_AT == 0)
		printf("BENCH REPLAY fase=%d t=%d estado=%d hash=%08x\n", replay_levels[rp.level] + 1,
		       rp.t / 60, g.state, (unsigned)replay_hash());
#ifdef REPLAY_DETAIL                             /* depuração: hash por parte de g */
	if (rp.t % 60 == 0) {
		const uint8_t (*c)[MAP_MAX_W] = level_cells();
		uint32_t hc = 2166136261u;
		for (int z = 0; z < level_height(); z++) hc = fnv(hc, c[z], level_width());
		printf("BENCH DET f=%d s=%d jog=%08x inim=%08x rng=%08x ev=%08x tiros=%08x itens=%08x efe=%08x cam=%d cel=%08x\n",
		       replay_levels[rp.level] + 1, rp.t / 60,
		       (unsigned)fnv(2166136261u, g.players, sizeof(g.players)),
		       (unsigned)fnv(2166136261u, g.enemies, sizeof(g.enemies)), (unsigned)g.rng.s,
		       (unsigned)fnv(2166136261u, &g.ev, sizeof(g.ev)),
		       (unsigned)fnv(2166136261u, g.bullets, sizeof(g.bullets)),
		       (unsigned)fnv(2166136261u, g.pickups, sizeof(g.pickups)),
		       (unsigned)fnv(2166136261u, g.effects, sizeof(g.effects)), g.cam_yaw, (unsigned)hc);
	}
#endif
	if (rp.t >= REPLAY_TICKS) {
		if (++rp.level >= REPLAY_NLEVELS) {
			rp.done = 1;
			printf("BENCH FIM\n");
			return;
		}
		replay_begin_level();
	}
}
#endif

static void bench_start(void) {
	sel[0].joined = 1; sel[0].ready = 1; sel[0].character = 0; sel[0].skin = 0;
	sel[1].joined = 0;
	memset(&progress, 0, sizeof(progress));
#ifdef BENCH_REPLAY
	memset(&rp, 0, sizeof(rp));
	printf("BENCH INICIO replay fases=%d passos=%d\n", REPLAY_NLEVELS, REPLAY_TICKS);
	replay_begin_level();
	bench.phase = BENCH_DONE;                /* sem os pontos do bench */
	return;
#endif
	start_level(BENCH_LEVEL, BENCH_SEED);
	g.state = STATE_PLAY;                    /* pula a introdução */
	memset(&bench, 0, sizeof(bench));
#ifdef BENCH_FROM
	bench.point = BENCH_FROM;            /* ./dev bench -D BENCH_FROM=6: só as fases 1-4 */
#endif
	printf("BENCH INICIO fase=%d semente=%d pontos=%d\n", BENCH_LEVEL + 1, BENCH_SEED, BENCH_POINTS);
	printf("BENCH carga fase=%d nome=TESTE linhas=%d ms=%d\n", BENCH_LEVEL + 1,
	       level_load_lines(), level_load_lines() * 635 / 10000);
}

static void bench_remove_extras(void) {
	for (int i = 0; i < MAX_ENEMIES; i++)
		if ((bench.spawned & (1u << i)) && g.enemies[i].active) {
			g.enemies[i].active = 0;
			g.enemies_left--;
			collide_track(&g.enemies[i]);    /* sai da grade */
		}
	bench.spawned = 0;
}

static void bench_reset_stats(void) {
	bench.frames = bench.vsyncs = 0;
	bench.fps_min = 60;
	bench.polis_max = bench.polis_sum = bench.ram_max = 0;
	bench.col_max = bench.col_sum = bench.col_ticks = 0;
	bench.tree_m_max = bench.tree_b_max = 0;
	bench.grid_err = 0;
	bench.cam_max = bench.cam_sum = bench.cam_cells_max = 0;
	bench.trans_max = 0;
	bench.cpu_max = bench.cpu_sum = bench.gpu_max = bench.gpu_sum = 0;
	bench.logic_sum = 0;
	bench.p_chunks = bench.p_trees = bench.p_other = 0;
	bench.build_max = 0;
	bench.logic_max = 0;
	bench.ev_job = bench.ev_impact = bench.ev_check = 0;
	{ int d; level_build_stats(&d); }         /* zera o pior */
	bench.cut_max = 0;
	memset(bench.sec, 0, sizeof(bench.sec));
	bench_combat_t = 0;
	bench.walk_done = 0;
	bench_prof_players = bench_prof_enemies = 0;
}

/* Antes de cada passo de lógica: entradas roteirizadas do ponto de COMBATE
 * (o jogador ataca sem parar; ninguém morre, para a multidão não diminuir) */
static void bench_input(void) {
	if (!(bench.cur.combat && bench.stage == 1 && bench.phase == BENCH_MEASURING))
		return;
	int t = bench_combat_t++;
	INPUT *in = &g.in[0];
	in->held |= PAD_SQUARE;                          /* ataca sempre */
	if (t % 120 == 0) in->pressed |= PAD_TRIANGLE;   /* troca de arma a cada 2 s */
	if ((t % 240) < 60) in->held |= PAD_R1;          /* gira a câmera */
	for (int i = 0; i < MAX_ENEMIES; i++)            /* (só no bench) */
		if (g.enemies[i].active) g.enemies[i].hp = enemy_defs[g.enemies[i].type].hp;
	for (int i = 0; i < MAX_PLAYERS; i++)
		if (g.players[i].active && g.players[i].hp > 0) {
			g.players[i].hp = g.players[i].max_hp;
			g.players[i].energy = PLAYER_MAX_ENERGY;     /* pior caso: todas as armas, */
			g.players[i].weapons_owned = (1u << num_weapons) - 1;   /* munição infinita */
			g.players[i].arrows = MAX_ARROWS;
			g.players[i].bullets = MAX_BULLETS_AMMO;
		}
}

/* Depois de cada passo de lógica: custo da colisão naquele passo */
static void bench_tick(void) {
	if (bench.cur.walk && bench.stage == 9 && bench.phase == BENCH_MEASURING && !bench.walk_done)
		bench.walk_done = !bench_walk_tick();
	if (bench.phase != BENCH_MEASURING) return;
	int c = collide_tests_now();
	if (c > bench.col_frame) bench.col_frame = c;
	bench.tick_count++;
	if (c > bench.col_max) bench.col_max = c;
	bench.col_sum += c;
	bench.col_ticks++;
}

/* Depois de cada quadro desenhado. vs = retraços que o quadro anterior levou */
static void bench_frame(int vs) {
	switch (bench.phase) {
	case BENCH_TELEPORT:
		bench_remove_extras();
		bench_player2(0);
		if (bench.point >= BENCH_RUN) {
			printf("BENCH FIM\n");
			bench.phase = BENCH_DONE;
			break;
		}
		{
			bench.cur = bench_point(BENCH_INDEX(bench.point), &bench.seed, bench.name);
			const BENCH_PT *bp = &bench.cur;
			if (g.level != bp->level || g.seed != bench.seed || bench.dirty) {   /* outro mapa (ou mexido): carrega */
				bench.dirty = 0;
				bench_force_fixed = bench.seed == BENCH_SEED_FIXED;
				start_level(bp->level, bench.seed);
				const MAPGEN_STATS *ms = mapgen_stats();
				printf("BENCH carga fase=%d nome=%s semente=%08x linhas=%d ms=%d tentativas=%d plano_b=%d janela=%d "
				       "gerar_ms=%d validar_ms=%d mata=%d\n",
				       bp->level + 1, level_defs[bp->level].name, (unsigned)bench.seed, level_load_lines(),
				       level_load_lines() * 635 / 10000, ms->tries, ms->fallback, ms->win_weight / 4,
				       ms->gen_lines * 635 / 10000, ms->val_lines * 635 / 10000, ms->thicket_permil);
			}
			g.state = STATE_PLAY;                /* pula a introdução */
			if (bp->pt == 5)
				teleport_to(mapgen_worst_point());
			else if (bp->pt == 4)
				teleport_to(mapgen_tree_point());
			else if (bp->pt >= 0)
				teleport_to(&mapgen_points(bp->level)[bp->pt]);
			else
				teleport_to(&bench_extra[-bp->pt - 1]);
			if (bp->two)
				bench_player2(1);
		}
		bench.extras = 0;
		bench.stage = 0;
		if (bench.cur.walk) {                /* andar: do início até o ponto 3 */
			const DEBUG_POINT *tp = &mapgen_points(bench.cur.level)[3];
			const PLAYER *p0 = &g.players[0];
			if (!bench_walk_path(p0->pos.vx / TILE_SIZE, p0->pos.vz / TILE_SIZE, tp->cx, tp->cz))
				walk_n = walk_i = 0;
			bench.stage = 9;
		}
		bench.timer = BENCH_SETTLE;
		bench.phase = BENCH_WAIT;
		break;

	case BENCH_WAIT:
		if (--bench.timer <= 0) {
			bench_reset_stats();
			bench.timer = BENCH_MEASURE;
			if (bench.stage == 1) bench.timer = 1 << 20;     /* por retraços: BENCH_CROWD_VS */
			if (bench.stage == 9) bench.timer = 1 << 20;     /* até chegar ou BENCH_WALK_VS */
			if (bench.stage >= 3 && bench.stage < 3 + BENCH_NEVENTS) {   /* um evento na hora (06) */
				if (bench_events[bench.stage - 3] >= 0)
					events_debug(bench_events[bench.stage - 3]);
				bench.dirty = 1;
				bench.timer = 180;           /* busca + aviso + tombo + ~60 quadros */
			}
			if (bench.stage == 2) {          /* remonta EM FATIAS o bloco do jogador */
				const PLAYER *p0 = &g.players[0];  /* (como a barreira que se abre) */
#ifndef BENCH_NO_REBUILD                     /* controle: a mesma janela sem remontar */
				level_rebuild_chunk(p0->pos.vx / TILE_SIZE, p0->pos.vz / TILE_SIZE);
#else
				(void)p0;
#endif
				bench.timer = 60;            /* o quadro da montagem + 59 */
			}
			bench.phase = BENCH_MEASURING;
		}
		break;

	case BENCH_MEASURING: {
		int fps = vs > 0 ? 60 / vs : 60;
		if (fps < bench.fps_min) bench.fps_min = fps;
		bench.frames++;
		bench.vsyncs += vs;
		int p = render_stats_polys(), r = render_stats_bytes();
		if (p > bench.polis_max) bench.polis_max = p;
		bench.polis_sum += p;
		if (r > bench.ram_max) bench.ram_max = r;
		int tt, tm, tb;
		level_tree_stats(&tt, &tm, &tb);
		if (tm > bench.tree_m_max) bench.tree_m_max = tm;
		if (tb > bench.tree_b_max) bench.tree_b_max = tb;
		int ct, cc;
		camera_stats(&ct, &cc);
		if (ct > bench.cam_max) bench.cam_max = ct;
		if (cc > bench.cam_cells_max) bench.cam_cells_max = cc;
		bench.cam_sum += ct;
		if (level_tree_transparent() > bench.trans_max) bench.trans_max = level_tree_transparent();
		int pc, pg, pv;
		render_stats_time(&pc, &pg, &pv);
		if (pc > bench.cpu_max) bench.cpu_max = pc;
		if (pg > bench.gpu_max) bench.gpu_max = pg;
		bench.cpu_sum += pc;
		bench.gpu_sum += pg;
		bench.logic_sum += bench.logic_last;
		if (bench.logic_last > bench.logic_max) bench.logic_max = bench.logic_last;
		{
			int j, im;
			events_prof(&j, &im);
			if (j > bench.ev_job) bench.ev_job = j;
			if (im > bench.ev_impact) bench.ev_impact = im;
			int c = events_prof_check();
			if (c > bench.ev_check) bench.ev_check = c;
		}
		bench.p_chunks += prof_get(PROF_CHUNKS);
		{
			int b;
			level_build_stats(&b);
			if (b > bench.build_max) bench.build_max = b;
			bench.last_build = b;
		}
		bench.p_trees  += prof_get(PROF_TREES);
		bench.p_other  += prof_get(PROF_OTHER);
		if (level_cut_count() > bench.cut_max) bench.cut_max = level_cut_count();
		int ge = collide_check_grid();
		if (ge > bench.grid_err) bench.grid_err = ge;
		{                                    /* série: fatia do segundo atual */
			int si = (bench.vsyncs - vs) / 60;
			if (si >= BENCH_SECS) si = BENCH_SECS - 1;
			BENCH_SEC *sc = &bench.sec[si];
			int ed = prof_get(PROF_ENEMIES), ot = prof_get(PROF_OTHER) - ed;
#define BMAX(f, v) if ((v) > sc->f) sc->f = (v)
			BMAX(cpu, pc); BMAX(logic, bench.logic_last);
			BMAX(en_log, bench_prof_enemies); BMAX(pl_log, bench_prof_players);
			BMAX(en_draw, ed); BMAX(chunks, prof_get(PROF_CHUNKS)); BMAX(trees, prof_get(PROF_TREES));
			BMAX(other, ot); BMAX(col, bench.col_frame); BMAX(polis, p); BMAX(build, bench.last_build);
#undef BMAX
			sc->ticks += vs;
			sc->frames++;
			sc->logic_ticks += bench.tick_count;
			bench.tick_count = 0;
			sc->builds += bench.last_build > 0;
			bench.col_frame = 0;
			bench_prof_players = bench_prof_enemies = 0;
		}
		if (bench.stage == 1) {              /* multidão: 10 s em retraços */
			if (bench.vsyncs < BENCH_CROWD_VS) break;
		} else if (bench.stage == 9) {       /* andar: até chegar ou 25 s */
			if (bench.vsyncs < BENCH_WALK_VS && !bench.walk_done) break;
		} else if (--bench.timer > 0) {
			break;
		}

		int n = 0;
		for (int i = 0; i < MAX_ENEMIES; i++) n += g.enemies[i].active;
		printf("BENCH ponto=%s inimigos_extra=%d inimigos_total=%d fps_min=%d fps_med=%d "
		       "polis_max=%d polis_med=%d ram_gpu_max=%d col_max=%d col_med=%d "
		       "arvores_modelo=%d arvores_plana=%d grade_erros=%d cam_max=%d cam_med=%d cam_celulas_max=%d arvores_transp=%d "
		       "cpu_max=%d cpu_med=%d gpu_espera_max=%d gpu_espera_med=%d logica_med=%d "
		       "desenho_blocos=%d desenho_arvores=%d desenho_resto=%d separacao=%d corte=%d "
		       "medida=%s montagem_max=%d evento_ok=%d logica_max=%d busca_max=%d impacto=%d checagem=%d\n",
		       bench.name, bench.extras, n, bench.fps_min,
		       bench.vsyncs ? bench.frames * 60 / bench.vsyncs : 0,
		       bench.polis_max, bench.frames ? bench.polis_sum / bench.frames : 0, bench.ram_max,
		       bench.col_max, bench.col_ticks ? bench.col_sum / bench.col_ticks : 0,
		       bench.tree_m_max, bench.tree_b_max, bench.grid_err,
		       bench.cam_max, bench.frames ? bench.cam_sum / bench.frames : 0, bench.cam_cells_max,
		       bench.trans_max,
		       bench.cpu_max, bench.frames ? bench.cpu_sum / bench.frames : 0,
		       bench.gpu_max, bench.frames ? bench.gpu_sum / bench.frames : 0,
		       bench.frames ? bench.logic_sum / bench.frames : 0,
		       bench.frames ? bench.p_chunks / bench.frames : 0,
		       bench.frames ? bench.p_trees / bench.frames : 0,
		       bench.frames ? bench.p_other / bench.frames : 0,
		       g.players[1].active ? dist2d(g.players[1].pos.vx - g.players[0].pos.vx,
		                                    g.players[1].pos.vz - g.players[0].pos.vz) : 0,
		       bench.cut_max,
		       bench.stage == 0 ? "natural" : bench.stage == 1 ? (bench.cur.combat ? "combate" : "multidao") :
		       bench.stage == 9 ? "andar" :
		       bench.stage == 2 ? "remontagem" : bench_event_names[bench.stage - 3],
		       bench.build_max,
		       bench.stage >= 3 ? g.ev.last == bench_events[bench.stage - 3] : -1,
		       bench.logic_max, bench.ev_job, bench.ev_impact, bench.ev_check);

#ifdef SUBCULL_CHECK
		{ extern int subcull_lost; printf("BENCH subcull ponto=%s perdidos=%d\n", bench.name, subcull_lost); subcull_lost = 0; }
#endif
#ifdef YAW_CHECK
		{ extern int yaw_mismatch, yaw_checks;
		  printf("BENCH yaw ponto=%s diferentes=%d de=%d\n", bench.name, yaw_mismatch, yaw_checks); }
#endif
		if (bench.stage == 1 || bench.stage == 9)   /* a série, 1 linha por segundo */
			for (int si = 0; si < BENCH_SECS && bench.sec[si].frames; si++) {
				const BENCH_SEC *sc = &bench.sec[si];
				printf("BENCH serie ponto=%s medida=%s s=%d cpu_max=%d logica_max=%d passos=%d quadros=%d "
				       "inim_logica=%d jog_logica=%d inim_desenho=%d blocos=%d arvores=%d resto=%d "
				       "col_max=%d polis_max=%d montagem_max=%d montagens=%d passos_logica=%d\n",
				       bench.name, bench.stage == 1 ? (bench.cur.combat ? "combate" : "multidao") : "andar",
				       si, sc->cpu, sc->logic,
				       sc->ticks, sc->frames, sc->en_log, sc->pl_log, sc->en_draw, sc->chunks, sc->trees,
				       sc->other, sc->col, sc->polis, sc->build, sc->builds, sc->logic_ticks);
			}

		if (bench.stage == 9) {              /* andar: acabou o ponto */
			bench.point++;
			bench.dirty = 1;
			bench.phase = BENCH_TELEPORT;
		} else if (bench.stage == 0) {       /* agora a "multidão" */
			uint32_t before = 0, after = 0;
			for (int i = 0; i < MAX_ENEMIES; i++) before |= (uint32_t)g.enemies[i].active << i;
			switch (bench.cur.crowd) {
			case CROWD_SPAWN8:  debug_spawn_enemies(); break;
			case CROWD_GATHER:  bench.extras = bench_gather(); break;
			case CROWD_SURVIVE: bench_survive_fill(); break;
			}
			for (int i = 0; i < MAX_ENEMIES; i++) after |= (uint32_t)g.enemies[i].active << i;
			bench.spawned = after & ~before;
			if (bench.cur.crowd != CROWD_GATHER) {
				bench.extras = 0;            /* contar bits sem builtin (libgcc) */
				for (uint32_t m = bench.spawned; m; m &= m - 1)
					bench.extras++;
			}
			bench.stage = 1;
			bench.timer = BENCH_SETTLE_E;
			bench.phase = BENCH_WAIT;
		} else if (bench.stage >= 2 && bench.stage < 2 + BENCH_NEVENTS) {   /* próximo evento */
			bench.stage++;
			bench.timer = 10;                /* espera o printf (lento) sair do quadro */
			bench.phase = BENCH_WAIT;
		} else if (bench.stage == 1 && bench.cur.combat) {   /* combate: só a multidão */
			bench.point++;
			bench.dirty = 1;
			bench.phase = BENCH_TELEPORT;
		} else if (bench.stage == 1) {       /* remontagem: o bloco do jogador */
#ifdef BENCH_NO_EVENTS                       /* iterações: só natural + multidão */
			bench.point++;
			bench.phase = BENCH_TELEPORT;
			break;
#endif
			bench.stage = 2;                 /* (custo de um evento que muda o mapa); */
			bench.timer = 10;                /* espera: o printf acima é lento e cairia */
			bench.phase = BENCH_WAIT;        /* no quadro medido */
		} else {
			bench.point++;
			bench.phase = BENCH_TELEPORT;
		}
		break;
	}
	}
}
#endif

/* Loops de ambiente (vozes 0 e 1): tocam enquanto há uma partida na tela
 * (jogo, pausa, vitória, derrota) e param no título e na seleção. */
static void ambience_update(void) {
	static int playing = 0;
	/* evento "silêncio": o ambiente para (só lê g.ev) */
	int want = g.state != STATE_TITLE && g.state != STATE_SELECT && g.ev.silence == 0;
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
#ifdef DEBUG_BENCHMARK
	bench_start();
#endif

	int last_vsync = VSync(-1);

	while (1) {
		/* PASSO FIXO: a lógica sempre roda 60 vezes por segundo, mesmo que
		 * o desenho caia para 30 ou 20 quadros. Assim o jogo não fica
		 * "em câmera lenta" quando a cena é pesada. */
		int now   = VSync(-1);
		int ticks = now - last_vsync;
#ifdef DEBUG_BENCHMARK
		int frame_vs = ticks;          /* retraços do quadro anterior (sem limite) */
#endif
		last_vsync = now;
		if (ticks < 1) ticks = 1;
		if (ticks > MAX_TICKS_PER_FRAME) ticks = MAX_TICKS_PER_FRAME;

		input_update(g.in);
		debug_input(&g.in[0]);         /* L2: overlay e atalhos (controle 1) */

#ifdef DEBUG_BENCHMARK
		uint16_t t_logic = TIMER_VALUE(1);     /* perfil: tempo da lógica do quadro */
#endif
		for (int t = 0; t < ticks; t++) {
#ifdef BENCH_REPLAY
			replay_input();
#endif
#ifdef DEBUG_BENCHMARK
			bench_input();
#endif
			game_tick();
#ifdef BENCH_REPLAY
			replay_after_tick();
#endif
#ifdef DEBUG_BENCHMARK
			bench_tick();
#endif
			for (int i = 0; i < MAX_PLAYERS; i++)
				g.in[i].pressed = 0;    /* "apertou agora" vale só 1 passo */
		}

#ifdef DEBUG_BENCHMARK
		bench.logic_last = (uint16_t)(TIMER_VALUE(1) - t_logic);
#endif
		/* câmera visual: segue o grupo depois da lógica (fora de g) */
		if (g.state != STATE_TITLE && g.state != STATE_SELECT)
			camera_view_update(ticks);
		ambience_update();
		game_draw();
		render_end_frame();
#ifdef DEBUG_BENCHMARK
		bench_frame(frame_vs);
#endif

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
