#include <stdio.h>
#include <pthread.h>
#include <err.h>
#include "bench.h"

static char name[] =  "Null test";
static char doc[] = 
"\tThe time needed for performing a tight empty loop. This indicates\n"
"\tthe overhead incurred by the measurement harness.";



int
main() {
	bench_t b;

	bench_init(&b, name, doc, "per cycle");
	bench_header(&b);
	bench_amortize(&b, BENCH_LOOPS) {
		/* nothng */
	}
	bench_report(&b);
	exit(0);
}


