#include <pthread.h>
#include <string.h>
#include <err.h>
#include "bench.h"

static char name[] =  "Condition Variable Signal/Broadcast, No Waiters";
static char doc[] = 
"\tThis is the amount of time needed to execute pthread_cond_signal()\n"
"\tor pthread_cond_broadcast() if there are no threads blocked on\n"
"\tthe condition.";

int
main() {
	pthread_cond_t c;
	bench_t b;

	bench_init(&b, name, doc, "per call of pthread_cond_signal()");
	bench_header(&b);
	pthread_cond_init(&c, NULL);
	bench_amortize(&b, BENCH_LOOPS) {
		pthread_cond_signal(&c);
	}
	bench_report(&b);

	bench_init(&b, NULL, NULL, "per call of pthread_cond_broadcast()");
	pthread_cond_init(&c, NULL);
	bench_amortize(&b, BENCH_LOOPS) {
		pthread_cond_broadcast(&c);
	}
	bench_report(&b);

	exit(0);
}


