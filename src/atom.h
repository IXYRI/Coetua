#pragma once
#include <stddef.h>
#include <limits.h>

/* ═══════════════════════════════════════════════════════
   atom — portable types, bit operations, u128 helpers
   All functions are static inline; no link-time dependency.
   ═══════════════════════════════════════════════════════ */

/* ── Portable integer types ─────────────────────────── */
#if LONG_MAX != INT_MAX
  #define vlong long
#else
  #define vlong long long
#endif

#define uchar  unsigned char
#define ushort unsigned short
#define uint   unsigned int
#define ulong  unsigned long
#define uvlong unsigned vlong
#define rune   uint
#define null   (( void * ) 0)

/* ── Array slice ───────────────────────────────────── */
typedef struct arrst {
	uvlong len;
	void  *x;
} arrst;

static arrst inline mkarrst(uvlong len, void *x) { return ( arrst ) {len, x}; }

#define arrlen(arr)  (sizeof(arr) / sizeof((arr) [0]))
#define toarrst(arr) mkarrst(arrlen(arr), (arr))
#define umarrst(a)   (a).len, (a).x

/* ── Bool for C17 ──────────────────────────────────── */
#if __STDC_VERSION__ < 202311l
  #ifndef true
	#define bool  _Bool
	#define true  1
	#define false 0
  #endif
#endif

/* ── 128-bit unsigned (portable struct) ────────────── */
typedef struct {
	uvlong lo, hi;
} u128;

/* ── Count leading zeros ───────────────────────────── */
static int inline clz32(uint x) {
	if (!x) return 32;
	int n = 0;
	if (!(x & 0xffff0000)) n += 16, x <<= 16;
	if (!(x & 0xff000000)) n += 8, x <<= 8;
	if (!(x & 0xf0000000)) n += 4, x <<= 4;
	if (!(x & 0xc0000000)) n += 2, x <<= 2;
	if (!(x & 0x80000000)) n += 1;
	return n;
}

static int inline clz64(uvlong x) {
	if (!x) return 64;
	int n = 0;
	if (!(x & 0xffffffff00000000ull)) n += 32, x <<= 32;
	if (!(x & 0xffff000000000000ull)) n += 16, x <<= 16;
	if (!(x & 0xff00000000000000ull)) n += 8, x <<= 8;
	if (!(x & 0xf000000000000000ull)) n += 4, x <<= 4;
	if (!(x & 0xc000000000000000ull)) n += 2, x <<= 2;
	if (!(x & 0x8000000000000000ull)) n += 1;
	return n;
}

/* ── Count trailing zeros ──────────────────────────── */
static int inline ctz32(uint x) {
	if (!x) return 32;
	int n = 0;
	if (!(x & 0x0000ffff)) n += 16, x >>= 16;
	if (!(x & 0x000000ff)) n += 8, x >>= 8;
	if (!(x & 0x0000000f)) n += 4, x >>= 4;
	if (!(x & 0x00000003)) n += 2, x >>= 2;
	if (!(x & 0x00000001)) n += 1;
	return n;
}

static int inline ctz64(uvlong x) {
	if (!x) return 64;
	int n = 0;
	if (!(x & 0x00000000ffffffffull)) n += 32, x >>= 32;
	if (!(x & 0x000000000000ffffull)) n += 16, x >>= 16;
	if (!(x & 0x00000000000000ffull)) n += 8, x >>= 8;
	if (!(x & 0x000000000000000full)) n += 4, x >>= 4;
	if (!(x & 0x0000000000000003ull)) n += 2, x >>= 2;
	if (!(x & 0x0000000000000001ull)) n += 1;
	return n;
}

/* ── Population count ──────────────────────────────── */
static int inline popcnt32(uint x) {
	x = x - ((x >> 1) & 0x55555555u);
	x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
	return ( int ) (((x + (x >> 4)) & 0x0f0f0f0fu) * 0x01010101u) >> 24;
}

static int inline popcnt64(uvlong x) {
	x = x - ((x >> 1) & 0x5555555555555555ull);
	x = (x & 0x3333333333333333ull) + ((x >> 2) & 0x3333333333333333ull);
	return ( int ) ((((x + (x >> 4)) & 0x0f0f0f0f0f0f0f0full) * 0x0101010101010101ull) >> 56);
}

/* ── Byte swap ─────────────────────────────────────── */
static uint inline bswap32(uint x) {
	return (x >> 24) | ((x >> 8) & 0x0000ff00u) | ((x << 8) & 0x00ff0000u) | (x << 24);
}

static uvlong inline bswap64(uvlong x) {
	return (x >> 56)
	     | ((x >> 40) & 0x000000000000ff00ull)
	     | ((x >> 24) & 0x0000000000ff0000ull)
	     | ((x >> 8) & 0x00000000ff000000ull)
	     | ((x << 8) & 0x000000ff00000000ull)
	     | ((x << 24) & 0x0000ff0000000000ull)
	     | ((x << 40) & 0x00ff000000000000ull)
	     | (x << 56);
}

/* ── Rotate ────────────────────────────────────────── */
static uint inline rotl32(uint x, int k) {
	k &= 31;
	return (x << k) | (x >> (32 - k));
}

static uint inline rotr32(uint x, int k) {
	k &= 31;
	return (x >> k) | (x << (32 - k));
}

static uvlong inline rotl64(uvlong x, int k) {
	k &= 63;
	return (x << k) | (x >> (64 - k));
}

static uvlong inline rotr64(uvlong x, int k) {
	k &= 63;
	return (x >> k) | (x << (64 - k));
}

/* ── Power-of-2 / alignment ────────────────────────── */
static bool inline ispow2(uint x) { return x && !(x & (x - 1)); }

static uint inline nextpow2(uint x) { return x <= 1 ? 1 : x > 0x80000000u ? UINT_MAX : 1u << (32 - clz32(x - 1)); }

static uint inline alignup(uint x, uint a) { return (x + a - 1) & ~(a - 1); }

static uint inline aligndown(uint x, uint a) { return x & ~(a - 1); }

static bool inline ispow2_64(uvlong x) { return x && !(x & (x - 1)); }

static uvlong inline nextpow2_64(uvlong x) {
	return x <= 1 ? 1 : x > (( uvlong ) 1 << 63) ? ( uvlong ) -1 : ( uvlong ) 1 << (64 - clz64(x - 1));
}

static uvlong inline alignup_64(uvlong x, uvlong a) { return (x + a - 1) & ~(a - 1); }

static uvlong inline aligndown_64(uvlong x, uvlong a) { return x & ~(a - 1); }

/* ── Checked uvlong arithmetic ─────────────────────── */
static bool inline addok64(uvlong a, uvlong b, uvlong *out) {
	if (a > ( uvlong ) -1 - b) return false;
	*out = a + b;
	return true;
}

static bool inline mulok64(uvlong a, uvlong b, uvlong *out) {
	if (a != 0 && b > ( uvlong ) -1 / a) return false;
	*out = a * b;
	return true;
}

/* ── 128-bit helpers ───────────────────────────────── */
static u128 inline u128_make(uvlong lo, uvlong hi) { return ( u128 ) {lo, hi}; }

/* 64×64 → 128 widening multiply */
static u128 inline wmul64(uvlong a, uvlong b) {
	uint   ah = ( uint ) (a >> 32), al = ( uint ) a;
	uint   bh = ( uint ) (b >> 32), bl = ( uint ) b;
	uvlong lo  = ( uvlong ) al * bl;
	uvlong mid = ( uvlong ) ah * bl + ( uvlong ) al * bh;
	uvlong hi  = ( uvlong ) ah * bh;
	return ( u128 ) {lo + (mid << 32), hi + (mid >> 32)};
}

static u128 inline u128_add(u128 a, u128 b) {
	uvlong lo = a.lo + b.lo;
	return ( u128 ) {lo, a.hi + b.hi + (lo < a.lo)};
}
