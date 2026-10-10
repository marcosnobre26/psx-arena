/*
 * collision.c - Colisão entre tudo que existe no chão da fase
 *
 * Cada coisa ocupa um CÍRCULO no plano X/Z:
 *   - paredes e caixas      -> células do mapa (level_blocked)
 *   - objetos de cenário    -> círculo com o raio do modelo (PROP.radius)
 *   - jogadores e inimigos  -> círculo com o raio do personagem
 *
 * Para mover algo:   collide_move(&pos, dx, dz, raio, eu_mesmo, COL_ALL);
 * O movimento é testado separadamente em X e em Z, então quem bate numa
 * parede de lado "desliza" nela em vez de parar.
 *
 * Detalhes que evitam travamentos:
 *   - "self" é o próprio objeto, ignorado no teste;
 *   - se dois círculos já estão sobrepostos (ex.: um empurrão), o movimento
 *     que os AFASTA é permitido; só é bloqueado o que os aproxima;
 *   - quem está alto no ar (pulando) passa por cima de jogadores e inimigos.
 */
#include <stdlib.h>
#include "game.h"

#define JUMP_CLEAR  150     /* altura do pulo que passa por cima de alguém */

/* Contador de testes de círculo (depuração: overlay L2, "COL"). Mostra o
 * custo da colisão: hoje cada teste percorre todos os objetos. */
static int col_tests, col_tests_last;

void collide_tick_begin(void) {
	col_tests_last = col_tests;
	col_tests = 0;
}

int collide_stats(void) {
	return col_tests_last;
}

/* Testes do passo em andamento (válido logo depois de game_tick) */
int collide_tests_now(void) {
	return col_tests;
}

/* Testa um círculo (x,z,r) contra outro (ox,oz,orad).
 * Bloqueia só se vai sobrepor E está se aproximando (de from_x/from_z). */
static int circle_blocks(int x, int z, int r, int ox, int oz, int orad,
                         int from_x, int from_z) {
	col_tests++;
	int min = r + orad;
	int dx = x - ox, dz = z - oz;
	if (dx >= min || dx <= -min || dz >= min || dz <= -min)
		return 0;                                   /* longe: descarte rápido */
	int d2 = dx * dx + dz * dz;
	if (d2 >= min * min)
		return 0;
	int fx = from_x - ox, fz = from_z - oz;
	return d2 < fx * fx + fz * fz;                  /* aproximando? bloqueia */
}

int collide_blocked(int x, int z, int y, int radius, const void *self, int mask,
                    int from_x, int from_z) {
	if (level_blocked(x, z, radius))
		return 1;

	for (int i = 0; i < MAX_PROPS; i++) {
		const PROP *p = &g.props[i];
		if (!p->active || !prop_defs[p->def].solid) continue;
		int px = p->cx * TILE_SIZE + TILE_SIZE / 2, pz = p->cz * TILE_SIZE + TILE_SIZE / 2;
		if (circle_blocks(x, z, radius, px, pz, p->radius, from_x, from_z))
			return 1;
	}

	if (mask & COL_PLAYERS) {
		for (int i = 0; i < MAX_PLAYERS; i++) {
			const PLAYER *o = &g.players[i];
			if (o == self || !player_alive(o)) continue;
			if (abs(o->pos.vy - y) > JUMP_CLEAR) continue;   /* um está no ar */
			if (circle_blocks(x, z, radius, o->pos.vx, o->pos.vz, PLAYER_RADIUS, from_x, from_z))
				return 1;
		}
	}

	if (mask & COL_ENEMIES) {
		for (int i = 0; i < MAX_ENEMIES; i++) {
			const ENEMY *e = &g.enemies[i];
			if (e == self || !e->active) continue;
			if (abs(e->pos.vy - y) > JUMP_CLEAR) continue;
			if (circle_blocks(x, z, radius, e->pos.vx, e->pos.vz, enemy_radius(e), from_x, from_z))
				return 1;
		}
	}
	return 0;
}

int collide_move(VECTOR *pos, int dx, int dz, int radius, const void *self, int mask) {
	int hit = 0;
	if (dx) {
		if (!collide_blocked(pos->vx + dx, pos->vz, pos->vy, radius, self, mask, pos->vx, pos->vz))
			pos->vx += dx;
		else
			hit = 1;
	}
	if (dz) {
		if (!collide_blocked(pos->vx, pos->vz + dz, pos->vy, radius, self, mask, pos->vx, pos->vz))
			pos->vz += dz;
		else
			hit = 1;
	}
	return hit;
}

int collide_prop_at(int x, int z, int radius) {
	for (int i = 0; i < MAX_PROPS; i++) {
		const PROP *p = &g.props[i];
		if (!p->active || !prop_defs[p->def].solid) continue;
		col_tests++;
		int px = p->cx * TILE_SIZE + TILE_SIZE / 2, pz = p->cz * TILE_SIZE + TILE_SIZE / 2;
		int min = radius + p->radius;
		int dx = x - px, dz = z - pz;
		if (abs(dx) < min && abs(dz) < min && dx * dx + dz * dz < min * min)
			return 1;
	}
	return 0;
}
