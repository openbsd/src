/*	$OpenBSD: co.c,v 1.22 2005/10/20 17:10:01 xsa Exp $	*/
/*
 * Copyright (c) 2005 Joris Vink <joris@openbsd.org>
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

#include <sys/param.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "rcs.h"
#include "rcsprog.h"

#define LOCK_LOCK	1
#define LOCK_UNLOCK	2

int
checkout_main(int argc, char **argv)
{
	int i, ch;
	int fflag, lock;
	RCSNUM *frev, *rev;
	RCSFILE *file;
	char fpath[MAXPATHLEN], buf[16];
	char *username;

	fflag = lock = 0;
	rev = RCS_HEAD_REV;
	frev = NULL;

	if ((username = getlogin()) == NULL) {
		cvs_log(LP_ERRNO, "failed to get username");
		exit (1);
	}

	while ((ch = rcs_getopt(argc, argv, "f::l::p::qr::u::V")) != -1) {
		switch (ch) {
		case 'f':
			rcs_set_rev(rcs_optarg, &rev);
			fflag = 1;
			break;
		case 'l':
			rcs_set_rev(rcs_optarg, &rev);
			lock = LOCK_LOCK;
			break;
		case 'p':
			rcs_set_rev(rcs_optarg, &rev);
			pipeout = 1;
			break;
		case 'q':
			verbose = 0;
			break;
		case 'r':
			rcs_set_rev(rcs_optarg, &rev);
			break;
		case 'u':
			rcs_set_rev(rcs_optarg, &rev);
			lock = LOCK_UNLOCK;
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
		default:
			(usage)();
			exit(1);
		}
	}

	argc -= rcs_optind;
	argv += rcs_optind;

	if (argc == 0) {
		cvs_log(LP_ERR, "no input file");
		(usage)();
		exit (1);
	}

	for (i = 0; i < argc; i++) {
		if (rcs_statfile(argv[i], fpath, sizeof(fpath)) < 0)
			continue;

		if (verbose == 1)
			printf("%s  -->  %s\n", fpath,
			    (pipeout == 1) ? "standard output" : argv[i]);

		if ((file = rcs_open(fpath, RCS_RDWR)) == NULL)
			continue;

		if (rev == RCS_HEAD_REV)
			frev = file->rf_head;
		else
			frev = rev;

		rcsnum_tostr(frev, buf, sizeof(buf));

		if (checkout_rev(file, frev, argv[i], lock, username, fflag) < 0) { 
			rcs_close(file);
			continue;
		}

		rcs_close(file);
	}

	if (rev != RCS_HEAD_REV)
		rcsnum_free(frev);

	return (0);
}

void
checkout_usage(void)
{
	fprintf(stderr,
	    "usage: co [-qV] [-l[rev]] [-r[rev]] [-u[rev]] file ...\n");
}

/*
 * Checkout revision <rev> from RCSFILE <file>, writing it to the path <dst>
 * <lkmode> is either LOCK_LOCK or LOCK_UNLOCK or something else
 * (which has no effect).
 * In the case of LOCK_LOCK, a lock is set for <username> if it is not NULL.
 * In the case of LOCK_UNLOCK, all locks are removed for that revision.
 *
 * Returns 0 on success, -1 on failure.
 */
int
checkout_rev(RCSFILE *file, RCSNUM *frev, const char *dst, int lkmode,
    const char *username, int force)
{
	char buf[16], yn;
	mode_t mode = 0444;
	BUF *bp;
	struct stat st;
	char *content;

	/*
	 * Check out the latest revision if <frev> is greater than HEAD
	 */
	if (rcsnum_cmp(frev, file->rf_head, 0) == -1)
		frev = file->rf_head; 

	rcsnum_tostr(frev, buf, sizeof(buf));

	if (verbose == 1)
		printf("revision %s", buf);

	if ((bp = rcs_getrev(file, frev)) == NULL) {
		cvs_log(LP_ERR, "cannot find revision `%s'", buf);
		return (-1);
	}

	if (lkmode == LOCK_LOCK) {
		if ((username != NULL)
		    && (rcs_lock_add(file, username, frev) < 0)) {
			if ((rcs_errno != RCS_ERR_DUPENT) && (verbose == 1))
				cvs_log(LP_ERR, "failed to lock '%s'", buf);
		}

		mode = 0644;
		if (verbose == 1)
			printf(" (locked)");
	} else if (lkmode == LOCK_UNLOCK) {
		if (rcs_lock_remove(file, frev) < 0) {
			if (rcs_errno != RCS_ERR_NOENT)
				cvs_log(LP_ERR,
				    "failed to remove lock '%s'", buf);
		}

		mode = 0444;
		if (verbose == 1)
			printf(" (unlocked)");
	}

	if (verbose == 1)
		printf("\n");

	if ((stat(dst, &st) != -1) && force == 0) {
		if (st.st_mode & S_IWUSR) {
			yn = 0;
			if (verbose == 0) {
				cvs_log(LP_ERR,
				    "writeable %s exists; checkout aborted",
				    dst);
				return (-1);
			}
			while (yn != 'y' && yn != 'n') {
				printf("writeable %s exists; ", dst);
				printf("remove it? [ny](n): ");
				fflush(stdout);
				yn = getchar();
			}

			if (yn == 'n') {
				cvs_log(LP_ERR, "checkout aborted");
				return (-1);
			}
		}
	}

	if (pipeout == 1) {
		cvs_buf_putc(bp, '\0');
		content = cvs_buf_release(bp);
		printf("%s", content);
		free(content);
	} else {
		if (cvs_buf_write(bp, dst, mode) < 0) {
			cvs_log(LP_ERR, "failed to write revision to file");
			cvs_buf_free(bp);
			return (-1);
		}
		cvs_buf_free(bp);

		if (verbose == 1)
			printf("done\n");
	}

	return (0);
}

