/*
 * enemies.c - Inimigos: perseguem o jogador mais próximo, causam dano ao encostar
 *
 * Ideias para praticar:
 *   - um inimigo que atira (reaproveite weapons.c com uma flag "do inimigo")
 *   - um inimigo que foge quando está com pouca vida
 *   - um chefe com mais vida e um ataque de onda de choque
 */
#include <stdlib.h>
#include "game.h"
#include "models.h"

#define CHASE_DIST   3600   /* distância em que o inimigo "vê" o jogador */
#define ENEMY_RADIUS 90     /* raio de colisão com tamanho ONE */

int enemy_radius(const ENEMY *e) {
	return (ENEMY_RADIUS * enemy_defs[e->type].scale) >> 12;
}

void enemy_spawn(int type, int x, int z) {
	for (int i = 0; i < MAX_ENEMIES; i++) {
		ENEMY *e = &g.enemies[i];
		if (e->active) continue;
		e->active = 1;
		e->type = type;
		e->hp = enemy_defs[type].hp;
		e->pos.vx = x; e->pos.vy = 0; e->pos.vz = z;
		e->angle = rng_next(&g.rng) & 4095;
		e->flash = e->kx = e->kz = 0;
		e->think = rand_range(30, 120);
		e->hit_cooldown = 0;
		g.enemies_left++;
		return;
	}
}

void enemy_damage(ENEMY *e, int amount, int push_x, int push_z) {
	e->hp -= amount;
	e->flash = 6;
	e->kx += push_x;
	e->kz += push_z;

	if (e->hp <= 0) {
		const ENEMY_DEF *d = &enemy_defs[e->type];
		e->active = 0;
		g.enemies_left--;
		g.score += d->score;
		effect_spawn(FX_BURST, e->pos.vx, e->pos.vz, (300 * d->scale) >> 12, 18,
		             d->palette[0].r, d->palette[0].g, d->palette[0].b);

		/* chance de soltar um item */
		int r = rng_next(&g.rng) % 100;
		if (r < 20)      pickup_spawn(PICK_HEALTH, e->pos.vx, e->pos.vz);
		else if (r < 40) pickup_spawn(PICK_ENERGY, e->pos.vx, e->pos.vz);

		if (g.enemies_left == 0)
			show_message("ARENA LIMPA!", 90);
	}
}

void enemies_update(void) {
	for (int i = 0; i < MAX_ENEMIES; i++) {
		ENEMY *e = &g.enemies[i];
		if (!e->active) continue;
		const ENEMY_DEF *d = &enemy_defs[e->type];
		int rad = enemy_radius(e);

		/* alvo: o jogador vivo mais perto (com 2 jogadores, divide a atenção) */
		int dist = 0x7fffffff;
		PLAYER *p = player_nearest(e->pos.vx, e->pos.vz, &dist);
		int chasing = p && dist < CHASE_DIST && g.state == STATE_PLAY;

		/* --- decidir direção --- */
		if (chasing) {
			e->angle = turn_towards(e->angle, angle_of(p->pos.vx - e->pos.vx, p->pos.vz - e->pos.vz), 96);
		} else if (--e->think <= 0) {
			e->angle = (e->angle + rand_range(-900, 900)) & 4095;
			e->think = rand_range(40, 140);
		}

		/* --- mover com colisão contra paredes, cenário, jogadores e outros
		 *     inimigos (ninguém atravessa ninguém) --- */
		int sp = chasing ? d->speed : d->speed / 2;
		int mx = ((isin(e->angle) * sp) >> 12) + e->kx;
		int mz = ((icos(e->angle) * sp) >> 12) + e->kz;
		e->kx = e->kx * 3 / 4;     /* empurrão vai diminuindo */
		e->kz = e->kz * 3 / 4;

		int blocked = collide_move(&e->pos, mx, mz, rad, e, COL_ALL);
		if (blocked && !chasing)
			e->angle = (e->angle + 1024 + (rng_next(&g.rng) & 1023)) & 4095;
		else if (blocked && chasing && (g.frame & 31) == (i & 31))
			e->angle = (e->angle + ((rng_next(&g.rng) & 1) ? 700 : -700)) & 4095;  /* contorna */

		/* --- encostou no jogador? (quem está pulando por cima escapa) ---
		 * Como os dois não se atravessam, "encostar" é chegar a poucas
		 * unidades da soma dos raios. */
		if (e->hit_cooldown > 0) e->hit_cooldown--;
		if (p && dist <= rad + PLAYER_RADIUS + 16 && p->pos.vy > -140 && e->hit_cooldown == 0) {
			player_damage(p, d->damage, e->pos.vx, e->pos.vz);
			e->hit_cooldown = 30;
		}

		if (e->flash > 0) e->flash--;
	}
}

void enemies_draw(void) {
	for (int i = 0; i < MAX_ENEMIES; i++) {
		ENEMY *e = &g.enemies[i];
		if (!e->active) continue;
		const ENEMY_DEF *d = &enemy_defs[e->type];

		DRAWOPT opt = { 0 };
		opt.palette  = d->palette;
		opt.npalette = 3;
		if (e->flash & 2) opt.flags |= DRAW_FLASH;

		/* pulinhos enquanto anda */
		VECTOR pos = e->pos;
		int hop = isin((g.frame * 128 + i * 700) & 4095);
		pos.vy = -((hop < 0 ? -hop : hop) >> 8);

		SVECTOR rot = { 0, e->angle, 0 };
		render_mesh(d->mesh, &pos, &rot, d->scale, &opt);
		shadow_draw(&e->pos, d->scale);
	}
}

/* Inimigo mais próximo dentro de um cone (usado na mira automática) */
ENEMY *enemy_nearest_in_cone(int x, int z, int angle, int cone, int max_dist) {
	ENEMY *best = NULL;
	int best_d = max_dist;
	for (int i = 0; i < MAX_ENEMIES; i++) {
		ENEMY *e = &g.enemies[i];
		if (!e->active) continue;
		int dx = e->pos.vx - x, dz = e->pos.vz - z;
		int d = dist2d(dx, dz);
		if (d >= best_d) continue;
		if (abs(angle_diff(angle, angle_of(dx, dz))) > cone) continue;
		best = e;
		best_d = d;
	}
	return best;
}
