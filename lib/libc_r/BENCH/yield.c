#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <err.h>
#include "bench.h"

static char name[] =  "Thread Yield Time (Busy)";
static char doc[] = 
"\tThread yield time is defined as the amount of time between that\n"
"\tpoint when a running thread voluntarily gives up the CPU until\n"
"\tthe highest priority runnable thread begins execution of its\n"
"\tapplication code.";

#ifdef DEBUG
volatile int state = 0;
#endif
bench_t	b;

void *
other_thread(arg)
	void *arg;
{

	pthread_set_name_np(pthread_self(), "oth");
	bench_amortize(&b, BENCH_LOOPS) {
#ifdef DEBUG
		if (state != 0)	abort();
		state = 1;
#endif
		sched_yield();
	}
}

int
main() {
	pthread_t other;

	bench_init(&b, name, doc, "per yield");
	b.n = BENCH_LOOPS;
	bench_header(&b);
	pthread_create(&other, NULL, other_thread, NULL);
	while (b.i < b.n) {
#ifdef DEBUG
		if (state != 1) abort();
		state = 0;
#endif
		sched_yield();
	}
	pthread_join(other, NULL);
	b.divisor = 2;
	bench_report(&b);
	exit(0);
}


