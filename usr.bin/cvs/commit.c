/*	$OpenBSD: commit.c,v 1.3 2004/11/09 20:59:31 krapht Exp $	*/
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


#define CVS_COMMIT_BIGMSG     8000
#define CVS_COMMIT_FTMPL      "/tmp/cvsXXXXXXXXXX"
#define CVS_COMMIT_LOGPREFIX  "CVS:"
#define CVS_COMMIT_LOGLINE \
"----------------------------------------------------------------------"



static char*  cvs_commit_openmsg   (const char *);
static char*  cvs_commit_getmsg   (const char *);


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

#if 0
	cvs_commit_getmsg(".");
#endif

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
	int fd, ch;
	size_t sz;
	char buf[32], *msg;
	struct stat st;

	if (stat(path, &st) == -1) {
		cvs_log(LP_ERRNO, "failed to stat `%s'", path);
		return (NULL);
	}

	if (!S_ISREG(st.st_mode)) {
		cvs_log(LP_ERR, "message file must be a regular file");
		return (NULL);
	}

	if (st.st_size > CVS_COMMIT_BIGMSG) {
		do {
			fprintf(stderr,
			    "The specified message file seems big.  "
			    "Proceed anyways? (y/n) ");
			if (fgets(buf, sizeof(buf), stdin) == NULL) {
				cvs_log(LP_ERRNO,
				    "failed to read from standard input");
				return (NULL);
			}

			sz = strlen(buf);
			if ((sz == 0) || (sz > 2)) {
				continue;
			}

				cvs_log(LP_ERR, "aborted by user");
				return (NULL);
			}

			fprintf(stderr, "Invalid character\n");
		} while (1);
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


/*
 * cvs_commit_getmsg()
 *
 * Get a commit log message by forking the user's editor.
 * Returns the message in a dynamically allocated string on success, NULL on
 * failure.
 */

static char*
cvs_commit_getmsg(const char *dir)
{
	int ret, fd, argc, fds[3];
	char *argv[4], path[MAXPATHLEN], *msg;
	FILE *fp;

	fds[0] = -1;
	fds[1] = -1;
	fds[2] = -1;
	strlcpy(path, CVS_COMMIT_FTMPL, sizeof(path));
	argc = 0;
	argv[argc++] = cvs_editor;
	argv[argc++] = path;
	argv[argc] = NULL;

	if ((fd = mkstemp(path)) == -1) {
		cvs_log(LP_ERRNO, "failed to create temporary file");
		return (NULL);
	}

	fp = fdopen(fd, "w");
	if (fp == NULL) {
		cvs_log(LP_ERRNO, "failed to fdopen");
		exit(1);
	} else {
		fprintf(fp,
		    "\n%s %s\n%s Enter Log.  Lines beginning with `%s' are "
		    "removed automatically\n%s\n%s Commiting in %s\n"
		    "%s\n%s Modified Files:\n",
		    CVS_COMMIT_LOGPREFIX, CVS_COMMIT_LOGLINE,
		    CVS_COMMIT_LOGPREFIX, CVS_COMMIT_LOGPREFIX,
		    CVS_COMMIT_LOGPREFIX, CVS_COMMIT_LOGPREFIX,
		    dir, CVS_COMMIT_LOGPREFIX, CVS_COMMIT_LOGPREFIX);

		/* XXX list files here */

		fprintf(fp, "%s %s\n", CVS_COMMIT_LOGPREFIX,
		    CVS_COMMIT_LOGLINE);
	}
	(void)fflush(fp);
	(void)fclose(fp);

	do {
		ret = cvs_exec(argc, argv, fds);
		if (ret == -1) {
			fprintf(stderr,
			    "Log message unchanged or not specified\n"
			    "a)bort, c)ontinue, e)dit, !)reuse this message "
			    "unchanged for remaining dirs\nAction: () ");

			ret = getchar();
			if (ret == 'a') {
				cvs_log(LP_ERR, "aborted by user");
				break;
			} else if (ret == 'c') {
			} else if (ret == 'e') {
			} else if (ret == '!') {
			}
				
		}
	} while (0);

	(void)close(fd);

	return (msg);
}


/*
 * cvs_commit_gettmpl()
 *
 * Get the template to display when invoking the editor to get a commit
 * message.
 */

cvs_commit_gettmpl(void)
{

}
