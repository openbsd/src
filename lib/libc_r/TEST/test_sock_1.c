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

struct sockaddr_in a_sout;
int success = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_attr_t attr;

#define MESSAGE5 "This should be message #5"
#define MESSAGE6 "This should be message #6"

void * sock_connect(void* arg)
{
	char buf[1024];
	int fd, tmp;
	int ret;

	/* Ensure sock_read runs first */
	if ((ret = pthread_mutex_lock(&mutex))) 
		DIE(ret, "sock_connect:pthread_mutex_lock()");

	a_sout.sin_addr.s_addr = htonl(0x7f000001); /* loopback */

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		DIE(errno, "sock_connect:socket()");

	printf("This should be message #2\n");
	if (connect(fd, (struct sockaddr *) &a_sout, sizeof(a_sout)) < 0)
		DIE(errno, "sock_connect:connect()");

	close(fd);
		
	if ((ret = pthread_mutex_unlock(&mutex)))
		DIE(ret, "sock_connect:pthread_mutex_lock()");

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		DIE(ret, "sock_connect:socket()");

	printf("This should be message #3\n");

	if (connect(fd, (struct sockaddr *) &a_sout, sizeof(a_sout)) < 0)
		DIE(errno, "sock_connect:connect()");

	/* Ensure sock_read runs again */
	pthread_yield();
	pthread_yield();
	pthread_yield();
	pthread_yield();
	if ((ret = pthread_mutex_lock(&mutex)))
		DIE(ret, "sock_connect:pthread_mutex_lock()");

	if ((tmp = read(fd, buf, 1024)) <= 0)
		DIE(errno, "sock_connect:read() == %d", tmp);

	write(fd, MESSAGE6, sizeof(MESSAGE6));
	printf("%s\n", buf);
	close(fd);
	success++;

	if ((ret = pthread_mutex_unlock(&mutex)))
		DIE(ret, "sock_connect:pthread_mutex_unlock()");

	return(NULL);
}

extern struct fd_table_entry ** fd_table;
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
	int ret;

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

	if ((ret = pthread_create(&thread, &attr, sock_connect, 
	    (void *)0xdeadbeaf)))
		DIE(ret, "sock_accept:pthread_create(sock_connect)");

	if ((fd = accept(a_fd, &a_sin, &a_sin_size)) < 0) 
		DIE(errno, "sock_accept:accept()");
	
	if ((ret = pthread_mutex_lock(&mutex)))
		DIE(ret, "sock_accept:pthread_mutex_lock()");

	close(fd);

	a_sin_size = sizeof(a_sin);
	printf("This should be message #4\n");
	if ((fd = accept(a_fd, &a_sin, &a_sin_size)) < 0)
		DIE(errno, "sock_accept:accept()");

	if ((ret = pthread_mutex_unlock(&mutex)))
		DIE(ret, "sock_accept:pthread_mutex_unlock()");

	/* Setup a write thread */
	if ((ret = pthread_create(&thread, &attr, sock_write, &fd)))
		DIE(ret, "sock_accept:pthread_create(sock_write)");
	if ((tmp = read(fd, buf, 1024)) <= 0)
		DIE(errno, "sock_accept:read() == %d\n", tmp);

	printf("%s\n", buf);
	close(fd);

	if ((ret = pthread_mutex_lock(&mutex)))
		DIE(ret, "sock_accept:pthread_mutex_lock()");
	success++;
	if ((ret = pthread_mutex_unlock(&mutex)))
		DIE(ret, "sock_accept:pthread_mutex_unlock()");
	return(NULL);
}

int
main()
{
	pthread_t thread;
	int ret;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	if ((ret = pthread_attr_init(&attr))) 
		DIE(ret, "main:pthread_attr_init()");
#if 0
	if ((ret = pthread_attr_setschedpolicy(&attr, SCHED_FIFO)))
		DIE(ret, "main:pthread_attr_setschedpolicy()");
#endif
	if ((ret = pthread_create(&thread, &attr, sock_accept,
	    (void *)0xdeadbeaf)))
		DIE(ret, "main:pthread_create(sock_accept)");

	printf("initial thread %p going to sleep\n", pthread_self());
	sleep(2);
	printf("done sleeping. success = %d\n", success);
	exit(success == 2?0:1);
}
