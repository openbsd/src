/* $OpenBSD: cancel2.c,v 1.1 2003/01/19 21:23:46 marc Exp $ */
/* PUBLIC DOMAIN <marc@snafu.org> */

/*
 * Check that a thread waiting on a select without timeout can be
 * cancelled.
 */

#include <sys/types.h>
#include <sys/time.h>

#include <pthread.h>
#include <unistd.h>

#include "test.h"

void *
select_thread(void *arg)
{
	int read_fd = *(int*) arg;
	fd_set read_fds;
	int result;

	FD_ZERO(&read_fds);
	FD_SET(read_fd, &read_fds);
	result = select(read_fd + 1, &read_fds, NULL, NULL, NULL);
	printf("select returned %d\n", result);
	return 0;
}

int
main(int argc, char *argv[])
{
	pthread_t thread;
	int pipe_fd[2];

	CHECKe(pipe(pipe_fd));
	CHECKr(pthread_create(&thread, NULL, select_thread, pipe_fd));
	sleep(2);
	CHECKr(pthread_cancel(thread));
	CHECKr(pthread_join(thread, NULL));
	SUCCEED;
}
