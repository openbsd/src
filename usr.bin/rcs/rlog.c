/*	$OpenBSD: rlog.c,v 1.65 2011/07/14 16:38:39 sobrado Exp $	*/
/*
 * Copyright (c) 2005, 2009 Joris Vink <joris@openbsd.org>
 * Copyright (c) 2005, 2006 Xavier Santolaria <xsa@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <ctype.h>
#include <err.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rcsprog.h"
#include "diff.h"

#define RLOG_DATE_LATER		0x01
#define RLOG_DATE_EARLIER	0x02
#define RLOG_DATE_SINGLE	0x04
#define RLOG_DATE_RANGE		0x08
#define RLOG_DATE_INCLUSIVE	0x10

static int	rlog_select_daterev(RCSFILE *, char *);
static void	rlog_file(const char *, RCSFILE *);
static void	rlog_rev_print(struct rcs_delta *);

#define RLOG_OPTSTRING	"d:hLl::NqRr::s:TtVw::x::z::"
#define REVSEP		"----------------------------"
#define REVEND \
 "============================================================================="

static int dflag, hflag, Lflag, lflag, rflag, tflag, Nflag, wflag;
static char *llist = NULL;
static char *slist = NULL;
static char *wlist = NULL;
static char *revisions = NULL;
static char *rlog_dates = NULL;

void
rlog_usage(void)
{
	fprintf(stderr,
	    "usage: rlog [-bhLNRtV] [-ddates] [-l[lockers]] [-r[revs]]\n"
	    "            [-sstates] [-w[logins]] [-xsuffixes]\n"
	    "            [-ztz] file ...\n");
}

int
rlog_main(int argc, char **argv)
{
	RCSFILE *file;
	int Rflag;
	int i, ch, fd, status;
	char fpath[MAXPATHLEN];

	rcsnum_flags |= RCSNUM_NO_MAGIC;
	hflag = Rflag = rflag = status = 0;
	while ((ch = rcs_getopt(argc, argv, RLOG_OPTSTRING)) != -1) {
		switch (ch) {
		case 'd':
			dflag = 1;
			rlog_dates = rcs_optarg;
			break;
		case 'h':
			hflag = 1;
			break;
		case 'L':
			Lflag = 1;
			break;
		case 'l':
			lflag = 1;
			llist = rcs_optarg;
			break;
		case 'N':
			Nflag = 1;
			break;
		case 'q':
			/*
			 * kept for compatibility
			 */
			break;
		case 'R':
			Rflag = 1;
			break;
		case 'r':
			rflag = 1;
			revisions = rcs_optarg;
			break;
		case 's':
			slist = rcs_optarg;
			break;
		case 'T':
			/*
			 * kept for compatibility
			 */
			break;
		case 't':
			tflag = 1;
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
		case 'w':
			wflag = 1;
			wlist = rcs_optarg;
			break;
		case 'x':
			/* Use blank extension if none given. */
			rcs_suffixes = rcs_optarg ? rcs_optarg : "";
			break;
		case 'z':
			timezone_flag = rcs_optarg;
			break;
		default:
			(usage());
			exit(1);
		}
	}

	argc -= rcs_optind;
	argv += rcs_optind;

	if (argc == 0) {
		warnx("no input file");
		(usage)();
		exit(1);
	}

	if (hflag == 1 && tflag == 1) {
		warnx("warning: -t overrides -h.");
		hflag = 0;
	}

	for (i = 0; i < argc; i++) {
		fd = rcs_choosefile(argv[i], fpath, sizeof(fpath));
		if (fd < 0) {
			warn("%s", fpath);
			status = 1;
			continue;
		}

		if ((file = rcs_open(fpath, fd,
		    RCS_READ|RCS_PARSE_FULLY)) == NULL) {
			status = 1;
			continue;
		}

		if (Lflag == 1 && TAILQ_EMPTY(&(file->rf_locks))) {
			rcs_close(file);
			continue;
		}

		if (Rflag == 1) {
			printf("%s\n", fpath);
			rcs_close(file);
			continue;
		}

		rlog_file(argv[i], file);

		rcs_close(file);
	}

	return (status);
}

static int
rlog_select_daterev(RCSFILE *rcsfile, char *date)
{
	int i, nrev, flags;
	struct rcs_delta *rdp;
	struct rcs_argvector *args;
	char *first, *last, delim;
	time_t firstdate, lastdate, rcsdate;

	nrev = 0;
	args = rcs_strsplit(date, ";");

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
				flags |= RLOG_DATE_INCLUSIVE;
			}
		} else {
			last = strchr(args->argv[i], '>');
			if (last != NULL) {
				delim = *last;
				*last++ = '\0';

				if (*last == '=') {
					last++;
					flags |= RLOG_DATE_INCLUSIVE;
				}
			}
		}

		if (last == NULL) {
			flags |= RLOG_DATE_SINGLE;
			if ((firstdate = date_parse(first)) == -1)
				return -1;
			delim = '\0';
			last = "\0";
		} else {
			while (*last && isspace(*last))
				last++;
		}

		if (delim == '>' && *last == '\0') {
			flags |= RLOG_DATE_EARLIER;
			if ((firstdate = date_parse(first)) == -1)
				return -1;
		}

		if (delim == '>' && *first == '\0' && *last != '\0') {
			flags |= RLOG_DATE_LATER;
			if ((firstdate = date_parse(last)) == -1)
				return -1;
		}

		if (delim == '<' && *last == '\0') {
			flags |= RLOG_DATE_LATER;
			if ((firstdate = date_parse(first)) == -1)
				return -1;
		}

		if (delim == '<' && *first == '\0' && *last != '\0') {
			flags |= RLOG_DATE_EARLIER;
			if ((firstdate = date_parse(last)) == -1)
				return -1;
		}

		if (*first != '\0' && *last != '\0') {
			flags |= RLOG_DATE_RANGE;

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

		TAILQ_FOREACH(rdp, &(rcsfile->rf_delta), rd_list) {
			rcsdate = mktime(&(rdp->rd_date));

			if (flags & RLOG_DATE_SINGLE) {
				if (rcsdate <= firstdate) {
					rdp->rd_flags |= RCS_RD_SELECT;
					nrev++;
					break;
				}
			}

			if (flags & RLOG_DATE_EARLIER) {
				if (rcsdate < firstdate) {
					rdp->rd_flags |= RCS_RD_SELECT;
					nrev++;
					continue;
				}

				if (flags & RLOG_DATE_INCLUSIVE &&
				    (rcsdate <= firstdate)) {
					rdp->rd_flags |= RCS_RD_SELECT;
					nrev++;
					continue;
				}
			}

			if (flags & RLOG_DATE_LATER) {
				if (rcsdate > firstdate) {
					rdp->rd_flags |= RCS_RD_SELECT;
					nrev++;
					continue;
				}

				if (flags & RLOG_DATE_INCLUSIVE &&
				    (rcsdate >= firstdate)) {
					rdp->rd_flags |= RCS_RD_SELECT;
					nrev++;
					continue;
				}
			}

			if (flags & RLOG_DATE_RANGE) {
				if ((rcsdate > firstdate) &&
				    (rcsdate < lastdate)) {
					rdp->rd_flags |= RCS_RD_SELECT;
					nrev++;
					continue;
				}

				if (flags & RLOG_DATE_INCLUSIVE &&
				    ((rcsdate >= firstdate) &&
				    (rcsdate <= lastdate))) {
					rdp->rd_flags |= RCS_RD_SELECT;
					nrev++;
					continue;
				}
			}
		}
	}

	return (nrev);
}

static void
rlog_file(const char *fname, RCSFILE *file)
{
	char numb[RCS_REV_BUFSZ];
	u_int nrev;
	struct rcs_sym *sym;
	struct rcs_access *acp;
	struct rcs_delta *rdp;
	struct rcs_lock *lkp;
	char *workfile, *p;

	if (rflag == 1)
		nrev = rcs_rev_select(file, revisions);
	else if (dflag == 1) {
		if ((nrev = rlog_select_daterev(file, rlog_dates)) == -1)
			errx(1, "invalid date: %s", rlog_dates);
	} else
		nrev = file->rf_ndelta;

	if ((workfile = basename(fname)) == NULL)
		err(1, "basename");

	/*
	 * In case they specified 'foo,v' as argument.
	 */
	if ((p = strrchr(workfile, ',')) != NULL)
		*p = '\0';

	printf("\nRCS file: %s", file->rf_path);
	printf("\nWorking file: %s", workfile);
	printf("\nhead:");
	if (file->rf_head != NULL)
		printf(" %s", rcsnum_tostr(file->rf_head, numb, sizeof(numb)));

	printf("\nbranch:");
	if (rcs_branch_get(file) != NULL) {
		printf(" %s", rcsnum_tostr(rcs_branch_get(file),
		    numb, sizeof(numb)));
	}

	printf("\nlocks: %s", (file->rf_flags & RCS_SLOCK) ? "strict" : "");
	TAILQ_FOREACH(lkp, &(file->rf_locks), rl_list)
		printf("\n\t%s: %s", lkp->rl_name,
		    rcsnum_tostr(lkp->rl_num, numb, sizeof(numb)));
	printf("\naccess list:\n");
	TAILQ_FOREACH(acp, &(file->rf_access), ra_list)
		printf("\t%s\n", acp->ra_name);

	if (Nflag == 0) {
		printf("symbolic names:\n");
		TAILQ_FOREACH(sym, &(file->rf_symbols), rs_list) {
			printf("\t%s: %s\n", sym->rs_name,
			    rcsnum_tostr(sym->rs_num, numb, sizeof(numb)));
		}
	}

	printf("keyword substitution: %s\n",
	    file->rf_expand == NULL ? "kv" : file->rf_expand);

	printf("total revisions: %u", file->rf_ndelta);

	if (file->rf_head != NULL && hflag == 0 && tflag == 0)
		printf(";\tselected revisions: %u", nrev);

	printf("\n");


	if (hflag == 0 || tflag == 1)
		printf("description:\n%s", file->rf_desc);

	if (hflag == 0 && tflag == 0 &&
	    !(lflag == 1 && TAILQ_EMPTY(&file->rf_locks))) {
		TAILQ_FOREACH(rdp, &(file->rf_delta), rd_list) {
			/*
			 * if selections are enabled verify that entry is
			 * selected.
			 */
			if ((rflag == 0 && dflag == 0)
			    || (rdp->rd_flags & RCS_RD_SELECT))
				rlog_rev_print(rdp);
		}
	}

	printf("%s\n", REVEND);
}

static void
rlog_rev_print(struct rcs_delta *rdp)
{
	int i, found;
	struct tm t;
	char *author, numb[RCS_REV_BUFSZ], *fmt, timeb[RCS_TIME_BUFSZ];
	struct rcs_argvector *largv, *sargv, *wargv;
	struct rcs_branch *rb;
	struct rcs_delta *nrdp;

	i = found = 0;
	author = NULL;

	/* -l[lockers] */
	if (lflag == 1) {
		if (rdp->rd_locker != NULL)
			found++;

		if (llist != NULL) {
			/* if locker is empty, no need to go further. */
			if (rdp->rd_locker == NULL)
				return;
			largv = rcs_strsplit(llist, ",");
			for (i = 0; largv->argv[i] != NULL; i++) {
				if (strcmp(rdp->rd_locker, largv->argv[i])
				    == 0) {
					found++;
					break;
				}
				found = 0;
			}
			rcs_argv_destroy(largv);
		}
	}

	/* -sstates */
	if (slist != NULL) {
		sargv = rcs_strsplit(slist, ",");
		for (i = 0; sargv->argv[i] != NULL; i++) {
			if (strcmp(rdp->rd_state, sargv->argv[i]) == 0) {
				found++;
				break;
			}
			found = 0;
		}
		rcs_argv_destroy(sargv);
	}

	/* -w[logins] */
	if (wflag == 1) {
		if (wlist != NULL) {
			wargv = rcs_strsplit(wlist, ",");
			for (i = 0; wargv->argv[i] != NULL; i++) {
				if (strcmp(rdp->rd_author, wargv->argv[i])
				    == 0) {
					found++;
					break;
				}
				found = 0;
			}
			rcs_argv_destroy(wargv);
		} else {
			if ((author = getlogin()) == NULL)
				err(1, "getlogin");

			if (strcmp(rdp->rd_author, author) == 0)
				found++;
		}
	}

	/* XXX dirty... */
	if ((((slist != NULL && wflag == 1) ||
	    (slist != NULL && lflag == 1) ||
	    (lflag == 1 && wflag == 1)) && found < 2) ||
	    (((slist != NULL && lflag == 1 && wflag == 1) ||
	    (slist != NULL || lflag == 1 || wflag == 1)) && found == 0))
		return;

	printf("%s\n", REVSEP);

	rcsnum_tostr(rdp->rd_num, numb, sizeof(numb));

	printf("revision %s", numb);
	if (rdp->rd_locker != NULL)
		printf("\tlocked by: %s;", rdp->rd_locker);

	if (timezone_flag != NULL) {
		rcs_set_tz(timezone_flag, rdp, &t);
		fmt = "%Y-%m-%d %H:%M:%S%z";
	} else {
		t = rdp->rd_date;
		fmt = "%Y/%m/%d %H:%M:%S";
	}

	(void)strftime(timeb, sizeof(timeb), fmt, &t);

	printf("\ndate: %s;  author: %s;  state: %s;", timeb, rdp->rd_author,
	    rdp->rd_state);

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
			printf("  lines: +%d -%d", added, removed);
		else
			printf("  lines: +%d -%d", removed, added);
	}
	printf("\n");

	if (!TAILQ_EMPTY(&(rdp->rd_branches))) {
		printf("branches:");
		TAILQ_FOREACH(rb, &(rdp->rd_branches), rb_list) {
			RCSNUM *branch;
			branch = rcsnum_revtobr(rb->rb_num);
			(void)rcsnum_tostr(branch, numb, sizeof(numb));
			printf("  %s;", numb);
			rcsnum_free(branch);
		}
		printf("\n");
	}

	printf("%s", rdp->rd_log);
}
