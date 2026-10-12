/*
 * enemies.c - Inimigos de terror (etapa 07): percebem, investigam, perseguem
 *
 * Estados (EN_*): vagar -> investigar um barulho -> perseguir quem viu ->
 * procurar onde viu por último -> vagar. Percepção:
 *   - AUDIÇÃO (todo passo, barata): os barulhos do passo (g.noise, os 4 mais
 *     fortes: tiro alto, passo baixo, corpo a corpo nenhum, R2 = silêncio);
 *   - VISÃO (a cada 8 passos, escalonada pelo índice): distância do tipo no
 *     escuro, o dobro com a lanterna do jogador ligada (até a névoa atual),
 *     cone à frente (se ainda não persegue) e LINHA DE VISÃO pela grade.
 * Caçador foge da lanterna apontada para ele; o grande faz passos pesados.
 * Tudo lógica (g, g.rng, por passo); som é só som.
 */
#include <stdlib.h>
#include "game.h"
#include "models.h"

#define SEE_CONE     1100   /* meia abertura da visão de quem não persegue (~97 graus) */
#define SEE_CLOSE    384    /* tão perto que sente mesmo de costas (1,5 m) */
#define LOSE_TIME    240    /* perseguindo sem ver por 4 s: vai procurar */
#define LOOK_TIME    180    /* chegou onde ouviu/viu: olha em volta por 3 s */
#define LIGHT_FLEE   90     /* caçador: passos fugindo da lanterna */
#define BIG_SHOW_DIST (6 * TILE_SIZE)   /* o grande só aparece mais perto que isto */
#define ENEMY_RADIUS 90     /* raio de colisão com tamanho ONE */

/* Intervalo entre rosnados. É estado SÓ DE ÁUDIO: fica fora de g de
 * propósito (não muda a partida) e usa fx_range, não g.rng. */
static int growl_wait = 0;

/* Inimigo perto e fora do campo de visão da câmera rosna de vez em quando:
 * o jogador ouve o que não vê (pilar "ver pouco, ouvir muito"). */
static void enemy_growl(const ENEMY *e) {
	const VECTOR *cam = camera_eye();    /* só decide o SOM (não muda g) */
	int dx = e->pos.vx - cam->vx, dz = e->pos.vz - cam->vz;
	if (dist2d(dx, dz) > GROWL_DIST)
		return;
	int rel = angle_diff(g.cam_yaw, angle_of(dx, dz));
	if (rel > -640 && rel < 640)
		return;                       /* dentro da tela (~56 graus para cada lado) */
	if (fx_range(0, GROWL_CHANCE - 1) != 0)
		return;
	sound_play_at(&sfx_rosnado, e->pos.vx, e->pos.vz, VOL_ROSNADO);
	growl_wait = GROWL_GAP;
}

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
		e->vy = 0;
		e->stun = e->flee = 0;
		e->state = EN_WANDER;
		e->tx = x; e->tz = z;
		e->timer = 0;
		g.enemies_left++;
		collide_track(e);                 /* posição definida direto: entra na grade */
		return;
	}
}

void enemy_damage(ENEMY *e, int amount, int push_x, int push_z, int attacker) {
	e->hp -= amount;
	e->flash = 6;
	e->kx += push_x;
	e->kz += push_z;

	if (e->hp > 0) {
		sound_play_at(&sfx_acerto, e->pos.vx, e->pos.vz, VOL_ACERTO);
		/* apanhou: sabe onde está quem bateu (o jogador mais perto) */
		PLAYER *p = player_nearest(e->pos.vx, e->pos.vz, NULL);
		if (p) {
			e->state = EN_CHASE;
			e->tx = p->pos.vx; e->tz = p->pos.vz;
			e->timer = LOSE_TIME;
		}
	}

	if (e->hp <= 0) {
		const ENEMY_DEF *d = &enemy_defs[e->type];
		sound_play_at(&sfx_morte, e->pos.vx, e->pos.vz, VOL_MORTE);
		e->active = 0;
		collide_track(e);                 /* morreu: sai da grade */
		g.enemies_left--;
		PL(pl_kill(e->type));
		if (attacker >= 0 && e->type < STAT_ENEMY_TYPES)   /* estatística (menu) */
			g.players[attacker].st.kills[e->type]++;
		events_enemy_killed();
		g.score += d->score;
		effect_spawn(FX_BURST, e->pos.vx, e->pos.vz, (300 * d->scale) >> 12, 18,
		             d->palette[0].r, d->palette[0].g, d->palette[0].b);

		/* chance de soltar um item */
		int r = rng_next(&g.rng) % 100;                /* escasso (etapa 07) */
		if (r < 12)      pickup_spawn(PICK_HEALTH, e->pos.vx, e->pos.vz);   /* 12% */
		else if (r < 20) pickup_spawn(PICK_ENERGY, e->pos.vx, e->pos.vz);   /*  8% */
		else if (r < 26) pickup_spawn(PICK_BATTERY, e->pos.vx, e->pos.vz);  /*  6% */
		else if (r < 32) pickup_spawn(PICK_ARROWS, e->pos.vx, e->pos.vz);   /*  6% */

	}
}

/* O jogador p está visível para o inimigo e? (distância pela escuridão e
 * pela lanterna, cone, linha de visão pela grade) */
static int can_see(const ENEMY *e, const ENEMY_DEF *d, const PLAYER *p, int dist) {
	int sight = d->sight * TILE_SIZE;
	if (p->lantern_on) sight *= 2;                    /* luz denuncia */
	if (sight > g.ev.fog_far) sight = g.ev.fog_far;   /* névoa atual (lógica) */
	if (dist > sight) return 0;
	if (e->state != EN_CHASE && dist > SEE_CLOSE &&
	    abs(angle_diff(e->angle, angle_of(p->pos.vx - e->pos.vx, p->pos.vz - e->pos.vz))) > SEE_CONE)
		return 0;
	return collide_line_of_sight(e->pos.vx, e->pos.vz, p->pos.vx, p->pos.vz);
}

/* Lanterna do jogador apontada para e, perto? (caçador foge da luz) */
static int in_light(const ENEMY *e, const PLAYER *p, int dist) {
	if (!p->lantern_on || dist > 7 * TILE_SIZE) return 0;
	return abs(angle_diff(p->angle, angle_of(e->pos.vx - p->pos.vx, e->pos.vz - p->pos.vz))) < 350;
}

void enemies_update(void) {
	if (growl_wait > 0) growl_wait--;
	for (int i = 0; i < MAX_ENEMIES; i++) {
		ENEMY *e = &g.enemies[i];
		if (!e->active) continue;
		const ENEMY_DEF *d = &enemy_defs[e->type];
		int rad = enemy_radius(e);

		int dist = 0x7fffffff;
		PLAYER *p = player_nearest(e->pos.vx, e->pos.vz, &dist);
		int play = g.state == STATE_PLAY;

		/* --- percepção --- */
		if (play && e->state != EN_CHASE)                 /* audição (todo passo) */
			for (int k = 0; k < g.nnoise; k++) {
				int nd = dist2d(g.noise[k].x - e->pos.vx, g.noise[k].z - e->pos.vz);
				if (nd * 100 < g.noise[k].r * d->hearing) {
					e->state = EN_INVESTIGATE;
					e->tx = g.noise[k].x; e->tz = g.noise[k].z;
					e->timer = 0;
				}
			}
		if (play && p && ((g.frame + i) & 7) == 0) {      /* visão (a cada 8 passos) */
			if (can_see(e, d, p, dist)) {
				if (e->state != EN_CHASE)          /* viu agora: grita (só som) */
					sound_play_at(&sfx_grito, e->pos.vx, e->pos.vz, VOL_GRITO);
				e->state = EN_CHASE;
				e->tx = p->pos.vx; e->tz = p->pos.vz;
				e->timer = LOSE_TIME;
			}
			if (d->flee_light && in_light(e, p, dist))
				e->flee = LIGHT_FLEE;
		}
		if (e->state == EN_CHASE && (!p || --e->timer <= 0)) {   /* perdeu o rastro */
			if (d->heavy) PL(pl_big_escape());          /* fugiu do grande */
			e->state = EN_SEARCH;
			e->timer = 0;
		}

		/* --- decidir direção e velocidade --- */
		int sp = d->speed / 2;
		if (e->stun > 0) {                                 /* clarão: atordoado */
			e->stun--;
			sp = 0;
		} else if (e->flee > 0 && p) {                     /* clarão / luz: foge */
			e->flee--;
			e->angle = turn_towards(e->angle, angle_of(e->pos.vx - p->pos.vx, e->pos.vz - p->pos.vz), 160);
			sp = d->speed;
		} else if (e->state == EN_CHASE && p) {
			e->angle = turn_towards(e->angle, angle_of(p->pos.vx - e->pos.vx, p->pos.vz - e->pos.vz), 96);
			sp = d->speed;
		} else if (e->state == EN_INVESTIGATE || e->state == EN_SEARCH) {
			int dx = e->tx - e->pos.vx, dz = e->tz - e->pos.vz;
			if (e->timer > 0 || dist2d(dx, dz) < 200) {   /* chegou: olha em volta */
				if (++e->timer > LOOK_TIME) e->state = EN_WANDER;
				if (--e->think <= 0) {
					e->angle = (e->angle + rand_range(-1200, 1200)) & 4095;
					e->think = rand_range(30, 70);
				}
				sp = 0;
			} else {
				e->angle = turn_towards(e->angle, angle_of(dx, dz), 96);
				sp = d->speed * 3 / 4;
			}
		} else if (--e->think <= 0) {                      /* vaga */
			e->angle = (e->angle + rand_range(-900, 900)) & 4095;
			e->think = rand_range(40, 140);
		}
		int chasing = e->state == EN_CHASE && e->stun == 0 && e->flee == 0;

		/* --- mover com colisão contra paredes, cenário, jogadores e outros
		 *     inimigos (ninguém atravessa ninguém) --- */
		int mx = ((isin(e->angle) * sp) >> 12) + e->kx;
		int mz = ((icos(e->angle) * sp) >> 12) + e->kz;
		e->kx = e->kx * 3 / 4;     /* empurrão vai diminuindo */
		e->kz = e->kz * 3 / 4;

		/* tronco caído logo à frente e no chão? pula (mesma ideia do jogador:
		 * acima de LOW_CLEAR a célula baixa não bloqueia) */
		if (e->pos.vy == 0 && sp > 0) {
			int ax = e->pos.vx + ((isin(e->angle) * (rad + 48)) >> 12);
			int az = e->pos.vz + ((icos(e->angle) * (rad + 48)) >> 12);
			if (ax >= 0 && az >= 0 && level_cell_low(ax / TILE_SIZE, az / TILE_SIZE))
				e->vy = -ENEMY_HOP;
		}
		e->vy += GRAVITY;
		e->pos.vy += e->vy;
		if (e->pos.vy >= 0) { e->pos.vy = 0; e->vy = 0; }

		int blocked = (mx || mz) ? collide_move(&e->pos, mx, mz, rad, e, COL_ALL) : 0;
		if (blocked && !chasing)
			e->angle = (e->angle + 1024 + (rng_next(&g.rng) & 1023)) & 4095;
		else if (blocked && chasing && (g.frame & 31) == (i & 31))
			e->angle = (e->angle + ((rng_next(&g.rng) & 1) ? 700 : -700)) & 4095;  /* contorna */

		/* passos pesados do grande: ouve-se antes de ver (só som) */
		if (d->heavy && sp > 0 && ((g.frame + i * 7) % 40) == 0)
			sound_play_at(&sfx_pisada, e->pos.vx, e->pos.vz, VOL_PASSO_GRANDE);

		/* --- encostou no jogador? (quem está pulando por cima escapa) ---
		 * Como os dois não se atravessam, "encostar" é chegar a poucas
		 * unidades da soma dos raios. */
		if (e->hit_cooldown > 0) e->hit_cooldown--;
		if (p && dist <= rad + PLAYER_RADIUS + 16 && p->pos.vy > -140 && e->hit_cooldown == 0 &&
		    e->stun == 0) {
			PL(pl_source(e->type));
			player_damage(p, d->damage, e->pos.vx, e->pos.vz);
			e->hit_cooldown = 30;
		}

		if (e->flash > 0) e->flash--;
		if (growl_wait == 0 && play)
			enemy_growl(e);
	}
	g.nnoise = 0;                  /* barulhos deste passo já foram ouvidos */
}

void enemies_draw(void) {
	for (int i = 0; i < MAX_ENEMIES; i++) {
		ENEMY *e = &g.enemies[i];
		if (!e->active) continue;
		const ENEMY_DEF *d = &enemy_defs[e->type];
		if (d->heavy) {                        /* o grande: de longe, só se ouve */
			int dist = 0x7fffffff;
			player_nearest(e->pos.vx, e->pos.vz, &dist);
			if (dist > BIG_SHOW_DIST) continue;
		}

		DRAWOPT opt = { 0 };
		opt.palette  = d->palette;
		opt.npalette = 3;
		if (e->flash & 2) opt.flags |= DRAW_FLASH;

		/* pulinhos enquanto anda */
		VECTOR pos = e->pos;
		int hop = isin((g.frame * 128 + i * 700) & 4095);
		pos.vy = e->pos.vy - ((hop < 0 ? -hop : hop) >> 8);   /* + pulo real */

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
