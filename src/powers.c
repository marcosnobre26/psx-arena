/*
 * powers.c - Poderes especiais (gastam energia)
 *
 * Para criar um poder novo:
 *   1. Escreva uma função  void power_xxx(PLAYER *p)  aqui
 *      (p = jogador que usou o poder)
 *   2. Declare e adicione à tabela power_defs em data.c
 * Pronto: R2 já alterna para ele e CÍRCULO usa.
 */
#include "game.h"

#define SHOCK_RADIUS 900

void power_use(PLAYER *p) {
	const POWER_DEF *pw = &power_defs[p->power];
	if (p->energy < pw->cost) {
		show_message("SEM ENERGIA", 40);
		return;
	}
	p->energy -= pw->cost;
	pw->use(p);
}

/* Onda de choque: dano e empurrão em todos os inimigos por perto */
void power_shockwave(PLAYER *p) {
	effect_spawn(FX_SHOCKWAVE, p->pos.vx, p->pos.vz, SHOCK_RADIUS, 22, 120, 200, 255);

	for (int i = 0; i < MAX_ENEMIES; i++) {
		ENEMY *e = &g.enemies[i];
		if (!e->active) continue;
		int dx = e->pos.vx - p->pos.vx, dz = e->pos.vz - p->pos.vz;
		if (dist2d(dx, dz) < SHOCK_RADIUS) {
			int a = angle_of(dx, dz);
			enemy_damage(e, 3, (isin(a) * 60) >> 12, (icos(a) * 60) >> 12);
		}
	}
}

/* Dash: corrida rápida e invulnerável na direção em que olha */
void power_dash(PLAYER *p) {
	p->dash_timer = 12;
	effect_spawn(FX_BURST, p->pos.vx, p->pos.vz, 200, 10, 255, 255, 255);
}

/* Cura: recupera vida de quem usou (e do parceiro, se estiver perto) */
void power_heal(PLAYER *p) {
	for (int i = 0; i < MAX_PLAYERS; i++) {
		PLAYER *o = &g.players[i];
		if (!player_alive(o)) continue;
		if (o != p && dist2d(o->pos.vx - p->pos.vx, o->pos.vz - p->pos.vz) > 600) continue;
		o->hp += 35;
		if (o->hp > o->max_hp) o->hp = o->max_hp;
		effect_spawn(FX_HEAL, o->pos.vx, o->pos.vz, 300, 30, 80, 255, 120);
	}
}
