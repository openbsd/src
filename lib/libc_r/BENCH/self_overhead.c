#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <err.h>
#include "bench.h"

static char name[] =  "Self Overhead";
static char doc[] = 
"\tThe time needed for the highest priority thread to perform the\n"
"\tpthread_self() operation, for the following numbers of threads:\n"
"\t1, 21, 101, 1023";


static int nthreads = 1;
pthread_t children[1024];

void *
child() {
	pause();
}

void
numthreads(n)
	int n;
{
	int error;
	pthread_attr_t	small_stack_attr;

	pthread_attr_init(&small_stack_attr);
	pthread_attr_setstacksize(&small_stack_attr, PTHREAD_STACK_MIN);

	while (nthreads < n) {
		error = pthread_create(&children[nthreads], 
		    &small_stack_attr, child, NULL);
		if (error != 0)
			errx(1, "pthread_create #%d: %s", nthreads, 
				strerror(error));
		sched_yield();
		nthreads++;
	}

	while (nthreads > n) {
		error = pthread_cancel(children[nthreads - 1]);
		if (error != 0)
			errx(1, "pthread_cancel: %s", strerror(error));
		sched_yield();
		nthreads --;
	}

	printf("\n#threads: %d\n", nthreads);
}

void
doit(b, n)
	bench_t *b;
	int n;
{

	numthreads(n);
	bench_amortize(b, BENCH_LOOPS) {
		pthread_self();
	}
	bench_report(b);
}

int
main() {
	bench_t b;

	bench_init(&b, name, doc, "per call");
	bench_header(&b);

	doit(&b, 1);
	doit(&b, 21);
	doit(&b, 101);
	doit(&b, 1023);
	exit(0);
}


