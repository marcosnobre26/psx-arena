/*
 * weapons.c - Disparo e projéteis
 *
 * As características de cada arma ficam na tabela weapon_defs (data.c).
 * Aqui está só a lógica: criar projéteis, movê-los e testar colisões.
 */
#include <stdlib.h>
#include "game.h"
#include "models.h"

#define BULLET_HEIGHT  -150   /* altura do tiro (relativa ao pé) */
#define AUTO_AIM_CONE   300   /* ~26 graus para cada lado */
#define AUTO_AIM_DIST  2600

static void spawn_bullet(int x, int y, int z, int angle, const WEAPON_DEF *w, int widx) {
	for (int i = 0; i < MAX_BULLETS; i++) {
		BULLET *b = &g.bullets[i];
		if (b->active) continue;
		b->active = 1;
		b->pos.vx = x; b->pos.vy = y; b->pos.vz = z;
		b->vx = (isin(angle) * w->speed) >> 12;
		b->vz = (icos(angle) * w->speed) >> 12;
		b->life = w->life;
		b->damage = w->damage;
		b->weapon = widx;
		return;
	}
}

void weapon_fire(PLAYER *p) {
	const WEAPON_DEF *w = &weapon_defs[p->weapon];

	/* mira automática: se houver inimigo à frente, mira nele */
	int aim = p->angle;
	ENEMY *target = enemy_nearest_in_cone(p->pos.vx, p->pos.vz, p->angle, AUTO_AIM_CONE, AUTO_AIM_DIST);
	if (target)
		aim = angle_of(target->pos.vx - p->pos.vx, target->pos.vz - p->pos.vz);

	/* sai um pouco à frente do jogador, na altura da arma */
	int sx = p->pos.vx + ((isin(p->angle) * 90) >> 12);
	int sz = p->pos.vz + ((icos(p->angle) * 90) >> 12);
	int sy = p->pos.vy + BULLET_HEIGHT;

	for (int k = 0; k < w->pellets; k++) {
		int a = aim;
		if (w->pellets > 1)
			a += ((2 * k - (w->pellets - 1)) * w->spread) / 2;  /* leque centralizado */
		if (w->jitter)
			a += rand_range(-w->jitter, w->jitter);
		spawn_bullet(sx, sy, sz, a & 4095, w, p->weapon);
	}
	p->cooldown = w->cooldown;
	sound_play_at(&sfx_tiro, sx, sz, VOL_TIRO);
}

void bullets_update(void) {
	for (int i = 0; i < MAX_BULLETS; i++) {
		BULLET *b = &g.bullets[i];
		if (!b->active) continue;

		b->pos.vx += b->vx;
		b->pos.vz += b->vz;

		if (--b->life <= 0) { b->active = 0; continue; }

		/* bateu na parede, numa caixa ou num objeto de cenário? */
		int cx = b->pos.vx / TILE_SIZE, cz = b->pos.vz / TILE_SIZE;
		int wall = b->pos.vx < 0 || b->pos.vz < 0 || level_cell_solid(cx, cz);
		if (wall || collide_prop_at(b->pos.vx, b->pos.vz, 24)) {
			if (wall)
				crate_damage(cx, cz, b->damage);
			sound_play_at(&sfx_acerto, b->pos.vx, b->pos.vz, VOL_PAREDE);
			effect_spawn(FX_BURST, b->pos.vx - b->vx, b->pos.vz - b->vz, 90, 8,
			             weapon_defs[b->weapon].color.r, weapon_defs[b->weapon].color.g,
			             weapon_defs[b->weapon].color.b);
			b->active = 0;
			continue;
		}

		/* acertou um inimigo? */
		for (int j = 0; j < MAX_ENEMIES; j++) {
			ENEMY *e = &g.enemies[j];
			if (!e->active) continue;
			int r = ((90 * enemy_defs[e->type].scale) >> 12) + 30;
			int dx = e->pos.vx - b->pos.vx, dz = e->pos.vz - b->pos.vz;
			if (abs(dx) < r && abs(dz) < r) {
				enemy_damage(e, b->damage, b->vx / 3, b->vz / 3);
				b->active = 0;
				break;
			}
		}
	}
}

void bullets_draw(void) {
	DRAWOPT opt = { 0 };
	opt.flags = DRAW_UNLIT;

	for (int i = 0; i < MAX_BULLETS; i++) {
		BULLET *b = &g.bullets[i];
		if (!b->active) continue;
		const WEAPON_DEF *w = &weapon_defs[b->weapon];

		opt.palette  = &w->color;
		opt.npalette = 1;
		SVECTOR rot = { g.frame * 90, g.frame * 130, 0 };
		render_mesh(&bullet_mesh, &b->pos, &rot, w->scale, &opt);
	}
}
