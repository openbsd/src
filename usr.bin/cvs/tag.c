/*	$OpenBSD: tag.c,v 1.45 2006/06/07 18:19:07 xsa Exp $	*/
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
#include "proto.h"

int	cvs_tag(int, char **);
void	cvs_tag_local(struct cvs_file *);

static int tag_del(struct cvs_file *);
static int tag_add(struct cvs_file *);

static int	tag_delete = 0;
static char	*tag = NULL;
static char	*tag_date = NULL;
static char	*tag_name = NULL;
static char	*tag_oldname = NULL;
static RCSNUM	*tag_rev = NULL;

struct cvs_cmd cvs_cmd_tag = {
	CVS_OP_TAG, CVS_REQ_TAG, "tag",
	{ "ta", "freeze" },
	"Add a symbolic tag to checked out version of files",
	"[-bcdFflR] [-D date | -r rev] tag [file ...]",
	"bcD:dFflRr:",
	NULL,
	cvs_tag
};

int
cvs_tag(int argc, char **argv)
{
	int ch, flags;
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmd_tag.cmd_opts)) != -1) {
		switch (ch) {
		case 'D':
			tag_date = optarg;
			break;
		case 'd':
			tag_delete = 1;
			break;
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'R':
			break;
		case 'r':
			tag_oldname = optarg;
			break;
		default:
			fatal("%s", cvs_cmd_tag.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		fatal("%s", cvs_cmd_tag.cmd_synopsis);

	tag_name = argv[0];
	argc--;
	argv++;

	if (!rcs_sym_check(tag_name))
		fatal("tag `%s' must not contain the characters `%s'",
		    tag_name, RCS_SYM_INVALCHAR);

	if (tag_oldname != NULL) {
		if (tag_delete == 1)
			tag_oldname = NULL;
		else
			tag = tag_oldname;
	}

	if (tag_date != NULL) {
		if (tag_delete == 1)
			tag_date = NULL;
		else
			tag = tag_date;
	}

	if (tag_oldname != NULL && tag_date != NULL)
		fatal("-r and -D options are mutually exclusive");

	cr.enterdir = NULL;
	cr.leavedir = NULL;
	cr.local = cvs_tag_local;
	cr.remote = NULL;
	cr.flags = flags;

	cvs_file_run(argc, argv, &cr);

	return (0);
}

void
cvs_tag_local(struct cvs_file *cf)
{
	cvs_log(LP_TRACE, "cvs_tag_local(%s)", cf->file_path);

	if (cf->file_type == CVS_DIR) {
		if (verbosity > 1) {
			cvs_log(LP_NOTICE, "%s %s",
			    (tag_delete == 1) ? "Untagging" : "Tagging",
			    cf->file_path);
		}
		return;
	}

	cvs_file_classify(cf, tag, 0);

	if (tag_delete == 1) {
		if (tag_del(cf) == 0) {
			if (verbosity > 0)
				cvs_printf("D %s\n", cf->file_path);

			rcs_write(cf->file_rcs);
		}
		return;
	}

	switch(cf->file_status) {
	case FILE_ADDED:
		if (verbosity > 1) {
			cvs_log(LP_NOTICE,
			    "couldn't tag added but un-commited file `%s'",
			    cf->file_path);
		}
		return;
	case FILE_REMOVED:
		if (verbosity > 1) {
			cvs_log(LP_NOTICE,
			    "skipping removed but un-commited file `%s'",
			    cf->file_path);
		}
		return;
	case FILE_CHECKOUT:
	case FILE_MODIFIED:
	case FILE_UPTODATE:
		if (tag_add(cf) == 0) {
			if (verbosity > 0)
				cvs_printf("T %s\n", cf->file_path);

			rcs_write(cf->file_rcs);
		}
		break;
	default:
		break;
	}
}

static int
tag_del(struct cvs_file *cf)
{
	if (cf->file_rcs == NULL)
		return (-1);

	if (cvs_noexec == 1)
		return (0);

	return (rcs_sym_remove(cf->file_rcs, tag_name));
}

static int
tag_add(struct cvs_file *cf)
{
	char revbuf[16];

	if (cf->file_rcs == NULL) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "cannot find revision "
			    "control file for `%s'", cf->file_name);
		return (-1);
	}

	if (cvs_noexec == 1)
		return (0);

	if (rcs_sym_add(cf->file_rcs, tag_name, cf->file_rcsrev) == -1) {
		if (rcs_errno != RCS_ERR_DUPENT) {
			(void)rcsnum_tostr(tag_rev, revbuf, sizeof(revbuf));
			cvs_log(LP_NOTICE,
			    "failed to set tag %s to revision %s in %s",
			    tag_name, revbuf, cf->file_rcs->rf_path);
		}
		return (-1);
	}

	return (0);
}
