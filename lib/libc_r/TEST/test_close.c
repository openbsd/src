/*	$OpenBSD: test_close.c,v 1.1 2000/01/06 06:51:20 d Exp $	*/

/*
 * Test the semantics of close() while a select() is happening.
 * Not a great test. You need the 'discard' service running in inetd for
 * this to work.
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "test.h"

int fd;

void* new_thread(void* arg)
{
	fd_set r;
	int ret;
	char garbage[] = "blah blah blah";

	FD_ZERO(&r);
	FD_SET(fd, &r);

	printf("child: writing some garbage to fd %d\n", fd);
	CHECKe(write(fd, garbage, sizeof garbage));
	printf("child: calling select() with fd %d\n", fd);
	CHECKe(ret = select(fd + 1, &r, NULL, NULL, NULL));
	printf("child: select() returned %d\n", ret);
	return NULL;
}

int
main()
{
	pthread_t thread;
	pthread_attr_t attr;
	struct sockaddr_in addr;

	/* Open up a TCP connection to the local discard port */
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(9);	/* port 9/tcp is discard service */

	CHECKe(fd = socket(AF_INET, SOCK_STREAM, 0));
	printf("main: connecting to discard port with fd %d\n", fd);
	CHECKe(connect(fd, (struct sockaddr *)&addr, sizeof addr));
	printf("main: connected on fd %d\n", fd);

	CHECKr(pthread_attr_init(&attr));
	CHECKe(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED));
	printf("starting child thread\n");
	CHECKr(pthread_create(&thread, &attr, new_thread, NULL));
	sleep(1);
	printf("main: closing fd %d\n", fd);
	CHECKe(close(fd));
	printf("main: closed\n");
	sleep(1);
	SUCCEED;
}
