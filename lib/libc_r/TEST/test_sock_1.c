/*	$OpenBSD: test_sock_1.c,v 1.4 2000/01/06 06:58:34 d Exp $	*/
/* ==== test_sock_1.c =========================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test pthread_create() and pthread_exit() calls.
 *
 *  1.00 93/08/03 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "test.h"
#include <sched.h>
#include <string.h>
#include <stdlib.h>

struct sockaddr_in a_sout;
int success = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_attr_t attr;

static int counter = 0;

void *
sock_connect(arg)
	void *arg;
{
	char buf[1024];
	int fd;

	/* Ensure sock_read runs first */
	CHECKr(pthread_mutex_lock(&mutex));

	a_sout.sin_addr.s_addr = htonl(0x7f000001); /* loopback */
	CHECKe(fd = socket(AF_INET, SOCK_STREAM, 0));

	ASSERT(++counter == 2);

	/* connect to the socket */
	CHECKe(connect(fd, (struct sockaddr *) &a_sout, sizeof(a_sout)));
	CHECKe(close(fd));

	CHECKr(pthread_mutex_unlock(&mutex));

	CHECKe(fd = socket(AF_INET, SOCK_STREAM, 0));
	ASSERT(++counter == 3);
	CHECKe(connect(fd, (struct sockaddr *) &a_sout, sizeof(a_sout)));

	/* Ensure sock_read runs again */
	pthread_yield();
	sleep(1);

	CHECKr(pthread_mutex_lock(&mutex));
	CHECKe(read(fd, buf, 1024));

	write(fd, "6", 1);

	ASSERT(++counter == atoi(buf));
	CHECKe(close(fd));
	success++;
	CHECKr(pthread_mutex_unlock(&mutex));

	return(NULL);
}

void *
sock_write(arg)
	void *arg;
{
	int fd = *(int *)arg;

	CHECKe(write(fd, "5", 1));
	return(NULL);
}

void *
sock_accept(arg)
	void *arg;
{
	pthread_t thread;
	struct sockaddr a_sin;
	int a_sin_size, a_fd, fd;
	short port;
	char buf[1024];

	port = 3276;
	a_sout.sin_family = AF_INET;
	a_sout.sin_port = htons(port);
	a_sout.sin_addr.s_addr = INADDR_ANY;

	CHECKe(a_fd = socket(AF_INET, SOCK_STREAM, 0));

	while (1) {
		if(0 == bind(a_fd, (struct sockaddr *) &a_sout, sizeof(a_sout)))
			break;
		if (errno == EADDRINUSE) { 
			a_sout.sin_port = htons((++port));
			continue;
		}
		DIE(errno, "bind");
	}
	CHECKe(listen(a_fd, 2));

	ASSERT(++counter == 1);

	CHECKr(pthread_create(&thread, &attr, sock_connect, 
	    (void *)0xdeadbeaf));

	a_sin_size = sizeof(a_sin);
	CHECKe(fd = accept(a_fd, &a_sin, &a_sin_size));
	CHECKr(pthread_mutex_lock(&mutex));
	CHECKe(close(fd));

	ASSERT(++counter == 4);

	a_sin_size = sizeof(a_sin);
	CHECKe(fd = accept(a_fd, &a_sin, &a_sin_size));
	CHECKr(pthread_mutex_unlock(&mutex));

	/* Setup a write thread */
	CHECKr(pthread_create(&thread, &attr, sock_write, &fd));
	CHECKe(read(fd, buf, 1024));

	ASSERT(++counter == atoi(buf));

	CHECKe(close(fd));

	CHECKr(pthread_mutex_lock(&mutex));
	success++;
	CHECKr(pthread_mutex_unlock(&mutex));

	CHECKr(pthread_join(thread, NULL));
	return(NULL);
}

int
main()
{
	pthread_t thread;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	CHECKr(pthread_attr_init(&attr));
#if 0
	CHECKr(pthread_attr_setschedpolicy(&attr, SCHED_FIFO));
#endif
	CHECKr(pthread_create(&thread, &attr, sock_accept,
	    (void *)0xdeadbeaf));

	CHECKr(pthread_join(thread, NULL));

	ASSERT(success == 2);
	SUCCEED;
}
