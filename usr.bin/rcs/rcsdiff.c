/*	$OpenBSD: rcsdiff.c,v 1.18 2005/11/22 15:00:06 xsa Exp $	*/
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
#include "diff.h"
#include "rcsprog.h"

static int rcsdiff_file(RCSFILE *, RCSNUM *, const char *);
static int rcsdiff_rev(RCSFILE *, RCSNUM *, RCSNUM *, const char *);

static int kflag = RCS_KWEXP_ERR;

int
rcsdiff_main(int argc, char **argv)
{
	int i, ch, status;
	RCSNUM *rev, *rev2, *frev;
	RCSFILE *file;
	char fpath[MAXPATHLEN];

	rev = RCS_HEAD_REV;
	rev2 = NULL;
	status = 0;

	strlcpy(diffargs, "diff", sizeof(diffargs));

	while ((ch = rcs_getopt(argc, argv, "ck:nqr:uVx:")) != -1) {
		switch (ch) {
		case 'c':
			strlcat(diffargs, " -c", sizeof(diffargs));
			diff_format = D_CONTEXT;
			break;
		case 'k':
			kflag = rcs_kflag_get(rcs_optarg);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid RCS keyword expansion mode");
				(usage)();
				exit(1);
			}
			break;
		case 'n':
			strlcat(diffargs, " -n", sizeof(diffargs));
			diff_format = D_RCSDIFF;
			break;
		case 'q':
			verbose = 0;
			break;
		case 'u':
			strlcat(diffargs, " -u", sizeof(diffargs));
			diff_format = D_UNIFIED;
			break;
		case 'r':
			if (rev == RCS_HEAD_REV) {
				if ((rev = rcsnum_parse(rcs_optarg)) == NULL) {
					cvs_log(LP_ERR, "bad revision number");
					exit(1);
				}
			} else {
				if ((rev2 = rcsnum_parse(rcs_optarg)) == NULL) {
					cvs_log(LP_ERR, "bad revision number");
					exit(1);
				}
			}
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
		case 'x':
			rcs_suffixes = rcs_optarg;
			break;
		default:
			(usage)();
			exit (1);
		}
	}

	argc -= rcs_optind;
	argv += rcs_optind;

	if (argc == 0) {
		cvs_log(LP_ERR, "no input file");
		(usage)();
		exit(1);
	}

	for (i = 0; i < argc; i++) {
		if (rcs_statfile(argv[i], fpath, sizeof(fpath)) < 0)
			continue;

		if ((file = rcs_open(fpath, RCS_READ)) == NULL)
			continue;

		if (rev == RCS_HEAD_REV)
			frev = file->rf_head;
		else
			frev = rev;

		if (verbose == 1) {
			fprintf(stderr, "%s\n", RCS_DIFF_DIV);
			fprintf(stderr, "RCS file: %s\n", fpath);
		}

		diff_file = argv[i];

		if (rev2 == NULL) {
			if (rcsdiff_file(file, frev, argv[i]) < 0) {
				rcs_close(file);
				status = 2;
				continue;
			}
		} else {
			if (rcsdiff_rev(file, rev, rev2, argv[i]) < 0) {
				rcs_close(file);
				status = 2;
				continue;
			}
		}

		rcs_close(file);
	}

	exit(status);
}

void
rcsdiff_usage(void)
{
	fprintf(stderr,
	    "usage: rcsdiff [-cnquV] [-kmode] [-rrev1 [-rrev2]] file ...\n");
}

static int
rcsdiff_file(RCSFILE *rfp, RCSNUM *rev, const char *filename)
{
	char path1[MAXPATHLEN], path2[MAXPATHLEN];
	BUF *b1, *b2;
	char rbuf[64];
	struct stat st;

	if (stat(filename, &st) == -1) {
		cvs_log(LP_ERRNO, "%s", filename);
		return (-1);
	}

	rcsnum_tostr(rev, rbuf, sizeof(rbuf));
	if (verbose == 1) {
		fprintf(stderr, "retrieving revision %s\n", rbuf);
		fprintf(stderr, "%s -r%s %s\n", diffargs, rbuf, filename);
	}

	if ((b1 = rcs_getrev(rfp, rev)) == NULL) {
		cvs_log(LP_ERR, "failed to retrieve revision");
		return (-1);
	}

	if ((b2 = cvs_buf_load(filename, BUF_AUTOEXT)) == NULL) {
		cvs_log(LP_ERR, "failed to load file: '%s'", filename);
		cvs_buf_free(b1);
		return (-1);
	}

	strlcpy(path1, rcs_tmpdir, sizeof(path1));
	strlcat(path1, "/diff1.XXXXXXXXXX", sizeof(path1));
	if (cvs_buf_write_stmp(b1, path1, 0600) == -1) {
		cvs_log(LP_ERRNO, "could not write temporary file");
		cvs_buf_free(b1);
		cvs_buf_free(b2);
		return (-1);
	}
	cvs_buf_free(b1);

	strlcpy(path2, rcs_tmpdir, sizeof(path2));
	strlcat(path2, "/diff2.XXXXXXXXXX", sizeof(path2));
	if (cvs_buf_write_stmp(b2, path2, 0600) == -1) {
		cvs_buf_free(b2);
		(void)unlink(path1);
		return (-1);
	}
	cvs_buf_free(b2);

	cvs_diffreg(path1, path2, NULL);
	(void)unlink(path1);
	(void)unlink(path2);

	return (0);
}

static int
rcsdiff_rev(RCSFILE *rfp, RCSNUM *rev1, RCSNUM *rev2, const char *filename)
{
	char path1[MAXPATHLEN], path2[MAXPATHLEN];
	BUF *b1, *b2;
	char rbuf1[64], rbuf2[64];

	rcsnum_tostr(rev1, rbuf1, sizeof(rbuf1));
	if (verbose == 1)
		printf("retrieving revision %s\n", rbuf1);

	if ((b1 = rcs_getrev(rfp, rev1)) == NULL) {
		cvs_log(LP_ERR, "failed to retrieve revision");
		return (-1);
	}

	rcsnum_tostr(rev2, rbuf2, sizeof(rbuf2));
	if (verbose == 1)
		fprintf(stderr, "retrieving revision %s\n", rbuf2);

	if ((b2 = rcs_getrev(rfp, rev2)) == NULL) {
		cvs_log(LP_ERR, "failed to retrieve revision");
		return (-1);
	}

	if (verbose == 1)
		fprintf(stderr,
		    "%s -r%s -r%s %s\n", diffargs, rbuf1, rbuf2, filename);

	strlcpy(path1, rcs_tmpdir, sizeof(path1));
	strlcat(path1, "/diff1.XXXXXXXXXX", sizeof(path1));
	if (cvs_buf_write_stmp(b1, path1, 0600) == -1) {
		cvs_log(LP_ERRNO, "could not write temporary file");
		cvs_buf_free(b1);
		cvs_buf_free(b2);
		return (-1);
	}
	cvs_buf_free(b1);

	strlcpy(path2, rcs_tmpdir, sizeof(path2));
	strlcat(path2, "/diff2.XXXXXXXXXX", sizeof(path2));
	if (cvs_buf_write_stmp(b2, path2, 0600) == -1) {
		cvs_buf_free(b2);
		(void)unlink(path1);
		return (-1);
	}
	cvs_buf_free(b2);

	cvs_diffreg(path1, path2, NULL);
	(void)unlink(path1);
	(void)unlink(path2);

	return (0);
}
