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

void * sock_write(void* arg)
{
	int fd = *(int *)arg;

	write(fd, MESSAGE5, sizeof(MESSAGE5));
	return(NULL);
}

void * sock_accept(void* arg)
{
	pthread_t thread;
	struct sockaddr a_sin;
	int a_sin_size, a_fd, fd, tmp;
	short port;
	char buf[1024];

	port = 3276;
	a_sout.sin_family = AF_INET;
	a_sout.sin_port = htons(port);
	a_sout.sin_addr.s_addr = INADDR_ANY;

	if ((a_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
		DIE(errno, "sock_accept:socket()");

	while (bind(a_fd, (struct sockaddr *) &a_sout, sizeof(a_sout)) < 0) {
		if (errno == EADDRINUSE) { 
			a_sout.sin_port = htons((++port));
			continue;
		}
		DIE(errno, "sock_accept:bind()");
	}

	if (listen(a_fd, 2)) 
		DIE(errno, "sock_accept:listen()");
		
	a_sin_size = sizeof(a_sin);
	printf("This should be message #1\n");
	if ((fd = accept(a_fd, &a_sin, &a_sin_size)) < 0) 
		DIE(errno, "Error: sock_accept:accept()");
	close(fd); 
	sleep(1);

	a_sin_size = sizeof(a_sin);
	memset(&a_sin, 0, sizeof(a_sin));
	printf("This should be message #4\n");
	if ((fd = accept(a_fd, &a_sin, &a_sin_size)) < 0)
		DIE(errno, "sock_accept:accept()");

	/* Setup a write thread */
	if (pthread_create(&thread, NULL, sock_write, &fd)) 
		DIE(errno, "sock_accept:pthread_create(sock_write)");
	if ((tmp = read(fd, buf, 1024)) <= 0)
		DIE(errno, "Error: sock_accept:read() == %d", tmp);

	printf("%s\n", buf);
	close(fd);
	return(NULL);
}

int
main()
{
	pthread_t thread;
	int ret;

	switch(fork()) {
	case -1:
		DIE(errno, "main:fork()");

	case 0:
		execl("test_sock_2a", "test_sock_2a", "fork okay", NULL);
		DIE(errno, "execl");
	default:
		break;
	}

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	if ((ret = pthread_create(&thread, NULL, sock_accept, 
	    (void *)0xdeadbeaf)))
		DIE(ret, "main:pthread_create(sock_accept)");

	if ((ret = pthread_join(thread, NULL)))
		DIE(ret, "main:pthread_join()");

	return (0);
}
