#include "silo_priv.h"
#include <string.h>

static htab_t *mset_get(int ms) { return htab_get(ms); }

static uchar  *mset_key(htab_t *t, uvlong idx, uint *len) {
	*len = t->klens [idx];
	return t->keys + t->koffs [idx];
}

static bool mset_next_key(htab_t *t, uvlong *pos, uchar **key, uint *len) {
	while (*pos < t->cap && !htab_slot_live(t->meta [*pos])) (*pos)++;
	if (*pos >= t->cap) return false;
	*key = mset_key(t, *pos, len);
	(*pos)++;
	return true;
}

static void mset_set_count(int ms, void *data, uvlong len, uvlong cnt) {
	if (cnt == 0) oblit(ms, data, len);
	else insert(ms, data, len, &cnt, sizeof(cnt));
}

static void mset_add_count(int dst, void *data, uvlong len, uvlong add) {
	mset_set_count(dst, data, len, cntms(dst, data, len) + add);
}

static uvlong mset_min(uvlong a, uvlong b) { return a < b ? a : b; }

static uvlong mset_sub(uvlong a, uvlong b) { return a > b ? a - b : 0; }

static uvlong mset_absdiff(uvlong a, uvlong b) { return a > b ? a - b : b - a; }

int           mkmultiset(int arena) {
	int id = htab_new_desc();
	if (id < 0) return -1;
	htab_init(&htabs [id], arena, true);
	htabs [id].ismultiset = true;
	return make_desc(id, silo_multiset);
}

uvlong cntms(int ms, void *data, uvlong len) {
	if (!mset_get(ms)) return 0;
	uvlong cnt  = 0;
	uvlong clen = sizeof(cnt);
	if (!lookup(ms, data, len, &cnt, &clen) || clen != sizeof(cnt)) return 0;
	return cnt;
}

bool memms(int ms, void *data, uvlong len) { return cntms(ms, data, len) > 0; }

void addms(int ms, void *data, uvlong len) {
	if (!mset_get(ms)) return;
	uvlong cnt = cntms(ms, data, len) + 1;
	mset_set_count(ms, data, len, cnt);
}

void delms(int ms, void *data, uvlong len) {
	if (!mset_get(ms)) return;
	uvlong cnt = cntms(ms, data, len);
	if (cnt == 0) return;
	if (cnt == 1) {
		oblit(ms, data, len);
		return;
	}
	mset_set_count(ms, data, len, cnt - 1);
}

void prgms(int ms, void *data, uvlong len) {
	if (!mset_get(ms)) return;
	oblit(ms, data, len);
}

void addtums(int dst, int src) {
	htab_t *d = mset_get(dst);
	htab_t *s = mset_get(src);
	if (!d || !s || d == s) return;
	uvlong pos = 0;
	uint   klen;
	uchar *key;
	while (mset_next_key(s, &pos, &key, &klen)) {
		uvlong add = 0, clen = sizeof(add);
		lookup(src, key, klen, &add, &clen);
		mset_add_count(dst, key, klen, add);
	}
}

void unionms(int dst, int src) {
	htab_t *d = mset_get(dst);
	htab_t *s = mset_get(src);
	if (!d || !s || d == s) return;
	uvlong pos = 0;
	uint   klen;
	uchar *key;
	while (mset_next_key(s, &pos, &key, &klen)) {
		uvlong sc = cntms(src, key, klen);
		uvlong dc = cntms(dst, key, klen);
		if (sc > dc) mset_set_count(dst, key, klen, sc);
	}
}

void intxnms(int dst, int src) {
	htab_t *d = mset_get(dst);
	htab_t *s = mset_get(src);
	if (!d || !s) return;
	if (d == s) return;
	for (uvlong i = 0; i < d->cap; i++) {
		if (!htab_slot_live(d->meta [i])) continue;
		uint   klen;
		uchar *key = mset_key(d, i, &klen);
		uvlong dc  = cntms(dst, key, klen);
		uvlong sc  = cntms(src, key, klen);
		mset_set_count(dst, key, klen, mset_min(dc, sc));
	}
}

void diffms(int dst, int src) {
	htab_t *d = mset_get(dst);
	htab_t *s = mset_get(src);
	if (!d || !s) return;
	if (d == s) {
		teem(dst);
		return;
	}
	uvlong pos = 0;
	uint   klen;
	uchar *key;
	while (mset_next_key(s, &pos, &key, &klen)) {
		uvlong dc = cntms(dst, key, klen);
		uvlong sc = cntms(src, key, klen);
		mset_set_count(dst, key, klen, mset_sub(dc, sc));
	}
}

void symmdiffms(int dst, int src) {
	htab_t *d = mset_get(dst);
	htab_t *s = mset_get(src);
	if (!d || !s) return;
	if (d == s) {
		teem(dst);
		return;
	}
	int tmp = mkmultiset(d->arena);
	if (tmp < 0) return;
	for (uvlong i = 0; i < d->cap; i++) {
		if (!htab_slot_live(d->meta [i])) continue;
		uint   klen;
		uchar *key = mset_key(d, i, &klen);
		uvlong dc  = cntms(dst, key, klen);
		uvlong sc  = cntms(src, key, klen);
		mset_set_count(tmp, key, klen, mset_absdiff(dc, sc));
	}
	uvlong pos = 0;
	uint   klen;
	uchar *key;
	while (mset_next_key(s, &pos, &key, &klen))
		if (cntms(dst, key, klen) == 0) mset_set_count(tmp, key, klen, cntms(src, key, klen));
	teem(dst);
	addtums(dst, tmp);
	rmmultiset(tmp);
}

bool submultisets(int a, int b) {
	htab_t *ma = mset_get(a);
	htab_t *mb = mset_get(b);
	if (!ma || !mb) return false;
	if (ma == mb) return true;
	for (uvlong i = 0; i < ma->cap; i++) {
		if (!htab_slot_live(ma->meta [i])) continue;
		uint   klen;
		uchar *key = mset_key(ma, i, &klen);
		if (cntms(a, key, klen) > cntms(b, key, klen)) return false;
	}
	return true;
}

bool simsubmss(int a, int b, int deviat, int excess) {
	htab_t *ma = mset_get(a);
	htab_t *mb = mset_get(b);
	if (!ma || !mb) return false;
	uvlong total = 0;
	uvlong pos   = 0;
	uint   klen;
	uchar *key;
	while (mset_next_key(ma, &pos, &key, &klen)) {
		uvlong ac = cntms(a, key, klen);
		uvlong bc = cntms(b, key, klen);
		if (ac <= bc) continue;
		uvlong over = ac - bc;
		if (deviat >= 0 && over > ( uvlong ) deviat) return false;
		total += over;
		if (excess >= 0 && total > ( uvlong ) excess) return false;
	}
	return true;
}

int cartprodms(int arena, int a, int b) {
	htab_t *ma = mset_get(a);
	htab_t *mb = mset_get(b);
	if (!ma || !mb) return -1;
	int prod = mkmultiset(arena);
	if (prod < 0) return -1;
	for (uvlong i = 0; i < ma->cap; i++) {
		if (!htab_slot_live(ma->meta [i])) continue;
		uint   alen;
		uchar *ak = mset_key(ma, i, &alen);
		uvlong ac = cntms(a, ak, alen);
		for (uvlong j = 0; j < mb->cap; j++) {
			if (!htab_slot_live(mb->meta [j])) continue;
			uint   blen;
			uchar *bk         = mset_key(mb, j, &blen);
			uvlong fields [4] = {alen, ( uvlong ) ak, blen, ( uvlong ) bk};
			uvlong count      = ac * cntms(b, bk, blen);
			mset_set_count(prod, fields, sizeof(fields), count);
		}
	}
	return prod;
}

void rmmultiset(int ms) {
	htab_t *t = mset_get(ms);
	if (t) htab_clear(t);
}
