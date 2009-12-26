/*	$OpenBSD: setsockopt1.c,v 1.1 2009/12/26 01:34:18 fgsch Exp $	*/
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
sock_connect(void *arg)
{
	struct sockaddr_in sin;
	struct timeval to;
	int s, s2, s3;

	CHECKe(s = socket(AF_INET, SOCK_STREAM, 0));
	CHECKe(s2 = dup(s));
	CHECKe(s3 = fcntl(s, F_DUPFD, s));
	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);
	sin.sin_port = htons(6543);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	CHECKe(connect(s, (struct sockaddr *)&sin, sizeof(sin)));
	to.tv_sec = 2;
	to.tv_usec = 0.5 * 1e6;
	CHECKe(setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to)));
	CHECKr(check_timeout(s, 3, &to));
	CHECKr(check_timeout(s2, 3, &to));
	CHECKr(check_timeout(s3, 3, &to));
	to.tv_sec = 1;
	to.tv_usec = 0.5 * 1e6;
	CHECKe(setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to)));
	CHECKr(check_timeout(s, 2, &to));
	CHECKr(check_timeout(s2, 2, &to));
	CHECKr(check_timeout(s3, 2, &to));
	return (NULL);
}

static void *
sock_accept(void *arg)
{
	pthread_t connect_thread;
	struct sockaddr_in sin;
	int s;

	CHECKe(s = socket(AF_INET, SOCK_STREAM, 0));
	bzero(&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);
	sin.sin_port = htons(6543);
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	CHECKe(bind(s, (struct sockaddr *)&sin, sizeof(sin)));
	CHECKe(listen(s, 2));

	CHECKr(pthread_create(&connect_thread, NULL, sock_connect, NULL));
	CHECKr(pthread_join(connect_thread, NULL));
	return (NULL);
}

int
main(int argc, char **argv)
{
	pthread_t accept_thread;

	CHECKr(pthread_create(&accept_thread, NULL, sock_accept, NULL));
	CHECKr(pthread_join(accept_thread, NULL));
	SUCCEED;
}
