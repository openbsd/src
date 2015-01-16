/*	$OpenBSD: rcsmerge.c,v 1.55 2015/01/16 06:40:11 deraadt Exp $	*/
/*
 * Copyright (c) 2005, 2006 Xavier Santolaria <xsa@openbsd.org>
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

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rcsprog.h"
#include "diff.h"

int
rcsmerge_main(int argc, char **argv)
{
	int fd, ch, flags, kflag, status;
	char fpath[PATH_MAX], r1[RCS_REV_BUFSZ], r2[RCS_REV_BUFSZ];
	char *rev_str1, *rev_str2;
	RCSFILE *file;
	RCSNUM *rev1, *rev2;
	BUF *bp;

	flags = 0;
	status = D_ERROR;
	rev1 = rev2 = NULL;
	rev_str1 = rev_str2 = NULL;

	while ((ch = rcs_getopt(argc, argv, "AEek:p::q::r::TVx::z:")) != -1) {
		switch (ch) {
		case 'A':
			/*
			 * kept for compatibility
			 */
			break;
		case 'E':
			flags |= MERGE_EFLAG;
			flags |= MERGE_OFLAG;
			break;
		case 'e':
			flags |= MERGE_EFLAG;
			break;
		case 'k':
			kflag = rcs_kflag_get(rcs_optarg);
			if (RCS_KWEXP_INVAL(kflag)) {
				warnx("invalid RCS keyword substitution mode");
				(usage)();
			}
			break;
		case 'p':
			rcs_setrevstr2(&rev_str1, &rev_str2, rcs_optarg);
			flags |= PIPEOUT;
			break;
		case 'q':
			rcs_setrevstr2(&rev_str1, &rev_str2, rcs_optarg);
			flags |= QUIET;
			break;
		case 'r':
			rcs_setrevstr2(&rev_str1, &rev_str2,
			    rcs_optarg ? rcs_optarg : "");
			break;
		case 'T':
			/*
			 * kept for compatibility
			 */
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
		case 'x':
			/* Use blank extension if none given. */
			rcs_suffixes = rcs_optarg ? rcs_optarg : "";
			break;
		case 'z':
			timezone_flag = rcs_optarg;
			break;
		default:
			(usage)();
		}
	}

	argc -= rcs_optind;
	argv += rcs_optind;

	if (rev_str1 == NULL) {
		warnx("no base revision number given");
		(usage)();
	}

	if (argc < 1) {
		warnx("no input file");
		(usage)();
	}

	if (argc > 2 || (argc == 2 && argv[1] != NULL))
		warnx("warning: excess arguments ignored");

	if ((fd = rcs_choosefile(argv[0], fpath, sizeof(fpath))) < 0)
		err(status, "%s", fpath);

	if (!(flags & QUIET))
		(void)fprintf(stderr, "RCS file: %s\n", fpath);

	if ((file = rcs_open(fpath, fd, RCS_READ)) == NULL)
		return (status);

	if (strcmp(rev_str1, "") == 0) {
		rev1 = rcsnum_alloc();
		rcsnum_cpy(file->rf_head, rev1, 0);
	} else if ((rev1 = rcs_getrevnum(rev_str1, file)) == NULL)
		errx(D_ERROR, "invalid revision: %s", rev_str1);

	if (rev_str2 != NULL && strcmp(rev_str2, "") != 0) {
		if ((rev2 = rcs_getrevnum(rev_str2, file)) == NULL)
			errx(D_ERROR, "invalid revision: %s", rev_str2);
	} else {
		rev2 = rcsnum_alloc();
		rcsnum_cpy(file->rf_head, rev2, 0);
	}

	if (rcsnum_cmp(rev1, rev2, 0) == 0)
		goto out;

	if ((bp = rcs_diff3(file, argv[0], rev1, rev2, flags)) == NULL)
		errx(D_ERROR, "failed to merge");

	if (!(flags & QUIET)) {
		(void)rcsnum_tostr(rev1, r1, sizeof(r1));
		(void)rcsnum_tostr(rev2, r2, sizeof(r2));

		(void)fprintf(stderr, "Merging differences between %s and "
		    "%s into %s%s\n", r1, r2, argv[0],
		    (flags & PIPEOUT) ? "; result to stdout":"");
	}

	if (diff3_conflicts != 0)
		status = D_OVERLAPS;
	else
		status = 0;

	if (flags & PIPEOUT)
		buf_write_fd(bp, STDOUT_FILENO);
	else {
		/* XXX mode */
		if (buf_write(bp, argv[0], 0644) < 0)
			warnx("buf_write failed");

	}

	buf_free(bp);

out:
	rcs_close(file);

	if (rev1 != NULL)
		rcsnum_free(rev1);
	if (rev2 != NULL)
		rcsnum_free(rev2);

	return (status);
}

__dead void
rcsmerge_usage(void)
{
	fprintf(stderr,
	    "usage: rcsmerge [-EV] [-kmode] [-p[rev]] [-q[rev]]\n"
	    "                [-xsuffixes] [-ztz] -rrev file ...\n");

	exit(D_ERROR);
}
