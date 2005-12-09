/*	$OpenBSD: co.c,v 1.46 2005/12/09 06:59:27 joris Exp $	*/
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

#include <sys/param.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "rcs.h"
#include "rcsprog.h"

#define CO_OPTSTRING	"f::k:l::M::p::q::r::s:Tu::Vw::x:"

static void	checkout_err_nobranch(RCSFILE *, const char *, const char *,
    const char *, int);

int
checkout_main(int argc, char **argv)
{
	int i, ch, flags, kflag;
	RCSNUM *frev, *rev;
	RCSFILE *file;
	char fpath[MAXPATHLEN];
	char *author, *username;
	const char *state;
	time_t rcs_mtime = -1;

	flags = 0;
	kflag = RCS_KWEXP_ERR;
	rev = RCS_HEAD_REV;
	frev = NULL;
	state = NULL;
	author = NULL;

	while ((ch = rcs_getopt(argc, argv, CO_OPTSTRING)) != -1) {
		switch (ch) {
		case 'f':
			rcs_set_rev(rcs_optarg, &rev);
			flags |= FORCE;
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
			rcs_set_rev(rcs_optarg, &rev);
			flags |= CO_LOCK;
			break;
		case 'M':
			rcs_set_rev(rcs_optarg, &rev);
			flags |= CO_REVDATE;
			break;
		case 'p':
			rcs_set_rev(rcs_optarg, &rev);
			pipeout = 1;
			break;
		case 'q':
			rcs_set_rev(rcs_optarg, &rev);
			verbose = 0;
			break;
		case 'r':
			rcs_set_rev(rcs_optarg, &rev);
			break;
		case 's':
			if ((state = strdup(rcs_optarg)) == NULL) {
				cvs_log(LP_ERRNO, "out of memory");
				exit(1);
			}
			flags |= CO_STATE;
			break;
		case 'T':
			flags |= PRESERVETIME;
			break;
		case 'u':
			rcs_set_rev(rcs_optarg, &rev);
			flags |= CO_UNLOCK;
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
		case 'w':
			/* if no argument, assume current user */
			if (rcs_optarg == NULL) {
				if ((author = getlogin()) == NULL) {
					cvs_log(LP_ERRNO,
					    "could not get login");
					exit(1);
				}
			} else if ((author = strdup(rcs_optarg)) == NULL) {
				cvs_log(LP_ERRNO, "out of memory");
				exit(1);
			}

			flags |= CO_AUTHOR;
			break;
		case 'x':
			rcs_suffixes = rcs_optarg;
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
		if (rcs_statfile(argv[i], fpath, sizeof(fpath)) < 0)
			continue;

		if (verbose == 1)
			printf("%s  -->  %s\n", fpath,
			    (pipeout == 1) ? "standard output" : argv[i]);

		if ((flags & CO_LOCK) && (kflag & RCS_KWEXP_VAL)) {
			cvs_log(LP_ERR, "%s: cannot combine -kv and -l", fpath);
			continue;
		}

		if ((file = rcs_open(fpath, RCS_RDWR)) == NULL)
			continue;

		if (flags & PRESERVETIME)
			rcs_mtime = rcs_get_mtime(file->rf_path);

		if (kflag != RCS_KWEXP_ERR)
			rcs_kwexp_set(file, kflag);

		if (rev == RCS_HEAD_REV)
			frev = file->rf_head;
		else
			frev = rev;

		if (checkout_rev(file, frev, argv[i], flags,
		    username, author, state) < 0) {
				rcs_close(file);
				continue;
		}

		rcs_close(file);

		if (flags & PRESERVETIME)
			rcs_set_mtime(fpath, rcs_mtime);
	}

	if (rev != RCS_HEAD_REV)
		rcsnum_free(frev);

	return (0);
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
 * Looks up revision based upon <lockname>, <author>, <state>
 *
 * Returns 0 on success, -1 on failure.
 */
int
checkout_rev(RCSFILE *file, RCSNUM *frev, const char *dst, int flags,
    const char *lockname, const char *author, const char *state)
{
	BUF *bp;
	int lcount;
	char buf[16], yn;
	mode_t mode = 0444;
	struct stat st;
	struct rcs_delta *rdp;
	struct rcs_lock *lkp;
	char *content, msg[128];

	/* Check out the latest revision if <frev> is greater than HEAD */
	if (rcsnum_cmp(frev, file->rf_head, 0) == -1)
		frev = file->rf_head;

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
		checkout_err_nobranch(file, author, NULL, state, flags);
		return (-1);
	}

	rcsnum_tostr(frev, buf, sizeof(buf));

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

	if (verbose == 1)
		printf("revision %s", buf);


	if ((bp = rcs_getrev(file, frev)) == NULL) {
		cvs_log(LP_ERR, "cannot find revision `%s'", buf);
		return (-1);
	}

	if (flags & CO_LOCK) {
		if ((lockname != NULL)
		    && (rcs_lock_add(file, lockname, frev) < 0)) {
			if (rcs_errno != RCS_ERR_DUPENT)
				return (-1);
		}

		mode = 0644;
		if (verbose == 1)
			printf(" (locked)");
	} else if (flags & CO_UNLOCK) {
		if (rcs_lock_remove(file, lockname, frev) < 0) {
			if (rcs_errno != RCS_ERR_NOENT)
				return (-1);
		}

		mode = 0444;
		if (verbose == 1)
			printf(" (unlocked)");
	}

	if (verbose == 1)
		printf("\n");

	if (flags & CO_LOCK) {
		lcount++;
		if (lcount > 1)
			cvs_log(LP_WARN, "You now have %d locks.", lcount);
	}

	if ((pipeout == 0) && (stat(dst, &st) == 0) && !(flags & FORCE)) {
		if (st.st_mode & S_IWUSR) {
			yn = 0;
			if (verbose == 0) {
				cvs_log(LP_ERR,
				    "writable %s exists; checkout aborted",
				    dst);
				return (-1);
			}

			while ((yn != 'y') && (yn != 'n')) {
				printf("writable %s exists%s; ", dst,
				    ((uid_t)getuid() == st.st_uid) ? "" :
				    ", and you do not own it");
				printf("remove it? [ny](n): ");
				fflush(stdout);
				yn = getchar();
			}

			if (yn == 'n') {
				cvs_log(LP_ERR, "checkout aborted");
				return (-1);
			}
		}
	}

	if (pipeout == 1) {
		cvs_buf_putc(bp, '\0');
		content = cvs_buf_release(bp);
		printf("%s", content);
		free(content);
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
			tv[0].tv_sec = (long)rcs_rev_getdate(file, frev);
			tv[1].tv_sec = tv[0].tv_sec;
			if (utimes(dst, (const struct timeval *)&tv) < 0)
				cvs_log(LP_ERRNO, "error setting utimes");
		}

		if (verbose == 1)
			printf("done\n");
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
