/*	$OpenBSD: rlog.c,v 1.23 2006/01/25 08:02:26 xsa Exp $	*/
/*
 * Copyright (c) 2005 Joris Vink <joris@openbsd.org>
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

#include "includes.h"

#include "rcsprog.h"
#include "diff.h"

static int	  rlog_file(const char *, const char *);
static void	  rlog_rev_print(struct rcs_delta *);
static char	**rlog_strsplit(char *, const char *);

#define REVSEP		"----------------------------"
#define REVEND \
 "============================================================================="

static int hflag, Lflag, lflag, tflag, Nflag, wflag;
static char *llist = NULL;
static char *slist = NULL;
static char *wlist = NULL;
static RCSFILE *file;

void
rlog_usage(void)
{
	fprintf(stderr,
	    "usage: rlog [-hLNqRTtV] [-l[lockers]] [-sstates] [-w[logins]]\n"
	    "            [-xsuffixes] file ...\n");
}

int
rlog_main(int argc, char **argv)
{
	int Rflag;
	int i, ch;
	char fpath[MAXPATHLEN];

	hflag = Rflag = 0;
	while ((ch = rcs_getopt(argc, argv, "hLl::NqRs:TtVw::x:")) != -1) {
		switch (ch) {
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
			verbose = 0;
			break;
		case 'R':
			Rflag = 1;
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
			rcs_suffixes = rcs_optarg;
			break;
		default:
			(usage());
			exit(1);
		}
	}

	argc -= rcs_optind;
	argv += rcs_optind;

	if (argc == 0) {
		cvs_log(LP_ERR, "no input file");
		(usage)();
		exit(1);
	}

	if ((hflag == 1) && (tflag == 1)) {
		cvs_log(LP_WARN, "warning: -t overrides -h.");
		hflag = 0;
	}

	for (i = 0; i < argc; i++) {
		if (rcs_statfile(argv[i], fpath, sizeof(fpath)) < 0)
			continue;

		if ((file = rcs_open(fpath, RCS_READ|RCS_PARSE_FULLY)) == NULL)
			continue;

		if ((Lflag == 1) && (TAILQ_EMPTY(&(file->rf_locks)))) {
			rcs_close(file);
			continue;
		}

		if (Rflag == 1) {
			printf("%s\n", fpath);
			rcs_close(file);
			continue;
		}

		rlog_file(argv[i], fpath);

		rcs_close(file);
	}

	return (0);
}

static int
rlog_file(const char *fname, const char *fpath)
{
	char numb[64];
	struct rcs_sym *sym;
	struct rcs_access *acp;
	struct rcs_delta *rdp;
	struct rcs_lock *lkp;

	printf("\nRCS file: %s", fpath);
	printf("\nWorking file: %s", fname);
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

	if ((hflag == 0) && (tflag == 0))
		printf(";\tselected revisions:"); /* XXX */

	printf("\n");


	if ((hflag == 0) || (tflag == 1))
		printf("description:\n%s", file->rf_desc);

	if ((hflag == 0) && (tflag == 0)) {
		TAILQ_FOREACH(rdp, &(file->rf_delta), rd_list)
			rlog_rev_print(rdp);
	}

	printf("%s\n", REVEND);
	return (0);
}

static void
rlog_rev_print(struct rcs_delta *rdp)
{
	int i, found;
	char *author, numb[64];
	char **largv, **sargv, **wargv;

	i = found = 0;
	author = NULL;

	/* -l[lockers] */
	if (lflag == 1) {
		/* if no locks at all, abort. */
		if (TAILQ_EMPTY(&(file->rf_locks)))
			return;
		else
			if (rdp->rd_locker != NULL)
				found++;

		if (llist != NULL) {
			/* if locker is empty, no need to go further. */
			if (rdp->rd_locker == NULL)
				return;
			largv = rlog_strsplit(llist, ",");
			for (i = 0; largv[i] != NULL; i++) {
				if (strcmp(rdp->rd_locker, largv[i]) == 0) {
					found++;
					break;
				}
				found = 0;
			}
		}
	}
	/* -sstates */
	if (slist != NULL) {
		sargv = rlog_strsplit(slist, ",");
		for (i = 0; sargv[i] != NULL; i++) {
			if (strcmp(rdp->rd_state, sargv[i]) == 0) {
				found++;
				break;
			}
			found = 0;
		}
	}
	/* -w[logins] */
	if (wflag == 1) {
		if (wlist != NULL) {
			wargv = rlog_strsplit(wlist, ",");
			for (i = 0; wargv[i] != NULL; i++) {
				if (strcmp(rdp->rd_author, wargv[i]) == 0) {
					found++;
					break;
				}
				found = 0;
			}
		} else {
			if ((author = getlogin()) == NULL)
				fatal("getlogin failed");

			if (strcmp(rdp->rd_author, author) == 0)
				found++;
		}
	}

	/* XXX dirty... */
	if (((((slist != NULL) && (wflag == 1)) ||
	    ((slist != NULL) && (lflag == 1)) ||
	    ((lflag == 1) && (wflag == 1))) && (found < 2)) ||
	    ((((slist != NULL) && (lflag == 1) && (wflag == 1)) ||
	    ((slist != NULL) || (lflag == 1) || (wflag == 1))) && (found == 0)))
		return;

	printf("%s\n", REVSEP);

	rcsnum_tostr(rdp->rd_num, numb, sizeof(numb));

	printf("revision %s", numb);
	if (rdp->rd_locker != NULL)
		printf("\tlocked by: %s;", rdp->rd_locker);
	printf("\ndate: %d/%02d/%02d %02d:%02d:%02d;"
	    "  author: %s;  state: %s;\n",
	    rdp->rd_date.tm_year + 1900,
	    rdp->rd_date.tm_mon + 1,
	    rdp->rd_date.tm_mday, rdp->rd_date.tm_hour,
	    rdp->rd_date.tm_min, rdp->rd_date.tm_sec,
	    rdp->rd_author, rdp->rd_state);
	printf("%s", rdp->rd_log);
}

/*
 * rlog_strsplit()
 *
 * Split a string <str> of <sep>-separated values and allocate
 * an argument vector for the values found.
 */
static char **
rlog_strsplit(char *str, const char *sep)
{
	char **argv, **nargv;
	char *cp, *p;
	int i = 0;

	cp = xstrdup(str);
	argv = (char **)xmalloc((i+1) * sizeof(char *));

	while ((p = strsep(&cp, sep)) != NULL) {
		argv[i++] = p;
		nargv = (char **)xrealloc((void *)argv, (i+1) * sizeof(char *));
		argv = nargv;
	}
	argv[i] = NULL;

	return (argv);
}
