/*
 * weapons.c - Armas por TIPO (etapa 07), projéteis, barulho e flechas no chão
 *
 * Os números de cada arma ficam em weapon_defs (data.c); aqui está só a
 * lógica de cada tipo:
 *   WK_MELEE  espada/machado: □ começa um golpe; na janela de acerto
 *             (hit_from..hit_to passos) quem está no arco à frente leva dano,
 *             uma vez por golpe. Sem projétil, sem munição, SILENCIOSO.
 *   WK_BOW    arco: segurar □ carrega; soltar atira a flecha (munição). Mais
 *             carga = mais dano e alcance. A flecha às vezes fica no chão.
 *   WK_MAGIC  gasta mana: bola de fogo (projétil), clarão (atordoa e assusta
 *             em volta) e cura. Substituem os antigos "poderes".
 *   WK_GUN    pistola/espingarda: munição rara, mira automática curta e
 *             MUITO barulho (atrai inimigos de longe).
 *
 * Tudo é lógica (muda g): só g.in, passos e g.rng. Sem fogo amigo: projéteis
 * e golpes só acertam inimigos.
 */
#include <stdlib.h>
#include <psxpad.h>
#include "game.h"
#include "models.h"

#define BULLET_HEIGHT  -150   /* altura do tiro (relativa ao pé) */
#define AUTO_AIM_DIST  2600

/* ------------------------------------------------------------------ */
/* Barulho: os inimigos (enemies.c) leem a lista no passo e ela é zerada */
/* ------------------------------------------------------------------ */

/* Guarda os MAX_NOISES mais fortes do passo (sem alocação): cheia, o novo
 * entra no lugar do mais fraco se for mais forte. */
void noise_emit(int x, int z, int cells) {
	if (cells <= 0) return;
	int r = cells * TILE_SIZE;
	if (g.nnoise < MAX_NOISES) {
		g.noise[g.nnoise].x = x; g.noise[g.nnoise].z = z; g.noise[g.nnoise].r = r;
		g.nnoise++;
		return;
	}
	int weak = 0;
	for (int i = 1; i < MAX_NOISES; i++)
		if (g.noise[i].r < g.noise[weak].r) weak = i;
	if (g.noise[weak].r < r) {
		g.noise[weak].x = x; g.noise[weak].z = z; g.noise[weak].r = r;
	}
}

/* ------------------------------------------------------------------ */
/* Munição                                                              */
/* ------------------------------------------------------------------ */

static int *ammo_of(PLAYER *p, int ammo) {
	switch (ammo) {
	case AMMO_ARROW:  return &p->arrows;
	case AMMO_BULLET: return &p->bullets;
	case AMMO_MANA:   return &p->energy;
	}
	return NULL;
}

static int has_ammo(PLAYER *p, const WEAPON_DEF *w) {
	int *a = ammo_of(p, w->ammo);
	return !a || *a >= w->cost;
}

static void spend(PLAYER *p, const WEAPON_DEF *w) {
	int *a = ammo_of(p, w->ammo);
	if (a) *a -= w->cost;
}

static void no_ammo_msg(const PLAYER *p, const WEAPON_DEF *w) {
	static const char *const msg[] = { "", "SEM FLECHAS", "SEM BALAS", "SEM MANA" };
	(void)p;
	show_message(msg[w->ammo], 40);
}

/* ------------------------------------------------------------------ */
/* Projéteis (flecha, bola de fogo, bala)                               */
/* ------------------------------------------------------------------ */

static void spawn_bullet(int x, int y, int z, int angle, int speed, int damage, int life, int widx) {
	for (int i = 0; i < MAX_BULLETS; i++) {
		BULLET *b = &g.bullets[i];
		if (b->active) continue;
		b->active = 1;
		b->pos.vx = x; b->pos.vy = y; b->pos.vz = z;
		b->vx = (isin(angle) * speed) >> 12;
		b->vz = (icos(angle) * speed) >> 12;
		b->life = life;
		b->damage = damage;
		b->weapon = widx;
		return;
	}
}

/* Sai um pouco à frente do jogador, na altura da arma */
static void muzzle(const PLAYER *p, int *x, int *y, int *z) {
	*x = p->pos.vx + ((isin(p->angle) * 90) >> 12);
	*z = p->pos.vz + ((icos(p->angle) * 90) >> 12);
	*y = p->pos.vy + BULLET_HEIGHT;
}

/* Flecha que parou: às vezes fica no chão para recolher (pool pequeno; a
 * mais antiga some quando enche) */
static void drop_arrow(int x, int z) {
	if (rng_range(&g.rng, 0, 99) >= ARROW_DROP_CHANCE) return;
	int i = g.garrow_next;
	g.garrow_next = (g.garrow_next + 1) % MAX_GROUND_ARROWS;
	g.garrow[i].active = 1;
	g.garrow[i].x = x;
	g.garrow[i].z = z;
	g.garrow[i].age = 0;
}

static void projectile_hit(BULLET *b, int x, int z) {
	const WEAPON_DEF *w = &weapon_defs[b->weapon];
	if (w->kind == WK_BOW)
		drop_arrow(x, z);
	if (w->magic == MAGIC_FIRE) {                 /* bola de fogo: estoura */
		effect_spawn(FX_BURST, x, z, 260, 14, w->color.r, w->color.g, w->color.b);
		noise_emit(x, z, w->noise);
	}
}

void bullets_update(void) {
	for (int i = 0; i < MAX_BULLETS; i++) {
		BULLET *b = &g.bullets[i];
		if (!b->active) continue;

		b->pos.vx += b->vx;
		b->pos.vz += b->vz;

		if (--b->life <= 0) {
			b->active = 0;
			projectile_hit(b, b->pos.vx, b->pos.vz);
			continue;
		}

		/* bateu na parede, numa caixa ou num objeto de cenário? */
		int cx = b->pos.vx / TILE_SIZE, cz = b->pos.vz / TILE_SIZE;
		int wall = b->pos.vx < 0 || b->pos.vz < 0 || level_cell_solid(cx, cz);
		if (wall || collide_solid_at(b->pos.vx, b->pos.vz, 24)) {
			if (wall)
				crate_damage(cx, cz, b->damage);
			sound_play_at(&sfx_acerto, b->pos.vx, b->pos.vz, VOL_PAREDE);
			effect_spawn(FX_BURST, b->pos.vx - b->vx, b->pos.vz - b->vz, 90, 8,
			             weapon_defs[b->weapon].color.r, weapon_defs[b->weapon].color.g,
			             weapon_defs[b->weapon].color.b);
			b->active = 0;
			projectile_hit(b, b->pos.vx - b->vx, b->pos.vz - b->vz);
			continue;
		}

		/* acertou um inimigo? (só os das células perto, pela grade) */
		ENEMY *e = collide_enemy_at(b->pos.vx, b->pos.vz, 30);
		if (e) {
			enemy_damage(e, b->damage, b->vx / 3, b->vz / 3);
			b->active = 0;
			projectile_hit(b, e->pos.vx - b->vx, e->pos.vz - b->vz);
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

		if (w->kind == WK_BOW) {                 /* flecha: aponta para onde voa */
			SVECTOR rot = { 0, angle_of(b->vx, b->vz), 0 };
			render_mesh(&flecha_mesh, &b->pos, &rot, ONE, NULL);
			continue;
		}
		opt.palette  = &w->color;
		opt.npalette = 1;
		SVECTOR rot = { g.frame * 90, g.frame * 130, 0 };
		render_mesh(&bullet_mesh, &b->pos, &rot, w->scale, &opt);
	}
}

/* ------------------------------------------------------------------ */
/* Flechas no chão                                                      */
/* ------------------------------------------------------------------ */

void ground_arrows_update(void) {
	for (int i = 0; i < MAX_GROUND_ARROWS; i++) {
		if (!g.garrow[i].active) continue;
		g.garrow[i].age++;
		for (int k = 0; k < MAX_PLAYERS; k++) {
			PLAYER *p = &g.players[k];
			if (!player_alive(p) || p->arrows >= MAX_ARROWS) continue;
			if (abs(p->pos.vx - g.garrow[i].x) < 140 && abs(p->pos.vz - g.garrow[i].z) < 140) {
				p->arrows++;
				g.garrow[i].active = 0;
				sound_play_at(&sfx_item, p->pos.vx, p->pos.vz, VOL_ITEM / 2);
				break;
			}
		}
	}
}

void ground_arrows_draw(void) {
	for (int i = 0; i < MAX_GROUND_ARROWS; i++) {
		if (!g.garrow[i].active) continue;
		VECTOR pos = { g.garrow[i].x, -10, g.garrow[i].z };
		SVECTOR rot = { 0, (g.garrow[i].x * 7 + g.garrow[i].z * 3) & 4095, 0 };   /* deitada, ao acaso */
		render_mesh(&flecha_mesh, &pos, &rot, ONE, NULL);
	}
}

/* ------------------------------------------------------------------ */
/* Cada tipo                                                            */
/* ------------------------------------------------------------------ */

/* Golpe: na janela de acerto, quem está no arco à frente (alcance + raio
 * do inimigo) leva dano uma vez por golpe. Inimigos pela grade. */
static void melee_hits(PLAYER *p, const WEAPON_DEF *w) {
	int widx = p->weapon;
	uint32_t near = collide_enemies_near(p->pos.vx, p->pos.vz, w->reach + 160);
	for (int i = 0; near; i++, near >>= 1) {
		if (!(near & 1) || (p->swing_hit & (1u << i))) continue;
		ENEMY *e = &g.enemies[i];
		int dx = e->pos.vx - p->pos.vx, dz = e->pos.vz - p->pos.vz;
		if (dist2d(dx, dz) > w->reach + enemy_radius(e)) continue;
		if (abs(angle_diff(p->angle, angle_of(dx, dz))) > w->arc) continue;
		p->swing_hit |= 1u << i;
		int a = angle_of(dx, dz);
		enemy_damage(e, w->damage, (isin(a) * 40) >> 12, (icos(a) * 40) >> 12);
	}
	(void)widx;
}

static void magic_cast(PLAYER *p, const WEAPON_DEF *w) {
	int x, y, z;
	switch (w->magic) {
	case MAGIC_FIRE:
		muzzle(p, &x, &y, &z);
		spawn_bullet(x, y, z, p->angle, w->speed, w->damage, w->reach, p->weapon);
		break;
	case MAGIC_FLASH: {
		/* clarão: atordoa e assusta quem está em volta (ideia da antiga onda) */
		effect_spawn(FX_SHOCKWAVE, p->pos.vx, p->pos.vz, w->reach, 22, w->color.r, w->color.g, w->color.b);
		uint32_t near = collide_enemies_near(p->pos.vx, p->pos.vz, w->reach);
		for (int i = 0; near; i++, near >>= 1) {
			if (!(near & 1)) continue;
			ENEMY *e = &g.enemies[i];
			int dx = e->pos.vx - p->pos.vx, dz = e->pos.vz - p->pos.vz;
			if (dist2d(dx, dz) >= w->reach) continue;
			int a = angle_of(dx, dz);
			enemy_damage(e, w->damage, (isin(a) * 60) >> 12, (icos(a) * 60) >> 12);
			if (e->active) {
				e->stun = FLASH_STUN;
				e->flee = FLASH_FLEE;
			}
		}
		break;
	}
	case MAGIC_HEAL:
		for (int i = 0; i < MAX_PLAYERS; i++) {
			PLAYER *o = &g.players[i];
			if (!player_alive(o)) continue;
			if (o != p && dist2d(o->pos.vx - p->pos.vx, o->pos.vz - p->pos.vz) > w->reach) continue;
			o->hp += w->damage;
			if (o->hp > o->max_hp) o->hp = o->max_hp;
			effect_spawn(FX_HEAL, o->pos.vx, o->pos.vz, 300, 30, w->color.r, w->color.g, w->color.b);
		}
		break;
	}
}

static void gun_fire(PLAYER *p, const WEAPON_DEF *w) {
	/* mira automática só nas armas de fogo, cone curto */
	int aim = p->angle;
	ENEMY *target = enemy_nearest_in_cone(p->pos.vx, p->pos.vz, p->angle, GUN_AIM_CONE, AUTO_AIM_DIST);
	if (target)
		aim = angle_of(target->pos.vx - p->pos.vx, target->pos.vz - p->pos.vz);
	int x, y, z;
	muzzle(p, &x, &y, &z);
	for (int k = 0; k < w->pellets; k++) {
		int a = aim;
		if (w->pellets > 1)
			a += ((2 * k - (w->pellets - 1)) * w->spread) / 2;  /* leque centralizado */
		spawn_bullet(x, y, z, a & 4095, w->speed, w->damage, w->reach, p->weapon);
	}
	sound_play_at(&sfx_tiro, x, z, VOL_TIRO);
}

/* Um passo da arma do jogador: golpe em andamento, carga do arco, uso */
void weapon_update(PLAYER *p, const INPUT *in) {
	const WEAPON_DEF *w = &weapon_defs[p->weapon];
	int held = in->held & PAD_SQUARE;
	if (p->cooldown > 0) p->cooldown--;

	/* golpe em andamento (corpo a corpo): janela de acerto */
	if (p->swing > 0) {
		const WEAPON_DEF *sw = &weapon_defs[p->weapon];
		if (p->swing >= sw->hit_from && p->swing <= sw->hit_to)
			melee_hits(p, sw);
		if (sw->chops && p->swing == sw->hit_from)   /* machado: corta o que está à frente */
			for (int d = 128; d <= sw->reach; d += 64)
				if (events_chop((p->pos.vx + ((isin(p->angle) * d) >> 12)) / TILE_SIZE,
				                (p->pos.vz + ((icos(p->angle) * d) >> 12)) / TILE_SIZE))
					break;
		if (++p->swing > sw->cooldown)
			p->swing = 0;
	}

	switch (w->kind) {
	case WK_MELEE:
		if (held && p->cooldown == 0 && p->swing == 0) {
			p->swing = 1;
			p->swing_hit = 0;
			p->cooldown = w->cooldown;
		}
		break;

	case WK_BOW:
		if (held && p->cooldown == 0) {
			if (!has_ammo(p, w)) {
				if (in->pressed & PAD_SQUARE) no_ammo_msg(p, w);
			} else if (p->charge < BOW_CHARGE) {
				p->charge++;
			}
		} else if (!held && p->charge > 0) {
			/* soltou: 30% de força sem carga, 100% com carga cheia */
			int k = 1229 + (2867 * p->charge) / BOW_CHARGE;          /* em ONE */
			int dmg = (w->damage * k) >> 12, spd = (w->speed * k) >> 12;
			int x, y, z;
			if (dmg < 1) dmg = 1;
			muzzle(p, &x, &y, &z);
			spawn_bullet(x, y, z, p->angle, spd, dmg, w->reach, p->weapon);
			spend(p, w);
			noise_emit(x, z, w->noise);
			p->charge = 0;
			p->cooldown = w->cooldown;
		}
		break;

	case WK_MAGIC:
	case WK_GUN:
		if (held && p->cooldown == 0) {
			if (!has_ammo(p, w)) {
				if (in->pressed & PAD_SQUARE) no_ammo_msg(p, w);
				break;
			}
			spend(p, w);
			p->cooldown = w->cooldown;
			if (w->kind == WK_MAGIC) magic_cast(p, w);
			else                     gun_fire(p, w);
			noise_emit(p->pos.vx, p->pos.vz, w->noise);
		}
		break;
	}
}

/* Arma na mão (só desenho: lê g). Presa ao ponto 'hand' do personagem,
 * girando com ele; o golpe corpo a corpo inclina a arma para a frente e a
 * carga do arco puxa o arco para trás. Magias: sem modelo. */
void weapon_draw(const PLAYER *p) {
	const WEAPON_DEF *w = &weapon_defs[p->weapon];
	if (!w->mesh) return;
	const CHARACTER_DEF *c = &character_defs[p->character];
	int s = isin(p->angle), co = icos(p->angle);
	int hx = (c->hand.vx * c->scale) >> 12, hy = (c->hand.vy * c->scale) >> 12;
	int hz = (c->hand.vz * c->scale) >> 12;
	VECTOR pos = {
		p->pos.vx + ((hx * co + hz * s) >> 12),
		p->pos.vy + hy,
		p->pos.vz + ((-hx * s + hz * co) >> 12),
	};
	SVECTOR rot = { 0, p->angle, 0 };
	if (p->swing > 0) {                          /* golpe: desce para a frente */
		int k = p->swing * 4096 / (w->cooldown ? w->cooldown : 1);
		rot.vx = -(isin(k >> 1) * 900 >> 12);
	}
	if (p->charge > 0)                           /* arco: recua com a carga */
		pos.vy -= p->charge;
	render_mesh(w->mesh, &pos, &rot, ONE, NULL);
}
