/*	$OpenBSD: co.c,v 1.9 2005/10/10 13:32:16 niallo Exp $	*/
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

extern char *__progname;

#define LOCK_LOCK	1
#define LOCK_UNLOCK	2

int
checkout_main(int argc, char **argv)
{
	int i, ch;
	int lock;
	RCSNUM *frev, *rev;
	RCSFILE *file;
	BUF *bp;
	char buf[16];
	char fpath[MAXPATHLEN];
	char *username;
	mode_t mode = 0444;

	lock = 0;
	rev = RCS_HEAD_REV;
	frev = NULL;

	if ((username = getlogin()) == NULL) {
		cvs_log(LP_ERR, "failed to get username");
		exit (1);
	}

	while ((ch = getopt(argc, argv, "l::qr::u::V")) != -1) {
		switch (ch) {
		case 'l':
			if (rev != RCS_HEAD_REV)
				cvs_log(LP_WARN,
				    "redefinition of revision number");
			if (optarg != NULL) {
				if ((rev = rcsnum_parse(optarg)) == NULL) {
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
			if (optarg != NULL) {
				if ((rev = rcsnum_parse(optarg)) == NULL) {
					cvs_log(LP_ERR, "bad revision number");
					exit (1);
				}
			}
			break;
		case 'u':
			if (rev != RCS_HEAD_REV)
				cvs_log(LP_WARN,
				    "redefinition of revision number");
			if (optarg != NULL) {
				if ((rev = rcsnum_parse(optarg)) == NULL) {
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

	argc -= optind;
	argv += optind;

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

		if ((bp = rcs_getrev(file, frev)) == NULL) {
			cvs_log(LP_ERR, "cannot find '%s' in %s", buf, fpath);
			rcs_close(file);
			continue;
		}

		if (lock == LOCK_LOCK) {
			if (rcs_lock_add(file, username, frev) < 0) {
				if (rcs_errno != RCS_ERR_DUPENT)
					cvs_log(LP_ERR, "failed to lock '%s'", buf);
				else
					cvs_log(LP_WARN, "you already have a lock");
			}
			mode = 0644;
		} else if (lock == LOCK_UNLOCK) {
			if (rcs_lock_remove(file, frev) < 0) {
				if (rcs_errno != RCS_ERR_NOENT)
					cvs_log(LP_ERR,
					    "failed to remove lock '%s'", buf);
			}
			mode = 0444;
		}
		if (cvs_buf_write(bp, argv[i], mode) < 0) {
			cvs_log(LP_ERR, "failed to write revision to file");
			cvs_buf_free(bp);
			rcs_close(file);
			continue;
		}

		cvs_buf_free(bp);

		rcs_close(file);
		if (verbose) {
			printf("revision %s ", buf);
			if (lock == LOCK_LOCK)
				printf("(locked)");
			else if (lock == LOCK_UNLOCK)
				printf("(unlocked)");
			printf("\n");
			printf("done\n");
		}
	}

	if (rev != RCS_HEAD_REV)
		rcsnum_free(frev);

	return (0);
}

void
checkout_usage(void)
{
	fprintf(stderr, "usage %s [-qV] [-l [rev]] [-r [rev]] [-u [rev]]"
	    " file ...\n", __progname);
}
