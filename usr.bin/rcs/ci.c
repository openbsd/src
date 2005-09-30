/*	$OpenBSD: ci.c,v 1.2 2005/09/30 16:49:37 joris Exp $	*/
/*
 * Copyright (c) 2005 Niall O'Higgins <niallo@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following cinditions
 * are met:
 *
 * 1. Redistributions of source cide must retain the above cipyright
 *    notice, this list of cinditions and the following disclaimer.
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include "log.h"
#include "rcs.h"
#include "rcsprog.h"

extern char *__progname;

void
checkin_usage(void)
{
	fprintf(stderr,
	    "usage: %s [-jlMNqu] [-d date | -r rev] [-m msg] [-k mode] "
	    "file ...\n", __progname);
}

/*
 * checkin_main()
 *
 * Handler for the `ci' program.
 * Returns 0 on success, or >0 on error.
 */
/*
Options:

-r | -r[rev]: check in revision rev
-l[rev]:      ", but do co -l
-u[rev]:      ", bt do co -u
-f[rev]:      force a deposit (check in?)
-k[rev]:      ?
-q[rev]:      quiet mode
-i[rev]:      initial check in, errors if RCS file already exists.
-j[rev]:      just checkin and do not initialize, errors if RCS file already exists.
-I[rev]:      user is prompted even if stdin is not a tty
-d[date]:     uses date for checkin dat and time.
-M[rev]:      set modification time on any new working file to be that of the retrieved version.
-mmsg:        msg is the log message, don't start editor. log messages with #are comments.
*/
int
checkin_main(int argc, char **argv)
{
	int i, ch, dflag, flags, lkmode;
	mode_t fmode;
	RCSFILE *file;
	char fpath[MAXPATHLEN];
	char *rcs_msg, *rev;

	lkmode = -1;
	flags = RCS_RDWR;
	file = NULL;
	rev = rcs_msg = NULL;
	fmode = dflag = 0;

	while ((ch = getopt(argc, argv, "j:l:M:N:qu:d:r::m:k:")) != -1) {
		switch (ch) {
		case 'h':
			(usage)();
			exit(0);
		case 'l':
			lkmode = RCS_LOCK_STRICT;
			break;
		case 'm':
			rcs_msg = optarg;
			break;
		case 'r':
			rev = optarg;
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
		default:
			(usage)();
			exit(1);
		}
	}

	argc -= optind;
	argv += optind;
	if (argc == 0) {
		cvs_log(LP_ERR, "no input file");
		(usage)();
		exit(1);
	}

	for (i = 0; i < argc; i++) {
		if (rcs_statfile(argv[i], fpath, sizeof(fpath)) < 0)
			continue;

		flags = RCS_RDWR;
		file = rcs_open(fpath, flags, fmode);
		if (file == NULL) {
			exit(1);
		}
		if (rcs_msg == NULL) {
			cvs_log(LP_ERR, "no log message");
			exit(1);
		}
		if (rev != NULL ) {
		 /* XXX */
		} else {
			if (dflag) {
				/* XXX */
			} else {
				if (rcs_rev_add(file, RCS_HEAD_REV, rcs_msg, -1)
				    != 0) {
					cvs_log(LP_ERR,
					    "rcs_rev_add() failure");
					exit(1);
				}
			}
		}
		rcs_close(file);
	}

	exit(0);
}
