/*
 * menu.c - Menu do START: Mapa, Objetivo, Estatísticas, Armas e Itens (etapa 08)
 *
 * START na partida abre o menu (g.state = STATE_MENU) e o jogo PARA: o
 * game_tick não roda nenhum *_update e nem avança g.frame (a pausa antiga
 * avançava g.frame, e os inimigos usam g.frame para escalonar a visão: a
 * partida depois da pausa seria outra). ./dev replay-test -D REPLAY_MENU=1
 * e ./dev robo-test -D ROBOT_MENU=1 provam: abrir e navegar sem usar nada
 * dá o MESMO hash.
 *
 * O menu é lógica (usar um item muda g), então o estado dele fica em g:
 *   g.menu_tab     aba atual (fica: reabre nela)
 *   g.menu_cursor  linha na aba (zera ao fechar)
 *   g.menu_owner   quem abriu (só o controle dele navega)
 *   g.menu_player  de quem é o inventário mostrado (SELECT troca P1/P2)
 *
 * Controles: L1/R1 ou esquerda/direita = aba; cima/baixo = linha;
 * X = usar/equipar; QUADRADO = largar o item no chão; CÍRCULO ou START = sair.
 *
 * Desenho (menu_draw) só LÊ g. Lembrete do PS1: render_rect põe tudo na
 * entrada 1 da Ordering Table e o ÚLTIMO colocado é desenhado PRIMEIRO
 * (fica por baixo); o texto (hud_print) fica na entrada 0, por cima de tudo.
 * Por isso os destaques são colocados ANTES dos painéis.
 */
#include <psxpad.h>
#include <stdio.h>
#include <string.h>
#include "game.h"
#include "models.h"

enum { TAB_MAP, TAB_GOAL, TAB_STATS, TAB_WEAPONS, TAB_ITEMS, NUM_TABS };
static const char *const tab_names[NUM_TABS] = { "MAPA", "OBJETIVO", "ESTAT.", "ARMAS", "ITENS" };

static PLAYER *shown(void) { return &g.players[g.menu_player]; }

/* Armas que o jogador mostrado possui, na ordem da tabela */
static int owned_list(const PLAYER *p, int *list) {
	int n = 0;
	for (int w = 0; w < num_weapons; w++)
		if (p->weapons_owned & (1u << w)) list[n++] = w;
	return n;
}

static int rows(void) {
	int list[NUM_WEAPON_SLOTS];
	switch (g.menu_tab) {
	case TAB_WEAPONS: return owned_list(shown(), list);
	case TAB_ITEMS:   return INV_SLOTS;
	}
	return 0;
}

void menu_open(int who) {
	g.state = STATE_MENU;
	g.menu_owner = who;
	g.menu_player = who;
	g.menu_cursor = 0;
}

static void menu_close(void) {
	g.state = STATE_PLAY;
	g.menu_cursor = 0;                    /* a aba fica; o cursor zera */
}

/* Um passo com o menu aberto (só o controle de quem abriu) */
void menu_update(void) {
	const INPUT *in = &g.in[g.menu_owner];
	uint16_t pr = in->pressed;
	if (pr & (PAD_START | PAD_CIRCLE)) { menu_close(); return; }

	if (pr & (PAD_R1 | PAD_RIGHT)) { g.menu_tab = (g.menu_tab + 1) % NUM_TABS; g.menu_cursor = 0; }
	if (pr & (PAD_L1 | PAD_LEFT))  { g.menu_tab = (g.menu_tab + NUM_TABS - 1) % NUM_TABS; g.menu_cursor = 0; }
	if ((pr & PAD_SELECT) && g.num_players > 1) {       /* inventário do outro jogador */
		for (int k = 1; k < MAX_PLAYERS; k++) {
			int o = (g.menu_player + k) % MAX_PLAYERS;
			if (g.players[o].active) { g.menu_player = o; g.menu_cursor = 0; break; }
		}
	}
	int n = rows();
	if (n > 0) {
		if (pr & PAD_DOWN) g.menu_cursor = (g.menu_cursor + 1) % n;
		if (pr & PAD_UP)   g.menu_cursor = (g.menu_cursor + n - 1) % n;
	}

	PLAYER *p = shown();
	if (g.menu_tab == TAB_WEAPONS && (pr & PAD_CROSS) && player_alive(p)) {
		int list[NUM_WEAPON_SLOTS];
		if (g.menu_cursor < owned_list(p, list) && p->weapon != list[g.menu_cursor]) {
			p->weapon = list[g.menu_cursor];          /* equipar */
			p->swing = 0;
			p->charge = 0;
		}
	}
	if (g.menu_tab == TAB_ITEMS) {
		if (pr & PAD_CROSS)  inv_use(p, g.menu_cursor);
		if (pr & PAD_SQUARE) inv_drop(p, g.menu_cursor);
	}
}

/* ------------------------------------------------------------------ */
/* Desenho (só lê g)                                                    */
/* ------------------------------------------------------------------ */
#define PX 16                       /* painel: x, y, largura, altura */
#define PY 30
#define PW (SCREEN_W - 32)
#define PH 172

/* Ícone 16x16 da folha (item k: 16*(k-1); arma w: 64 + 16*w) */
static void icon(int u, int x, int y) {
	render_sprite(&tex_icones_menu_t, u, 0, 16, 16, x, y, 16, 16);
}


static void draw_tabs(void) {
	int x = 24;
	for (int t = 0; t < NUM_TABS; t++) {
		int w = (int)strlen(tab_names[t]) * 8;
		if (t == g.menu_tab)
			render_rect(x - 4, 14, w + 8, 12, 90, 70, 30, 0);   /* aba atual */
		hud_print(x, 16, "%s", tab_names[t]);
		x += w + 16;
	}
}

static void draw_goal(void) {
	const LEVEL_DEF *ld = &level_defs[g.level];
	int y = PY + 10;
	hud_print(PX + 8, y, "FASE %d: %s", g.level + 1, ld->name);     y += 16;
	hud_print(PX + 8, y, "%s", ld->obj_text);                       y += 20;
	hud_print(PX + 8, y, "AGORA: %s", objective_text());             y += 12;
	switch (ld->objective) {
	case OBJ_KILL_ALL:
		hud_print(PX + 8, y, "INIMIGOS RESTANTES: %d", g.enemies_left); y += 12;
		break;
	case OBJ_COLLECT:
		hud_print(PX + 8, y, "RELIQUIAS: %d DE %d", g.quest, g.quest_need); y += 12;
		break;
	case OBJ_SURVIVE: {
		int s = (g.survive_left + 59) / 60;
		hud_print(PX + 8, y, "FALTAM %d:%02d", s / 60, s % 60); y += 12;
		break;
	}
	}
	if (g.has_exit)
		hud_print(PX + 8, y, "SAIDA: %s", g.exit_open ? (explore_seen(g.exit_x / TILE_SIZE, g.exit_z / TILE_SIZE) ?
		          "ABERTA (NO MAPA)" : "ABERTA (AINDA NAO VISTA)") : "FECHADA");
	y += 20;
	hud_print(PX + 8, y, "GEMAS %d/%d   CHAVES %d", g.gems, g.gems_total, g.ev.keys);
}

static void draw_stats(void) {
	const PLAYER *p = shown();
	const PSTATS *s = &p->st;
	int y = PY + 10, t = g.play_frames / 60;
	hud_print(PX + 8, y, "TEMPO %d:%02d   SEMENTE %08X", t / 60, t % 60, (unsigned)g.seed); y += 16;
	hud_print(PX + 8, y, "INIMIGOS DERROTADOS");                                         y += 12;
	for (int k = 0; k < num_enemy_types && k < STAT_ENEMY_TYPES; k++) {
		hud_print(PX + 16, y, "%-12s %d", enemy_defs[k].name, s->kills[k]);              y += 10;
	}
	y += 6;
	if (s->shots)
		hud_print(PX + 8, y, "PRECISAO %d%%  (%d DE %d)", s->hits * 100 / s->shots, s->hits, s->shots);
	else
		hud_print(PX + 8, y, "PRECISAO -");
	y += 12;
	hud_print(PX + 8, y, "DANO RECEBIDO %d", s->dmg_taken);                                y += 12;
	hud_print(PX + 8, y, "ITENS USADOS  %d", s->items_used);                               y += 12;
	hud_print(PX + 8, y, "DISTANCIA     %d M", s->dist / TILE_SIZE);                       y += 12;
	hud_print(PX + 8, y, "PONTOS        %d", g.score);
}

static void draw_weapons(void) {
	const PLAYER *p = shown();
	int list[NUM_WEAPON_SLOTS], n = owned_list(p, list), y = PY + 10;
	for (int i = 0; i < n; i++, y += 18) {
		const WEAPON_DEF *w = &weapon_defs[list[i]];
		icon(64 + 16 * list[i], PX + 12, y - 5);
		if (i == g.menu_cursor)
			render_rect(PX + 4, y - 6, PW - 8, 18, 70, 60, 30, 0);
		char ammo[24];
		switch (w->ammo) {
		case AMMO_ARROW:  snprintf(ammo, sizeof(ammo), "FLECHAS %d", p->arrows); break;
		case AMMO_BULLET: snprintf(ammo, sizeof(ammo), "BALAS %d", p->bullets); break;
		case AMMO_MANA:   snprintf(ammo, sizeof(ammo), "MANA %d/%d", w->cost, p->energy); break;
		default:          snprintf(ammo, sizeof(ammo), "-"); break;
		}
		hud_print(PX + 32, y, "%c %-13s %s", list[i] == p->weapon ? '*' : ' ', w->name, ammo);
	}
}

static void draw_items(void) {
	const PLAYER *p = shown();
	int y = PY + 10;
	for (int i = 0; i < INV_SLOTS; i++, y += 18) {
		const INV_SLOT *s = &p->inv[i];
		if (s->type != IT_NONE)
			icon(16 * (s->type - 1), PX + 24, y - 5);
		if (i == g.menu_cursor)
			render_rect(PX + 4, y - 6, PW - 8, 18, 70, 60, 30, 0);
		if (s->type == IT_NONE)
			hud_print(PX + 10, y, "%d", i + 1);
		else
			hud_print(PX + 10, y, "%d    %-13s X%d %s", i + 1, item_defs[s->type].name, s->count,
			          item_defs[s->type].desc);
	}
	y += 6;
	hud_print(PX + 8, y, "VIDA %d/%d  MANA %d  PILHA %d%%", p->hp, p->max_hp, p->energy,
	          p->battery * 100 / BATTERY_MAX);
}

/* ------------------------------------------------------------------ */
/* Aba Mapa                                                             */
/* ------------------------------------------------------------------ */
/* A grade (até 128x128) vira uma textura 4 bits de 128x128 (8 KB aqui) que
 * substitui a reserva tex_mapa_menu_t na VRAM; um único sprite na tela.
 * Desenhar célula a célula seriam até 16 mil retângulos por quadro. Só é
 * remontada quando algo pode ter mudado (com o menu aberto o jogo para:
 * uma vez por abertura). Células nunca vistas (g.seen) ficam escuras. */
enum { MC_NONE, MC_UNSEEN, MC_DIRT, MC_LEAVES, MC_ROOTS, MC_THICKET, MC_BARRIER, MC_TREE, MC_LOG, MC_VOID };
#define RGB15(r, gg, b) (uint16_t)(((r) >> 3) | (((gg) >> 3) << 5) | (((b) >> 3) << 10))
static const uint16_t map_clut[16] = {
	0, RGB15(14, 14, 18), RGB15(150, 115, 70), RGB15(60, 85, 45), RGB15(110, 80, 50),
	RGB15(25, 55, 25), RGB15(140, 70, 35), RGB15(10, 32, 12), RGB15(170, 120, 60), RGB15(6, 6, 6),
};
static uint8_t map_pix[128 * 128 / 2];
static uint32_t map_sig = 0xffffffffu;
static int map_row = 128, map_seen, map_seen_pct;    /* montagem em faixas */
#define MAP_ROWS_PER_FRAME 16                      /* 8 quadros para 128 linhas */

/* Uma faixa de linhas. Lê as tabelas DIRETO (células, obstáculos, g.seen):
 * uma chamada de função por célula custava ~4x mais (16 mil células). */
static void map_build_rows(int z0, int n) {
	const uint8_t (*cells)[MAP_MAX_W] = level_cells();
	const int16_t (*obst)[MAP_MAX_W] = collide_obst_heads();
	int w = level_width(), h = level_height();
	static const uint8_t type_color[8] = { MC_VOID, MC_DIRT, MC_LEAVES, MC_ROOTS, MC_THICKET, MC_BARRIER,
	                                       MC_LEAVES, MC_LEAVES };
	for (int z = z0; z < z0 + n; z++) {
		uint8_t *row = &map_pix[z * 64];
		for (int x = 0; x < 128; x += 2) {
			int col[2];
			for (int k = 0; k < 2; k++) {
				int cx = x + k;
				if (cx >= w || z >= h) { col[k] = MC_VOID; continue; }
				if (!((g.seen[z][cx >> 3] >> (cx & 7)) & 1)) { col[k] = MC_UNSEEN; continue; }
				map_seen++;
				int c = cells[z][cx];
				col[k] = (c & CELL_LOW) ? MC_LOG : obst[z][cx] >= 0 ? MC_TREE : type_color[c & 7];
			}
			row[x >> 1] = (uint8_t)(col[0] | (col[1] << 4));   /* 4 bits: pixel da esquerda embaixo */
		}
	}
	render_texture_update4(&tex_mapa_menu_t, &map_pix[z0 * 64], z0, n, z0 == 0 ? map_clut : NULL);
	if (z0 + n >= 128) map_seen_pct = map_seen * 100 / (w * h);
}

/* Marcador quadrado de lado s no ponto do mundo (x,z) */
static void map_mark(int ox, int oy, int scale, int x, int z, int s, int r, int gg, int b) {
	int px = ox + (x * scale) / TILE_SIZE, py = oy + (z * scale) / TILE_SIZE;
	render_rect(px - s / 2, py - s / 2, s, s, r, gg, b, 0);
}

void menu_draw_map(void) {
	int w = level_width(), h = level_height();
	int scale = (w <= 64 && h <= 64) ? 2 : 1;           /* 64x64 aparece em 128 px também */
	int ox = PX + 8, oy = PY + 10;
	uint32_t sig = (uint32_t)g.frame * 2654435761u ^ g.seed ^ (uint32_t)g.level;
	if (sig != map_sig) {                               /* recomeça a montagem */
		map_sig = sig;
		map_row = 0;
		map_seen = 0;
	}
	if (map_row < 128) {
		map_build_rows(map_row, MAP_ROWS_PER_FRAME);
		map_row += MAP_ROWS_PER_FRAME;
	}
	/* marcadores ANTES do sprite (ficam por cima): jogadores, saída e
	 * relíquias só se a célula já foi vista */
	for (int i = 0; i < MAX_PLAYERS; i++) {
		const PLAYER *p = &g.players[i];
		if (!player_alive(p)) continue;
		int dx = (isin(p->angle) * 4) >> 12, dz = (icos(p->angle) * 4) >> 12;   /* para onde olha */
		render_rect(ox + (p->pos.vx * scale) / TILE_SIZE + dx - 1, oy + (p->pos.vz * scale) / TILE_SIZE + dz - 1,
		            2, 2, 255, 255, 255, 0);
		map_mark(ox, oy, scale, p->pos.vx, p->pos.vz, 4, i ? 255 : 80, i ? 90 : 160, i ? 90 : 255);
	}
	if (g.has_exit && explore_seen(g.exit_x / TILE_SIZE, g.exit_z / TILE_SIZE))
		map_mark(ox, oy, scale, g.exit_x, g.exit_z, 5, g.exit_open ? 120 : 90, g.exit_open ? 230 : 90, 255);
	for (int i = 0; i < MAX_PICKUPS; i++) {
		const PICKUP *it = &g.pickups[i];
		if (it->active && it->type == PICK_QUEST && explore_seen(it->pos.vx / TILE_SIZE, it->pos.vz / TILE_SIZE))
			map_mark(ox, oy, scale, it->pos.vx, it->pos.vz, 4, 200, 100, 255);
	}
	render_sprite(&tex_mapa_menu_t, 0, 0, w < 128 ? w : 128, h < 128 ? h : 128, ox, oy, w * scale, h * scale);

	/* legenda à direita */
	int lx = ox + 140, y = oy;
	render_rect(lx, y + 1, 6, 6, 80, 160, 255, 0);   hud_print(lx + 10, y, "VOCE");      y += 12;
	render_rect(lx, y + 1, 6, 6, 120, 230, 255, 0);  hud_print(lx + 10, y, "SAIDA");     y += 12;
	render_rect(lx, y + 1, 6, 6, 200, 100, 255, 0);  hud_print(lx + 10, y, "RELIQUIA");  y += 12;
	render_rect(lx, y + 1, 6, 6, 150, 115, 70, 0);   hud_print(lx + 10, y, "TRILHA");    y += 12;
	render_rect(lx, y + 1, 6, 6, 25, 55, 25, 0);     hud_print(lx + 10, y, "MATA");      y += 12;
	render_rect(lx, y + 1, 6, 6, 170, 120, 60, 0);   hud_print(lx + 10, y, "TRONCO");    y += 20;
	if (map_row >= 128) hud_print(lx, y, "VISTO %d%%", map_seen_pct);
}

/* O mundo já foi desenhado; por cima: escurece, painel, abas e a aba atual */
void menu_draw(void) {
	static const char *const help[NUM_TABS] = {
		"L1/R1 ABA   O SAIR",
		"L1/R1 ABA   O SAIR",
		"L1/R1 ABA   O SAIR",
		"X EQUIPAR   L1/R1 ABA   O SAIR",
		"X USAR  QUAD LARGAR  O SAIR",
	};
	draw_tabs();
	switch (g.menu_tab) {
	case TAB_MAP:     menu_draw_map(); break;
	case TAB_GOAL:    draw_goal(); break;
	case TAB_STATS:   draw_stats(); break;
	case TAB_WEAPONS: draw_weapons(); break;
	case TAB_ITEMS:   draw_items(); break;
	}
	hud_print(PX + 8, PY + PH + 8, "%s", help[g.menu_tab]);
	if (g.num_players > 1)
		hud_print(SCREEN_W - 96, PY + PH + 8, "P%d SELECT", g.menu_player + 1);
	if (g.message_timer > 0)                      /* resposta de usar/largar */
		hud_print(SCREEN_W / 2 - (int)strlen(g.message) * 4, PY - 2 + PH - 10, "%s", g.message);
	/* painéis por último = desenhados primeiro (por baixo dos destaques) */
	render_rect(PX, PY, PW, PH, 10, 12, 20, 1);          /* painel (mistura 50%) */
	render_rect(0, 0, SCREEN_W, SCREEN_H, 0, 0, 0, 1);   /* escurece o mundo */
}
