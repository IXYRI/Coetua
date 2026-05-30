#include "coetua.h"
#include <math.h>
#include <stdio.h>

static int failures = 0;
#define CHECK(cond, msg)                    \
	do {                                    \
		if (!(cond)) {                      \
			printf("FAIL: %s\n", msg);      \
			failures++;                     \
		}                                   \
		else { printf("  ok: %s\n", msg); } \
	}                                       \
	while (0)

static void check_expected_error(bool ok, char *label) {
	CHECK(ok && err(), label);
	errmsg(null);
}

static double pinf(void) {
	volatile double z = 0.0;
	return 1.0 / z;
}

static double qnan(void) {
	volatile double z = 0.0;
	return z / z;
}

static bool near_double(double a, double b) {
	double d = a - b;
	return d < 0 ? d > -1e-12 : d < 1e-12;
}

static bool has_id(uvlong *xs, uvlong n, uvlong id) {
	for (uvlong i = 0; i < n; i++)
		if (xs [i] == id) return true;
	return false;
}

static void descriptor_and_block(void) {
	printf("\n=== space: descriptor block ===\n");
	errmsg(null);
	rmspace(123456);
	CHECK(!err(), "rmspace bad descriptor is quiet");
	check_expected_error(scnpt(123456) == 0, "scnpt bad descriptor sets error");
	int sc = mkspace(0, 4.0);
	CHECK(sc >= 0, "mkspace creates descriptor");
	rmspace(sc);
	int reused = mkspace(0, 8.0);
	CHECK(reused == sc, "mkspace reuses descriptor");
	rmspace(reused);
	check_expected_error(mkspace(0, 0.0) < 0, "zero block rejected");
	check_expected_error(mkspace(0, -1.0) < 0, "negative block rejected");
	check_expected_error(mkspace(0, pinf()) < 0, "infinite block rejected");
	check_expected_error(mkspace(0, qnan()) < 0, "nan block rejected");

	int ids [80];
	bool ok = true;
	for (uvlong i = 0; i < arrlen(ids); i++) {
		ids [i] = mkspace(0, 1.0);
		ok = ok && ids [i] >= 0;
	}
	CHECK(ok, "space descriptor table grows");
	for (uvlong i = 0; i < arrlen(ids); i++) rmspace(ids [i]);
}

static void points_phi_location_and_at(void) {
	printf("\n=== space: points phi location at ===\n");
	int sc = mkspace(0, 2.0);
	CHECK(sc >= 0, "mkspace for points");
	uvlong a = scput(sc, -0.0, 0.0, -0.0, 10);
	uvlong b = scput(sc, pinf(), -pinf(), pinf(), 20);
	CHECK(a != ( uvlong ) -1 && b != ( uvlong ) -1 && a != b, "scput creates stable points");
	CHECK(scnpt(sc) == 2 && scphi(sc, a) == 10 && scphi(sc, b) == 20, "scnpt and scphi");
	rscphi(sc, a, 11);
	CHECK(scphi(sc, a) == 11, "rscphi updates phi");
	double x = 99, y = 99, z = 99;
	scloctn(sc, a, &x, &y, &z);
	CHECK(near_double(x, 0.0) && near_double(y, 0.0) && near_double(z, 0.0), "scloctn canonicalizes negative zero");
	CHECK(scat(sc, 0.0, -0.0, 0.0) == a, "scat finds canonical zero coordinate");
	CHECK(scat(sc, pinf(), -pinf(), pinf()) == b, "scat finds infinite coordinate");
	check_expected_error(scat(sc, 123.0, 0.0, 0.0) == ( uvlong ) -1, "scat missing point sets error");
	check_expected_error(scput(sc, qnan(), 0.0, 0.0, 1) == ( uvlong ) -1, "scput rejects nan x");
	check_expected_error(scput(sc, 0.0, qnan(), 0.0, 1) == ( uvlong ) -1, "scput rejects nan y");
	check_expected_error(scput(sc, 0.0, 0.0, qnan(), 1) == ( uvlong ) -1, "scput rejects nan z");
	uvlong d0 = scput(sc, 7.0, 7.0, 7.0, 70);
	uvlong d1 = scput(sc, 7.0, 7.0, 7.0, 71);
	CHECK(d0 != ( uvlong ) -1 && d1 != ( uvlong ) -1, "duplicates can be inserted");
	check_expected_error(scat(sc, 7.0, 7.0, 7.0) == ( uvlong ) -1, "scat duplicate coordinate sets error");
	CHECK(scmov(sc, a, 3.5, -4.5, 5.5) && scat(sc, 3.5, -4.5, 5.5) == a && scphi(sc, a) == 11, "scmov preserves id and phi");
	check_expected_error(!scmov(sc, a, qnan(), 1.0, 1.0), "scmov rejects nan coordinate");
	CHECK(scat(sc, 3.5, -4.5, 5.5) == a, "failed scmov leaves point queryable");
	CHECK(scdel(sc, b) && scnpt(sc) == 3, "scdel removes live point");
	check_expected_error(scphi(sc, b) == 0, "deleted point rejected");
	check_expected_error(!scdel(sc, b), "double delete rejected");
	rmspace(sc);
}

static void soa_points(void) {
	printf("\n=== space: soa points ===\n");
	int sc = mkspace(0, 1.0);
	CHECK(sc >= 0, "mkspace for soa");
	uvlong a = scput(sc, 1.0, 2.0, 3.0, 10);
	uvlong b = scput(sc, 4.0, 5.0, 6.0, 20);
	uvlong ids [4] = {99, 99, 99, 99};
	double xs [4] = {99, 99, 99, 99};
	double ys [4] = {99, 99, 99, 99};
	double zs [4] = {99, 99, 99, 99};
	uvlong phis [4] = {99, 99, 99, 99};
	CHECK(scpts(sc, null, null, null, null, null, 0) == 2, "scpts all-null cap zero counts");
	CHECK(scpts(sc, ids, null, null, null, null, 1) == 2 && (ids [0] == a || ids [0] == b) && ids [1] == 99,
	      "scpts ids-only respects cap");
	CHECK(scpts(sc, null, xs, ys, zs, phis, 4) == 2
	        && ((near_double(xs [0], 1.0) && near_double(ys [0], 2.0) && near_double(zs [0], 3.0) && phis [0] == 10)
	            || (near_double(xs [1], 1.0) && near_double(ys [1], 2.0) && near_double(zs [1], 3.0) && phis [1] == 10))
	        && ((near_double(xs [0], 4.0) && near_double(ys [0], 5.0) && near_double(zs [0], 6.0) && phis [0] == 20)
	            || (near_double(xs [1], 4.0) && near_double(ys [1], 5.0) && near_double(zs [1], 6.0) && phis [1] == 20)),
	      "scpts writes selected soa columns");
	check_expected_error(scpts(sc, null, null, null, null, null, 1) == 0, "scpts cap with no outputs sets error");
	CHECK(scdel(sc, a) && scpts(sc, ids, null, null, null, null, 4) == 1 && has_id(ids, 1, b), "scpts skips deleted points");
	rmspace(sc);
}

static void box_and_sphere_shape_queries(void) {
	printf("\n=== space: box sphere shape queries ===\n");
	int sc = mkspace(0, 1.0);
	CHECK(sc >= 0, "mkspace for shape queries");
	uvlong a = scput(sc, 0.0, 0.0, 0.0, 10);
	uvlong b = scput(sc, 1.0, 1.0, 1.0, 20);
	uvlong c = scput(sc, -2.0, 3.0, 0.5, 30);
	uvlong d = scput(sc, pinf(), 2.0, 3.0, 40);
	uvlong e = scput(sc, -pinf(), -pinf(), -pinf(), 50);
	uvlong ids [8] = {99, 99, 99, 99, 99, 99, 99, 99};
	uvlong n = scbox(sc, -2.0, 1.0, 0.0, 3.0, 0.0, 1.0, ids, null, null, null, null, arrlen(ids));
	CHECK(n == 3 && has_id(ids, n, a) && has_id(ids, n, b) && has_id(ids, n, c), "scbox inclusive finite bounds");
	n = scbox(sc, -pinf(), pinf(), -pinf(), pinf(), -pinf(), pinf(), ids, null, null, null, null, arrlen(ids));
	CHECK(n == 5 && has_id(ids, n, d) && has_id(ids, n, e), "scbox supports infinite bounds and points");
	n = scsphr(sc, 0.0, 0.0, 0.0, 1.75, ids, null, null, null, null, arrlen(ids));
	CHECK(n == 2 && has_id(ids, n, a) && has_id(ids, n, b), "scsphr includes boundary finite points only");
	check_expected_error(scbox(sc, 2.0, 1.0, 0.0, 1.0, 0.0, 1.0, ids, null, null, null, null, arrlen(ids)) == 0,
	                     "scbox rejects reversed bounds");
	check_expected_error(scsphr(sc, pinf(), 0.0, 0.0, 1.0, ids, null, null, null, null, arrlen(ids)) == 0,
	                     "scsphr rejects infinite center");
	CHECK(scmov(sc, c, 10.0, 10.0, 10.0), "scmov reindexes point");
	n = scbox(sc, -2.0, 1.0, 0.0, 3.0, 0.0, 1.0, ids, null, null, null, null, arrlen(ids));
	CHECK(n == 2 && !has_id(ids, n, c), "scbox reflects moved point");
	CHECK(scdel(sc, b), "scdel removes from shape");
	n = scsphr(sc, 0.0, 0.0, 0.0, 1.75, ids, null, null, null, null, arrlen(ids));
	CHECK(n == 1 && has_id(ids, n, a), "scsphr reflects deleted point");
	rmspace(sc);
}

static void split_heavy_points(void) {
	printf("\n=== space: split heavy points ===\n");
	enum
	{
		N = 216,
	};
	int sc = mkspace(0, 0.5);
	CHECK(sc >= 0, "mkspace split heavy");
	uvlong ids [N];
	bool ok = true;
	for (uvlong i = 0; i < N; i++) {
		double x = ( double ) (i % 6) * 0.5;
		double y = ( double ) ((i / 6) % 6) * 0.5;
		double z = ( double ) (i / 36) * 0.5;
		ids [i] = scput(sc, x, y, z, i + 1000);
		ok = ok && ids [i] != ( uvlong ) -1;
	}
	ok = ok && scnpt(sc) == N;
	ok = ok && scbox(sc, 0.0, 2.5, 0.0, 2.5, 0.0, 2.5, null, null, null, null, null, 0) == N;
	for (uvlong i = 0; i < N; i += 4) ok = ok && scdel(sc, ids [i]);
	ok = ok && scnpt(sc) == N - (N + 3) / 4;
	ok = ok && scbox(sc, 0.0, 2.5, 0.0, 2.5, 0.0, 2.5, null, null, null, null, null, 0) == scnpt(sc);
	CHECK(ok, "split-heavy insert/delete stays queryable");
	rmspace(sc);

	sc = mkspace(0, 10.0);
	CHECK(sc >= 0, "mkspace same-block split case");
	ok = true;
	for (uvlong i = 0; i < 64; i++) ok = ok && scput(sc, 1.0, 1.0, 1.0, i) != ( uvlong ) -1;
	uvlong far = scput(sc, 1000.0, -1000.0, 500.0, 999);
	ok = ok && far != ( uvlong ) -1 && scat(sc, 1000.0, -1000.0, 500.0) == far;
	ok = ok && scbox(sc, 999.0, 1001.0, -1001.0, -999.0, 499.0, 501.0, null, null, null, null, null, 0) == 1;
	CHECK(ok, "full same-block bucket splits when new tile differs");
	rmspace(sc);
}

static void batch_moves(void) {
	printf("\n=== space: batch moves ===\n");
	int sc = mkspace(0, 1.0);
	CHECK(sc >= 0, "mkspace batch moves");
	uvlong a = scput(sc, 0.0, 0.0, 0.0, 10);
	uvlong b = scput(sc, 1.0, 0.0, 0.0, 20);
	uvlong c = scput(sc, 4.0, 4.0, 4.0, 30);
	uvlong d = scput(sc, pinf(), 0.0, 0.0, 40);
	CHECK(scboxmove(sc, -0.5, 1.5, -0.5, 0.5, -0.5, 0.5, 10.0, 1.0, 2.0) == 2,
	      "scboxmove moves selected finite points");
	double x = 0, y = 0, z = 0;
	scloctn(sc, a, &x, &y, &z);
	bool ok = near_double(x, 10.0) && near_double(y, 1.0) && near_double(z, 2.0) && scphi(sc, a) == 10;
	scloctn(sc, b, &x, &y, &z);
	ok = ok && near_double(x, 11.0) && near_double(y, 1.0) && near_double(z, 2.0) && scphi(sc, b) == 20;
	ok = ok && scbox(sc, 10.0, 11.0, 1.0, 1.0, 2.0, 2.0, null, null, null, null, null, 0) == 2;
	CHECK(ok, "scboxmove preserves ids phis and reindexes");
	CHECK(scsphrmove(sc, 4.0, 4.0, 4.0, 0.0, -1.0, -1.0, -1.0) == 1 && scat(sc, 3.0, 3.0, 3.0) == c,
	      "scsphrmove moves sphere matches");
	CHECK(scboxmove(sc, pinf(), pinf(), -1.0, 1.0, -1.0, 1.0, 5.0, 0.0, 0.0) == 1,
	      "scboxmove can move infinite point by finite delta");
	scloctn(sc, d, &x, &y, &z);
	CHECK(isinf(x) && x > 0 && near_double(y, 0.0) && near_double(z, 0.0), "infinite point remains infinite after finite move");
	check_expected_error(scboxmove(sc, -pinf(), pinf(), -pinf(), pinf(), -pinf(), pinf(), pinf(), 0.0, 0.0) == 0,
	                     "scboxmove rejects infinite delta");
	check_expected_error(scsphrmove(sc, 0.0, 0.0, 0.0, 1.0, 0.0, qnan(), 0.0) == 0, "scsphrmove rejects nan delta");
	rmspace(sc);
}

static void nearest_queries(void) {
	printf("\n=== space: nearest ===\n");
	int sc = mkspace(0, 1.0);
	CHECK(sc >= 0, "mkspace nearest");
	uvlong inf = scput(sc, pinf(), 0.0, 0.0, 99);
	uvlong a = scput(sc, 0.0, 0.0, 0.0, 10);
	uvlong b = scput(sc, 2.0, 0.0, 0.0, 20);
	uvlong c = scput(sc, -5.0, 0.0, 0.0, 30);
	uvlong first = 99, second = 99;
	CHECK(scnear(sc, 0.25, 0.0, 0.0, &first, null) == 1 && first == a, "scnear writes nearest point");
	CHECK(scnear(sc, 0.25, 0.0, 0.0, &first, &second) == 2 && first == a && second == b, "scnear writes nearest two points");
	CHECK(scdel(sc, a) && scdel(sc, b) && scdel(sc, c), "remove finite points");
	check_expected_error(scnear(sc, 0.0, 0.0, 0.0, &first, null) == 0, "scnear ignores infinite-only space");
	CHECK(inf != ( uvlong ) -1 && scphi(sc, inf) == 99, "infinite point remains live");
	uvlong only = scput(sc, 4.0, 4.0, 4.0, 40);
	CHECK(scnear(sc, 0.0, 0.0, 0.0, &first, null) == 1 && first == only, "scnear allows optional second");
	check_expected_error(scnear(sc, 0.0, 0.0, 0.0, &first, &second) == 1 && first == only,
	                     "scnear errors when requested second is absent");
	check_expected_error(scnear(sc, pinf(), 0.0, 0.0, &first, null) == 0, "scnear rejects infinite query");
	check_expected_error(scnear(sc, 0.0, 0.0, 0.0, null, null) == 0, "scnear requires first output");
	rmspace(sc);
}

int main(void) {
	descriptor_and_block();
	points_phi_location_and_at();
	soa_points();
	box_and_sphere_shape_queries();
	split_heavy_points();
	batch_moves();
	nearest_queries();
	printf("\n=== result: %d failures ===\n", failures);
	return failures;
}
