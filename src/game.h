/*
 * game.h - Tipos e estado global do jogo
 *
 * Mapa dos arquivos:
 *   main.c      laço principal, câmera, telas (título, pausa, fim)
 *   data.c      TABELAS: personagens, armas, poderes, inimigos, skins  <- comece por aqui
 *   level.c     mapa da fase (desenhado em texto!) e colisão com paredes
 *   player.c    controle do jogador
 *   enemies.c   inteligência e dano dos inimigos
 *   weapons.c   tiros
 *   powers.c    poderes especiais (onda de choque, dash, cura)
 *   items.c     itens coletáveis, caixas destrutíveis e efeitos visuais
 *   input.c     leitura do controle
 *   render.c    motor 3D (GTE/GPU)
 *   (gerado)    build/gen/assets_gen.h: modelos (models/) e texturas (assets/)
 */
#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <psxgte.h>
#include <psxgpu.h>
#include "config.h"
#include "mesh.h"
#include "render.h"

/* ------------------------------------------------------------------ */
/* Controle                                                            */
/* ------------------------------------------------------------------ */
typedef struct {
	uint16_t held;      /* botões pressionados agora (bits PAD_*) */
	uint16_t pressed;   /* botões que acabaram de ser apertados */
	int      lx, ly;    /* analógico esquerdo  (-128..127, 0 = centro) */
	int      rx, ry;    /* analógico direito */
	int      connected;
} INPUT;

void input_init(void);
void input_update(INPUT in[MAX_PLAYERS]);   /* lê as duas portas */

/* ------------------------------------------------------------------ */
/* Definições (tabelas em data.c)                                     */
/* ------------------------------------------------------------------ */
typedef struct {
	const char *name;
	int      cooldown;  /* quadros entre tiros */
	int      speed;     /* velocidade do projétil */
	int      damage;
	int      pellets;   /* projéteis por disparo */
	int      spread;    /* abertura entre projéteis (ângulo) */
	int      jitter;    /* imprecisão aleatória (ângulo) */
	int      life;      /* quadros até o projétil sumir */
	int      scale;     /* tamanho do projétil (ONE = normal) */
	CVECTOR  color;
} WEAPON_DEF;

struct PLAYER_S;   /* declarado mais abaixo */

typedef struct {
	const char *name;
	int      cost;           /* energia gasta */
	void   (*use)(struct PLAYER_S *p);  /* função do poder (powers.c) */
} POWER_DEF;

typedef struct {
	const char   *name;
	char          map_char;  /* letra que representa este inimigo no mapa */
	int           hp;
	int           speed;
	int           damage;
	int           scale;     /* ONE = tamanho do modelo exportado */
	int           score;
	int           weapon;    /* índice em enemy_weapon_defs; -1 = só corpo a corpo */
	int           range;     /* atiradores param a esta distância e atiram */
	const MESH   *mesh;
	CVECTOR       palette[3];/* cores por material do modelo */
} ENEMY_DEF;

typedef struct {
	const char *name;
	CVECTOR     colors[5];   /* corpo, cabeça, membros, visor, arma */
} SKIN_DEF;

/* Personagem jogável (tela de seleção) */
typedef struct {
	const char    *name;
	const char    *desc;      /* uma linha de descrição (sem acentos) */
	const MESH    *mesh;
	int            scale;     /* ONE = tamanho exportado */
	int            use_skins; /* 1 = cores trocáveis pelas skins (5 materiais) */
	const TEXTURE *tex;       /* textura do modelo (ou NULL) */
	int            max_hp;
	int            speed;     /* velocidade de corrida */
	int            jump;      /* força do pulo */
	int            weapon;    /* arma inicial (índice em weapon_defs) */
} CHARACTER_DEF;

/* Objeto de cenário: um modelo colocado no mapa por um dígito (1..9) */
typedef struct {
	char           map_char;  /* dígito usado no mapa */
	const MESH    *mesh;
	int            scale;     /* ONE = tamanho exportado */
	int            solid;     /* 1 = bloqueia a passagem (e os tiros) */
	const TEXTURE *tex;       /* textura, se o modelo tiver UV (ou NULL) */
} PROP_DEF;

extern const WEAPON_DEF weapon_defs[];
extern const int        num_weapons;
extern const WEAPON_DEF enemy_weapon_defs[];   /* armas que só inimigos usam */
extern const int        num_enemy_weapons;
extern const POWER_DEF  power_defs[];
extern const int        num_powers;
extern const ENEMY_DEF  enemy_defs[];
extern const int        num_enemy_types;
extern const SKIN_DEF   skin_defs[];
extern const int        num_skins;
extern const CHARACTER_DEF character_defs[];
extern const int        num_characters;
extern const PROP_DEF   prop_defs[];
extern const int        num_props;

/* ------------------------------------------------------------------ */
/* Entidades                                                           */
/* ------------------------------------------------------------------ */
typedef struct PLAYER_S {
	int      index;         /* 0 = jogador 1, 1 = jogador 2 */
	int      active;        /* está participando da partida */
	int      respawn_timer; /* caído: quadros até voltar (modo 2 jogadores) */
	VECTOR   pos;           /* pos.vy <= 0 (Y negativo é para cima) */
	int      vy;            /* velocidade vertical */
	int      angle;         /* direção em que olha (0..4095) */
	int      on_ground;
	int      hp, energy;
	int      invuln;        /* quadros restantes de invulnerabilidade */
	int      weapon;        /* índice em weapon_defs */
	uint32_t weapons_owned; /* bit N = possui a arma N */
	int      cooldown;
	int      power;         /* índice em power_defs */
	int      skin;
	int      character;     /* índice em character_defs */
	int      max_hp;
	int      dash_timer;
	int      walk_anim;
	int      regen_timer;
} PLAYER;

typedef struct {
	int     active;
	int     type;           /* índice em enemy_defs */
	int     hp;
	VECTOR  pos;
	int     angle;
	int     flash;          /* quadros piscando após levar dano */
	int     kx, kz;         /* empurrão (knockback) */
	int     think;          /* contador para mudar de direção ao vagar */
	int     hit_cooldown;
	int     shot_timer;     /* atiradores: quadros até o próximo tiro */
	int     windup;         /* atiradores: >0 = parado, piscando, prestes a atirar */
	int     sees;           /* atiradores: tem linha de visão até o alvo */
} ENEMY;

/* Quem disparou o tiro: decide em quem ele acerta e de qual tabela vem a arma */
enum { OWNER_PLAYER, OWNER_ENEMY };

typedef struct {
	int     active;
	int     owner;          /* OWNER_PLAYER ou OWNER_ENEMY */
	VECTOR  pos;
	int     vx, vz;
	int     life;
	int     damage;
	int     weapon;         /* índice em weapon_defs ou enemy_weapon_defs (veja owner) */
} BULLET;

enum { PICK_GEM, PICK_HEALTH, PICK_ENERGY, PICK_WEAPON, NUM_PICKUP_TYPES };

typedef struct {
	int     active;
	int     type;
	VECTOR  pos;
} PICKUP;

typedef struct {
	int     active;
	int     hp;
	int     cx, cz;         /* célula do mapa */
	int     flash;
} CRATE;

typedef struct {
	int     active;
	int     def;            /* índice em prop_defs */
	int     cx, cz;
	int     angle;
	int     radius;         /* raio de colisão (calculado do modelo) */
} PROP;

enum { FX_SHOCKWAVE, FX_BURST, FX_HEAL };

typedef struct {
	int     active;
	int     type;
	VECTOR  pos;
	int     timer, duration;
	int     radius;         /* raio final */
	CVECTOR color;
} EFFECT;

/* ------------------------------------------------------------------ */
/* Estado global                                                       */
/* ------------------------------------------------------------------ */
enum { STATE_TITLE, STATE_SELECT, STATE_PLAY, STATE_PAUSE, STATE_WIN, STATE_DEAD };

typedef struct {
	int     state;
	int     frame;
	int     play_frames;    /* tempo de jogo (para o placar) */
	int     score;
	int     gems, gems_total;
	int     enemies_left;

	INPUT   in[MAX_PLAYERS];      /* controle de cada porta */
	PLAYER  players[MAX_PLAYERS];
	int     num_players;          /* 1 ou 2 */
	int     spawn_x, spawn_z;     /* posição do 'P' no mapa */
	ENEMY   enemies[MAX_ENEMIES];
	BULLET  bullets[MAX_BULLETS];
	PICKUP  pickups[MAX_PICKUPS];
	CRATE   crates[MAX_CRATES];
	EFFECT  effects[MAX_EFFECTS];
	PROP    props[MAX_PROPS];

	int     cam_yaw;        /* ângulo da câmera ao redor dos jogadores */
	VECTOR  cam_pos;        /* posição suavizada da câmera */

	char    message[40];    /* mensagem temporária no centro da tela */
	int     message_timer;
} GAME;

extern GAME g;

/* Modelos e texturas: gerados automaticamente a partir das pastas
 * models/ e assets/ (veja CMakeLists.txt) */
#include "assets_gen.h"

/* ------------------------------------------------------------------ */
/* Funções                                                             */
/* ------------------------------------------------------------------ */
/* mathutil.c */
int  angle_of(int dx, int dz);          /* direção do vetor (0..4095) */
int  angle_diff(int from, int to);      /* diferença em -2048..2047 */
int  turn_towards(int angle, int target, int rate);
int  dist2d(int dx, int dz);            /* distância aproximada */
int  rand_range(int lo, int hi);

/* level.c */
void level_load(int index);
void level_draw(void);
int  level_cell_solid(int cx, int cz);
int  level_blocked(int x, int z, int radius);
void level_set_solid(int cx, int cz, int solid);
int  level_width(void);
int  level_height(void);

/* player.c */
void player_spawn(PLAYER *p, int x, int z);
void player_update(PLAYER *p, INPUT *in);
void player_draw(PLAYER *p);
void player_damage(PLAYER *p, int amount, int from_x, int from_z);
void player_draw_model(const VECTOR *pos, const SVECTOR *rot, int character, int skin);
int  player_alive(const PLAYER *p);
int  players_alive(void);
PLAYER *player_nearest(int x, int z, int *dist_out);   /* vivo mais próximo */

/* collision.c — tudo que ocupa espaço no chão é um círculo */
#define COL_PLAYERS  1
#define COL_ENEMIES  2
#define COL_ALL      (COL_PLAYERS | COL_ENEMIES)
int  collide_blocked(int x, int z, int y, int radius, const void *self, int mask,
                     int from_x, int from_z);
int  collide_move(VECTOR *pos, int dx, int dz, int radius, const void *self, int mask);
int  collide_prop_at(int x, int z, int radius);        /* objeto sólido ali? */
int  enemy_radius(const ENEMY *e);

/* enemies.c */
void enemy_spawn(int type, int x, int z);
void enemies_update(void);
void enemies_draw(void);
void enemy_damage(ENEMY *e, int amount, int push_x, int push_z);
ENEMY *enemy_nearest_in_cone(int x, int z, int angle, int cone, int max_dist);

/* weapons.c */
void weapon_fire(PLAYER *p);
void enemy_fire(ENEMY *e, int angle);
void bullets_update(void);
void bullets_draw(void);

/* powers.c */
void power_use(PLAYER *p);

/* items.c */
void pickup_spawn(int type, int x, int z);
void pickups_update(void);
void pickups_draw(void);
void crate_spawn(int cx, int cz);
void crate_damage(int cx, int cz, int amount);
void crates_draw(void);
void effect_spawn(int type, int x, int z, int radius, int duration, int r, int gr, int b);
void effects_update(void);
void effects_draw(void);
void shadow_draw(const VECTOR *pos, int scale);
void prop_spawn(int def, int cx, int cz);
void props_draw(void);

/* main.c */
void show_message(const char *text, int frames);

#endif
