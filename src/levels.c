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

static const char *const map_arena[] = {
	"########################",
	"#P.......#......C......#",
	"#........#..E.......G..#",
	"#..CC....#.....####....#",
	"#.............H#..#..E.#",
	"#....G.........#W.#....#",
	"#####...####...##.###..#",
	"#...L...#..#.1.......1.#",
	"#..E....#G.#.....F.....#",
	"#.......#..#....C.C....#",
	"#..N.........E.........#",
	"#....####.......####...#",
	"#....#..#..B....#G.#...#",
	"#.G..#..#......L#..#.E.#",
	"#....##.#..C.C..##.#...#",
	"#.F...........W........#",
	"#.....E....H.......B...#",
	"#..C........G...F...N..#",
	"#..1.....2.....2....1..#",
	"########################",
	NULL
};

/* Fase 2: labirinto; a saída (X) fica no canto oposto ao início. */
static const char *const map_corredores[] = {
	"########################",
	"#P..#.......#.....#....#",
	"#...#..E....#..C..#..H.#",
	"#...#...##..#.....#....#",
	"#...#...#...####..#..E.#",
	"#.......#......#.......#",
	"######..#..F...#..######",
	"#.......#####..#.......#",
	"#..N....#.L.E..#####...#",
	"#..######......#.......#",
	"#.......#..B...#...C...#",
	"#..E....#......#..######",
	"####..###..#####.......#",
	"#.........1#...L.E.....#",
	"#..C.......#..######...#",
	"#.....######..#....#...#",
	"#..H..#.......#.F..#...#",
	"#.........E...#....#.X.#",
	"#..2..#.......2....#...#",
	"########################",
	NULL
};

/* Fase 3: quatro relíquias (Q) nos cantos; a saída (X) só abre com todas. */
static const char *const map_reliquias[] = {
	"########################",
	"#Q.....#.........#....Q#",
	"#..E...#....G....#..E..#",
	"#.L....#..........C....#",
	"#..C......####.........#",
	"#.........#..#....###..#",
	"####..1...#H.#....#....#",
	"#.........##.##...#..F.#",
	"#..F...............N...#",
	"#......#....P....#.....#",
	"#..G...#.........#..G..#",
	"#......####...####.....#",
	"#.....E...........B....#",
	"#..1.......X...........#",
	"#.....####...####..C...#",
	"#.H...#.........#...L..#",
	"#.....#..E...E..#...1..#",
	"#..C..#.........#......#",
	"#Q....#....W....#.....Q#",
	"########################",
	NULL
};

/* Fase 4: sobreviver; reforços chegam pelos quatro cantos (S). */
static const char *const map_cerco[] = {
	"########################",
	"#S.........#..........S#",
	"#......................#",
	"#..1....C.......C...1..#",
	"#..........L...........#",
	"#.....####......####...#",
	"#.....#..........H.#...#",
	"#..E..#..1....1....#.E.#",
	"#..........P...........#",
	"#..N.......G..........N#",
	"#.....#..1....1....#...#",
	"#.....#.H..........#...#",
	"#.....####......####...#",
	"#......................#",
	"#..1....C.......C...1..#",
	"#......................#",
	"#S.........#..........S#",
	"########################",
	NULL
};

/* ------------------------------------------------------------------ */
/* Fase 5: floresta de TESTE (128 x 128), feita por código              */
/* ------------------------------------------------------------------ */
/* Enquanto a geração procedural (etapa 05) não existe, este mapa fixo
 * serve para testar o mundo grande: borda de mata densa, duas trilhas que
 * se cruzam no meio, quatro clareiras, moitas espalhadas e um canto
 * nordeste mais fechado (o pior caso de desempenho). Nada aleatório: a
 * "variedade" vem de um hash da posição, então o mapa é sempre igual. */

#define FOREST_W     128
#define FOREST_H     128
#define FOREST_BORDER  4
#define TRAIL_Z      63      /* trilha leste-oeste (3 células: 62..64) */
#define TRAIL_X      63      /* trilha norte-sul */

/* Hash de posição COM SEMENTE. A fase de teste usa uma semente fixa
 * (FOREST_TEST_SEED: medições sempre no mesmo cenário). Na etapa 05 o
 * gerador deve passar uma semente derivada de g.seed (já definida antes
 * do level_load): mapas diferentes a cada partida, iguais nos dois
 * consoles. Nunca use g.rng aqui (o mapa não deve consumir o gerador
 * da lógica) nem nada local (tempo, VSync). */
#define FOREST_TEST_SEED 0x5eed04b1u

static uint32_t fhash(uint32_t seed, int x, int z) {
	uint32_t h = seed ^ ((uint32_t)x * 374761393u + (uint32_t)z * 668265263u);
	h = (h ^ (h >> 13)) * 1274126177u;
	return h ^ (h >> 16);
}

static const struct { int x, z, r; } clearings[] = {
	{ 32, 32, 7 }, { 96, 96, 9 }, { 96, 30, 6 }, { 30, 96, 7 },
	{ 108, 16, 2 },          /* respiro no canto denso (ponto de medição) */
};
#define NUM_CLEARINGS ((int)(sizeof(clearings) / sizeof(clearings[0])))

static int in_clearing(int x, int z) {
	for (int i = 0; i < NUM_CLEARINGS; i++) {
		int dx = x - clearings[i].x, dz = z - clearings[i].z;
		if (dx * dx + dz * dz <= clearings[i].r * clearings[i].r)
			return 1;
	}
	return 0;
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

static void build_test_forest(void) {
	const uint32_t seed = FOREST_TEST_SEED;
	level_begin(FOREST_W, FOREST_H);

	for (int z = 0; z < FOREST_H; z++)
	for (int x = 0; x < FOREST_W; x++) {
		uint32_t h = fhash(seed, x, z), patch = fhash(seed, x >> 2, z >> 2);
		int trail = (z >= TRAIL_Z - 1 && z <= TRAIL_Z + 1) || (x >= TRAIL_X - 1 && x <= TRAIL_X + 1);
		int border = x < FOREST_BORDER || z < FOREST_BORDER ||
		             x >= FOREST_W - FOREST_BORDER || z >= FOREST_H - FOREST_BORDER;
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
	 * Interior: ~5% das células, ~16% no canto nordeste (o pior caso);
	 * nunca nas trilhas, perto delas ou nas clareiras. */
	for (int z = FOREST_BORDER; z < FOREST_H - FOREST_BORDER; z++)
	for (int x = FOREST_BORDER; x < FOREST_W - FOREST_BORDER; x++) {
		int near_trail = (z >= TRAIL_Z - 2 && z <= TRAIL_Z + 2) || (x >= TRAIL_X - 2 && x <= TRAIL_X + 2);
		if (near_trail || in_clearing(x, z))
			continue;
		int edge = x == FOREST_BORDER || z == FOREST_BORDER ||
		           x == FOREST_W - FOREST_BORDER - 1 || z == FOREST_H - FOREST_BORDER - 1;
		int dense = (x >= 96 && z < 32) ? 160 : 50;     /* por mil */
		if (edge ? ((x + z) & 1) == 0                   /* fileira na frente da borda */
		         : fhash(seed, x, z) % 1000 < (uint32_t)dense)
			forest_tree(seed, x, z);
	}

	/* TRONCOS CAÍDOS: ~15, deitados em X ou em Z (a colisão é por célula),
	 * em 3 células livres longe das trilhas. */
	for (int k = 0, placed = 0; k < 400 && placed < 15; k++) {
		uint32_t h = fhash(seed ^ 0x1061u, k, 77);
		int x = FOREST_BORDER + 2 + (int)(h % (FOREST_W - 2 * FOREST_BORDER - 4));
		int z = FOREST_BORDER + 2 + (int)((h >> 12) % (FOREST_H - 2 * FOREST_BORDER - 4));
		int along_z = (h >> 24) & 1;
		int ok = 1;
		for (int i = -2; i <= 2 && ok; i++) {              /* 3 células + folga de 1 */
			int cx = x + (along_z ? 0 : i), cz = z + (along_z ? i : 0);
			int near_trail = (cz >= TRAIL_Z - 2 && cz <= TRAIL_Z + 2) || (cx >= TRAIL_X - 2 && cx <= TRAIL_X + 2);
			if (near_trail || in_clearing(cx, cz) || level_cell_solid(cx, cz))
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

	/* início a oeste, saída a leste, na trilha */
	level_place('P', 8, TRAIL_Z);
	level_place('X', FOREST_W - 9, TRAIL_Z);

	/* inimigos nas clareiras e ao longo das trilhas */
	static const struct { char ch; int x, z; } things[] = {
		{ 'E', 30, 30 }, { 'E', 34, 33 }, { 'F', 28, 97 }, { 'E', 32, 95 },
		{ 'B', 96, 96 }, { 'E', 93, 99 }, { 'F', 96, 29 }, { 'E', 63, 40 },
		{ 'E', 63, 90 }, { 'F', 85, 63 }, { 'E', 45, 63 },
		{ 'L', 33, 31 }, { 'L', 97, 95 }, { 'L', 63, 20 },
		{ 'H', 31, 97 }, { 'H', 75, 63 }, { 'N', 95, 31 }, { 'W', 63, 63 },
		{ 'G', 108, 16 }, { 'G', 20, 63 }, { 'G', 63, 110 },
	};
	for (int i = 0; i < (int)(sizeof(things) / sizeof(things[0])); i++)
		level_place(things[i].ch, things[i].x, things[i].z);
}

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
	 *
	 * A câmera fica a ~1000 de profundidade do jogador: near abaixo disso
	 * enevoa o próprio jogador. Lanterna: objetos no cone usam near/far
	 * x LANTERN_FOG_MUL. Céu/névoa escuros (texturas só escurecem). */
	{ "ARENA",      map_arena,
	  &tex_floor_t, &tex_wall_t,   10, 12, 24,        1300, 3200,     40, 40, 56,   0,
	  OBJ_KILL_ALL,   0,         0,             "DERROTE TODOS OS INIMIGOS" },
	{ "CORREDORES", map_corredores,
	  &tex_wall_t,  &tex_floor_t,   6, 14, 10,        1200, 2800,     30, 40, 34,   0,
	  OBJ_REACH_EXIT, 0,         0,             "ENCONTRE A SAIDA" },
	{ "RELIQUIAS",  map_reliquias,
	  &tex_floor_t, &tex_crate_t,  16,  8, 20,        1300, 3000,     40, 30, 48,   0,
	  OBJ_COLLECT,    4,         0,             "PEGUE 4 RELIQUIAS E SAIA" },
	{ "CERCO",      map_cerco,
	  &tex_wall_t,  &tex_wall_t,   20,  8,  8,        1400, 3400,     48, 32, 32,   0,
	  OBJ_SURVIVE,    60,        8,             "SOBREVIVA 60 SEGUNDOS" },
	/* teste do mundo grande (04a): mapa por código, sem texto */
	{ "FLORESTA (TESTE)", NULL,
	  &tex_terra_t, &tex_mata_t,    8, 12, 10,        1300, 3200,     36, 40, 36,   0,
	  OBJ_REACH_EXIT, 0,         0,             "ATRAVESSE A FLORESTA",
	  build_test_forest, forest_debug_pts },
};
const int num_levels = sizeof(level_defs) / sizeof(level_defs[0]);
