/*	$OpenBSD: server.c,v 1.26 2005/12/24 19:07:52 xsa Exp $	*/
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
#include <sys/stat.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "log.h"
#include "proto.h"


/* argument vector built by the `Argument' and `Argumentx' requests */
char   **cvs_args;
u_int   cvs_nbarg = 0;
u_int   cvs_utf8ok = 0;
u_int   cvs_case = 0;

struct cvs_cmd cvs_cmd_server = {
	CVS_OP_SERVER, 0, "server",
	{ },
	"Server mode",
	"",
	"",
	NULL,
	0,
	NULL, NULL, NULL, NULL, NULL, NULL,
	0
};

char cvs_server_tmpdir[MAXPATHLEN];

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
	int l, ret;
	size_t len;
	char reqbuf[512];

	if (argc != 1)
		return (CVS_EX_USAGE);

	/* make sure standard in and standard out are line-buffered */
	(void)setvbuf(stdin, NULL, _IOLBF, (size_t)0);
	(void)setvbuf(stdout, NULL, _IOLBF, (size_t)0);

	/* create the temporary directory */
	l = snprintf(cvs_server_tmpdir, sizeof(cvs_server_tmpdir),
	    "%s/cvs-serv%d", cvs_tmpdir, getpid());
	if (l == -1 || l >= (int)sizeof(cvs_server_tmpdir)) {
		errno = ENAMETOOLONG;
		fatal("cvs_server: tmpdir path too long: `%s'",
		    cvs_server_tmpdir);
	}

	if (mkdir(cvs_server_tmpdir, 0700) == -1)
		fatal("cvs_server: mkdir: `%s': %s",
		    cvs_server_tmpdir, strerror(errno));

	cvs_chdir(cvs_server_tmpdir, 1);

	for (;;) {
		if (fgets(reqbuf, (int)sizeof(reqbuf), stdin) == NULL) {
			if (feof(stdin))
				break;
			else if (ferror(stdin)) {
				(void)cvs_rmdir(cvs_server_tmpdir);
				fatal("cvs_server: fgets failed");
			}
		}

		len = strlen(reqbuf);
		if (len == 0)
			continue;
		else if (reqbuf[len - 1] != '\n') {
			(void)cvs_rmdir(cvs_server_tmpdir);
			fatal("cvs_server: truncated request");
		}
		reqbuf[--len] = '\0';

		cvs_req_handle(reqbuf);


	}

	/* cleanup the temporary tree */
	ret = cvs_rmdir(cvs_server_tmpdir);

	return (ret);
}
