#include <pthread.h>
#include <string.h>
#include <err.h>
#include "bench.h"

static char name[] =  "Mutex Lock/Unlock, No Contention";
static char doc[] = 
"\tThis is the time interval needed to call pthread_mutex_lock()\n"
"\tfollowed immediately by pthread_mutex_unlock() on a mutex that\n"
"\tis unowned and which is only being used by the thread doing\n"
"\tthe test.";

int
main() {
	pthread_mutex_t m;
	bench_t b;

	bench_init(&b, name, doc, "from lock to unlock inclusive");
	bench_header(&b);
	pthread_mutex_init(&m, NULL);
	bench_amortize(&b, BENCH_LOOPS) {
		pthread_mutex_lock(&m);
		pthread_mutex_unlock(&m);
	}
	bench_report(&b);
	exit(0);
}


