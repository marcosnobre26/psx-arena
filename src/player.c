/*
 * player.c - Os jogadores (1 ou 2; cada um com o seu controle)
 *
 * Controles:
 *   D-pad / analógico esq.  andar (relativo à câmera)
 *   X                        pular (no ar você passa por cima dos inimigos)
 *   QUADRADO                 atirar (segure)
 *   TRIÂNGULO                trocar de arma
 *   CÍRCULO                  usar poder
 *   R2                       trocar de poder
 *   L1 / R1 / analóg. dir.   girar câmera
 *   SELECT                   trocar skin (personagens com use_skins)
 *   START                    pausar
 *
 * Com 2 jogadores: quem cai volta depois de RESPAWN_TIME ao lado do
 * parceiro. Se os dois caírem, é game over.
 */
#include <psxpad.h>
#include <stdio.h>
#include "game.h"
#include "models.h"

static const CHARACTER_DEF *cdef(const PLAYER *p) {
	return &character_defs[p->character];
}

int player_alive(const PLAYER *p) {
	return p->active && p->hp > 0;
}

int players_alive(void) {
	int n = 0;
	for (int i = 0; i < MAX_PLAYERS; i++)
		n += player_alive(&g.players[i]);
	return n;
}

PLAYER *player_nearest(int x, int z, int *dist_out) {
	PLAYER *best = NULL;
	int best_d = 0x7fffffff;
	for (int i = 0; i < MAX_PLAYERS; i++) {
		PLAYER *p = &g.players[i];
		if (!player_alive(p)) continue;
		int d = dist2d(p->pos.vx - x, p->pos.vz - z);
		if (d < best_d) { best = p; best_d = d; }
	}
	if (dist_out) *dist_out = best_d;
	return best;
}

/* Mensagem com prefixo "P2 " quando há dois jogadores */
static void pmsg(const PLAYER *p, const char *text, int frames) {
	char buf[40];
	if (g.num_players > 1) {
		snprintf(buf, sizeof(buf), "P%d %s", p->index + 1, text);
		show_message(buf, frames);
	} else {
		show_message(text, frames);
	}
}

/* Coloca o jogador no mapa. character/skin/index/active já vêm preenchidos. */
void player_spawn(PLAYER *p, int x, int z) {
	const CHARACTER_DEF *c = cdef(p);
	p->pos.vx = x; p->pos.vy = 0; p->pos.vz = z;
	p->vy = 0;
	p->angle = 0;
	p->on_ground = 1;
	p->max_hp = c->max_hp;
	p->hp = c->max_hp;
	p->energy = PLAYER_MAX_ENERGY / 2;
	p->invuln = 0;
	p->weapon = c->weapon;                   /* arma inicial do personagem */
	p->weapons_owned = 1 | (1 << c->weapon); /* blaster + a arma inicial */
	p->cooldown = 0;
	p->power = 0;
	p->dash_timer = 0;
	p->walk_anim = 0;
	p->regen_timer = 0;
	p->respawn_timer = 0;
}

static void move(PLAYER *p, int dx, int dz) {
	collide_move(&p->pos, dx, dz, PLAYER_RADIUS, p, COL_ALL);
}

/* Volta para o jogo ao lado de um parceiro vivo */
static void respawn(PLAYER *p) {
	PLAYER *mate = NULL;
	for (int i = 0; i < MAX_PLAYERS; i++)
		if (&g.players[i] != p && player_alive(&g.players[i]))
			mate = &g.players[i];
	if (!mate) return;

	/* procura um lugar livre em volta do parceiro */
	static const int off[8][2] = { {1,0},{-1,0},{0,1},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1} };
	int x = mate->pos.vx, z = mate->pos.vz;
	for (int k = 0; k < 8; k++) {
		int tx = mate->pos.vx + off[k][0] * 200, tz = mate->pos.vz + off[k][1] * 200;
		if (!collide_blocked(tx, tz, 0, PLAYER_RADIUS, p, COL_ALL, tx, tz)) { x = tx; z = tz; break; }
	}
	int skin = p->skin;
	player_spawn(p, x, z);
	p->skin = skin;
	p->hp = p->max_hp / 2;
	p->invuln = PLAYER_INVULN * 2;
	effect_spawn(FX_HEAL, x, z, 300, 30, 80, 255, 120);
	pmsg(p, "VOLTOU!", 60);
}

void player_update(PLAYER *p, INPUT *in) {
	if (!p->active) return;

	/* caído: espera para voltar */
	if (p->hp <= 0) {
		if (p->respawn_timer > 0 && --p->respawn_timer == 0)
			respawn(p);
		return;
	}

	/* ---- direção desejada (no espaço da câmera) ---- */
	int ix = in->lx, iz = -in->ly;            /* analógico: para cima = frente */
	if (in->held & PAD_LEFT)  ix = -127;
	if (in->held & PAD_RIGHT) ix =  127;
	if (in->held & PAD_UP)    iz =  127;
	if (in->held & PAD_DOWN)  iz = -127;

	if (ix || iz) {
		/* gira o vetor do controle pelo ângulo da câmera */
		int s = isin(g.cam_yaw), c = icos(g.cam_yaw);
		int wx = (ix * c + iz * s) >> 12;
		int wz = (-ix * s + iz * c) >> 12;

		int mag = dist2d(wx, wz);
		if (mag > 127) mag = 127;
		int speed = (cdef(p)->speed * mag) / 127;

		int dir = angle_of(wx, wz);
		p->angle = turn_towards(p->angle, dir, 256);
		move(p, (isin(dir) * speed) >> 12, (icos(dir) * speed) >> 12);
		p->walk_anim += speed;
	} else {
		p->walk_anim = 0;
	}

	/* ---- dash (poder) ---- */
	if (p->dash_timer > 0) {
		p->dash_timer--;
		int sp = cdef(p)->speed * 3;
		move(p, (isin(p->angle) * sp) >> 12, (icos(p->angle) * sp) >> 12);
	}

	/* ---- pulo e gravidade ---- */
	if ((in->pressed & PAD_CROSS) && p->on_ground) {
		p->vy = -cdef(p)->jump;
		p->on_ground = 0;
	}
	p->vy += GRAVITY;
	p->pos.vy += p->vy;
	if (p->pos.vy >= 0) {          /* chão em Y = 0 */
		p->pos.vy = 0;
		p->vy = 0;
		p->on_ground = 1;
	}

	/* ---- armas ---- */
	if (p->cooldown > 0) p->cooldown--;
	if ((in->held & PAD_SQUARE) && p->cooldown == 0)
		weapon_fire(p);

	if (in->pressed & PAD_TRIANGLE) {
		for (int i = 1; i <= num_weapons; i++) {     /* próxima arma que possui */
			int w = (p->weapon + i) % num_weapons;
			if (p->weapons_owned & (1 << w)) {
				if (w != p->weapon) {
					p->weapon = w;
					pmsg(p, weapon_defs[w].name, 50);
				}
				break;
			}
		}
	}

	/* ---- poderes ---- */
	if (in->pressed & PAD_R2) {
		p->power = (p->power + 1) % num_powers;
		pmsg(p, power_defs[p->power].name, 50);
	}
	if (in->pressed & PAD_CIRCLE)
		power_use(p);

	if (++p->regen_timer >= ENERGY_REGEN_DELAY) {
		p->regen_timer = 0;
		if (p->energy < PLAYER_MAX_ENERGY) p->energy++;
	}

	/* ---- customização (só personagens com use_skins) ---- */
	if ((in->pressed & PAD_SELECT) && cdef(p)->use_skins) {
		p->skin = (p->skin + 1) % num_skins;
		char buf[32];
		snprintf(buf, sizeof(buf), "SKIN: %s", skin_defs[p->skin].name);
		pmsg(p, buf, 50);
	}

	if (p->invuln > 0) p->invuln--;
}

void player_damage(PLAYER *p, int amount, int from_x, int from_z) {
	if (!player_alive(p) || p->invuln > 0 || p->dash_timer > 0 || g.state != STATE_PLAY)
		return;
	p->hp -= amount;
	p->invuln = PLAYER_INVULN;

	/* empurra o jogador para longe da origem do dano */
	int dir = angle_of(p->pos.vx - from_x, p->pos.vz - from_z);
	move(p, (isin(dir) * 60) >> 12, (icos(dir) * 60) >> 12);
	if (p->on_ground) { p->vy = -14; p->on_ground = 0; }

	if (p->hp <= 0) {
		p->hp = 0;
		effect_spawn(FX_BURST, p->pos.vx, p->pos.vz, 500, 30, 255, 80, 40);
		if (players_alive() == 0) {
			g.state = STATE_DEAD;
		} else {
			p->respawn_timer = RESPAWN_TIME;
			pmsg(p, "CAIU!", 60);
		}
	}
}

/* Desenha o modelo de um personagem (usado no jogo e na tela de seleção) */
void player_draw_model(const VECTOR *pos, const SVECTOR *rot, int character, int skin) {
	const CHARACTER_DEF *c = &character_defs[character];
	DRAWOPT opt = { 0 };
	if (c->use_skins) {
		opt.palette  = skin_defs[skin].colors;
		opt.npalette = 5;
	}
	opt.tex = c->tex;
	render_mesh(c->mesh, pos, rot, c->scale, &opt);
}

void player_draw(PLAYER *p) {
	if (!player_alive(p))
		return;
	if (p->invuln > 0 && (p->invuln & 4))     /* pisca quando invulnerável */
		return;

	VECTOR pos = p->pos;
	SVECTOR rot = { 0, p->angle, 0 };

	/* "animação" simples: balança e inclina ao andar */
	if (p->on_ground && p->walk_anim) {
		int bob = isin(p->walk_anim * 6);
		pos.vy -= (bob < 0 ? -bob : bob) >> 9;   /* sobe e desce 0..8 */
		rot.vz = isin(p->walk_anim * 3) >> 6;
	}
	if (p->dash_timer > 0)
		rot.vx = -300;           /* inclina para frente no dash */

	player_draw_model(&pos, &rot, p->character, p->skin);
	shadow_draw(&p->pos, ONE * 3 / 4);

	/* com 2 jogadores, um anel colorido no chão mostra quem é quem */
	if (g.num_players > 1) {
		static const CVECTOR mark[MAX_PLAYERS] = { { 80, 160, 255 }, { 255, 90, 90 } };
		DRAWOPT o = { 0 };
		o.flags = DRAW_UNLIT | DRAW_SEMITRANS | DRAW_NOCULL;
		o.palette = &mark[p->index];
		o.npalette = 1;
		o.zbias = 1;
		VECTOR mp = { p->pos.vx, -3, p->pos.vz };
		render_mesh(&ring_mesh, &mp, NULL, ONE * 3 / 8, &o);
	}
}
