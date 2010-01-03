/*	$OpenBSD: setsockopt3a.c,v 1.2 2010/01/03 23:02:34 fgsch Exp $	*/
/*
 * Federico G. Schwindt <fgsch@openbsd.org>, 2009. Public Domain.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <err.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "test.h"

static void
alarm_handler(int sig)
{
	_exit(NOTOK);
}

int
check_timeout(int s, int sec, struct timeval *to)
{
	struct timeval t1, t2;
	struct timeval e, d;
	char buf[BUFSIZ];

	ASSERT(signal(SIGALRM, alarm_handler) != SIG_ERR);
	CHECKe(alarm(sec));
	CHECKe(gettimeofday(&t1, NULL));
	ASSERT(read(s, &buf, sizeof(buf)) == -1);
	CHECKe(gettimeofday(&t2, NULL));
	ASSERT(errno == EAGAIN);
	timersub(&t2, &t1, &e);
	timersub(&e, to, &d);
	return ((d.tv_sec > 1 || (d.tv_usec / 1000) > 100) ? 1 : 0);
}

static void *
sock_accept(void *arg)
{
	struct sockaddr_in sin;
	struct timeval to;
	int s, s2, s3;

	CHECKe(s = strtol(arg, NULL, 10));
	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);
	sin.sin_port = htons(6543);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	CHECKe(connect(s, (struct sockaddr *)&sin, sizeof(sin)));
	to.tv_sec = 2;
	to.tv_usec = 0.5 * 1e6;
	CHECKr(check_timeout(s, 3, &to));
	CHECKe(s2 = dup(s));
	CHECKe(s3 = fcntl(s, F_DUPFD, s));
	CHECKr(check_timeout(s2, 3, &to));
	CHECKr(check_timeout(s3, 3, &to));
	return (NULL);
}

int
main(int argc, char **argv)
{
	pthread_t accept_thread;

	if (argc != 2)
		exit(NOTOK);
	CHECKr(pthread_create(&accept_thread, NULL, sock_accept, argv[1]));
	CHECKr(pthread_join(accept_thread, NULL));
	SUCCEED;
}
