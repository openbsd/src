/*	$OpenBSD: server.c,v 1.10 2005/04/03 17:32:50 xsa Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <paths.h>
#include <sysexits.h>

#include "cvs.h"
#include "log.h"
#include "proto.h"


/* argument vector built by the `Argument' and `Argumentx' requests */
char   **cvs_args;
u_int   cvs_nbarg = 0;
u_int   cvs_utf8ok = 0;
u_int   cvs_case   = 0;

struct cvs_cmd_info cmd_server = {
	NULL, NULL, NULL, NULL, NULL, 0, 0, 0 };

/*
 * cvs_server()
 *
 * Implement the `cvs server' command.  As opposed to the general method of
 * CVS client/server implementation, the cvs program merely acts as a
 * redirector to the cvs daemon for most of the tasks.
 *
 * The `cvs server' command is only used on the server side of a remote
 * cvs command.  With this command, the cvs program starts listening on
 * standard input for CVS protocol requests.
 */
int
cvs_server(int argc, char **argv)
{
	size_t len;
	char reqbuf[512];

	if (argc != 1) {
		return (EX_USAGE);
	}

	/* make sure standard in and standard out are line-buffered */
	(void)setvbuf(stdin, NULL, _IOLBF, 0);
	(void)setvbuf(stdout, NULL, _IOLBF, 0);

	for (;;) {
		if (fgets(reqbuf, sizeof(reqbuf), stdin) == NULL) {
			if (feof(stdin))
				break;
			else if (ferror(stdin))
				return (-1);
		}

		len = strlen(reqbuf);
		if (len == 0)
			continue;
		else if (reqbuf[len - 1] != '\n') {
			cvs_log(LP_ERR, "truncated request");
			return (-1);
		}
		reqbuf[--len] = '\0';

		cvs_req_handle(reqbuf);


	}

	return (0);
}
