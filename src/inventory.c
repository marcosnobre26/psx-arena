/*
 * inventory.c - Inventário de cada jogador e névoa de exploração (etapa 08)
 *
 * INVENTÁRIO: INV_SLOTS espaços por jogador (PLAYER.inv). Só os itens de
 * item_defs (data.c) ocupam espaço: bandagem, kit médico, poção de mana e
 * pilha. Cada espaço guarda um tipo e uma quantidade até item_defs.stack
 * (bandagem 3, kit 1, poção 2, pilha 2). Pegar com tudo cheio: o item fica
 * no chão. Usar aplica o efeito e consome uma unidade; descartar devolve
 * uma unidade ao chão (se não houver vaga de item no chão, ela some).
 * Relíquias, chaves, gemas, armas, flechas e balas NÃO ocupam espaço.
 *
 * NÉVOA DE EXPLORAÇÃO: g.seen, 1 bit por célula (128 x 128 = 2 KB). Quando
 * um jogador muda de célula, marca um círculo em volta (maior com a
 * lanterna ligada). É lógica: depende só das posições em g.
 *
 * Tudo aqui é lógica (muda g); mensagens via show_message.
 */
#include <psxpad.h>
#include <stdio.h>
#include "game.h"

int inv_count(const PLAYER *p, int type) {
	int n = 0;
	for (int i = 0; i < INV_SLOTS; i++)
		if (p->inv[i].type == type) n += p->inv[i].count;
	return n;
}

/* Guarda uma unidade: primeiro num espaço do mesmo tipo com folga, senão
 * no primeiro espaço vazio. 0 = inventário cheio para este item. */
int inv_add(PLAYER *p, int type) {
	for (int i = 0; i < INV_SLOTS; i++)
		if (p->inv[i].type == type && p->inv[i].count < item_defs[type].stack) {
			p->inv[i].count++;
			return 1;
		}
	for (int i = 0; i < INV_SLOTS; i++)
		if (p->inv[i].type == IT_NONE) {
			p->inv[i].type = (uint8_t)type;
			p->inv[i].count = 1;
			return 1;
		}
	return 0;
}

static void take_one(PLAYER *p, int slot) {
	if (--p->inv[slot].count == 0)
		p->inv[slot].type = IT_NONE;
}

/* Usa uma unidade do espaço slot. Não gasta à toa: com a vida (ou a mana,
 * ou a pilha) cheia, avisa e não consome. */
int inv_use(PLAYER *p, int slot) {
	if (slot < 0 || slot >= INV_SLOTS || !player_alive(p)) return 0;
	int type = p->inv[slot].type;
	if (type == IT_NONE) return 0;
	const ITEM_DEF *d = &item_defs[type];
	char buf[24];
	switch (type) {
	case IT_BANDAGE:
	case IT_MEDKIT:
		if (p->hp >= p->max_hp) { show_message("VIDA CHEIA", 40); return 0; }
		p->hp += d->amount;
		if (p->hp > p->max_hp) p->hp = p->max_hp;
		PL(pl_heal(0));
		effect_spawn(FX_HEAL, p->pos.vx, p->pos.vz, 300, 30, 255, 80, 100);
		snprintf(buf, sizeof(buf), "+%d VIDA", d->amount);
		break;
	case IT_POTION: {
		if (p->energy >= PLAYER_MAX_ENERGY) { show_message("MANA CHEIA", 40); return 0; }
		int before = p->energy;
		p->energy += d->amount;
		if (p->energy > PLAYER_MAX_ENERGY) p->energy = PLAYER_MAX_ENERGY;
		PL(pl_found(p, AMMO_MANA, p->energy - before));
		(void)before;
		effect_spawn(FX_HEAL, p->pos.vx, p->pos.vz, 300, 30, 60, 160, 255);
		snprintf(buf, sizeof(buf), "+%d MANA", d->amount);
		break;
	}
	case IT_BATTERY:
		if (p->battery >= BATTERY_MAX) { show_message("PILHA CHEIA", 40); return 0; }
		p->battery += d->amount;
		if (p->battery > BATTERY_MAX) p->battery = BATTERY_MAX;
		p->lantern_dying = 0;
		snprintf(buf, sizeof(buf), "PILHA TROCADA");
		break;
	default:
		return 0;
	}
	take_one(p, slot);
	p->st.items_used++;
	show_message(buf, 40);
	sound_play_at(&sfx_item, p->pos.vx, p->pos.vz, VOL_ITEM);
	return 1;
}

/* Larga uma unidade no chão, aos pés do jogador */
int inv_drop(PLAYER *p, int slot) {
	if (slot < 0 || slot >= INV_SLOTS || !player_alive(p)) return 0;
	int type = p->inv[slot].type;
	if (type == IT_NONE) return 0;
	take_one(p, slot);
	int k = pickup_spawn(item_defs[type].pick, p->pos.vx, p->pos.vz);
	if (!k) {
		show_message("ITEM SUMIU (CHAO CHEIO)", 60);
		return 1;
	}
	/* nasce sob o jogador: ele só pega de volta depois de se afastar */
	g.pickups[k - 1].lock = p->index + 1;
	show_message("LARGOU NO CHAO", 40);
	return 1;
}

/* Atalho do CÍRCULO: a cura mais fraca primeiro (bandagem, depois kit).
 * A magia CURA nunca entra aqui: gastar mana é escolha do jogador. */
void inv_quick_heal(PLAYER *p) {
	static const int order[] = { IT_BANDAGE, IT_MEDKIT };
	for (int k = 0; k < 2; k++)
		for (int i = 0; i < INV_SLOTS; i++)
			if (p->inv[i].type == order[k]) {
				inv_use(p, i);
				return;
			}
	show_message("SEM CURA", 40);
}

/* ------------------------------------------------------------------ */
/* Névoa de exploração                                                  */
/* ------------------------------------------------------------------ */
int explore_seen(int cx, int cz) {
	if (cx < 0 || cz < 0 || cx >= MAP_MAX_W || cz >= MAP_MAX_H) return 0;
	return (g.seen[cz][cx >> 3] >> (cx & 7)) & 1;
}

static void mark_disk(int cx, int cz, int r) {
	for (int z = cz - r; z <= cz + r; z++) {
		if (z < 0 || z >= MAP_MAX_H) continue;
		for (int x = cx - r; x <= cx + r; x++) {
			if (x < 0 || x >= MAP_MAX_W) continue;
			int dx = x - cx, dz = z - cz;
			if (dx * dx + dz * dz <= r * r + r)
				g.seen[z][x >> 3] |= (uint8_t)(1 << (x & 7));
		}
	}
}

/* Um passo: quem mudou de célula marca o que vê (4 células no escuro, 6
 * com a lanterna). Barato: só ao trocar de célula (~1 vez a cada 15 passos). */
void explore_update(void) {
	for (int i = 0; i < MAX_PLAYERS; i++) {
		PLAYER *p = &g.players[i];
		if (!player_alive(p)) continue;
		int cx = p->pos.vx / TILE_SIZE, cz = p->pos.vz / TILE_SIZE;
		if (cx == p->seen_cx && cz == p->seen_cz) continue;
		p->seen_cx = cx; p->seen_cz = cz;
		mark_disk(cx, cz, p->lantern_on ? EXPLORE_R_LIGHT : EXPLORE_R_DARK);
	}
}
