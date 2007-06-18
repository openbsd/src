/*	$OpenBSD: tag.c,v 1.56 2007/06/18 17:54:13 joris Exp $	*/
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

#include <unistd.h>

#include "cvs.h"
#include "remote.h"

#define T_CHECK_UPTODATE	0x01
#define T_DELETE		0x02
#define T_FORCE_MOVE		0x04

void	cvs_tag_local(struct cvs_file *);

static int tag_del(struct cvs_file *);
static int tag_add(struct cvs_file *);

static int	 runflags = 0;
static char	*tag = NULL;
static char	*tag_date = NULL;
static char	*tag_name = NULL;
static char	*tag_oldname = NULL;

struct cvs_cmd cvs_cmd_tag = {
	CVS_OP_TAG, 0, "tag",
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
	char *arg = ".";
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmd_tag.cmd_opts)) != -1) {
		switch (ch) {
		case 'c':
			runflags |= T_CHECK_UPTODATE;
			break;
		case 'D':
			tag_date = optarg;
			break;
		case 'd':
			runflags |= T_DELETE;
			break;
		case 'F':
			runflags |= T_FORCE_MOVE;
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
		if (runflags & T_DELETE)
			tag_oldname = NULL;
		else
			tag = tag_oldname;
	}

	if (tag_date != NULL) {
		if (runflags & T_DELETE)
			tag_date = NULL;
		else
			tag = tag_date;
	}

	if (tag_oldname != NULL && tag_date != NULL)
		fatal("-r and -D options are mutually exclusive");

	cr.enterdir = NULL;
	cr.leavedir = NULL;

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_connect_to_server();
		cr.fileproc = cvs_client_sendfile;

		if (runflags & T_CHECK_UPTODATE)
			cvs_client_send_request("Argument -c");

		if (runflags & T_DELETE)
			cvs_client_send_request("Argument -d");

		if (runflags & T_FORCE_MOVE)
			cvs_client_send_request("Argument -F");

		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");

		if (tag_oldname != NULL)
			cvs_client_send_request("Argument -r%s", tag_oldname);

		cvs_client_send_request("Argument %s", tag_name);
	} else {
		cr.fileproc = cvs_tag_local;
	}

	cr.flags = flags;

	if (argc > 0)
		cvs_file_run(argc, argv, &cr);
	else
		cvs_file_run(1, &arg, &cr);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");
		cvs_client_send_request("tag");
		cvs_client_get_responses();
	}

	return (0);
}

void
cvs_tag_local(struct cvs_file *cf)
{
	cvs_log(LP_TRACE, "cvs_tag_local(%s)", cf->file_path);

	if (cf->file_type == CVS_DIR) {
		if (verbosity > 1) {
			cvs_log(LP_NOTICE, "%s %s",
			    (runflags & T_DELETE) ? "Untagging" : "Tagging",
			    cf->file_path);
		}
		return;
	}

	cvs_file_classify(cf, tag);

	if (runflags & T_CHECK_UPTODATE) {
		if (cf->file_status != FILE_UPTODATE &&
		    cf->file_status != FILE_CHECKOUT &&
		    cf->file_status != FILE_PATCH) {
			cvs_log(LP_NOTICE,
			    "%s is locally modified", cf->file_path);
			return;
		}
	}

	if (runflags & T_DELETE) {
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
			cvs_history_add(CVS_HISTORY_TAG, cf, tag_name);
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
	char revbuf[16], trevbuf[16];
	RCSNUM *trev;
	struct rcs_sym *sym;

	if (cf->file_rcs == NULL) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "cannot find revision "
			    "control file for `%s'", cf->file_name);
		return (-1);
	}

	if (cvs_noexec == 1)
		return (0);

	trev = rcs_sym_getrev(cf->file_rcs, tag_name);
	if (trev != NULL) {
		if (rcsnum_cmp(cf->file_rcsrev, trev, 0) == 0) {
			rcsnum_free(trev);
			return (-1);
		}
		(void)rcsnum_tostr(trev, trevbuf, sizeof(trevbuf));

		if (!(runflags & T_FORCE_MOVE)) {
			cvs_printf("W %s : %s ", cf->file_path, tag_name);
			cvs_printf("already exists on version %s", trevbuf);
			cvs_printf(" : NOT MOVING tag to version %s\n", revbuf);

			return (-1);
		} else if (runflags & T_FORCE_MOVE) {
			sym = rcs_sym_get(cf->file_rcs, tag_name);
			rcsnum_cpy(cf->file_rcsrev, sym->rs_num, 0);
			cf->file_rcs->rf_flags &= ~RCS_SYNCED;

			return (0);
		}
	}

	if (rcs_sym_add(cf->file_rcs, tag_name, cf->file_rcsrev) == -1) {
		if (rcs_errno != RCS_ERR_DUPENT) {
			(void)rcsnum_tostr(cf->file_rcsrev, revbuf,
			    sizeof(revbuf));
			cvs_log(LP_NOTICE,
			    "failed to set tag %s to revision %s in %s",
			    tag_name, revbuf, cf->file_rcs->rf_path);
		}
		return (-1);
	}

	return (0);
}
