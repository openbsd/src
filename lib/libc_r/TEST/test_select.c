/*	$OpenBSD: test_select.c,v 1.4 1999/11/28 12:31:42 d Exp $	*/
/*
 * Rudimentary test of select().
 */

#include <pthread.h>
#include <pthread_np.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include "test.h"

#define NLOOPS 10000

int ntouts = 0;

void *
bg_routine(arg)
	void *arg;
{
	char dot = '.';
	int n;

	SET_NAME("bg");

	/* Busy loop, printing dots */
	for (;;) {
		pthread_yield();
		write(STDOUT_FILENO, &dot, sizeof dot);
		pthread_yield();
		n = NLOOPS;
		while (n-- > 0)
			pthread_yield();
	}
}

void *
fg_routine(arg)
	void *arg;
{
	int	flags;
	int	n;
	fd_set	r;
	int	fd = fileno((FILE *) arg);
	int	tty = isatty(fd);
	int	maxfd;
	int	nb;
	char	buf[128];

	SET_NAME("fg");

	/* Set the file descriptor to non-blocking */
	flags = fcntl(fd, F_GETFL);
	CHECKr(fcntl(fd, F_SETFL, flags | O_NONBLOCK));

	for (;;) {

		/* Print a prompt if it's a tty: */
		if (tty) {
			printf("type something> ");
			fflush(stdout);
		}

		/* Select on the fdesc: */
		FD_ZERO(&r);
		FD_SET(fd, &r);
		maxfd = fd;
		errno = 0;
		CHECKe(n = select(maxfd + 1, &r, (fd_set *) 0, (fd_set *) 0,
				  (struct timeval *) 0));

		if (n > 0) {
			/* Something was ready for read. */
			printf("select returned %d\n", n);
			while ((nb = read(fd, buf, sizeof(buf) - 1)) > 0) {
				printf("read %d: `%.*s'\n", nb, nb, buf);
			}
			printf("last read was %d, errno = %d %s\n", nb, errno,
			       errno == 0 ? "success" : strerror(errno));
			if (nb < 0)
				ASSERTe(errno, == EWOULDBLOCK || 
				    errno == EAGAIN);
			if (nb == 0)
				break;
		} else
			ntouts++;
	}
	printf("read finished\n");
	return (NULL);
}

int
main(argc, argv)
	int argc;
	char *argv[];
{
	pthread_t	bg_thread, fg_thread;
	FILE *		slpr;

	/* Create a fdesc that will block for a while on read: */
	CHECKn(slpr = popen("sleep 2; echo foo", "r"));

	/* Create a busy loop thread that yields a lot: */
	CHECKr(pthread_create(&bg_thread, NULL, bg_routine, 0));

	/* Create the thread that reads the fdesc: */
	CHECKr(pthread_create(&fg_thread, NULL, fg_routine, (void *) slpr));

	/* Wait for the reader thread to finish */
	CHECKr(pthread_join(fg_thread, NULL));

	/* Clean up*/
	CHECKe(pclose(slpr));

	SUCCEED;
}
