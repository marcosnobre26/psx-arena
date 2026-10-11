/*
 * events.c - A floresta que muda: eventos durante a partida (etapa 06)
 *
 * Cada fase tem uma tabela de EVENT_DEF (levels.c): GATILHO -> AÇÃO.
 * Gatilhos: tempo, aleatório (a cada 45-90 s), entrar numa área, pegar o
 * N-ésimo item de missão, derrotar N inimigos, encostar na barreira de
 * raízes com uma chave. O gatilho põe a ação numa FILA com prioridade; a
 * cada passo sai no máximo uma ação, respeitando o ritmo:
 *   - nada nos primeiros EV_GRACE passos da fase;
 *   - no máximo uma ação que MUDA O MAPA a cada EV_MAP_COOLDOWN passos.
 *
 * Tudo isto é LÓGICA: muda g e depende só de g.in, do tempo de jogo em
 * passos (g.play_frames) e de g.rng. Nada de VSync nem da câmera visual
 * (a "visão" é a do grupo + g.cam_yaw, como nos reforços do objective.c).
 * O que é só visual (tremor da câmera, névoa desenhada, sons de ambiente)
 * LÊ g.ev, nunca escreve.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"

#ifdef DEBUG_BENCHMARK
#include <hwregs_c.h>
static int prof_job, prof_impact, prof_check;   /* perfil (bench): pior, em linhas */
void events_prof(int *job, int *impact) {
	*job = prof_job; *impact = prof_impact;
	prof_job = prof_impact = 0;
}
int events_prof_check(void) { int c = prof_check; prof_check = 0; return c; }
#define EVP_BEGIN() uint16_t evp_t0 = TIMER_VALUE(1)
#define EVP_END(v)  do { int d = (uint16_t)(TIMER_VALUE(1) - evp_t0); if (d > (v)) (v) = d; } while (0)
#define EVP_CHECK(call) ({ uint16_t t_ = TIMER_VALUE(1); int r_ = (call); int d_ = (uint16_t)(TIMER_VALUE(1) - t_); if (d_ > prof_check) prof_check = d_; r_; })
#else
#define EVP_BEGIN()
#define EVP_END(v)
#define EVP_CHECK(call) (call)
#endif

static const LEVEL_DEF *cur(void) {
	return &level_defs[g.level];
}

const char *event_name(int action) {
	static const char *names[NUM_EV_ACTIONS] = {
		"NEVOA", "ARVORE CAI", "CAMINHO ABRE", "MATA FECHA", "ONDA", "SILENCIO",
	};
	return action >= 0 && action < NUM_EV_ACTIONS ? names[action] : "-";
}

void events_start(void) {
	g.ev.fog_far = cur()->fog_far;
	g.ev.next_random = -1;                     /* sorteado no primeiro passo */
	g.ev.last = -1;
	g.ev.job.action = -1;
	g.ev.enemy_cap = g.enemies_left;           /* a onda repõe até aqui */
	g.ev.barrier_hp = CHOP_BARRIER_HP;
}

void events_enemy_killed(void) {
	g.ev.kills++;
}

/* ------------------------------------------------------------------ */
/* Fila                                                                 */
/* ------------------------------------------------------------------ */

static void enqueue(int action, int prio, int arg, int force) {
	if (g.ev.nq >= EV_QUEUE_LEN) {             /* cheia: sai a de menor prioridade */
		int low = 0;
		for (int i = 1; i < g.ev.nq; i++)
			if (g.ev.queue[i].prio < g.ev.queue[low].prio) low = i;
		if (g.ev.queue[low].prio > prio) return;
		g.ev.queue[low] = g.ev.queue[--g.ev.nq];
	}
	g.ev.queue[g.ev.nq].action = action;
	g.ev.queue[g.ev.nq].prio = prio;
	g.ev.queue[g.ev.nq].arg = arg;
	g.ev.queue[g.ev.nq].force = force;
	g.ev.nq++;
}

/* ------------------------------------------------------------------ */
/* Ações                                                                */
/* Cada uma devolve 1 se aconteceu, 0 se não achou como (sai da fila).  */
/* ------------------------------------------------------------------ */

/* Névoa avança: desce até arg% da névoa da fase em EV_FOG_RAMP passos,
 * fica, e volta ao normal no fim de EV_FOG_TOTAL. Mais névoa = menos coisa
 * desenhada: nunca pesa no quadro. */
static int act_fog(int arg) {
	if (g.ev.fog_timer > 0) return 0;          /* já está avançando */
	g.ev.fog_target = cur()->fog_far * (arg ? arg : 55) / 100;
	g.ev.fog_timer = EV_FOG_TOTAL;
	show_message("A NEVOA AVANCA", 120);
	return 1;
}

static int act_silence(int arg) {
	(void)arg;
	if (g.ev.silence > 0) return 0;
	g.ev.silence = EV_SILENCE_TIME;
	return 1;                                  /* sem texto: o silêncio é a pista */
}

/* ---------- ajudas: grupo, visão (lógica), entidades ---------- */

/* Centro do grupo (jogadores vivos), em células; 0 se ninguém vivo */
static int group_cell(int *cx, int *cz) {
	int n = 0, x = 0, z = 0;
	for (int i = 0; i < MAX_PLAYERS; i++)
		if (player_alive(&g.players[i])) {
			x += g.players[i].pos.vx; z += g.players[i].pos.vz; n++;
		}
	if (!n) return 0;
	*cx = x / n / TILE_SIZE;
	*cz = z / n / TILE_SIZE;
	return 1;
}

/* A câmera veria (x, z)? Mesmo critério dos reforços (objective.c): cone de
 * g.cam_yaw a partir do grupo, até a névoa ATUAL (g.ev.fog_far). Só g. */
static int seen_by_camera(int x, int z) {
	int gx, gz;
	if (!group_cell(&gx, &gz)) return 0;
	int dx = x - (gx * TILE_SIZE + TILE_SIZE / 2), dz = z - (gz * TILE_SIZE + TILE_SIZE / 2);
	int rel = angle_diff(g.cam_yaw, angle_of(dx, dz));
	return rel > -700 && rel < 700 && dist2d(dx, dz) < g.ev.fog_far;
}

/* Algo (jogador, inimigo, item, caixa, prop, saída, início, ponto S) está
 * na célula (cx, cz) ou encostando nela? Árvore nova / copa não pode cair
 * em cima. Jogadores e inimigos contam com o raio (margem de meia célula). */
static int cell_busy(int cx, int cz) {
	const int m = TILE_SIZE / 2;
	int x0 = cx * TILE_SIZE - m, x1 = (cx + 1) * TILE_SIZE + m;
	int z0 = cz * TILE_SIZE - m, z1 = (cz + 1) * TILE_SIZE + m;
#define INSIDE(px, pz) ((px) >= x0 && (px) < x1 && (pz) >= z0 && (pz) < z1)
	for (int i = 0; i < MAX_PLAYERS; i++)
		if (g.players[i].active && INSIDE(g.players[i].pos.vx, g.players[i].pos.vz)) return 1;
	for (int i = 0; i < MAX_ENEMIES; i++)
		if (g.enemies[i].active && INSIDE(g.enemies[i].pos.vx, g.enemies[i].pos.vz)) return 1;
	for (int i = 0; i < MAX_PICKUPS; i++)
		if (g.pickups[i].active && INSIDE(g.pickups[i].pos.vx, g.pickups[i].pos.vz)) return 1;
	/* caixas e props: as células deles já são sólidas / têm obstáculo, e
	 * quem chama testa reach_walkable antes (mais barato) */
	for (int i = 0; i < g.num_spawn_pts; i++)
		if (g.spawn_pts[i].x / TILE_SIZE == cx && g.spawn_pts[i].z / TILE_SIZE == cz) return 1;
#undef INSIDE
	/* saída e início: nem a célula nem as vizinhas */
	if (g.has_exit && abs(g.exit_x / TILE_SIZE - cx) <= 1 && abs(g.exit_z / TILE_SIZE - cz) <= 1) return 1;
	if (abs(g.spawn_x / TILE_SIZE - cx) <= 1 && abs(g.spawn_z / TILE_SIZE - cz) <= 1) return 1;
	int bcx, bcz, br;                          /* nem no anel de raízes nem dentro */
	if (mapgen_barrier(&bcx, &bcz, &br) &&
	    (cx - bcx) * (cx - bcx) + (cz - bcz) * (cz - bcz) <= (br + 3) * (br + 3))
		return 1;
	return 0;
}

static int in_cells(int cx, int cz, const uint16_t *cells, int n) {
	uint16_t c = (uint16_t)(cx | (cz << 8));
	for (int i = 0; i < n; i++)
		if (cells[i] == c) return i;
	return -1;
}

/* Tira (x, z) de dentro das células: vai para a célula andável mais perto
 * fora delas (anéis de até 4). Devolve 1 se mudou. */
static int push_out(VECTOR *pos, const uint16_t *cells, int n) {
	int cx = pos->vx / TILE_SIZE, cz = pos->vz / TILE_SIZE;
	if (in_cells(cx, cz, cells, n) < 0) return 0;
	for (int r = 1; r <= 4; r++)
		for (int dz = -r; dz <= r; dz++)
			for (int dx = -r; dx <= r; dx++) {
				int x = cx + dx, z = cz + dz;
				if ((abs(dx) != r && abs(dz) != r) || in_cells(x, z, cells, n) >= 0) continue;
				if (!reach_get(x, z) || !reach_walkable(x, z)) continue;
				pos->vx = x * TILE_SIZE + TILE_SIZE / 2;
				pos->vz = z * TILE_SIZE + TILE_SIZE / 2;
				return 1;
			}
	return 0;
}

/* ---------- árvore cai ---------- */

/* Células que o pinheiro deitado ocupa, de (x, z) na direção dir. Tronco
 * (primeiros 45%): sólido BAIXO (pula-se); copa: sólida, com ~3 células de
 * largura no meio. Sem a célula da base. Devolve quantas (0 = não cabe). */
int ev_fall_footprint(int x, int z, int dir, int scale, uint16_t *cells, uint8_t *crown) {
	int len = (EV_FALL_LEN * scale) >> 12;
	int sx = isin(dir), sz = icos(dir);
	int n = 0, bx = x / TILE_SIZE, bz = z / TILE_SIZE;
	for (int t = 160; t <= len; t += 112) {
		int is_crown = t > len * 45 / 100;
		int px = x + ((sx * t) >> 12), pz = z + ((sz * t) >> 12);
		for (int side = -1; side <= 1; side++) {
			if (side && (!is_crown || t < len * 55 / 100 || t > len * 90 / 100)) continue;
			int qx = px + ((sz * 160 * side) >> 12), qz = pz - ((sx * 160 * side) >> 12);
			int cx = qx / TILE_SIZE, cz = qz / TILE_SIZE;
			if (qx < 0 || qz < 0 || (cx == bx && cz == bz)) continue;
			int i = in_cells(cx, cz, cells, n);
			if (i >= 0) { crown[i] |= is_crown; continue; }
			if (n >= REACH_MAX_BLOCK) return 0;
			cells[n] = (uint16_t)(cx | (cz << 8));
			crown[n] = is_crown;
			n++;
		}
	}
	return n;
}

/* A árvore i pode cair na direção dir agora? (sem a checagem de alcance) */
static int fall_fits(int x, int z, int dir, int scale, uint16_t *cells, uint8_t *crown, int *n) {
	*n = ev_fall_footprint(x, z, dir, scale, cells, crown);
	if (*n == 0) return 0;
	for (int i = 0; i < *n; i++) {
		int cx = cells[i] & 0xff, cz = cells[i] >> 8;
		if (cx < FOREST_EDGE || cz < FOREST_EDGE ||
		    cx >= level_width() - FOREST_EDGE || cz >= level_height() - FOREST_EDGE)
			return 0;
		/* cai em chão aberto e alcançável (trilha, clareira) */
		if (!reach_get(cx, cz) || !reach_walkable(cx, cz))
			return 0;
	}
	for (int i = 0; i < *n; i++)                    /* ... e sem nada em cima */
		if (cell_busy(cells[i] & 0xff, cells[i] >> 8))
			return 0;
	return 1;
}

/* Checagem passou: confere de novo o que pode ter mudado enquanto ela
 * andava (alguém entrou nas células) e começa o aviso (estalo) */
static int fall_commit(void) {
	for (int i = 0; i < g.ev.fall.n; i++)
		if (cell_busy(g.ev.fall.cells[i] & 0xff, g.ev.fall.cells[i] >> 8))
			return 0;                              /* procura outro */
	int x = g.ev.fall.x, z = g.ev.fall.z;
	level_remove_tree(g.ev.job.cand);
	int bxc = x / TILE_SIZE, bzc = z / TILE_SIZE;
	if (reach_get(bxc + 1, bzc) || reach_get(bxc - 1, bzc) || reach_get(bxc, bzc + 1) || reach_get(bxc, bzc - 1))
		reach_set(bxc, bzc, 1);                    /* a base fica livre */
	g.ev.fall.timer = EV_FALL_WARN + EV_FALL_TIME;
	sound_play_at(&sfx_estalo, x, z, VOL_ESTALO);
	return 1;
}

/* Um passo da busca: um candidato (árvore, direção) ou um pedaço da
 * checagem de alcance dele. Só a copa entra na checagem (o tronco é
 * baixo: passa-se pulando). */
static int job_fall_step(void) {
	int gx, gz;
	if (!group_cell(&gx, &gz)) return -1;
	if (g.ev.job.checking) {                       /* checagem em andamento */
		int r = EVP_CHECK(reach_step(EV_CHECK_VISITS));
		if (r == REACH_PENDING) return 0;
		g.ev.job.checking = 0;
		return r == REACH_OK ? fall_commit() : 0;
	}
	int bx0 = gx / CHUNK_CELLS - 2, bz0 = gz / CHUNK_CELLS - 2, total = 0, start;
	for (int bz = bz0; bz < bz0 + 5; bz++)
		for (int bx = bx0; bx < bx0 + 5; bx++)
			total += level_chunk_trees(bx, bz, &start);
	if (total == 0) return -1;
	int fits = 0;
	for (int scans = 0; scans < 40 && fits < EV_FITS_PER_TICK; scans++) {   /* orçamento por passo */
		if (g.ev.job.k >= total || g.ev.job.k >= 48) return -1;
		int want = (g.ev.job.start + g.ev.job.k) % total, idx = -1;
		for (int bz = bz0; bz < bz0 + 5 && idx < 0; bz++)
			for (int bx = bx0; bx < bx0 + 5 && idx < 0; bx++) {
				int c = level_chunk_trees(bx, bz, &start);
				if (want < c) idx = start + want;
				else want -= c;
			}
		int x, z, type, scale, ang;
		level_tree_info(idx, &x, &z, &type, &scale, &ang);
		int dx = x / TILE_SIZE - gx, dz = z / TILE_SIZE - gz, d2 = dx * dx + dz * dz;
		/* um pinheiro À FRENTE do grupo, a 4..12 células: o jogador vê cair */
		if (type != TREE_PINE || d2 < 4 * 4 || d2 > 12 * 12 ||
		    abs(angle_diff(g.cam_yaw, angle_of(dx, dz))) > 1200) {
			g.ev.job.k++; g.ev.job.j = 0;
			continue;
		}
		int dir = (g.ev.job.dir0 + g.ev.job.j * 512) & 4095;
		if (++g.ev.job.j >= 8) { g.ev.job.k++; g.ev.job.j = 0; }
		/* zerados: o resto do vetor (além de n) é copiado para g.ev.fall;
		 * lixo da pilha em g quebraria o hash do replay (e o modo link) */
		uint16_t cells[REACH_MAX_BLOCK] = { 0 }, blk[REACH_MAX_BLOCK] = { 0 };
		uint8_t  crown[REACH_MAX_BLOCK] = { 0 };
		int n, nb = 0;
		fits++;
		if (!fall_fits(x, z, dir, scale, cells, crown, &n)) continue;
		for (int i = 0; i < n; i++)
			if (crown[i]) blk[nb++] = cells[i];
		/* candidato: guarda e começa a checagem (anda nos próximos passos) */
		g.ev.fall.x = x; g.ev.fall.z = z; g.ev.fall.dir = dir; g.ev.fall.scale = scale;
		g.ev.fall.n = n;
		memcpy(g.ev.fall.cells, cells, sizeof(cells));
		memcpy(g.ev.fall.crown, crown, sizeof(crown));
		g.ev.job.cand = idx;
		int r = EVP_CHECK(reach_begin(x / TILE_SIZE, z / TILE_SIZE, blk, nb));
		if (r == REACH_PENDING) { g.ev.job.checking = 1; return 0; }
		return r == REACH_OK ? fall_commit() : 0;
	}
	return 0;
}

/* Impacto: marca as células (só bits: o bloco não é remontado), empurra
 * quem entrou nelas durante o aviso, põe o pinheiro deitado na lista */
static void fall_impact(void) {
	const int n = g.ev.fall.n;
	const uint16_t *cells = g.ev.fall.cells;
	for (int i = 0; i < n; i++) {
		int cx = cells[i] & 0xff, cz = cells[i] >> 8;
		level_set_flags(cx, cz, g.ev.fall.crown[i] ? CELL_SOLID : CELL_SOLID | CELL_LOW);
		if (g.ev.fall.crown[i]) reach_set(cx, cz, 0);
	}
	for (int i = 0; i < MAX_PLAYERS; i++)
		if (g.players[i].active && push_out(&g.players[i].pos, cells, n))
			collide_track(&g.players[i]);
	for (int i = 0; i < MAX_ENEMIES; i++)
		if (g.enemies[i].active && push_out(&g.enemies[i].pos, cells, n))
			collide_track(&g.enemies[i]);
	for (int i = 0; i < MAX_PICKUPS; i++)
		if (g.pickups[i].active) push_out(&g.pickups[i].pos, cells, n);
	level_add_tree(TREE_FALLEN, g.ev.fall.x, g.ev.fall.z, g.ev.fall.dir, g.ev.fall.scale);
	level_trees_changed();
	g.ev.shake = EV_SHAKE_TIME;
	sound_play_at(&sfx_queda, g.ev.fall.x, g.ev.fall.z, VOL_QUEDA);
	show_message("UMA ARVORE CAIU", 90);
	noise_emit(g.ev.fall.x, g.ev.fall.z, FALL_NOISE);   /* atrai quem está perto */

	/* o machado pode cortar (etapa 07): guarda as células (as 4 últimas) */
	int k = g.ev.fallen_next;
	g.ev.fallen_next = (k + 1) & 3;
	g.ev.fallen[k].active = 1;
	g.ev.fallen[k].x = g.ev.fall.x;
	g.ev.fallen[k].z = g.ev.fall.z;
	g.ev.fallen[k].hp = CHOP_FALLEN_HP;
	g.ev.fallen[k].n = n;
	memcpy(g.ev.fallen[k].cells, cells, sizeof(g.ev.fallen[k].cells));
}

/* ---------- a mata se fecha atrás ---------- */

/* Tetos da 05 para árvores novas: por bloco e por janela 3x3 blocos.
 * Conta as árvores em pé dos 5x5 blocos em volta UMA vez e soma as janelas. */
static int tree_caps_ok(int cx, int cz) {
	const GEN_PARAMS *gp = cur()->gen;
	int chunk_cap = gp ? gp->chunk_cap : 5, win_cap = gp ? gp->window_trees : 16;
	int bx = cx / CHUNK_CELLS, bz = cz / CHUNK_CELLS, cnt[5][5];
	for (int z = 0; z < 5; z++)
		for (int x = 0; x < 5; x++)
			cnt[z][x] = level_chunk_standing(bx + x - 2, bz + z - 2);
	if (cnt[2][2] >= chunk_cap) return 0;
	for (int wz = 1; wz <= 3; wz++)                  /* toda janela que contém o bloco */
		for (int wx = 1; wx <= 3; wx++) {
			int t = 0;
			for (int z = wz - 1; z <= wz + 1; z++)
				for (int x = wx - 1; x <= wx + 1; x++)
					t += cnt[z][x];
			if (t >= win_cap) return 0;
		}
	return 1;
}

/* Checagem passou: confere de novo (alguém entrou, a câmera virou, os
 * tetos) e planta o pinheiro */
static int close_commit(int target) {
	int cx = g.ev.job.cand & 0xff, cz = g.ev.job.cand >> 8;
	int x = cx * TILE_SIZE + TILE_SIZE / 2, z = cz * TILE_SIZE + TILE_SIZE / 2;
	if (cell_busy(cx, cz) || seen_by_camera(x, z) || !tree_caps_ok(cx, cz))
		return 0;
	int scale = rng_range(&g.rng, tree_defs[TREE_PINE].scale_min, tree_defs[TREE_PINE].scale_max);
	level_add_tree(TREE_PINE, x, z, rng_next(&g.rng) & 4095, scale);
	level_trees_changed();
	reach_set(cx, cz, 0);
	g.ev.job.placed++;
	return g.ev.job.placed >= target ? 1 : 0;
}

static int job_close_step(void) {
	int gx, gz;
	if (!group_cell(&gx, &gz)) return -1;
	int target = g.ev.job.arg ? g.ev.job.arg : EV_PLANT_COUNT;
	if (g.ev.job.placed >= target || level_tree_room() < 1) return 1;
	if (g.ev.job.checking) {                       /* checagem em andamento */
		int r = EVP_CHECK(reach_step(EV_CHECK_VISITS));
		if (r == REACH_PENDING) return 0;
		g.ev.job.checking = 0;
		return r == REACH_OK ? close_commit(target) : 0;
	}
	for (int scans = 0; scans < EV_FITS_PER_TICK; scans++) {   /* orçamento por passo */
		if (++g.ev.job.tries > EV_JOB_TRIES) return g.ev.job.placed ? 1 : -1;
		/* atrás do grupo (oposto à câmera), a 7..14 células */
		int a = (g.cam_yaw + 2048 + rng_range(&g.rng, -900, 900)) & 4095;
		int d = rng_range(&g.rng, 7 * TILE_SIZE, 14 * TILE_SIZE);
		int x = gx * TILE_SIZE + TILE_SIZE / 2 + ((isin(a) * d) >> 12);
		int z = gz * TILE_SIZE + TILE_SIZE / 2 + ((icos(a) * d) >> 12);
		int cx = x / TILE_SIZE, cz = z / TILE_SIZE;
		if (cx < FOREST_EDGE + 1 || cz < FOREST_EDGE + 1 ||
		    cx >= level_width() - FOREST_EDGE - 1 || cz >= level_height() - FOREST_EDGE - 1)
			continue;
		x = cx * TILE_SIZE + TILE_SIZE / 2;
		z = cz * TILE_SIZE + TILE_SIZE / 2;
		if (!reach_get(cx, cz) || !reach_walkable(cx, cz) || cell_busy(cx, cz) ||
		    seen_by_camera(x, z) || !tree_caps_ok(cx, cz))
			continue;
		uint16_t blk = (uint16_t)(cx | (cz << 8));
		g.ev.job.cand = blk;
		int r = EVP_CHECK(reach_begin(cx, cz, &blk, 1));
		if (r == REACH_PENDING) { g.ev.job.checking = 1; return 0; }
		return r == REACH_OK ? close_commit(target) : 0;
	}
	return 0;
}

/* ---------- onda de inimigos ---------- */

static int act_wave(int arg) {
	int gx, gz, n = arg ? arg : EV_WAVE_COUNT, made = 0;
	if (!group_cell(&gx, &gz)) return 0;
	/* repõe até o número de inimigos do começo da fase (cerco: o máximo da
	 * fase): a onda assusta sem passar do orçamento de desempenho */
	int cap = cur()->objective == OBJ_SURVIVE ? cur()->max_enemies : g.ev.enemy_cap;
	for (int k = 0; k < n; k++) {
		if (g.enemies_left >= cap) break;
		for (int t = 0; t < 40; t++) {
			int a = rng_next(&g.rng) & 4095;
			int d = rng_range(&g.rng, REINFORCE_MIN_DIST, REINFORCE_MIN_DIST + 10 * TILE_SIZE);
			int cx = (gx * TILE_SIZE + ((isin(a) * d) >> 12)) / TILE_SIZE;
			int cz = (gz * TILE_SIZE + ((icos(a) * d) >> 12)) / TILE_SIZE;
			int x = cx * TILE_SIZE + TILE_SIZE / 2, z = cz * TILE_SIZE + TILE_SIZE / 2;
			if (!reach_get(cx, cz) || !reach_walkable(cx, cz) || cell_busy(cx, cz) ||
			    seen_by_camera(x, z))
				continue;
			int before = g.enemies_left;
			enemy_spawn(rng_range(&g.rng, 0, num_enemy_types - 1), x, z);
			if (g.enemies_left == before) return made > 0;   /* pool cheio */
			if (!made) sound_play_at(&sfx_rosnado, x, z, VOL_ROSNADO);
			made++;
			break;
		}
	}
	if (made) show_message("ALGO SE APROXIMA", 90);
	return made > 0;
}

/* ---------- caminho se abre (chave na barreira de raízes) ---------- */

/* use_key: 1 = a chave abre (gasta uma); 0 = o machado cortou (etapa 07) */
static int open_barrier(int use_key) {
	int bcx, bcz, br;
	if (g.ev.barrier_open || !mapgen_barrier(&bcx, &bcz, &br)) return 0;
	if (use_key && g.ev.keys <= 0 && !g.ev.job.arg) return 0;
	for (int z = bcz - br - 2; z <= bcz + br + 2; z++)
		for (int x = bcx - br - 2; x <= bcx + br + 2; x++) {
			int d = (x - bcx) * (x - bcx) + (z - bcz) * (z - bcz);
			if (d > (br + 2) * (br + 2)) continue;
			if (level_cell_type(x, z) == CELL_BARRIER) {
				level_set_cell(x, z, CELL_ROOTS);     /* vira chão de raízes */
				level_rebuild_chunk(x, z);            /* em fatias (desenho) */
			}
			if (reach_walkable(x, z)) reach_set(x, z, 1);   /* a região de trás */
		}
	if (use_key && g.ev.keys > 0) g.ev.keys--;
	g.ev.barrier_open = 1;
	sound_play_at(&sfx_raizes, bcx * TILE_SIZE, bcz * TILE_SIZE, VOL_RAIZES);
	show_message(use_key ? "AS RAIZES SE ABREM" : "AS RAIZES CEDEM", 120);
	return 1;
}

static int act_open_path(void) {
	return open_barrier(1);
}

/* ---------- machado (etapa 07) ---------- */

/* Golpe de machado na célula (cx, cz): copa/tronco de uma árvore caída ou
 * a barreira de raízes. Cada um aguenta alguns golpes; cada golpe faz
 * barulho médio. Cortar só LIBERA células (não precisa da checagem de
 * alcance). Devolve 1 se acertou algo. */
int events_chop(int cx, int cz) {
	for (int k = 0; k < 4; k++) {
		if (!g.ev.fallen[k].active || in_cells(cx, cz, g.ev.fallen[k].cells, g.ev.fallen[k].n) < 0)
			continue;
		noise_emit(cx * TILE_SIZE + TILE_SIZE / 2, cz * TILE_SIZE + TILE_SIZE / 2, CHOP_NOISE);
		sound_play_at(&sfx_flecha, cx * TILE_SIZE, cz * TILE_SIZE, VOL_GOLPE);   /* madeira */
		effect_spawn(FX_BURST, cx * TILE_SIZE + TILE_SIZE / 2, cz * TILE_SIZE + TILE_SIZE / 2, 160, 10, 150, 110, 60);
		if (--g.ev.fallen[k].hp > 0) return 1;
		/* cedeu: tira o pinheiro deitado e libera as células */
		g.ev.fallen[k].active = 0;
		for (int i = 0; i < g.ev.fallen[k].n; i++) {
			int x = g.ev.fallen[k].cells[i] & 0xff, z = g.ev.fallen[k].cells[i] >> 8;
			level_set_flags(x, z, 0);
			if (reach_walkable(x, z)) reach_set(x, z, 1);
		}
		int bx = g.ev.fallen[k].x / (CHUNK_CELLS * TILE_SIZE), bz = g.ev.fallen[k].z / (CHUNK_CELLS * TILE_SIZE);
		int start, n = level_chunk_trees(bx, bz, &start);
		for (int i = start; i < start + n; i++) {
			int x, z, type, scale, ang;
			level_tree_info(i, &x, &z, &type, &scale, &ang);
			if (type == TREE_FALLEN && x == g.ev.fallen[k].x && z == g.ev.fallen[k].z) {
				level_remove_tree(i);
				break;
			}
		}
		sound_play_at(&sfx_queda, cx * TILE_SIZE, cz * TILE_SIZE, VOL_QUEDA / 2);
		show_message("O TRONCO CEDEU", 90);
		return 1;
	}
	if (level_cell_type(cx, cz) == CELL_BARRIER && !g.ev.barrier_open) {
		noise_emit(cx * TILE_SIZE + TILE_SIZE / 2, cz * TILE_SIZE + TILE_SIZE / 2, CHOP_NOISE);
		effect_spawn(FX_BURST, cx * TILE_SIZE + TILE_SIZE / 2, cz * TILE_SIZE + TILE_SIZE / 2, 160, 10, 120, 90, 50);
		if (--g.ev.barrier_hp <= 0)
			open_barrier(0);
		return 1;
	}
	return 0;
}

/* Jogador com chave encostado na barreira (gatilho EVT_KEY) */
static int key_at_barrier(void) {
	int bcx, bcz, br;
	if (g.ev.keys <= 0 || g.ev.barrier_open || !mapgen_barrier(&bcx, &bcz, &br)) return 0;
	for (int i = 0; i < MAX_PLAYERS; i++) {
		const PLAYER *p = &g.players[i];
		if (!player_alive(p)) continue;
		int d = dist2d(p->pos.vx - (bcx * TILE_SIZE + TILE_SIZE / 2), p->pos.vz - (bcz * TILE_SIZE + TILE_SIZE / 2));
		if (d < (br + 3) * TILE_SIZE + TILE_SIZE / 2) return 1;
	}
	return 0;
}

/* ---------- despacho ---------- */

/* 1 = aconteceu; 0 = não deu (sai da fila); 2 = começou uma busca */
static int apply(int action, int arg, int force) {
	switch (action) {
	case EV_FOG:      return act_fog(arg);
	case EV_SILENCE:  return act_silence(arg);
	case EV_WAVE:     return act_wave(arg);
	case EV_OPEN_PATH:
		g.ev.job.arg = force;                  /* depuração: abre mesmo sem chave */
		{ int r = act_open_path(); g.ev.job.arg = 0; return r; }
	case EV_TREE_FALL:
	case EV_CLOSE_BEHIND:
		if (g.ev.fall.timer > 0) return 0;     /* uma árvore por vez */
		g.ev.job.action = action;
		g.ev.job.arg = arg;
		g.ev.job.k = g.ev.job.j = g.ev.job.tries = g.ev.job.placed = 0;
		g.ev.job.checking = 0;
		g.ev.job.dir0 = rng_range(&g.rng, 0, 7) * 512;
		g.ev.job.start = rng_next(&g.rng) & 0xffff;
		return 2;
	}
	return 0;
}

static void job_done(int ok) {
	if (ok) {
		g.ev.last = g.ev.job.action;
		g.ev.map_cooldown = EV_MAP_COOLDOWN;
		if (g.ev.job.action == EV_CLOSE_BEHIND)
			show_message("A MATA SE FECHA ATRAS", 120);
	}
	g.ev.job.action = -1;
}


/* Busca em andamento e árvore caindo: um passo */
static void tick_map_work(void) {
	if (g.ev.fall.timer > 0 && --g.ev.fall.timer == 0) {
		EVP_BEGIN();
		fall_impact();
		EVP_END(prof_impact);
	}
	if (g.ev.job.action < 0) return;
	EVP_BEGIN();
	int r = g.ev.job.action == EV_TREE_FALL ? job_fall_step() : job_close_step();
	EVP_END(prof_job);
	if (r != 0) job_done(r > 0);
}

/* Árvore caindo: o pinheiro em pé tremendo (aviso) e depois tombando com
 * aceleração. Só desenho (lê g.ev.fall). */
void events_draw(void) {
	if (g.ev.fall.timer <= 0) return;
	int pitch = 0, yaw = g.ev.fall.dir;
	if (g.ev.fall.timer > EV_FALL_TIME) {           /* estalo: balança */
		pitch = isin((g.frame * 700) & 4095) >> 7;
	} else {
		int p = EV_FALL_TIME - g.ev.fall.timer;     /* 0..EV_FALL_TIME */
		pitch = tree_defs[TREE_FALLEN].pitch * p * p / (EV_FALL_TIME * EV_FALL_TIME);
	}
	VECTOR  pos = { g.ev.fall.x, 0, g.ev.fall.z };
	SVECTOR rot = { pitch, yaw, 0 };
	DRAWOPT opt = { 0 };
	render_mesh(tree_defs[TREE_PINE].mesh, &pos, &rot, g.ev.fall.scale, &opt);
}

/* ------------------------------------------------------------------ */
/* Gatilhos                                                             */
/* ------------------------------------------------------------------ */

/* Ação sorteada da máscara; as que mudam o mapa só se o intervalo deixar */
static int pick_random_action(int mask) {
	if (g.ev.map_cooldown > 0) mask &= ~EV_MAP_ACTIONS;
	mask &= ~(1 << EV_OPEN_PATH);              /* abrir caminho: só com a chave */
	int n = 0, list[NUM_EV_ACTIONS];
	for (int a = 0; a < NUM_EV_ACTIONS; a++)
		if (mask & (1 << a)) list[n++] = a;
	return n ? list[rng_range(&g.rng, 0, n - 1)] : -1;
}

static int near_area(int area, int radius_cells) {
	int r = radius_cells * TILE_SIZE;
	for (int i = 0; i < MAX_PLAYERS; i++) {
		const PLAYER *p = &g.players[i];
		if (!player_alive(p)) continue;
		if (area == AREA_EXIT && g.has_exit &&
		    dist2d(p->pos.vx - g.exit_x, p->pos.vz - g.exit_z) < r)
			return 1;
		if (area == AREA_QUEST)
			for (int k = 0; k < MAX_PICKUPS; k++) {
				const PICKUP *q = &g.pickups[k];
				if (q->active && q->type == PICK_QUEST &&
				    dist2d(p->pos.vx - q->pos.vx, p->pos.vz - q->pos.vz) < r)
					return 1;
			}
	}
	return 0;
}

static void check_triggers(void) {
	const LEVEL_DEF *ld = cur();
	for (int i = 0; i < ld->num_events && i < 32; i++) {
		const EVENT_DEF *e = &ld->events[i];
		if (e->trigger == EVT_RANDOM) {
			if (g.ev.next_random < 0)          /* primeiro sorteio */
				g.ev.next_random = rng_range(&g.rng, e->p1 * 60, e->p2 * 60);
			if (g.ev.next_random > 0) continue;
			int a = pick_random_action(e->arg);
			if (a >= 0) enqueue(a, e->prio, 0, 0);
			g.ev.next_random = rng_range(&g.rng, e->p1 * 60, e->p2 * 60);
			continue;
		}
		if (g.ev.fired & (1u << i)) continue;  /* os outros disparam uma vez */
		int hit = 0;
		switch (e->trigger) {
		case EVT_TIME:   hit = g.play_frames >= e->p1 * 60; break;
		case EVT_AREA:   hit = near_area(e->p1, e->p2); break;
		case EVT_PICKUP: hit = g.quest >= e->p1; break;
		case EVT_KILLS:  hit = g.ev.kills >= e->p1; break;
		case EVT_KEY:    hit = key_at_barrier(); break;
		}
		if (hit) {
			g.ev.fired |= 1u << i;
			enqueue(e->action, e->prio, e->arg, 0);
		}
	}
}

/* ------------------------------------------------------------------ */
/* Passo                                                                */
/* ------------------------------------------------------------------ */

static void tick_timers(void) {
	if (g.ev.next_random > 0) g.ev.next_random--;
	if (g.ev.map_cooldown > 0) g.ev.map_cooldown--;
	if (g.ev.silence > 0) g.ev.silence--;
	if (g.ev.shake > 0) g.ev.shake--;

	/* névoa: desce em EV_FOG_RAMP, fica, sobe nos últimos EV_FOG_RAMP */
	int base = cur()->fog_far;
	if (g.ev.fog_timer > 0) {
		int t = EV_FOG_TOTAL - g.ev.fog_timer;  /* passos desde o começo */
		int k = g.ev.fog_timer < EV_FOG_RAMP ? g.ev.fog_timer : t < EV_FOG_RAMP ? t : EV_FOG_RAMP;
		g.ev.fog_far = base - (base - g.ev.fog_target) * k / EV_FOG_RAMP;
		g.ev.fog_timer--;
	} else {
		g.ev.fog_far = base;
	}
}

void events_update(void) {
	tick_timers();
	tick_map_work();
	if (g.play_frames < EV_GRACE) {             /* começo da fase: só o relógio */
		for (int i = 0; i < g.ev.nq; i++)
			if (g.ev.queue[i].force) goto run; /* depuração passa */
		return;
	}
#if !defined(DEBUG_BENCHMARK) || defined(BENCH_REPLAY)
	check_triggers();                           /* bench: só os forçados (replay: todos) */
#endif
run:
	if (g.ev.nq == 0) return;

	/* a de maior prioridade que pode sair agora */
	int best = -1;
	for (int i = 0; i < g.ev.nq; i++) {
		const int a = g.ev.queue[i].action;
		int ok = g.ev.queue[i].force || !((EV_MAP_ACTIONS >> a) & 1) ||
		         g.ev.map_cooldown == 0 || a == EV_OPEN_PATH;   /* chave: na hora */
		if (ok && (best < 0 || g.ev.queue[i].prio > g.ev.queue[best].prio))
			best = i;
	}
	if (best < 0) return;
	int a = g.ev.queue[best].action, arg = g.ev.queue[best].arg, force = g.ev.queue[best].force;
	if (g.ev.job.action >= 0 && (a == EV_TREE_FALL || a == EV_CLOSE_BEHIND))
		return;                                  /* espera a busca atual acabar */
	g.ev.queue[best] = g.ev.queue[--g.ev.nq];
	int r = apply(a, arg, force);
	if (r == 1) {
		g.ev.last = a;
		if ((EV_MAP_ACTIONS >> a) & 1)
			g.ev.map_cooldown = EV_MAP_COOLDOWN;
	}
}

/* Atalho de depuração (overlay L2): entra na fila com prioridade máxima e
 * sem as regras de ritmo; continua passando por events_update (lógica). */
void events_debug(int action) {
	if (action >= 0 && action < NUM_EV_ACTIONS)
		enqueue(action, 255, 0, 1);
}
