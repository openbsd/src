/*	$OpenBSD: rcsdiff.c,v 1.60 2006/05/04 07:06:58 xsa Exp $	*/
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

#include "includes.h"

#include "rcsprog.h"
#include "diff.h"

static int rcsdiff_file(RCSFILE *, RCSNUM *, const char *);
static int rcsdiff_rev(RCSFILE *, RCSNUM *, RCSNUM *);

static int flags = 0;
static int kflag = RCS_KWEXP_ERR;

int
rcsdiff_main(int argc, char **argv)
{
	int fd, i, ch, status;
	RCSNUM *rev1, *rev2;
	RCSFILE *file;
	char fpath[MAXPATHLEN], *rev_str1, *rev_str2;

	rev1 = rev2 = NULL;
	rev_str1 = rev_str2 = NULL;
	status = 0;

	if (strlcpy(diffargs, "diff", sizeof(diffargs)) >= sizeof(diffargs))
		errx(1, "diffargs too long");

	while ((ch = rcs_getopt(argc, argv, "ck:nqr:TuVx::z::")) != -1) {
		switch (ch) {
		case 'c':
			if (strlcat(diffargs, " -c", sizeof(diffargs)) >=
			    sizeof(diffargs))
				errx(1, "diffargs too long");
			diff_format = D_CONTEXT;
			break;
		case 'k':
			kflag = rcs_kflag_get(rcs_optarg);
			if (RCS_KWEXP_INVAL(kflag)) {
				warnx("invalid RCS keyword substitution mode");
				(usage)();
				exit(1);
			}
			break;
		case 'n':
			if (strlcat(diffargs, " -n", sizeof(diffargs)) >=
			    sizeof(diffargs))
				errx(1, "diffargs too long");
			diff_format = D_RCSDIFF;
			break;
		case 'q':
			flags |= QUIET;
			break;
		case 'r':
			rcs_setrevstr2(&rev_str1, &rev_str2, rcs_optarg);
			break;
		case 'T':
			/*
			 * kept for compatibility
			 */
			break;
		case 'u':
			if (strlcat(diffargs, " -u", sizeof(diffargs)) >=
			    sizeof(diffargs))
				errx(1, "diffargs too long");
			diff_format = D_UNIFIED;
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
			/* NOTREACHED */
		case 'x':
			/* Use blank extension if none given. */
			rcs_suffixes = rcs_optarg ? rcs_optarg : "";
			break;
		case 'z':
			timezone_flag = rcs_optarg;
			break;
		default:
			(usage)();
			exit (1);
		}
	}

	argc -= rcs_optind;
	argv += rcs_optind;

	if (argc == 0) {
		warnx("no input file");
		(usage)();
		exit(1);
	}

	for (i = 0; i < argc; i++) {
		fd = rcs_statfile(argv[i], fpath, sizeof(fpath), flags);
		if (fd < 0)
			continue;

		if ((file = rcs_open(fpath, fd,
		    RCS_READ|RCS_PARSE_FULLY)) == NULL)
			continue;

		rcs_kwexp_set(file, kflag);

		if (rev_str1 != NULL) {
			if ((rev1 = rcs_getrevnum(rev_str1, file)) == NULL)
				errx(1, "bad revision number");
		}
		if (rev_str2 != NULL) {
			if ((rev2 = rcs_getrevnum(rev_str2, file)) == NULL)
				errx(1, "bad revision number");
		}

		if (!(flags & QUIET)) {
			fprintf(stderr, "%s\n", RCS_DIFF_DIV);
			fprintf(stderr, "RCS file: %s\n", fpath);
		}

		diff_file = argv[i];

		/* No revisions given. */
		if (rev_str1 == NULL) {
			if (rcsdiff_file(file, file->rf_head, argv[i]) < 0)
				status = 2;
		/* One revision given. */
		} else if (rev_str2 == NULL) {
			if (rcsdiff_file(file, rev1, argv[i]) < 0)
				status = 2;
		/* Two revisions given. */
		} else {
			if (rcsdiff_rev(file, rev1, rev2) < 0)
				status = 2;
		}

		rcs_close(file);

		if (rev1 != NULL) {
			rcsnum_free(rev1);
			rev1 = NULL;
		}
		if (rev2 != NULL) {
			rcsnum_free(rev2);
			rev2 = NULL;
		}
	}

	return (status);
}

void
rcsdiff_usage(void)
{
	fprintf(stderr,
	    "usage: rcsdiff [-cnquV] [-kmode] [-rrev] "
	    "[-xsuffixes] [-ztz] file ...\n");
}

static int
rcsdiff_file(RCSFILE *file, RCSNUM *rev, const char *filename)
{
	int ret, fd;
	time_t t;
	struct stat st;
	char *path1, *path2;
	BUF *b1, *b2;
	char rbuf[64];
	struct tm *tb;
	struct timeval tv[2], tv2[2];

	memset(&tv, 0, sizeof(tv));
	memset(&tv2, 0, sizeof(tv2));

	ret = -1;
	b1 = b2 = NULL;

	diff_rev1 = rev;
	diff_rev2 = NULL;

	if ((fd = open(filename, O_RDONLY)) == -1) {
		warn("%s", filename);
		goto out;
	}

	rcsnum_tostr(rev, rbuf, sizeof(rbuf));
	if (!(flags & QUIET)) {
		fprintf(stderr, "retrieving revision %s\n", rbuf);
		fprintf(stderr, "%s -r%s %s\n", diffargs, rbuf, filename);
	}

	if ((b1 = rcs_getrev(file, rev)) == NULL) {
		warnx("failed to retrieve revision %s", rbuf);
		goto out;
	}

	b1 = rcs_kwexp_buf(b1, file, rev);
	tv[0].tv_sec = (long)rcs_rev_getdate(file, rev);
	tv[1].tv_sec = tv[0].tv_sec;

	if ((b2 = rcs_buf_load(filename, BUF_AUTOEXT)) == NULL) {
		warnx("failed to load file: `%s'", filename);
		goto out;
	}

	/* XXX - GNU uses GMT */
	if (fstat(fd, &st) == -1)
		err(1, "%s", filename);

	tb = gmtime(&st.st_mtime);
	t = mktime(tb);

	tv2[0].tv_sec = t;
	tv2[1].tv_sec = t;

	(void)xasprintf(&path1, "%s/diff1.XXXXXXXXXX", rcs_tmpdir);
	rcs_buf_write_stmp(b1, path1, 0600);

	rcs_buf_free(b1);
	b1 = NULL;

	if (utimes(path1, (const struct timeval *)&tv) < 0)
		warn("utimes");

	(void)xasprintf(&path2, "%s/diff2.XXXXXXXXXX", rcs_tmpdir);
	rcs_buf_write_stmp(b2, path2, 0600);

	rcs_buf_free(b2);
	b2 = NULL;

	if (utimes(path2, (const struct timeval *)&tv2) < 0)
		warn("utimes");

	rcs_diffreg(path1, path2, NULL);
	ret = 0;

out:
	if (fd != -1)
		(void)close(fd);
	if (b1 != NULL)
		rcs_buf_free(b1);
	if (b2 != NULL)
		rcs_buf_free(b2);
	if (path1 != NULL)
		xfree(path1);
	if (path2 != NULL)
		xfree(path2);

	return (ret);
}

static int
rcsdiff_rev(RCSFILE *file, RCSNUM *rev1, RCSNUM *rev2)
{
	int ret;
	char *path1, *path2;
	BUF *b1, *b2;
	char rbuf1[64], rbuf2[64];
	struct timeval tv[2], tv2[2];

	ret = -1;
	b1 = b2 = NULL;
	memset(&tv, 0, sizeof(tv));
	memset(&tv2, 0, sizeof(tv2));

	diff_rev1 = rev1;
	diff_rev2 = rev2;

	rcsnum_tostr(rev1, rbuf1, sizeof(rbuf1));
	if (!(flags & QUIET))
		fprintf(stderr, "retrieving revision %s\n", rbuf1);

	if ((b1 = rcs_getrev(file, rev1)) == NULL) {
		warnx("failed to retrieve revision %s", rbuf1);
		goto out;
	}

	b1 = rcs_kwexp_buf(b1, file, rev1);
	tv[0].tv_sec = (long)rcs_rev_getdate(file, rev1);
	tv[1].tv_sec = tv[0].tv_sec;

	rcsnum_tostr(rev2, rbuf2, sizeof(rbuf2));
	if (!(flags & QUIET))
		fprintf(stderr, "retrieving revision %s\n", rbuf2);

	if ((b2 = rcs_getrev(file, rev2)) == NULL) {
		warnx("failed to retrieve revision %s", rbuf2);
		goto out;
	}

	b2 = rcs_kwexp_buf(b2, file, rev2);
	tv2[0].tv_sec = (long)rcs_rev_getdate(file, rev2);
	tv2[1].tv_sec = tv2[0].tv_sec;

	if (!(flags & QUIET))
		fprintf(stderr, "%s -r%s -r%s\n", diffargs, rbuf1, rbuf2);

	(void)xasprintf(&path1, "%s/diff1.XXXXXXXXXX", rcs_tmpdir);
	rcs_buf_write_stmp(b1, path1, 0600);

	rcs_buf_free(b1);
	b1 = NULL;

	if (utimes(path1, (const struct timeval *)&tv) < 0)
		warn("utimes");

	(void)xasprintf(&path2, "%s/diff2.XXXXXXXXXX", rcs_tmpdir);
	rcs_buf_write_stmp(b2, path2, 0600);

	rcs_buf_free(b2);
	b2 = NULL;

	if (utimes(path2, (const struct timeval *)&tv2) < 0)
		warn("utimes");

	rcs_diffreg(path1, path2, NULL);
	ret = 0;

out:
	if (b1 != NULL)
		rcs_buf_free(b1);
	if (b2 != NULL)
		rcs_buf_free(b2);
	if (path1 != NULL)
		xfree(path1);
	if (path2 != NULL)
		xfree(path2);

	return (ret);
}
