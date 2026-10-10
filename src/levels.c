/*
 * levels.c - TABELA DE FASES: mapa, aparência e objetivo de cada fase
 *
 * Para criar uma fase: desenhe o mapa (vetor de strings terminado em NULL)
 * e acrescente uma linha em level_defs. A ordem da tabela é a ordem do jogo.
 *
 * Legenda do mapa (cada caractere = 1 bloco de 256x256, 1 metro):
 *   #  parede            .  chão vazio         (espaço) = nada/vazio
 *   P  início do jogador C  caixa (quebra)     G  gema (pontos)
 *   H  vida              N  energia            W  arma nova
 *   X  saída da fase     Q  item de missão     S  ponto de reforço (SURVIVE)
 *   L  pilha da lanterna
 *   Letras de inimigos e dígitos de cenário: data.c (enemy_defs, prop_defs)
 *
 * Letras reservadas (não use em enemy_defs/prop_defs): # . P C G H N W X Q S L
 * e A (ATIRADOR, registrado no histórico, ainda não está no código).
 *
 * Objetivos (ver objective.c): OBJ_KILL_ALL, OBJ_REACH_EXIT (precisa de X),
 * OBJ_COLLECT (parâmetro = quantos Q; 0 = todos), OBJ_SURVIVE (parâmetro =
 * segundos; "máx. inimigos" limita os reforços que chegam pelos S).
 * Com X no mapa, cumprir o objetivo abre a saída e vence-se ao chegar nela.
 *
 * Em cima do texto fica o "norte" (Z menor). Todas as linhas com o mesmo
 * tamanho; máximo 128x128 (MAP_MAX_W/H). Mapas grandes podem ser feitos
 * por código: map = NULL e uma função em LEVEL_DEF.build (ver fase 5).
 */
#include "game.h"

#define FOREST_BORDER  4

/* Hash de posição COM SEMENTE. Cada fase usa uma semente fixa própria
 * (florestas diferentes, sempre iguais). Na etapa 05 o gerador deve passar
 * uma semente derivada de g.seed (já definida antes do level_load): mapas
 * diferentes a cada partida, iguais nos dois consoles. Nunca use g.rng
 * aqui (o mapa não deve consumir o gerador da lógica) nem nada local
 * (tempo, VSync). */
#define FOREST_TEST_SEED 0x5eed04b1u

/* Ajustes da fase de teste que o ./dev bench -D pode trocar (ex.:
 * ./dev bench -D FOREST_FOG_FAR=2800) para comparar desempenho */
#ifndef FOREST_FOG_FAR
#define FOREST_FOG_FAR    2800      /* névoa: além disso nada é desenhado (3200 -> 2800 na 04d) */
#endif
#ifndef FOREST_DENSE
#define FOREST_DENSE      50        /* árvores por mil células no interior */
#endif
#ifndef FOREST_DENSE_NE
#define FOREST_DENSE_NE   160       /* ... no canto nordeste (o pior caso) */
#endif

static uint32_t fhash(uint32_t seed, int x, int z) {
	uint32_t h = seed ^ ((uint32_t)x * 374761393u + (uint32_t)z * 668265263u);
	h = (h ^ (h >> 13)) * 1274126177u;
	return h ^ (h >> 16);
}

/* Parâmetros de uma floresta. A estrutura é sempre a mesma: borda de mata
 * alta, duas trilhas que se cruzam, clareiras, árvores por hash, troncos
 * caídos. O que muda é o tamanho, a semente, a densidade e as clareiras. */
typedef struct { int x, z, r; } CLEARING;
typedef struct {
	int             w, h;             /* tamanho em células (até 128) */
	uint32_t        seed;
	int             dense;            /* árvores por mil células no interior */
	int             dense_ne;         /* por mil no canto nordeste (0 = igual) */
	int             trail_x, trail_z; /* colunas/linhas centrais das trilhas */
	const CLEARING *clear;
	int             nclear;
	int             logs;             /* troncos caídos */
} FOREST_PARAMS;

static const FOREST_PARAMS *fp_cur;     /* floresta sendo montada */

static int in_clearing(int x, int z) {
	for (int i = 0; i < fp_cur->nclear; i++) {
		int dx = x - fp_cur->clear[i].x, dz = z - fp_cur->clear[i].z;
		if (dx * dx + dz * dz <= fp_cur->clear[i].r * fp_cur->clear[i].r)
			return 1;
	}
	return 0;
}

static int near_trail(int x, int z, int w) {
	return (z >= fp_cur->trail_z - w && z <= fp_cur->trail_z + w) ||
	       (x >= fp_cur->trail_x - w && x <= fp_cur->trail_x + w);
}

/* Árvore com variação por hash: tipo (pinheiro 65%/seca 35%), deslocamento
 * dentro da célula, rotação e tamanho. */
static void forest_tree(uint32_t seed, int x, int z) {
	uint32_t h = fhash(seed ^ 0x7a3u, x, z);
	int type = (h % 100) < 65 ? TREE_PINE : TREE_DEAD;
	const TREE_DEF *d = &tree_defs[type];
	int wx = x * TILE_SIZE + TILE_SIZE / 2 + (int)((h >> 8) % 81) - 40;
	int wz = z * TILE_SIZE + TILE_SIZE / 2 + (int)((h >> 16) % 81) - 40;
	int scale = d->scale_min + (int)((h >> 4) % (uint32_t)(d->scale_max - d->scale_min + 1));
	level_add_tree(type, wx, wz, (h >> 20) & 4095, scale);
}

static void build_forest(const FOREST_PARAMS *fp) {
	const uint32_t seed = fp->seed;
	const int W = fp->w, H = fp->h;
	fp_cur = fp;
	level_begin(W, H);

	for (int z = 0; z < H; z++)
	for (int x = 0; x < W; x++) {
		uint32_t h = fhash(seed, x, z), patch = fhash(seed, x >> 2, z >> 2);
		int trail = near_trail(x, z, 1);
		int border = x < FOREST_BORDER || z < FOREST_BORDER ||
		             x >= W - FOREST_BORDER || z >= H - FOREST_BORDER;
		int type;

		/* chão: manchas de 4x4 células de folhas/raízes/terra */
		switch (patch % 10) {
		case 0: case 1:  type = CELL_ROOTS;  break;
		case 2:          type = CELL_DIRT;   break;
		default:         type = CELL_LEAVES; break;
		}
		if (trail)
			type = (h % 9 == 0) ? CELL_ROOTS : CELL_DIRT;   /* trilha com raízes soltas */
		else if (in_clearing(x, z))
			type = CELL_LEAVES;
		if (border)
			type = CELL_THICKET;                         /* borda: mata alta de 3 m */
		level_set_cell(x, z, type);
	}

	/* ÁRVORES (depois das células: level_set_cell zeraria a solidez).
	 * Nunca nas trilhas, perto delas ou nas clareiras. */
	for (int z = FOREST_BORDER; z < H - FOREST_BORDER; z++)
	for (int x = FOREST_BORDER; x < W - FOREST_BORDER; x++) {
		if (near_trail(x, z, 2) || in_clearing(x, z))
			continue;
		int edge = x == FOREST_BORDER || z == FOREST_BORDER ||
		           x == W - FOREST_BORDER - 1 || z == H - FOREST_BORDER - 1;
		int dense = (fp->dense_ne && x >= W * 3 / 4 && z < H / 4) ? fp->dense_ne : fp->dense;
		if (edge ? ((x + z) & 1) == 0                   /* fileira na frente da borda */
		         : fhash(seed, x, z) % 1000 < (uint32_t)dense)
			forest_tree(seed, x, z);
	}

	/* TRONCOS CAÍDOS: deitados em X ou em Z (a colisão é por célula), em 3
	 * células livres longe das trilhas */
	for (int k = 0, placed = 0; k < 400 && placed < fp->logs; k++) {
		uint32_t h = fhash(seed ^ 0x1061u, k, 77);
		int x = FOREST_BORDER + 2 + (int)(h % (W - 2 * FOREST_BORDER - 4));
		int z = FOREST_BORDER + 2 + (int)((h >> 12) % (H - 2 * FOREST_BORDER - 4));
		int along_z = (h >> 24) & 1;
		int ok = 1;
		for (int i = -2; i <= 2 && ok; i++) {              /* 3 células + folga de 1 */
			int cx = x + (along_z ? 0 : i), cz = z + (along_z ? i : 0);
			if (near_trail(cx, cz, 2) || in_clearing(cx, cz) || level_cell_solid(cx, cz) ||
			    collide_cell_has_obstacle(cx, cz))
				ok = 0;
		}
		if (!ok)
			continue;
		int jitter = (int)((h >> 4) % 129) - 64;           /* +-5 graus */
		level_add_tree(TREE_LOG, x * TILE_SIZE + TILE_SIZE / 2, z * TILE_SIZE + TILE_SIZE / 2,
		               (along_z ? 1024 : 0) + jitter,
		               tree_defs[TREE_LOG].scale_min + (int)((h >> 8) % 400));
		placed++;
	}
}

#define NCLEAR(list) ((int)(sizeof(list) / sizeof(list[0])))   /* nº de clareiras */

/* Coloca uma lista de coisas, cada uma na célula livre mais próxima */
typedef struct { char ch; int x, z; } THING;

static void place_all(const THING *t, int n) {
	for (int i = 0; i < n; i++)
		level_place_safe(t[i].ch, t[i].x, t[i].z);
}
#define PLACE_ALL(list) place_all(list, (int)(sizeof(list) / sizeof(list[0])))

/* ------------------------------------------------------------------ */
/* Fase 5 (só teste: DEBUG_START_LEVEL 5 e ./dev bench)                 */
/* ------------------------------------------------------------------ */
#define TRAIL_Z      63      /* trilha leste-oeste (3 células: 62..64) */
#define TRAIL_X      63      /* trilha norte-sul */

static const CLEARING test_clearings[] = {
	{ 32, 32, 7 }, { 96, 96, 9 }, { 96, 30, 6 }, { 30, 96, 7 },
	{ 108, 16, 2 },          /* respiro no canto denso (ponto de medição) */
};
static const FOREST_PARAMS test_forest = {
	128, 128, FOREST_TEST_SEED, FOREST_DENSE, FOREST_DENSE_NE, TRAIL_X, TRAIL_Z,
	test_clearings, NCLEAR(test_clearings), 15,
};

static void build_test_forest(void) {
	build_forest(&test_forest);
	static const THING things[] = {
		{ 'P', 8, TRAIL_Z }, { 'X', 128 - 9, TRAIL_Z },      /* início a oeste, saída a leste */
		{ 'E', 30, 30 }, { 'E', 34, 33 }, { 'F', 28, 97 }, { 'E', 32, 95 },
		{ 'B', 96, 96 }, { 'E', 93, 99 }, { 'F', 96, 29 }, { 'E', 63, 40 },
		{ 'E', 63, 90 }, { 'F', 85, 63 }, { 'E', 45, 63 },
		{ 'L', 33, 31 }, { 'L', 97, 95 }, { 'L', 63, 20 },
		{ 'H', 31, 97 }, { 'H', 75, 63 }, { 'N', 95, 31 }, { 'W', 63, 63 },
		{ 'G', 108, 16 }, { 'G', 20, 63 }, { 'G', 63, 110 },
	};
	PLACE_ALL(things);
}

/* ------------------------------------------------------------------ */
/* Fases 1-4 na floresta (cada uma com semente fixa própria)            */
/* ------------------------------------------------------------------ */
/* Tudo é colocado com level_place_safe (nada nasce dentro de tronco ou de
 * mata). Ângulos dos pontos de teleporte: 0 = sul (+Z), 1024 = leste. */

/* Fase 1: CACADA (derrotar todos), 64x64 */
static const CLEARING f1_clear[] = { { 16, 16, 5 }, { 47, 16, 4 }, { 47, 47, 6 }, { 16, 47, 5 } };
static const FOREST_PARAMS f1_forest = { 64, 64, 0xf0e57001u, 50, 0, 31, 31, f1_clear, NCLEAR(f1_clear), 6 };
static void build_f1(void) {
	build_forest(&f1_forest);
	static const THING t[] = {
		{ 'P', 8, 31 },
		{ 'E', 16, 16 }, { 'E', 18, 15 }, { 'F', 14, 18 }, { 'E', 47, 16 }, { 'B', 45, 17 },
		{ 'E', 47, 47 }, { 'E', 49, 46 }, { 'F', 46, 49 }, { 'E', 16, 47 }, { 'F', 18, 48 },
		{ 'E', 31, 12 }, { 'E', 31, 52 },
		{ 'H', 17, 17 }, { 'H', 46, 46 }, { 'N', 47, 15 }, { 'L', 15, 46 }, { 'L', 31, 40 },
		{ 'W', 31, 31 }, { 'G', 20, 31 }, { 'G', 40, 31 }, { 'G', 31, 20 },
		{ 'C', 10, 10 }, { 'C', 52, 52 }, { 'C', 52, 12 },
	};
	PLACE_ALL(t);
}
static const DEBUG_POINT f1_pts[4] = { { 8, 31, 1024 }, { 31, 31, 512 }, { 47, 47, 3072 }, { 16, 16, 0 } };

/* Fase 2: TRAVESSIA (chegar à saída), 128x128, oeste -> leste */
static const CLEARING f2_clear[] = { { 32, 32, 7 }, { 96, 96, 9 }, { 96, 30, 6 }, { 30, 96, 7 } };
static const FOREST_PARAMS f2_forest = { 128, 128, 0xf0e57002u, 50, 0, 63, 63, f2_clear, NCLEAR(f2_clear), 15 };
static void build_f2(void) {
	build_forest(&f2_forest);
	static const THING t[] = {
		{ 'P', 8, 63 }, { 'X', 119, 63 },
		{ 'E', 20, 60 }, { 'E', 30, 30 }, { 'F', 34, 33 }, { 'E', 45, 63 }, { 'E', 63, 40 },
		{ 'F', 63, 52 }, { 'E', 63, 75 }, { 'B', 63, 90 }, { 'E', 80, 63 }, { 'F', 85, 60 },
		{ 'E', 96, 96 }, { 'E', 93, 99 }, { 'B', 96, 29 }, { 'F', 105, 66 },
		{ 'L', 33, 31 }, { 'L', 97, 95 }, { 'L', 63, 20 }, { 'H', 31, 97 }, { 'H', 75, 63 },
		{ 'N', 95, 31 }, { 'W', 63, 63 }, { 'G', 20, 63 }, { 'G', 63, 110 }, { 'G', 110, 60 },
		{ 'C', 40, 70 }, { 'C', 90, 50 },
	};
	PLACE_ALL(t);
}
static const DEBUG_POINT f2_pts[4] = { { 8, 63, 1024 }, { 63, 63, 512 }, { 96, 30, 2048 }, { 96, 96, 3072 } };

/* Fase 3: RELIQUIAS (4 itens e a saída), 64x64 */
static const CLEARING f3_clear[] = { { 14, 14, 5 }, { 49, 14, 5 }, { 49, 49, 5 }, { 14, 49, 5 } };
static const FOREST_PARAMS f3_forest = { 64, 64, 0xf0e57003u, 50, 0, 31, 31, f3_clear, NCLEAR(f3_clear), 6 };
static void build_f3(void) {
	build_forest(&f3_forest);
	static const THING t[] = {
		{ 'P', 31, 56 }, { 'X', 31, 31 },
		{ 'Q', 14, 14 }, { 'Q', 49, 14 }, { 'Q', 49, 49 }, { 'Q', 14, 49 },
		{ 'E', 16, 15 }, { 'F', 13, 16 }, { 'E', 48, 15 }, { 'B', 50, 13 },
		{ 'E', 48, 48 }, { 'F', 50, 50 }, { 'E', 15, 48 }, { 'E', 13, 50 },
		{ 'H', 31, 44 }, { 'N', 20, 31 }, { 'L', 42, 31 }, { 'L', 31, 18 }, { 'G', 24, 24 },
		{ 'C', 40, 40 }, { 'C', 22, 40 },
	};
	PLACE_ALL(t);
}
static const DEBUG_POINT f3_pts[4] = { { 31, 56, 2048 }, { 31, 31, 0 }, { 49, 49, 3072 }, { 14, 14, 1024 } };

/* Fase 4: CERCO (sobreviver), 64x64. Começa no meio de uma clareira grande;
 * os reforços (S) ficam em células LIVRES perto da borda da clareira (não na
 * mata alta, que é sólida); objective.c escolhe o S fora do cone de visão. */
static const CLEARING f4_clear[] = { { 31, 31, 11 } };
static const FOREST_PARAMS f4_forest = { 64, 64, 0xf0e57004u, 50, 0, 31, 31, f4_clear, NCLEAR(f4_clear), 6 };
static void build_f4(void) {
	build_forest(&f4_forest);
	static const THING t[] = {
		{ 'P', 31, 31 },
		{ 'S', 24, 24 }, { 'S', 38, 24 }, { 'S', 38, 38 }, { 'S', 24, 38 },
		{ 'E', 26, 34 }, { 'F', 36, 28 }, { 'E', 28, 27 },
		{ 'H', 33, 33 }, { 'N', 29, 33 }, { 'L', 33, 29 }, { 'L', 27, 29 },
		{ 'C', 35, 35 }, { 'C', 27, 35 },
	};
	PLACE_ALL(t);
}
static const DEBUG_POINT f4_pts[4] = { { 31, 31, 0 }, { 31, 31, 1024 }, { 31, 31, 512 }, { 24, 24, 2560 } };

/* Teleporte de depuração (L2 + direcional com o overlay aberto): sempre
 * os mesmos lugares e a mesma direção de câmera, para medir FPS/POLIS.
 * Ordem: esquerda, direita, cima, baixo. Ângulo: 0 = sul (+Z), 1024 = leste. */
static const DEBUG_POINT forest_debug_pts[4] = {
	{   8, TRAIL_Z, 1024 },   /* <- início, olhando a trilha para leste */
	{ TRAIL_X, TRAIL_Z, 512 },/* -> cruzamento das trilhas, olhando para sudeste */
	{ 108,  16, 2048 },       /* ^  canto denso (nordeste), olhando a borda norte */
	{  96,  96, 3072 },       /* v  maior clareira, olhando para oeste */
};

const LEVEL_DEF level_defs[] = {
	/* nome        mapa
	 *   chão          parede        céu=névoa (R,G,B)  névoa near/far  luz ambiente  música
	 *   objetivo        parâmetro  máx. inimigos  texto do objetivo
	 *   gerador  pontos de teleporte (L2 + direcional; o 3o = pior ponto do bench)
	 *
	 * A câmera fica a ~1000 de profundidade do jogador: near abaixo disso
	 * enevoa o próprio jogador. Lanterna: objetos no cone usam near/far
	 * x LANTERN_FOG_MUL. Céu/névoa escuros (texturas só escurecem).
	 * Névoa e luz de cada fase = etapa 03. */
	{ "CACADA",     NULL,
	  &tex_terra_t, &tex_mata_t,   10, 12, 24,        1300, 3200,     40, 40, 56,   0,
	  OBJ_KILL_ALL,   0,         0,             "DERROTE TODOS OS INIMIGOS",
	  build_f1, f1_pts },
	{ "TRAVESSIA",  NULL,
	  &tex_terra_t, &tex_mata_t,    6, 14, 10,        1200, 2800,     30, 40, 34,   0,
	  OBJ_REACH_EXIT, 0,         0,             "ATRAVESSE A FLORESTA",
	  build_f2, f2_pts },
	{ "RELIQUIAS",  NULL,
	  &tex_terra_t, &tex_mata_t,   16,  8, 20,        1300, 3000,     40, 30, 48,   0,
	  OBJ_COLLECT,    4,         0,             "PEGUE 4 RELIQUIAS E SAIA",
	  build_f3, f3_pts },
	{ "CERCO",      NULL,
	  &tex_terra_t, &tex_mata_t,   20,  8,  8,        1400, 3400,     48, 32, 32,   0,
	  OBJ_SURVIVE,    60,        8,             "SOBREVIVA 60 SEGUNDOS",
	  build_f4, f4_pts },
	/* SÓ TESTE (fora da sequência normal): DEBUG_START_LEVEL 5 e ./dev bench */
	{ "FLORESTA (TESTE)", NULL,
	  &tex_terra_t, &tex_mata_t,    8, 12, 10,        1300, FOREST_FOG_FAR, 36, 40, 36,   0,
	  OBJ_REACH_EXIT, 0,         0,             "ATRAVESSE A FLORESTA",
	  build_test_forest, forest_debug_pts },
};
/* num_levels = fases da sequência normal (a de teste fica depois delas) */
const int num_level_defs = sizeof(level_defs) / sizeof(level_defs[0]);
const int num_levels = 4;

