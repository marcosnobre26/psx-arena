/*
 * weapons.c - Disparo e projéteis
 *
 * As características de cada arma ficam nas tabelas weapon_defs (jogador) e
 * enemy_weapon_defs (inimigos), em data.c. Aqui está só a lógica: criar
 * projéteis, movê-los e testar colisões.
 *
 * Todo projétil tem um "dono" (owner): tiro do jogador só acerta inimigos e
 * tiro de inimigo só acerta jogadores. Assim o mesmo pool g.bullets serve
 * para os dois lados, sem fogo amigo.
 */
#include <stdlib.h>
#include "game.h"
#include "models.h"

#define BULLET_HEIGHT  -150   /* altura do tiro (relativa ao pé) */
#define AUTO_AIM_CONE   300   /* ~26 graus para cada lado */
#define AUTO_AIM_DIST  2600

/* A definição da arma de um projétil depende de quem atirou */
static const WEAPON_DEF *bullet_def(const BULLET *b) {
	return b->owner == OWNER_ENEMY ? &enemy_weapon_defs[b->weapon] : &weapon_defs[b->weapon];
}

static void spawn_bullet(int x, int y, int z, int angle, const WEAPON_DEF *w, int widx, int owner) {
	for (int i = 0; i < MAX_BULLETS; i++) {
		BULLET *b = &g.bullets[i];
		if (b->active) continue;
		b->active = 1;
		b->owner = owner;
		b->pos.vx = x; b->pos.vy = y; b->pos.vz = z;
		b->vx = (isin(angle) * w->speed) >> 12;
		b->vz = (icos(angle) * w->speed) >> 12;
		b->life = w->life;
		b->damage = w->damage;
		b->weapon = widx;
		return;
	}
}

/* Dispara os projéteis de uma arma (leque e imprecisão inclusos).
 * Comum ao jogador e aos inimigos: só muda a tabela e o dono. */
static void fire_pattern(int x, int y, int z, int aim, const WEAPON_DEF *w, int widx, int owner) {
	for (int k = 0; k < w->pellets; k++) {
		int a = aim;
		if (w->pellets > 1)
			a += ((2 * k - (w->pellets - 1)) * w->spread) / 2;  /* leque centralizado */
		if (w->jitter)
			a += rand_range(-w->jitter, w->jitter);
		spawn_bullet(x, y, z, a & 4095, w, widx, owner);
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

	fire_pattern(sx, sy, sz, aim, w, p->weapon, OWNER_PLAYER);
	p->cooldown = w->cooldown;
}

/* Tiro de inimigo: quem decide QUANDO atirar é a IA (enemies.c);
 * aqui só criamos os projéteis na direção pedida. */
void enemy_fire(ENEMY *e, int angle) {
	const ENEMY_DEF *d = &enemy_defs[e->type];
	const WEAPON_DEF *w = &enemy_weapon_defs[d->weapon];

	/* sai da frente do corpo (o raio cresce com o tamanho do inimigo) */
	int ahead = (100 * d->scale) >> 12;
	int sx = e->pos.vx + ((isin(angle) * ahead) >> 12);
	int sz = e->pos.vz + ((icos(angle) * ahead) >> 12);

	fire_pattern(sx, BULLET_HEIGHT, sz, angle, w, d->weapon, OWNER_ENEMY);
}

/* Tiro do jogador: acertou algum inimigo? */
static int hit_enemies(BULLET *b) {
	for (int j = 0; j < MAX_ENEMIES; j++) {
		ENEMY *e = &g.enemies[j];
		if (!e->active) continue;
		int r = ((90 * enemy_defs[e->type].scale) >> 12) + 30;
		int dx = e->pos.vx - b->pos.vx, dz = e->pos.vz - b->pos.vz;
		if (abs(dx) < r && abs(dz) < r) {
			enemy_damage(e, b->damage, b->vx / 3, b->vz / 3);
			return 1;
		}
	}
	return 0;
}

/* Tiro de inimigo: acertou algum jogador? */
static int hit_players(BULLET *b) {
	for (int j = 0; j < MAX_PLAYERS; j++) {
		PLAYER *p = &g.players[j];
		if (!player_alive(p)) continue;
		/* pulou alto o bastante para o tiro passar por baixo dos pés?
		 * (Y negativo é para cima: pé acima do tiro = pos.vy < b->pos.vy) */
		if (p->pos.vy <= b->pos.vy) continue;
		int r = PLAYER_RADIUS + 24;
		int dx = p->pos.vx - b->pos.vx, dz = p->pos.vz - b->pos.vz;
		if (abs(dx) < r && abs(dz) < r) {
			/* "origem" do dano = um passo atrás do tiro, para empurrar na
			 * direção em que ele vinha */
			player_damage(p, b->damage, b->pos.vx - b->vx, b->pos.vz - b->vz);
			return 1;
		}
	}
	return 0;
}

void bullets_update(void) {
	for (int i = 0; i < MAX_BULLETS; i++) {
		BULLET *b = &g.bullets[i];
		if (!b->active) continue;
		const WEAPON_DEF *w = bullet_def(b);

		b->pos.vx += b->vx;
		b->pos.vz += b->vz;

		if (--b->life <= 0) { b->active = 0; continue; }

		/* bateu na parede, numa caixa ou num objeto de cenário? */
		int cx = b->pos.vx / TILE_SIZE, cz = b->pos.vz / TILE_SIZE;
		int wall = b->pos.vx < 0 || b->pos.vz < 0 || level_cell_solid(cx, cz);
		if (wall || collide_prop_at(b->pos.vx, b->pos.vz, 24)) {
			if (wall)
				crate_damage(cx, cz, b->damage);
			effect_spawn(FX_BURST, b->pos.vx - b->vx, b->pos.vz - b->vz, 90, 8,
			             w->color.r, w->color.g, w->color.b);
			b->active = 0;
			continue;
		}

		/* acertou alguém do outro lado? */
		if (b->owner == OWNER_ENEMY ? hit_players(b) : hit_enemies(b))
			b->active = 0;
	}
}

void bullets_draw(void) {
	DRAWOPT opt = { 0 };
	opt.flags = DRAW_UNLIT;

	for (int i = 0; i < MAX_BULLETS; i++) {
		BULLET *b = &g.bullets[i];
		if (!b->active) continue;
		const WEAPON_DEF *w = bullet_def(b);

		opt.palette  = &w->color;
		opt.npalette = 1;
		SVECTOR rot = { g.frame * 90, g.frame * 130, 0 };
		render_mesh(&bullet_mesh, &b->pos, &rot, w->scale, &opt);
	}
}
