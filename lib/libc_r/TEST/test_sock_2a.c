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
sock_connect(void* arg)
{
	char buf[1024];
	int fd, tmp;
	short port;

	port = 3276;
 	a_sout.sin_family = AF_INET;
 	a_sout.sin_port = htons(port);
	a_sout.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* loopback */

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
		DIE(errno, "sock_connect:socket()");

	printf("This should be message #2\n");
	if (connect(fd, (struct sockaddr *) &a_sout, sizeof(a_sout)) < 0) 
		DIE(errno, "sock_connect:connect()");
	close(fd); 
		
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		DIE(errno, "sock_connect:socket()");

	printf("This should be message #3\n");

	if (connect(fd, (struct sockaddr *) &a_sout, sizeof(a_sout)) < 0)
		DIE(errno, "sock_connect:connect()");

	/* Ensure sock_read runs again */

	if ((tmp = read(fd, buf, 1024)) <= 0) 
		DIE(errno, "sock_connect:read() == %d\n", tmp);
	write(fd, MESSAGE6, sizeof(MESSAGE6));
	printf("%s\n", buf);
	close(fd);

	return (NULL);
}

int
main(int argc, char **argv)
{
	pthread_t thread;
	int ret;

	if (argv[1] && (!strcmp(argv[1], "fork okay"))) {
		sleep(1);
		setbuf(stdout, NULL);
		setbuf(stderr, NULL);

		if ((ret = pthread_create(&thread, NULL, sock_connect, 
		    (void *)0xdeadbeaf)))
			DIE(ret, "main:pthread_create(sock_connect)");
		pthread_join(thread, NULL);
		exit(0);
	} else {
		printf("test_sock_2a needs to be execed from test_sock_2.\n");
		printf("It is not a stand alone test.\n");
		exit(1);
	}
}
