/*	$OpenBSD: ci.c,v 1.47 2005/10/19 11:37:11 niallo Exp $	*/
/*
 * Copyright (c) 2005 Niall O'Higgins <niallo@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following cinditions
 * are met:
 *
 * 1. Redistributions of source cide must retain the above cipyright
 *    notice, this list of cinditions and the following disclaimer.
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <pwd.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include "log.h"
#include "rcs.h"
#include "diff.h"
#include "rcsprog.h"

#define LOCK_LOCK	1
#define LOCK_UNLOCK	2

#define DATE_NOW        -1
#define DATE_MTIME      -2

static char * checkin_diff_file(RCSFILE *, RCSNUM *, const char *);
static char * checkin_getlogmsg(RCSNUM *, RCSNUM *);

void
checkin_usage(void)
{
	fprintf(stderr,
	    "usage: ci [-jMNqV] [-d[date]] [-f[rev]] [-kmode] [-l[rev]]\n"
	    "          [-mmsg] [-Nsymbol] [-nsymbol] [-r[rev]] [-u[rev]]\n"
            "          [-wusername] file ...\n");
}

/*
 * checkin_main()
 *
 * Handler for the `ci' program.
 * Returns 0 on success, or >0 on error.
 */
int
checkin_main(int argc, char **argv)
{
	int found, notlocked, ret;
	int i, ch, force, lkmode, interactive, rflag, status, symforce;
	mode_t fmode;
	time_t date;
	RCSFILE *file;
	RCSNUM *frev, *newrev;
	char fpath[MAXPATHLEN];
	char *rcs_msg, *filec, *deltatext, *username, rbuf[16];
	const char *symbol = NULL;
	struct rcs_lock *lkp;
	BUF *bp;

	date = DATE_NOW;
	file = NULL;
	rcs_msg = username = NULL;
	newrev =  NULL;
	fmode = force = lkmode = rflag = status = symforce = 0;
	interactive = 1;


	while ((ch = rcs_getopt(argc, argv, "d::f::j:k:l::m:M:N:n:qr::u::Vw:")) != -1) {
		switch (ch) {
		case 'd':
			if (rcs_optarg == NULL)
				date = DATE_MTIME;
			else if ((date = cvs_date_parse(rcs_optarg)) <= 0) {
				cvs_log(LP_ERR, "invalide date");
				exit(1);
			}
			break;
		case 'f':
			rcs_set_rev(rcs_optarg, &newrev);
			force = 1;
			break;
		case 'h':
			(usage)();
			exit(0);
		case 'l':
			rcs_set_rev(rcs_optarg, &newrev);
			lkmode = LOCK_LOCK;
			break;
		case 'm':
			rcs_msg = rcs_optarg;
			interactive = 0;
			break;
		case 'N':
			if ((symbol = strdup(rcs_optarg)) == NULL) {
				cvs_log(LP_ERRNO, "out of memory");
				exit(1);
			}
			if (rcs_sym_check(symbol) != 1) {
				cvs_log(LP_ERR, "invalid symbol `%s'", symbol);
				exit(1);
			}
			symforce = 1;
			break;
		case 'n':
			if ((symbol = strdup(rcs_optarg)) == NULL) {
				cvs_log(LP_ERRNO, "out of memory");
				exit(1);
			}
			if (rcs_sym_check(symbol) != 1) {
				cvs_log(LP_ERR, "invalid symbol `%s'", symbol);
				exit(1);
			}
			break;
		case 'q':
			verbose = 0;
			break;
		case 'r':
			rcs_set_rev(rcs_optarg, &newrev);
			rflag = 1;
			break;
		case 'u':
			rcs_set_rev(rcs_optarg, &newrev);
			lkmode = LOCK_UNLOCK;
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
		case 'w':
			username = rcs_optarg;
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
		exit(1);
	}

	if ((username == NULL) && (username = getlogin()) == NULL) {
		cvs_log(LP_ERRNO, "failed to get username");
		exit(1);
	}


	for (i = 0; i < argc; i++) {
		if (rcs_statfile(argv[i], fpath, sizeof(fpath)) < 0)
			continue;

		file = rcs_open(fpath, RCS_RDWR, fmode);
		if (file == NULL) {
			cvs_log(LP_ERR, "failed to open rcsfile '%s'", fpath);
			exit(1);
		}

		frev = file->rf_head;

		if (verbose == 1)
			printf("%s  <--  %s\n", fpath, argv[i]);

		/*
		 * If revision passed on command line is less than HEAD, bail.
		 */
		if ((newrev != NULL) && (rcsnum_cmp(newrev, frev, 0) > 0)) {
			cvs_log(LP_ERR, "revision is too low!");
			status = 1;
			rcs_close(file);
			continue;
		}

		/*
		 * Load file contents
		 */
		if ((bp = cvs_buf_load(argv[i], BUF_AUTOEXT)) == NULL) {
			cvs_log(LP_ERR, "failed to load '%s'", argv[i]);
			exit(1);
		}

		if (cvs_buf_putc(bp, '\0') < 0)
			exit(1);

		filec = (char *)cvs_buf_release(bp);

		/*
		 * Get RCS patch
		 */
		if ((deltatext = checkin_diff_file(file, frev, argv[i])) == NULL) {
			cvs_log(LP_ERR, "failed to get diff");
			exit(1);
		}

		/*
		 * If -f is not specified and there are no differences, tell the
		 * user and revert to latest version.
		 */
		if ((!force) && (strlen(deltatext) < 1)) {
			rcsnum_tostr(frev, rbuf, sizeof(rbuf));
			cvs_log(LP_WARN, 
			    "file is unchanged; reverting to previous revision %s",
			    rbuf);
			(void)unlink(argv[i]);
			if (lkmode != 0)
				checkout_rev(file, frev, argv[i], lkmode,
				    username, 0);
			rcs_lock_remove(file, frev);
			rcs_close(file);
			if (verbose == 1)
				printf("done\n");
			continue;
		}

		/*
		 * Check for a lock belonging to this user. If none,
		 * abort check-in.
		 */
		found = 0;
		notlocked = 1;
		if (!TAILQ_EMPTY(&(file->rf_locks))) {
			TAILQ_FOREACH(lkp, &(file->rf_locks), rl_list) {
				if (!strcmp(lkp->rl_name, username))
					notlocked = 0;

				if (!strcmp(lkp->rl_name, username) &&
				    !rcsnum_cmp(lkp->rl_num, frev, 0)) {
					found = 1;
					break;
				}
			}
		}

		if (found == 0 && notlocked == 0) {
			cvs_log(LP_ERR, "no locks set for '%s'", username);
			status = 1;
			rcs_close(file);
			continue;
		}

		/*
		 * If no log message specified, get it interactively.
		 */
		if (rcs_msg == NULL)
			rcs_msg = checkin_getlogmsg(frev, newrev);

		/*
		 * Remove the lock
		 */
		if (rcs_lock_remove(file, frev) < 0) {
			if (rcs_errno != RCS_ERR_NOENT)
			    cvs_log(LP_WARN, "failed to remove lock");
                }

		/*
		 * Current head revision gets the RCS patch as rd_text
		 */
		if (rcs_deltatext_set(file, frev, deltatext) == -1) {
			cvs_log(LP_ERR,
			    "failed to set new rd_text for head rev");
			exit (1);
		}

		/*
		 * Set the date of the revision to be the last modification time
		 * of the working file if -d is specified without an argument.
		 */
		if (date == DATE_MTIME) {
			struct stat sb;
			if (stat(argv[i], &sb) != 0) {
				cvs_log(LP_ERRNO, "failed to stat: `%s'",
				    argv[i]);
				rcs_close(file);
				continue;
			}
			date = (time_t)sb.st_mtimespec.tv_sec;
		}

		/*
		 * Now add our new revision
		 */
		if (rcs_rev_add(file, (newrev == NULL ? RCS_HEAD_REV : newrev),
		    rcs_msg, date, username) != 0) {
			cvs_log(LP_ERR, "failed to add new revision");
			exit(1);
		}

		/*
		 * If we are checking in to a non-default (ie user-specified)
		 * revision, set head to this revision.
		 */
		if (newrev != NULL)
			rcs_head_set(file, newrev);
		else
			newrev = file->rf_head;

		/*
		 * New head revision has to contain entire file;
		 */
                if (rcs_deltatext_set(file, frev, filec) == -1) {
			cvs_log(LP_ERR, "failed to set new head revision");
			exit(1);
		}

		/*
		 * Attach a symbolic name to this revision if specified.
		 */
		if (symbol != NULL) {
			if (verbose == 1)
				printf("symbol: %s\n", symbol);
			if (symforce)
				rcs_sym_remove(file, symbol);
			if ((ret = rcs_sym_add(file, symbol, newrev) == -1)
			    && (rcs_errno == RCS_ERR_DUPENT)) {
				rcsnum_tostr(rcs_sym_getrev(file, symbol),
				    rbuf, sizeof(rbuf));
				cvs_log(LP_ERR,
				    "symbolic name %s already bound to %s",
				    symbol, rbuf);
				status = 1;
				rcs_close(file);
				continue;
			} else if (ret == -1) {
				cvs_log(LP_ERR, "problem adding symbol: %s",
				    symbol);
				status = 1;
				rcs_close(file);
				continue;
			}
		}

		free(deltatext);
		free(filec);
		(void)unlink(argv[i]);

		/*
		 * Do checkout if -u or -l are specified.
		 */
		if (lkmode != 0 && !rflag)
			checkout_rev(file, newrev, argv[i], lkmode, username, 0);

		/* File will NOW be synced */
		rcs_close(file);

		if (interactive) {
			free(rcs_msg);
			rcs_msg = NULL;
		}
	}

	return (status);
}

static char *
checkin_diff_file(RCSFILE *rfp, RCSNUM *rev, const char *filename)
{
	char path1[MAXPATHLEN], path2[MAXPATHLEN];
	BUF *b1, *b2, *b3;
	char rbuf[64], *deltatext;

	rcsnum_tostr(rev, rbuf, sizeof(rbuf));

	if ((b1 = cvs_buf_load(filename, BUF_AUTOEXT)) == NULL) {
		cvs_log(LP_ERR, "failed to load file: '%s'", filename);
		return (NULL);
	}

	if ((b2 = rcs_getrev(rfp, rev)) == NULL) {
		cvs_log(LP_ERR, "failed to load revision");
		cvs_buf_free(b1);
		return (NULL);
	}

	if ((b3 = cvs_buf_alloc(128, BUF_AUTOEXT)) == NULL) {
		cvs_log(LP_ERR, "failed to allocated buffer for diff");
		cvs_buf_free(b1);
		cvs_buf_free(b2);
		return (NULL);
	}

	strlcpy(path1, "/tmp/diff1.XXXXXXXXXX", sizeof(path1));
	if (cvs_buf_write_stmp(b1, path1, 0600) == -1) {
		cvs_log(LP_ERRNO, "could not write temporary file");
		cvs_buf_free(b1);
		cvs_buf_free(b2);
		return (NULL);
	}
	cvs_buf_free(b1);

	strlcpy(path2, "/tmp/diff2.XXXXXXXXXX", sizeof(path2));
	if (cvs_buf_write_stmp(b2, path2, 0600) == -1) {
		cvs_buf_free(b2);
		(void)unlink(path1);
		return (NULL);
	}
	cvs_buf_free(b2);

	diff_format = D_RCSDIFF;
	cvs_diffreg(path1, path2, b3);
	(void)unlink(path1);
	(void)unlink(path2);

	cvs_buf_putc(b3, '\0');
	deltatext = (char *)cvs_buf_release(b3);

	return (deltatext);
}

/*
 * Get log message from user interactively.
 */
static char *
checkin_getlogmsg(RCSNUM *rev, RCSNUM *rev2)
{
	char   *rcs_msg, buf[128], nrev[16], prev[16];
	BUF    *logbuf;
	RCSNUM *tmprev;

	rcs_msg = NULL;
	tmprev = rcsnum_alloc();
	rcsnum_cpy(rev, tmprev, 16);
	rcsnum_tostr(tmprev, prev, sizeof(prev));
	if (rev2 == NULL)
		rcsnum_tostr(rcsnum_inc(tmprev), nrev, sizeof(nrev));
	else
		rcsnum_tostr(rev2, nrev, sizeof(nrev));
	rcsnum_free(tmprev);

	if ((logbuf = cvs_buf_alloc(64, BUF_AUTOEXT)) == NULL) {
		cvs_log(LP_ERR, "failed to allocate log buffer");
		return (NULL);
	}

	if (verbose == 1)
		printf("new revision: %s; previous revision: %s\n", nrev,
		    prev);
	printf("enter log message, terminated with single "
	    "'.' or end of file:\n");
	printf(">> ");

	for (;;) {
		fgets(buf, (int)sizeof(buf), stdin);
		if (feof(stdin) || ferror(stdin) || buf[0] == '.')
			break;
		cvs_buf_append(logbuf, buf, strlen(buf));
		printf(">> ");
	}

	cvs_buf_putc(logbuf, '\0');
	rcs_msg = (char *)cvs_buf_release(logbuf);

	return (rcs_msg);
}
