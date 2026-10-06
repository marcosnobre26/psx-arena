/*
 * enemies.c - Inimigos: perseguem o jogador mais próximo, causam dano ao encostar
 *
 * Atiradores (enemy_defs com arma >= 0): perseguem até o alcance, param
 * (ou recuam se o jogador chegar perto), e quando enxergam o alvo fazem
 * um "aviso" piscando antes de cada tiro — tempo para o jogador reagir.
 *
 * Ideias para praticar:
 *   - um atirador que anda de lado (strafe) em vez de ficar parado
 *   - um inimigo que foge quando está com pouca vida
 *   - um chefe com mais vida e um ataque de onda de choque
 */
#include <stdlib.h>
#include "game.h"
#include "models.h"

#define CHASE_DIST   3600   /* distância em que o inimigo "vê" o jogador */
#define ENEMY_RADIUS 90     /* raio de colisão com tamanho ONE */
#define WINDUP_TIME  20     /* passos parado e piscando antes de atirar (1/3 s) */
#define LOS_STEP     64     /* passo da checagem de linha de visão */

int enemy_radius(const ENEMY *e) {
	return (ENEMY_RADIUS * enemy_defs[e->type].scale) >> 12;
}

/* Linha de visão: anda pela reta de LOS_STEP em LOS_STEP unidades
 * procurando parede, caixa ou objeto sólido — os mesmos obstáculos que
 * param um tiro. Sem isso o atirador gastaria tiros na parede (e o pool
 * de projéteis, que é dividido com o jogador). */
static int line_of_sight(int x0, int z0, int x1, int z1) {
	int dx = x1 - x0, dz = z1 - z0;
	int steps = dist2d(dx, dz) / LOS_STEP;
	for (int s = 1; s < steps; s++) {
		int x = x0 + dx * s / steps;
		int z = z0 + dz * s / steps;
		if (level_cell_solid(x / TILE_SIZE, z / TILE_SIZE) || collide_prop_at(x, z, 24))
			return 0;
	}
	return 1;
}

void enemy_spawn(int type, int x, int z) {
	for (int i = 0; i < MAX_ENEMIES; i++) {
		ENEMY *e = &g.enemies[i];
		if (e->active) continue;
		e->active = 1;
		e->type = type;
		e->hp = enemy_defs[type].hp;
		e->pos.vx = x; e->pos.vy = 0; e->pos.vz = z;
		e->angle = rand() & 4095;
		e->flash = e->kx = e->kz = 0;
		e->think = rand_range(30, 120);
		e->hit_cooldown = 0;
		e->shot_timer = rand_range(60, 150);  /* não atiram todos juntos */
		e->windup = e->sees = 0;
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
		int r = rand() % 100;
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

		/* --- atirador: enxerga o alvo? ---
		 * A linha de visão custa ~30 testes, então cada inimigo só refaz a
		 * conta a cada 8 passos, e cada um num passo diferente (+ i). */
		int shooter = d->weapon >= 0;
		if (!shooter || !chasing)
			e->sees = 0;
		else if (((g.frame + i) & 7) == 0)
			e->sees = line_of_sight(e->pos.vx, e->pos.vz, p->pos.vx, p->pos.vz);
		int aiming = shooter && chasing && e->sees && dist < d->range;

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
		if (aiming) {
			if (e->windup == 0 && dist < d->range / 2)
				sp = -d->speed / 2;   /* jogador perto demais: recua de frente para ele */
			else
				sp = 0;               /* na distância certa (ou mirando): fica parado */
		}
		int mx = ((isin(e->angle) * sp) >> 12) + e->kx;
		int mz = ((icos(e->angle) * sp) >> 12) + e->kz;
		e->kx = e->kx * 3 / 4;     /* empurrão vai diminuindo */
		e->kz = e->kz * 3 / 4;

		int blocked = collide_move(&e->pos, mx, mz, rad, e, COL_ALL);
		if (blocked && !chasing)
			e->angle = (e->angle + 1024 + (rand() & 1023)) & 4095;
		else if (blocked && chasing && (g.frame & 31) == (i & 31))
			e->angle = (e->angle + ((rand() & 1) ? 700 : -700)) & 4095;  /* contorna */

		/* --- encostou no jogador? (quem está pulando por cima escapa) ---
		 * Como os dois não se atravessam, "encostar" é chegar a poucas
		 * unidades da soma dos raios. */
		if (e->hit_cooldown > 0) e->hit_cooldown--;
		if (p && dist <= rad + PLAYER_RADIUS + 16 && p->pos.vy > -140 && e->hit_cooldown == 0) {
			player_damage(p, d->damage, e->pos.vx, e->pos.vz);
			e->hit_cooldown = 30;
		}

		/* --- atirar: espera a recarga, avisa (windup) e dispara ---
		 * Atira na direção em que está olhando; como turn_towards continua
		 * girando durante o aviso, o tiro sai mirado no jogador, mas quem
		 * se mexe de lado depois do disparo escapa (o tiro é lento). */
		if (aiming) {
			if (e->windup > 0) {
				if (--e->windup == 0) {
					enemy_fire(e, e->angle);
					e->shot_timer = enemy_weapon_defs[d->weapon].cooldown;
				}
			} else if (--e->shot_timer <= 0) {
				e->windup = WINDUP_TIME;
			}
		} else {
			e->windup = 0;   /* perdeu o alvo de vista: cancela o aviso */
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
		if (e->windup & 4) opt.flags |= DRAW_FLASH;   /* aviso: vai atirar! */

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
