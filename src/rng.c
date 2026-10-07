/*
 * rng.c - Gerador de números aleatórios determinístico (xorshift32)
 *
 * Por que não usar rand() da libc? Porque o estado dele é escondido e
 * global: qualquer código (até um efeito visual) que chame rand() muda a
 * sequência da lógica. Para o modo link (dois consoles rodando a mesma
 * partida em lockstep) os dois lados precisam sortear EXATAMENTE os mesmos
 * números, na mesma ordem. Com estado explícito fica fácil garantir isso:
 *
 *   g.rng   -> lógica do jogo (dentro de GAME, semente g.seed)
 *   fx_rng  -> efeitos puramente visuais (fora de g; pode variar à vontade)
 *
 * xorshift32 (Marsaglia, 2003): três deslocamentos e XORs, período 2^32-1.
 * Usa só operações de 32 bits, então não puxa nada da libgcc.
 */
#include "game.h"

static RNG fx_rng = { 0x9E3779B9u };   /* gerador dos efeitos visuais */

void rng_seed(RNG *r, uint32_t seed) {
	/* o estado 0 é um ponto fixo do xorshift (0 ^ 0 = 0 para sempre),
	 * então trocamos por uma constante qualquer diferente de zero */
	r->s = seed ? seed : 0x2545F491u;
}

uint32_t rng_next(RNG *r) {
	uint32_t x = r->s;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	r->s = x;
	return x;
}

/* inteiro em [lo, hi] (inclusive). O módulo tem um viés minúsculo para
 * intervalos pequenos como os do jogo; não vale o custo de corrigir. */
int rng_range(RNG *r, int lo, int hi) {
	return lo + (int)(rng_next(r) % (uint32_t)(hi - lo + 1));
}

/* Atalho para a lógica: sorteia com o gerador da partida (g.rng) */
int rand_range(int lo, int hi) {
	return rng_range(&g.rng, lo, hi);
}

/* Para efeitos visuais: NÃO use na lógica (não é sincronizado no modo link) */
int fx_range(int lo, int hi) {
	return rng_range(&fx_rng, lo, hi);
}
