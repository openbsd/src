#include <pthread.h>
#include <string.h>
#include <err.h>
#include "bench.h"

static char name[] =  "Condition Variable, Wake Up";
static char doc[] = 
"\tThis is the amount of time from when one thread calls\n"
"\tpthread_cond_signal() and a thread blocked on that condition\n"
"\tvariable returns from its pthread_cond_wait() call. The condition\n"
"\tand its associated mutex should not be used by any other thread.\n"
"\tMetrics shall be provided for both the case when the\n"
"\tpthread_cond_signal() call is executed under the associated mutex,\n"
"\tas well as not under the mutex.";

pthread_mutex_t	m1, m2;
pthread_cond_t	c;
bench_t	b;

void *
other_thread(arg)
	void *arg;
{

	pthread_set_name_np(pthread_self(), "oth");
	pthread_mutex_lock(&m2);
	
	bench_amortize(&b, BENCH_LOOPS) {
		pthread_cond_wait(&c, &m2);
		pthread_cond_signal(&c);
	}
	pthread_mutex_unlock(&m2);
}

int
main() {
	pthread_t other;
	bench_init(&b, name, doc, "per call");
	b.n = BENCH_LOOPS;
	bench_header(&b);
	pthread_cond_init(&c, NULL);
	pthread_mutex_init(&m1, NULL);
	pthread_mutex_init(&m2, NULL);
	pthread_mutex_lock(&m1);
	pthread_create(&other, NULL, other_thread, NULL);

	pthread_yield();
	while (b.i < b.n) {
		pthread_cond_signal(&c);
		pthread_cond_wait(&c, &m1);
	}

	b.divisor = 2;
	bench_report(&b);
	exit(0);
}


