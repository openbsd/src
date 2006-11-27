/*	$OpenBSD: annotate.c,v 1.32 2006/11/27 16:02:52 xsa Exp $	*/
/*
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

#include "includes.h"

#include "cvs.h"
#include "log.h"
#include "remote.h"

void	cvs_annotate_local(struct cvs_file *);

static int	 force_head = 0;
static char	*rev = NULL;

struct cvs_cmd cvs_cmd_annotate = {
	CVS_OP_ANNOTATE, 0, "annotate",
	{ "ann", "blame" },
	"Show last revision where each line was modified",
	"[-flR] [-D date | -r rev] [file ...]",
	"D:flRr:",
	NULL,
	cvs_annotate
};

int
cvs_annotate(int argc, char **argv)
{
	int ch, flags;
	char *arg = ".";
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmd_annotate.cmd_opts)) != -1) {
		switch (ch) {
		case 'D':
			break;
		case 'f':
			force_head = 1;
			break;
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'R':
			break;
		case 'r':
			rev = optarg;
			break;
		default:
			fatal("%s", cvs_cmd_annotate.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	cr.enterdir = NULL;
	cr.leavedir = NULL;

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cr.fileproc = cvs_client_sendfile;

		if (force_head == 1)
			cvs_client_send_request("Argument -f");

		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");

		if (rev != NULL)
			cvs_client_send_request("Argument -r%s", rev);
	} else {
		cr.fileproc = cvs_annotate_local;
	}

	cr.flags = flags;

	if (argc > 0)
		cvs_file_run(argc, argv, &cr);
	else
		cvs_file_run(1, &arg, &cr);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");
		cvs_client_send_request("annotate");
		cvs_client_get_responses();
	}

	return (0);
}

void
cvs_annotate_local(struct cvs_file *cf)
{
	struct cvs_line *lp;
	struct cvs_lines *lines;
	BUF *b;
	RCSNUM *ann_rev;
	char *content;

	ann_rev = NULL;

	cvs_log(LP_TRACE, "cvs_annotate_local(%s)", cf->file_path);

	cvs_file_classify(cf, NULL, 0);

	if (cf->file_status == FILE_UNKNOWN ||
	    cf->file_status == FILE_UNLINK)
		return;

	cvs_printf("Annotations for %s", cf->file_name);
	cvs_printf("\n***************\n");

	if (rev != NULL)
		ann_rev = rcs_translate_tag(rev, cf->file_rcs);
	else {
		ann_rev = rcsnum_alloc();
		rcsnum_cpy(cf->file_rcs->rf_head, ann_rev, 0);
	}

	b = rcs_getrev(cf->file_rcs, ann_rev);
	cvs_buf_putc(b, '\0');

	content = cvs_buf_release(b);
	if ((lines = cvs_splitlines(content)) == NULL)
		fatal("cvs_annotate_local: cvs_splitlines failed");

        xfree(content);

	/* XXX */
	TAILQ_FOREACH(lp, &(lines->l_lines), l_list) {
		if (lp->l_line == NULL)
			continue;

		cvs_printf("%s\n", lp->l_line);
	}
        cvs_freelines(lines);

	if (ann_rev != NULL)
		rcsnum_free(ann_rev);
}
