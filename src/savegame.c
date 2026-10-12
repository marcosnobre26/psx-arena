/*
 * savegame.c - Memory card: salvar e carregar o progresso (etapa 10)
 *
 * O QUE SE SALVA: o ponto de partida da fase atual (fase, semente,
 * personagens e tudo o que o jogador levou para ela: vida, mana, armas,
 * munição, pilha, inventário, pontos e os totais da campanha) e o volume
 * dos efeitos. Ao carregar, o jogo REGERA o mesmo mapa pela semente e a
 * fase recomeça exatamente como começou (como no game over). Salvar no
 * meio da fase exigiria gravar o mapa mudado pelos eventos: não cabe.
 * Quem monta e aplica os dados é o main.c (progress/sel são dele); aqui
 * fica só o formato e o acesso ao cartão.
 *
 * FORMATO: um arquivo de 1 bloco (8 KB) no cartão da porta 1 ("bu00:"):
 *   0..127   cabeçalho do BIOS: "SC", ícone de 1 quadro, título em
 *            Shift-JIS (aparece no gerenciador de memory card), paleta
 *   128..255 ícone 16x16 (4 bits)
 *   256..    os nossos dados: SAVE_HEAD (marca "MATA", versão, tamanho,
 *            checksum) + o conteúdo
 * Versão diferente ou checksum errado = SAVE INVALIDO: o jogo avisa e
 * ignora (nada é aplicado; nunca trava).
 *
 * O acesso usa as funções de arquivo do BIOS (open/read/write), que
 * esperam o cartão terminar: leva alguns quadros. Por isso só acontece no
 * menu, na troca de fase e no título, nunca no meio da ação.
 */
#include <psxapi.h>
#include <string.h>
#include "game.h"

#define SAVE_FILE   "bu00:BASLUS-90000MATAESC"
#define BLOCK       8192
#define DATA_OFS    256

static uint8_t blk[BLOCK];                    /* um bloco inteiro (8 KB) */

typedef struct {
	char     magic[4];                        /* "MATA" */
	uint16_t version;                         /* SAVE_VERSION */
	uint16_t len;                             /* bytes do conteúdo */
	uint32_t sum;                             /* checksum do conteúdo */
} SAVE_HEAD;

/* Checksum simples e suficiente para pegar dados corrompidos (FNV-1a) */
static uint32_t checksum(const uint8_t *p, int n) {
	uint32_t h = 2166136261u;
	for (int i = 0; i < n; i++) h = (h ^ p[i]) * 16777619u;
	return h;
}

/* Título em Shift-JIS: letras/dígitos "largos" (o BIOS mostra assim) */
static void sjis_title(uint8_t *dst, const char *s, int max_bytes) {
	int k = 0;
	for (; *s && k + 2 <= max_bytes; s++) {
		uint16_t c = 0x8140;                  /* espaço */
		if (*s >= 'A' && *s <= 'Z') c = (uint16_t)(0x8260 + (*s - 'A'));
		else if (*s >= '0' && *s <= '9') c = (uint16_t)(0x824F + (*s - '0'));
		dst[k++] = (uint8_t)(c >> 8);
		dst[k++] = (uint8_t)c;
	}
}

/* Cabeçalho + ícone (uma lanterna/lua clara num fundo escuro) */
static void build_header(void) {
	memset(blk, 0, DATA_OFS);
	blk[0] = 'S'; blk[1] = 'C';
	blk[2] = 0x11;                            /* ícone estático (1 quadro) */
	blk[3] = 1;                               /* blocos usados */
	sjis_title(&blk[4], "MATA ESCURA", 64);
	/* paleta (16 cores, 15 bits) em 0x60: 0 = fundo, 1 = mata, 2 = lua */
	uint16_t *pal = (uint16_t *)&blk[0x60];
	pal[0] = 0x0842; pal[1] = 0x0D04; pal[2] = 0x6F7B;
	uint8_t *ic = &blk[128];                  /* 16x16, 2 pixels por byte */
	for (int y = 0; y < 16; y++)
		for (int x = 0; x < 16; x++) {
			int dx = x - 10, dy = y - 5, c = 0;
			if (dx * dx + dy * dy <= 9) c = 2;              /* lua */
			if (y >= 9 + ((x * 5) & 3)) c = 1;             /* copas das árvores */
			ic[y * 8 + x / 2] |= (uint8_t)(c << ((x & 1) * 4));
		}
}

static int card_ready;

void card_init(void) {
	_bu_init();                               /* o driver de arquivos do cartão */
	card_ready = 1;
}

/* Grava o conteúdo (len bytes). 0 = ok, < 0 = erro (sem cartão, cheio...) */
int card_save(const void *data, int len) {
	if (!card_ready || len + (int)sizeof(SAVE_HEAD) > BLOCK - DATA_OFS) return -1;
	build_header();
	memset(blk + DATA_OFS, 0, BLOCK - DATA_OFS);
	SAVE_HEAD *h = (SAVE_HEAD *)(blk + DATA_OFS);
	memcpy(h->magic, "MATA", 4);
	h->version = SAVE_VERSION;
	h->len = (uint16_t)len;
	memcpy(blk + DATA_OFS + sizeof(SAVE_HEAD), data, len);
	h->sum = checksum(blk + DATA_OFS + sizeof(SAVE_HEAD), len);

	int fd = open(SAVE_FILE, 1 | 2);          /* já existe? */
	if (fd < 0) {
		fd = open(SAVE_FILE, 0x200 | (1 << 16));   /* cria com 1 bloco */
		if (fd < 0) return -2;
		close(fd);
		fd = open(SAVE_FILE, 2);
		if (fd < 0) return -3;
	}
	int n = write(fd, blk, BLOCK);
	close(fd);
	return n == BLOCK ? 0 : -4;
}

/* Lê para data (até len bytes). 1 = ok; 0 = não há save; -1 = SAVE INVALIDO
 * (versão diferente, tamanho errado ou checksum que não bate) */
int card_load(void *data, int len) {
	if (!card_ready) return 0;
	int fd = open(SAVE_FILE, 1);
	if (fd < 0) return 0;
	int n = read(fd, blk, BLOCK);
	close(fd);
	if (n != BLOCK) return -1;
	const SAVE_HEAD *h = (const SAVE_HEAD *)(blk + DATA_OFS);
	if (memcmp(h->magic, "MATA", 4) || h->version != SAVE_VERSION || h->len != len)
		return -1;
	const uint8_t *body = blk + DATA_OFS + sizeof(SAVE_HEAD);
	if (checksum(body, len) != h->sum)
		return -1;
	memcpy(data, body, len);
	return 1;
}

#if defined(DEBUG_ROBOT) && defined(ROBOT_SAVETEST)
/* Só no ./dev save-test: estraga um byte do conteúdo gravado (checksum) */
int card_corrupt_for_test(void) {
	int fd = open(SAVE_FILE, 1);
	if (fd < 0) return -1;
	int n = read(fd, blk, BLOCK);
	close(fd);
	if (n != BLOCK) return -1;
	blk[DATA_OFS + sizeof(SAVE_HEAD) + 5] ^= 0x5A;
	fd = open(SAVE_FILE, 2);
	if (fd < 0) return -1;
	n = write(fd, blk, BLOCK);
	close(fd);
	return n == BLOCK ? 0 : -1;
}

/* ... apaga o save (cartão "vazio") */
int card_erase_for_test(void) { return erase(SAVE_FILE) ? 0 : -1; }

/* ... e grava com outra versão (save de uma versão antiga do jogo) */
int card_old_version_for_test(const void *data, int len) {
	int r = card_save(data, len);
	if (r) return r;
	int fd = open(SAVE_FILE, 1);
	read(fd, blk, BLOCK);
	close(fd);
	((SAVE_HEAD *)(blk + DATA_OFS))->version = SAVE_VERSION + 1;
	fd = open(SAVE_FILE, 2);
	int n = write(fd, blk, BLOCK);
	close(fd);
	return n == BLOCK ? 0 : -1;
}
#endif
