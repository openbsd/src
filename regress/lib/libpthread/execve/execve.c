/*	$OpenBSD: execve.c,v 1.4 2003/09/18 01:59:58 henning Exp $	*/
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
 * Copyright (c) 1994 by Chris Provenzano, proven@athena.mit.edu
 *
 * Test execve() and dup2() calls.
 */

#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include "test.h"

extern char **environ;
char *new_argv[] = {
	"/bin/echo",
	"This line should appear after the execve",
	NULL
};

char * should_succeed = "This line should be displayed\n";

int
main(int argc, char *argv[])
{
	int fd;

	printf("This is the first message\n");
	if (isatty(STDOUT_FILENO)) {
		char *ttynm;

		CHECKn(ttynm = ttyname(STDOUT_FILENO));
		printf("tty is %s\n", ttynm);
		CHECKe(fd = open(ttynm, O_RDWR));
	} else {
		printf("IGNORED: stdout is not a tty: this test needs a tty\n");
		SUCCEED;
	}

	CHECKn(printf("This output is necessary to set the stdout fd to NONBLOCKING\n"));

	/* do a dup2 */
	CHECKe(dup2(fd, STDOUT_FILENO));
	CHECKe(write(STDOUT_FILENO, should_succeed,
	    (size_t)strlen(should_succeed)));
	CHECKe(execve(new_argv[0], new_argv, environ));
	DIE(errno, "execve %s", new_argv[0]);
}
