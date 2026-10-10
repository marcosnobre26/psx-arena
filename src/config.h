/*
 * config.h - Constantes de ajuste do jogo
 *
 * Este é o primeiro arquivo para mexer. Mude um número, recompile e veja o
 * efeito. Todas as distâncias estão em "unidades do mundo": 256 unidades
 * equivalem a 1 metro no Blender (e a 1 bloco do mapa).
 *
 * Ângulos usam o padrão do PS1: 4096 = volta completa (360 graus),
 * 1024 = 90 graus, 512 = 45 graus.
 *
 * Números fracionários não existem aqui: o PS1 não tem FPU. Usamos
 * "ponto fixo", em que 4096 (constante ONE) representa 1.0.
 */
#ifndef CONFIG_H
#define CONFIG_H

/* ---------- Tela ---------- */
#define SCREEN_W        320
#define SCREEN_H        240
#define CLEAR_R         20      /* cor do céu/fundo */
#define CLEAR_G         24
#define CLEAR_B         48

/* ---------- Renderização ---------- */
#define OT_LEN          2048    /* níveis de profundidade da Ordering Table */
#define PACKET_LEN      (96*1024) /* memória para primitivas por quadro */
#define NEAR_Z          40      /* polígonos mais perto que isso são descartados */
#define DRAW_DIST       4200    /* distância inicial; com névoa vira fog_far da lanterna */
#define FOV_H           160     /* "distância da tela": maior = zoom maior */

/* ---------- Clima: luz e névoa ---------- */
/* Cada fase define névoa (near/far) e luz ambiente em levels.c; aqui ficam
 * a "lua" (luz direcional, ONE = 1.0 por canal) e o visual dos menus. */
#define MOON_R          (ONE * 30 / 100)   /* lua: fraca e azulada */
#define MOON_G          (ONE * 36 / 100)
#define MOON_B          (ONE * 55 / 100)
#define MENU_FOG_NEAR   6000    /* tela de seleção: praticamente sem névoa */
#define MENU_FOG_FAR    9000

/* ---------- Lanterna ---------- */
#define LANTERN_MAX       MAX_PLAYERS
#define LANTERN_RANGE     1600  /* alcance do cone (e do "ver mais longe") */
#define LANTERN_FOG_MUL   26    /* no cone a névoa vai 26/16 = 1,6x mais longe */
#define LANTERN_HALF      340   /* meio-ângulo do cone (340/4096*360 ~ 30 graus) */
#define LANTERN_SEGS      4     /* triângulos do leque no chão */
#define LANTERN_R         110   /* cor da ponta do cone (somada ao chão) */
#define LANTERN_G         100
#define LANTERN_B         70
#define BATTERY_MAX       1000
#define BATTERY_DRAIN     6     /* ligada: -1 de bateria a cada N passos (~100 s) */
#define BATTERY_PICKUP    400   /* uma pilha (letra L no mapa) recarrega isto */
#define BATTERY_LOW       150   /* abaixo disto a lanterna falha de vez em quando */
#define LANTERN_DYING     60    /* bateria zerou: pisca por 1 s e apaga */

/* ---------- Mundo ---------- */
#define TILE_SIZE       256     /* tamanho de uma célula do mapa (1 metro) */
#define MAP_MAX_W       128     /* maior mapa: 128 x 128 células */
#define MAP_MAX_H       128
#define CHUNK_SLOTS     36      /* blocos montados ao mesmo tempo (cache) */
#define CHUNK_LOAD_RADIUS 2     /* monta o 5x5 de blocos em volta do grupo */
#define CHUNK_BUILDS_PER_FRAME 1 /* fila: blocos distantes montados por quadro */
#define MAX_TREES       1024    /* árvores e troncos no mapa (8 bytes cada) */
#define TREE_LOD_DIST   1800    /* profundidade em que a árvore vira imagem plana */
#define WALL_SLICE      192     /* paredes são montadas em fatias desta altura */
#define WALL_HEIGHT     384     /* parede dos mapas de texto (2 fatias) */
#define THICKET_HEIGHT  768     /* mata densa: 3 m (4 fatias) */
#define GRAVITY         3       /* aceleração para baixo por quadro */

/* ---------- Câmera ---------- */
#define CAM_DIST        820     /* distância atrás do jogador */
#define CAM_MIN_DIST    260     /* o mais perto que chega ao encostar na parede */
#define CAM_HEIGHT      600     /* altura acima do jogador */
#define CAM_LOOK_UP     120     /* ponto que a câmera mira (acima do pé) */
#define CAM_TURN_SPEED  40      /* giro com L1/R1 por quadro */

/* ---------- Jogador ---------- */
/* vida, velocidade e pulo de cada personagem ficam em data.c (character_defs) */
#define PLAYER_MAX_ENERGY   100
#define PLAYER_RADIUS       70
#define ENEMY_HOP           27  /* pulo do inimigo sobre tronco caído (sobe ~120) */
#define PLAYER_INVULN       60  /* quadros invulnerável após levar dano */
#define ENERGY_REGEN_DELAY  20  /* a cada N quadros recupera 1 de energia */

/* ---------- Limites de objetos ---------- */
#define MAX_PLAYERS     2       /* jogadores ao mesmo tempo (porta 1 e 2) */
#define RESPAWN_TIME    300     /* 5 s caído antes de voltar (com 2 jogadores) */
#define MAX_ENEMIES     24
#define MAX_BULLETS     48
#define MAX_PICKUPS     24
#define MAX_CRATES      32
#define MAX_EFFECTS     16
#define MAX_PROPS       64

/* ---------- Fases e objetivos ---------- */
#define INTRO_TIME          180  /* introdução da fase: 3 s (START/X pulam) */
#define EXIT_RADIUS         160  /* distância para "entrar" na saída (X) */
#define MAX_SPAWN_PTS       8    /* pontos de reforço (S) por mapa */
#define REINFORCE_TIME      480  /* SURVIVE: um reforço a cada 8 s */
#define REINFORCE_MIN_DIST  1200 /* reforço só nasce longe dos jogadores */

/* ---------- Som ---------- */
#define SOUND_NEAR      300     /* até aqui (da câmera) o som fica no volume cheio */
#define SOUND_FAR       3000    /* daqui em diante não se ouve nada */
/* volume de cada evento (0..0x3fff); os sons em si ficam em assets/sounds/ */
#define VOL_PASSO       0x1c00
#define VOL_TIRO        0x3000
#define VOL_ACERTO      0x2c00  /* tiro acertou um inimigo */
#define VOL_PAREDE      0x1400  /* tiro bateu na parede */
#define VOL_DOR         0x3800
#define VOL_MORTE       0x3fff
#define VOL_ITEM        0x3000
#define VOL_ROSNADO     0x3800
#define VOL_VENTO       0x1400  /* loops de ambiente durante a partida */
#define VOL_GRILOS      0x0c00
/* rosnado: inimigo perto e fora da tela "se denuncia" de vez em quando */
#define GROWL_DIST      1800    /* distância máxima até a câmera */
#define GROWL_CHANCE    240     /* 1 chance em N por passo, por inimigo (~4 s) */
#define GROWL_GAP       90      /* passos mínimos entre dois rosnados */

/* ---------- Depuração ---------- */
/* Descomente para forçar a mesma semente em toda partida. Útil para
 * conferir o determinismo: duas partidas com as mesmas entradas devem
 * se comportar igual (mesmos inimigos, mesmos sorteios). */
/* #define DEBUG_FIXED_SEED 1234 */

/* Descomente para o jogo novo começar direto na fase N (1 = primeira).
 * Para testar uma fase sem jogar as anteriores. */
/* #define DEBUG_START_LEVEL 3 */

/* Descomente para ajustar a névoa ao vivo: com o overlay L2 aberto, segure
 * R2 e use o direcional (cima/baixo = far, direita/esquerda = near). Anote
 * os valores bons e passe para levels.c. Trocar de fase volta ao da tabela. */
/* #define DEBUG_FOG_TUNING */
#define COLMARK_RADIUS  5       /* depuração da colisão: raio em células */
#define DEBUG_SPAWN_DIST 640    /* L2 + Triângulo: distância dos inimigos extras */
#define DEBUG_SPAWN_COUNT 8     /* L2 + Triângulo: quantos inimigos extras */

/* DEBUG_BENCHMARK: NÃO defina aqui. Use ./dev bench, que compila numa
 * pasta separada (build-bench/) com o modo ligado. */

#endif
