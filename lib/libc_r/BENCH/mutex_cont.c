#include <stdio.h>
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <err.h>
#include "bench.h"

static char name[] =  "Mutex Lock/Unlock, Contention";
static char doc[] = 
"\tThis is the time interval between when one thread calls\n"
"\tpthread_mutex_unlock() and another thread that was blocked\n"
"\ton pthread_mutex_lock() returns with the lock held.";

/*

The order of locking looks like this:

	A		B		1  2  3
	===============	===============	== == ==
	lock(2)				   A  
	yield()				   A
			lock(1)		   A  B
			lock(3)		B  A  B
			yield()		B  A  B
	lock(1)				Ba A  B
-------
			unlock(1)	 a A  B
			lock(2)		 a Ab B
	^				A  Ab B
	unlock(2)			A   b B
	lock(3)				A   b Ba
			^		A  B  Ba
			unlock(3)	A  B   a
			lock(1)		Ab B   a
	^				Ab B  A
	unlock(1)			 b B  A
	lock(2)				 b Ba A
			^		B  Ba A
			unlock(2)	B   a A
			lock(3)		B   a Ab
	^				B  A  Ab
	unlock(3)			B  A   b
	lock(1)				Ba A   b
			^		Ba A  B
-------
			unlock(1)	 a A  B
			unlock(3)	 a A
			exit
	^				A  A
	unlock(1)			   A
	unlock(2)
	exit

	In every cycle, there will be 6 transitions and 6 lock/unlock
	pairs.  So, to compute the transition time, we subtract the
	lock/unlock time computed without contention.
*/

static pthread_mutex_t m1, m2, m3;
static bench_t ba, bb;

void *
thread_a(arg)
{
	pthread_set_name_np(pthread_self(), "ta");
	pthread_mutex_lock(&m2);
	sched_yield();

	pthread_mutex_lock(&m1);
	bench_amortize(&ba, BENCH_LOOPS) {
		pthread_mutex_unlock(&m2);
		pthread_mutex_lock(&m3);
		pthread_mutex_unlock(&m1);
		pthread_mutex_lock(&m2);
		pthread_mutex_unlock(&m3);
		pthread_mutex_lock(&m1);
	}
	pthread_mutex_unlock(&m1);
	pthread_mutex_unlock(&m2);
	return (NULL);
}

void *
thread_b(arg)
{
	pthread_set_name_np(pthread_self(), "tb");
	pthread_mutex_lock(&m1);
	pthread_mutex_lock(&m3);
	sched_yield();

	bench_amortize(&bb, BENCH_LOOPS) {
		pthread_mutex_unlock(&m1);
		pthread_mutex_lock(&m2);
		pthread_mutex_unlock(&m3);
		pthread_mutex_lock(&m1);
		pthread_mutex_unlock(&m2);
		pthread_mutex_lock(&m3);
	}
	pthread_mutex_unlock(&m1);
	pthread_mutex_unlock(&m3);
	return (NULL);
}

int
main() {
	pthread_t ta, tb;

	bench_init(&ba, name, doc, "from unlock to lock inclusive");
	bench_init(&bb, NULL, NULL, NULL);

	bench_header(&ba);

	pthread_mutex_init(&m1, NULL);
	pthread_mutex_init(&m2, NULL);
	pthread_mutex_init(&m3, NULL);

	pthread_create(&ta, NULL, thread_a, NULL);
	pthread_create(&tb, NULL, thread_b, NULL);

	pthread_join(ta, NULL);
	pthread_join(tb, NULL);

	ba.divisor = bb.divisor = 6;

	bench_report(&ba);
/*	bench_report(&bb); */
	exit(0);
}

