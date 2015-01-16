/*	$OpenBSD: rcsclean.c,v 1.54 2015/01/16 06:40:11 deraadt Exp $	*/
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

#include <sys/types.h>

#include <dirent.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "rcsprog.h"
#include "diff.h"

static void	rcsclean_file(char *, const char *);

static int nflag = 0;
static int kflag = RCS_KWEXP_ERR;
static int uflag = 0;
static int flags = 0;
static char *locker = NULL;

int
rcsclean_main(int argc, char **argv)
{
	int i, ch;
	char *rev_str;
	DIR *dirp;
	struct dirent *dp;

	rev_str = NULL;

	while ((ch = rcs_getopt(argc, argv, "k:n::q::r::Tu::Vx::")) != -1) {
		switch (ch) {
		case 'k':
			kflag = rcs_kflag_get(rcs_optarg);
			if (RCS_KWEXP_INVAL(kflag)) {
				warnx("invalid RCS keyword substitution mode");
				(usage)();
			}
			break;
		case 'n':
			rcs_setrevstr(&rev_str, rcs_optarg);
			nflag = 1;
			break;
		case 'q':
			rcs_setrevstr(&rev_str, rcs_optarg);
			flags |= QUIET;
			break;
		case 'r':
			rcs_setrevstr(&rev_str, rcs_optarg);
			break;
		case 'T':
			flags |= PRESERVETIME;
			break;
		case 'u':
			rcs_setrevstr(&rev_str, rcs_optarg);
			uflag = 1;
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
		case 'x':
			/* Use blank extension if none given. */
			rcs_suffixes = rcs_optarg ? rcs_optarg : "";
			break;
		default:
			(usage)();
		}
	}

	argc -= rcs_optind;
	argv += rcs_optind;

	if ((locker = getlogin()) == NULL)
		err(1, "getlogin");

	if (argc == 0) {
		if ((dirp = opendir(".")) == NULL) {
			warn("opendir");
			(usage)();
		}

		while ((dp = readdir(dirp)) != NULL) {
			if (dp->d_type == DT_DIR)
				continue;
			rcsclean_file(dp->d_name, rev_str);
		}

		(void)closedir(dirp);
	} else
		for (i = 0; i < argc; i++)
			rcsclean_file(argv[i], rev_str);

	return (0);
}

__dead void
rcsclean_usage(void)
{
	fprintf(stderr,
	    "usage: rcsclean [-TV] [-kmode] [-n[rev]] [-q[rev]] [-r[rev]]\n"
	    "                [-u[rev]] [-xsuffixes] [-ztz] [file ...]\n");

	exit(1);
}

static void
rcsclean_file(char *fname, const char *rev_str)
{
	int fd, match;
	RCSFILE *file;
	char fpath[PATH_MAX], numb[RCS_REV_BUFSZ];
	RCSNUM *rev;
	BUF *b1, *b2;
	time_t rcs_mtime = -1;

	b1 = b2 = NULL;
	file = NULL;
	rev = NULL;

	if ((fd = rcs_choosefile(fname, fpath, sizeof(fpath))) < 0)
		goto out;

	if ((file = rcs_open(fpath, fd, RCS_RDWR)) == NULL)
		goto out;

	if (flags & PRESERVETIME)
		rcs_mtime = rcs_get_mtime(file);

	rcs_kwexp_set(file, kflag);

	if (rev_str == NULL)
		rev = file->rf_head;
	else if ((rev = rcs_getrevnum(rev_str, file)) == NULL) {
		warnx("%s: Symbolic name `%s' is undefined.", fpath, rev_str);
		goto out;
	}

	if ((b1 = rcs_getrev(file, rev)) == NULL) {
		warnx("failed to get needed revision");
		goto out;
	}
	if ((b2 = buf_load(fname)) == NULL) {
		warnx("failed to load `%s'", fname);
		goto out;
	}

	/* If buffer lengths are the same, compare contents as well. */
	if (buf_len(b1) != buf_len(b2))
		match = 0;
	else {
		size_t len, n;

		len = buf_len(b1);

		match = 1;
		for (n = 0; n < len; ++n)
			if (buf_getc(b1, n) != buf_getc(b2, n)) {
				match = 0;
				break;
			}
	}

	if (match == 1) {
		if (uflag == 1 && !TAILQ_EMPTY(&(file->rf_locks))) {
			if (!(flags & QUIET) && nflag == 0) {
				printf("rcs -u%s %s\n",
				    rcsnum_tostr(rev, numb, sizeof(numb)),
				    fpath);
			}
			(void)rcs_lock_remove(file, locker, rev);
		}

		if (TAILQ_EMPTY(&(file->rf_locks))) {
			if (!(flags & QUIET))
				printf("rm -f %s\n", fname);

			if (nflag == 0)
				(void)unlink(fname);
		}
	}

	rcs_write(file);
	if (flags & PRESERVETIME)
		rcs_set_mtime(file, rcs_mtime);

out:
	if (b1 != NULL)
		buf_free(b1);
	if (b2 != NULL)
		buf_free(b2);
	if (file != NULL)
		rcs_close(file);
}
