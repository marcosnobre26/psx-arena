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
	{ 190,  90, 255 },   /* PICK_QUEST  (item de missão, letra Q) */
	{ 140, 255, 110 },   /* PICK_BATTERY (pilha da lanterna, letra L) */
	{ 255, 150,  20 },   /* PICK_KEY    (chave da barreira de raízes, letra K) */
	{ 210, 180, 110 },   /* PICK_ARROWS (feixe de flechas, letra V) */
	{ 200, 200, 210 },   /* PICK_AMMO   (balas, letra M) */
};
static const int pickup_scale[NUM_PICKUP_TYPES] = { ONE, ONE * 3 / 4, ONE * 3 / 4, ONE * 3 / 2, ONE * 5 / 4, ONE * 2 / 3,
                                                    ONE * 5 / 4, ONE * 3 / 4, ONE * 2 / 3 };

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
		if (type == PICK_QUEST) g.quest_total++;
		return;
	}
}

static void collect(PICKUP *it, PLAYER *p) {
#ifdef DEBUG_PLAYLOG
	int before_a = p->arrows, before_b = p->bullets, before_m = p->energy;
#endif
	switch (it->type) {
	case PICK_GEM:
		g.gems++;
		g.score += 250;
		p->energy += 15;
		show_message("GEMA!", 40);
		break;
	case PICK_HEALTH:
		PL(pl_heal(0));
		p->hp += 30;
		show_message("+30 VIDA", 40);
		break;
	case PICK_ENERGY:
		p->energy += 40;
		show_message("+40 MANA", 40);
		break;
	case PICK_ARROWS:
		p->arrows += ARROW_PICKUP;
		if (p->arrows > MAX_ARROWS) p->arrows = MAX_ARROWS;
		show_message("+FLECHAS", 40);
		break;
	case PICK_AMMO:
		p->bullets += BULLET_PICKUP;
		if (p->bullets > MAX_BULLETS_AMMO) p->bullets = MAX_BULLETS_AMMO;
		show_message("+BALAS", 40);
		break;
	case PICK_BATTERY:
		PL(pl_battery(p));
		p->battery += BATTERY_PICKUP;
		if (p->battery > BATTERY_MAX) p->battery = BATTERY_MAX;
		p->lantern_dying = 0;
		show_message("+PILHA", 40);
		break;
	case PICK_KEY:
		g.ev.keys++;                      /* do grupo, como os itens de missão */
		show_message("CHAVE! ABRE AS RAIZES", 90);
		break;
	case PICK_QUEST:
		g.quest++;
		g.score += 200;
		show_message("ITEM DE MISSAO!", 50);
		break;
	case PICK_WEAPON: {
		/* arma nova, na ordem: arco, machado, pistola, espingarda (fogo é
		 * raro: vem por último), com um pouco de munição */
		static const int order[] = { W_BOW, W_AXE, W_PISTOL, W_SHOTGUN };
		int w = -1;
		for (int k = 0; k < 4 && w < 0; k++)
			if (!(p->weapons_owned & (1u << order[k]))) w = order[k];
		if (w >= 0) {
			p->weapons_owned |= 1u << w;
			p->weapon = w;
			p->swing = p->charge = 0;
			show_message(weapon_defs[w].name, 70);
		}
		if (w == W_BOW || w < 0) p->arrows += 4;
		if (w == W_PISTOL || w == W_SHOTGUN || w < 0) p->bullets += 3;
		if (p->arrows > MAX_ARROWS) p->arrows = MAX_ARROWS;
		if (p->bullets > MAX_BULLETS_AMMO) p->bullets = MAX_BULLETS_AMMO;
		break;
	}
	}
	if (p->hp > p->max_hp) p->hp = p->max_hp;
	if (p->energy > PLAYER_MAX_ENERGY) p->energy = PLAYER_MAX_ENERGY;
	PL(pl_found(p, AMMO_ARROW, p->arrows - before_a));      /* o que entrou de fato */
	PL(pl_found(p, AMMO_BULLET, p->bullets - before_b));
	PL(pl_found(p, AMMO_MANA, p->energy - before_m));
	sound_play_at(&sfx_item, it->pos.vx, it->pos.vz, VOL_ITEM);
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
			int r = rng_next(&g.rng) % 100;
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

/* Sombra SIMPLES (etapa 10a): um quadrado de mesma área do octógono
 * (lado 2 x 108 ~ área de um octógono de raio 128), 1 quad em vez de 6
 * triângulos. Usada longe da câmera, onde a diferença de forma não se vê
 * (o preparo + 6 faces custavam ~1,8 linha por sombra). Montada aqui no
 * código: models/*.h só o exportador gera. */
static const SVECTOR shadow4_verts[4] = {
	{ -108, 0, -108, 0 }, { -108, 0, 108, 0 }, { 108, 0, -108, 0 }, { 108, 0, 108, 0 },
};
static const SVECTOR shadow4_norm[1] = { { 0, -4096, 0, 0 } };
static const MESH_FACE shadow4_face[1] = {
	{ { 0, 1, 2, 3 }, 0, 0, 0, FACE_QUAD | FACE_UNLIT, 0, 0, { 0 }, 0 },
};
static const MESH shadow4_mesh = { 4, 1, shadow4_verts, shadow4_norm, shadow4_face, 153 };

void shadow_draw(const VECTOR *pos, int scale) {
	static const CVECTOR black = { 0, 0, 0 };
	DRAWOPT opt = { 0 };
	opt.flags = DRAW_UNLIT | DRAW_SEMITRANS;
	if (render_view_depth(pos->vx, 0, pos->vz) > SHADOW_SIMPLE_DIST) {
		opt.flags |= DRAW_NOCULL;              /* vale qualquer sentido do quad */
		opt.palette = &black;
		opt.npalette = 1;
		opt.zbias = 2;
		VECTOR p = { pos->vx, -2, pos->vz };
		render_mesh(&shadow4_mesh, &p, NULL, scale, &opt);
		return;
	}
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
		if (prop_defs[def].solid)            /* colisão pela grade espacial */
			collide_add_obstacle(cx * TILE_SIZE + TILE_SIZE / 2, cz * TILE_SIZE + TILE_SIZE / 2, p->radius);
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
