/*	$OpenBSD: co.c,v 1.17 2005/10/16 15:46:07 joris Exp $	*/
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
	int lock;
	RCSNUM *frev, *rev;
	RCSFILE *file;
	char fpath[MAXPATHLEN], buf[16];
	char *username;

	lock = 0;
	rev = RCS_HEAD_REV;
	frev = NULL;

	if ((username = getlogin()) == NULL) {
		cvs_log(LP_ERRNO, "failed to get username");
		exit (1);
	}

	while ((ch = rcs_getopt(argc, argv, "l::qr::u::V")) != -1) {
		switch (ch) {
		case 'l':
			if (rev != RCS_HEAD_REV)
				cvs_log(LP_WARN,
				    "redefinition of revision number");
			if (rcs_optarg != NULL) {
				if ((rev = rcsnum_parse(rcs_optarg)) == NULL) {
					cvs_log(LP_ERR, "bad revision number");
					exit (1);
				}
			}
			lock = LOCK_LOCK;
			break;
		case 'q':
			verbose = 0;
			break;
		case 'r':
			if (rev != RCS_HEAD_REV)
				cvs_log(LP_WARN,
				    "redefinition of revision number");
			if (rcs_optarg != NULL) {
				if ((rev = rcsnum_parse(rcs_optarg)) == NULL) {
					cvs_log(LP_ERR, "bad revision number");
					exit (1);
				}
			}
			break;
		case 'u':
			if (rev != RCS_HEAD_REV)
				cvs_log(LP_WARN,
				    "redefinition of revision number");
			if (rcs_optarg != NULL) {
				if ((rev = rcsnum_parse(rcs_optarg)) == NULL) {
					cvs_log(LP_ERR, "bad revision number");
					exit (1);
				}
			}
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

		if ((file = rcs_open(fpath, RCS_RDWR)) == NULL)
			continue;

		if (rev == RCS_HEAD_REV)
			frev = file->rf_head;
		else
			frev = rev;

		rcsnum_tostr(frev, buf, sizeof(buf));

		if (checkout_rev(file, frev, argv[i], lock, username) < 0) { 
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
    const char *username)
{
	char buf[16];
	mode_t mode = 0444;
	BUF *bp;

	/*
	 * Check out the latest revision if <frev> is greater than HEAD
	 */
	if (rcsnum_cmp(frev, file->rf_head, 0) == -1)
		frev = file->rf_head; 

	rcsnum_tostr(frev, buf, sizeof(buf));

	if ((bp = rcs_getrev(file, frev)) == NULL) {
		cvs_log(LP_ERR, "cannot find revision `%s'", buf);
		return (-1);
	}

	if (lkmode == LOCK_LOCK) {
		if ((username != NULL)
		    && (rcs_lock_add(file, username, frev) < 0)) {
			if (rcs_errno != RCS_ERR_DUPENT)
				cvs_log(LP_ERR, "failed to lock '%s'", buf);
			else
				cvs_log(LP_WARN, "you already have a lock");
		}
		mode = 0644;
	} else if (lkmode == LOCK_UNLOCK) {
		if (rcs_lock_remove(file, frev) < 0) {
			if (rcs_errno != RCS_ERR_NOENT)
				cvs_log(LP_ERR,
				    "failed to remove lock '%s'", buf);
		}
		mode = 0444;
	}

	if (cvs_buf_write(bp, dst, mode) < 0) {
		cvs_log(LP_ERR, "failed to write revision to file");
		cvs_buf_free(bp);
		return (-1);
	}

	cvs_buf_free(bp);

	if (verbose == 1) {
		cvs_printf("revision %s ", buf);
		if (lkmode == LOCK_LOCK)
			cvs_printf("(locked)");
		else if (lkmode == LOCK_UNLOCK)
			cvs_printf("(unlocked)");
		cvs_printf("\n");
		cvs_printf("done\n");
	}

	return (0);
}

