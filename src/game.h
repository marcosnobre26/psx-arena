/*
 * game.h - Tipos e estado global do jogo
 *
 * Mapa dos arquivos:
 *   main.c      laço principal, câmera, telas (título, pausa, fim)
 *   data.c      TABELAS: personagens, armas, poderes, inimigos, skins  <- comece por aqui
 *   levels.c    TABELA DE FASES: mapas (desenhados em texto!), aparência, objetivo
 *   level.c     lê o mapa, gera a geometria e responde à colisão com paredes
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
#include "sound.h"

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
/* Armas (etapa 07): o TIPO decide como a arma age (weapons.c) */
enum { WK_MELEE,     /* golpe em arco à frente, sem projétil, silencioso */
       WK_BOW,       /* segurar carrega; solta a flecha (munição) */
       WK_MAGIC,     /* gasta mana: bola de fogo, clarão, cura */
       WK_GUN };     /* arma de fogo: munição rara, barulhenta, mira auto curta */
enum { AMMO_NONE, AMMO_ARROW, AMMO_BULLET, AMMO_MANA };
enum { MAGIC_NONE, MAGIC_FIRE, MAGIC_FLASH, MAGIC_HEAL };
/* índices em weapon_defs (data.c, mesma ordem) */
enum { W_SWORD, W_AXE, W_BOW, W_FIREBALL, W_FLASH, W_HEAL, W_PISTOL, W_SHOTGUN, NUM_WEAPON_SLOTS };
#define MAGIC_BITS ((1u << W_FIREBALL) | (1u << W_FLASH) | (1u << W_HEAL))
typedef struct {
	const char  *name;
	uint8_t      kind;      /* WK_* */
	uint8_t      ammo;      /* AMMO_* gasto a cada uso */
	uint8_t      magic;     /* MAGIC_* (só WK_MAGIC) */
	uint8_t      chops;     /* 1 = corta copa de árvore caída e raízes (machado) */
	int          cost;      /* munição/mana por uso */
	int          cooldown;  /* passos entre usos */
	int          damage;    /* dano (arco: com carga cheia) */
	int          reach;     /* corpo a corpo: alcance; projétil: passos de vida */
	int          arc;       /* corpo a corpo: meia abertura do golpe (ângulo) */
	int          hit_from, hit_to;   /* corpo a corpo: janela de acerto (passos do golpe) */
	int          speed;     /* projétil: velocidade (arco: com carga cheia) */
	int          pellets;   /* projéteis por disparo */
	int          spread;    /* abertura entre projéteis (ângulo) */
	int          noise;     /* raio do barulho, em células (0 = silencioso) */
	int          scale;     /* tamanho do projétil / do efeito */
	CVECTOR      color;
	const MESH  *mesh;      /* modelo na mão (NULL = nenhum: magias) */
} WEAPON_DEF;

typedef struct {
	const char   *name;
	char          map_char;  /* letra que representa este inimigo no mapa */
	int           hp;
	int           speed;
	int           damage;
	int           scale;     /* ONE = tamanho do modelo exportado */
	int           score;
	const MESH   *mesh;
	CVECTOR       palette[3];/* cores por material do modelo */
	/* percepção (etapa 07) */
	int           sight;     /* visão em células, no escuro (lanterna ligada: x2, até a névoa) */
	int           hearing;   /* % do raio de um barulho que ele ouve */
	uint8_t       flee_light;/* 1 = foge da lanterna apontada para ele */
	uint8_t       heavy;     /* 1 = passos pesados que se ouvem (só som) */
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
	int            weapon2;   /* segunda arma inicial (-1 = nenhuma) */
	int            arrows;    /* flechas iniciais */
	int            weapon_mat;/* material da arma embutida no modelo (some no jogo; -1 = nenhum) */
	SVECTOR        hand;      /* onde a arma fica presa (espaço do modelo, escala ONE) */
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
extern const ENEMY_DEF  enemy_defs[];
extern const int        num_enemy_types;
extern const SKIN_DEF   skin_defs[];
extern const int        num_skins;
extern const CHARACTER_DEF character_defs[];
extern const int        num_characters;
extern const PROP_DEF   prop_defs[];
extern const int        num_props;

/* Ajustes de cada som (data.c). A tabela termina com { NULL }. */
typedef struct {
	SOUND  *sound;      /* sfx_<nome>, gerado de assets/sounds/<nome>.wav */
	int     prio;       /* 1 = fundo (passos) ... 3 = importante (dor, morte) */
	int     pitch_var;  /* variação aleatória de tom em % (0 = sempre igual) */
} SOUND_DEF;
extern const SOUND_DEF  sound_defs[];

/* Tipo de cada célula da grade (level.c). Bit 7 = sólido. */
enum {
	CELL_VOID,      /* nada (sem chão), sólido */
	CELL_DIRT,      /* trilha de terra */
	CELL_LEAVES,    /* chão de folhas */
	CELL_ROOTS,     /* terra com raízes */
	CELL_THICKET,   /* mata densa (3 m), sólida */
	CELL_BARRIER,   /* barreira de raízes (1,5 m), sólida; abre com a chave (etapa 06) */
};
#define CELL_TYPE_MASK  0x3f
#define CELL_LOW        0x40   /* sólido BAIXO (tronco caído): pula-se por cima, não tapa a visão */
#define CELL_SOLID      0x80

/* Árvores e troncos caídos (data.c: tree_defs). Ficam guardados por bloco
 * em level.c (fora de g: vêm do mapa, como as células). */
enum { TREE_PINE, TREE_DEAD, TREE_LOG, TREE_FALLEN, NUM_TREE_TYPES };
typedef struct {
	const MESH    *mesh;
	const TEXTURE *bb;          /* imagem plana de longe (NULL = some de longe) */
	int            bb_w, bb_h;  /* tamanho da imagem no mundo, em escala ONE */
	int            scale_min, scale_max;   /* variação de tamanho (ONE = 1.0) */
	int            cells;       /* 1 = em pé (círculo do tronco); 3 = deitado (3 células baixas) */
	int            trunk_r;     /* em pé: raio do tronco na escala ONE (colisão) */
	int            canopy_r;    /* em pé: raio da copa (câmera: "esta árvore tapa?") */
	int            pitch;       /* != 0: modelo deitado (árvore que caiu; etapa 06) */
} TREE_DEF;
extern const TREE_DEF tree_defs[];

/* Geração procedural (mapgen.c): parâmetros de uma fase gerada */
enum { GEN_HUNT, GEN_CROSS, GEN_COLLECT, GEN_SURVIVE };   /* onde ficam início/saída/... */
enum { GEN_HEALTH, GEN_ENERGY, GEN_BATTERY, GEN_WEAPON, GEN_GEM, GEN_CRATE,
       GEN_ARROWS, GEN_AMMO, GEN_MEDKIT, GEN_ITEM_TYPES };   /* (novos no fim: o gerador sorteia na ordem) */
typedef struct {
	int w, h;                    /* tamanho em células (até 128) */
	int layout;                  /* GEN_* */
	int dens_min, dens_max;      /* árvores por mil células (pelo ruído) */
	int chunk_cap;               /* árvores no máximo por bloco 8x8 */
	int window_cap;              /* "árvores" no máximo numa janela 3x3 blocos
	                                (mata conta 1/4 por célula) */
	int window_trees;            /* árvores no máximo numa janela 3x3 blocos
	                                (as árvores pesam mais que a mata na CPU) */
	int clear_min, clear_max;    /* clareiras sorteadas */
	int clear_rmin, clear_rmax;  /* raio delas */
	int survive_r;               /* GEN_SURVIVE: raio da clareira do cerco */
	int logs;                    /* troncos caídos */
	int enemies;                 /* orçamento de inimigos */
	int enemy_min_dist;          /* distância mínima do início (células) */
	int items[GEN_ITEM_TYPES];   /* bandagem, poção, pilha, arma, gema, caixa, flechas, balas, kit */
	int thicket;                 /* mata alta: ruído >= isto (0..255; 256 = nenhuma) */
	int walls;                   /* paredes ao lado das trilhas: limiar mais baixo
	                                (thicket - walls) colado na faixa da trilha */
} GEN_PARAMS;

typedef struct {
	uint32_t seed;               /* semente usada (base + tentativas - 1) */
	int tries, fallback;         /* tentativas; 1 = usou o plano B */
	int trees, dens_permil, max_chunk;
	int thicket_permil;               /* mata alta no interior (por mil células) */
	int gen_lines, val_lines;         /* perfil: linhas gerando / validando */
	int win_sides;                    /* maior contorno de mata numa janela 3x3 */
	int win_weight, win_bx, win_bz;   /* janela 3x3 mais pesada (peso: árvore = 4, mata = 1/célula) */
	int win_trees, wt_bx, wt_bz;      /* janela 3x3 com MAIS ÁRVORES (o interior) */
} MAPGEN_STATS;

/* Ponto fixo para o teleporte de depuração (célula + direção da câmera) */
typedef struct {
	int cx, cz;
	int yaw;
} DEBUG_POINT;

/* Objetivo de uma fase (ver objective.c) */
enum { OBJ_KILL_ALL, OBJ_REACH_EXIT, OBJ_COLLECT, OBJ_SURVIVE };

/* Estados de um inimigo (enemies.c; etapa 07) */
enum { EN_WANDER,        /* vaga à toa */
       EN_INVESTIGATE,   /* ouviu algo: vai até o barulho */
       EN_CHASE,         /* viu o jogador: persegue */
       EN_SEARCH };      /* perdeu o rastro: vai onde viu por último e desiste */

/* Eventos durante a partida (events.c; etapa 06) */
enum { EV_FOG, EV_TREE_FALL, EV_OPEN_PATH, EV_CLOSE_BEHIND, EV_WAVE, EV_SILENCE,
       NUM_EV_ACTIONS };
#define EV_MAP_ACTIONS ((1 << EV_TREE_FALL) | (1 << EV_OPEN_PATH) | (1 << EV_CLOSE_BEHIND))
enum { EVT_RANDOM,      /* a cada p1..p2 s (g.rng); ação sorteada na máscara arg */
       EVT_TIME,        /* uma vez, p1 s depois do começo da fase */
       EVT_AREA,        /* uma vez, um jogador a p2 células da área p1 (AREA_*) */
       EVT_PICKUP,      /* uma vez, ao pegar o p1-ésimo item de missão */
       EVT_KILLS,       /* uma vez, ao derrotar p1 inimigos */
       EVT_KEY };       /* jogador com chave encosta na barreira de raízes */
enum { AREA_EXIT, AREA_QUEST };
typedef struct {
	uint8_t trigger;     /* EVT_* */
	uint8_t action;      /* EV_* (ignorado em EVT_RANDOM) */
	uint8_t prio;        /* maior sai da fila antes */
	int16_t p1, p2;      /* parâmetros do gatilho */
	int16_t arg;         /* parâmetro da ação (EVT_RANDOM: máscara de ações) */
} EVENT_DEF;

/* Uma fase (levels.c): gerador do mapa + aparência + objetivo */
typedef struct {
	const char        *name;
	uint8_t            sky_r, sky_g, sky_b;   /* cor do céu = cor da névoa */
	int                fog_near, fog_far;     /* névoa: começa / esconde tudo (z) */
	uint8_t            amb_r, amb_g, amb_b;   /* luz ambiente (0..255) */
	int                music;        /* faixa de CD-DA (etapa 01b: ainda não toca) */
	int                objective;    /* OBJ_* */
	int                obj_param;    /* COLLECT: itens (0 = todos os Q); SURVIVE: segundos */
	int                max_enemies;  /* SURVIVE: inimigos vivos no máximo (reforços) */
	const char        *obj_text;     /* frase da introdução (ASCII, sem acentos) */
	void             (*build)(void); /* monta a grade e os objetos (por código) */
	const DEBUG_POINT *debug_pts;    /* 4 pontos de teleporte (L2 + direcional) */
	const GEN_PARAMS  *gen;          /* != NULL: mapa GERADO por g.seed (build = plano B) */
	const EVENT_DEF   *events;       /* eventos da fase (etapa 06) */
	int                num_events;
} LEVEL_DEF;
extern const LEVEL_DEF  level_defs[];
extern const int        num_levels;       /* fases da sequência normal */
extern const int        num_level_defs;   /* todas, inclusive a de teste */

/* ------------------------------------------------------------------ */
/* Entidades                                                           */
/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ */
/* Inventário (etapa 08): itens GUARDADOS, usados quando o jogador quer */
/* ------------------------------------------------------------------ */
/* Só estes ocupam espaço (INV_SLOTS). Relíquias, chaves, gemas, armas,
 * flechas e balas NÃO ocupam: são contadores. A ordem é a de item_defs. */
enum { IT_NONE, IT_BANDAGE, IT_MEDKIT, IT_POTION, IT_BATTERY, NUM_ITEM_TYPES };
typedef struct {
	const char *name;       /* ASCII sem acentos (menu) */
	const char *desc;       /* uma linha curta */
	int         stack;      /* quantos cabem num espaço */
	int         amount;     /* efeito: vida, mana ou pilha */
	int         pick;       /* PICK_* que vira este item (descartar = volta ao chão) */
} ITEM_DEF;
extern const ITEM_DEF item_defs[];
typedef struct { uint8_t type, count; } INV_SLOT;

/* Estatísticas de cada jogador (etapa 08): ficam em g porque o menu mostra
 * no build normal; o registro de partida (playlog) conta o resto. */
#define STAT_ENEMY_TYPES 4
typedef struct {
	uint16_t kills[STAT_ENEMY_TYPES];   /* inimigos derrotados por tipo */
	uint16_t shots, hits;               /* ataques / ataques que acertaram (precisão) */
	uint16_t dmg_taken, items_used;
	int      dist;                      /* unidades andadas (256 = 1 m) */
} PSTATS;

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
	int      arrows, bullets;   /* munição (cada jogador a sua) */
	int      swing;         /* corpo a corpo: passos desde o começo do golpe (0 = parado) */
	uint32_t swing_hit;     /* inimigos já atingidos neste golpe (bit = índice) */
	int      charge;        /* arco: passos segurando (0 = sem carga) */
	int      skin;
	int      character;     /* índice em character_defs */
	int      max_hp;
	int      walk_anim;
	int      regen_timer;
	int      lantern_on;    /* lanterna ligada (SELECT liga/desliga) */
	int      battery;       /* 0..BATTERY_MAX */
	int      battery_tick;  /* passos até gastar mais 1 de bateria */
	int      lantern_dying; /* bateria acabou: passos piscando até apagar */
	INV_SLOT inv[INV_SLOTS];  /* inventário (etapa 08) */
	PSTATS   st;            /* estatísticas da fase atual (menu) */
	PSTATS   total;         /* soma das fases JÁ VENCIDAS (campanha; etapa 10) */
	int      seen_cx, seen_cz;  /* última célula marcada na névoa de exploração */
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
	int     vy;             /* velocidade vertical (pulo por cima de tronco caído) */
	int     stun;           /* passos atordoado (clarão): não anda */
	int     flee;           /* passos fugindo (clarão / luz) */
	int     state;          /* EN_* (percepção, etapa 07) */
	int     tx, tz;         /* para onde vai: barulho ouvido / onde viu o jogador */
	int     timer;          /* passos no estado (perseguir sem ver / investigar) */
} ENEMY;

typedef struct {
	int     active;
	VECTOR  pos;
	int     vx, vz;
	int     life;
	int     damage;
	int     weapon;
	int     owner;          /* índice do jogador que atirou (estatística) */
} BULLET;

enum { PICK_GEM, PICK_HEALTH, PICK_ENERGY, PICK_WEAPON, PICK_QUEST, PICK_BATTERY, PICK_KEY,
       PICK_ARROWS, PICK_AMMO,   /* etapa 07: feixe de flechas (V), balas (M) */
       PICK_MEDKIT,              /* etapa 08: kit médico (J) */
       NUM_PICKUP_TYPES };

typedef struct {
	int     active;
	int     type;
	VECTOR  pos;
	int     lock;           /* etapa 08: largado pelo jogador lock-1; ele só pega de
	                           novo depois de se afastar (0 = qualquer um pega) */
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
/* Gerador aleatório com estado explícito (ver rng.c) */
typedef struct {
	uint32_t s;
} RNG;

enum { STATE_TITLE, STATE_SELECT, STATE_PLAY, STATE_MENU, STATE_WIN, STATE_DEAD,
       STATE_INTRO, STATE_END };   /* novos no fim: não mudam os números antigos */

typedef struct {
	int     state;
	int     state_timer;    /* contador do estado atual (ex.: duração da introdução) */
	int     frame;
	uint32_t seed;          /* semente da partida (no modo link virá do handshake) */
	RNG     rng;            /* único gerador que a LÓGICA pode usar */
	int     play_frames;    /* tempo de jogo (para o placar) */
	int     score;
	int     gems, gems_total;
	int     enemies_left;

	/* fase e objetivo (objective.c) */
	int     level;                /* índice em level_defs */
	int     quest, quest_total;   /* itens de missão (Q) pegos / no mapa */
	int     quest_need;           /* quantos o objetivo pede */
	int     survive_left;         /* SURVIVE: passos que faltam */
	int     reinforce_timer;      /* SURVIVE: passos até o próximo reforço */
	int     has_exit, exit_open;  /* o mapa tem saída (X)? já abriu? */
	int     exit_x, exit_z;
	int     num_spawn_pts;        /* pontos de reforço (S) */
	struct { int x, z; } spawn_pts[MAX_SPAWN_PTS];

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

	int     cam_yaw;        /* ângulo da câmera ao redor dos jogadores (LÓGICA:
	                           o direcional do jogador gira com ele). A posição
	                           da câmera é visual e fica em camera.c */

	char    message[40];    /* mensagem temporária no centro da tela */
	int     message_timer;

	/* combate (etapa 07): barulhos deste passo (os MAX_NOISES mais fortes;
	 * os inimigos leem e a lista é zerada) e flechas caídas no chão */
	struct { int x, z, r; } noise[MAX_NOISES];
	int     nnoise;
	struct { int active, x, z, age; } garrow[MAX_GROUND_ARROWS];
	int     garrow_next;          /* a próxima vaga (a mais antiga some) */
	/* etapa 08: névoa de exploração (1 bit por célula vista; o mapa do menu
	 * só mostra estas) e o menu do START. O menu é LÓGICA (usar um item muda
	 * g), por isso fica em g: a última aba fica; o cursor zera ao fechar. */
	uint8_t seen[MAP_MAX_H][MAP_MAX_W / 8];
	int     menu_tab, menu_cursor, menu_player, menu_owner;
	int     time_total;           /* passos das fases já vencidas (campanha; etapa 10) */

	/* eventos da fase (events.c). Tudo aqui é LÓGICA: só g.in, passos e g.rng */
	struct {
		int      next_random;     /* passos até o próximo evento aleatório */
		uint32_t fired;           /* bit i = EVENT_DEF i já disparou (uma vez) */
		int      map_cooldown;    /* passos até poder mudar o mapa de novo */
		struct { uint8_t action, prio, force; int16_t arg; } queue[EV_QUEUE_LEN];
		int      nq;
		int      kills;           /* inimigos derrotados na fase */
		int      keys;            /* chaves (abre barreira de raízes) */
		int      fog_far;         /* névoa atual (a lógica lê ESTA, não a da tabela) */
		int      fog_timer;       /* passos do avanço da névoa (0 = normal) */
		int      fog_target;      /* névoa mínima do avanço */
		int      silence;         /* passos de silêncio (sons de ambiente parados) */
		int      shake;           /* passos de tremor (a câmera visual só lê) */
		int      last;            /* última ação aplicada (EV_*), para o overlay */
		int      barrier_open;    /* a barreira de raízes já foi aberta */
		int      enemy_cap;       /* onda: inimigos vivos no máximo (os do começo) */
		int      barrier_hp;      /* golpes de machado que a barreira aguenta (etapa 07) */
		/* árvores caídas que o machado pode cortar (as 4 últimas) */
		struct {
			int      active, x, z, hp, n;
			uint16_t cells[REACH_MAX_BLOCK];
		} fallen[4];
		int      fallen_next;
		/* busca de lugar em andamento (árvore cai / mata fecha): no máximo 1
		 * checagem de alcance por passo, para não pesar num quadro só */
		struct { int action, arg, k, j, tries, placed, dir0, start, checking, cand; } job;
		/* árvore caindo: aviso (estalo), tombo, impacto */
		struct {
			int      timer;           /* 0 = nenhuma */
			int      x, z, dir, scale;
			int      n;
			uint16_t cells[REACH_MAX_BLOCK];   /* x | z << 8 */
			uint8_t  crown[REACH_MAX_BLOCK];   /* 1 = copa (sólida); 0 = tronco (baixo) */
		} fall;
	} ev;
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

/* rng.c */
void     rng_seed(RNG *r, uint32_t seed);
uint32_t rng_next(RNG *r);
int      rng_range(RNG *r, int lo, int hi);   /* lo..hi, inclusive */
int      rand_range(int lo, int hi);          /* lógica: usa g.rng */
int      fx_range(int lo, int hi);            /* só efeitos visuais */

/* player.c (lanterna) */
int  lantern_lit(const PLAYER *p);      /* acesa neste quadro? (só para o desenho) */

/* camera.c */
void camera_turn(void);                 /* lógica: gira g.cam_yaw pelos controles */
void camera_view_update(int ticks);     /* visual: segue o grupo (uma vez por quadro) */
void camera_snap(void);                 /* visual: direto no lugar */
void camera_apply(void);                /* envia para o motor 3D */
const VECTOR *camera_eye(void);         /* olho da câmera (som posicional) */
void camera_stats(int *tests, int *cells);   /* custo do quadro anterior (L2/bench) */

/* objective.c */
void objective_start(void);
int  objective_update(void);            /* 1 = fase vencida */
const char *objective_text(void);       /* linha do HUD */
void exit_draw(void);

/* level.c */
void level_load(int index);
void level_reset_objects(void);         /* sem árvores/colocados, colisão vazia */
void *level_scratch(int bytes);         /* rascunho (só dentro do level_load) */

/* mapgen.c */
int  mapgen_build(int index);           /* 1 = gerou; 0 = plano B */
const MAPGEN_STATS *mapgen_stats(void);
const DEBUG_POINT *mapgen_points(int index);   /* teleporte: gerados ou fixos */
const DEBUG_POINT *mapgen_tree_point(void);     /* centro da janela com mais árvores */
const DEBUG_POINT *mapgen_worst_point(void);    /* centro da janela mais pesada */
int mapgen_barrier(int *cx, int *cz, int *r);   /* anel de raízes do mapa (0 = não tem) */
#ifdef DEBUG_BENCHMARK
extern int bench_force_fixed;                   /* bench: carrega a floresta fixa (plano B) */
#endif
int  mapgen_active(void);
void mapgen_mark_fixed(void);
int  level_load_lines(void);            /* tempo do último level_load em linhas (~63,5 us) */
void level_begin(int w, int h);         /* mapa por código: tamanho (tudo vazio) */
void level_set_cell(int cx, int cz, int type);   /* CELL_* (solidez automática) */
int  level_cell_type(int cx, int cz);
int  level_tree_get(int i, int *x, int *z);       /* tipo da árvore i, ou -1 */
int  level_standing_tree_at(int cx, int cz);       /* índice da árvore em pé, ou -1 */
void level_tree_info(int i, int *x, int *z, int *type, int *scale, int *angle);
int  level_chunk_standing(int bx, int bz);
void level_remove_tree(int i);
void level_trees_changed(void);                   /* depois de level_add_tree no jogo */
int  level_tree_room(void);
int  level_cell_raw(int cx, int cz);
const uint8_t (*level_cells(void))[MAP_MAX_W];         /* leitura rápida */
const int16_t (*collide_obst_heads(void))[MAP_MAX_W];  /* leitura rápida */
int  level_chunk_trees(int bx, int bz, int *start);
void level_set_flags(int cx, int cz, int flags);  /* só bits (sólido/baixo): não remonta */
void level_rebuild_chunk(int cx, int cz);         /* remonta em fatias (tipo mudou) */
void level_place(char ch, int cx, int cz);       /* cria o que a letra representa */
int  level_place_safe(char ch, int cx, int cz);  /* idem, na célula livre mais próxima */
int  level_place_failures(int i, char *ch, int *cx, int *cz);   /* teste: falhas */
int  level_placed(int i, char *ch, int *cx, int *cz);           /* teste: colocados */
void level_invalidate(int cx, int cz);  /* célula mudou: remonta o bloco */
void level_stats(int *loaded, int *slots, int *built, int *overflow);
void level_build_stats(int *max_lines);           /* pior montagem de bloco; zera ao ler */
int  level_cell_top(int cx, int cz);    /* altura do topo da célula (0, 384, 768) */
/* Árvore na posição do mundo (x, z). angle 0..4095; scale ONE = 1.0.
 * Marca a(s) célula(s) do tronco como sólida(s). Troncos caídos: use
 * ângulo perto de 0/1024/2048/3072 (a colisão é por célula). */
void level_add_tree(int type, int x, int z, int angle, int scale);
void level_tree_stats(int *total, int *models, int *billboards);
int  level_tree_transparent(void);      /* árvores desenhadas a 50% no último quadro */
int  level_cut_count(void);             /* blocos+árvores cortados por falta de buffer */
/* Câmera: marca as árvores cuja copa tapa algum ponto (x[i], z[i]) visto do
 * olho (ex, ez); ficam semitransparentes por TREE_OCCLUDE_TICKS passos */
void level_mark_occluders(int ex, int ez, const int *x, const int *z, int n, int ticks);
int  level_tree_density(int x, int z, int radius);   /* árvores (+mata) perto: para a câmera */
void level_apply_look(void);            /* névoa e luz da fase atual */
void level_apply_fog(void);             /* só a névoa (segue g.ev.fog_far) */
void level_draw(void);
int  level_cell_solid(int cx, int cz);
int  level_cell_low(int cx, int cz);    /* sólido baixo (tronco caído)? */
int  level_blocked(int x, int z, int radius);
void level_set_solid(int cx, int cz, int solid);
int  level_width(void);
int  level_height(void);

/* player.c */
void player_spawn(PLAYER *p, int x, int z);
void player_update(PLAYER *p, INPUT *in);
void player_draw(PLAYER *p);
void player_damage(PLAYER *p, int amount, int from_x, int from_z);
void player_draw_model(const VECTOR *pos, const SVECTOR *rot, int character, int skin, int hide_weapon);
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
int  collide_solid_at(int x, int z, int radius);       /* célula sólida ou obstáculo ali? */
void collide_reset(void);               /* início do level_load */
void collide_add_obstacle(int x, int z, int r);   /* tronco em pé / prop sólido */
void collide_remove_obstacle(int x, int z);         /* árvore que caiu (etapa 06) */
int  collide_cell_has_obstacle(int cx, int cz);
void collide_track(const void *self);   /* posição mudou direto: atualiza a grade */
ENEMY *collide_enemy_at(int x, int z, int pad);   /* tiro: inimigo atingido (ou NULL) */
uint32_t collide_enemies_near(int x, int z, int r);   /* bits dos inimigos perto (grade) */
int  collide_line_of_sight(int x0, int z0, int x1, int z1);   /* 1 = visão livre */
int  collide_camera_clear(int x, int z, int ex, int ez);      /* câmera vê o jogador? */
int  collide_seg_cells_take(void);      /* células percorridas pela câmera (custo) */
int  collide_check_grid(void);          /* depuração: entidades fora da célula certa */
int  collide_cell_obstacle(int cx, int cz, int k, int *x, int *z, int *r);
void collide_tick_begin(void);          /* depuração: zera o contador do passo */
int  collide_stats(void);               /* testes de círculo no último passo */
int  collide_tests_now(void);           /* testes do passo que acabou de rodar */
int  enemy_radius(const ENEMY *e);

/* enemies.c */
void enemy_spawn(int type, int x, int z);
void enemies_update(void);
void enemies_draw(void);
void enemy_damage(ENEMY *e, int amount, int push_x, int push_z, int attacker);   /* attacker = jogador ou -1 */
ENEMY *enemy_nearest_in_cone(int x, int z, int angle, int cone, int max_dist);

/* weapons.c */
void weapon_update(PLAYER *p, const INPUT *in);   /* ataque, carga, golpe (um passo) */
void weapon_draw(const PLAYER *p);                 /* arma na mão */
void noise_emit(int x, int z, int cells);          /* barulho (raio em células) */
void ground_arrows_update(void);
void ground_arrows_draw(void);
void bullets_update(void);
void bullets_draw(void);

/* items.c */
int  pickup_spawn(int type, int x, int z);      /* vaga + 1 (0 = sem vaga no chão) */

/* inventory.c (etapa 08) */
int  inv_add(PLAYER *p, int type);              /* 1 = guardou uma unidade */
int  inv_count(const PLAYER *p, int type);
int  inv_use(PLAYER *p, int slot);              /* 1 = usou (aplica o efeito) */
int  inv_drop(PLAYER *p, int slot);             /* 1 = largou no chão */
void inv_quick_heal(PLAYER *p);                 /* CÍRCULO: bandagem, depois kit */
void stats_add(PSTATS *dst, const PSTATS *src); /* dst += src (totais da campanha) */
void explore_update(void);                      /* névoa de exploração (todo passo) */
int  explore_seen(int cx, int cz);

/* ambient.c (etapa 09): céu, lua, pássaros e animais. SÓ VISUAL (fx_range,
 * nada em g). update uma vez por quadro, depois da lógica. */
void ambient_update(int ticks);
void ambient_draw_sky(void);                    /* céu e lua (fundo da OT) */
void ambient_draw(void);                        /* pássaros e animais */
void ambient_note_noise(int x, int z, int r);   /* cópia visual de um barulho (noise_emit) */
void ambient_debug(int what);                   /* 0 bando da árvore, 1 olhos, 2 vulto, 3 salto,
                                                   4 galho, 5 lua à frente, 6 bando no céu */
void ambient_stats(int *active, int *polys, int *delayed, int *cpu);

/* menu.c (etapa 08): menu do START */
void menu_open(int who);                        /* who = controle que apertou START */
void menu_update(void);                         /* um passo com o menu aberto (lógica) */
void menu_draw(void);                           /* por cima do mundo (só lê g) */
void menu_draw_map(void);
void pickups_update(void);
void pickups_draw(void);
void crate_spawn(int cx, int cz);
int  crate_damage(int cx, int cz, int amount);   /* 1 = havia caixa */
void crates_draw(void);
void effect_spawn(int type, int x, int z, int radius, int duration, int r, int gr, int b);
void effects_update(void);
void effects_draw(void);
void shadow_draw(const VECTOR *pos, int scale);
void prop_spawn(int def, int cx, int cz);
void props_draw(void);

/* main.c */
void show_message(const char *text, int frames);

/* reach.c: alcance durante a partida (mapa de bits + checagem local) */
enum { REACH_OK, REACH_CUT, REACH_WINDOW, REACH_LIMIT,     /* LIMIT: só nas estatísticas */
       REACH_PENDING };
void reach_clear(void);
void reach_set(int cx, int cz, int on);
int  reach_get(int cx, int cz);
int  reach_walkable(int cx, int cz);
int  reach_local_ok(int cx, int cz, const uint16_t *blk, int n);   /* tudo de uma vez */
int  reach_begin(int cx, int cz, const uint16_t *blk, int n);      /* ... ou em passos */
int  reach_step(int budget);
void reach_stats(int *ok, int *cut, int *window, int *limit);
int  reach_last_visits(void);

/* playlog.c: registro de partida (só com DEBUG_PLAYLOG; etapa 07b). PL(x)
 * some no build normal; os contadores ficam FORA de g (não muda a lógica) */
#ifdef DEBUG_PLAYLOG
void pl_begin(void);
void pl_end(const char *reason);      /* fecha a fase (se ainda aberta) */
void pl_tick(void);
void pl_source(int enemy_type);
void pl_damage(const PLAYER *p, int amount);
void pl_heal(int magic);
void pl_use(const PLAYER *p, int weapon);
void pl_found(const PLAYER *p, int ammo, int amount);
void pl_arrow_picked(const PLAYER *p);
void pl_battery(const PLAYER *p);
void pl_attack(int weapon);
void pl_kill(int enemy_type);
void pl_big_escape(void);
#define PL(call) call
#else
#define PL(call) ((void)0)
#endif

/* robot.c: jogador robô (só com DEBUG_ROBOT; ./dev robo). Escreve g.in[0]
 * lendo g, como um controle; nunca muda g nem usa g.rng */
#ifdef DEBUG_ROBOT
int  robot_match(int k, int *level, uint32_t *seed, int *character);
int  robot_limit_ticks(int level);
void robot_begin(uint32_t salt);
void robot_input(INPUT *in);
void robot_report(const char *reason);
#endif

/* events.c */
void events_start(void);              /* fim do game_reset */
void events_update(void);             /* um passo de lógica (STATE_PLAY) */
void events_debug(int action);        /* atalho de depuração: na hora, sem regras */
void events_enemy_killed(void);
void events_draw(void);               /* árvore caindo (só desenho: lê g) */
int  events_chop(int cx, int cz);     /* golpe de machado na célula: 1 = acertou algo */
void events_prof(int *job, int *impact);   /* só no bench */
int  events_prof_check(void);
int  ev_fall_footprint(int x, int z, int dir, int scale, uint16_t *cells, uint8_t *crown);
const char *event_name(int action);

#endif
