/*	$OpenBSD: commit.c,v 1.2 2004/07/30 01:49:22 jfb Exp $	*/
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
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sysexits.h>

#include "cvs.h"
#include "log.h"
#include "proto.h"




static char*  cvs_commit_openmsg   (const char *);




/*
 * cvs_commit()
 *
 * Handler for the `cvs commit' command.
 */

int
cvs_commit(int argc, char **argv)
{
	int ch, recurse;
	char *msg, *mfile;

	recurse = 1;
	mfile = NULL;
	msg = NULL;

	while ((ch = getopt(argc, argv, "F:flm:R")) != -1) {
		switch (ch) {
		case 'F':
			mfile = optarg;
			break;
		case 'f':
			recurse = 0;
			break;
		case 'l':
			recurse = 0;
			break;
		case 'm':
			msg = optarg;
			break;
		case 'R':
			recurse = 1;
			break;
		default:
			return (EX_USAGE);
		}
	}

	if ((msg != NULL) && (mfile != NULL)) {
		cvs_log(LP_ERR, "the -F and -m flags are mutually exclusive");
		return (EX_USAGE);
	}

	if ((mfile != NULL) && (msg = cvs_commit_openmsg(mfile)) == NULL)
		return (EX_DATAERR);

	argc -= optind;
	argv += optind;

	return (0);
}


/*
 * cvs_commit_openmsg()
 *
 * Open the file specified by <path> and allocate a buffer large enough to
 * hold all of the file's contents.  The returned value must later be freed
 * using the free() function.
 * Returns a pointer to the allocated buffer on success, or NULL on failure.
 */

static char*
cvs_commit_openmsg(const char *path)
{
	int fd;
	size_t sz;
	char *msg;
	struct stat st;

	if (stat(path, &st) == -1) {
		cvs_log(LP_ERRNO, "failed to stat `%s'", path);
		return (NULL);
	}

	sz = st.st_size + 1;

	msg = (char *)malloc(sz);
	if (msg == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate message buffer");
		return (NULL);
	}

	fd = open(path, O_RDONLY, 0);
	if (fd == -1) {
		cvs_log(LP_ERRNO, "failed to open message file `%s'", path);
		return (NULL);
	}

	if (read(fd, msg, sz - 1) == -1) {
		cvs_log(LP_ERRNO, "failed to read CVS commit message");
		return (NULL);
	}
	msg[sz - 1] = '\0';

	return (msg);
}
