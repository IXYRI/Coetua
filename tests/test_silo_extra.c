#include "coetua.h"
#include <stdio.h>
#include <string.h>

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

static bool pair_key_eq(uvlong *fields, char *a, char *b) {
	uvlong *pair = ( uvlong * ) fields [1];
	return fields [0] == sizeof(uvlong) * 4
	    && pair [0] == strlen(a)
	    && pair [2] == strlen(b)
	    && memcmp(( void * ) pair [1], a, pair [0]) == 0
	    && memcmp(( void * ) pair [3], b, pair [2]) == 0;
}

static bool set_has_pair(int set, char *a, char *b) {
	int    it         = mkiter(set);
	uvlong fields [2] = {0};
	bool   found      = false;
	while (next(it, fields))
		if (pair_key_eq(fields, a, b)) {
			found = true;
			break;
		}
	rmiter(it);
	return found;
}

int main(void) {
	printf("=== swath ===\n");
	int seq = mkseq(0);
	for (uvlong i = 10; i <= 50; i += 10) atch(seq, i);
	uvlong *slice = null;
	uvlong  len   = swath(seq, 1, 3, &slice);
	CHECK(len == 3, "swath returns inclusive length");
	CHECK(slice && slice [0] == 20 && slice [1] == 30 && slice [2] == 40, "swath returns contiguous view");
	len = swath(seq, -3, -1, &slice);
	CHECK(len == 3 && slice [0] == 30 && slice [2] == 50, "swath supports negative indices");
	len = swath(seq, 3, 1, &slice);
	CHECK(len == 0 && slice == null, "swath rejects reversed range");
	len = swath(seq, -99, 2, &slice);
	CHECK(len == 0 && slice == null, "swath rejects out of range");
	rmseq(seq);

	printf("\n=== cartesprod ===\n");
	int a = mkset(0);
	int b = mkset(0);
	adds(a, "a", 1);
	adds(a, "b", 1);
	adds(b, "x", 1);
	adds(b, "y", 1);
	int prod = cartesprod(0, a, b);
	CHECK(prod >= 0 && silotype_of(prod) == silo_set, "cartesprod returns set");
	CHECK(carten(prod) == 4, "cartesprod size");
	CHECK(set_has_pair(prod, "a", "x"), "cartesprod has ax");
	CHECK(set_has_pair(prod, "a", "y"), "cartesprod has ay");
	CHECK(set_has_pair(prod, "b", "x"), "cartesprod has bx");
	CHECK(set_has_pair(prod, "b", "y"), "cartesprod has by");
	rmset(a);
	rmset(b);
	rmset(prod);

	printf("\n=== result: %d failures ===\n", failures);
	return failures;
}
