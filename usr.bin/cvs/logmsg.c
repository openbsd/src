/*	$OpenBSD: logmsg.c,v 1.1 2004/11/12 17:49:11 jfb Exp $	*/
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
#include <unistd.h>
#include <string.h>

#include "cvs.h"
#include "log.h"
#include "buf.h"


#define CVS_LOGMSG_BIGMSG     32000
#define CVS_LOGMSG_FTMPL      "/tmp/cvsXXXXXXXXXX"
#define CVS_LOGMSG_LOGPREFIX  "CVS:"
#define CVS_LOGMSG_LOGLINE \
"----------------------------------------------------------------------"


/*
 * cvs_logmsg_open()
 *
 * Open the file specified by <path> and allocate a buffer large enough to
 * hold all of the file's contents.  Lines starting with the log prefix
 * are not included in the result.
 * The returned value must later be free()d.
 * Returns a pointer to the allocated buffer on success, or NULL on failure.
 */

char*
cvs_logmsg_open(const char *path)
{
	int lcont;
	size_t len;
	char lbuf[256], *msg;
	struct stat st;
	FILE *fp;
	BUF *bp;

	if (stat(path, &st) == -1) {
		cvs_log(LP_ERRNO, "failed to stat `%s'", path);
		return (NULL);
	}

	if (!S_ISREG(st.st_mode)) {
		cvs_log(LP_ERR, "message file must be a regular file");
		return (NULL);
	}

	if (st.st_size > CVS_LOGMSG_BIGMSG) {
		do {
			fprintf(stderr,
			    "The specified message file seems big.  "
			    "Proceed anyways? (y/n) ");
			if (fgets(lbuf, sizeof(lbuf), stdin) == NULL) {
				cvs_log(LP_ERRNO,
				    "failed to read from standard input");
				return (NULL);
			}

			len = strlen(lbuf);
			if ((len == 0) || (len > 2) ||
			    ((lbuf[0] != 'y') && (lbuf[0] != 'n'))) {
				fprintf(stderr, "invalid input\n");
				continue;
			}
			else if (lbuf[0] == 'y') 
				break;
			else if (lbuf[0] == 'n') {
				cvs_log(LP_ERR, "aborted by user");
				return (NULL);
			}

		} while (1);
	}

	if ((fp = fopen(path, "r")) == NULL) {
		cvs_log(LP_ERRNO, "failed to open message file `%s'", path);
		return (NULL);
	}

	bp = cvs_buf_alloc(128, BUF_AUTOEXT);
	if (bp == NULL) {
		return (NULL);
	}

	/* lcont is used to tell if a buffer returned by fgets is a start
	 * of line or just line continuation because the buffer isn't
	 * large enough to hold the entire line.
	 */
	lcont = 0;

	while (fgets(lbuf, sizeof(lbuf), fp) != NULL) {
		len = strlen(lbuf);
		if (len == 0)
			continue;
		else if ((lcont == 0) && (strncmp(lbuf, CVS_LOGMSG_LOGPREFIX,
		    strlen(CVS_LOGMSG_LOGPREFIX)) == 0))
			/* skip lines starting with the prefix */
			continue;

		cvs_buf_append(bp, lbuf, strlen(lbuf));

		lcont = (lbuf[len - 1] == '\n') ? 0 : 1;
	}
	cvs_buf_putc(bp, '\0');

	msg = (char *)cvs_buf_release(bp);

	return (msg);
}


/*
 * cvs_logmsg_get()
 *
 * Get a log message by forking and executing the user's editor.
 * Returns the message in a dynamically allocated string on success, NULL on
 * failure.
 */

char*
cvs_logmsg_get(const char *dir)
{
	int ret, fd, argc, fds[3];
	size_t len;
	char *argv[4], buf[16], path[MAXPATHLEN], *msg;
	FILE *fp;
	struct stat st1, st2;

	msg = NULL;
	fds[0] = -1;
	fds[1] = -1;
	fds[2] = -1;
	strlcpy(path, CVS_LOGMSG_FTMPL, sizeof(path));
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
	} else {
		fprintf(fp,
		    "\n%s %s\n%s Enter Log.  Lines beginning with `%s' are "
		    "removed automatically\n%s\n%s Commiting in %s\n"
		    "%s\n%s Modified Files:\n",
		    CVS_LOGMSG_LOGPREFIX, CVS_LOGMSG_LOGLINE,
		    CVS_LOGMSG_LOGPREFIX, CVS_LOGMSG_LOGPREFIX,
		    CVS_LOGMSG_LOGPREFIX, CVS_LOGMSG_LOGPREFIX,
		    dir, CVS_LOGMSG_LOGPREFIX, CVS_LOGMSG_LOGPREFIX);

		/* XXX list files here */

		fprintf(fp, "%s %s\n", CVS_LOGMSG_LOGPREFIX,
		    CVS_LOGMSG_LOGLINE);
	}
	(void)fflush(fp);

	if (fstat(fd, &st1) == -1) {
		cvs_log(LP_ERRNO, "failed to stat log message file");

		(void)fclose(fp);
		if (unlink(path) == -1)
			cvs_log(LP_ERRNO, "failed to unlink log file %s", path);
		return (NULL);
	}

	for (;;) {
		ret = cvs_exec(argc, argv, fds);
		if (ret == -1)
			break;
		if (fstat(fd, &st2) == -1) {
			cvs_log(LP_ERRNO, "failed to stat log message file");
			break;
		}

		if (st2.st_mtime != st1.st_mtime) {
			msg = cvs_logmsg_open(path);
			break;
		}

		/* nothing was entered */
		fprintf(stderr,
		    "Log message unchanged or not specified\na)bort, "
		    "c)ontinue, e)dit, !)reuse this message unchanged "
		    "for remaining dirs\nAction: (continue) ");

		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			cvs_log(LP_ERRNO, "failed to read from standard input");
			break;
		}

		len = strlen(buf);
		if ((len == 0) || (len > 2)) {
			fprintf(stderr, "invalid input\n");
			continue;
		}
		else if (buf[0] == 'a') { 
			cvs_log(LP_ERR, "aborted by user");
			break;
		} else if ((buf[0] == '\n') || (buf[0] == 'c')) {
			/* empty message */
			msg = strdup("");
			break;
		} else if (ret == 'e')
			continue;
		else if (ret == '!') {
			/* XXX do something */
		}
	}

	(void)fclose(fp);
	(void)close(fd);

	if (unlink(path) == -1)
		cvs_log(LP_ERRNO, "failed to unlink log file %s", path);

	return (msg);
}
