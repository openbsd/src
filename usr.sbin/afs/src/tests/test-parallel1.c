/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <unistd.h>

#include <err.h>
#include <roken.h>

#include <atypes.h>
#include <kafs.h>

#ifdef RCSID
RCSID("$KTH: test-parallel1.c,v 1.3 2000/10/03 00:36:05 lha Exp $");
#endif

#define WORKER_TIMES 100
#define NUM_WORKER 10

static int
worker (int num)
{
    int i, fd;

    for (i = 0 ; i < WORKER_TIMES ; i++) {
	fd = open ("foo", O_CREAT|O_RDWR, 0600);
	if (fd >= 0) {
	    fchmod (fd, 0700);
	    close (fd);
	}
	unlink("foo");
	if (i % 1000) {
	    printf (" %d", num);
	    fflush (stdout);
	}
    }
    return 0;
}


int
main(int argc, char **argv)
{
    int i, ret;
    
    set_progname (argv[0]);

    for (i = 0; i < NUM_WORKER ; i++) {
	int ret;
	
	ret = fork();
	switch (ret) {
	case 0:
	    return worker(i);
	case -1:
	    err (1, "fork");
	}
    }
    i = NUM_WORKER;
    while (i && wait (&ret)) {
	i--;
	if (ret)
	    err (1, "wait: %d", ret);
    }
    return 0;
}
