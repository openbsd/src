/*	$OpenBSD: rcsdiff.c,v 1.78 2010/12/06 22:50:34 chl Exp $	*/
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

#include <sys/stat.h>
#include <sys/time.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rcsprog.h"
#include "diff.h"

static int rcsdiff_file(RCSFILE *, RCSNUM *, const char *, int);
static int rcsdiff_rev(RCSFILE *, RCSNUM *, RCSNUM *, int);
static void push_ignore_pats(char *);

static int quiet;
static int kflag = RCS_KWEXP_ERR;
static char *diff_ignore_pats;

int
rcsdiff_main(int argc, char **argv)
{
	int fd, i, ch, dflags, status;
	RCSNUM *rev1, *rev2;
	RCSFILE *file;
	char fpath[MAXPATHLEN], *rev_str1, *rev_str2;
	const char *errstr;

	rev1 = rev2 = NULL;
	rev_str1 = rev_str2 = NULL;
	status = D_SAME;
	dflags = 0;

	if (strlcpy(diffargs, "diff", sizeof(diffargs)) >= sizeof(diffargs))
		errx(D_ERROR, "diffargs too long");

	while ((ch = rcs_getopt(argc, argv, "abC:cdI:ik:npqr:TtU:uVwx::z::")) != -1) {
		switch (ch) {
		case 'a':
			if (strlcat(diffargs, " -a", sizeof(diffargs)) >=
			    sizeof(diffargs))
				errx(D_ERROR, "diffargs too long");
			dflags |= D_FORCEASCII;
			break;
		case 'b':
			if (strlcat(diffargs, " -b", sizeof(diffargs)) >=
			    sizeof(diffargs))
				errx(D_ERROR, "diffargs too long");
			dflags |= D_FOLDBLANKS;
			break;
		case 'C':
			(void)strlcat(diffargs, " -C", sizeof(diffargs));
			if (strlcat(diffargs, rcs_optarg, sizeof(diffargs)) >=
			    sizeof(diffargs))
				errx(D_ERROR, "diffargs too long");
			diff_context = strtonum(rcs_optarg, 0, INT_MAX, &errstr);
			if (errstr)
				errx(D_ERROR, "context is %s: %s",
				    errstr, rcs_optarg);
			diff_format = D_CONTEXT;
			break;
		case 'c':
			if (strlcat(diffargs, " -c", sizeof(diffargs)) >=
			    sizeof(diffargs))
				errx(D_ERROR, "diffargs too long");
			diff_format = D_CONTEXT;
			break;
		case 'd':
			if (strlcat(diffargs, " -d", sizeof(diffargs)) >=
			    sizeof(diffargs))
				errx(D_ERROR, "diffargs too long");
			dflags |= D_MINIMAL;
			break;
		case 'i':
			if (strlcat(diffargs, " -i", sizeof(diffargs)) >=
			    sizeof(diffargs))
				errx(D_ERROR, "diffargs too long");
			dflags |= D_IGNORECASE;
			break;
		case 'I':
			(void)strlcat(diffargs, " -I", sizeof(diffargs));
			if (strlcat(diffargs, rcs_optarg, sizeof(diffargs)) >=
			    sizeof(diffargs))
				errx(D_ERROR, "diffargs too long");
			push_ignore_pats(rcs_optarg);
			break;
		case 'k':
			kflag = rcs_kflag_get(rcs_optarg);
			if (RCS_KWEXP_INVAL(kflag)) {
				warnx("invalid RCS keyword substitution mode");
				(usage)();
				exit(D_ERROR);
			}
			break;
		case 'n':
			if (strlcat(diffargs, " -n", sizeof(diffargs)) >=
			    sizeof(diffargs))
				errx(D_ERROR, "diffargs too long");
			diff_format = D_RCSDIFF;
			break;
		case 'p':
			if (strlcat(diffargs, " -p", sizeof(diffargs)) >=
			    sizeof(diffargs))
				errx(D_ERROR, "diffargs too long");
			dflags |= D_PROTOTYPE;
			break;
		case 'q':
			quiet = 1;
			break;
		case 'r':
			rcs_setrevstr2(&rev_str1, &rev_str2, rcs_optarg);
			break;
		case 'T':
			/*
			 * kept for compatibility
			 */
			break;
		case 't':
			if (strlcat(diffargs, " -t", sizeof(diffargs)) >=
			    sizeof(diffargs))
				errx(D_ERROR, "diffargs too long");
			dflags |= D_EXPANDTABS;
			break;
		case 'U':
			(void)strlcat(diffargs, " -U", sizeof(diffargs));
			if (strlcat(diffargs, rcs_optarg, sizeof(diffargs)) >=
			    sizeof(diffargs))
				errx(D_ERROR, "diffargs too long");
			diff_context = strtonum(rcs_optarg, 0, INT_MAX, &errstr);
			if (errstr)
				errx(D_ERROR, "context is %s: %s",
				    errstr, rcs_optarg);
			diff_format = D_UNIFIED;
			break;
		case 'u':
			if (strlcat(diffargs, " -u", sizeof(diffargs)) >=
			    sizeof(diffargs))
				errx(D_ERROR, "diffargs too long");
			diff_format = D_UNIFIED;
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
		case 'w':
			if (strlcat(diffargs, " -w", sizeof(diffargs)) >=
			    sizeof(diffargs))
				errx(D_ERROR, "diffargs too long");
			dflags |= D_IGNOREBLANKS;
			break;
		case 'x':
			/* Use blank extension if none given. */
			rcs_suffixes = rcs_optarg ? rcs_optarg : "";
			break;
		case 'z':
			timezone_flag = rcs_optarg;
			break;
		default:
			(usage)();
			exit(D_ERROR);
		}
	}

	argc -= rcs_optind;
	argv += rcs_optind;

	if (argc == 0) {
		warnx("no input file");
		(usage)();
		exit(D_ERROR);
	}

	if (diff_ignore_pats != NULL) {
		char buf[BUFSIZ];
		int error;

		diff_ignore_re = xmalloc(sizeof(*diff_ignore_re));
		if ((error = regcomp(diff_ignore_re, diff_ignore_pats,
		    REG_NEWLINE | REG_EXTENDED)) != 0) {
			regerror(error, diff_ignore_re, buf, sizeof(buf));
			if (*diff_ignore_pats != '\0')
				errx(D_ERROR, "%s: %s", diff_ignore_pats, buf);
			else
				errx(D_ERROR, "%s", buf);
		}
	}

	for (i = 0; i < argc; i++) {
		fd = rcs_choosefile(argv[i], fpath, sizeof(fpath));
		if (fd < 0) {
			warn("%s", fpath);
			continue;
		}

		if ((file = rcs_open(fpath, fd,
		    RCS_READ|RCS_PARSE_FULLY)) == NULL)
			continue;

		rcs_kwexp_set(file, kflag);

		if (rev_str1 != NULL) {
			if ((rev1 = rcs_getrevnum(rev_str1, file)) == NULL)
				errx(D_ERROR, "bad revision number");
		}
		if (rev_str2 != NULL) {
			if ((rev2 = rcs_getrevnum(rev_str2, file)) == NULL)
				errx(D_ERROR, "bad revision number");
		}

		if (!quiet) {
			fprintf(stderr, "%s\n", RCS_DIFF_DIV);
			fprintf(stderr, "RCS file: %s\n", fpath);
		}

		diff_file = argv[i];

		/* No revisions given. */
		if (rev_str1 == NULL)
			status = rcsdiff_file(file, file->rf_head, argv[i],
			    dflags);
		/* One revision given. */
		else if (rev_str2 == NULL)
			status = rcsdiff_file(file, rev1, argv[i], dflags);
		/* Two revisions given. */
		else
			status = rcsdiff_rev(file, rev1, rev2, dflags);

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
	    "usage: rcsdiff [-cnquV] [-kmode] [-rrev] [-xsuffixes] [-ztz]\n"
	    "               [diff_options] file ...\n");
}

static int
rcsdiff_file(RCSFILE *file, RCSNUM *rev, const char *filename, int dflags)
{
	int ret, fd;
	time_t t;
	struct stat st;
	char *path1, *path2;
	BUF *b1, *b2;
	char rbuf[RCS_REV_BUFSZ];
	struct tm *tb;
	struct timeval tv[2], tv2[2];

	memset(&tv, 0, sizeof(tv));
	memset(&tv2, 0, sizeof(tv2));

	ret = D_ERROR;
	b1 = b2 = NULL;

	diff_rev1 = rev;
	diff_rev2 = NULL;
	path1 = path2 = NULL;

	if ((fd = open(filename, O_RDONLY)) == -1) {
		warn("%s", filename);
		goto out;
	}

	rcsnum_tostr(rev, rbuf, sizeof(rbuf));
	if (!quiet) {
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

	if ((b2 = buf_load(filename)) == NULL) {
		warnx("failed to load file: `%s'", filename);
		goto out;
	}

	/* XXX - GNU uses GMT */
	if (fstat(fd, &st) == -1)
		err(D_ERROR, "%s", filename);

	tb = gmtime(&st.st_mtime);
	t = mktime(tb);

	tv2[0].tv_sec = t;
	tv2[1].tv_sec = t;

	(void)xasprintf(&path1, "%s/diff1.XXXXXXXXXX", rcs_tmpdir);
	buf_write_stmp(b1, path1);

	buf_free(b1);
	b1 = NULL;

	if (utimes(path1, (const struct timeval *)&tv) < 0)
		warn("utimes");

	(void)xasprintf(&path2, "%s/diff2.XXXXXXXXXX", rcs_tmpdir);
	buf_write_stmp(b2, path2);

	buf_free(b2);
	b2 = NULL;

	if (utimes(path2, (const struct timeval *)&tv2) < 0)
		warn("utimes");

	ret = diffreg(path1, path2, NULL, dflags);

out:
	if (fd != -1)
		(void)close(fd);
	if (b1 != NULL)
		buf_free(b1);
	if (b2 != NULL)
		buf_free(b2);
	if (path1 != NULL)
		xfree(path1);
	if (path2 != NULL)
		xfree(path2);

	return (ret);
}

static int
rcsdiff_rev(RCSFILE *file, RCSNUM *rev1, RCSNUM *rev2, int dflags)
{
	struct timeval tv[2], tv2[2];
	BUF *b1, *b2;
	int ret;
	char *path1, *path2, rbuf1[RCS_REV_BUFSZ], rbuf2[RCS_REV_BUFSZ];

	ret = D_ERROR;
	b1 = b2 = NULL;
	memset(&tv, 0, sizeof(tv));
	memset(&tv2, 0, sizeof(tv2));

	diff_rev1 = rev1;
	diff_rev2 = rev2;
	path1 = path2 = NULL;

	rcsnum_tostr(rev1, rbuf1, sizeof(rbuf1));
	if (!quiet)
		fprintf(stderr, "retrieving revision %s\n", rbuf1);

	if ((b1 = rcs_getrev(file, rev1)) == NULL) {
		warnx("failed to retrieve revision %s", rbuf1);
		goto out;
	}

	b1 = rcs_kwexp_buf(b1, file, rev1);
	tv[0].tv_sec = (long)rcs_rev_getdate(file, rev1);
	tv[1].tv_sec = tv[0].tv_sec;

	rcsnum_tostr(rev2, rbuf2, sizeof(rbuf2));
	if (!quiet)
		fprintf(stderr, "retrieving revision %s\n", rbuf2);

	if ((b2 = rcs_getrev(file, rev2)) == NULL) {
		warnx("failed to retrieve revision %s", rbuf2);
		goto out;
	}

	b2 = rcs_kwexp_buf(b2, file, rev2);
	tv2[0].tv_sec = (long)rcs_rev_getdate(file, rev2);
	tv2[1].tv_sec = tv2[0].tv_sec;

	if (!quiet)
		fprintf(stderr, "%s -r%s -r%s\n", diffargs, rbuf1, rbuf2);

	(void)xasprintf(&path1, "%s/diff1.XXXXXXXXXX", rcs_tmpdir);
	buf_write_stmp(b1, path1);

	buf_free(b1);
	b1 = NULL;

	if (utimes(path1, (const struct timeval *)&tv) < 0)
		warn("utimes");

	(void)xasprintf(&path2, "%s/diff2.XXXXXXXXXX", rcs_tmpdir);
	buf_write_stmp(b2, path2);

	buf_free(b2);
	b2 = NULL;

	if (utimes(path2, (const struct timeval *)&tv2) < 0)
		warn("utimes");

	ret = diffreg(path1, path2, NULL, dflags);

out:
	if (b1 != NULL)
		buf_free(b1);
	if (b2 != NULL)
		buf_free(b2);
	if (path1 != NULL)
		xfree(path1);
	if (path2 != NULL)
		xfree(path2);

	return (ret);
}

static void
push_ignore_pats(char *pattern)
{
	size_t len;

	if (diff_ignore_pats == NULL) {
		len = strlen(pattern) + 1;
		diff_ignore_pats = xmalloc(len);
		strlcpy(diff_ignore_pats, pattern, len);
	} else {
		/* old + "|" + new + NUL */
		len = strlen(diff_ignore_pats) + strlen(pattern) + 2;
		diff_ignore_pats = xrealloc(diff_ignore_pats, len, 1);
		strlcat(diff_ignore_pats, "|", len);
		strlcat(diff_ignore_pats, pattern, len);
	}
}
