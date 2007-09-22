/*	$OpenBSD: diff.c,v 1.121 2007/09/22 16:01:22 joris Exp $	*/
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

#include <sys/stat.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "diff.h"
#include "remote.h"

void	cvs_diff_local(struct cvs_file *);

static int Nflag = 0;
static char *rev1 = NULL;
static char *rev2 = NULL;

struct cvs_cmd cvs_cmd_diff = {
	CVS_OP_DIFF, 0, "diff",
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

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_connect_to_server();
		cr.fileproc = cvs_client_sendfile;

		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");

		switch (diff_format) {
		case D_CONTEXT:
			cvs_client_send_request("Argument -c");
			break;
		case D_RCSDIFF:
			cvs_client_send_request("Argument -n");
			break;
		case D_UNIFIED:
			cvs_client_send_request("Argument -u");
			break;
		default:
			break;
		}

		if (Nflag == 1)
			cvs_client_send_request("Argument -N");

		if (diff_pflag == 1)
			cvs_client_send_request("Argument -p");

		if (rev1 != NULL)
			cvs_client_send_request("Argument -r%s", rev1);
		if (rev2 != NULL)
			cvs_client_send_request("Argument -r%s", rev2);
	} else {
		cr.fileproc = cvs_diff_local;
	}

	cr.flags = flags;

	diff_rev1 = diff_rev2 = NULL;

	if (argc > 0)
		cvs_file_run(argc, argv, &cr);
	else
		cvs_file_run(1, &arg, &cr);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");
		cvs_client_send_request("diff");
		cvs_client_get_responses();
	}

	return (0);
}

void
cvs_diff_local(struct cvs_file *cf)
{
	RCSNUM *r1;
	BUF *b1;
	struct stat st;
	struct timeval tv[2], tv2[2];
	char rbuf[CVS_REV_BUFSZ], *p1, *p2;

	r1 = NULL;
	b1 = NULL;

	cvs_log(LP_TRACE, "cvs_diff_local(%s)", cf->file_path);

	if (cf->file_type == CVS_DIR) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "Diffing inside %s", cf->file_path);
		return;
	}

	cvs_file_classify(cf, cvs_directory_tag);

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
		if ((diff_rev1 = rcs_translate_tag(rev1, cf->file_rcs)) == NULL)
			return;

	if (rev2 != NULL)
		if ((diff_rev2 = rcs_translate_tag(rev2, cf->file_rcs)) == NULL)
			return;

	diff_file = cf->file_path;
	cvs_printf("Index: %s\n%s\nRCS file: %s\n", cf->file_path,
	    RCS_DIFF_DIV, cf->file_rpath);

	(void)xasprintf(&p1, "%s/diff1.XXXXXXXXXX", cvs_tmpdir);
	(void)xasprintf(&p2, "%s/diff2.XXXXXXXXXX", cvs_tmpdir);

	if (cf->file_status != FILE_ADDED) {
		if (diff_rev1 != NULL)
			r1 = diff_rev1;
		else
			r1 = cf->file_ent->ce_rev;

		diff_rev1 = r1;
		rcsnum_tostr(r1, rbuf , sizeof(rbuf));

		tv[0].tv_sec = rcs_rev_getdate(cf->file_rcs, r1);
		tv[0].tv_usec = 0;
		tv[1] = tv[0];

		cvs_printf("Retrieving revision %s\n", rbuf);
		rcs_rev_write_stmp(cf->file_rcs, r1, p1, 0);
	}

	if (diff_rev2 != NULL && cf->file_status != FILE_ADDED &&
	    cf->file_status != FILE_REMOVED) {
		rcsnum_tostr(diff_rev2, rbuf, sizeof(rbuf));

		tv2[0].tv_sec = rcs_rev_getdate(cf->file_rcs, diff_rev2);
		tv2[0].tv_usec = 0;
		tv2[1] = tv2[0];

		cvs_printf("Retrieving revision %s\n", rbuf);
		rcs_rev_write_stmp(cf->file_rcs, diff_rev2, p2, 0);
	} else if (cf->file_status != FILE_REMOVED) {
		if (cvs_server_active == 1 &&
		    cf->file_status != FILE_MODIFIED) {
			rcs_rev_write_stmp(cf->file_rcs,
			    cf->file_rcsrev, p2, 0);
		} else {
			if (fstat(cf->fd, &st) == -1)
				fatal("fstat failed %s", strerror(errno));
			if ((b1 = cvs_buf_load_fd(cf->fd, BUF_AUTOEXT)) == NULL)
				fatal("failed to load %s", cf->file_path);

			tv2[0].tv_sec = st.st_mtime;
			tv2[0].tv_usec = 0;
			tv2[1] = tv2[0];

			cvs_buf_write_stmp(b1, p2, tv2);
			cvs_buf_free(b1);
		}
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

	if (cf->file_status == FILE_ADDED) {
		xfree(p1);
		(void)xasprintf(&p1, "%s", CVS_PATH_DEVNULL);
	} else if (cf->file_status == FILE_REMOVED) {
		xfree(p2);
		(void)xasprintf(&p2, "%s", CVS_PATH_DEVNULL);
	}

	if (cvs_diffreg(p1, p2, NULL) == D_ERROR)
		fatal("cvs_diff_local: failed to get RCS patch");

	cvs_worklist_run(&temp_files, cvs_worklist_unlink);

	if (p1 != NULL)
		xfree(p1);
	if (p2 != NULL)
		xfree(p2);

	if (diff_rev1 != NULL && diff_rev1 != cf->file_ent->ce_rev)
		rcsnum_free(diff_rev1);
	if (diff_rev2 != NULL && diff_rev2 != cf->file_rcsrev)
		rcsnum_free(diff_rev2);

	diff_rev1 = diff_rev2 = NULL;
}
