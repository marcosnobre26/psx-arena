/*
 * playlog.c - Registro de partida para balancear o combate (etapa 07b)
 *
 * Só existe com DEBUG_PLAYLOG (./dev run compila assim em build-debug/;
 * ./dev run --sem-registro abre o build normal). Os ganchos PL(...) nos
 * pontos de lógica (dano, morte, item, munição, golpe, grande) só CONTAM:
 * os contadores ficam aqui, FORA de g, e nada daqui escreve em g. Por
 * isso a lógica não muda (./dev replay-test -D DEBUG_PLAYLOG=1 dá IGUAL).
 *
 * Saída no TTY (o ./dev run grava em logs/partida-AAAAMMDD-HHMM.txt):
 *   PLAYLOG RESUMO fase=1 t=30 ...     a cada 30 s de jogo (em passos)
 *   PLAYLOG FIM_FASE fase=1 motivo=vitoria|morte|pulou ...
 * Formato chave=valor; cada registro em várias linhas (PLAYLOG MAIS ...),
 * porque o DuckStation corta linhas longas do TTY.
 */
#ifdef DEBUG_PLAYLOG
#include <stdio.h>
#include <string.h>
#include "game.h"

#define NE 8                                   /* tipos de inimigo (folga) */
static struct {
	int level, character, active;
	int hp_min, deaths, killed_by[NE];
	int heals_magic, heals_item;
	int mana_spent[NUM_WEAPON_SLOTS];          /* por arma (magias) */
	int found[3], used[3];                     /* flechas, balas, mana: achado / usado */
	int arrows_picked;                         /* flechas recolhidas do chão */
	int uses[NUM_WEAPON_SLOTS];                /* golpes/tiros/magias por arma */
	int kills[NE][NUM_WEAPON_SLOTS + 1];       /* por tipo x arma (+1 = outro) */
	int big_escapes;
	int battery_min, batteries;
	int dmg_taken, dmg_by[NE];                 /* dano recebido: total e por tipo de inimigo */
	int last_summary;
} pl;
static int attack_weapon = -1;                 /* arma do golpe/tiro em andamento */
static void dump(const char *kind, const char *reason);

/* Nome como chave: espaço vira _ ("BOLA DE FOGO" -> "BOLA_DE_FOGO") */
static const char *key(const char *name) {
	static char buf[4][24];
	static int k;
	char *b = buf[k = (k + 1) & 3];
	int i = 0;
	for (; name[i] && i < 23; i++) b[i] = name[i] == ' ' ? '_' : name[i];
	b[i] = 0;
	return b;
}

static int ammo_index(int ammo) {
	return ammo == AMMO_ARROW ? 0 : ammo == AMMO_BULLET ? 1 : ammo == AMMO_MANA ? 2 : -1;
}

void pl_begin(void) {
	memset(&pl, 0, sizeof(pl));
	pl.level = g.level;
	pl.character = g.players[0].character;
	pl.hp_min = g.players[0].hp;
	pl.battery_min = g.players[0].battery;
	pl.active = 1;
	pl.last_summary = 0;
	printf("PLAYLOG INICIO fase=%d semente=%08x personagem=%s\n", g.level + 1,
	       (unsigned)g.seed, key(character_defs[pl.character].name));
}

void pl_attack(int weapon) { attack_weapon = weapon; }

static int damage_source = -1;                 /* tipo do inimigo que vai bater */
void pl_source(int enemy_type) { damage_source = enemy_type; }

void pl_damage(const PLAYER *p, int amount) {
	int enemy_type = damage_source;
	damage_source = -1;
	if (p->index != 0) return;                 /* 1 jogador (etapa 07b) */
	pl.dmg_taken += amount;
	if (enemy_type >= 0 && enemy_type < NE) pl.dmg_by[enemy_type] += amount;
	if (p->hp < pl.hp_min) pl.hp_min = p->hp < 0 ? 0 : p->hp;
	if (p->hp <= 0) {
		pl.deaths++;
		if (enemy_type >= 0 && enemy_type < NE) pl.killed_by[enemy_type]++;
	}
}

void pl_heal(int magic) {
	if (magic) pl.heals_magic++;
	else       pl.heals_item++;
}

void pl_use(const PLAYER *p, int weapon) {
	if (p->index != 0) return;
	pl.uses[weapon]++;
	const WEAPON_DEF *w = &weapon_defs[weapon];
	int a = ammo_index(w->ammo);
	if (a >= 0) pl.used[a] += w->cost;
	if (w->ammo == AMMO_MANA) pl.mana_spent[weapon] += w->cost;
}

void pl_found(const PLAYER *p, int ammo, int amount) {
	if (p->index != 0) return;
	int a = ammo_index(ammo);
	if (a >= 0) pl.found[a] += amount;
}

void pl_arrow_picked(const PLAYER *p) {
	if (p->index != 0) return;
	pl.arrows_picked++;
	pl.found[0]++;
}

void pl_battery(const PLAYER *p) {
	if (p->index == 0) pl.batteries++;
}

void pl_kill(int enemy_type) {
	int w = attack_weapon >= 0 && attack_weapon < NUM_WEAPON_SLOTS ? attack_weapon : NUM_WEAPON_SLOTS;
	if (enemy_type >= 0 && enemy_type < NE) pl.kills[enemy_type][w]++;
}

void pl_big_escape(void) { pl.big_escapes++; }

/* Resumo ou fim de fase. O DuckStation corta linhas longas do TTY, então
 * sai em várias linhas curtas: a primeira com o tipo, as outras com
 * "PLAYLOG MAIS" (o ./dev analisar-partida junta tudo num registro só). */
static void dump(const char *kind, const char *reason) {
	const PLAYER *p = &g.players[0];
	printf("PLAYLOG %s fase=%d t=%d%s%s vida=%d vida_min=%d dano=%d mortes=%d",
	       kind, pl.level + 1, g.play_frames / 60, reason ? " motivo=" : "", reason ? reason : "",
	       p->hp, pl.hp_min, pl.dmg_taken, pl.deaths);
	for (int t = 0; t < num_enemy_types && t < NE; t++)
		if (pl.killed_by[t]) printf(" morto_por_%s=%d", key(enemy_defs[t].name), pl.killed_by[t]);
	for (int t = 0; t < num_enemy_types && t < NE; t++)
		if (pl.dmg_by[t]) printf(" dano_de_%s=%d", key(enemy_defs[t].name), pl.dmg_by[t]);
	printf(" curas_magia=%d curas_item=%d\n", pl.heals_magic, pl.heals_item);

	printf("PLAYLOG MAIS flechas_achadas=%d flechas_usadas=%d flechas_sobra=%d flechas_recolhidas=%d",
	       pl.found[0], pl.used[0], p->arrows, pl.arrows_picked);
	printf(" balas_achadas=%d balas_usadas=%d balas_sobra=%d", pl.found[1], pl.used[1], p->bullets);
	printf(" mana_achada=%d mana_usada=%d mana_sobra=%d\n", pl.found[2], pl.used[2], p->energy);

	printf("PLAYLOG MAIS");
	for (int w = 0; w < num_weapons; w++) {
		if (pl.uses[w]) printf(" usos_%s=%d", key(weapon_defs[w].name), pl.uses[w]);
		if (pl.mana_spent[w]) printf(" mana_%s=%d", key(weapon_defs[w].name), pl.mana_spent[w]);
	}
	printf("\nPLAYLOG MAIS");
	for (int t = 0; t < num_enemy_types && t < NE; t++)
		for (int w = 0; w <= num_weapons; w++)
			if (pl.kills[t][w])
				printf(" mortos_%s_%s=%d", key(enemy_defs[t].name), w < num_weapons ? key(weapon_defs[w].name) : "OUTRO",
				       pl.kills[t][w]);
	printf(" inimigos_vivos=%d\n", g.enemies_left);

	printf("PLAYLOG MAIS fugas_do_grande=%d pilha=%d pilha_min=%d pilhas=%d", pl.big_escapes, p->battery,
	       pl.battery_min, pl.batteries);
	int left[NUM_PICKUP_TYPES] = { 0 };            /* itens que ficaram no mapa */
	for (int i = 0; i < MAX_PICKUPS; i++)
		if (g.pickups[i].active) left[g.pickups[i].type]++;
	static const char *const pick_names[NUM_PICKUP_TYPES] = {
		"gema", "bandagem", "pocao", "arma", "missao", "pilha", "chave", "flechas", "balas", "kit" };
	for (int k = 0; k < NUM_PICKUP_TYPES; k++)
		if (left[k]) printf(" nao_pegos_%s=%d", pick_names[k], left[k]);
	printf("\n");
}

/* Um passo (depois do game_tick): mínimo da vida/pilha, resumo a cada 30 s */
void pl_tick(void) {
	if (!pl.active) return;
	if (g.state == STATE_WIN)  { dump("FIM_FASE", "vitoria"); pl.active = 0; return; }
	if (g.state == STATE_DEAD) { dump("FIM_FASE", "morte");   pl.active = 0; return; }
	if (g.state != STATE_PLAY) return;
	const PLAYER *p = &g.players[0];
	if (p->active && p->hp < pl.hp_min) pl.hp_min = p->hp < 0 ? 0 : p->hp;
	if (p->battery < pl.battery_min) pl.battery_min = p->battery;
	if (g.play_frames > 0 && g.play_frames % (30 * 60) == 0 && g.play_frames != pl.last_summary) {
		pl.last_summary = g.play_frames;
		dump("RESUMO", NULL);
	}
}

void pl_end(const char *reason) {
	if (!pl.active) return;
	dump("FIM_FASE", reason);
	pl.active = 0;
}
#endif
