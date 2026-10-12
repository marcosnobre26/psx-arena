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
#ifndef PACKET_LEN                      /* (./dev bench -D testa outros tamanhos) */
#define PACKET_LEN      (96*1024) /* memória para primitivas por quadro */
#endif
/* Orçamento do buffer de primitivas: o mundo (blocos e árvores) pode usar
 * até PACKET_LEN - PACKET_RESERVE; a reserva fica para inimigos, jogadores,
 * efeitos e HUD. Os blocos param antes, em ... - PACKET_TREE_ROOM, para
 * sobrar lugar às árvores próximas. Tudo é desenhado do perto para o longe,
 * então o que é cortado é sempre o mais distante (aviso CORTE no L2). */
#define PACKET_RESERVE   (16*1024)
#define PACKET_TREE_ROOM (8*1024)
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
#define CHUNK_BUILDS_PER_FRAME 1 /* (sem uso desde a 10a: o anel externo é montado em fatias) */
#define MAX_TREES       1024    /* árvores e troncos no mapa (8 bytes cada) */
#define PLACE_SEARCH    6       /* level_place_safe: procura célula livre até aqui */
#define FOREST_EDGE     4       /* borda de mata alta das florestas (células) */
/* Eventos (etapa 06): tempos em passos de 1/60 s */
#define EV_QUEUE_LEN     8
#define EV_GRACE         (30 * 60)   /* nada nos primeiros 30 s da fase */
#define EV_MAP_COOLDOWN  (30 * 60)   /* no máximo 1 evento que muda o mapa a cada 30 s */
#define EV_FOG_RAMP      (5 * 60)    /* névoa: 5 s descendo, ... */
#define EV_FOG_TOTAL     (40 * 60)   /* ... e volta ao normal depois de ~40 s */
#define EV_SILENCE_TIME  (12 * 60)
#define EV_FALL_WARN     50          /* árvore: estalo antes de cair (passos) */
#define EV_FALL_TIME     36          /* ... e o tombo */
#define EV_FALL_LEN      1024        /* comprimento do pinheiro deitado (escala ONE) */
#ifndef EV_SHAKE_TIME
#define EV_SHAKE_TIME    24          /* tremor da câmera no impacto */
#endif
#define EV_JOB_TRIES     90          /* busca de lugar: passos no máximo */
#define EV_PLANT_COUNT   5           /* mata fecha: árvores novas */
#define EV_WAVE_COUNT    4           /* onda: inimigos */
#ifndef MAX_TICKS_PER_FRAME
#define MAX_TICKS_PER_FRAME 2        /* passos de lógica por quadro, no máximo (10a: sob sobrecarga,
                                         câmera lenta em vez de prender em 20 FPS) */
#endif
#ifndef SHADOW_SIMPLE_DIST
#define SHADOW_SIMPLE_DIST 800        /* sombra vira 1 quad além disto da câmera (visual) */
#endif
#define REBUILD_QUEUE    9           /* blocos esperando remontagem (anel de raízes: até 3x3) */
#define REBUILD_LINES    40          /* montagem em fatias: linhas por quadro, no máximo */
#ifndef REBUILD_CPU_CAP
#define REBUILD_CPU_CAP  440         /* ... e só se o quadro anterior deixou folga até aqui */
#endif
#define REACH_WIN        33          /* janela da checagem local (células) */
#define REACH_MAX_BLOCK  12          /* células bloqueadas por evento, no máximo */
#ifndef REACH_MAX_VISIT
#define REACH_MAX_VISIT  600         /* checagem local: visitas no máximo (em passos de EV_CHECK_VISITS) */
#endif
#define EV_FITS_PER_TICK 3           /* busca de lugar: testes de pegada por passo */
#define EV_CHECK_VISITS  40          /* ... e visitas da checagem de alcance por passo */
#ifndef MAPGEN_TRIES
#define MAPGEN_TRIES    16      /* mapa gerado inválido: tenta semente + 1 até aqui */
#endif
#ifndef TREE_LOD_DIST                   /* (./dev bench -D TREE_LOD_DIST=... testa outros) */
#define TREE_LOD_DIST   1300    /* profundidade em que a árvore vira imagem plana
                                   (1800 -> 1300 na 04d: canto denso a 30 FPS) */
#endif
#define TREE_OCCLUDE_TICKS 18   /* árvore que tapou o jogador fica 50% por ~0,3 s */
#define WALL_SLICE      192     /* paredes são montadas em fatias desta altura */
#define THICKET_HEIGHT  768     /* mata densa: 3 m (4 fatias) */
#define BARRIER_HEIGHT  384     /* barreira de raízes: 1,5 m (2 fatias; etapa 06) */
#define GRAVITY         3       /* aceleração para baixo por quadro */

/* ---------- Câmera ---------- */
#define CAM_DIST        820     /* distância atrás do jogador */
#define CAM_MIN_DIST    260     /* o mais perto que chega ao encostar na parede */
#define CAM_HEIGHT      600     /* altura acima do jogador */
#define CAM_LOOK_UP     120     /* ponto que a câmera mira (acima do pé) */
#define CAM_STEP        64      /* passo ao aproximar/afastar por causa de troncos */
#define TITLE_CAM_DIST  1400    /* tela de título: raio do giro em volta do centro */
#define TITLE_CAM_HEIGHT 600
#define CAM_DENSE_DIST  540     /* na mata densa a câmera chega até aqui (de CAM_DIST) */
#define CAM_DENSE_FULL  8       /* "densidade" (árvores em ~4 células) = aproximação total */
#define CAM_FRAME_FULL  600     /* 2 jogadores: recuo de enquadramento que anula a aproximação */
#define CAM_BLOCK_RAISE 120     /* quanto a câmera sobe, no máximo, ao aproximar por tronco */
#define CAM_TRUNK_MARGIN 32     /* folga entre a linha da câmera e um tronco */
#define CAM_TURN_SPEED  40      /* giro com L1/R1 por quadro */

/* ---------- Jogador ---------- */
/* vida, velocidade e pulo de cada personagem ficam em data.c (character_defs) */
#define PLAYER_MAX_ENERGY   100
#define PLAYER_RADIUS       70
#ifndef MAX_PLAYER_SEPARATION       /* (./dev bench -D testa outros valores) */
#define MAX_PLAYER_SEPARATION 896   /* 2 jogadores: distância máxima entre eles
                                       (escolhido pelo ./dev bench: 30 FPS nas 8 direções) */
#endif
#define ENEMY_HOP           27  /* pulo do inimigo sobre tronco caído (sobe ~120) */
#define PLAYER_INVULN       60  /* quadros invulnerável após levar dano */
#define ENERGY_REGEN_DELAY  120 /* mana: +1 a cada 2 s ... */
#define ENERGY_REGEN_CAP    25  /* ... só até 25% do máximo (acima, só poções/gemas) */
#define MAX_ARROWS          20  /* munição máxima por jogador */
#define MAX_BULLETS_AMMO    12
#define ARROW_PICKUP        4   /* feixe de flechas (letra V) */
#define BULLET_PICKUP       3   /* caixa de balas (letra M) */
#define MAX_GROUND_ARROWS   6   /* flechas no chão (a mais antiga some) */
#define ARROW_DROP_CHANCE   40  /* % de a flecha ficar no chão (g.rng) */
#define BOW_CHARGE          45  /* passos para a carga cheia do arco */
#define GUN_AIM_CONE        150 /* mira automática só das armas de fogo (~13 graus) */
#define MAX_NOISES          4   /* barulhos guardados por passo (os mais fortes) */
#define STEP_NOISE          3   /* passo andando (células); R2 segurado = 0 */
#define CHOP_NOISE          8   /* golpe de machado em copa/raízes: barulho médio */
#define CHOP_FALLEN_HP      3   /* golpes para a árvore caída ceder */
#define CHOP_BARRIER_HP     4   /* golpes para a barreira de raízes ceder */
#define FALL_NOISE          14  /* árvore caindo: barulho alto */
#define FLASH_STUN          90  /* clarão: passos atordoado ... */
#define FLASH_FLEE          180 /* ... e fugindo */

/* ---------- Limites de objetos ---------- */
#define MAX_PLAYERS     2       /* jogadores ao mesmo tempo (porta 1 e 2) */
#define RESPAWN_TIME    300     /* 5 s caído antes de voltar (com 2 jogadores) */
#define MAX_ENEMIES     24
#define MAX_BULLETS     48
#define MAX_PICKUPS     24
#define INV_SLOTS        8      /* espaços do inventário (etapa 08: escassez) */
#define EXPLORE_R_DARK   4      /* névoa de exploração: raio visto sem lanterna (células) */
#define EXPLORE_R_LIGHT  6      /* ... com a lanterna ligada */
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
#define VOL_PASSO_GRANDE 0x3fff /* passos pesados do grande (etapa 07) */
#define VOL_GOLPE       0x2c00  /* espada / machado / arco / flecha / magias */
#define VOL_MAGIA       0x3400
#define VOL_GRITO       0x3800
#define VOL_ESTALO      0x3400  /* eventos (etapa 06) */
#define VOL_QUEDA       0x3fff
#define VOL_RAIZES      0x3000
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
