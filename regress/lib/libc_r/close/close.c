/*	$OpenBSD: close.c,v 1.2 2001/09/20 16:43:15 todd Exp $	*/
/*
 * Copyright (c) 1993, 1994, 1995, 1996 by Chris Provenzano and contributors, 
 * proven@mit.edu All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Chris Provenzano,
 *	the University of California, Berkeley, and contributors.
 * 4. Neither the name of Chris Provenzano, the University, nor the names of
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CHRIS PROVENZANO, THE REGENTS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */ 

/*
 * Test the semantics of close() while a select() is happening.
 * Not a great test. You need the 'discard' service running in inetd for
 * this to work.
 */

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
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
	int ret;

	/* Open up a TCP connection to the local discard port */
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(9);	/* port 9/tcp is discard service */

	CHECKe(fd = socket(AF_INET, SOCK_STREAM, 0));
	printf("main: connecting to discard port with fd %d\n", fd);
	ret = connect(fd, (struct sockaddr *)&addr, sizeof addr);
	if (ret == -1)
		fprintf(stderr, "connect() failed: ensure that the discard port is enabled for inetd(8)\n");
	CHECKe(ret);
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
