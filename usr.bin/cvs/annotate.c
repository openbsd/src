/*	$OpenBSD: annotate.c,v 1.64 2015/01/16 06:40:06 deraadt Exp $	*/
/*
 * Copyright (c) 2007 Tobias Stoeckmann <tobias@openbsd.org>
 * Copyright (c) 2006 Xavier Santolaria <xsa@openbsd.org>
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

#include <sys/types.h>
#include <sys/dirent.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cvs.h"
#include "remote.h"

void	cvs_annotate_local(struct cvs_file *);

extern char	*cvs_specified_tag;

static int	 force_head = 0;

struct cvs_cmd cvs_cmd_annotate = {
	CVS_OP_ANNOTATE, CVS_USE_WDIR, "annotate",
	{ "ann", "blame" },
	"Show last revision where each line was modified",
	"[-flR] [-D date | -r rev] [file ...]",
	"D:flRr:",
	NULL,
	cvs_annotate
};

struct cvs_cmd cvs_cmd_rannotate = {
	CVS_OP_RANNOTATE, 0, "rannotate",
	{ "rann", "ra" },
	"Show last revision where each line was modified",
	"[-flR] [-D date | -r rev] module ...",
	"D:flRr:",
	NULL,
	cvs_annotate
};

int
cvs_annotate(int argc, char **argv)
{
	int ch, flags;
	char *arg = ".";
	char *dateflag = NULL;
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmdop == CVS_OP_ANNOTATE ?
	    cvs_cmd_annotate.cmd_opts : cvs_cmd_rannotate.cmd_opts)) != -1) {
		switch (ch) {
		case 'D':
			dateflag = optarg;
			if ((cvs_specified_date = date_parse(dateflag)) == -1)
				fatal("invalid date: %s", dateflag);
			break;
		case 'f':
			force_head = 1;
			break;
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'R':
			flags |= CR_RECURSE_DIRS;
			break;
		case 'r':
			cvs_specified_tag = optarg;
			break;
		default:
			fatal("%s", cvs_cmdop == CVS_OP_ANNOTATE ?
			    cvs_cmd_annotate.cmd_synopsis :
			    cvs_cmd_rannotate.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (cvs_cmdop == CVS_OP_RANNOTATE) {
		if (argc == 0)
			fatal("%s", cvs_cmd_rannotate.cmd_synopsis);

		flags |= CR_REPO;
	}

	cr.enterdir = NULL;
	cr.leavedir = NULL;

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_connect_to_server();
		cr.fileproc = cvs_client_sendfile;

		if (dateflag != NULL)
			cvs_client_send_request("Argument -D%s", dateflag);

		if (force_head == 1)
			cvs_client_send_request("Argument -f");

		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");

		if (cvs_specified_tag != NULL)
			cvs_client_send_request("Argument -r%s",
			    cvs_specified_tag);
	} else {
		if (cvs_cmdop == CVS_OP_RANNOTATE &&
		    chdir(current_cvsroot->cr_dir) == -1)
			fatal("cvs_annotate: %s", strerror(errno));

		cr.fileproc = cvs_annotate_local;
	}

	cr.flags = flags;

	if (cvs_cmdop == CVS_OP_ANNOTATE ||
	    current_cvsroot->cr_method == CVS_METHOD_LOCAL) {
		if (argc > 0)
			cvs_file_run(argc, argv, &cr);
		else
			cvs_file_run(1, &arg, &cr);
	}

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");

		cvs_client_send_request((cvs_cmdop == CVS_OP_RANNOTATE) ?
		    "rannotate" : "annotate");

		cvs_client_get_responses();
	}

	return (0);
}

void
cvs_annotate_local(struct cvs_file *cf)
{
	int i;
	char date[10], rnum[13], *p;
	RCSNUM *bnum, *rev;
	struct rcs_line *line;
	struct rcs_line **alines;

	cvs_log(LP_TRACE, "cvs_annotate_local(%s)", cf->file_path);

	cvs_file_classify(cf, cvs_directory_tag);

	if (cf->file_rcs == NULL || cf->file_rcs->rf_head == NULL)
		return;

	if (cvs_specified_tag != NULL) {
		if ((rev = rcs_translate_tag(cvs_specified_tag,
		    cf->file_rcs)) == NULL) {
			if (!force_head)
				/* Stick at weird GNU cvs, ignore error. */
				return;

			/* -f is not allowed for unknown symbols */
			rev = rcsnum_parse(cvs_specified_tag);
			if (rev == NULL)
				fatal("no such tag %s", cvs_specified_tag);
                        rcsnum_free(rev);
			rev = rcsnum_alloc();
			rcsnum_cpy(cf->file_rcs->rf_head, rev, 0);
		}

		/*
		 * If this is a revision in a branch, we have to go first
		 * from HEAD to branch, then down to 1.1. After that, take
		 * annotated branch and go up to branch revision. This must
		 * be done this way due to different handling of "a" and
		 * "d" in rcs file for annotation.
		 */
		if (!RCSNUM_ISBRANCHREV(rev)) {
			bnum = rev;
		} else {
			bnum = rcsnum_alloc();
			rcsnum_cpy(rev, bnum, 2);
		}

		rcs_rev_getlines(cf->file_rcs, bnum, &alines);

		/*
		 * Go into branch and receive annotations for branch revision,
		 * with inverted "a" and "d" meaning.
		 */
		if (bnum != rev) {
			rcs_annotate_getlines(cf->file_rcs, rev, &alines);
			rcsnum_free(bnum);
		}
		rcsnum_free(rev);
	} else {
		rcs_rev_getlines(cf->file_rcs, (cvs_specified_date != -1 ||
		    cvs_directory_date != -1) ? cf->file_rcsrev :
		    cf->file_rcs->rf_head, &alines);
	}

	/* Stick at weird GNU cvs, ignore error. */
	if (alines == NULL)
		return;

	cvs_log(LP_RCS, "Annotations for %s", cf->file_path);
	cvs_log(LP_RCS, "***************");

	for (i = 0; alines[i] != NULL; i++) {
		line = alines[i];

		rcsnum_tostr(line->l_delta->rd_num, rnum, sizeof(rnum));
		strftime(date, sizeof(date), "%d-%b-%y",
		    &(line->l_delta->rd_date));
		if (line->l_len && line->l_line[line->l_len - 1] == '\n')
			line->l_line[line->l_len - 1] = '\0';
		else {
			p = xmalloc(line->l_len + 1);
			memcpy(p, line->l_line, line->l_len);
			p[line->l_len] = '\0';

			if (line->l_needsfree)
				xfree(line->l_line);
			line->l_line = p;
			line->l_len++;
			line->l_needsfree = 1;
		}
		cvs_printf("%-12.12s (%-8.8s %s): %s\n", rnum,
		    line->l_delta->rd_author, date, line->l_line);

		if (line->l_needsfree)
			xfree(line->l_line);
		xfree(line);
	}

	xfree(alines);
}
