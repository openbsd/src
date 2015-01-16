/*	$OpenBSD: admin.c,v 1.65 2015/01/16 06:40:06 deraadt Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * Copyright (c) 2005 Joris Vink <joris@openbsd.org>
 * Copyright (c) 2006, 2007 Xavier Santolaria <xsa@openbsd.org>
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
#include <fcntl.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>

#include "cvs.h"
#include "remote.h"

#define ADM_EFLAG	0x01

void	cvs_admin_local(struct cvs_file *);

struct cvs_cmd cvs_cmd_admin = {
	CVS_OP_ADMIN, CVS_USE_WDIR | CVS_LOCK_REPO, "admin",
	{ "adm", "rcs" },
	"Administrative front-end for RCS",
	"[-ILqU] [-A oldfile] [-a users] [-b branch]\n"
	"[-c string] [-e [users]] [-k mode] [-l [rev]] [-m rev:msg]\n"
	"[-N tag[:rev]] [-n tag[:rev]] [-o rev] [-s state[:rev]]"
	"[-t file | str]\n"
	"[-u [rev]] file ...",
	"A:a:b::c:e::Ik:l::Lm:N:n:o:qs:t:Uu::",
	NULL,
	cvs_admin
};

static int	 runflags = 0;
static int	 lkmode = RCS_LOCK_INVAL;
static char	*alist, *comment, *elist, *logmsg, *logstr, *koptstr;
static char	*oldfilename, *orange, *state, *staterevstr;

int
cvs_admin(int argc, char **argv)
{
	int ch;
	int flags;
	char *statestr;
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;

	alist = comment = elist = logmsg = logstr = NULL;
	oldfilename = orange = state = statestr = NULL;

	while ((ch = getopt(argc, argv, cvs_cmd_admin.cmd_opts)) != -1) {
		switch (ch) {
		case 'A':
			oldfilename = optarg;
			break;
		case 'a':
			alist = optarg;
			break;
		case 'b':
			break;
		case 'c':
			comment = optarg;
			break;
		case 'e':
			elist = optarg;
			runflags |= ADM_EFLAG;
			break;
		case 'I':
			break;
		case 'k':
			koptstr = optarg;
			kflag = rcs_kflag_get(koptstr);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid RCS keyword expansion mode");
				fatal("%s", cvs_cmd_admin.cmd_synopsis);
			}
			break;
		case 'L':
			if (lkmode == RCS_LOCK_LOOSE) {
				cvs_log(LP_ERR, "-L and -U are incompatible");
				fatal("%s", cvs_cmd_admin.cmd_synopsis);
			}
			lkmode = RCS_LOCK_STRICT;
			break;
		case 'l':
			break;
		case 'm':
			logstr = optarg;
			break;
		case 'N':
			break;
		case 'n':
			break;
		case 'o':
			orange = optarg;
			break;
		case 'q':
			verbosity = 0;
			break;
		case 's':
			statestr = optarg;
			break;
		case 't':
			break;
		case 'U':
			if (lkmode == RCS_LOCK_STRICT) {
				cvs_log(LP_ERR, "-U and -L are incompatible");
				fatal("%s", cvs_cmd_admin.cmd_synopsis);
			}
			lkmode = RCS_LOCK_LOOSE;
			break;
		case 'u':
			break;
		default:
			fatal("%s", cvs_cmd_admin.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		fatal("%s", cvs_cmd_admin.cmd_synopsis);

	cr.enterdir = NULL;
	cr.leavedir = NULL;

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_connect_to_server();
		cr.fileproc = cvs_client_sendfile;

		if (oldfilename != NULL)
			cvs_client_send_request("Argument -A%s", oldfilename);

		if (alist != NULL)
			cvs_client_send_request("Argument -a%s", alist);

		if (comment != NULL)
			cvs_client_send_request("Argument -c%s", comment);

		if (runflags & ADM_EFLAG)
			cvs_client_send_request("Argument -e%s",
			    (elist != NULL) ? elist : "");

		if (koptstr != NULL)
			cvs_client_send_request("Argument -k%s", koptstr);

		if (lkmode == RCS_LOCK_STRICT)
			cvs_client_send_request("Argument -L");
		else if (lkmode == RCS_LOCK_LOOSE)
			cvs_client_send_request("Argument -U");

		if (logstr != NULL)
			cvs_client_send_logmsg(logstr);

		if (orange != NULL)
			cvs_client_send_request("Argument -o%s", orange);

		if (statestr != NULL)
			cvs_client_send_request("Argument -s%s", statestr);

		if (verbosity == 0)
			cvs_client_send_request("Argument -q");

	} else {
		if (statestr != NULL) {
			if ((staterevstr = strchr(statestr, ':')) != NULL)
				*staterevstr++ = '\0';
			state = statestr;
			if (rcs_state_check(state) < 0) {
				cvs_log(LP_ERR, "invalid state `%s'", state);
				state = NULL;
			}
		}

		flags |= CR_REPO;
		cr.fileproc = cvs_admin_local;
	}

	cr.flags = flags;

	cvs_file_run(argc, argv, &cr);

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");
		cvs_client_send_request("admin");
		cvs_client_get_responses();
	}

	return (0);
}

void
cvs_admin_local(struct cvs_file *cf)
{
	int i;
	RCSNUM *rev;

	cvs_log(LP_TRACE, "cvs_admin_local(%s)", cf->file_path);

	cvs_file_classify(cf, cvs_directory_tag);

	if (cf->file_type == CVS_DIR) {
		if (verbosity > 1)
			cvs_log(LP_NOTICE, "Administrating %s", cf->file_name);
		return;
	}

	if (cf->file_ent == NULL)
		return;
	else if (cf->file_status == FILE_ADDED) {
		cvs_log(LP_ERR, "cannot admin newly added file `%s'",
		    cf->file_name);
		return;
	}

	if (cf->file_rcs == NULL) {
		cvs_log(LP_ERR, "lost RCS file for `%s'", cf->file_path);
		return;
	}

	if (verbosity > 0)
		cvs_printf("RCS file: %s\n", cf->file_rcs->rf_path);

	if (oldfilename != NULL) {
		struct cvs_file *ocf;
		struct rcs_access *acp;
		int ofd;
		char *d, *f, fpath[PATH_MAX], repo[PATH_MAX];


		if ((f = basename(oldfilename)) == NULL)
			fatal("cvs_admin_local: basename failed");
		if ((d = dirname(oldfilename)) == NULL)
			fatal("cvs_admin_local: dirname failed");

		cvs_get_repository_path(d, repo, PATH_MAX);

		(void)xsnprintf(fpath, PATH_MAX, "%s/%s", repo, f);

		if (strlcat(fpath, RCS_FILE_EXT, PATH_MAX) >= PATH_MAX)
			fatal("cvs_admin_local: truncation");

		if ((ofd = open(fpath, O_RDONLY)) == -1)
			fatal("cvs_admin_local: open: `%s': %s", fpath,
			    strerror(errno));

		/* XXX: S_ISREG() check instead of blindly using CVS_FILE? */
		ocf = cvs_file_get_cf(d, f, oldfilename, ofd, CVS_FILE, 0);

		ocf->file_rcs = rcs_open(fpath, ofd, RCS_READ, 0444);
		if (ocf->file_rcs == NULL)
			fatal("cvs_admin_local: rcs_open failed");

		TAILQ_FOREACH(acp, &(ocf->file_rcs->rf_access), ra_list)
			rcs_access_add(cf->file_rcs, acp->ra_name);

		cvs_file_free(ocf);
	}

	if (alist != NULL) {
		struct cvs_argvector *aargv;

		aargv = cvs_strsplit(alist, ",");
		for (i = 0; aargv->argv[i] != NULL; i++)
			rcs_access_add(cf->file_rcs, aargv->argv[i]);

		cvs_argv_destroy(aargv);
	}

	if (comment != NULL)
		rcs_comment_set(cf->file_rcs, comment);

	if (elist != NULL) {
		struct cvs_argvector *eargv;

		eargv = cvs_strsplit(elist, ",");
		for (i = 0; eargv->argv[i] != NULL; i++)
			rcs_access_remove(cf->file_rcs, eargv->argv[i]);

		cvs_argv_destroy(eargv);
	} else if (runflags & ADM_EFLAG) {
		struct rcs_access *rap;

		while (!TAILQ_EMPTY(&(cf->file_rcs->rf_access))) {
			rap = TAILQ_FIRST(&(cf->file_rcs->rf_access));
			TAILQ_REMOVE(&(cf->file_rcs->rf_access), rap, ra_list);
			xfree(rap->ra_name);
			xfree(rap);
		}
		/* no synced anymore */
		cf->file_rcs->rf_flags &= ~RCS_SYNCED;
	}

	/* Default `-kv' is accepted here. */
	if (kflag) {
		if (cf->file_rcs->rf_expand == NULL ||
		    strcmp(cf->file_rcs->rf_expand, koptstr) != 0)
			rcs_kwexp_set(cf->file_rcs, kflag);
	}

	if (logstr != NULL) {
		if ((logmsg = strchr(logstr, ':')) == NULL) {
			cvs_log(LP_ERR, "missing log message");
			return;
		}

		*logmsg++ = '\0';
		if ((rev = rcsnum_parse(logstr)) == NULL) {
			cvs_log(LP_ERR, "`%s' bad revision number", logstr);
			return;
		}

		if (rcs_rev_setlog(cf->file_rcs, rev, logmsg) < 0) {
			cvs_log(LP_ERR, "failed to set logmsg for `%s' to `%s'",
			    logstr, logmsg);
			rcsnum_free(rev);
			return;
		}

		rcsnum_free(rev);
	}

	if (orange != NULL) {
		struct rcs_delta *rdp, *nrdp;
		char b[CVS_REV_BUFSZ];

		cvs_revision_select(cf->file_rcs, orange);
		for (rdp = TAILQ_FIRST(&(cf->file_rcs->rf_delta));
		    rdp != NULL; rdp = nrdp) {
			nrdp = TAILQ_NEXT(rdp, rd_list);

			/*
			 * Delete selected revisions.
			 */
			if (rdp->rd_flags & RCS_RD_SELECT) {
				rcsnum_tostr(rdp->rd_num, b, sizeof(b));
				if (verbosity > 0)
					cvs_printf("deleting revision %s\n", b);

				(void)rcs_rev_remove(cf->file_rcs, rdp->rd_num);
			}
		}
	}

	if (state != NULL) {
		if (staterevstr != NULL) {
			if ((rev = rcsnum_parse(staterevstr)) == NULL) {
				cvs_log(LP_ERR, "`%s' bad revision number",
				    staterevstr);
				return;
			}
		} else if (cf->file_rcs->rf_head != NULL) {
			rev = rcsnum_alloc();
			rcsnum_cpy(cf->file_rcs->rf_head, rev, 0);
		} else {
			cvs_log(LP_ERR, "head revision missing");
			return;
		}

		(void)rcs_state_set(cf->file_rcs, rev, state);

		rcsnum_free(rev);
	}

	if (lkmode != RCS_LOCK_INVAL)
		(void)rcs_lock_setmode(cf->file_rcs, lkmode);

	rcs_write(cf->file_rcs);

	if (verbosity > 0)
		cvs_printf("done\n");
}
