
#include  <pthread.h>
#include "bench.h"

static char name[] =  "Once Overhead";
static char doc[] = 
"\tThe time needed for the highest priority thread to execute the\n"
"\tpthread_once() function when the init_routine has already been\n"
"\texecuted.";

void
init_routine()
{
}

int
main() {
	pthread_once_t once_control = PTHREAD_ONCE_INIT;
	bench_t b;
	bench_init(&b, name, doc, "per call");
	bench_header(&b);
	pthread_once(&once_control, init_routine);
	bench_amortize(&b, BENCH_LOOPS) {
		pthread_once(&once_control, init_routine);
	}
	bench_report(&b);
	exit(0);
}


