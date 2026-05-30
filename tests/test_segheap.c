#include "coetua.h"
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

static int cmpuv(uvlong a, uvlong b, void *arg) {
	( void ) arg;
	return (a > b) - (a < b);
}

static void check_expected_error(bool ok, char *label) {
	CHECK(ok && err(), label);
	errmsg(null);
}

static bool has_key(uvlong *xs, uvlong n, uvlong key) {
	for (uvlong i = 0; i < n; i++)
		if (xs [i] == key) return true;
	return false;
}

static void descriptor_cases(void) {
	printf("\n=== segheap: descriptor ===\n");
	errmsg(null);
	rmsegheap(123456);
	CHECK(!err(), "rmsegheap bad descriptor is quiet");
	check_expected_error(sgnitem(123456) == 0, "sgnitem bad descriptor sets error");
	int h = mksegheap(0, false);
	CHECK(h >= 0, "mksegheap creates descriptor");
	rmsegheap(h);
	int reused = mksegheap(0, true);
	CHECK(reused == h, "mksegheap reuses descriptor");
	rmsegheap(reused);

	int ids [80];
	bool ok = true;
	for (uvlong i = 0; i < arrlen(ids); i++) {
		ids [i] = mksegheap(0, false);
		ok = ok && ids [i] >= 0;
	}
	CHECK(ok, "segheap descriptor table grows");
	for (uvlong i = 0; i < arrlen(ids); i++) rmsegheap(ids [i]);
}

static void max_heap_pop(void) {
	printf("\n=== segheap: max put top pop ===\n");
	int h = mksegheap(0, false);
	CHECK(h >= 0, "mksegheap max");
	uvlong keys [] = {5, 1, 9, 3, 9, 2, 8};
	for (uvlong i = 0; i < arrlen(keys); i++) sgput(h, keys [i], keys [i] + 100, cmpuv, null);
	CHECK(!err() && sgnitem(h) == arrlen(keys), "sgput inserts items");
	uvlong key = 0, ref = 0;
	CHECK(sgtop(h, &key, &ref) && key == 9 && ref == 109, "sgtop reads max item");
	uvlong expect [] = {9, 9, 8, 5, 3, 2, 1};
	bool ok = true;
	for (uvlong i = 0; i < arrlen(expect); i++) {
		ok = ok && sgpop(h, &key, &ref, cmpuv, null) && key == expect [i] && ref == expect [i] + 100;
	}
	CHECK(ok && sgnitem(h) == 0, "sgpop drains in max priority order");
	CHECK(!sgtop(h, &key, &ref) && !err(), "empty sgtop quiet false");
	CHECK(!sgpop(h, &key, &ref, cmpuv, null) && !err(), "empty sgpop quiet false");
	rmsegheap(h);
}

static void min_heap_smoke(void) {
	printf("\n=== segheap: min heap ===\n");
	int h = mksegheap(0, true);
	CHECK(h >= 0, "mksegheap min");
	uvlong xs [] = {7, 4, 9, 1, 4, 2};
	for (uvlong i = 0; i < arrlen(xs); i++) sgput(h, xs [i], xs [i] + 10, cmpuv, null);
	uvlong key = 0, ref = 0;
	CHECK(sgtop(h, &key, &ref) && key == 1, "min sgtop reads minimum");
	CHECK(sgdel(h, 4, cmpuv, null) && sgnitem(h) == 5, "min sgdel removes one equal");
	CHECK(sgdels(h, 4, 10, cmpuv, null) == 1 && !sgfind(h, 4, cmpuv, null, null, null), "min sgdels removes remaining equal");
	CHECK(sgpop(h, &key, &ref, cmpuv, null) && key == 1, "min sgpop keeps minimum direction");
	rmsegheap(h);
}

static void find_delete_items(void) {
	printf("\n=== segheap: find delete items ===\n");
	int h = mksegheap(0, false);
	CHECK(h >= 0, "mksegheap find");
	for (uvlong i = 0; i < 12; i++) sgput(h, i % 4, i + 1000, cmpuv, null);
	uvlong key = 99, ref = 99;
	CHECK(sgfind(h, 2, cmpuv, null, &key, &ref) && key == 2, "sgfind returns an equal item");
	CHECK(sgfind(h, 2, cmpuv, null, null, null), "sgfind allows null outputs");
	CHECK(!sgfind(h, 99, cmpuv, null, &key, &ref) && !err(), "sgfind miss quiet false");
	CHECK(sgdel(h, 2, cmpuv, null) && sgnitem(h) == 11, "sgdel deletes one equal");
	CHECK(sgdels(h, 2, 1, cmpuv, null) == 1, "sgdels deletes at most n");
	CHECK(sgdels(h, 2, 0, cmpuv, null) == 0 && !err(), "sgdels n zero quiet");
	CHECK(sgdels(h, 99, 3, cmpuv, null) == 0 && !err(), "sgdels miss quiet zero");
	CHECK(sgdels(h, 2, 10, cmpuv, null) == 1 && !sgfind(h, 2, cmpuv, null, null, null), "sgdels deletes remaining matches");
	rmsegheap(h);
}

static void batch_put_cases(void) {
	printf("\n=== segheap: batch put ===\n");
	int h = mksegheap(0, false);
	CHECK(h >= 0, "mksegheap batch");
	uvlong keys [] = {12, 4, 18, 7, 18, 1, 30, 9, 2};
	uvlong refs [] = {112, 104, 118, 107, 218, 101, 130, 109, 102};
	sgputs(h, keys, refs, arrlen(keys), cmpuv, null);
	CHECK(!err() && sgnitem(h) == arrlen(keys), "sgputs inserts batch");
	uvlong key = 0, ref = 0;
	CHECK(sgtop(h, &key, &ref) && key == 30 && ref == 130, "sgputs heapifies max batch");
	CHECK(sgdel(h, 18, cmpuv, null) && sgnitem(h) == arrlen(keys) - 1, "sgdel works after sgputs");
	uvlong morek [] = {40, 3, 41, 6};
	uvlong morer [] = {400, 300, 410, 600};
	sgputs(h, morek, morer, arrlen(morek), cmpuv, null);
	CHECK(!err() && sgtop(h, &key, &ref) && key == 41 && ref == 410, "sgputs appends to existing heap");
	uvlong prev = ( uvlong ) -1;
	bool ok = true;
	while (sgpop(h, &key, null, cmpuv, null)) {
		ok = ok && key <= prev;
		prev = key;
	}
	CHECK(ok, "batch heap drains in priority order");
	rmsegheap(h);

	h = mksegheap(0, true);
	CHECK(h >= 0, "mksegheap batch min");
	sgputs(h, keys, refs, arrlen(keys), cmpuv, null);
	CHECK(!err() && sgtop(h, &key, null) && key == 1, "sgputs respects min heap order");
	rmsegheap(h);

	h = mksegheap(0, false);
	CHECK(h >= 0, "mksegheap batch invalids");
	sgputs(h, keys, refs, 0, cmpuv, null);
	CHECK(!err() && sgnitem(h) == 0, "sgputs empty batch quiet");
	errmsg(null);
	sgputs(h, null, refs, 1, cmpuv, null);
	CHECK(err() && sgnitem(h) == 0, "sgputs null keys sets error");
	errmsg(null);
	sgputs(h, keys, null, 1, cmpuv, null);
	CHECK(err() && sgnitem(h) == 0, "sgputs null refs sets error");
	errmsg(null);
	sgputs(h, keys, refs, 1, null, null);
	CHECK(err() && sgnitem(h) == 0, "sgputs null comparator sets error");
	errmsg(null);
	rmsegheap(h);
}

static void soa_and_invalids(void) {
	printf("\n=== segheap: soa invalids ===\n");
	int h = mksegheap(0, false);
	CHECK(h >= 0, "mksegheap soa");
	sgput(h, 1, 11, cmpuv, null);
	sgput(h, 3, 33, cmpuv, null);
	sgput(h, 2, 22, cmpuv, null);
	uvlong keys [4] = {99, 99, 99, 99};
	uvlong refs [4] = {99, 99, 99, 99};
	CHECK(sgitems(h, null, null, 0) == 3, "sgitems all-null cap zero counts");
	CHECK(sgitems(h, keys, null, 2) == 3 && has_key(keys, 2, 1) + has_key(keys, 2, 2) + has_key(keys, 2, 3) >= 2,
	      "sgitems key-only respects cap");
	CHECK(sgitems(h, keys, refs, 4) == 3 && has_key(keys, 3, 1) && has_key(keys, 3, 2) && has_key(keys, 3, 3),
	      "sgitems writes soa columns");
	check_expected_error(sgitems(h, null, null, 1) == 0, "sgitems all-null cap sets error");
	errmsg(null);
	sgput(123456, 1, 1, cmpuv, null);
	CHECK(err(), "sgput bad descriptor sets error");
	errmsg(null);
	sgput(h, 1, 1, null, null);
	CHECK(err(), "sgput null comparator sets error");
	errmsg(null);
	check_expected_error(!sgpop(h, null, null, null, null), "sgpop null comparator sets error");
	check_expected_error(!sgfind(h, 1, null, null, null, null), "sgfind null comparator sets error");
	check_expected_error(!sgdel(h, 1, null, null), "sgdel null comparator sets error");
	check_expected_error(sgdels(h, 1, 1, null, null) == 0, "sgdels null comparator sets error");
	rmsegheap(h);
}

static void stress_cases(void) {
	printf("\n=== segheap: stress ===\n");
	int h = mksegheap(0, false);
	CHECK(h >= 0, "mksegheap stress");
	bool ok = true;
	for (uvlong i = 0; i < 200; i++) sgput(h, i, i + 1, cmpuv, null);
	for (uvlong i = 0; i < 50; i++) ok = ok && sgdel(h, i * 3, cmpuv, null);
	for (uvlong i = 0; i < 100; i++) sgput(h, 500 - i, i, cmpuv, null);
	uvlong prev = ( uvlong ) -1;
	uvlong key = 0;
	while (sgpop(h, &key, null, cmpuv, null)) {
		ok = ok && key <= prev;
		prev = key;
	}
	CHECK(ok && sgnitem(h) == 0, "mixed insert/delete/pop remains ordered");
	rmsegheap(h);

	h = mksegheap(0, false);
	ok = true;
	for (uvlong i = 0; i < 120; i++) sgput(h, i % 5, i, cmpuv, null);
	ok = ok && sgdels(h, 3, 1000, cmpuv, null) == 24;
	ok = ok && !sgfind(h, 3, cmpuv, null, null, null);
	CHECK(ok, "duplicate-heavy delete removes matching key");
	rmsegheap(h);
}

static void dynamic_delete_shapes(void) {
	printf("\n=== segheap: dynamic delete shapes ===\n");
	int h = mksegheap(0, false);
	CHECK(h >= 0, "mksegheap delete shapes");
	bool ok = true;
	for (uvlong i = 0; i < 88; i++) sgput(h, i, i, cmpuv, null);
	ok = ok && sgdel(h, 87, cmpuv, null);
	ok = ok && sgdel(h, 43, cmpuv, null);
	ok = ok && sgdel(h, 3, cmpuv, null);
	ok = ok && !sgfind(h, 87, cmpuv, null, null, null);
	ok = ok && !sgfind(h, 43, cmpuv, null, null, null);
	ok = ok && !sgfind(h, 3, cmpuv, null, null, null);
	uvlong prev = ( uvlong ) -1;
	uvlong key = 0;
	while (sgpop(h, &key, null, cmpuv, null)) {
		ok = ok && key <= prev && key != 87 && key != 43 && key != 3;
		prev = key;
	}
	CHECK(ok, "delete root internal and leaf cases preserve heap");
	rmsegheap(h);

	h = mksegheap(0, false);
	ok = true;
	for (uvlong i = 0; i < 143; i++) sgput(h, (i * 37) % 211, i, cmpuv, null);
	uvlong dels [] = {0, 37, 74, 111, 148, 185, 11, 48, 85};
	for (uvlong i = 0; i < arrlen(dels); i++) ok = ok && sgdel(h, dels [i], cmpuv, null);
	prev = ( uvlong ) -1;
	while (sgpop(h, &key, null, cmpuv, null)) {
		ok = ok && key <= prev;
		prev = key;
	}
	CHECK(ok, "delete across multi-root forest preserves priority order");
	rmsegheap(h);
}

int main(void) {
	descriptor_cases();
	max_heap_pop();
	min_heap_smoke();
	find_delete_items();
	batch_put_cases();
	soa_and_invalids();
	stress_cases();
	dynamic_delete_shapes();
	printf("\n=== result: %d failures ===\n", failures);
	return failures;
}
