/*
 * mathutil.c - Matemática em inteiros (o PS1 não tem ponto flutuante)
 */
#include <stdlib.h>
#include "game.h"

/* atan2 aproximado. Devolve o ângulo "a" tal que isin(a) ~ dx e icos(a) ~ dz,
 * ou seja: 0 = olhando para +Z, 1024 = olhando para +X. */
int angle_of(int dx, int dz) {
	int ax = abs(dx), az = abs(dz), t;
	if (ax == 0 && az == 0)
		return 0;
	if (ax <= az) {
		int r = (ax << 12) / az;                     /* 0..4096 */
		t = (512 * r >> 12) + (((178 * r) >> 12) * (4096 - r) >> 12);
	} else {
		int r = (az << 12) / ax;
		t = 1024 - ((512 * r >> 12) + (((178 * r) >> 12) * (4096 - r) >> 12));
	}
	if (dx >= 0 && dz >= 0) return t;
	if (dx >= 0)            return 2048 - t;
	if (dz < 0)             return 2048 + t;
	return (4096 - t) & 4095;
}

int angle_diff(int from, int to) {
	return ((to - from + 2048) & 4095) - 2048;
}

int turn_towards(int angle, int target, int rate) {
	int d = angle_diff(angle, target);
	if (d > rate)  d = rate;
	if (d < -rate) d = -rate;
	return (angle + d) & 4095;
}

/* Distância aproximada (erro de poucos %), sem raiz quadrada */
int dist2d(int dx, int dz) {
	int ax = abs(dx), az = abs(dz);
	int mx = ax > az ? ax : az, mn = ax > az ? az : ax;
	return mx + (mn * 3 >> 3);
}

int rand_range(int lo, int hi) {
	return lo + (rand() % (hi - lo + 1));
}
