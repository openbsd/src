/*	$OpenBSD: tag.c,v 1.80 2010/10/28 12:30:27 tobias Exp $	*/
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

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "remote.h"

#define T_CHECK_UPTODATE	0x01
#define T_DELETE		0x02
#define T_FORCE_MOVE		0x04
#define T_BRANCH		0x08

void	cvs_tag_check_files(struct cvs_file *);
void	cvs_tag_local(struct cvs_file *);

static int tag_del(struct cvs_file *);
static int tag_add(struct cvs_file *);

struct file_info_list	files_info;

static int	 runflags = 0;
static char	*tag = NULL;
static char	*tag_date = NULL;
static char	*tag_name = NULL;
static char	*tag_oldname = NULL;

struct cvs_cmd cvs_cmd_rtag = {
	CVS_OP_RTAG, CVS_LOCK_REPO, "rtag",
	{ "rt", "rfreeze" },
	"Add a symbolic tag to a module",
	"[-bcdFflR] [-D date | -r rev] symbolic_tag module ...",
	"bcD:dFflRr:",
	NULL,
	cvs_tag
};

struct cvs_cmd cvs_cmd_tag = {
	CVS_OP_TAG, CVS_USE_WDIR | CVS_LOCK_REPO, "tag",
	{ "ta", "freeze" },
	"Add a symbolic tag to checked out version of files",
	"[-bcdFflR] [-D date | -r rev] symbolic_tag [file ...]",
	"bcD:dFflRr:",
	NULL,
	cvs_tag
};

int
cvs_tag(int argc, char **argv)
{
	int ch, flags, i;
	char repo[MAXPATHLEN];
	char *arg = ".";
	struct cvs_recursion cr;
	struct trigger_list *line_list;

	flags = CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmdop == CVS_OP_TAG ?
	    cvs_cmd_tag.cmd_opts : cvs_cmd_rtag.cmd_opts)) != -1) {
		switch (ch) {
		case 'b':
			runflags |= T_BRANCH;
			break;
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
			flags |= CR_RECURSE_DIRS;
			break;
		case 'r':
			tag_oldname = optarg;
			break;
		default:
			fatal("%s", cvs_cmdop == CVS_OP_TAG ?
			    cvs_cmd_tag.cmd_synopsis :
			    cvs_cmd_rtag.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (cvs_cmdop == CVS_OP_RTAG) {
		flags |= CR_REPO;

		if (argc < 2)
			fatal("%s", cvs_cmd_rtag.cmd_synopsis);

		for (i = 1; i < argc; i++) {
			if (argv[i][0] == '/')
				fatal("Absolute path name is invalid: %s",
				    argv[i]);
		}
        } else if (cvs_cmdop == CVS_OP_TAG && argc == 0)
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

		if (runflags & T_BRANCH)
			cvs_client_send_request("Argument -b");

		if (runflags & T_CHECK_UPTODATE)
			cvs_client_send_request("Argument -c");

		if (runflags & T_DELETE)
			cvs_client_send_request("Argument -d");

		if (runflags & T_FORCE_MOVE)
			cvs_client_send_request("Argument -F");

		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");

		if (tag_date != NULL)
			cvs_client_send_request("Argument -D%s", tag_date);

		if (tag_oldname != NULL)
			cvs_client_send_request("Argument -r%s", tag_oldname);

		cvs_client_send_request("Argument %s", tag_name);
	} else {
		if (cvs_cmdop == CVS_OP_RTAG &&
		    chdir(current_cvsroot->cr_dir) == -1)
			fatal("cvs_tag: %s", strerror(errno));

	}

	cr.flags = flags;

	cvs_get_repository_name(".", repo, MAXPATHLEN);
	line_list = cvs_trigger_getlines(CVS_PATH_TAGINFO, repo);
	if (line_list != NULL) {
		TAILQ_INIT(&files_info);
		cr.fileproc = cvs_tag_check_files;
		if (argc > 0)
			cvs_file_run(argc, argv, &cr);
		else
			cvs_file_run(1, &arg, &cr);

		if (cvs_trigger_handle(CVS_TRIGGER_TAGINFO, repo, NULL,
		    line_list, &files_info)) {
			cvs_log(LP_ERR, "Pre-tag check failed");
			cvs_trigger_freelist(line_list);
			goto bad;
		}
		cvs_trigger_freelist(line_list);
	}

	cr.fileproc = cvs_tag_local;

	if (cvs_cmdop == CVS_OP_TAG ||
	    current_cvsroot->cr_method == CVS_METHOD_LOCAL) {
		if (argc > 0)
			cvs_file_run(argc, argv, &cr);
		else
			cvs_file_run(1, &arg, &cr);
	}

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");

		cvs_client_send_request((cvs_cmdop == CVS_OP_RTAG) ?
		    "rtag" : "tag");

		cvs_client_get_responses();
	}

bad:
	if (line_list != NULL)
		cvs_trigger_freeinfo(&files_info);

	return (0);
}

void
cvs_tag_check_files(struct cvs_file *cf)
{
	RCSNUM *srev = NULL, *rev = NULL;
	char rbuf[CVS_REV_BUFSZ];
	struct file_info *fi;

	cvs_log(LP_TRACE, "cvs_tag_check_files(%s)", cf->file_path);

	cvs_file_classify(cf, tag);

	if (cf->file_type == CVS_DIR)
		return;

	if (runflags & T_CHECK_UPTODATE) {
		if (cf->file_status != FILE_UPTODATE &&
		    cf->file_status != FILE_CHECKOUT &&
		    cf->file_status != FILE_PATCH) {
			return;
		}
	}

	switch (cf->file_status) {
	case FILE_ADDED:
	case FILE_REMOVED:
		return;
	default:
		break;
	}

	if (cvs_cmdop == CVS_OP_TAG) {
		if (cf->file_ent == NULL)
			return;
		srev = cf->file_ent->ce_rev;
	} else
		srev = cf->file_rcsrev;

	rcsnum_tostr(srev, rbuf, sizeof(rbuf));
	fi = xcalloc(1, sizeof(*fi));
	fi->nrevstr = xstrdup(rbuf);
	fi->file_path = xstrdup(cf->file_path);

	if (tag_oldname != NULL)
		fi->tag_old = xstrdup(tag_oldname);
	else if (tag_date != NULL)
		fi->tag_old = xstrdup(tag_date);

	if ((rev = rcs_sym_getrev(cf->file_rcs, tag_name)) != NULL) {
		if (!rcsnum_differ(srev, rev))
			goto bad;
		rcsnum_tostr(rev, rbuf, sizeof(rbuf));
		fi->crevstr = xstrdup(rbuf);
		rcsnum_free(rev);
	} else if (runflags & T_DELETE)
		goto bad;

	fi->tag_new = xstrdup(tag_name);

	if (runflags & T_BRANCH)
		fi->tag_type = 'T';
	else if (runflags & T_DELETE)
		fi->tag_type = '?';
	else
		fi->tag_type = 'N';

	if (runflags & T_FORCE_MOVE)
		fi->tag_op = "mov";
	else if (runflags & T_DELETE)
		fi->tag_op = "del";
	else
		fi->tag_op = "add";

	TAILQ_INSERT_TAIL(&files_info, fi, flist);
	return;

bad:
	if (fi->file_path != NULL)
		xfree(fi->file_path);
	if (fi->crevstr != NULL)
		xfree(fi->crevstr);
	if (fi->nrevstr != NULL)
		xfree(fi->nrevstr);
	if (fi->tag_new != NULL)
		xfree(fi->tag_new);
	if (fi->tag_old != NULL)
		xfree(fi->tag_old);
	if (rev != NULL)
		rcsnum_free(rev);
	xfree(fi);
}

void
cvs_tag_local(struct cvs_file *cf)
{
	cvs_log(LP_TRACE, "cvs_tag_local(%s)", cf->file_path);

	cvs_file_classify(cf, tag);

	if (cf->file_type == CVS_DIR) {
		if (verbosity > 1) {
			cvs_log(LP_NOTICE, "%s %s",
			    (runflags & T_DELETE) ? "Untagging" : "Tagging",
			    cf->file_path);
		}
		return;
	}

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
		}
		return;
	}

	switch (cf->file_status) {
	case FILE_ADDED:
		if (verbosity > 1) {
			cvs_log(LP_NOTICE,
			    "couldn't tag added but un-commited file `%s'",
			    cf->file_path);
		}
		break;
	case FILE_REMOVED:
		if (verbosity > 1) {
			cvs_log(LP_NOTICE,
			    "skipping removed but un-commited file `%s'",
			    cf->file_path);
		}
		break;
	case FILE_CHECKOUT:
	case FILE_MODIFIED:
	case FILE_PATCH:
	case FILE_UPTODATE:
		if (tag_add(cf) == 0) {
			if (verbosity > 0)
				cvs_printf("T %s\n", cf->file_path);
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
	int ret;
	char revbuf[CVS_REV_BUFSZ], trevbuf[CVS_REV_BUFSZ];
	RCSNUM *srev, *trev;
	struct rcs_sym *sym;

	if (cf->file_rcs == NULL) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "cannot find revision "
			    "control file for `%s'", cf->file_name);
		return (-1);
	}

	if (cvs_cmdop == CVS_OP_TAG) {
		if (cf->file_ent == NULL)
			return (-1);
		srev = cf->file_ent->ce_rev;
	} else
		srev = cf->file_rcsrev;

	if (cvs_noexec == 1)
		return (0);

	(void)rcsnum_tostr(srev, revbuf, sizeof(revbuf));

	trev = rcs_sym_getrev(cf->file_rcs, tag_name);
	if (trev != NULL) {
		if (rcsnum_cmp(srev, trev, 0) == 0) {
			rcsnum_free(trev);
			return (-1);
		}
		(void)rcsnum_tostr(trev, trevbuf, sizeof(trevbuf));
		rcsnum_free(trev);

		if (!(runflags & T_FORCE_MOVE)) {
			cvs_printf("W %s : %s ", cf->file_path, tag_name);
			cvs_printf("already exists on version %s", trevbuf);
			cvs_printf(" : NOT MOVING tag to version %s\n", revbuf);

			return (-1);
		} else {
			sym = rcs_sym_get(cf->file_rcs, tag_name);
			rcsnum_cpy(srev, sym->rs_num, 0);
			cf->file_rcs->rf_flags &= ~RCS_SYNCED;

			return (0);
		}
	}

	if (runflags & T_BRANCH) {
		if ((trev = rcs_branch_new(cf->file_rcs, srev)) == NULL)
			fatal("Cannot create a new branch");
	} else {
		trev = rcsnum_alloc();
		rcsnum_cpy(srev, trev, 0);
	}

	if ((ret = rcs_sym_add(cf->file_rcs, tag_name, trev)) != 0) {
		if (ret != 1) {
			cvs_log(LP_NOTICE,
			    "failed to set tag %s to revision %s in %s",
			    tag_name, revbuf, cf->file_rcs->rf_path);
		}
		rcsnum_free(trev);
		return (-1);
	}

	rcsnum_free(trev);
	return (0);
}
