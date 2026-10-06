/*
 * items.c - Itens coletáveis, caixas destrutíveis, efeitos e sombras
 */
#include <stdlib.h>
#include "game.h"
#include "models.h"

/* Cores de cada tipo de item (usam o modelo da gema) */
static const CVECTOR pickup_colors[NUM_PICKUP_TYPES] = {
	{ 255, 220,  40 },   /* PICK_GEM    */
	{ 255,  60,  80 },   /* PICK_HEALTH */
	{  60, 160, 255 },   /* PICK_ENERGY */
	{ 255, 255, 255 },   /* PICK_WEAPON */
};
static const int pickup_scale[NUM_PICKUP_TYPES] = { ONE, ONE * 3 / 4, ONE * 3 / 4, ONE * 3 / 2 };

/* ------------------------------------------------------------------ */
/* Itens                                                               */
/* ------------------------------------------------------------------ */

void pickup_spawn(int type, int x, int z) {
	for (int i = 0; i < MAX_PICKUPS; i++) {
		PICKUP *p = &g.pickups[i];
		if (p->active) continue;
		p->active = 1;
		p->type = type;
		p->pos.vx = x; p->pos.vy = 0; p->pos.vz = z;
		if (type == PICK_GEM) g.gems_total++;
		return;
	}
}

static void collect(PICKUP *it, PLAYER *p) {
	switch (it->type) {
	case PICK_GEM:
		g.gems++;
		g.score += 250;
		p->energy += 15;
		show_message("GEMA!", 40);
		break;
	case PICK_HEALTH:
		p->hp += 30;
		show_message("+30 VIDA", 40);
		break;
	case PICK_ENERGY:
		p->energy += 40;
		show_message("+40 ENERGIA", 40);
		break;
	case PICK_WEAPON: {
		/* libera a próxima arma que o jogador ainda não tem */
		int w;
		for (w = 0; w < num_weapons; w++)
			if (!(p->weapons_owned & (1 << w))) break;
		if (w < num_weapons) {
			p->weapons_owned |= 1 << w;
			p->weapon = w;
			show_message(weapon_defs[w].name, 70);
		} else {
			g.score += 500;
		}
		break;
	}
	}
	if (p->hp > p->max_hp) p->hp = p->max_hp;
	if (p->energy > PLAYER_MAX_ENERGY) p->energy = PLAYER_MAX_ENERGY;
	effect_spawn(FX_HEAL, it->pos.vx, it->pos.vz, 160, 14,
	             pickup_colors[it->type].r, pickup_colors[it->type].g, pickup_colors[it->type].b);
	it->active = 0;
}

void pickups_update(void) {
	for (int i = 0; i < MAX_PICKUPS; i++) {
		PICKUP *it = &g.pickups[i];
		if (!it->active) continue;
		for (int k = 0; k < MAX_PLAYERS; k++) {      /* qualquer jogador pega */
			PLAYER *p = &g.players[k];
			if (!player_alive(p)) continue;
			int dx = p->pos.vx - it->pos.vx, dz = p->pos.vz - it->pos.vz;
			if (abs(dx) < 140 && abs(dz) < 140) {
				collect(it, p);
				break;
			}
		}
	}
}

void pickups_draw(void) {
	DRAWOPT opt = { 0 };
	opt.npalette = 1;
	for (int i = 0; i < MAX_PICKUPS; i++) {
		PICKUP *it = &g.pickups[i];
		if (!it->active) continue;
		VECTOR pos = it->pos;
		pos.vy = -80 + (isin((g.frame * 60 + i * 500) & 4095) >> 8);   /* flutua */
		SVECTOR rot = { 0, (g.frame * 48 + i * 300) & 4095, 0 };       /* gira */
		opt.palette = &pickup_colors[it->type];
		render_mesh(&gem_mesh, &pos, &rot, pickup_scale[it->type], &opt);
		shadow_draw(&it->pos, ONE / 2);
	}
}

/* ------------------------------------------------------------------ */
/* Caixas                                                              */
/* ------------------------------------------------------------------ */

void crate_spawn(int cx, int cz) {
	for (int i = 0; i < MAX_CRATES; i++) {
		CRATE *c = &g.crates[i];
		if (c->active) continue;
		c->active = 1;
		c->hp = 3;
		c->cx = cx; c->cz = cz;
		c->flash = 0;
		return;
	}
}

/* Chamado quando um tiro bate numa célula sólida */
void crate_damage(int cx, int cz, int amount) {
	for (int i = 0; i < MAX_CRATES; i++) {
		CRATE *c = &g.crates[i];
		if (!c->active || c->cx != cx || c->cz != cz) continue;
		c->hp -= amount;
		c->flash = 4;
		if (c->hp <= 0) {
			int x = cx * TILE_SIZE + TILE_SIZE / 2, z = cz * TILE_SIZE + TILE_SIZE / 2;
			c->active = 0;
			level_set_solid(cx, cz, 0);
			effect_spawn(FX_BURST, x, z, 260, 16, 200, 140, 70);
			g.score += 20;
			int r = rand() % 100;
			if (r < 35)      pickup_spawn(PICK_HEALTH, x, z);
			else if (r < 70) pickup_spawn(PICK_ENERGY, x, z);
		}
		return;
	}
}

void crates_draw(void) {
	DRAWOPT opt = { 0 };
	opt.tex = &tex_crate_t;
	for (int i = 0; i < MAX_CRATES; i++) {
		CRATE *c = &g.crates[i];
		if (!c->active) continue;
		VECTOR pos = { c->cx * TILE_SIZE + TILE_SIZE / 2, 0, c->cz * TILE_SIZE + TILE_SIZE / 2 };
		opt.flags = (c->flash > 0) ? DRAW_FLASH : 0;
		if (c->flash > 0) c->flash--;
		render_mesh(&crate_mesh, &pos, NULL, ONE * 15 / 16, &opt);
	}
}

/* ------------------------------------------------------------------ */
/* Efeitos visuais (anéis semitransparentes)                          */
/* ------------------------------------------------------------------ */

void effect_spawn(int type, int x, int z, int radius, int duration, int r, int gr, int b) {
	int slot = 0;
	for (int i = 0; i < MAX_EFFECTS; i++) {
		if (!g.effects[i].active) { slot = i; break; }
		if (g.effects[i].timer > g.effects[slot].timer) slot = i;  /* substitui o mais velho */
	}
	EFFECT *e = &g.effects[slot];
	e->active = 1;
	e->type = type;
	e->pos.vx = x; e->pos.vy = -8; e->pos.vz = z;
	e->timer = 0;
	e->duration = duration;
	e->radius = radius;
	e->color.r = r; e->color.g = gr; e->color.b = b;
}

void effects_update(void) {
	for (int i = 0; i < MAX_EFFECTS; i++) {
		EFFECT *e = &g.effects[i];
		if (e->active && ++e->timer >= e->duration)
			e->active = 0;
	}
}

void effects_draw(void) {
	DRAWOPT opt = { 0 };
	opt.flags = DRAW_UNLIT | DRAW_SEMITRANS | DRAW_NOCULL;
	opt.npalette = 1;
	opt.zbias = -4;

	for (int i = 0; i < MAX_EFFECTS; i++) {
		EFFECT *e = &g.effects[i];
		if (!e->active) continue;

		/* o anel cresce de 0 até o raio; o modelo tem raio 256 */
		int t = (e->timer << 12) / e->duration;          /* 0..4096 */
		int radius = (e->radius * t) >> 12;
		int scale = (radius << 12) / 256;
		if (scale < 64) scale = 64;

		VECTOR pos = e->pos;
		SVECTOR rot = { 0, e->timer * 40, 0 };
		if (e->type == FX_HEAL)
			pos.vy = -8 - (t * 200 >> 12);                 /* sobe */
		if (e->type == FX_BURST)
			pos.vy = -100;                                  /* no ar */

		opt.palette = &e->color;
		render_mesh(&ring_mesh, &pos, &rot, scale, &opt);
		if (e->type == FX_BURST) {                          /* anel em pé */
			SVECTOR r2 = { 1024, e->timer * 60, 0 };
			render_mesh(&ring_mesh, &pos, &r2, scale, &opt);
		}
	}
}

/* ------------------------------------------------------------------ */
/* Sombra redonda: um truque barato e muito usado no PS1               */
/* ------------------------------------------------------------------ */

void shadow_draw(const VECTOR *pos, int scale) {
	static const CVECTOR black = { 0, 0, 0 };
	DRAWOPT opt = { 0 };
	opt.flags = DRAW_UNLIT | DRAW_SEMITRANS;
	opt.palette = &black;
	opt.npalette = 1;
	opt.zbias = 2;                 /* entre o chão (+6) e os objetos */
	VECTOR p = { pos->vx, -2, pos->vz };
	render_mesh(&shadow_mesh, &p, NULL, scale, &opt);
}

/* ------------------------------------------------------------------ */
/* Objetos de cenário (tabela prop_defs em data.c)                     */
/* ------------------------------------------------------------------ */

void prop_spawn(int def, int cx, int cz) {
	for (int i = 0; i < MAX_PROPS; i++) {
		PROP *p = &g.props[i];
		if (p->active) continue;
		p->active = 1;
		p->def = def;
		p->cx = cx; p->cz = cz;
		p->angle = ((cx * 7 + cz * 3) & 3) * 1024;   /* varia a rotação */

		/* raio de colisão = maior distância horizontal de um vértice
		 * ao centro do modelo (já com a escala da tabela) */
		const MESH *m = prop_defs[def].mesh;
		int r2 = 0;
		for (int v = 0; v < m->nverts; v++) {
			int x = m->verts[v].vx, z = m->verts[v].vz;
			if (x * x + z * z > r2) r2 = x * x + z * z;
		}
		p->radius = (SquareRoot0(r2) * prop_defs[def].scale) >> 12;
		return;
	}
}

void props_draw(void) {
	for (int i = 0; i < MAX_PROPS; i++) {
		PROP *p = &g.props[i];
		if (!p->active) continue;
		const PROP_DEF *d = &prop_defs[p->def];
		DRAWOPT opt = { 0 };
		opt.tex = d->tex;
		VECTOR pos = { p->cx * TILE_SIZE + TILE_SIZE / 2, 0, p->cz * TILE_SIZE + TILE_SIZE / 2 };
		SVECTOR rot = { 0, p->angle, 0 };
		render_mesh(d->mesh, &pos, &rot, d->scale, &opt);
	}
}
