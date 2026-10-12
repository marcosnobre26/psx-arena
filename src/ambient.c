/*
 * ambient.c - Floresta viva: céu, lua, pássaros e animais (etapa 09)
 *
 * SÓ VISUAL E SOM. Nada aqui escreve em g nem usa g.rng: o estado fica
 * neste arquivo e os sorteios usam fx_range (gerador dos efeitos). Lê de g
 * só posições, lanterna, estado da partida e os barulhos (cópia guardada
 * por ambient_note_noise, chamada no noise_emit). Prova: replay-test e
 * robo-test dão IGUAL. Se um dia um animal precisar mexer no jogo (ex.: o
 * bando alertar os inimigos), vira LÓGICA com g.rng (ver HISTORICO).
 *
 * Pilar "ver pouco, ouvir muito": os animais inquietam, não enfeitam.
 *   CÉU     degradê do alto da tela até o horizonte + lua de relance
 *   BANDO   3-8 pássaros (2 triângulos cada) cruzam a faixa de céu no alto
 *           da tela, ou LEVANTAM VOO de uma árvore quando o jogador chega
 *           perto ou há um barulho forte (ataque, tiro). Silhuetas sem névoa.
 *   OLHOS   um par de olhos no escuro a 5-8 m; some quando a lanterna
 *           aponta (1 em 3 vezes ENCARA um instante antes de sumir)
 *   VULTO   uma silhueta preta atravessa ao longe (9-12 m), recortada no céu
 *   SALTO   um bicho pula de um tronco para outro, num arco
 *   GALHO   só som: um galho estala atrás do jogador
 *
 * Desempenho: ~40 polígonos no pior caso. Freio: se o quadro anterior passou
 * de AMB_CPU_LIMIT linhas, o evento que ia começar ESPERA (até AMB_WAIT
 * passos) e aparece quando aliviar; o céu sempre aparece.
 *
 * Tudo anda por passos de jogo (ambient_update(ticks)); com o menu aberto
 * (ou fora da partida) o ambiente congela. O build do robô não desenha e
 * por isso nem chama isto.
 */
#include <psxpad.h>
#include <stdlib.h>
#include <string.h>
#include "game.h"
#include "models.h"

#ifndef AMB_CPU_LIMIT
#define AMB_CPU_LIMIT 420           /* quadro anterior acima disto: o evento espera */
#endif
#define AMB_WAIT      300           /* espera máxima (5 s); depois desiste */
#define S(x) ((x) * 60)             /* segundos -> passos */
#define CHUNK_SIZE (CHUNK_CELLS * TILE_SIZE)

enum { AM_FLOCK_SKY, AM_FLOCK_TREE, AM_EYES, AM_SHADOW, AM_JUMP, AM_BRANCH, AM_KINDS };

/* sorteio do intervalo de cada evento (segundos) */
static const int16_t every_min[AM_KINDS] = { 40, 0, 25, 45, 35, 20 };
static const int16_t every_max[AM_KINDS] = { 90, 0, 60, 90, 70, 50 };

#define MAX_BIRDS 8
static struct {
	uint32_t sig;                   /* fase/semente: mudou = recomeça */
	int      next[AM_KINDS];        /* passos até o próximo (0 = esperando) */
	int      pending[AM_KINDS];     /* quer começar: passos esperando o freio */
	int      cpu_last;              /* linhas do quadro anterior (o freio lê) */
	int      takeoff_cd, roost_scan;
	int      moon_az, moon_front;   /* direção da lua; > 0 = forçada à frente (debug) */
	int      last_polys, delayed;   /* medidas (bench/overlay) */
	/* bando */
	struct {
		int active, t, life, n;
		int x, y, z, vx, vy, vz;
		int16_t ox[MAX_BIRDS], oy[MAX_BIRDS], oz[MAX_BIRDS], ph[MAX_BIRDS];
	} fl;
	/* olhos */
	struct { int active, t, life, x, y, z, stare, lit_t; } ey;
	/* vulto */
	struct { int active, t, life, x, z, vx, vz; } sh;
	/* salto */
	struct { int active, t, life, x0, z0, x1, z1; } jp;
	/* último barulho forte (cópia visual do noise_emit) */
	struct { int x, z, r, frame; } noise;
} am;

static int rnd(int lo, int hi) { return fx_range(lo, hi); }

static void schedule(int k) {
	am.next[k] = every_max[k] ? S(rnd(every_min[k], every_max[k])) : 0;
}

/* O noise_emit (lógica) avisa: só guardamos uma cópia, nada volta para g */
void ambient_note_noise(int x, int z, int r) {
	if (r < 5) return;                            /* passos: pequenos demais */
	am.noise.x = x; am.noise.z = z; am.noise.r = r; am.noise.frame = g.frame;
}

static void reset(void) {
	memset(&am, 0, sizeof(am));
	am.sig = g.seed * 31u + (uint32_t)g.level;
	for (int k = 0; k < AM_KINDS; k++) schedule(k);
	for (int k = 0; k < AM_KINDS; k++)            /* nada nos primeiros 15 s */
		if (am.next[k] && am.next[k] < S(15)) am.next[k] += S(15);
	am.moon_az = (int)((g.seed * 2654435761u) >> 20) & 4095;   /* cada mapa, um lado */
}

/* ------------------------------------------------------------------ */
/* Começar cada evento (só lê g)                                         */
/* ------------------------------------------------------------------ */
static const PLAYER *me(void) { return &g.players[0]; }

/* Árvore em pé mais perto de (x,z) entre rmin e rmax; 1 = achou. Com
 * cone > 0: só as que estão até 'cone' (ângulo) da direção da câmera,
 * vistas do jogador (para o bicho saltar onde se vê). */
static int tree_pick(int x, int z, int rmin, int rmax, int *tx, int *tz, int skip_x, int skip_z, int cone) {
	int best = 0x7fffffff, found = 0;
	int bx0 = (x - rmax) / CHUNK_SIZE, bx1 = (x + rmax) / CHUNK_SIZE;
	int bz0 = (z - rmax) / CHUNK_SIZE, bz1 = (z + rmax) / CHUNK_SIZE;
	int nbx = level_width() / CHUNK_CELLS, nbz = level_height() / CHUNK_CELLS;
	const PLAYER *p = &g.players[0];
	for (int bz = bz0; bz <= bz1; bz++)
		for (int bx = bx0; bx <= bx1; bx++) {
			if (bx < 0 || bz < 0 || bx >= nbx || bz >= nbz) continue;
			int start, n = level_chunk_trees(bx, bz, &start);
			for (int k = 0; k < n; k++) {
				int ax, az, type, scale, ang;
				level_tree_info(start + k, &ax, &az, &type, &scale, &ang);
				if (tree_defs[type].cells != 1 || tree_defs[type].pitch) continue;
				if (ax == skip_x && az == skip_z) continue;
				int d = dist2d(ax - x, az - z);
				if (d < rmin || d > rmax || d >= best) continue;
				if (cone && abs(angle_diff(g.cam_yaw, angle_of(ax - p->pos.vx, az - p->pos.vz))) > cone)
					continue;
				best = d; *tx = ax; *tz = az; found = 1;
			}
		}
	return found;
}

static int tree_near(int x, int z, int rmin, int rmax, int *tx, int *tz, int skip_x, int skip_z) {
	return tree_pick(x, z, rmin, rmax, tx, tz, skip_x, skip_z, 0);
}

static void start_flock(int from_tree, int tx, int tz) {
	const PLAYER *p = me();
	const VECTOR *eye = camera_eye();
	am.fl.active = 1;
	am.fl.t = 0;
	am.fl.n = rnd(3, MAX_BIRDS);
	if (from_tree) {                              /* levanta voo: para longe do jogador */
		int a = angle_of(tx - p->pos.vx, tz - p->pos.vz) + rnd(-300, 300);
		am.fl.x = tx; am.fl.z = tz; am.fl.y = -900;
		am.fl.vx = (isin(a & 4095) * 14) >> 12;
		am.fl.vz = (icos(a & 4095) * 14) >> 12;
		am.fl.vy = -5;
		am.fl.life = 170;
		sound_play_at(&sfx_asas, tx, tz, VOL_AMB_ASAS);
		sound_play_at(&sfx_corvo, tx, tz, VOL_AMB_CORVO);
	} else {                                      /* cruza a faixa de céu no alto da tela */
		int s = isin(g.cam_yaw), c = icos(g.cam_yaw);
		int d = rnd(1800, 2800), side = rnd(0, 1) ? 1 : -1;
		/* altura: um pouco acima do horizonte visto do olho (2-4 graus) */
		int up = (d * rnd(35, 70)) / 1000;
		am.fl.x = eye->vx + ((s * d) >> 12) - ((c * 1500 * side) >> 12);
		am.fl.z = eye->vz + ((c * d) >> 12) + ((s * 1500 * side) >> 12);
		am.fl.y = eye->vy - up;
		am.fl.vx = (c * 16 * side) >> 12;
		am.fl.vz = -(s * 16 * side) >> 12;
		am.fl.vy = 0;
		am.fl.life = 200;
		if (rnd(0, 2) == 0) sound_play_at(&sfx_corvo, am.fl.x, am.fl.z, VOL_AMB_CORVO / 2);
	}
	for (int i = 0; i < am.fl.n; i++) {
		am.fl.ox[i] = (int16_t)rnd(-260, 260);
		am.fl.oy[i] = (int16_t)rnd(-120, 120);
		am.fl.oz[i] = (int16_t)rnd(-260, 260);
		am.fl.ph[i] = (int16_t)rnd(0, 15);
	}
}

/* Ponto à frente da câmera (ângulo +-spread), a d do jogador */
static void ahead(int d, int spread, int *x, int *z) {
	const PLAYER *p = me();
	int a = (g.cam_yaw + rnd(-spread, spread)) & 4095;
	*x = p->pos.vx + ((isin(a) * d) >> 12);
	*z = p->pos.vz + ((icos(a) * d) >> 12);
}

static int start_eyes(void) {
	int x, z;
	for (int tries = 0; tries < 6; tries++) {
		ahead(rnd(1300, 2000), 380, &x, &z);
		if (!level_blocked(x, z, 60)) {
			am.ey.active = 1; am.ey.t = 0; am.ey.life = rnd(240, 420);
			am.ey.x = x; am.ey.z = z; am.ey.y = -rnd(140, 320);
			am.ey.stare = rnd(0, 2) == 0;          /* 1 em 3: encara antes de sumir */
			am.ey.lit_t = 0;
			sound_play_at(&sfx_respiro, x, z, VOL_AMB_RESPIRO);
			return 1;
		}
	}
	return 0;
}

static int start_shadow(void) {
	int x, z;
	ahead(rnd(2400, 3000), 200, &x, &z);       /* além do chão visível: recorta no céu */
	int s = isin(g.cam_yaw), c = icos(g.cam_yaw), side = rnd(0, 1) ? 1 : -1;
	am.sh.active = 1; am.sh.t = 0; am.sh.life = 110;
	am.sh.vx = (c * 22 * side) >> 12;             /* atravessa de lado */
	am.sh.vz = -(s * 22 * side) >> 12;
	am.sh.x = x - am.sh.vx * am.sh.life / 2;
	am.sh.z = z - am.sh.vz * am.sh.life / 2;
	sound_play_at(&sfx_folhas, x, z, VOL_AMB_FOLHAS);
	return 1;
}

static int start_jump(void) {
	/* 2 árvores à vista: a 1a a 2-6 m do jogador, no campo da câmera; a 2a
	 * a até 5 m dela (perto, para o salto ser visto) */
	int ax, az, bx, bz;
	const PLAYER *p = me();
	if (!tree_pick(p->pos.vx, p->pos.vz, 500, 1600, &ax, &az, -1, -1, 450)) return 0;
	if (!tree_pick(ax, az, 400, 1300, &bx, &bz, ax, az, 600)) return 0;
	am.jp.active = 1; am.jp.t = 0; am.jp.life = 50;
	am.jp.x0 = ax; am.jp.z0 = az; am.jp.x1 = bx; am.jp.z1 = bz;
	sound_play_at(&sfx_folhas, ax, az, VOL_AMB_FOLHAS);
	return 1;
}

static void start_branch(void) {
	const PLAYER *p = me();
	int a = (p->angle + 2048 + rnd(-600, 600)) & 4095, d = rnd(1300, 2000);
	sound_play_at(&sfx_galho, p->pos.vx + ((isin(a) * d) >> 12), p->pos.vz + ((icos(a) * d) >> 12),
	              VOL_AMB_GALHO);
}

/* Tenta começar o evento k agora (o freio já deixou). 1 = começou. */
static int try_start(int k) {
	switch (k) {
	case AM_FLOCK_SKY:
		if (am.fl.active) return 0;
		start_flock(0, 0, 0);
		return 1;
	case AM_FLOCK_TREE: {
		int tx, tz;
		if (am.fl.active) return 0;
		if (!tree_near(me()->pos.vx, me()->pos.vz, 0, 1400, &tx, &tz, -1, -1)) return 0;
		start_flock(1, tx, tz);
		return 1;
	}
	case AM_EYES:   return !am.ey.active && start_eyes();
	case AM_SHADOW: return !am.sh.active && start_shadow();
	case AM_JUMP:   return !am.jp.active && start_jump();
	case AM_BRANCH: start_branch(); return 1;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* Um quadro (depois da lógica): só muda o estado deste arquivo          */
/* ------------------------------------------------------------------ */
void ambient_update(int ticks) {
	uint32_t sig = g.seed * 31u + (uint32_t)g.level;
	if (sig != am.sig) reset();
	int pc, pg, pv;
	render_stats_time(&pc, &pg, &pv);
	am.cpu_last = pc;
	if (g.state != STATE_PLAY || !player_alive(me()))
		return;                                   /* menu/pausa/fim: congela */
	const PLAYER *p = me();

	for (int t = 0; t < ticks; t++) {
		/* relógio de cada evento */
		for (int k = 0; k < AM_KINDS; k++)
			if (am.next[k] > 0 && --am.next[k] == 0) { am.pending[k] = 1; schedule(k); }
		if (am.takeoff_cd > 0) am.takeoff_cd--;
		/* levanta voo: barulho forte perto, ou o jogador chegou numa árvore
		 * de pouso (uma em 6, pela posição) */
		if (!am.takeoff_cd && !am.fl.active) {
			int loud = am.noise.r && (int)(g.frame - am.noise.frame) < 30 &&
			           dist2d(am.noise.x - p->pos.vx, am.noise.z - p->pos.vz) < 2000;
			int roost = 0;
			if (++am.roost_scan >= 30) {
				int tx, tz;
				am.roost_scan = 0;
				if (tree_near(p->pos.vx, p->pos.vz, 0, 520, &tx, &tz, -1, -1) &&
				    ((unsigned)(tx * 7 + tz * 13) / TILE_SIZE) % 6 == 0)
					roost = 1;
			}
			if (loud || roost) { am.pending[AM_FLOCK_TREE] = 1; am.takeoff_cd = S(30); am.noise.r = 0; }
		}
		/* freio: começa só se o quadro anterior foi leve; senão espera */
		for (int k = 0; k < AM_KINDS; k++) {
			if (!am.pending[k]) continue;
			if (am.cpu_last > AMB_CPU_LIMIT && k != AM_BRANCH) {
				if (++am.pending[k] > AMB_WAIT) am.pending[k] = 0;   /* desiste */
				else am.delayed++;
				continue;
			}
			if (try_start(k) || ++am.pending[k] > AMB_WAIT) am.pending[k] = 0;
		}

		/* andar dos que estão ativos */
		if (am.fl.active) {
			am.fl.x += am.fl.vx; am.fl.y += am.fl.vy; am.fl.z += am.fl.vz;
			if (am.fl.vy < 0 && am.fl.y < -1500) am.fl.vy = 0;          /* sobe e plana */
			if (++am.fl.t >= am.fl.life) am.fl.active = 0;
		}
		if (am.ey.active) {
			int dx = am.ey.x - p->pos.vx, dz = am.ey.z - p->pos.vz;
			int lit = p->lantern_on && abs(angle_diff(p->angle, angle_of(dx, dz))) < 300;
			if (lit) am.ey.lit_t++;
			/* com luz: somem (ou encaram ~0,6 s antes) */
			if ((lit && am.ey.lit_t > (am.ey.stare ? 36 : 3)) || ++am.ey.t >= am.ey.life)
				am.ey.active = 0;
		}
		if (am.sh.active) {
			am.sh.x += am.sh.vx; am.sh.z += am.sh.vz;
			if (++am.sh.t >= am.sh.life) am.sh.active = 0;
		}
		if (am.jp.active && ++am.jp.t >= am.jp.life) {
			am.jp.active = 0;
			sound_play_at(&sfx_folhas, am.jp.x1, am.jp.z1, VOL_AMB_FOLHAS / 2);
		}
		if (am.moon_front > 0) am.moon_front--;
	}
}

/* ------------------------------------------------------------------ */
/* Desenho (só lê)                                                       */
/* ------------------------------------------------------------------ */
void ambient_draw_sky(void) {
	const LEVEL_DEF *ld = &level_defs[g.level];
	int h = render_horizon_y();
	/* alto: a cor da névoa pela metade; horizonte: ~2,5x mais claro (luar) */
	render_sky(h, ld->sky_r / 2, ld->sky_g / 2, ld->sky_b / 2,
	           ld->sky_r * 5 / 2 + 14, ld->sky_g * 5 / 2 + 14, ld->sky_b * 5 / 2 + 20);
	/* lua: um pouco acima do horizonte, numa direção fixa por mapa */
	int az = am.moon_front ? g.cam_yaw : am.moon_az;
	int sx, sy;
	if (h > 0 && render_project_dir(isin(az), -260, icos(az), &sx, &sy))
		render_moon(&tex_lua_t, sx, sy, 18);
}

void ambient_draw(void) {
	int polys0 = render_stats_polys_now();
	static const CVECTOR black = { 0, 0, 0 };
	DRAWOPT sil = { 0 };
	sil.flags = DRAW_UNLIT | DRAW_NOFOG | DRAW_NOCULL;
	sil.palette = &black;
	sil.npalette = 1;
	if (am.fl.active) {
		SVECTOR rot = { 0, angle_of(am.fl.vx, am.fl.vz), 0 };
		for (int i = 0; i < am.fl.n; i++) {
			int flap = ((am.fl.t + am.fl.ph[i]) >> 2) & 1;      /* bate as asas */
			VECTOR pos = { am.fl.x + am.fl.ox[i], am.fl.y + am.fl.oy[i] + (flap ? 6 : -6), am.fl.z + am.fl.oz[i] };
			render_mesh(flap ? &passaro_a_mesh : &passaro_b_mesh, &pos, &rot, ONE * 2, &sil);   /* ~1,4 m de asa: corvo */
		}
	}
	if (am.ey.active && ((am.ey.t / 6) % 16) != 15) {          /* pisca de vez em quando */
		const PLAYER *p = me();
		DRAWOPT eo = { 0 };
		eo.flags = DRAW_UNLIT | DRAW_NOFOG;
		SVECTOR rot = { 0, angle_of(p->pos.vx - am.ey.x, p->pos.vz - am.ey.z), 0 };   /* de frente para o jogador */
		VECTOR pos = { am.ey.x, am.ey.y, am.ey.z };
		render_mesh(&olhos_mesh, &pos, &rot, ONE * 3, &eo);        /* algo grande */
	}
	if (am.sh.active) {
		SVECTOR rot = { 0, angle_of(am.sh.vx, am.sh.vz) + 2048, 0 };
		VECTOR pos = { am.sh.x, 0, am.sh.z };
		render_mesh(&vulto_mesh, &pos, &rot, ONE, &sil);          /* silhueta preta, sem névoa */
	}
	if (am.jp.active) {
		int t = am.jp.t, L = am.jp.life;
		int x = am.jp.x0 + (am.jp.x1 - am.jp.x0) * t / L;
		int z = am.jp.z0 + (am.jp.z1 - am.jp.z0) * t / L;
		int arc = 4 * 220 * t * (L - t) / (L * L);              /* parábola: sobe e desce */
		SVECTOR rot = { 0, angle_of(am.jp.x1 - am.jp.x0, am.jp.z1 - am.jp.z0) + 2048, 0 };
		VECTOR pos = { x, -200 - arc, z };                       /* na altura dos troncos, sob as copas */
		render_mesh(&bicho_mesh, &pos, &rot, ONE * 2, NULL);
	}
	am.last_polys = render_stats_polys_now() - polys0;
}

#ifdef ROBOT_SHOTS
/* SÓ nas capturas (09-salto): um lugar livre a ~2,5 m de uma árvore que tem
 * outra a até 5 m, e o ângulo da câmera para olhar para ela. 1 = achou. */
int ambient_shot_spot(int *px, int *pz, int *yaw) {
	const PLAYER *p = me();
	for (int r = 1500; r <= 6000; r += 1500) {
		int ax, az, bx, bz;
		if (!tree_near(p->pos.vx, p->pos.vz, 0, r, &ax, &az, -1, -1)) continue;
		if (!tree_near(ax, az, 400, 1300, &bx, &bz, ax, az)) continue;
		for (int k = 0; k < 16; k++) {
			int a = k * 256, x = ax + ((isin(a) * 640) >> 12), z = az + ((icos(a) * 640) >> 12);
			if (!level_blocked(x, z, PLAYER_RADIUS + 60)) {
				*px = x; *pz = z;
				*yaw = angle_of(ax - x, az - z);
				return 1;
			}
		}
	}
	return 0;
}
#endif

/* Depuração (overlay L2, página AMBIENTE): força na hora, sem freio */
void ambient_debug(int what) {
	switch (what) {
	case 0: am.fl.active = 0; try_start(AM_FLOCK_TREE); break;
	case 1: am.ey.active = 0; try_start(AM_EYES); break;
	case 2: am.sh.active = 0; try_start(AM_SHADOW); break;
	case 3: am.jp.active = 0; try_start(AM_JUMP); break;
	case 4: try_start(AM_BRANCH); break;
	case 5: am.moon_front = S(10); break;
	case 6: am.fl.active = 0; try_start(AM_FLOCK_SKY); break;
	case 7: am.moon_front = 0; break;
	}
}

void ambient_stats(int *active, int *polys, int *delayed, int *cpu) {
	*active = am.fl.active + am.ey.active + am.sh.active + am.jp.active;
	*polys = am.last_polys;
	*delayed = am.delayed;
	*cpu = am.cpu_last;
}
