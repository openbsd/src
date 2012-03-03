/*	$OpenBSD: sem_wait.c,v 1.2 2012/03/03 09:36:26 guenther Exp $	*/
/*
 * Martin Pieuchot <mpi@openbsd.org>, 2011. Public Domain.
 */

#include <errno.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include "test.h"


void *waiter(void *arg);

sem_t sem;

int
main(int argc, char **argv)
{
	pthread_t th;

	CHECKn(sem_wait(&sem));
	ASSERT(errno == EINVAL);

	CHECKr(sem_init(&sem, 0, 0));

	CHECKr(pthread_create(&th, NULL, waiter, &sem));

	sleep(1);

	CHECKn(sem_destroy(&sem));
	ASSERT(errno == EBUSY);

	CHECKr(sem_post(&sem));
	CHECKr(pthread_join(th, NULL));

	CHECKe(sem_destroy(&sem));

	SUCCEED;
}

void *
waiter(void *arg)
{
	sem_t *semp = arg;

	CHECKr(sem_wait(semp));

	return (NULL);
}
