/*	$OpenBSD: test_sock_2a.c,v 1.4 2000/01/06 06:58:34 d Exp $	*/
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
#include <string.h>
#include "test.h"

struct sockaddr_in a_sout;

#define MESSAGE5 "This should be message #5"
#define MESSAGE6 "This should be message #6"

void * 
sock_connect(arg)
	void *arg;
{
	char buf[1024];
	int fd;
	short port;

	port = 3276;
 	a_sout.sin_family = AF_INET;
 	a_sout.sin_port = htons(port);
	a_sout.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* loopback */

	CHECKe(fd = socket(AF_INET, SOCK_STREAM, 0));

	printf("%d: This should be message #2\n", getpid());

	CHECKe(connect(fd, (struct sockaddr *) &a_sout, sizeof(a_sout))); 
	CHECKe(close(fd)); 
		
	CHECKe(fd = socket(AF_INET, SOCK_STREAM, 0));

	printf("%d: This should be message #3\n", getpid());

	CHECKe(connect(fd, (struct sockaddr *) &a_sout, sizeof(a_sout)));

	/* Ensure sock_read runs again */

	CHECKe(read(fd, buf, 1024));
	CHECKe(write(fd, MESSAGE6, sizeof(MESSAGE6)));

	printf("%d: %s\n", getpid(), buf);

	CHECKe(close(fd));
	return (NULL);
}

int
main(argc, argv)
	int argc;
	char **argv;
{
	pthread_t thread;

	if (argv[1] && (!strcmp(argv[1], "fork okay"))) {
		sleep(1);
		setbuf(stdout, NULL);
		setbuf(stderr, NULL);

		CHECKr(pthread_create(&thread, NULL, sock_connect, 
		    (void *)0xdeadbeaf));
		CHECKr(pthread_join(thread, NULL));
		SUCCEED;
	} else {
		fprintf(stderr, "test_sock_2a needs to be exec'ed from "
		    "test_sock_2.\n");
		fprintf(stderr, "It is not a stand alone test.\n");
		PANIC("usage");
	}
}
