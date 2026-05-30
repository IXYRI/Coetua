#include "nexus.h"
#include "config.h"
#include "err.h"
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SCNONE (( uvlong ) -1)

#if LONG_MAX != INT_MAX
  #define SC_VLONG_MIN LONG_MIN
  #define SC_VLONG_MAX LONG_MAX
#else
  #define SC_VLONG_MIN LLONG_MIN
  #define SC_VLONG_MAX LLONG_MAX
#endif

enum
{
	SCFAN       = 64,
	SCBUCKETCAP = 64,
	SCLEVELS    = 32,
};

typedef struct scbranch scbranch;
typedef struct scbucket scbucket;
typedef struct scchild  scchild;
typedef struct scpoint  scpoint;
typedef struct space    space;

typedef struct scbounds {
	double xmin, xmax, ymin, ymax, zmin, zmax;
	bool   any;
} scbounds;

typedef struct scboxq {
	double xmin, xmax, ymin, ymax, zmin, zmax;
} scboxq;

typedef struct scsphrq {
	double x, y, z, r2;
} scsphrq;

typedef struct scoord {
	double x, y, z;
} scoord;

struct scchild {
	bool branch;
	union
	{
		scbranch *br;
		scbucket *bk;
	};
};

struct scbranch {
	uvlong   map;
	scchild *kids;
	scbounds bounds;
};

struct scbucket {
	uvlong  *pts;
	uvlong   n;
	uvlong   cap;
	scbounds bounds;
};

struct scpoint {
	double    x, y, z;
	uvlong    phi;
	vlong     tx, ty, tz;
	uvlong    ux, uy, uz;
	scbucket *bucket;
	uvlong    slot;
	bool      live;
};

struct space {
	double   block;
	scchild  root;
	scpoint *pts;
	uvlong   npt;
	uvlong   nlpt;
	uvlong   cappt;
	bool     live;
};

static space *scs;
static int    sccap;

static bool table_init(void) {
	if (scs) return true;
	sccap = COETUA_SPACE_TABLE_SEED > 0 ? COETUA_SPACE_TABLE_SEED : 1;
	scs   = ( space * ) calloc(( size_t ) sccap, sizeof(space));
	if (!scs) {
		errmsg("space: out of memory");
		sccap = 0;
		return false;
	}
	return true;
}

static bool table_grow(void) {
	int  need   = sccap + 1;
	uint ucap   = nextpow2(( uint ) need);
	int  newcap = ( int ) ucap;
	if (newcap < COETUA_SPACE_TABLE_SEED) newcap = COETUA_SPACE_TABLE_SEED;
	space *p = ( space * ) realloc(scs, ( size_t ) newcap * sizeof(space));
	if (!p) {
		errmsg("space: out of memory");
		return false;
	}
	memset(p + sccap, 0, ( size_t ) (newcap - sccap) * sizeof(space));
	scs   = p;
	sccap = newcap;
	return true;
}

static space *sc_lookup(int id) {
	if (!scs || id < 0 || id >= sccap || !scs [id].live) return null;
	return &scs [id];
}

static space *sc_get(int id) {
	if (!table_init()) return null;
	return sc_lookup(id);
}

static bool valid_block(double block) { return isfinite(block) && block > 0.0; }

static bool valid_coord(double x) { return !isnan(x); }

static double canon(double x) { return x == 0.0 ? 0.0 : x; }

static bool same_coord(double a, double b) { return canon(a) == canon(b); }

static bool pclive(space *sc, uvlong pt) { return sc && pt < sc->npt && sc->pts [pt].live; }

static bool badsoa(uvlong *ids, double *xs, double *ys, double *zs, uvlong *phis, uvlong cap) {
	return cap && !ids && !xs && !ys && !zs && !phis;
}

static bool ensure_pt_cap(space *sc) {
	if (sc->npt < sc->cappt) return true;
	uvlong newcap = nextpow2_64(sc->npt + 1);
	if (newcap < 16) newcap = 16;
	if (newcap > ( uvlong ) (SIZE_MAX / sizeof(scpoint))) {
		errmsg("space: point capacity overflow");
		return false;
	}
	scpoint *p = ( scpoint * ) realloc(sc->pts, ( size_t ) newcap * sizeof(scpoint));
	if (!p) {
		errmsg("space: out of memory");
		return false;
	}
	memset(p + sc->cappt, 0, ( size_t ) (newcap - sc->cappt) * sizeof(scpoint));
	sc->pts   = p;
	sc->cappt = newcap;
	return true;
}

static uint rank64(uvlong map, uint bit) {
	return ( uint ) popcnt64(map & ((( uvlong ) 1 << bit) - 1));
}

static uvlong bias(vlong x) { return (( uvlong ) x) ^ ((( uvlong ) 1) << 63); }

static void tile_of(double x, double block, vlong *tx, uvlong *ux) {
	if (isinf(x)) {
		*tx = x < 0 ? SC_VLONG_MIN : SC_VLONG_MAX;
		*ux = bias(*tx);
		return;
	}
	double q = floor(x / block);
	if (q <= ( double ) SC_VLONG_MIN) *tx = SC_VLONG_MIN;
	else if (q >= ( double ) SC_VLONG_MAX) *tx = SC_VLONG_MAX;
	else *tx = ( vlong ) q;
	*ux = bias(*tx);
}

static uvlong tile_key(double x, double block) {
	vlong tx;
	uvlong ux;
	tile_of(x, block, &tx, &ux);
	return ux;
}

static void set_point_coords(space *sc, scpoint *p, double x, double y, double z) {
	x = canon(x);
	y = canon(y);
	z = canon(z);
	p->x = x;
	p->y = y;
	p->z = z;
	tile_of(x, sc->block, &p->tx, &p->ux);
	tile_of(y, sc->block, &p->ty, &p->uy);
	tile_of(z, sc->block, &p->tz, &p->uz);
}

static uint axisfrag(uvlong u, uint level) {
	uint shift = 62 - level * 2;
	return ( uint ) ((u >> shift) & 3u);
}

static uint ptfrag(space *sc, uvlong pt, uint level) {
	scpoint *p = &sc->pts [pt];
	return axisfrag(p->ux, level) | (axisfrag(p->uy, level) << 2) | (axisfrag(p->uz, level) << 4);
}

static uint keyfrag(uvlong ux, uvlong uy, uvlong uz, uint level) {
	return axisfrag(ux, level) | (axisfrag(uy, level) << 2) | (axisfrag(uz, level) << 4);
}

static scbucket *find_bucket(scchild c, uvlong ux, uvlong uy, uvlong uz, uint level) {
	while (c.branch) {
		scbranch *br = c.br;
		uint f = keyfrag(ux, uy, uz, level++);
		uvlong bit = ( uvlong ) 1 << f;
		if (!(br->map & bit)) return null;
		c = br->kids [rank64(br->map, f)];
	}
	return c.bk;
}

static scbranch *new_branch(void) {
	scbranch *b = ( scbranch * ) calloc(1, sizeof(scbranch));
	if (!b) errmsg("space: out of memory");
	return b;
}

static scbucket *new_bucket(void) {
	scbucket *b = ( scbucket * ) calloc(1, sizeof(scbucket));
	if (!b) errmsg("space: out of memory");
	return b;
}

static void free_child(scchild c) {
	if (c.branch) {
		scbranch *b = c.br;
		if (!b) return;
		for (uint i = 0; i < ( uint ) popcnt64(b->map); i++) free_child(b->kids [i]);
		free(b->kids);
		free(b);
	}
	else if (c.bk) {
		free(c.bk->pts);
		free(c.bk);
	}
}

static bool ensure_bucket_cap(scbucket *b) {
	if (b->n < b->cap) return true;
	uvlong newcap = nextpow2_64(b->n + 1);
	if (newcap < 16) newcap = 16;
	if (newcap > ( uvlong ) (SIZE_MAX / sizeof(uvlong))) {
		errmsg("space: bucket capacity overflow");
		return false;
	}
	uvlong *p = ( uvlong * ) realloc(b->pts, ( size_t ) newcap * sizeof(uvlong));
	if (!p) {
		errmsg("space: out of memory");
		return false;
	}
	b->pts = p;
	b->cap = newcap;
	return true;
}

static void update_bucket(space *sc, scbucket *b) {
	for (uvlong i = 0; b && i < b->n; i++) {
		uvlong pt = b->pts [i];
		sc->pts [pt].bucket = b;
		sc->pts [pt].slot   = i;
	}
}

static bool bucket_append(space *sc, scbucket *b, uvlong pt) {
	if (!ensure_bucket_cap(b)) return false;
	b->pts [b->n++] = pt;
	update_bucket(sc, b);
	return true;
}

static scbounds empty_bounds(void) { return (scbounds) {0}; }

static void bounds_add_point(scbounds *b, scpoint *p) {
	if (!b->any) {
		*b = (scbounds) {.xmin = p->x, .xmax = p->x, .ymin = p->y, .ymax = p->y, .zmin = p->z, .zmax = p->z, .any = true};
		return;
	}
	if (p->x < b->xmin) b->xmin = p->x;
	if (p->x > b->xmax) b->xmax = p->x;
	if (p->y < b->ymin) b->ymin = p->y;
	if (p->y > b->ymax) b->ymax = p->y;
	if (p->z < b->zmin) b->zmin = p->z;
	if (p->z > b->zmax) b->zmax = p->z;
}

static void bounds_add_bounds(scbounds *b, scbounds c) {
	if (!c.any) return;
	if (!b->any) {
		*b = c;
		return;
	}
	if (c.xmin < b->xmin) b->xmin = c.xmin;
	if (c.xmax > b->xmax) b->xmax = c.xmax;
	if (c.ymin < b->ymin) b->ymin = c.ymin;
	if (c.ymax > b->ymax) b->ymax = c.ymax;
	if (c.zmin < b->zmin) b->zmin = c.zmin;
	if (c.zmax > b->zmax) b->zmax = c.zmax;
}

static scbounds refresh_bounds(space *sc, scchild c) {
	scbounds b = empty_bounds();
	if (c.branch) {
		scbranch *br = c.br;
		for (uint i = 0; i < ( uint ) popcnt64(br->map); i++) bounds_add_bounds(&b, refresh_bounds(sc, br->kids [i]));
		br->bounds = b;
	}
	else if (c.bk) {
		for (uvlong i = 0; i < c.bk->n; i++) bounds_add_point(&b, &sc->pts [c.bk->pts [i]]);
		c.bk->bounds = b;
	}
	return b;
}

static scbounds child_bounds(scchild c) {
	if (c.branch) return c.br ? c.br->bounds : empty_bounds();
	return c.bk ? c.bk->bounds : empty_bounds();
}

static bool bucket_append_raw(scbucket *b, uvlong pt) {
	if (!ensure_bucket_cap(b)) return false;
	b->pts [b->n++] = pt;
	return true;
}

static bool branch_insert_child(scbranch *b, uint f, scchild c) {
	uvlong bit = ( uvlong ) 1 << f;
	uint idx = rank64(b->map, f);
	uint n = ( uint ) popcnt64(b->map);
	if (b->map & bit) {
		b->kids [idx] = c;
		return true;
	}
	scchild *p = ( scchild * ) realloc(b->kids, ( size_t ) (n + 1) * sizeof(scchild));
	if (!p) {
		errmsg("space: out of memory");
		return false;
	}
	b->kids = p;
	for (uint i = n; i > idx; i--) b->kids [i] = b->kids [i - 1];
	b->kids [idx] = c;
	b->map |= bit;
	return true;
}

static bool same_tiles(space *sc, uvlong *pts, uvlong n) {
	if (n < 2) return true;
	vlong tx = sc->pts [pts [0]].tx;
	vlong ty = sc->pts [pts [0]].ty;
	vlong tz = sc->pts [pts [0]].tz;
	for (uvlong i = 1; i < n; i++)
		if (sc->pts [pts [i]].tx != tx || sc->pts [pts [i]].ty != ty || sc->pts [pts [i]].tz != tz) return false;
	return true;
}

static bool same_tiles_plus(space *sc, uvlong *pts, uvlong n, uvlong pt) {
	return !n
	    || (same_tiles(sc, pts, n)
	        && sc->pts [pt].tx == sc->pts [pts [0]].tx
	        && sc->pts [pt].ty == sc->pts [pts [0]].ty
	        && sc->pts [pt].tz == sc->pts [pts [0]].tz);
}

static bool build_bucket(uvlong *pts, uvlong n, scchild *out) {
	scbucket *b = new_bucket();
	if (!b) return false;
	for (uvlong i = 0; i < n; i++) {
		if (!bucket_append_raw(b, pts [i])) {
			free_child((scchild) {.bk = b});
			return false;
		}
	}
	*out = (scchild) {.bk = b};
	return true;
}

static bool build_subtree(space *sc, uvlong *pts, uvlong n, uint level, scchild *out) {
	*out = (scchild) {0};
	if (level >= SCLEVELS || same_tiles(sc, pts, n)) return build_bucket(pts, n, out);
	scbranch *br = new_branch();
	if (!br) return false;
	uvlong counts [SCFAN] = {0};
	for (uvlong i = 0; i < n; i++) counts [ptfrag(sc, pts [i], level)]++;
	for (uint f = 0; f < SCFAN; f++) {
		if (!counts [f]) continue;
		uvlong *grp = ( uvlong * ) malloc(( size_t ) counts [f] * sizeof(uvlong));
		if (!grp) {
			errmsg("space: out of memory");
			free_child((scchild) {.branch = true, .br = br});
			return false;
		}
		uvlong at = 0;
		for (uvlong i = 0; i < n; i++)
			if (ptfrag(sc, pts [i], level) == f) grp [at++] = pts [i];
		scchild c = {0};
		bool ok = build_subtree(sc, grp, counts [f], level + 1, &c);
		free(grp);
		if (!ok || !branch_insert_child(br, f, c)) {
			free_child(c);
			free_child((scchild) {.branch = true, .br = br});
			return false;
		}
	}
	*out = (scchild) {.branch = true, .br = br};
	return true;
}

static void update_child_positions(space *sc, scchild c) {
	if (c.branch) {
		scbranch *b = c.br;
		for (uint i = 0; i < ( uint ) popcnt64(b->map); i++) update_child_positions(sc, b->kids [i]);
	}
	else update_bucket(sc, c.bk);
}

static bool insert_into_child(space *sc, scchild *cp, uint level, uvlong pt);

static bool split_bucket_insert(space *sc, scchild *cp, uint level, uvlong pt) {
	scbucket *old = cp->bk;
	if (old->n < SCBUCKETCAP || level >= SCLEVELS || same_tiles_plus(sc, old->pts, old->n, pt)) return bucket_append(sc, old, pt);
	uvlong n = old->n + 1;
	uvlong *xs = ( uvlong * ) malloc(( size_t ) n * sizeof(uvlong));
	if (!xs) {
		errmsg("space: out of memory");
		return false;
	}
	for (uvlong i = 0; i < old->n; i++) xs [i] = old->pts [i];
	xs [old->n] = pt;
	scchild repl = {0};
	if (!build_subtree(sc, xs, n, level, &repl)) {
		free(xs);
		return false;
	}
	free(xs);
	*cp = repl;
	update_child_positions(sc, repl);
	free(old->pts);
	free(old);
	return true;
}

static bool insert_into_branch(space *sc, scbranch *br, uint level, uvlong pt) {
	uint f = ptfrag(sc, pt, level);
	uvlong bit = ( uvlong ) 1 << f;
	uint idx = rank64(br->map, f);
	if (br->map & bit) return insert_into_child(sc, &br->kids [idx], level + 1, pt);
	scbucket *b = new_bucket();
	if (!b) return false;
	if (!bucket_append_raw(b, pt)) {
		free(b);
		return false;
	}
	if (!branch_insert_child(br, f, (scchild) {.bk = b})) {
		free_child((scchild) {.bk = b});
		return false;
	}
	update_bucket(sc, b);
	return true;
}

static bool insert_into_child(space *sc, scchild *cp, uint level, uvlong pt) {
	if (cp->branch) return insert_into_branch(sc, cp->br, level, pt);
	if (cp->bk) return split_bucket_insert(sc, cp, level, pt);
	scbucket *b = new_bucket();
	if (!b) return false;
	if (!bucket_append(sc, b, pt)) {
		free(b);
		return false;
	}
	*cp = (scchild) {.bk = b};
	return true;
}

static void remove_child_at(scbranch *b, uint f) {
	uvlong bit = ( uvlong ) 1 << f;
	if (!(b->map & bit)) return;
	uint idx = rank64(b->map, f);
	uint n = ( uint ) popcnt64(b->map);
	for (uint i = idx; i + 1 < n; i++) b->kids [i] = b->kids [i + 1];
	b->map &= ~bit;
	if (n == 1) {
		free(b->kids);
		b->kids = null;
	}
	else {
		scchild *p = ( scchild * ) realloc(b->kids, ( size_t ) (n - 1) * sizeof(scchild));
		if (p) b->kids = p;
	}
}

static bool prune_child(scchild *cp, uvlong ux, uvlong uy, uvlong uz, uint level) {
	if (!cp->branch) return cp->bk && cp->bk->n == 0;
	scbranch *br = cp->br;
	uint f = keyfrag(ux, uy, uz, level);
	uvlong bit = ( uvlong ) 1 << f;
	if (br->map & bit) {
		uint idx = rank64(br->map, f);
		if (prune_child(&br->kids [idx], ux, uy, uz, level + 1)) {
			free_child(br->kids [idx]);
			remove_child_at(br, f);
		}
	}
	return br->map == 0;
}

static void bucket_remove(space *sc, scbucket *b, uvlong slot) {
	for (uvlong i = slot; i + 1 < b->n; i++) b->pts [i] = b->pts [i + 1];
	if (b->n) b->n--;
	update_bucket(sc, b);
}

static void remove_from_shape(space *sc, uvlong pt) {
	scpoint *p = &sc->pts [pt];
	scbucket *b = p->bucket;
	uvlong ux = p->ux;
	uvlong uy = p->uy;
	uvlong uz = p->uz;
	bucket_remove(sc, b, p->slot);
	p->bucket = null;
	p->slot = 0;
	if (prune_child(&sc->root, ux, uy, uz, 0)) {
		free_child(sc->root);
		sc->root = (scchild) {0};
	}
	else refresh_bounds(sc, sc->root);
}

static bool move_point_raw(space *sc, uvlong pt, double x, double y, double z) {
	scpoint old = sc->pts [pt];
	remove_from_shape(sc, pt);
	set_point_coords(sc, &sc->pts [pt], x, y, z);
	if (!insert_into_child(sc, &sc->root, 0, pt)) {
		sc->pts [pt] = old;
		if (!insert_into_child(sc, &sc->root, 0, pt)) errmsg("space move: rollback failed");
		else refresh_bounds(sc, sc->root);
		return false;
	}
	refresh_bounds(sc, sc->root);
	return true;
}

static void soa_put(space *sc, uvlong pt, uvlong row, uvlong *ids, double *xs, double *ys, double *zs, uvlong *phis) {
	scpoint *p = &sc->pts [pt];
	if (ids) ids [row] = pt;
	if (xs) xs [row] = p->x;
	if (ys) ys [row] = p->y;
	if (zs) zs [row] = p->z;
	if (phis) phis [row] = p->phi;
}

static void enum_child(space *sc, scchild c, bool (*match)(space *, uvlong, void *), void *arg, uvlong *ids,
                       double *xs, double *ys, double *zs, uvlong *phis, uvlong cap, uvlong *n) {
	if (c.branch) {
		scbranch *b = c.br;
		for (uint i = 0; i < ( uint ) popcnt64(b->map); i++) enum_child(sc, b->kids [i], match, arg, ids, xs, ys, zs, phis, cap, n);
		return;
	}
	if (!c.bk) return;
	for (uvlong i = 0; i < c.bk->n; i++) {
		uvlong pt = c.bk->pts [i];
		if (match && !match(sc, pt, arg)) continue;
		if (*n < cap) soa_put(sc, pt, *n, ids, xs, ys, zs, phis);
		(*n)++;
	}
}

static bool box_match(space *sc, uvlong pt, void *arg);
static bool sphr_match(space *sc, uvlong pt, void *arg);

static bool box_hits_bounds(scbounds b, scboxq *q) {
	return b.any
	    && b.xmax >= q->xmin && b.xmin <= q->xmax
	    && b.ymax >= q->ymin && b.ymin <= q->ymax
	    && b.zmax >= q->zmin && b.zmin <= q->zmax;
}

static double axis_box_dist(double x, double lo, double hi) {
	if (x < lo) return lo - x;
	if (x > hi) return x - hi;
	return 0.0;
}

static bool finite_bounds(scbounds b) {
	return isfinite(b.xmin) && isfinite(b.xmax) && isfinite(b.ymin) && isfinite(b.ymax) && isfinite(b.zmin) && isfinite(b.zmax);
}

static double bounds_dist2(scbounds b, double x, double y, double z) {
	double dx = axis_box_dist(x, b.xmin, b.xmax);
	double dy = axis_box_dist(y, b.ymin, b.ymax);
	double dz = axis_box_dist(z, b.zmin, b.zmax);
	return dx * dx + dy * dy + dz * dz;
}

static bool sphr_hits_bounds(scbounds b, scsphrq *q) {
	if (!b.any || !finite_bounds(b)) return b.any;
	return bounds_dist2(b, q->x, q->y, q->z) <= q->r2;
}

static void enum_box_child(space *sc, scchild c, scboxq *q, uvlong *ids, double *xs, double *ys, double *zs,
                           uvlong *phis, uvlong cap, uvlong *n) {
	if (!box_hits_bounds(child_bounds(c), q)) return;
	if (c.branch) {
		scbranch *b = c.br;
		for (uint i = 0; i < ( uint ) popcnt64(b->map); i++) enum_box_child(sc, b->kids [i], q, ids, xs, ys, zs, phis, cap, n);
		return;
	}
	enum_child(sc, c, box_match, q, ids, xs, ys, zs, phis, cap, n);
}

static void enum_sphr_child(space *sc, scchild c, scsphrq *q, uvlong *ids, double *xs, double *ys, double *zs,
                            uvlong *phis, uvlong cap, uvlong *n) {
	if (!sphr_hits_bounds(child_bounds(c), q)) return;
	if (c.branch) {
		scbranch *b = c.br;
		for (uint i = 0; i < ( uint ) popcnt64(b->map); i++) enum_sphr_child(sc, b->kids [i], q, ids, xs, ys, zs, phis, cap, n);
		return;
	}
	enum_child(sc, c, sphr_match, q, ids, xs, ys, zs, phis, cap, n);
}

typedef struct scnearq {
	double x, y, z;
	uvlong best [2];
	double dist [2];
	uvlong n;
} scnearq;

static void near_consider(space *sc, scnearq *q, uvlong pt) {
	scpoint *p = &sc->pts [pt];
	if (!isfinite(p->x) || !isfinite(p->y) || !isfinite(p->z)) return;
	double dx = p->x - q->x;
	double dy = p->y - q->y;
	double dz = p->z - q->z;
	double d = dx * dx + dy * dy + dz * dz;
	if (q->n == 0 || d < q->dist [0]) {
		q->best [1] = q->best [0];
		q->dist [1] = q->dist [0];
		q->best [0] = pt;
		q->dist [0] = d;
		if (q->n < 2) q->n++;
	}
	else if (q->n == 1 || d < q->dist [1]) {
		q->best [1] = pt;
		q->dist [1] = d;
		if (q->n < 2) q->n++;
	}
}

static double near_bound_dist(scbounds b, scnearq *q) {
	return finite_bounds(b) ? bounds_dist2(b, q->x, q->y, q->z) : 0.0;
}

static void near_child(space *sc, scchild c, scnearq *q) {
	scbounds b = child_bounds(c);
	if (!b.any) return;
	if (q->n >= 2 && finite_bounds(b) && bounds_dist2(b, q->x, q->y, q->z) > q->dist [1]) return;
	if (c.branch) {
		typedef struct {
			uint   idx;
			double dist;
		} nearord;
		scbranch *br = c.br;
		uint n = ( uint ) popcnt64(br->map);
		nearord ord [SCFAN];
		for (uint i = 0; i < n; i++) ord [i] = (nearord) {.idx = i, .dist = near_bound_dist(child_bounds(br->kids [i]), q)};
		for (uint i = 1; i < n; i++) {
			nearord v = ord [i];
			uint j = i;
			while (j && v.dist < ord [j - 1].dist) {
				ord [j] = ord [j - 1];
				j--;
			}
			ord [j] = v;
		}
		for (uint i = 0; i < n; i++) near_child(sc, br->kids [ord [i].idx], q);
		return;
	}
	if (!c.bk) return;
	for (uvlong i = 0; i < c.bk->n; i++) near_consider(sc, q, c.bk->pts [i]);
}

int mkspace(int arena, double block) {
	( void ) arena;
	if (!valid_block(block)) {
		errmsg("mkspace: bad block");
		return -1;
	}
	if (!table_init()) return -1;
	for (;;) {
		for (int i = 0; i < sccap; i++) {
			if (scs [i].live) continue;
			scs [i] = (space) {.block = block, .live = true};
			return i;
		}
		if (!table_grow()) return -1;
	}
}

void rmspace(int id) {
	space *sc = sc_lookup(id);
	if (!sc) return;
	free_child(sc->root);
	free(sc->pts);
	*sc = (space) {0};
}

uvlong scput(int id, double x, double y, double z, uvlong phi) {
	space *sc = sc_get(id);
	if (!sc || !valid_coord(x) || !valid_coord(y) || !valid_coord(z)) {
		errmsg("scput: bad argument");
		return SCNONE;
	}
	if (!ensure_pt_cap(sc)) return SCNONE;
	uvlong pt = sc->npt++;
	sc->pts [pt] = (scpoint) {.phi = phi, .live = true};
	set_point_coords(sc, &sc->pts [pt], x, y, z);
	if (!insert_into_child(sc, &sc->root, 0, pt)) {
		sc->pts [pt] = (scpoint) {0};
		sc->npt--;
		return SCNONE;
	}
	refresh_bounds(sc, sc->root);
	sc->nlpt++;
	return pt;
}

bool scdel(int id, uvlong pt) {
	space *sc = sc_get(id);
	if (!pclive(sc, pt)) {
		errmsg("scdel: bad point");
		return false;
	}
	remove_from_shape(sc, pt);
	sc->pts [pt].live = false;
	sc->nlpt--;
	return true;
}

uvlong scphi(int id, uvlong pt) {
	space *sc = sc_get(id);
	if (!pclive(sc, pt)) {
		errmsg("scphi: bad point");
		return 0;
	}
	return sc->pts [pt].phi;
}

void rscphi(int id, uvlong pt, uvlong phi) {
	space *sc = sc_get(id);
	if (!pclive(sc, pt)) {
		errmsg("rscphi: bad point");
		return;
	}
	sc->pts [pt].phi = phi;
}

void scloctn(int id, uvlong pt, double *x, double *y, double *z) {
	space *sc = sc_get(id);
	if (!pclive(sc, pt) || (!x && !y && !z)) {
		errmsg("scloctn: bad argument");
		return;
	}
	if (x) *x = sc->pts [pt].x;
	if (y) *y = sc->pts [pt].y;
	if (z) *z = sc->pts [pt].z;
}

bool scmov(int id, uvlong pt, double x, double y, double z) {
	space *sc = sc_get(id);
	if (!pclive(sc, pt) || !valid_coord(x) || !valid_coord(y) || !valid_coord(z)) {
		errmsg("scmov: bad argument");
		return false;
	}
	return move_point_raw(sc, pt, x, y, z);
}

uvlong scat(int id, double x, double y, double z) {
	space *sc = sc_get(id);
	if (!sc || !valid_coord(x) || !valid_coord(y) || !valid_coord(z)) {
		errmsg("scat: bad argument");
		return SCNONE;
	}
	uvlong found = SCNONE;
	x = canon(x);
	y = canon(y);
	z = canon(z);
	uvlong ux = tile_key(x, sc->block);
	uvlong uy = tile_key(y, sc->block);
	uvlong uz = tile_key(z, sc->block);
	scbucket *b = find_bucket(sc->root, ux, uy, uz, 0);
	for (uvlong i = 0; b && i < b->n; i++) {
		uvlong pt = b->pts [i];
		if (!same_coord(sc->pts [pt].x, x) || !same_coord(sc->pts [pt].y, y) || !same_coord(sc->pts [pt].z, z)) continue;
		if (found != SCNONE) {
			errmsg("scat: duplicate point");
			return SCNONE;
		}
		found = pt;
	}
	if (found == SCNONE) errmsg("scat: missing point");
	return found;
}

uvlong scnpt(int id) {
	space *sc = sc_get(id);
	if (!sc) {
		errmsg("scnpt: bad descriptor");
		return 0;
	}
	return sc->nlpt;
}

uvlong scpts(int id, uvlong *ids, double *xs, double *ys, double *zs, uvlong *phis, uvlong cap) {
	space *sc = sc_get(id);
	if (!sc || badsoa(ids, xs, ys, zs, phis, cap)) {
		errmsg("scpts: bad argument");
		return 0;
	}
	uvlong n = 0;
	enum_child(sc, sc->root, null, null, ids, xs, ys, zs, phis, cap, &n);
	return n;
}

static bool box_match(space *sc, uvlong pt, void *arg) {
	scboxq *q = ( scboxq * ) arg;
	scpoint *p = &sc->pts [pt];
	return p->x >= q->xmin && p->x <= q->xmax && p->y >= q->ymin && p->y <= q->ymax && p->z >= q->zmin && p->z <= q->zmax;
}

static bool sphr_match(space *sc, uvlong pt, void *arg) {
	scsphrq *q = ( scsphrq * ) arg;
	scpoint *p = &sc->pts [pt];
	if (!isfinite(p->x) || !isfinite(p->y) || !isfinite(p->z)) return false;
	double dx = p->x - q->x;
	double dy = p->y - q->y;
	double dz = p->z - q->z;
	return dx * dx + dy * dy + dz * dz <= q->r2;
}

static bool collect_push(uvlong pt, uvlong **out, uvlong *n, uvlong *cap) {
	if (*n >= *cap) {
		uvlong newcap = nextpow2_64(*n + 1);
		if (newcap < 16) newcap = 16;
		if (newcap > ( uvlong ) (SIZE_MAX / sizeof(uvlong))) {
			errmsg("space: collection capacity overflow");
			return false;
		}
		uvlong *p = ( uvlong * ) realloc(*out, ( size_t ) newcap * sizeof(uvlong));
		if (!p) {
			errmsg("space: out of memory");
			return false;
		}
		*out = p;
		*cap = newcap;
	}
	(*out) [(*n)++] = pt;
	return true;
}

static bool collect_child(space *sc, scchild c, bool (*match)(space *, uvlong, void *), void *arg, uvlong **out,
                          uvlong *n, uvlong *cap) {
	if (c.branch) {
		scbranch *b = c.br;
		for (uint i = 0; i < ( uint ) popcnt64(b->map); i++)
			if (!collect_child(sc, b->kids [i], match, arg, out, n, cap)) return false;
		return true;
	}
	if (!c.bk) return true;
	for (uvlong i = 0; i < c.bk->n; i++) {
		uvlong pt = c.bk->pts [i];
		if (match && !match(sc, pt, arg)) continue;
		if (!collect_push(pt, out, n, cap)) return false;
	}
	return true;
}

static bool collect_box_child(space *sc, scchild c, scboxq *q, uvlong **out, uvlong *n, uvlong *cap) {
	if (!box_hits_bounds(child_bounds(c), q)) return true;
	if (c.branch) {
		scbranch *b = c.br;
		for (uint i = 0; i < ( uint ) popcnt64(b->map); i++)
			if (!collect_box_child(sc, b->kids [i], q, out, n, cap)) return false;
		return true;
	}
	return collect_child(sc, c, box_match, q, out, n, cap);
}

static bool collect_sphr_child(space *sc, scchild c, scsphrq *q, uvlong **out, uvlong *n, uvlong *cap) {
	if (!sphr_hits_bounds(child_bounds(c), q)) return true;
	if (c.branch) {
		scbranch *b = c.br;
		for (uint i = 0; i < ( uint ) popcnt64(b->map); i++)
			if (!collect_sphr_child(sc, b->kids [i], q, out, n, cap)) return false;
		return true;
	}
	return collect_child(sc, c, sphr_match, q, out, n, cap);
}

static bool move_many(space *sc, uvlong *pts, uvlong n, double dx, double dy, double dz) {
	if (!isfinite(dx) || !isfinite(dy) || !isfinite(dz)) {
		errmsg("space move: bad delta");
		return false;
	}
	scoord *old = null;
	if (n) {
		if (n > ( uvlong ) (SIZE_MAX / sizeof(scoord))) {
			errmsg("space move: collection capacity overflow");
			return false;
		}
		old = ( scoord * ) malloc(( size_t ) n * sizeof(scoord));
		if (!old) {
			errmsg("space: out of memory");
			return false;
		}
	}
	for (uvlong i = 0; i < n; i++) {
		scpoint *p = &sc->pts [pts [i]];
		old [i] = (scoord) {.x = p->x, .y = p->y, .z = p->z};
		double nx = p->x + dx;
		double ny = p->y + dy;
		double nz = p->z + dz;
		if (isnan(nx) || isnan(ny) || isnan(nz)) {
			errmsg("space move: nan result");
			free(old);
			return false;
		}
	}
	for (uvlong i = 0; i < n; i++) {
		scpoint *p = &sc->pts [pts [i]];
		if (!move_point_raw(sc, pts [i], p->x + dx, p->y + dy, p->z + dz)) {
			for (uvlong j = i; j > 0; j--)
				if (!move_point_raw(sc, pts [j - 1], old [j - 1].x, old [j - 1].y, old [j - 1].z)) errmsg("space move: batch rollback failed");
			free(old);
			return false;
		}
	}
	free(old);
	return true;
}

uvlong scbox(int id, double xmin, double xmax, double ymin, double ymax, double zmin, double zmax, uvlong *ids,
             double *xs, double *ys, double *zs, uvlong *phis, uvlong cap) {
	space *sc = sc_get(id);
	if (!sc
	    || isnan(xmin) || isnan(xmax) || isnan(ymin) || isnan(ymax) || isnan(zmin) || isnan(zmax)
	    || xmin > xmax || ymin > ymax || zmin > zmax
	    || badsoa(ids, xs, ys, zs, phis, cap))
	{
		errmsg("scbox: bad argument");
		return 0;
	}
	scboxq q = {.xmin = canon(xmin), .xmax = canon(xmax), .ymin = canon(ymin), .ymax = canon(ymax), .zmin = canon(zmin), .zmax = canon(zmax)};
	uvlong n = 0;
	enum_box_child(sc, sc->root, &q, ids, xs, ys, zs, phis, cap, &n);
	return n;
}

uvlong scsphr(int id, double x, double y, double z, double r, uvlong *ids, double *xs, double *ys, double *zs,
              uvlong *phis, uvlong cap) {
	space *sc = sc_get(id);
	if (!sc || !isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(r) || r < 0.0 || badsoa(ids, xs, ys, zs, phis, cap)) {
		errmsg("scsphr: bad argument");
		return 0;
	}
	scsphrq q = {.x = canon(x), .y = canon(y), .z = canon(z), .r2 = r * r};
	uvlong n = 0;
	enum_sphr_child(sc, sc->root, &q, ids, xs, ys, zs, phis, cap, &n);
	return n;
}

uvlong scnear(int id, double x, double y, double z, uvlong *first, uvlong *second) {
	space *sc = sc_get(id);
	if (!sc || !isfinite(x) || !isfinite(y) || !isfinite(z) || !first) {
		errmsg("scnear: bad argument");
		return 0;
	}
	scnearq q = {.x = canon(x), .y = canon(y), .z = canon(z), .best = {SCNONE, SCNONE}, .dist = {0.0, 0.0}, .n = 0};
	near_child(sc, sc->root, &q);
	if (!q.n) {
		errmsg("scnear: no point");
		return 0;
	}
	*first = q.best [0];
	if (second) {
		if (q.n < 2) {
			errmsg("scnear: no second point");
			return 1;
		}
		*second = q.best [1];
	}
	return second ? 2 : 1;
}

uvlong scboxmove(int id, double xmin, double xmax, double ymin, double ymax, double zmin, double zmax, double dx,
                 double dy, double dz) {
	space *sc = sc_get(id);
	if (!sc || isnan(xmin) || isnan(xmax) || isnan(ymin) || isnan(ymax) || isnan(zmin) || isnan(zmax)
	    || xmin > xmax || ymin > ymax || zmin > zmax)
	{
		errmsg("scboxmove: bad argument");
		return 0;
	}
	scboxq q = {.xmin = canon(xmin), .xmax = canon(xmax), .ymin = canon(ymin), .ymax = canon(ymax), .zmin = canon(zmin), .zmax = canon(zmax)};
	uvlong *pts = null;
	uvlong n = 0, cap = 0;
	if (!collect_box_child(sc, sc->root, &q, &pts, &n, &cap)) {
		free(pts);
		return 0;
	}
	bool ok = move_many(sc, pts, n, dx, dy, dz);
	free(pts);
	return ok ? n : 0;
}

uvlong scsphrmove(int id, double x, double y, double z, double r, double dx, double dy, double dz) {
	space *sc = sc_get(id);
	if (!sc || !isfinite(x) || !isfinite(y) || !isfinite(z) || !isfinite(r) || r < 0.0) {
		errmsg("scsphrmove: bad argument");
		return 0;
	}
	scsphrq q = {.x = canon(x), .y = canon(y), .z = canon(z), .r2 = r * r};
	uvlong *pts = null;
	uvlong n = 0, cap = 0;
	if (!collect_sphr_child(sc, sc->root, &q, &pts, &n, &cap)) {
		free(pts);
		return 0;
	}
	bool ok = move_many(sc, pts, n, dx, dy, dz);
	free(pts);
	return ok ? n : 0;
}
