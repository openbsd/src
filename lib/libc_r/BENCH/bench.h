
/* The default number of cycles per test */
#define BENCH_LOOPS	(100000)

#include <sys/time.h>

typedef struct  {
	int i;				/* loop counter */
	int n;				/* loop maximum */
	int divisor;			/* operations per cycle */
	struct timeval start;		/* start time */
	struct timeval end;		/* end time */
	char *name;			/* benchmark title */
	char *doc;			/* benchmark description */
	char *units;			/* measurement units information */
} bench_t;

#define bench_now(tvp) \
	gettimeofday((tvp),0)

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
	timerclear(&(b)->start);					\
	timerclear(&(b)->end);						\
	(b)->n = (b)->i = 0;						\
	(b)->divisor = 1;						\
} while (0)
	
#define bench_header(b)							\
	printf("----------------------------------------------------\n" \
	       "Name:\t%s\nDesc:%s\n",	(b)->name, (b)->doc)

#define bench_report(b) do {						\
	bench_t overhead;						\
	struct timeval oh_elapsed;					\
	struct timeval elapsed;						\
	struct timeval normal;						\
	double average;							\
									\
	/* compute the loop overhead */					\
	bench_amortize(&overhead, (b)->n) { /* nothing */ }		\
									\
	/* compute the test time */					\
	timersub(&(b)->end, &(b)->start, &elapsed);			\
	timersub(&overhead.end, &overhead.start, &oh_elapsed);		\
	timersub(&elapsed, &oh_elapsed, &normal);			\
									\
	average = ((double)normal.tv_sec * 1000000.0 +			\
	    normal.tv_usec) / (double)((b)->divisor) / 			\
	    (double)((b)->n);						\
									\
	printf("Time: %f usec %s\n", average, (b)->units);		\
} while (0)
