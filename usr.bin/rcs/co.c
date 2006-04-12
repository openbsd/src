/*	$OpenBSD: co.c,v 1.74 2006/04/12 08:27:31 deraadt Exp $	*/
/*
 * Copyright (c) 2005 Joris Vink <joris@openbsd.org>
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

#define CO_OPTSTRING	"d:f::I::k:l::M::p::q::r::s:Tu::Vw::x::z::"

static void	checkout_err_nobranch(RCSFILE *, const char *, const char *,
    const char *, int);

int
checkout_main(int argc, char **argv)
{
	int i, ch, flags, kflag, status;
	RCSNUM *frev, *rev;
	RCSFILE *file;
	char fpath[MAXPATHLEN];
	char *author, *date, *rev_str, *username;
	const char *state;
	time_t rcs_mtime = -1;

	flags = status = 0;
	kflag = RCS_KWEXP_ERR;
	rev = RCS_HEAD_REV;
	frev = NULL;
	rev_str = NULL;
	state = NULL;
	author = NULL;
	date = NULL;

	while ((ch = rcs_getopt(argc, argv, CO_OPTSTRING)) != -1) {
		switch (ch) {
		case 'd':
			date = xstrdup(rcs_optarg);
			break;
		case 'f':
			rcs_setrevstr(&rev_str, rcs_optarg);
			flags |= FORCE;
			break;
		case 'I':
			rcs_setrevstr(&rev_str, rcs_optarg);
			flags |= INTERACTIVE;
			break;

		case 'k':
			kflag = rcs_kflag_get(rcs_optarg);
			if (RCS_KWEXP_INVAL(kflag)) {
				cvs_log(LP_ERR,
				    "invalid RCS keyword expansion mode");
				(usage)();
				exit(1);
			}
			break;
		case 'l':
			if (flags & CO_UNLOCK) {
				cvs_log(LP_ERR, "warning: -u overridden by -l");
				flags &= ~CO_UNLOCK;
			}
			rcs_setrevstr(&rev_str, rcs_optarg);
			flags |= CO_LOCK;
			break;
		case 'M':
			rcs_setrevstr(&rev_str, rcs_optarg);
			flags |= CO_REVDATE;
			break;
		case 'p':
			rcs_setrevstr(&rev_str, rcs_optarg);
			pipeout = 1;
			break;
		case 'q':
			rcs_setrevstr(&rev_str, rcs_optarg);
			verbose = 0;
			break;
		case 'r':
			rcs_setrevstr(&rev_str, rcs_optarg);
			break;
		case 's':
			state = xstrdup(rcs_optarg);
			flags |= CO_STATE;
			break;
		case 'T':
			flags |= PRESERVETIME;
			break;
		case 'u':
			rcs_setrevstr(&rev_str, rcs_optarg);
			if (flags & CO_LOCK) {
				cvs_log(LP_ERR, "warning: -l overridden by -u");
				flags &= ~CO_LOCK;
			}
			flags |= CO_UNLOCK;
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
			/* NOTREACHED */
		case 'w':
			/* if no argument, assume current user */
			if (rcs_optarg == NULL) {
				if ((author = getlogin()) == NULL)
					fatal("getlogin failed");
			} else
				author = xstrdup(rcs_optarg);
			flags |= CO_AUTHOR;
			break;
		case 'x':
			/* Use blank extension if none given. */
			rcs_suffixes = rcs_optarg ? rcs_optarg : "";
			break;
		case 'z':
			timezone_flag = rcs_optarg;
			break;
		default:
			(usage)();
			exit(1);
		}
	}

	argc -= rcs_optind;
	argv += rcs_optind;

	if (argc == 0) {
		cvs_log(LP_ERR, "no input file");
		(usage)();
		exit (1);
	}

	if ((username = getlogin()) == NULL) {
		cvs_log(LP_ERRNO, "failed to get username");
		exit (1);
	}

	for (i = 0; i < argc; i++) {
		frev = NULL;
		if (rcs_statfile(argv[i], fpath, sizeof(fpath)) < 0)
			continue;

		if (verbose == 1)
			printf("%s  -->  %s\n", fpath,
			    (pipeout == 1) ? "standard output" : argv[i]);

		if ((flags & CO_LOCK) && (kflag & RCS_KWEXP_VAL)) {
			cvs_log(LP_ERR, "%s: cannot combine -kv and -l", fpath);
			continue;
		}

		if ((file = rcs_open(fpath, RCS_RDWR|RCS_PARSE_FULLY)) == NULL)
			continue;

		if (flags & PRESERVETIME)
			rcs_mtime = rcs_get_mtime(file->rf_path);

		rcs_kwexp_set(file, kflag);

		if (rev_str != NULL)
			rcs_set_rev(rev_str, &rev);
		else {
			rev = rcsnum_alloc();
			rcsnum_cpy(file->rf_head, rev, 0);
		}

		if ((status = checkout_rev(file, rev, argv[i], flags,
		    username, author, state, date)) < 0) {
			rcs_close(file);
			rcsnum_free(rev);
			continue;
		}

		if (verbose == 1)
			printf("done\n");

		rcs_close(file);
		rcsnum_free(rev);

		if (flags & PRESERVETIME)
			rcs_set_mtime(fpath, rcs_mtime);
	}

	if ((rev != RCS_HEAD_REV) && (frev != NULL))
		rcsnum_free(frev);

	return (status);
}

void
checkout_usage(void)
{
	fprintf(stderr,
	    "usage: co [-TV] [-ddate] [-f[rev]] [-I[rev]] [-kmode] [-l[rev]]\n"
	    "          [-M[rev]] [-p[rev]] [-q[rev]] [-r[rev]] [-sstate]\n"
	    "          [-u[rev]] [-w[user]] [-xsuffixes] [-ztz] file ...\n");
}

/*
 * Checkout revision <rev> from RCSFILE <file>, writing it to the path <dst>
 * Currenly recognised <flags> are CO_LOCK, CO_UNLOCK and CO_REVDATE.
 *
 * Looks up revision based upon <lockname>, <author>, <state> and <date>
 *
 * Returns 0 on success, -1 on failure.
 */
int
checkout_rev(RCSFILE *file, RCSNUM *frev, const char *dst, int flags,
    const char *lockname, const char *author, const char *state,
    const char *date)
{
	BUF *bp;
	u_int i;
	int lcount;
	char buf[16];
	mode_t mode = 0444;
	struct stat st;
	struct rcs_delta *rdp;
	struct rcs_lock *lkp;
	char *content, msg[128], *fdate;
	time_t rcsdate, givendate;
	RCSNUM *rev;

	rcsdate = givendate = -1;
	if (date != NULL)
		givendate = cvs_date_parse(date);

	/* XXX rcsnum_cmp()
	 * Check out the latest revision if <frev> is greater than HEAD
	 */
	for (i = 0; i < file->rf_head->rn_len; i++) {
		if (file->rf_head->rn_id[i] < frev->rn_id[i]) {
			frev = file->rf_head;
			break;
		}
	}

	lcount = 0;
	TAILQ_FOREACH(lkp, &(file->rf_locks), rl_list) {
		if (!strcmp(lkp->rl_name, lockname))
			lcount++;
	}

	/*
	 * If the user didn't specify any revision, we cycle through
	 * revisions to lookup the first one that matches what he specified.
	 *
	 * If we cannot find one, we return an error.
	 */
	rdp = NULL;
	if (frev == file->rf_head) {
		if (lcount > 1) {
			cvs_log(LP_WARN,
			    "multiple revisions locked by %s; "
			    "please specify one", lockname);
			return (-1);
		}

		TAILQ_FOREACH(rdp, &file->rf_delta, rd_list) {
			if (date != NULL) {
				fdate = asctime(&rdp->rd_date);
				rcsdate = cvs_date_parse(fdate);
				if (givendate <= rcsdate)
					continue;
			}

			if ((author != NULL) &&
			    (strcmp(rdp->rd_author, author)))
				continue;

			if ((state != NULL) &&
			    (strcmp(rdp->rd_state, state)))
				continue;

			frev = rdp->rd_num;
			break;
		}
	} else {
		rdp = rcs_findrev(file, frev);
	}

	if (rdp == NULL) {
		checkout_err_nobranch(file, author, date, state, flags);
		return (-1);
	}

	rev = rdp->rd_num;
	rcsnum_tostr(rev, buf, sizeof(buf));

	if (rdp->rd_locker != NULL) {
		if (strcmp(lockname, rdp->rd_locker)) {
			strlcpy(msg, "Revision %s is already locked by %s; ",
			    sizeof(msg));
			if (flags & CO_UNLOCK)
				strlcat(msg, "use co -r or rcs -u", sizeof(msg));
			cvs_log(LP_ERR, msg, buf, rdp->rd_locker);
			return (-1);
		}
	}

	if ((verbose == 1) && !(flags & NEWFILE) && !(flags & CO_REVERT))
		printf("revision %s", buf);

	if ((verbose == 1) && (flags & CO_REVERT))
		printf("done");


	if ((bp = rcs_getrev(file, rev)) == NULL) {
		cvs_log(LP_ERR, "cannot find revision `%s'", buf);
		return (-1);
	}

	/*
	 * Do keyword expansion if required.
	 */
	bp = rcs_kwexp_buf(bp, file, rev);

	/*
	 * File inherits permissions from its ,v file
	 */
	if (stat(file->rf_path, &st) == -1)
		fatal("could not stat rcsfile");

	mode = st.st_mode;

	if (flags & CO_LOCK) {
		if ((lockname != NULL)
		    && (rcs_lock_add(file, lockname, rev) < 0)) {
			if (rcs_errno != RCS_ERR_DUPENT)
				return (-1);
		}

		/* Strip all write bits from mode */
		mode = st.st_mode &
		    (S_IXUSR|S_IXGRP|S_IXOTH|S_IRUSR|S_IRGRP|S_IROTH);
		mode |= S_IWUSR;
		if ((verbose == 1) && !(flags & NEWFILE)
		    && !(flags & CO_REVERT))
			printf(" (locked)");
	} else if (flags & CO_UNLOCK) {
		if (rcs_lock_remove(file, lockname, rev) < 0) {
			if (rcs_errno != RCS_ERR_NOENT)
				return (-1);
		}

		/* Strip all write bits from mode */
		mode = st.st_mode &
		    (S_IXUSR|S_IXGRP|S_IXOTH|S_IRUSR|S_IRGRP|S_IROTH);
		if ((verbose == 1) && !(flags & NEWFILE)
		    && !(flags & CO_REVERT))
			printf(" (unlocked)");
	}

	if ((verbose == 1) && !(flags & NEWFILE))
		printf("\n");

	if (flags & CO_LOCK) {
		if (rcs_errno != RCS_ERR_DUPENT)
			lcount++;
		if ((verbose == 1) && (lcount > 1) && !(flags & CO_REVERT))
			cvs_log(LP_WARN, "%s: warning: You now have %d locks.",
			    file->rf_path, lcount);
	}

	if ((pipeout == 0) && (stat(dst, &st) == 0) && !(flags & FORCE)) {
		/*
		 * XXX - Not sure what is "right".  If we go according
		 * to GNU's behavior, an existing file with no writable
		 * bits is overwritten without prompting the user.
		 *
		 * This is dangerous, so we always prompt.
		 * Unfortunately this interferes with an unlocked
		 * checkout followed by a locked checkout, which should
		 * not prompt.  One (unimplemented) solution is to check
		 * if the existing file is the same as the checked out
		 * revision, and prompt if there are differences.
		 */
		if (st.st_mode & (S_IWUSR|S_IWGRP|S_IWOTH))
			printf("writable ");
		printf("%s exists%s; ", dst,
		    (getuid() == st.st_uid) ? "" :
		    ", and you do not own it");
		printf("remove it? [ny](n): ");
		/* default is n */
		if (cvs_yesno() == -1) {
			if ((verbose == 1) && isatty(STDIN_FILENO))
				cvs_log(LP_ERR,
				    "writable %s exists; checkout aborted",
				    dst);
			else
				cvs_log(LP_ERR, "checkout aborted");
			return (-1);
		}
	}

	if (pipeout == 1) {
		cvs_buf_putc(bp, '\0');
		content = cvs_buf_release(bp);
		printf("%s", content);
		xfree(content);
	} else {
		if (cvs_buf_write(bp, dst, mode) < 0) {
			cvs_log(LP_ERR, "failed to write revision to file");
			cvs_buf_free(bp);
			return (-1);
		}
		cvs_buf_free(bp);
		if (flags & CO_REVDATE) {
			struct timeval tv[2];
			memset(&tv, 0, sizeof(tv));
			tv[0].tv_sec = (long)rcs_rev_getdate(file, rev);
			tv[1].tv_sec = tv[0].tv_sec;
			if (utimes(dst, (const struct timeval *)&tv) < 0)
				cvs_log(LP_ERRNO, "error setting utimes");
		}
	}

	return (0);
}

/*
 * checkout_err_nobranch()
 *
 * XXX - should handle the dates too.
 */
static void
checkout_err_nobranch(RCSFILE *file, const char *author, const char *date,
    const char *state, int flags)
{
	if (!(flags & CO_AUTHOR))
		author = NULL;
	if (!(flags & CO_STATE))
		state = NULL;

	cvs_log(LP_ERR, "%s: No revision on branch has%s%s%s%s%s%s.",
	    file->rf_path,
	    date ? " a date before " : "",
	    date ? date : "",
	    author ? " and author " + (date ? 0:4 ) : "",
	    author ? author : "",
	    state  ? " and state " + (date || author ? 0:4) : "",
	    state  ? state : "");
}
