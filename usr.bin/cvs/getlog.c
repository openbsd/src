/*	$OpenBSD: getlog.c,v 1.98 2014/12/01 21:58:46 deraadt Exp $	*/
/*
 * Copyright (c) 2005, 2006 Xavier Santolaria <xsa@openbsd.org>
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

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "cvs.h"
#include "remote.h"

#define L_HEAD		0x01
#define L_HEAD_DESCR	0x02
#define L_NAME		0x04
#define L_NOTAGS	0x08
#define L_LOGINS	0x10
#define L_STATES	0x20

#define LDATE_LATER	0x01
#define LDATE_EARLIER	0x02
#define LDATE_SINGLE	0x04
#define LDATE_RANGE	0x08
#define LDATE_INCLUSIVE	0x10

void		 cvs_log_local(struct cvs_file *);
static void	 log_rev_print(struct rcs_delta *);
static char 	*push_date(char *dest, const char *);
static u_int	 date_select(RCSFILE *, char *);

int	 runflags = 0;
char	*logrev = NULL;
char	*logdate = NULL;
char	*slist = NULL;
char	*wlist = NULL;

struct cvs_cmd cvs_cmd_log = {
	CVS_OP_LOG, CVS_USE_WDIR, "log",
	{ "lo" },
	"Print out history information for files",
	"[-bhlNRt] [-d dates] [-r revisions] [-s states] [-w logins]",
	"bd:hlNRr:s:tw:",
	NULL,
	cvs_getlog
};

struct cvs_cmd cvs_cmd_rlog = {
	CVS_OP_RLOG, 0, "rlog",
	{ "rlo" },
	"Print out history information for files",
	"[-bhlNRt] [-d dates] [-r revisions] [-s states] [-w logins]",
	"bd:hlNRr:s:tw:",
	NULL,
	cvs_getlog
};

int
cvs_getlog(int argc, char **argv)
{
	int ch, flags, i;
	char *arg = ".";
	struct cvs_recursion cr;

	flags = CR_RECURSE_DIRS;

	while ((ch = getopt(argc, argv, cvs_cmdop == CVS_OP_LOG ?
	    cvs_cmd_log.cmd_opts : cvs_cmd_rlog.cmd_opts)) != -1) {
		switch (ch) {
		case 'd':
			logdate = push_date(logdate, optarg);
			break;
		case 'h':
			runflags |= L_HEAD;
			break;
		case 'l':
			flags &= ~CR_RECURSE_DIRS;
			break;
		case 'N':
			runflags |= L_NOTAGS;
			break;
		case 'R':
			runflags |= L_NAME;
			break;
		case 'r':
			logrev = optarg;
			break;
		case 's':
			runflags |= L_STATES;
			slist = optarg;
			break;
		case 't':
			runflags |= L_HEAD_DESCR;
			break;
		case 'w':
			runflags |= L_LOGINS;
			wlist = optarg;
			break;
		default:
			fatal("%s", cvs_cmdop == CVS_OP_LOG ?
			    cvs_cmd_log.cmd_synopsis :
			    cvs_cmd_rlog.cmd_synopsis);
		}
	}

	argc -= optind;
	argv += optind;

	if (cvs_cmdop == CVS_OP_RLOG) {
		flags |= CR_REPO;

		if (argc == 0)
			return 0;

		for (i = 0; i < argc; i++)
			if (argv[i][0] == '/')
				fatal("Absolute path name is invalid: %s",
				    argv[i]);
	}

	cr.enterdir = NULL;
	cr.leavedir = NULL;

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_connect_to_server();
		cr.fileproc = cvs_client_sendfile;

		if (logdate != NULL)
			cvs_client_send_request("Argument -d%s", logdate);

		if (runflags & L_HEAD)
			cvs_client_send_request("Argument -h");

		if (!(flags & CR_RECURSE_DIRS))
			cvs_client_send_request("Argument -l");

		if (runflags & L_NOTAGS)
			cvs_client_send_request("Argument -N");

		if (runflags & L_NAME)
			cvs_client_send_request("Argument -R");

		if (logrev != NULL)
			cvs_client_send_request("Argument -r%s", logrev);

		if (runflags & L_STATES)
			cvs_client_send_request("Argument -s%s", slist);

		if (runflags & L_HEAD_DESCR)
			cvs_client_send_request("Argument -t");

		if (runflags & L_LOGINS)
			cvs_client_send_request("Argument -w%s", wlist);
	} else {
		if (cvs_cmdop == CVS_OP_RLOG &&
		    chdir(current_cvsroot->cr_dir) == -1)
			fatal("cvs_getlog: %s", strerror(errno));

		cr.fileproc = cvs_log_local;
	}

	cr.flags = flags;

	if (cvs_cmdop == CVS_OP_LOG ||
	    current_cvsroot->cr_method == CVS_METHOD_LOCAL) {
		if (argc > 0)
			cvs_file_run(argc, argv, &cr);
		else
			cvs_file_run(1, &arg, &cr);
	}

	if (current_cvsroot->cr_method != CVS_METHOD_LOCAL) {
		cvs_client_send_files(argv, argc);
		cvs_client_senddir(".");

		cvs_client_send_request((cvs_cmdop == CVS_OP_RLOG) ?
		    "rlog" : "log");

		cvs_client_get_responses();
	}

	return (0);
}

void
cvs_log_local(struct cvs_file *cf)
{
	u_int nrev;
	RCSNUM *rev;
	struct rcs_sym *sym;
	struct rcs_lock *lkp;
	struct rcs_delta *rdp;
	struct rcs_access *acp;
	char numb[CVS_REV_BUFSZ];

	cvs_log(LP_TRACE, "cvs_log_local(%s)", cf->file_path);

	cvs_file_classify(cf, cvs_directory_tag);

	if (cf->file_type == CVS_DIR) {
		if (verbosity > 1)
			cvs_log(LP_ERR, "Logging %s", cf->file_path);
		return;
	}

	if (cf->file_rcs == NULL) {
		return;
	} else if (cf->file_status == FILE_ADDED) {
		if (verbosity > 0)
			cvs_log(LP_ERR, "%s has been added, but not committed",
			    cf->file_path);
		return;
	}

	if (runflags & L_NAME) {
		cvs_printf("%s\n", cf->file_rpath);
		return;
	}

	if (logrev != NULL)
		nrev = cvs_revision_select(cf->file_rcs, logrev);
	else if (logdate != NULL) {
		if ((nrev = date_select(cf->file_rcs, logdate)) == -1) {
			cvs_log(LP_ERR, "invalid date: %s", logdate);
			return;
		}
	} else
		nrev = cf->file_rcs->rf_ndelta;

	cvs_printf("\nRCS file: %s", cf->file_rpath);

	if (cvs_cmdop != CVS_OP_RLOG)
		cvs_printf("\nWorking file: %s", cf->file_path);

	cvs_printf("\nhead:");
	if (cf->file_rcs->rf_head != NULL)
		cvs_printf(" %s", rcsnum_tostr(cf->file_rcs->rf_head,
		    numb, sizeof(numb)));

	cvs_printf("\nbranch:");
	if (rcs_branch_get(cf->file_rcs) != NULL) {
		cvs_printf(" %s", rcsnum_tostr(rcs_branch_get(cf->file_rcs),
		    numb, sizeof(numb)));
	}

	cvs_printf("\nlocks: %s", (cf->file_rcs->rf_flags & RCS_SLOCK)
	    ? "strict" : "");
	TAILQ_FOREACH(lkp, &(cf->file_rcs->rf_locks), rl_list)
		cvs_printf("\n\t%s: %s", lkp->rl_name,
		    rcsnum_tostr(lkp->rl_num, numb, sizeof(numb)));

	cvs_printf("\naccess list:\n");
	TAILQ_FOREACH(acp, &(cf->file_rcs->rf_access), ra_list)
		cvs_printf("\t%s\n", acp->ra_name);

	if (!(runflags & L_NOTAGS)) {
		cvs_printf("symbolic names:\n");
		TAILQ_FOREACH(sym, &(cf->file_rcs->rf_symbols), rs_list) {
			rev = rcsnum_alloc();
			rcsnum_cpy(sym->rs_num, rev, 0);
			if (RCSNUM_ISBRANCH(sym->rs_num))
				rcsnum_addmagic(rev);

			cvs_printf("\t%s: %s\n", sym->rs_name,
			    rcsnum_tostr(rev, numb, sizeof(numb)));
			rcsnum_free(rev);
		}
	}

	cvs_printf("keyword substitution: %s\n",
	    cf->file_rcs->rf_expand == NULL ? "kv" : cf->file_rcs->rf_expand);

	cvs_printf("total revisions: %u", cf->file_rcs->rf_ndelta);

	if (cf->file_rcs->rf_head != NULL &&
	    !(runflags & L_HEAD) && !(runflags & L_HEAD_DESCR))
		cvs_printf(";\tselected revisions: %u", nrev);

	cvs_printf("\n");

	if (!(runflags & L_HEAD) || (runflags & L_HEAD_DESCR))
		cvs_printf("description:\n%s", cf->file_rcs->rf_desc);

	if (!(runflags & L_HEAD) && !(runflags & L_HEAD_DESCR)) {
		TAILQ_FOREACH(rdp, &(cf->file_rcs->rf_delta), rd_list) {
			/*
			 * if selections are enabled verify that entry is
			 * selected.
			 */
			if ((logrev == NULL && logdate == NULL) ||
			    (rdp->rd_flags & RCS_RD_SELECT))
				log_rev_print(rdp);
		}
	}

	cvs_printf("%s\n", LOG_REVEND);
}

static void
log_rev_print(struct rcs_delta *rdp)
{
	int i, found;
	char numb[CVS_REV_BUFSZ], timeb[CVS_TIME_BUFSZ];
	struct cvs_argvector *sargv, *wargv;
	struct rcs_branch *rb;
	struct rcs_delta *nrdp;

	i = found = 0;

	/* -s states */
	if (runflags & L_STATES) {
		sargv = cvs_strsplit(slist, ",");
		for (i = 0; sargv->argv[i] != NULL; i++) {
			if (strcmp(rdp->rd_state, sargv->argv[i]) == 0) {
				found++;
				break;
			}
			found = 0;
		}
		cvs_argv_destroy(sargv);
	}

	/* -w[logins] */
	if (runflags & L_LOGINS) {
		wargv = cvs_strsplit(wlist, ",");
		for (i = 0; wargv->argv[i] != NULL; i++) {
			if (strcmp(rdp->rd_author, wargv->argv[i]) == 0) {
				found++;
				break;
			}
			found = 0;
		}
		cvs_argv_destroy(wargv);
	}

	if ((runflags & (L_STATES|L_LOGINS)) && found == 0)
		return;

	cvs_printf("%s\n", LOG_REVSEP);

	rcsnum_tostr(rdp->rd_num, numb, sizeof(numb));
	cvs_printf("revision %s", numb);

	strftime(timeb, sizeof(timeb), "%Y/%m/%d %H:%M:%S", &rdp->rd_date);
	cvs_printf("\ndate: %s;  author: %s;  state: %s;",
	    timeb, rdp->rd_author, rdp->rd_state);

	/*
	 * If we are a branch revision, the diff of this revision is stored
	 * in place.
	 * Otherwise, it is stored in the previous revision as a reversed diff.
	 */
	if (RCSNUM_ISBRANCHREV(rdp->rd_num))
		nrdp = rdp;
	else
		nrdp = TAILQ_NEXT(rdp, rd_list);

	/*
	 * We do not write diff stats for the first revision of the default
	 * branch, since it was not a diff but a full text.
	 */
	if (nrdp != NULL && rdp->rd_num->rn_len == nrdp->rd_num->rn_len) {
		int added, removed;
		rcs_delta_stats(nrdp, &added, &removed);
		if (RCSNUM_ISBRANCHREV(rdp->rd_num))
			cvs_printf("  lines: +%d -%d;", added, removed);
		else
			cvs_printf("  lines: +%d -%d;", removed, added);
	}

	if (rdp->rd_commitid != NULL)
		printf("  commitid: %s;", rdp->rd_commitid);

	cvs_printf("\n");

	if (!TAILQ_EMPTY(&(rdp->rd_branches))) {
		cvs_printf("branches:");
		TAILQ_FOREACH(rb, &(rdp->rd_branches), rb_list) {
			RCSNUM *branch;
			branch = rcsnum_revtobr(rb->rb_num);
			rcsnum_tostr(branch, numb, sizeof(numb));
			cvs_printf("  %s;", numb);
			rcsnum_free(branch);
		}
		cvs_printf("\n");
	}

	cvs_printf("%s", rdp->rd_log);
}

static char *
push_date(char *dest, const char *src)
{
	size_t len;

	if (dest == NULL)
		return (xstrdup(src));

	/* 2 = ; and '\0' */
	len = strlen(dest) + strlen(src) + 2;

	dest[strlen(dest)] = ';';
	dest = xreallocarray(dest, len, 1);
	strlcat(dest, src, len);
	return (dest);
}

static u_int
date_select(RCSFILE *file, char *date)
{
	int i, nrev, flags;
	struct rcs_delta *rdp;
	struct cvs_argvector *args;
	char *first, *last, delim;
	time_t firstdate, lastdate, rcsdate;

	nrev = 0;
	args = cvs_strsplit(date, ";");

	for (i = 0; args->argv[i] != NULL; i++) {
		flags = 0;
		firstdate = lastdate = -1;

		first = args->argv[i];
		last = strchr(args->argv[i], '<');
		if (last != NULL) {
			delim = *last;
			*last++ = '\0';

			if (*last == '=') {
				last++;
				flags |= LDATE_INCLUSIVE;
			}
		} else {
			last = strchr(args->argv[i], '>');
			if (last != NULL) {
				delim = *last;
				*last++ = '\0';

				if (*last == '=') {
					last++;
					flags |= LDATE_INCLUSIVE;
				}
			}
		}

		if (last == NULL) {
			flags |= LDATE_SINGLE;
			if ((firstdate = date_parse(first)) == -1)
				return -1;
			delim = '\0';
			last = "\0";
		} else {
			while (*last && isspace((unsigned char)*last))
				last++;
		}

		if (delim == '>' && *last == '\0') {
			flags |= LDATE_EARLIER;
			if ((firstdate = date_parse(first)) == -1)
				return -1;
		}

		if (delim == '>' && *first == '\0' && *last != '\0') {
			flags |= LDATE_LATER;
			if ((firstdate = date_parse(last)) == -1)
				return -1;
		}

		if (delim == '<' && *last == '\0') {
			flags |= LDATE_LATER;
			if ((firstdate = date_parse(first)) == -1)
				return -1;
		}

		if (delim == '<' && *first == '\0' && *last != '\0') {
			flags |= LDATE_EARLIER;
			if ((firstdate = date_parse(last)) == -1)
				return -1;
		}

		if (*first != '\0' && *last != '\0') {
			flags |= LDATE_RANGE;

			if (delim == '<') {
				firstdate = date_parse(first);
				lastdate = date_parse(last);
			} else {
				firstdate = date_parse(last);
				lastdate = date_parse(first);
			}
			if (firstdate == -1 || lastdate == -1)
				return -1;
		}

		TAILQ_FOREACH(rdp, &(file->rf_delta), rd_list) {
			rcsdate = mktime(&(rdp->rd_date));

			if (flags & LDATE_SINGLE) {
				if (rcsdate <= firstdate) {
					rdp->rd_flags |= RCS_RD_SELECT;
					nrev++;
					break;
				}
			}

			if (flags & LDATE_EARLIER) {
				if (rcsdate < firstdate) {
					rdp->rd_flags |= RCS_RD_SELECT;
					nrev++;
					continue;
				}

				if (flags & LDATE_INCLUSIVE &&
				    (rcsdate <= firstdate)) {
					rdp->rd_flags |= RCS_RD_SELECT;
					nrev++;
					continue;
				}
			}

			if (flags & LDATE_LATER) {
				if (rcsdate > firstdate) {
					rdp->rd_flags |= RCS_RD_SELECT;
					nrev++;
					continue;
				}

				if (flags & LDATE_INCLUSIVE &&
				    (rcsdate >= firstdate)) {
					rdp->rd_flags |= RCS_RD_SELECT;
					nrev++;
					continue;
				}
			}

			if (flags & LDATE_RANGE) {
				if ((rcsdate > firstdate) &&
				    (rcsdate < lastdate)) {
					rdp->rd_flags |= RCS_RD_SELECT;
					nrev++;
					continue;
				}

				if (flags & LDATE_INCLUSIVE &&
				    ((rcsdate >= firstdate) &&
				    (rcsdate <= lastdate))) {
					rdp->rd_flags |= RCS_RD_SELECT;
					nrev++;
					continue;
				}
			}
		}
	}

	cvs_argv_destroy(args);

	return (nrev);
}
