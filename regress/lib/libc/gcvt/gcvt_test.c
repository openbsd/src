/* Public domain */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct test_vector {
	double d;
	int ndig;
	char *expect;
} test_vectors[] = {
	{ 0.1, 8, "0.1" },
	{ 0.01, 8, "0.01" },
	{ 0.001, 8, "0.001" },
	{ 0.0001, 8, "0.0001" },
	{ 0.00009, 8, "9e-05" },
	{ 1.0, 8, "1" },
	{ 1.1, 8, "1.1" },
	{ 1.01, 8, "1.01" },
	{ 1.001, 8, "1.001" },
	{ 1.0001, 8, "1.0001" },
	{ 1.00001, 8, "1.00001" },
	{ 1.000001, 8, "1.000001" },
	{ 0.0, 8, "0" },
	{ -1.0, 8, "-1" },
	{ 100000.0, 8, "100000" },
	{ -100000.0, 8, "-100000" },
	{ 123.456, 8, "123.456" },
	{ 1e34, 8, "1e+34" },
	{ 0.0, 0, NULL }
};

static int
dotest(struct test_vector *tv)
{
	char buf[64];

	gcvt(tv->d, tv->ndig, buf);
	if (strcmp(tv->expect, buf) != 0) {
		fprintf(stderr, "gcvt: expected %s, got %s\n", tv->expect, buf);
		return 1;
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	int i, failures = 0;

	for (i = 0; test_vectors[i].expect != NULL; i++) {
		failures += dotest(&test_vectors[i]);
	}

	return failures;
}
