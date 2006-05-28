/*	$OpenBSD: getlog.c,v 1.57 2006/05/28 21:11:12 joris Exp $	*/
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

#define LOG_REVSEP \
"----------------------------"

#define LOG_REVEND \
 "============================================================================="

int	cvs_getlog(int, char **);
void	cvs_log_local(struct cvs_file *);

char 	*logrev = NULL;

struct cvs_cmd cvs_cmd_log = {
	CVS_OP_LOG, CVS_REQ_LOG, "log",
	{ "lo" },
	"Print out history information for files",
	"[-bhlNRt] [-d dates] [-r revisions] [-s states] [-w logins]",
	"bd:hlNRr:s:tw:",
	NULL,
	cvs_getlog
};

int
cvs_getlog(int argc, char **argv)
{
	int ch;
	int flags;
	char *arg = ".";
	struct cvs_recursion cr;

	rcsnum_flags |= RCSNUM_NO_MAGIC;
	flags = CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmd_log.cmd_opts)) != -1) {
		switch (ch) {
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'r':
			logrev = optarg;
			break;
		default:
			fatal("%s", cvs_cmd_log.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	cr.enterdir = NULL;
	cr.leavedir = NULL;
	cr.remote = NULL;
	cr.local = cvs_log_local;
	cr.flags = flags;

	if (argc > 0)
		cvs_file_run(argc, argv, &cr);
	else
		cvs_file_run(1, &arg, &cr);

	return (0);
}

void
cvs_log_local(struct cvs_file *cf)
{
	u_int nrev;
	struct rcs_sym *sym;
	struct rcs_lock *lkp;
	struct rcs_delta *rdp;
	struct rcs_access *acp;
	char numb[32], timeb[32];

	cvs_file_classify(cf, 0);

	if (cf->file_status == FILE_UNKNOWN) {
		if (verbosity > 0)
			cvs_log(LP_ERR, "nothing known about %s",
			    cf->file_path);
		return;
	} else if (cf->file_status == FILE_ADDED) {
		if (verbosity > 0)
			cvs_log(LP_ERR, "%s has been added, but not commited",
			    cf->file_path);
		return;
	}

	printf("\nRCS file: %s", cf->file_rpath);
	printf("\nWorking file: %s", cf->file_path);
	printf("\nhead:");
	if (cf->file_rcs->rf_head != NULL)
		printf(" %s", rcsnum_tostr(cf->file_rcs->rf_head,
		    numb, sizeof(numb)));

	printf("\nbranch:");
	if (rcs_branch_get(cf->file_rcs) != NULL) {
		printf(" %s", rcsnum_tostr(rcs_branch_get(cf->file_rcs),
		    numb, sizeof(numb)));
	}

	printf("\nlocks: %s", (cf->file_rcs->rf_flags & RCS_SLOCK)
	    ? "strict" : "");
	TAILQ_FOREACH(lkp, &(cf->file_rcs->rf_locks), rl_list)
		printf("\n\t%s: %s", lkp->rl_name,
		    rcsnum_tostr(lkp->rl_num, numb, sizeof(numb)));

	printf("\naccess list:\n");
	TAILQ_FOREACH(acp, &(cf->file_rcs->rf_access), ra_list)
		printf("\t%s\n", acp->ra_name);

	printf("symbolic names:\n");
	TAILQ_FOREACH(sym, &(cf->file_rcs->rf_symbols), rs_list) {
		printf("\t%s: %s\n", sym->rs_name,
		    rcsnum_tostr(sym->rs_num, numb, sizeof(numb)));
	}

	printf("keyword substitution: %s\n",
	    cf->file_rcs->rf_expand == NULL ? "kv" : cf->file_rcs->rf_expand);

	printf("total revisions: %u", cf->file_rcs->rf_ndelta);

	if (logrev != NULL)
		nrev = 1;
	else
		nrev = cf->file_rcs->rf_ndelta;

	printf(";\tselected revisions: %u", nrev);
	printf("\n");
	printf("description:\n%s", cf->file_rcs->rf_desc);

	TAILQ_FOREACH(rdp, &(cf->file_rcs->rf_delta), rd_list) {
		rcsnum_tostr(rdp->rd_num, numb, sizeof(numb));

		if (logrev != NULL &&
		    strcmp(logrev, numb))
			continue;

		printf("%s\n", LOG_REVSEP);

		printf("revision %s", numb);

		strftime(timeb, sizeof(timeb), "%Y/%m/%d %H:%M:%S",
		    &rdp->rd_date);
		printf("\ndate: %s;  author: %s;  state: %s;\n", timeb,
		    rdp->rd_author, rdp->rd_state);
		printf("%s", rdp->rd_log);
	}

	printf("%s\n", LOG_REVEND);
}
