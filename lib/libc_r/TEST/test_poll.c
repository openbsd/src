/*	$OpenBSD: test_poll.c,v 1.2 2000/01/06 06:55:37 d Exp $	*/
#include <pthread.h>
#include <fcntl.h>
#include <poll.h>
#include <paths.h>
#include <unistd.h>
#include "test.h"


#define POLLALL	(POLLIN|POLLOUT|POLLERR|POLLNVAL)

static void
print_pollfd(p)
	struct pollfd *p;
{

	printf("{fd=%d, events=< %s%s%s> revents=< %s%s%s%s%s>}",
		p->fd,
		p->events & POLLIN ? "in " : "",
		p->events & POLLOUT ? "out " : "",
		p->events & ~(POLLIN|POLLOUT) ? "XXX " : "",
		p->revents & POLLIN ? "in " : "",
		p->revents & POLLOUT ? "out " : "",
		p->revents & POLLERR ? "err " : "",
		p->revents & POLLHUP ? "hup " : "",
		p->revents & POLLNVAL ? "nval " : ""
	);
}

static void *
writer(arg)
	void *arg;
{
	int fd = (int)arg;
	const char msg[1] = { '!' };

	ASSERTe(write(fd, &msg, sizeof msg), == sizeof msg);
	return NULL;
}

static void *
reader(arg)
	void *arg;
{
	int fd = (int)arg;
	char buf[1];

	ASSERTe(read(fd, &buf, sizeof buf), == sizeof buf);
	return NULL;
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	pthread_t t;
	void *result;
	int null, zero, tty;
	int tube[2];
	struct pollfd p[3];

	/* Try an empty poll set */
	ASSERTe(poll(NULL, 0, 0), == 0);

	CHECKe(zero = open(_PATH_DEV "zero", O_RDONLY));
	CHECKe(null = open(_PATH_DEV "null", O_WRONLY));
	CHECKe(tty  = open(_PATH_DEV "tty", O_WRONLY));

	/* Try both descriptors being ready */
	p[0].fd = zero;
	p[0].events = POLLIN|POLLOUT;
	p[0].revents = 0;
	p[1].fd = null;
	p[1].events = POLLIN|POLLOUT;
	p[1].revents = 0;

	ASSERTe(poll(p, 2, 0), == 2);	/* if 4 then bug in kernel not fixed */
	printf("zero p[0]="); print_pollfd(&p[0]); putchar('\n');
	printf("null p[1]="); print_pollfd(&p[1]); putchar('\n');
	ASSERT((p[0].revents & POLLIN) == POLLIN);
	ASSERT((p[1].revents & POLLOUT) == POLLOUT);

	/*
	 * Try one of the descriptors being invalid
	 * and the other ready
	 */
	printf("closing zero\n");
	close(zero);

	p[0].fd = zero;
	p[0].events = POLLIN|POLLOUT;
	p[1].fd = null;
	p[1].events = POLLIN|POLLOUT;
	ASSERTe(poll(p, 2, 0), == 2);	/* again, old kernels had this bug */
	printf("zero p[0]="); print_pollfd(&p[0]); putchar('\n');
	printf("null p[1]="); print_pollfd(&p[1]); putchar('\n');
	ASSERT((p[0].revents & POLLNVAL) == POLLNVAL);
	ASSERT((p[1].revents & POLLOUT) == POLLOUT);

	printf("closing null\n");
	close(null);

	/* 
	 * New pipes. the write end should be writable (buffered)
	 */
	CHECKe(pipe(tube));
	CHECKe(fcntl(tube[0], F_SETFL, O_NONBLOCK));
	CHECKe(fcntl(tube[1], F_SETFL, O_NONBLOCK));

	p[0].fd = tube[0];
	p[0].events = POLLIN;
	p[1].fd = tube[1]; 
	p[1].events = POLLOUT;
	ASSERTe(poll(p, 2, 0), == 1);
	printf("rpipe p[0]="); print_pollfd(&p[0]); putchar('\n');
	printf("wpipe p[1]="); print_pollfd(&p[1]); putchar('\n');
	ASSERT(p[0].revents == 0);
	ASSERT(p[1].revents == POLLOUT);

	/* Start a writing thread to the write end [1] */
	printf("bg writing to wpipe\n");
	CHECKr(pthread_create(&t, NULL, writer, (void *)tube[1]));
	/* The read end [0] should soon be ready for read (POLLIN) */
	p[0].fd = tube[0];
	p[0].events = POLLIN;
	ASSERTe(poll(p, 1, -1), == 1);
	printf("rpipe p[0]="); print_pollfd(&p[0]); putchar('\n');
	ASSERT(p[0].revents == POLLIN);
	reader((void *)tube[0]);	/* consume */
	CHECKr(pthread_join(t, &result));

	SUCCEED;
}
