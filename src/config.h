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
#define DRAW_DIST       4200    /* objetos além disso não são desenhados */
#define FOV_H           160     /* "distância da tela": maior = zoom maior */

/* ---------- Mundo ---------- */
#define TILE_SIZE       256     /* tamanho de um bloco do mapa */
#define WALL_HEIGHT     384     /* altura das paredes */
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

/* ---------- Som ---------- */
#define SOUND_NEAR      300     /* até aqui (da câmera) o som fica no volume cheio */
#define SOUND_FAR       3000    /* daqui em diante não se ouve nada */

/* ---------- Depuração ---------- */
/* Descomente para forçar a mesma semente em toda partida. Útil para
 * conferir o determinismo: duas partidas com as mesmas entradas devem
 * se comportar igual (mesmos inimigos, mesmos sorteios). */
/* #define DEBUG_FIXED_SEED 1234 */

#endif
