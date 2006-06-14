/*	$OpenBSD: diff.c,v 1.103 2006/06/14 20:28:53 joris Exp $	*/
/*
 * Copyright (c) 2006 Joris Vink <joris@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"

#include "cvs.h"
#include "diff.h"
#include "log.h"
#include "proto.h"

int	cvs_diff(int, char **);
void	cvs_diff_local(struct cvs_file *);

static int Nflag = 0;
static char *rev1 = NULL;
static char *rev2 = NULL;

struct cvs_cmd cvs_cmd_diff = {
	CVS_OP_DIFF, CVS_REQ_DIFF, "diff",
	{ "di", "dif" },
	"Show differences between revisions",
	"[-cilNnpu] [[-D date] [-r rev] [-D date2 | -r rev2]] "
	"[-k mode] [file ...]",
	"cD:iklNnpr:Ru",
	NULL,
	cvs_diff
};

int
cvs_diff(int argc, char **argv)
{
	int ch;
	char *arg = ".";
	int flags;
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;
	strlcpy(diffargs, argv[0], sizeof(diffargs));

	while ((ch = getopt(argc, argv, cvs_cmd_diff.cmd_opts)) != -1) {
		switch (ch) {
		case 'c':
			strlcat(diffargs, " -c", sizeof(diffargs));
			diff_format = D_CONTEXT;
			break;
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'n':
			strlcat(diffargs, " -n", sizeof(diffargs));
			diff_format = D_RCSDIFF;
			break;
		case 'N':
			strlcat(diffargs, " -N", sizeof(diffargs));
			Nflag = 1;
			break;
		case 'p':
			strlcat(diffargs, " -p", sizeof(diffargs));
			diff_pflag = 1;
			break;
		case 'r':
			if (rev1 == NULL) {
				rev1 = optarg;
			} else if (rev2 == NULL) {
				rev2 = optarg;
			} else {
				fatal("no more than 2 revisions/dates can"
				    " be specified");
			}
			break;
		case 'u':
			strlcat(diffargs, " -u", sizeof(diffargs));
			diff_format = D_UNIFIED;
			break;
		default:
			fatal("%s", cvs_cmd_diff.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	cr.enterdir = NULL;
	cr.leavedir = NULL;
	cr.local = cvs_diff_local;
	cr.remote = NULL;
	cr.flags = flags;

	diff_rev1 = diff_rev2 = NULL;

	if (argc > 0)
		cvs_file_run(argc, argv, &cr);
	else
		cvs_file_run(1, &arg, &cr);

	return (0);
}

void
cvs_diff_local(struct cvs_file *cf)
{
	size_t len;
	RCSNUM *r1;
	BUF *b1, *b2;
	struct stat st;
	struct timeval tv[2], tv2[2];
	char rbuf[16], p1[MAXPATHLEN], p2[MAXPATHLEN];

	cvs_log(LP_TRACE, "cvs_diff_local(%s)", cf->file_path);

	if (cf->file_type == CVS_DIR) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "Diffing inside %s", cf->file_path);
		return;
	}

	cvs_file_classify(cf, NULL, 0);

	if (cf->file_status == FILE_LOST) {
		cvs_log(LP_ERR, "cannot find file %s", cf->file_path);
		return;
	} else if (cf->file_status == FILE_UNKNOWN) {
		cvs_log(LP_ERR, "I know nothing about %s", cf->file_path);
		return;
	} else if (cf->file_status == FILE_ADDED && Nflag == 0) {
		cvs_log(LP_ERR, "%s is a new entry, no comparison available",
		    cf->file_path);
		return;
	} else if (cf->file_status == FILE_REMOVED && Nflag == 0) {
		cvs_log(LP_ERR, "%s was removed, no comparison available",
		    cf->file_path);
		return;
	} else if (cf->file_status == FILE_UPTODATE && rev1 == NULL &&
	     rev2 == NULL) {
		return;
	}

	if (rev1 != NULL)
		diff_rev1 = rcs_translate_tag(rev1, cf->file_rcs);
	if (rev2 != NULL)
		diff_rev2 = rcs_translate_tag(rev2, cf->file_rcs);

	diff_file = cf->file_path;
	cvs_printf("Index: %s\n%s\nRCS file: %s\n", cf->file_path,
	    RCS_DIFF_DIV, cf->file_rpath);

	if (cf->file_status != FILE_ADDED) {
		if (diff_rev1 != NULL)
			r1 = diff_rev1;
		else
			r1 = cf->file_ent->ce_rev;

		diff_rev1 = r1;
		rcsnum_tostr(r1, rbuf , sizeof(rbuf));
		cvs_printf("retrieving revision %s\n", rbuf);
		if ((b1 = rcs_getrev(cf->file_rcs, r1)) == NULL)
			fatal("failed to retrieve revision %s", rbuf);

		b1 = rcs_kwexp_buf(b1, cf->file_rcs, r1);

		tv[0].tv_sec = rcs_rev_getdate(cf->file_rcs, r1);
		tv[0].tv_usec = 0;
		tv[1] = tv[0];
	}

	if (diff_rev2 != NULL && cf->file_status != FILE_ADDED &&
	    cf->file_status != FILE_REMOVED) {
		rcsnum_tostr(diff_rev2, rbuf, sizeof(rbuf));
		cvs_printf("retrieving revision %s\n", rbuf);
		if ((b2 = rcs_getrev(cf->file_rcs, diff_rev2)) == NULL)
			fatal("failed to retrieve revision %s", rbuf);

		b2 = rcs_kwexp_buf(b2, cf->file_rcs, diff_rev2);

		tv2[0].tv_sec = rcs_rev_getdate(cf->file_rcs, diff_rev2);
		tv2[0].tv_usec = 0;
		tv2[1] = tv2[0];
	} else if (cf->file_status != FILE_REMOVED) {
		if (fstat(cf->fd, &st) == -1)
			fatal("fstat failed %s", strerror(errno));
		if ((b2 = cvs_buf_load(cf->file_path, BUF_AUTOEXT)) == NULL)
			fatal("failed to load %s", cf->file_path);

		st.st_mtime = cvs_hack_time(st.st_mtime, 1);
		tv2[0].tv_sec = st.st_mtime;
		tv2[0].tv_usec = 0;
		tv2[1] = tv2[0];
	}

	cvs_printf("%s", diffargs);

	if (cf->file_status != FILE_ADDED) {
		rcsnum_tostr(r1, rbuf, sizeof(rbuf));
		cvs_printf(" -r%s", rbuf);

		if (diff_rev2 != NULL) {
			rcsnum_tostr(diff_rev2, rbuf, sizeof(rbuf));
			cvs_printf(" -r%s", rbuf);
		}
	}

	cvs_printf(" %s\n", cf->file_path);

	if (cf->file_status != FILE_ADDED) {
		len = strlcpy(p1, cvs_tmpdir, sizeof(p1));
		if (len >= sizeof(p1))
		fatal("cvs_diff_local: truncation");

		len = strlcat(p1, "/diff1.XXXXXXXXXX", sizeof(p1));
		if (len >= sizeof(p1))
			fatal("cvs_diff_local: truncation");

		cvs_buf_write_stmp(b1, p1, 0600, tv);
		cvs_buf_free(b1);
	} else {
		len = strlcpy(p1, CVS_PATH_DEVNULL, sizeof(p1));
		if (len >= sizeof(p1))
			fatal("cvs_diff_local: truncation");
	}

	if (cf->file_status != FILE_REMOVED) {
		len = strlcpy(p2, cvs_tmpdir, sizeof(p2));
		if (len >= sizeof(p2))
			fatal("cvs_diff_local: truncation");

		len = strlcat(p2, "/diff2.XXXXXXXXXX", sizeof(p2));
		if (len >= sizeof(p2))
			fatal("cvs_diff_local: truncation");

		cvs_buf_write_stmp(b2, p2, 0600, tv2);
		cvs_buf_free(b2);
	} else {
		len = strlcpy(p2, CVS_PATH_DEVNULL, sizeof(p2));
		if (len >= sizeof(p2))
			fatal("cvs_diff_local: truncation");
	}

	cvs_diffreg(p1, p2, NULL);
	cvs_worklist_run(&temp_files, cvs_worklist_unlink);

	if (diff_rev1 != NULL && diff_rev1 != cf->file_ent->ce_rev)
		rcsnum_free(diff_rev1);
	if (diff_rev2 != NULL && diff_rev2 != cf->file_rcsrev)
		rcsnum_free(diff_rev2);

	diff_rev1 = diff_rev2 = NULL;
}
