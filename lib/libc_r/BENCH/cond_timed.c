#include <pthread.h>
#include <string.h>
#include <err.h>
#include "bench.h"

static char name[] =  "Time of Wakeup After Timed Wait";
static char doc[] = 
"\tThe tiem required for the highest-priority thread to rresume\n"
"\texecution after a call to pthread_cond_timedwait(). Metrics\n"
"\tare provided for both the cases when the pthread_cond_timedwait()\n"
"\tcall is awakened by a call to pthread_cond_signal() and when\n"
"\tthe absolute time to be awaited has already passed at the time\n"
"\tof the call.";

pthread_mutex_t	m1, m2;
pthread_cond_t	c;
bench_t	b;
struct timespec waketime;

void *
other_thread(arg)
	void *arg;
{

	pthread_set_name_np(pthread_self(), "oth");
	pthread_mutex_lock(&m2);
	
	bench_amortize(&b, BENCH_LOOPS) {
		pthread_cond_timedwait(&c, &m2, &waketime);
		pthread_cond_signal(&c);
	}
	pthread_mutex_unlock(&m2);
}

int
main() {
	pthread_t other;
	pthread_mutex_t m;
	struct timespec ts;

	bench_init(&b, name, doc, "from signal to wait inclusive");
	b.n = BENCH_LOOPS;
	bench_header(&b);
	pthread_cond_init(&c, NULL);
	pthread_mutex_init(&m1, NULL);
	pthread_mutex_init(&m2, NULL);

	clock_gettime(CLOCK_REALTIME, &waketime);
	waketime.tv_sec += 10000; /* shouldn't take this long! */
	pthread_mutex_lock(&m1);

	pthread_create(&other, NULL, other_thread, NULL);
	pthread_yield();
	while (b.i < b.n) {
		pthread_cond_signal(&c);
		pthread_cond_timedwait(&c, &m1, &waketime);
	}
	pthread_join(other, NULL);
	pthread_mutex_unlock(&m1);

	b.divisor = 2;
	bench_report(&b);

	/* expired test */
	bench_init(&b, NULL, NULL, "per call when already expired");
	pthread_mutex_init(&m, NULL);
	pthread_mutex_lock(&m);
	timespecclear(&ts);
	bench_amortize(&b, BENCH_LOOPS) {
		pthread_cond_timedwait(&c, &m, &ts);
	}
	pthread_mutex_unlock(&m);
	bench_report(&b);

	exit(0);
}


