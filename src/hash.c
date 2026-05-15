#include "hash.h"
#include <string.h>

/* ═══════════════════════════════════════════════════════
   SipHash-2-4
   ═══════════════════════════════════════════════════════ */

#define SIPROUND(v0, v1, v2, v3) \
	do {                         \
		v0 += v1;                \
		v1  = rotl64(v1, 13);    \
		v1 ^= v0;                \
		v0  = rotl64(v0, 32);    \
		v2 += v3;                \
		v3  = rotl64(v3, 16);    \
		v3 ^= v2;                \
		v0 += v3;                \
		v3  = rotl64(v3, 21);    \
		v3 ^= v0;                \
		v2 += v1;                \
		v1  = rotl64(v1, 17);    \
		v1 ^= v2;                \
		v2  = rotl64(v2, 32);    \
	}                            \
	while (0)

uvlong siphash(void *data, uvlong len) {
	uvlong k0 = 0x0706050403020100ull, k1 = 0x0f0e0d0c0b0a0908ull;
	uvlong v0   = k0 ^ 0x736f6d6570736575ull;
	uvlong v1   = k1 ^ 0x646f72616e646f6dull;
	uvlong v2   = k0 ^ 0x6c7967656e657261ull;
	uvlong v3   = k1 ^ 0x7465646279746573ull;

	uchar *in   = ( uchar * ) data;
	uvlong left = len & 7;
	uchar *end  = in + len - left;

	for (; in != end; in += 8) {
		uvlong m;
		memcpy(&m, in, 8);
		v3 ^= m;
		SIPROUND(v0, v1, v2, v3);
		SIPROUND(v0, v1, v2, v3);
		v0 ^= m;
	}

	uvlong b = (( uvlong ) len) << 56;
	switch (left) {
	case 7 : b |= (( uvlong ) in [6]) << 48;
	case 6 : b |= (( uvlong ) in [5]) << 40;
	case 5 : b |= (( uvlong ) in [4]) << 32;
	case 4 : b |= (( uvlong ) in [3]) << 24;
	case 3 : b |= (( uvlong ) in [2]) << 16;
	case 2 : b |= (( uvlong ) in [1]) << 8;
	case 1 : b |= (( uvlong ) in [0]); break;
	case 0 : break;
	}

	v3 ^= b;
	SIPROUND(v0, v1, v2, v3);
	SIPROUND(v0, v1, v2, v3);
	v0 ^= b;
	v2 ^= 0xff;
	SIPROUND(v0, v1, v2, v3);
	SIPROUND(v0, v1, v2, v3);
	SIPROUND(v0, v1, v2, v3);
	SIPROUND(v0, v1, v2, v3);

	return v0 ^ v1 ^ v2 ^ v3;
}

/* ═══════════════════════════════════════════════════════
   XXHash64
   ═══════════════════════════════════════════════════════ */

static uvlong PRIME64_1 = 0x9e3779b185ebca87ull;
static uvlong PRIME64_2 = 0xc2b2ae3d27d4eb4full;
static uvlong PRIME64_3 = 0x165667b19e3779f9ull;
static uvlong PRIME64_4 = 0x85ebca77c2b2ae63ull;
static uvlong PRIME64_5 = 0x27d4eb2f165667c5ull;

static uvlong inline xx64_round(uvlong acc, uvlong input) {
	acc += input * PRIME64_2;
	acc  = rotl64(acc, 31);
	acc *= PRIME64_1;
	return acc;
}

static uvlong inline xx64_merge(uvlong acc, uvlong val) {
	acc ^= xx64_round(0, val);
	acc  = acc * PRIME64_1 + PRIME64_4;
	return acc;
}

uvlong xxhash64(void *data, uvlong len) {
	uchar *p    = ( uchar * ) data;
	uchar *bEnd = p + len;
	uvlong h64;

	if (len >= 32) {
		uchar *limit = bEnd - 32;
		uvlong v1    = PRIME64_1 + PRIME64_2;
		uvlong v2    = PRIME64_2;
		uvlong v3    = 0;
		uvlong v4    = -( vlong ) PRIME64_1;

		do {
			uvlong m1, m2, m3, m4;
			memcpy(&m1, p, 8);
			memcpy(&m2, p + 8, 8);
			memcpy(&m3, p + 16, 8);
			memcpy(&m4, p + 24, 8);
			v1  = xx64_round(v1, m1);
			v2  = xx64_round(v2, m2);
			v3  = xx64_round(v3, m3);
			v4  = xx64_round(v4, m4);
			p  += 32;
		}
		while (p <= limit);

		h64 = rotl64(v1, 1) + rotl64(v2, 7) + rotl64(v3, 12) + rotl64(v4, 18);
		h64 = xx64_merge(h64, v1);
		h64 = xx64_merge(h64, v2);
		h64 = xx64_merge(h64, v3);
		h64 = xx64_merge(h64, v4);
	}
	else { h64 = PRIME64_5; }

	h64 += ( uvlong ) len;

	while (p + 8 <= bEnd) {
		uvlong k1;
		memcpy(&k1, p, 8);
		h64 ^= xx64_round(0, k1);
		h64  = rotl64(h64, 27) * PRIME64_1 + PRIME64_4;
		p   += 8;
	}

	if (p + 4 <= bEnd) {
		uint k1;
		memcpy(&k1, p, 4);
		h64 ^= ( uvlong ) k1 * PRIME64_1;
		h64  = rotl64(h64, 23) * PRIME64_2 + PRIME64_3;
		p   += 4;
	}

	while (p < bEnd) {
		h64 ^= (*p) * PRIME64_5;
		h64  = rotl64(h64, 11) * PRIME64_1;
		p++;
	}

	h64 ^= h64 >> 33;
	h64 *= PRIME64_2;
	h64 ^= h64 >> 29;
	h64 *= PRIME64_3;
	h64 ^= h64 >> 32;

	return h64;
}
