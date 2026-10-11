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
#include <string.h>
#include "game.h"

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

static int apply(int action, int arg) {
	switch (action) {
	case EV_FOG:     return act_fog(arg);
	case EV_SILENCE: return act_silence(arg);
	}
	return 0;
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
	if (g.play_frames < EV_GRACE) {             /* começo da fase: só o relógio */
		for (int i = 0; i < g.ev.nq; i++)
			if (g.ev.queue[i].force) goto run; /* depuração passa */
		return;
	}
#ifndef DEBUG_BENCHMARK
	check_triggers();                           /* bench: só os forçados */
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
	int a = g.ev.queue[best].action, arg = g.ev.queue[best].arg;
	g.ev.queue[best] = g.ev.queue[--g.ev.nq];
	if (apply(a, arg)) {
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
