/*	$OpenBSD: sem_timedwait.c,v 1.3 2018/04/27 11:31:17 pirofti Exp $	*/
/*
 * Martin Pieuchot <mpi@openbsd.org>, 2011. Public Domain.
 */

#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <semaphore.h>
#include <signal.h>
#include <pthread.h>
#include "test.h"


void *waiter(void *arg);

void
handler(int sig)
{
	static char message[] = "got sig\n";

	write(STDERR_FILENO, message, sizeof(message) - 1);
}

sem_t sem;
volatile int posted = 0, eintr_ok = 0;

int
main(int argc, char **argv)
{
	pthread_t th;
	struct sigaction sa;
	struct timespec ts, ts2;

	CHECKr(clock_gettime(CLOCK_REALTIME, &ts));
	ts.tv_sec += 3;
	CHECKn(sem_timedwait(&sem, &ts));
	ASSERT(errno == EINVAL);

	CHECKr(sem_init(&sem, 0, 0));

	CHECKr(pthread_create(&th, NULL, waiter, &sem));

	sleep(1);

	printf("expect: sem_destroy on semaphore with waiters!\n");
	CHECKn(sem_destroy(&sem));
	ASSERT(errno == EBUSY);

	posted = 1;
	CHECKr(sem_post(&sem));
	CHECKr(pthread_join(th, NULL));

	/* test that sem_timedwait() resumes after handling a signal */
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = &handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGUSR1, &sa, NULL))
		err(1, "sigaction");
	posted = 0;
	CHECKr(pthread_create(&th, NULL, waiter, &sem));
	sleep(1);
	fprintf(stderr, "sending sig\n");
	eintr_ok = 1;
	pthread_kill(th, SIGUSR1);
	sleep(1);
	fprintf(stderr, "posting\n");
	posted = 1;
	eintr_ok = 0;
	CHECKr(sem_post(&sem));
	CHECKr(pthread_join(th, NULL));

	CHECKr(clock_gettime(CLOCK_REALTIME, &ts));
	ts.tv_sec += 2;
	CHECKn(sem_timedwait(&sem, &ts));
	ASSERT(errno == ETIMEDOUT);
	CHECKr(clock_gettime(CLOCK_REALTIME, &ts2));
	if (timespeccmp(&ts, &ts2, < ))
		timespecsub(&ts2, &ts, &ts);
	else
		timespecsub(&ts, &ts2, &ts);
	CHECKr(clock_getres(CLOCK_REALTIME, &ts2));
	timespecadd(&ts2, &ts2, &ts2);	/* 2 * resolution slop */
	ASSERT(timespeccmp(&ts, &ts2, < ));

	CHECKe(sem_destroy(&sem));

	SUCCEED;
}

void *
waiter(void *arg)
{
	sem_t *semp = arg;
	struct timespec ts;
	int value;
	int r;

	CHECKr(clock_gettime(CLOCK_REALTIME, &ts));
	ts.tv_sec += 3;
	r = sem_timedwait(semp, &ts);
	CHECKr(sem_getvalue(semp, &value));
	if (r == 0) {
		ASSERT(value == 0);
		ASSERT(posted != 0);
	} else {
		ASSERT(r == -1);
		ASSERT(errno == EINTR);
		ASSERT(eintr_ok);
		if (posted)
			ASSERT(value == 1);
		else
			ASSERT(value == 0);
	}

	return (NULL);
}
