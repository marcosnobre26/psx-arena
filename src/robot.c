/*
 * robot.c - Jogador ROBÔ para balancear o combate (etapa 07b)
 *
 * Só existe com DEBUG_ROBOT (./dev robo compila em build-robo/, sempre com
 * o registro de partida). A cada passo, robot_input() lê g (e o mapa: grade,
 * árvores, alcance) e ESCREVE só o controle g.in[0], como se alguém
 * estivesse jogando. A lógica do jogo não muda: o robô é "um controle".
 * Nada daqui usa g.rng (o sorteio do robô é um gerador próprio).
 *
 * O que ele faz, em ordem de prioridade:
 *   1. vida < 35% e mana para CURA: cura;
 *   2. vida baixa com o GRANDE perto: CLARAO (se ele está em cima) e foge;
 *   3. inimigo ameaçando (perseguindo ou muito perto): luta com a melhor
 *      arma (de longe: espingarda, pistola, arco, bola de fogo; de perto:
 *      machado ou espada). Contra o GRANDE de perto: bate e recua (a arma
 *      alcança mais longe que o golpe dele);
 *   4. item útil ou flecha no chão a até ~10 células de caminho: vai pegar;
 *   5. caixa perto: atira (arco com flechas, ou mana sobrando) ou golpeia;
 *   6. objetivo da fase: inimigos (CACADA), saída (TRAVESSIA), relíquias e
 *      depois a saída (RELIQUIAS), o centro da clareira (CERCO);
 *   7. sem caminho: anda até uma célula sorteada por perto.
 * Lanterna: ligada enquanto houver pilha; com a pilha abaixo de 25%, só
 * quando há inimigo perto. Preso (não anda): pula e desvia de lado.
 *
 * Para não ser um robô "perfeito" (que deixaria tudo fácil demais):
 *   - só sabe o que uma pessoa saberia: a saída, as relíquias e os itens
 *     depois de chegar a ~9 células deles (até lá, EXPLORA as células não
 *     visitadas); inimigos só quando os vê (4 células no escuro, 7 com a
 *     lanterna, com linha de visão), estão colados ou perseguindo perto;
 *   - reage com atraso, mira com erro e às vezes "fica" em vez de recuar
 *     do GRANDE: tudo pela habilidade ROBOT_SKILL (50 = jogador mediano).
 *
 * Caminhos: BFS na grade (reach_walkable: o mesmo "andável" do gerador).
 *   - campo GLOBAL: distância de cada célula até o objetivo (várias
 *     origens), refeito a cada 1 s; o robô desce o campo;
 *   - janela LOCAL 25x25 em volta do jogador, refeita ao mudar de célula:
 *     distância até itens e inimigos próximos e o primeiro passo até eles.
 */
#ifdef DEBUG_ROBOT
#include <psxpad.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "game.h"

/* ------------------------------------------------------------------ */
/* Quais partidas: ROBOT_SEEDS sementes x 4 fases x 3 personagens       */
/* ------------------------------------------------------------------ */
#ifndef ROBOT_SEEDS
#define ROBOT_SEEDS 20
#endif
#define ROBOT_LEVELS 4

/* Fatia (./dev robo roda várias cópias do emulador em paralelo): o ./dev
 * troca "0/1" por "k/N" numa CÓPIA do .exe. volatile: o compilador precisa
 * ler da memória, senão usaria o "0/1" daqui. */
static volatile const char shard_tag[] = "ROBOSHARD=0/1;";

static int shard_digit(int i) {
	char c = shard_tag[i];
	return c >= '0' && c <= '9' ? c - '0' : 0;
}

/* Tempo máximo de cada fase (s): o dobro do "tempo razoável" da economia */
static const int limit_s[ROBOT_LEVELS] = { 720, 960, 720, 120 };

int robot_limit_ticks(int level) {
	return limit_s[level < ROBOT_LEVELS ? level : 0] * 60;
}

/* Partida k: 1 = rodar (preenche fase/semente/personagem), 0 = é de outra
 * fatia, -1 = acabou. Mesmas sementes do ./dev economia (0x3000 + k*7919). */
int robot_match(int k, int *level, uint32_t *seed, int *character) {
	int total = ROBOT_SEEDS * ROBOT_LEVELS * num_characters;
	if (k >= total) return -1;
	int shard = shard_digit(10), shards = shard_digit(12);
	if (shards < 1) shards = 1;
	if (k % shards != shard) return 0;
	*character = k % num_characters;
	*level = (k / num_characters) % ROBOT_LEVELS;
	*seed = 0x3000u + (uint32_t)(k / (num_characters * ROBOT_LEVELS)) * 7919u;
	return 1;
}

/* ------------------------------------------------------------------ */
/* Estado do robô (fora de g: não é a partida, é "a pessoa jogando")    */
/* ------------------------------------------------------------------ */
/* Habilidade 0..100 (50 = jogador mediano): tempo de reação, erro de mira
 * e chance de "ser ganancioso" (não recuar do GRANDE). ./dev robo -D ROBOT_SKILL=85 */
#ifndef ROBOT_SKILL
#define ROBOT_SKILL 50
#endif
#define REACT    (6 + (100 - ROBOT_SKILL) * 30 / 100)   /* passos até reagir (50: 21) */
#define AIM_ERR  ((100 - ROBOT_SKILL) * 3)              /* erro de mira (50: +-150 = 13 graus) */
#define SEE_CELLS 9                   /* itens, relíquias e saída: vistos a ~9 células */
#define MEMORY   180                  /* lembra de um inimigo por 3 s depois de perdê-lo */

#define LW   25                       /* janela local 25x25 células */
#define LR   (LW / 2)
#define INF  0xffff
#define CELL(v) ((v) / TILE_SIZE)

static uint16_t gdist[MAP_MAX_H][MAP_MAX_W];   /* passos até o objetivo */
static uint16_t queue[MAP_MAX_H * MAP_MAX_W];
static uint16_t ldist[LW * LW];                /* janela local: passos desde o jogador */
static uint8_t  lfrom[LW * LW];                /* direção pela qual chegou (0..7) */
/* O que o robô SABE (uma pessoa não vê através das árvores nem conhece o
 * mapa): células visitadas, itens e saída já vistos, inimigos percebidos. */
static uint8_t  visited[MAP_MAX_H][MAP_MAX_W / 8];
static uint8_t  known_pick[MAX_PICKUPS];
static uint8_t  notice[MAX_ENEMIES], memory[MAX_ENEMIES];

static const int8_t ndx[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
static const int8_t ndz[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

static struct {
	int      goal_timer, has_goal;
	int      lox, loz, lpcx, lpcz;    /* origem da janela local; célula do jogador */
	int      check_t, last_x, last_z; /* preso? */
	int      dodge, dodge_dir;
	int      lantern_wait;
	int      wander_x, wander_z, wander_t;
	int      low_hp_prev, fleeing;
	int      kite;                    /* recuando do GRANDE */
	int      greedy;                  /* passos "ganancioso": não recua (erro humano) */
	int      aim_err, aim_t;          /* erro de mira atual (sorteado a cada ~0,7 s) */
	int      exit_known, exploring;
	int      nav;                     /* andando (item/objetivo): vale contar "preso" */
	uint32_t rng;
	/* medidas da partida (ROBO FIM_PARTIDA) */
	int      stucks, no_heal, no_ranged_t, flees;
} rb;

static int rb_rand(int n) {                    /* gerador PRÓPRIO (nunca g.rng) */
	rb.rng = rb.rng * 1103515245u + 12345u;
	return (int)((rb.rng >> 16) % (uint32_t)n);
}

static int walk(int cx, int cz) {
	if (cx < 0 || cz < 0 || cx >= level_width() || cz >= level_height()) return 0;
	return reach_walkable(cx, cz);
}

/* Diagonal só se as duas células retas do lado também forem andáveis
 * (senão o jogador raspa no canto de um tronco ou da mata). */
static int step_ok(int cx, int cz, int k) {
	int nx = cx + ndx[k], nz = cz + ndz[k];
	if (!walk(nx, nz)) return 0;
	return k < 4 || (walk(cx + ndx[k], cz) && walk(cx, cz + ndz[k]));
}

/* ------------------------------------------------------------------ */
/* Campo global (BFS de várias origens: os objetivos)                  */
/* ------------------------------------------------------------------ */
static int qh, qt;

static void goal_clear(void) {
	memset(gdist, 0xff, sizeof(gdist));
	qh = qt = 0;
	rb.has_goal = 0;
}

static void goal_add(int x, int z) {
	int cx = CELL(x), cz = CELL(z);
	if (cx < 0 || cz < 0 || cx >= level_width() || cz >= level_height()) return;
	if (gdist[cz][cx] == 0) return;
	gdist[cz][cx] = 0;
	queue[qt++] = (uint16_t)(cz * MAP_MAX_W + cx);
	rb.has_goal = 1;
}

static void goal_flood(void) {
	while (qh < qt) {
		int i = queue[qh++], cx = i % MAP_MAX_W, cz = i / MAP_MAX_W;
		int d = gdist[cz][cx] + 1;
		for (int k = 0; k < 8; k++) {
			int nx = cx + ndx[k], nz = cz + ndz[k];
			if (nx < 0 || nz < 0 || nx >= level_width() || nz >= level_height()) continue;
			if (gdist[nz][nx] <= d || !step_ok(cx, cz, k)) continue;
			gdist[nz][nx] = (uint16_t)d;
			queue[qt++] = (uint16_t)(nz * MAP_MAX_W + nx);
		}
	}
}

static int visited_get(int cx, int cz) { return (visited[cz][cx >> 3] >> (cx & 7)) & 1; }

/* Marca como visto o quadrado de raio 4 em volta do jogador */
static void visit(int pcx, int pcz) {
	for (int z = pcz - 4; z <= pcz + 4; z++)
		for (int x = pcx - 4; x <= pcx + 4; x++)
			if (x >= 0 && z >= 0 && x < MAP_MAX_W && z < MAP_MAX_H)
				visited[z][x >> 3] |= 1 << (x & 7);
}

static int enemy_known(int i) {
	return g.enemies[i].active && notice[i] >= REACT && memory[i] > 0;
}

/* Explorar: o campo leva à célula andável não visitada mais perto */
static void goal_explore(void) {
	for (int pass = 0; pass < 2 && !rb.has_goal; pass++) {
		for (int z = 0; z < level_height(); z++)
			for (int x = 0; x < level_width(); x++)
				if (!visited_get(x, z) && walk(x, z))
					goal_add(x * TILE_SIZE + TILE_SIZE / 2, z * TILE_SIZE + TILE_SIZE / 2);
		if (!rb.has_goal) memset(visited, 0, sizeof(visited));   /* viu tudo: recomeça */
	}
	rb.exploring = 1;
}

/* Os objetivos da fase agora (só o que o robô já viu; senão, explora) */
static void goal_update(const PLAYER *p) {
	goal_clear();
	rb.exploring = 0;
	switch (level_defs[g.level].objective) {
	case OBJ_KILL_ALL:
		for (int i = 0; i < MAX_ENEMIES; i++)
			if (enemy_known(i)) goal_add(g.enemies[i].pos.vx, g.enemies[i].pos.vz);
		break;
	case OBJ_REACH_EXIT:
		if (rb.exit_known) goal_add(g.exit_x, g.exit_z);
		break;
	case OBJ_COLLECT:
		if (g.quest < g.quest_need) {
			for (int i = 0; i < MAX_PICKUPS; i++)
				if (g.pickups[i].active && g.pickups[i].type == PICK_QUEST && known_pick[i])
					goal_add(g.pickups[i].pos.vx, g.pickups[i].pos.vz);
		} else if (g.has_exit && rb.exit_known) {
			goal_add(g.exit_x, g.exit_z);
		}
		break;
	case OBJ_SURVIVE:
		goal_add(g.spawn_x, g.spawn_z);       /* o centro da clareira */
		break;
	}
	if (g.has_exit && g.exit_open && level_defs[g.level].objective != OBJ_SURVIVE) {
		goal_clear();                         /* objetivo cumprido: saída */
		if (rb.exit_known) goal_add(g.exit_x, g.exit_z);
	}
	if (!rb.has_goal) goal_explore();
	goal_flood();
	(void)p;
}

/* Próxima célula descendo o campo global (0 = sem caminho / já chegou) */
static int goal_next(const PLAYER *p, int *nx, int *nz) {
	int cx = CELL(p->pos.vx), cz = CELL(p->pos.vz);
	int best = gdist[cz][cx], found = 0;
	if (best == 0) return 0;
	for (int k = 0; k < 8; k++) {
		int x = cx + ndx[k], z = cz + ndz[k];
		if (x < 0 || z < 0 || x >= level_width() || z >= level_height()) continue;
		if (gdist[z][x] < best && step_ok(cx, cz, k)) {
			best = gdist[z][x];
			*nx = x; *nz = z;
			found = 1;
		}
	}
	return found;
}

/* ------------------------------------------------------------------ */
/* Janela local (BFS a partir do jogador, com o caminho de volta)       */
/* ------------------------------------------------------------------ */
static void local_flood(int pcx, int pcz) {
	rb.lox = pcx - LR; rb.loz = pcz - LR;
	rb.lpcx = pcx; rb.lpcz = pcz;
	memset(ldist, 0xff, sizeof(ldist));
	int h = 0, t = 0;
	ldist[LR * LW + LR] = 0;
	queue[t++] = (uint16_t)(LR * LW + LR);
	while (h < t) {
		int i = queue[h++], lx = i % LW, lz = i / LW;
		int cx = rb.lox + lx, cz = rb.loz + lz;
		for (int k = 0; k < 8; k++) {
			int nlx = lx + ndx[k], nlz = lz + ndz[k];
			if (nlx < 0 || nlz < 0 || nlx >= LW || nlz >= LW) continue;
			int j = nlz * LW + nlx;
			if (ldist[j] != INF || !step_ok(cx, cz, k)) continue;
			ldist[j] = ldist[i] + 1;
			lfrom[j] = (uint8_t)k;
			queue[t++] = (uint16_t)j;
		}
	}
}

/* Passos (pela janela) até a célula; o alvo em célula bloqueada (inimigo
 * ao lado de um tronco) vale pela vizinha mais perto. */
static int local_dist(int cx, int cz, int *rx, int *rz) {
	int best = INF;
	for (int k = -1; k < 8; k++) {
		int x = cx + (k < 0 ? 0 : ndx[k]), z = cz + (k < 0 ? 0 : ndz[k]);
		int lx = x - rb.lox, lz = z - rb.loz;
		if (lx < 0 || lz < 0 || lx >= LW || lz >= LW) continue;
		int d = ldist[lz * LW + lx] + (k >= 0);
		if (ldist[lz * LW + lx] != INF && d < best) {
			best = d;
			if (rx) { *rx = x; *rz = z; }
		}
		if (k < 0 && best != INF) break;      /* a própria célula é andável */
	}
	return best;
}

/* Primeira célula do caminho do jogador até (cx,cz) (que já tem distância) */
static void local_first(int cx, int cz, int *nx, int *nz) {
	int lx = cx - rb.lox, lz = cz - rb.loz;
	while (ldist[lz * LW + lx] > 1) {
		int k = lfrom[lz * LW + lx];
		lx -= ndx[k]; lz -= ndz[k];
	}
	*nx = rb.lox + lx; *nz = rb.loz + lz;
}

/* Vai até (x,z) pela janela local (ou reto, se estiver na mesma célula) */
static int local_go(const PLAYER *p, int x, int z, int *nx, int *nz) {
	int cx = CELL(x), cz = CELL(z), rx, rz;
	if (local_dist(cx, cz, &rx, &rz) == INF) return 0;
	if (rx == rb.lpcx && rz == rb.lpcz) { *nx = rx; *nz = rz; return 2; }   /* já está */
	local_first(rx, rz, nx, nz);
	(void)p;
	return 1;
}

/* ------------------------------------------------------------------ */
/* Controle                                                             */
/* ------------------------------------------------------------------ */

/* Direção do MUNDO -> analógico (o jogo gira o analógico por g.cam_yaw;
 * aqui é a conta inversa). mag pequeno: só vira, quase sem andar. */
static void stick_world(INPUT *in, int wx, int wz, int mag) {
	if (!wx && !wz) return;
	int a = angle_of(wx, wz);
	int dx = isin(a), dz = icos(a);
	int s = isin(g.cam_yaw), c = icos(g.cam_yaw);
	int ix = (dx * c - dz * s) >> 12, iz = (dx * s + dz * c) >> 12;
	in->lx = (ix * mag) >> 12;
	in->ly = -((iz * mag) >> 12);
	if (!in->lx && !in->ly) in->lx = 1;          /* nunca "solto" quando quer virar */
}

static void go_cell(INPUT *in, const PLAYER *p, int cx, int cz) {
	int tx = cx * TILE_SIZE + TILE_SIZE / 2, tz = cz * TILE_SIZE + TILE_SIZE / 2;
	stick_world(in, tx - p->pos.vx, tz - p->pos.vz, 127);
	if (level_cell_low(cx, cz) && p->on_ground)  /* tronco caído: pula */
		in->pressed |= PAD_CROSS;
}

static void face(INPUT *in, const PLAYER *p, int x, int z) {
	stick_world(in, x - p->pos.vx, z - p->pos.vz, 20);
}

static int owns(const PLAYER *p, int w) { return (p->weapons_owned >> w) & 1; }

/* Troca até a arma w (TRIÂNGULO avança uma por passo). 1 = já está nela. */
static int select_weapon(INPUT *in, const PLAYER *p, int w) {
	if (p->weapon == w) return 1;
	if (p->swing == 0) in->pressed |= PAD_TRIANGLE;
	return 0;
}

/* ------------------------------------------------------------------ */
/* Combate                                                              */
/* ------------------------------------------------------------------ */
#define FIREBALL_KEEP 40              /* mana guardada para uma CURA */

static void flee(INPUT *in, const PLAYER *p, const ENEMY *e);

static int best_melee(const PLAYER *p) {
	return owns(p, W_AXE) ? W_AXE : W_SWORD;  /* machado: mais dano por segundo */
}

/* Arma de longe disponível para esta distância (-1 = nenhuma) */
static int best_ranged(const PLAYER *p, int dist) {
	if (owns(p, W_SHOTGUN) && p->bullets > 0 && dist < 1000) return W_SHOTGUN;
	if (owns(p, W_PISTOL) && p->bullets > 0 && dist < 2400) return W_PISTOL;
	if (owns(p, W_BOW) && p->arrows > 0 && dist < 2400) return W_BOW;
	if (p->energy >= weapon_defs[W_FIREBALL].cost + FIREBALL_KEEP && dist < 1500) return W_FIREBALL;
	return -1;
}

/* Usa a arma w contra o ponto (x,z): mira e aperta/segura/solta QUADRADO */
static void use_weapon(INPUT *in, const PLAYER *p, int w, int x, int z, int dist) {
	const WEAPON_DEF *d = &weapon_defs[w];
	int diff = abs(angle_diff(p->angle, angle_of(x - p->pos.vx, z - p->pos.vz)));
	if (!select_weapon(in, p, w)) return;
	switch (d->kind) {
	case WK_MELEE:
		if (diff < d->arc - 64) in->held |= PAD_SQUARE;
		break;
	case WK_BOW:                                 /* segura até carregar; solta mirando */
		if (p->charge < BOW_CHARGE && !(dist < 500 && p->charge > 20))
			in->held |= PAD_SQUARE;
		else if (diff > 60)
			in->held |= PAD_SQUARE;                  /* ainda virando: continua segurando */
		break;
	default:                                     /* magia e arma de fogo */
		if (diff < (d->kind == WK_GUN ? GUN_AIM_CONE : 80)) in->held |= PAD_SQUARE;
		break;
	}
}

static void fight(INPUT *in, const PLAYER *p, const ENEMY *e, int dist, int los) {
	int heavy = enemy_defs[e->type].heavy;
	int rad = enemy_radius(e);
	int w = -1;
	if (los && dist > (heavy ? 640 : 520))
		w = best_ranged(p, dist);
	if (w < 0 && los && dist > 520)
		rb.no_ranged_t++;                        /* queria atirar de longe e não tinha com quê */
	if (w >= 0) {
		/* mira com erro (habilidade): o ponto mirado gira rb.aim_err */
		int a = angle_of(e->pos.vx - p->pos.vx, e->pos.vz - p->pos.vz) + rb.aim_err;
		int ax = p->pos.vx + ((isin(a & 4095) * dist) >> 12), az = p->pos.vz + ((icos(a & 4095) * dist) >> 12);
		face(in, p, ax, az);
		use_weapon(in, p, w, ax, az, dist);
		return;
	}
	w = best_melee(p);
	int reach = weapon_defs[w].reach + rad;
	if (heavy) {
		/* GRANDE (lento, golpe forte): bate e recua. O alcance da arma
		 * (reach + raio) passa do alcance do golpe dele (raios + 16).
		 * Só começa o golpe se, no passo do acerto (hit_from), ele ainda
		 * estiver fora do alcance dele; depois recua até a ponta do alcance
		 * da arma (rb.kite) antes de virar de novo. */
		const WEAPON_DEF *wd = &weapon_defs[w];
		int contact = rad + PLAYER_RADIUS + 16;
		int closing = e->state == EN_CHASE ? enemy_defs[e->type].speed * wd->hit_from : 0;
		int hitting = p->swing > 0 && p->swing <= wd->hit_to;
		select_weapon(in, p, w);
		if (rb.greedy > 0) rb.greedy--;
		if (!hitting && dist < contact + 50 && !rb.kite && !rb.greedy) {
			/* hora de recuar. Erro humano (100 - habilidade)%: fica e troca golpes */
			if (rb_rand(100) >= ROBOT_SKILL) rb.greedy = 60;
			else rb.kite = 1;
		}
		if (rb.kite && dist >= reach - 20) rb.kite = 0;
		if (rb.kite && !hitting) {
			flee(in, p, e);
		} else if (dist > reach - 30) {
			stick_world(in, e->pos.vx - p->pos.vx, e->pos.vz - p->pos.vz, los ? 60 : 127);
		} else {
			face(in, p, e->pos.vx, e->pos.vz);
			if (rb.greedy || dist - closing >= contact + 10) use_weapon(in, p, w, e->pos.vx, e->pos.vz, dist);
		}
		return;
	}
	if (dist > reach - 40) {                     /* chega perto */
		int nx, nz;
		if (los) stick_world(in, e->pos.vx - p->pos.vx, e->pos.vz - p->pos.vz, 127);
		else if (local_go(p, e->pos.vx, e->pos.vz, &nx, &nz)) go_cell(in, p, nx, nz);
		else stick_world(in, e->pos.vx - p->pos.vx, e->pos.vz - p->pos.vz, 127);
		select_weapon(in, p, w);
	} else {
		face(in, p, e->pos.vx, e->pos.vz);
	}
	if (dist <= reach) use_weapon(in, p, w, e->pos.vx, e->pos.vz, dist);
}

/* Foge do inimigo e: a célula vizinha que mais afasta (prefere a do objetivo) */
static void flee(INPUT *in, const PLAYER *p, const ENEMY *e) {
	int cx = CELL(p->pos.vx), cz = CELL(p->pos.vz), bx = -1, bz = 0, best = -1;
	for (int k = 0; k < 8; k++) {
		if (!step_ok(cx, cz, k)) continue;
		int x = (cx + ndx[k]) * TILE_SIZE + TILE_SIZE / 2, z = (cz + ndz[k]) * TILE_SIZE + TILE_SIZE / 2;
		int d = dist2d(x - e->pos.vx, z - e->pos.vz);
		if (gdist[cz + ndz[k]][cx + ndx[k]] < gdist[cz][cx]) d += 128;   /* rumo ao objetivo */
		if (d > best) { best = d; bx = cx + ndx[k]; bz = cz + ndz[k]; }
	}
	if (bx >= 0) go_cell(in, p, bx, bz);
	else stick_world(in, p->pos.vx - e->pos.vx, p->pos.vz - e->pos.vz, 127);
}

/* ------------------------------------------------------------------ */
/* Itens                                                                */
/* ------------------------------------------------------------------ */
static int wants_pickup(const PLAYER *p, int type) {
	switch (type) {
	case PICK_HEALTH:  return p->hp <= p->max_hp - 20;
	case PICK_ENERGY:  return p->energy <= PLAYER_MAX_ENERGY - 20;
	case PICK_BATTERY: return p->battery < BATTERY_MAX * 4 / 5;
	case PICK_ARROWS:  return p->arrows < MAX_ARROWS;
	case PICK_AMMO:    return p->bullets < MAX_BULLETS_AMMO;
	default:           return 1;                 /* gema, arma, relíquia, chave */
	}
}

/* Item (ou flecha no chão) mais perto pelo caminho, a até max passos */
static int nearest_item(const PLAYER *p, int max, int *x, int *z) {
	int best = max + 1;
	for (int i = 0; i < MAX_PICKUPS; i++) {
		const PICKUP *it = &g.pickups[i];
		if (!it->active || !known_pick[i] || !wants_pickup(p, it->type)) continue;
		int d = local_dist(CELL(it->pos.vx), CELL(it->pos.vz), NULL, NULL);
		if (d < best) { best = d; *x = it->pos.vx; *z = it->pos.vz; }
	}
	if (p->arrows < MAX_ARROWS)
		for (int i = 0; i < MAX_GROUND_ARROWS; i++) {
			if (!g.garrow[i].active) continue;
			int d = local_dist(CELL(g.garrow[i].x), CELL(g.garrow[i].z), NULL, NULL);
			if (d < best) { best = d; *x = g.garrow[i].x; *z = g.garrow[i].z; }
		}
	return best <= max;
}

/* Caixa perto que dá para acertar (arco com flechas ou mana sobrando) */
static int crate_target(const PLAYER *p, int *w, int *x, int *z) {
	int ranged = 1;
	if (owns(p, W_BOW) && p->arrows >= 3) *w = W_BOW;
	else if (p->energy >= 90) *w = W_FIREBALL;
	else { *w = best_melee(p); ranged = 0; }     /* golpe também quebra caixa (07b) */
	for (int i = 0; i < MAX_CRATES; i++) {
		const CRATE *c = &g.crates[i];
		if (!c->active) continue;
		int cx = c->cx * TILE_SIZE + TILE_SIZE / 2, cz = c->cz * TILE_SIZE + TILE_SIZE / 2;
		int dx = cx - p->pos.vx, dz = cz - p->pos.vz, d = dist2d(dx, dz);
		if (!ranged) {                            /* de perto: até 6 passos de caminho */
			if (local_dist(c->cx, c->cz, NULL, NULL) > 6) continue;
			*x = cx; *z = cz;
			return 2;
		}
		if (d > 5 * TILE_SIZE || d < 200) continue;
		/* linha de visão até a frente da caixa (a célula dela é sólida) */
		int a = angle_of(dx, dz);
		if (!collide_line_of_sight(p->pos.vx, p->pos.vz, cx - ((isin(a) * 170) >> 12),
		                           cz - ((icos(a) * 170) >> 12)))
			continue;
		*x = cx; *z = cz;
		return 1;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* Um passo                                                             */
/* ------------------------------------------------------------------ */
void robot_begin(uint32_t salt) {
	memset(&rb, 0, sizeof(rb));
	rb.rng = 0x9e3779b9u ^ salt;
	rb.last_x = g.players[0].pos.vx;
	rb.last_z = g.players[0].pos.vz;
	rb.lpcx = -1000;
	memset(visited, 0, sizeof(visited));
	memset(known_pick, 0, sizeof(known_pick));
	memset(notice, 0, sizeof(notice));
	memset(memory, 0, sizeof(memory));
}

void robot_input(INPUT *in) {
	memset(in, 0, sizeof(*in));
	in->connected = 1;
	const PLAYER *p = &g.players[0];
	if (g.state != STATE_PLAY || !player_alive(p)) return;

	int pcx = CELL(p->pos.vx), pcz = CELL(p->pos.vz);
	if (--rb.goal_timer <= 0) {                  /* objetivo: 1 vez por segundo */
		goal_update(p);
		rb.goal_timer = 60;
	}
	if (pcx != rb.lpcx || pcz != rb.lpcz || (g.frame & 15) == 0) {
		if (pcx != rb.lpcx || pcz != rb.lpcz) visit(pcx, pcz);
		local_flood(pcx, pcz);
	}
	if (rb.exploring && gdist[pcz][pcx] == 0) rb.goal_timer = 0;   /* chegou: próxima */

	/* o que se vê: itens e saída a ~SEE_CELLS (brilham no escuro) */
	for (int i = 0; i < MAX_PICKUPS; i++) {
		const PICKUP *it = &g.pickups[i];
		if (!it->active) known_pick[i] = 0;
		else if (!known_pick[i] && dist2d(it->pos.vx - p->pos.vx, it->pos.vz - p->pos.vz) < SEE_CELLS * TILE_SIZE)
			known_pick[i] = 1;
	}
	if (g.has_exit && !rb.exit_known &&
	    dist2d(g.exit_x - p->pos.vx, g.exit_z - p->pos.vz) < SEE_CELLS * TILE_SIZE)
		rb.exit_known = 1;

	/* inimigos PERCEBIDOS: à vista (4 células no escuro, 7 com a lanterna,
	 * com linha de visão), muito perto, ou perseguindo perto (ouve-se).
	 * Reage depois de REACT passos percebendo; lembra por MEMORY passos. */
	const ENEMY *e = NULL, *big = NULL;
	int ed = 0x7fffffff, bd = 0x7fffffff, near8 = 0;
	int see = (p->lantern_on ? 7 : 4) * TILE_SIZE;
	for (int i = 0; i < MAX_ENEMIES; i++) {
		const ENEMY *o = &g.enemies[i];
		if (!o->active) { notice[i] = memory[i] = 0; continue; }
		int d = dist2d(o->pos.vx - p->pos.vx, o->pos.vz - p->pos.vz);
		int perceived = d < 2 * TILE_SIZE || (o->state == EN_CHASE && d < 5 * TILE_SIZE) ||
		                (d < see && collide_line_of_sight(p->pos.vx, p->pos.vz, o->pos.vx, o->pos.vz));
		if (perceived) {
			if (notice[i] < 255) notice[i]++;
			memory[i] = MEMORY;
		} else if (memory[i] > 0 && --memory[i] == 0) {
			notice[i] = 0;
		}
		if (!enemy_known(i)) continue;
		if (d < 8 * TILE_SIZE) near8 = 1;
		if (d < ed) { ed = d; e = o; }
		if (enemy_defs[o->type].heavy && d < bd) { bd = d; big = o; }
	}
	int los = e && ed < 10 * TILE_SIZE &&
	          collide_line_of_sight(p->pos.vx, p->pos.vz, e->pos.vx, e->pos.vz);
	if (--rb.aim_t <= 0) {                       /* erro de mira muda a cada ~0,7 s */
		rb.aim_err = AIM_ERR ? rb_rand(2 * AIM_ERR + 1) - AIM_ERR : 0;
		rb.aim_t = 40;
	}

	/* lanterna: ligada com pilha; pilha fraca: só com inimigo perto */
	if (rb.lantern_wait > 0) rb.lantern_wait--;
	else {
		int want = p->battery > 0 && (p->battery > BATTERY_MAX / 4 || near8);
		if (want != (p->lantern_on != 0)) {
			in->pressed |= PAD_SELECT;
			rb.lantern_wait = 60;
		}
	}

	int low = p->hp * 100 < p->max_hp * 35;
	if (low && !rb.low_hp_prev && p->energy < weapon_defs[W_HEAL].cost)
		rb.no_heal++;                             /* precisou de cura e não tinha mana */
	rb.low_hp_prev = low;

	int nx, nz, ix = 0, iz = 0, w, r;
	if (rb.fleeing > 0) rb.fleeing--;
	rb.nav = 0;
	/* 1. cura */
	if (low && p->energy >= weapon_defs[W_HEAL].cost) {
		if (select_weapon(in, p, W_HEAL) && p->cooldown == 0) in->held |= PAD_SQUARE;
		if (e && ed < 3 * TILE_SIZE) flee(in, p, e);
		goto done;
	}
	/* 2. vida baixa e o GRANDE perto: clarão (se ele estiver em cima) e fuga */
	if (low && big && bd < 6 * TILE_SIZE) {
		if (bd < 2 * TILE_SIZE && p->energy >= weapon_defs[W_FLASH].cost && big->stun == 0) {
			if (select_weapon(in, p, W_FLASH) && p->cooldown == 0) in->held |= PAD_SQUARE;
		}
		if (!rb.fleeing) rb.flees++;              /* conta cada fuga, não cada passo */
		rb.fleeing = 60;
		flee(in, p, big);
		goto done;
	}
	/* 3. luta: quem persegue, quem está muito perto, ou (CACADA) quem se vê */
	if (e && ((e->state == EN_CHASE && ed < 7 * TILE_SIZE) || ed < 3 * TILE_SIZE ||
	          (los && level_defs[g.level].objective == OBJ_KILL_ALL && ed < 8 * TILE_SIZE))) {
		fight(in, p, e, ed, los);
		goto done;
	}
	/* 4. itens perto */
	if (nearest_item(p, 10, &ix, &iz) && local_go(p, ix, iz, &nx, &nz)) {
		if (nx == pcx && nz == pcz) stick_world(in, ix - p->pos.vx, iz - p->pos.vz, 127);
		else go_cell(in, p, nx, nz);
		rb.nav = 1;
		goto done;
	}
	/* 5. caixa perto: de longe atira; de perto chega ao lado e golpeia */
	if ((r = crate_target(p, &w, &ix, &iz)) != 0) {
		int d = dist2d(ix - p->pos.vx, iz - p->pos.vz);
		if (r == 2 && d > 300) {
			if (local_go(p, ix, iz, &nx, &nz) == 1) go_cell(in, p, nx, nz);
			else stick_world(in, ix - p->pos.vx, iz - p->pos.vz, 127);
			rb.nav = 1;
		} else {
			face(in, p, ix, iz);
			use_weapon(in, p, w, ix, iz, d);
		}
		goto done;
	}
	/* (sem luta: volta para a arma de perto, para não ser pego com o arco) */
	if (p->weapon != best_melee(p) && p->charge == 0) select_weapon(in, p, best_melee(p));
	/* 6. objetivo */
	if (goal_next(p, &nx, &nz)) {
		go_cell(in, p, nx, nz);
		rb.wander_t = 0;
		rb.nav = 1;
		goto done;
	}
	if (rb.has_goal && gdist[pcz][pcx] == 0) {  /* chegou (CERCO: fica na clareira) */
		if (level_defs[g.level].objective == OBJ_SURVIVE) goto done;
	}
	/* 7. sem caminho: passeia por perto */
	if (--rb.wander_t <= 0 || (rb.wander_x == pcx && rb.wander_z == pcz)) {
		rb.wander_t = 300;
		for (int tries = 0; tries < 40; tries++) {
			int x = rb.lox + rb_rand(LW), z = rb.loz + rb_rand(LW);
			int d = local_dist(x, z, NULL, NULL);
			if (d != INF && d >= 4) { rb.wander_x = x; rb.wander_z = z; break; }
		}
	}
	if (local_go(p, rb.wander_x * TILE_SIZE + 128, rb.wander_z * TILE_SIZE + 128, &nx, &nz) == 1) {
		go_cell(in, p, nx, nz);
		rb.nav = 1;
	}

done:
	/* preso? (andando para um item/objetivo, quase não saiu do lugar em 0,75 s) */
	if (!rb.nav) {                               /* lutando/fugindo: não conta */
		rb.dodge = rb.check_t = 0;
		rb.last_x = p->pos.vx;
		rb.last_z = p->pos.vz;
	} else if (rb.dodge > 0) {
		rb.dodge--;
		int a = angle_of(in->lx, -in->ly) + (rb.dodge_dir ? 1024 : -1024);
		in->lx = (isin(a) * 127) >> 12;
		in->ly = -((icos(a) * 127) >> 12);
		if (rb.dodge == 18 && p->on_ground) in->pressed |= PAD_CROSS;
	} else if (++rb.check_t >= 45) {
		int moved = dist2d(p->pos.vx - rb.last_x, p->pos.vz - rb.last_z);
		int wants = abs(in->lx) + abs(in->ly) > 60;
		if (wants && moved < 48) {
			rb.dodge = 20;
			rb.dodge_dir = rb_rand(2);
			rb.stucks++;
		}
		rb.check_t = 0;
		rb.last_x = p->pos.vx;
		rb.last_z = p->pos.vz;
	}
	in->held |= in->pressed;
}

/* Fim da partida: o que o robô sentiu falta (complementa o PLAYLOG FIM_FASE) */
void robot_report(const char *reason) {
	printf("ROBO FIM_PARTIDA fase=%d semente=%08x personagem=%d motivo=%s t=%d presos=%d "
	       "sem_cura=%d sem_distancia_s=%d fugas=%d\n",
	       g.level + 1, (unsigned)g.seed, g.players[0].character, reason, g.play_frames / 60,
	       rb.stucks, rb.no_heal, rb.no_ranged_t / 60, rb.flees);
}
#endif
