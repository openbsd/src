
#define BENCH_LOOPS	(16384)
#include <sys/time.h>

typedef struct  {
	int i;
	int n;
	int divisor;
	struct timespec start;
	struct timespec end;
	struct timespec elapsed;
	double average;
	char *name;
	char *doc;
	char *units;
} bench_t;

#define bench_now(tsp) \
	clock_gettime(CLOCK_REALTIME, (tsp))

/*
 * Repeat the body of the loop 'max' times, with a few extra 'warm up'
 * cycles to negate cache effects.
 */
#define bench_amortize(b, max) 						\
	for ((b)->i = -64,						\
	     (b)->n = (max);						\
	     (b)->i < (b)->n;						\
	     (b)->i ++,							\
	     ((b)->i == 0 ? bench_now(&(b)->start) :			\
	      ((b)->i == (b)->n ? bench_now(&(b)->end)			\
		:0))\
	)

#define bench_init(b, nm, dc, un) do {					\
	(b)->name = (nm);						\
	(b)->doc = (dc);						\
	(b)->units = (un);						\
	timespecclear(&(b)->start);					\
	timespecclear(&(b)->end);					\
	timespecclear(&(b)->elapsed);					\
	(b)->n = (b)->i = 0;						\
	(b)->divisor = 1;						\
} while (0)
	
#define bench_header(b)							\
	printf("----------------------------------------------------\n" \
	       "Name:\t%s\nDesc:%s\n",	(b)->name, (b)->doc)

#define bench_report(b) do {						\
	struct timespec elapsed;					\
	bench_t overhead;						\
									\
	/* compute the loop overhead */					\
	bench_amortize(&overhead, BENCH_LOOPS) { /* nothing */ }	\
									\
	timespecsub(&(b)->end, &(b)->start, &(b)->elapsed);		\
	(b)->average = ((double)(b)->elapsed.tv_sec * 1000000000.0 +	\
	    (b)->elapsed.tv_nsec) / (double)((b)->divisor) / 		\
	    (double)((b)->n);						\
									\
	printf("Time: %f usec %s\n", (b)->average, (b)->units);		\
	if ((b)->divisor != 1)						\
		printf("\t(%d operations per cycle)\n", (b)->divisor);	\
} while (0)
