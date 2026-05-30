#include "nexus.h"
#include "config.h"
#include "err.h"
#include <stdlib.h>
#include <string.h>

enum
{
	SGLEO_MAX = 96,
};

typedef int           (*sgcmpfn)(uvlong, uvlong, void *);
typedef struct sgitem sgitem;
typedef struct sgheap sgheap;
typedef struct sgroot sgroot;
typedef struct sgpos  sgpos;

struct sgitem {
	uvlong key;
	uvlong ref;
};

struct sgheap {
	sgitem *items;
	uvlong  n;
	uvlong  cap;
	uvlong  p [2];
	uint    pshift;
	bool    order;
	bool    live;
};

struct sgroot {
	uvlong slot;
	uint   order;
};

struct sgpos {
	uvlong target;
	uvlong path [SGLEO_MAX];
	uint   npath;
	sgroot roots [SGLEO_MAX];
	uint   nroot;
	uint   iroot;
};

static sgheap *sgs;
static int     sgcap;
static uvlong  leo [SGLEO_MAX];
static uint    nleo;

static bool leo_init(void) {
	if (nleo) return true;
	leo [0] = 1;
	leo [1] = 1;
	nleo    = 2;
	while (nleo < SGLEO_MAX) {
		if (leo [nleo - 2] > ( uvlong ) -1 - leo [nleo - 1] || leo [nleo - 2] + leo [nleo - 1] > ( uvlong ) -2) break;
		leo [nleo] = leo [nleo - 2] + leo [nleo - 1] + 1;
		nleo++;
	}
	return true;
}

static bool sg_table_init(void) {
	if (sgs) return true;
	sgcap = COETUA_SEGHEAP_TABLE_SEED > 0 ? COETUA_SEGHEAP_TABLE_SEED : 1;
	sgs   = ( sgheap * ) calloc(( size_t ) sgcap, sizeof(sgheap));
	if (!sgs) {
		errmsg("segheap: out of memory");
		sgcap = 0;
		return false;
	}
	return leo_init();
}

static bool sg_table_grow(void) {
	int  need   = sgcap + 1;
	uint ucap   = nextpow2(( uint ) need);
	int  newcap = ( int ) ucap;
	if (newcap < COETUA_SEGHEAP_TABLE_SEED) newcap = COETUA_SEGHEAP_TABLE_SEED;
	sgheap *p = ( sgheap * ) realloc(sgs, ( size_t ) newcap * sizeof(sgheap));
	if (!p) {
		errmsg("segheap: out of memory");
		return false;
	}
	memset(p + sgcap, 0, ( size_t ) (newcap - sgcap) * sizeof(sgheap));
	sgs   = p;
	sgcap = newcap;
	return true;
}

static sgheap *sg_get(int heap) {
	if (!sg_table_init() || heap < 0 || heap >= sgcap || !sgs [heap].live) return null;
	return &sgs [heap];
}

static bool ensure_cap_for(sgheap *h, uvlong need) {
	if (need <= h->cap) return true;
	uvlong newcap = 0;
	for (uint i = 0; i < nleo; i++) {
		if (leo [i] >= need && leo [i] >= 67) {
			newcap = leo [i];
			break;
		}
	}
	if (!newcap) {
		errmsg("segheap: item capacity overflow");
		return false;
	}
	if (newcap > ( uvlong ) (SIZE_MAX / sizeof(sgitem))) {
		errmsg("segheap: item capacity overflow");
		return false;
	}
	sgitem *p = ( sgitem * ) realloc(h->items, ( size_t ) newcap * sizeof(sgitem));
	if (!p) {
		errmsg("segheap: out of memory");
		return false;
	}
	h->items = p;
	h->cap   = newcap;
	return true;
}

static bool ensure_cap(sgheap *h) { return ensure_cap_for(h, h->n + 1); }

static int ntz128(uvlong p [2]) {
	int r = ctz64(p [0] - 1);
	if (r != 0 || (r = 64 + ctz64(p [1])) != 64) return r;
	return 0;
}

static void shl128(uvlong p [2], int n) {
	u128 x = u128_shl(u128_make(p [0], p [1]), n);
	p [0] = x.lo;
	p [1] = x.hi;
}

static void shr128(uvlong p [2], int n) {
	u128 x = u128_shr(u128_make(p [0], p [1]), n);
	p [0] = x.lo;
	p [1] = x.hi;
}

static int priocmp(sgheap *h, sgitem a, sgitem b, sgcmpfn cmp, void *arg) {
	int r = cmp(a.key, b.key, arg);
	return h->order ? -r : r;
}

static int keycmp(sgheap *h, uvlong a, uvlong b, sgcmpfn cmp, void *arg) {
	int r = cmp(a, b, arg);
	return h->order ? -r : r;
}

static void sift(sgheap *h, uvlong head, uint pshift, sgcmpfn cmp, void *arg) {
	sgitem x = h->items [head];
	while (pshift > 1) {
		uvlong rt = head - 1;
		uvlong lf = head - 1 - leo [pshift - 2];
		if (priocmp(h, x, h->items [lf], cmp, arg) >= 0 && priocmp(h, x, h->items [rt], cmp, arg) >= 0) break;
		if (priocmp(h, h->items [lf], h->items [rt], cmp, arg) >= 0) {
			h->items [head] = h->items [lf];
			head            = lf;
			pshift--;
		}
		else {
			h->items [head] = h->items [rt];
			head            = rt;
			pshift -= 2;
		}
	}
	h->items [head] = x;
}

static void trinkle(sgheap *h, uvlong head, uvlong pp [2], uint pshift, bool trusty, sgcmpfn cmp, void *arg) {
	uvlong path [SGLEO_MAX];
	uvlong p [2] = {pp [0], pp [1]};
	uint   npath = 1;
	sgitem x     = h->items [head];
	path [0]      = head;
	while (p [0] != 1 || p [1] != 0) {
		uvlong stepson = head - leo [pshift];
		if (priocmp(h, h->items [stepson], x, cmp, arg) <= 0) break;
		if (!trusty && pshift > 1) {
			uvlong rt = head - 1;
			uvlong lf = head - 1 - leo [pshift - 2];
			if (priocmp(h, h->items [rt], h->items [stepson], cmp, arg) >= 0
			    || priocmp(h, h->items [lf], h->items [stepson], cmp, arg) >= 0)
				break;
		}
		path [npath++] = stepson;
		head           = stepson;
		int trail      = ntz128(p);
		shr128(p, trail);
		pshift += ( uint ) trail;
		trusty = false;
	}
	if (trusty) return;
	for (uint i = 0; i + 1 < npath; i++) h->items [path [i]] = h->items [path [i + 1]];
	h->items [path [npath - 1]] = x;
	sift(h, head, pshift, cmp, arg);
}

static void append_step(sgheap *h, uvlong head, uvlong high, sgcmpfn cmp, void *arg) {
	if ((h->p [0] & 3) == 3) {
		sift(h, head, h->pshift, cmp, arg);
		shr128(h->p, 2);
		h->pshift += 2;
	}
	else {
		if (leo [h->pshift - 1] >= high - head) trinkle(h, head, h->p, h->pshift, false, cmp, arg);
		else sift(h, head, h->pshift, cmp, arg);
		if (h->pshift == 1) {
			shl128(h->p, 1);
			h->pshift = 0;
		}
		else {
			shl128(h->p, ( int ) h->pshift - 1);
			h->pshift = 1;
		}
	}
	h->p [0] |= 1;
}

static bool append_heap(sgheap *h, sgcmpfn cmp, void *arg) {
	if (h->n == 1) {
		h->p [0] = 1;
		h->p [1] = 0;
		h->pshift = 1;
		return true;
	}
	if (h->n > leo [nleo - 1]) {
		errmsg("segheap: too many items");
		return false;
	}
	append_step(h, h->n - 2, h->n - 1, cmp, arg);
	trinkle(h, h->n - 1, h->p, h->pshift, false, cmp, arg);
	return true;
}

static bool append_range_heap(sgheap *h, uvlong oldn, sgcmpfn cmp, void *arg) {
	if (h->n == oldn) return true;
	if (h->n > leo [nleo - 1]) {
		errmsg("segheap: too many items");
		return false;
	}
	if (!oldn) {
		h->p [0] = 1;
		h->p [1] = 0;
		h->pshift = 1;
		oldn = 1;
	}
	for (uvlong head = oldn - 1; head + 1 < h->n; head++) append_step(h, head, h->n - 1, cmp, arg);
	trinkle(h, h->n - 1, h->p, h->pshift, false, cmp, arg);
	return true;
}

static void pop_root(sgheap *h, sgcmpfn cmp, void *arg) {
	if (h->n <= 1) {
		h->n = 0;
		h->p [0] = 0;
		h->p [1] = 0;
		h->pshift = 0;
		return;
	}
	uvlong head = h->n - 1;
	if (h->pshift <= 1) {
		int trail = ntz128(h->p);
		shr128(h->p, trail);
		h->pshift += ( uint ) trail;
	}
	else {
		shl128(h->p, 2);
		h->pshift -= 2;
		h->p [0] ^= 7;
		shr128(h->p, 1);
		trinkle(h, head - leo [h->pshift] - 1, h->p, h->pshift + 1, true, cmp, arg);
		shl128(h->p, 1);
		h->p [0] |= 1;
		trinkle(h, head - 1, h->p, h->pshift, true, cmp, arg);
	}
	h->n--;
	if (!h->n) {
		h->p [0] = 0;
		h->p [1] = 0;
		h->pshift = 0;
	}
}

static bool enum_roots(sgheap *h, sgroot *roots, uint *nroot) {
	*nroot = 0;
	if (!h->n) return true;
	uvlong p [2] = {h->p [0], h->p [1]};
	uint   ps = h->pshift;
	uvlong root = h->n - 1;
	sgroot tmp [SGLEO_MAX];
	uint   n = 0;
	for (;;) {
		if (n >= SGLEO_MAX) return false;
		tmp [n++] = (sgroot) {.slot = root, .order = ps};
		if (p [0] == 1 && p [1] == 0) break;
		root -= leo [ps];
		int trail = ntz128(p);
		shr128(p, trail);
		ps += ( uint ) trail;
	}
	for (uint i = 0; i < n; i++) roots [i] = tmp [n - 1 - i];
	*nroot = n;
	return true;
}

static bool find_in_tree(sgheap *h, uvlong root, uint order, uvlong chet, sgcmpfn cmp, void *arg, sgpos *pos, uint depth) {
	sgitem r = h->items [root];
	if (keycmp(h, r.key, chet, cmp, arg) < 0) return false;
	if (depth >= SGLEO_MAX) return false;
	pos->path [depth++] = root;
	if (cmp(r.key, chet, arg) == 0) {
		pos->target = root;
		pos->npath  = depth;
		return true;
	}
	if (order <= 1) return false;
	uvlong rt = root - 1;
	uvlong lf = root - 1 - leo [order - 2];
	return find_in_tree(h, rt, order - 2, chet, cmp, arg, pos, depth) || find_in_tree(h, lf, order - 1, chet, cmp, arg, pos, depth);
}

static bool find_pos(sgheap *h, uvlong chet, sgcmpfn cmp, void *arg, sgpos *pos) {
	*pos = (sgpos) {0};
	if (!h->n) return false;
	if (!enum_roots(h, pos->roots, &pos->nroot)) return false;
	for (uint i = pos->nroot; i > 0; i--) {
		uint j = i - 1;
		if (find_in_tree(h, pos->roots [j].slot, pos->roots [j].order, chet, cmp, arg, pos, 0)) {
			pos->iroot = j;
			return true;
		}
	}
	return false;
}

static bool badsoa(uvlong *keys, uvlong *refs, uvlong cap) { return cap && !keys && !refs; }

int mksegheap(int arena, bool order) {
	( void ) arena;
	if (!sg_table_init()) return -1;
	for (;;) {
		for (int i = 0; i < sgcap; i++) {
			if (sgs [i].live) continue;
			sgs [i] = (sgheap) {.order = order, .live = true};
			return i;
		}
		if (!sg_table_grow()) return -1;
	}
}

void rmsegheap(int heap) {
	sgheap *h = (!sgs || heap < 0 || heap >= sgcap || !sgs [heap].live) ? null : &sgs [heap];
	if (!h) return;
	free(h->items);
	*h = (sgheap) {0};
}

void sgput(int heap, uvlong key, uvlong ref, sgcmpfn cmp, void *arg) {
	sgheap *h = sg_get(heap);
	if (!h || !cmp) {
		errmsg("sgput: bad argument");
		return;
	}
	if (!ensure_cap(h)) return;
	h->items [h->n++] = (sgitem) {.key = key, .ref = ref};
	if (!append_heap(h, cmp, arg)) h->n--;
}

void sgputs(int heap, uvlong *keys, uvlong *refs, uvlong n, sgcmpfn cmp, void *arg) {
	sgheap *h = sg_get(heap);
	if (!h || !cmp || (n && (!keys || !refs))) {
		errmsg("sgputs: bad argument");
		return;
	}
	if (!n) return;
	uvlong oldn = h->n;
	uvlong newn = h->n + n;
	if (!ensure_cap_for(h, newn)) return;
	for (uvlong i = 0; i < n; i++) h->items [oldn + i] = (sgitem) {.key = keys [i], .ref = refs [i]};
	h->n = newn;
	if (!append_range_heap(h, oldn, cmp, arg)) h->n = oldn;
}

bool sgtop(int heap, uvlong *key, uvlong *ref) {
	sgheap *h = sg_get(heap);
	if (!h) {
		errmsg("sgtop: bad descriptor");
		return false;
	}
	if (!h->n) return false;
	sgitem *it = &h->items [h->n - 1];
	if (key) *key = it->key;
	if (ref) *ref = it->ref;
	return true;
}

bool sgpop(int heap, uvlong *key, uvlong *ref, sgcmpfn cmp, void *arg) {
	sgheap *h = sg_get(heap);
	if (!h || !cmp) {
		errmsg("sgpop: bad argument");
		return false;
	}
	if (!h->n) return false;
	sgitem it = h->items [h->n - 1];
	if (key) *key = it.key;
	if (ref) *ref = it.ref;
	pop_root(h, cmp, arg);
	return true;
}

bool sgfind(int heap, uvlong chet, sgcmpfn cmp, void *arg, uvlong *key, uvlong *ref) {
	sgheap *h = sg_get(heap);
	if (!h || !cmp) {
		errmsg("sgfind: bad argument");
		return false;
	}
	sgpos pos;
	if (!find_pos(h, chet, cmp, arg, &pos)) return false;
	if (key) *key = h->items [pos.target].key;
	if (ref) *ref = h->items [pos.target].ref;
	return true;
}

static bool delete_pos(sgheap *h, sgpos *pos, sgcmpfn cmp, void *arg) {
	for (uint i = pos->npath; i > 1; i--) h->items [pos->path [i - 1]] = h->items [pos->path [i - 2]];
	for (uint i = pos->iroot; i + 1 < pos->nroot; i++) h->items [pos->roots [i].slot] = h->items [pos->roots [i + 1].slot];
	pop_root(h, cmp, arg);
	return true;
}

bool sgdel(int heap, uvlong chet, sgcmpfn cmp, void *arg) {
	sgheap *h = sg_get(heap);
	if (!h || !cmp) {
		errmsg("sgdel: bad argument");
		return false;
	}
	sgpos pos;
	if (!find_pos(h, chet, cmp, arg, &pos)) return false;
	return delete_pos(h, &pos, cmp, arg);
}

uvlong sgdels(int heap, uvlong chet, uvlong n, sgcmpfn cmp, void *arg) {
	sgheap *h = sg_get(heap);
	if (!h || !cmp) {
		errmsg("sgdels: bad argument");
		return 0;
	}
	uvlong ndel = 0;
	while (ndel < n) {
		sgpos pos;
		if (!find_pos(h, chet, cmp, arg, &pos)) break;
		if (!delete_pos(h, &pos, cmp, arg)) break;
		ndel++;
	}
	return ndel;
}

uvlong sgnitem(int heap) {
	sgheap *h = sg_get(heap);
	if (!h) {
		errmsg("sgnitem: bad descriptor");
		return 0;
	}
	return h->n;
}

uvlong sgitems(int heap, uvlong *keys, uvlong *refs, uvlong cap) {
	sgheap *h = sg_get(heap);
	if (!h || badsoa(keys, refs, cap)) {
		errmsg("sgitems: bad argument");
		return 0;
	}
	uvlong n = h->n;
	uvlong m = cap < n ? cap : n;
	for (uvlong i = 0; i < m; i++) {
		if (keys) keys [i] = h->items [i].key;
		if (refs) refs [i] = h->items [i].ref;
	}
	return n;
}
