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
 *   Letras de inimigos e dígitos de cenário: data.c (enemy_defs, prop_defs)
 *
 * Letras reservadas (não use em enemy_defs/prop_defs): # . P C G H N W X Q S
 * e A (ATIRADOR, registrado no histórico, ainda não está no código).
 *
 * Objetivos (ver objective.c): OBJ_KILL_ALL, OBJ_REACH_EXIT (precisa de X),
 * OBJ_COLLECT (parâmetro = quantos Q; 0 = todos), OBJ_SURVIVE (parâmetro =
 * segundos; "máx. inimigos" limita os reforços que chegam pelos S).
 * Com X no mapa, cumprir o objetivo abre a saída e vence-se ao chegar nela.
 *
 * Em cima do texto fica o "norte" (Z menor). Todas as linhas com o mesmo
 * tamanho; máximo 32x32.
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
	"#.......#..#.1.......1.#",
	"#..E....#G.#.....F.....#",
	"#.......#..#....C.C....#",
	"#..N.........E.........#",
	"#....####.......####...#",
	"#....#..#..B....#G.#...#",
	"#.G..#..#.......#..#.E.#",
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
	"#..N....#...E..#####...#",
	"#..######......#.......#",
	"#.......#..B...#...C...#",
	"#..E....#......#..######",
	"####..###..#####.......#",
	"#.........1#.....E.....#",
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
	"#......#..........C....#",
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
	"#.H...#.........#......#",
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
	"#......................#",
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

const LEVEL_DEF level_defs[] = {
	/* nome     mapa        chão          parede       céu (R,G,B)  música
	 *          objetivo      parâmetro  máx. inimigos  texto do objetivo */
	{ "ARENA",      map_arena,      &tex_floor_t, &tex_wall_t,  20, 24, 48,  0,
	                OBJ_KILL_ALL,   0,         0,             "DERROTE TODOS OS INIMIGOS" },
	{ "CORREDORES", map_corredores, &tex_wall_t,  &tex_floor_t, 14, 30, 22,  0,
	                OBJ_REACH_EXIT, 0,         0,             "ENCONTRE A SAIDA" },
	{ "RELIQUIAS",  map_reliquias,  &tex_floor_t, &tex_crate_t, 34, 18, 40,  0,
	                OBJ_COLLECT,    4,         0,             "PEGUE 4 RELIQUIAS E SAIA" },
	{ "CERCO",      map_cerco,      &tex_wall_t,  &tex_wall_t,  44, 16, 16,  0,
	                OBJ_SURVIVE,    60,        8,             "SOBREVIVA 60 SEGUNDOS" },
};
const int num_levels = sizeof(level_defs) / sizeof(level_defs[0]);
