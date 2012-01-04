/*	$OpenBSD: sem_trywait.c,v 1.1.1.1 2012/01/04 17:36:40 mpi Exp $	*/
/*
 * Martin Pieuchot <mpi@openbsd.org>, 2011. Public Domain.
 */

#include <errno.h>
#include <unistd.h>
#include <semaphore.h>
#include "test.h"

int
main(int argc, char **argv)
{
	sem_t sem;
	int val;

	CHECKn(sem_trywait(&sem));
	ASSERT(errno == EINVAL);

	CHECKr(sem_init(&sem, 0, 0));

	CHECKn(sem_trywait(&sem));
	ASSERT(errno == EAGAIN);

	CHECKr(sem_post(&sem));
	CHECKr(sem_trywait(&sem));

	CHECKe(sem_getvalue(&sem, &val));
	ASSERT(val == 0);

	CHECKe(sem_destroy(&sem));

	SUCCEED;
}
