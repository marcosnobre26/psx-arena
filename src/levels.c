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
 *   Letras de inimigos e dígitos de cenário: data.c (enemy_defs, prop_defs)
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

const LEVEL_DEF level_defs[] = {
	/* nome     mapa        chão          parede       céu (R,G,B)  música
	 *          objetivo      parâmetro  máx. inimigos  texto do objetivo */
	{ "ARENA",  map_arena,  &tex_floor_t, &tex_wall_t, 20, 24, 48,  0,
	            OBJ_KILL_ALL, 0,         0,             "DERROTE TODOS OS INIMIGOS" },
};
const int num_levels = sizeof(level_defs) / sizeof(level_defs[0]);
