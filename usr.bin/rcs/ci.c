/*	$OpenBSD: ci.c,v 1.108 2006/03/06 14:49:54 jmc Exp $	*/
/*
 * Copyright (c) 2005, 2006 Niall O'Higgins <niallo@openbsd.org>
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

#include "includes.h"

#include "rcsprog.h"
#include "diff.h"

#define CI_OPTSTRING	"d::f::i::j::k::l::m:M::N:n:qr::s:Tt:u::Vw:x:"
#define DATE_NOW	-1
#define DATE_MTIME	-2

#define KW_ID		"Id"
#define KW_AUTHOR	"Author"
#define KW_DATE		"Date"
#define KW_STATE	"State"
#define KW_REVISION	"Revision"

#define KW_TYPE_ID		1
#define KW_TYPE_AUTHOR		2
#define KW_TYPE_DATE		3
#define KW_TYPE_STATE		4
#define KW_TYPE_REVISION	5

#define KW_NUMTOKS_ID		10
#define KW_NUMTOKS_AUTHOR	3
#define KW_NUMTOKS_DATE		4
#define KW_NUMTOKS_STATE	3
#define KW_NUMTOKS_REVISION	3

extern struct rcs_kw rcs_expkw[];

struct checkin_params {
	int flags, openflags;
	mode_t fmode;
	time_t date;
	RCSFILE *file;
	RCSNUM *frev, *newrev;
	char fpath[MAXPATHLEN], *rcs_msg, *username, *deltatext, *filename;
	char *author, *state;
	const char *symbol, *description;
};

static int	 checkin_attach_symbol(struct checkin_params *);
static int	 checkin_checklock(struct checkin_params *);
static char	*checkin_choose_rcsfile(const char *);
static char	*checkin_diff_file(struct checkin_params *);
static char	*checkin_getdesc(void);
static char	*checkin_getinput(const char *);
static char	*checkin_getlogmsg(RCSNUM *, RCSNUM *);
static int	 checkin_init(struct checkin_params *);
static int	 checkin_keywordscan(char *, RCSNUM **, time_t *, char **,
    char **);
static int	 checkin_keywordtype(char *);
static int	 checkin_mtimedate(struct checkin_params *);
static void	 checkin_parsekeyword(char *, RCSNUM **, time_t *, char **,
    char **);
static int	 checkin_update(struct checkin_params *);
static void	 checkin_revert(struct checkin_params *);

void
checkin_usage(void)
{
	fprintf(stderr,
	    "usage: ci [-jMNqV] [-d[date]] [-f[rev]] [-i[rev]] [-j[rev]]\n"
	    "	  [-k[rev]] [-l[rev]] [-M[rev]] [-mmsg] [-Nsymbol]\n"
	    "	  [-nsymbol] [-r[rev]] [-sstate] [-tfile|str] [-u[rev]]\n"
	    "	  [-wusername] [-xsuffixes] file ...\n");
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
	int i, ch, status;
	struct checkin_params pb;

	pb.date = DATE_NOW;
	pb.file = NULL;
	pb.rcs_msg = pb.username = pb.author = pb.state = NULL;
	pb.symbol = pb.description = NULL;
	pb.newrev =  NULL;
	pb.fmode = pb.flags = status = 0;

	pb.flags = INTERACTIVE;
	pb.openflags = RCS_RDWR|RCS_CREATE|RCS_PARSE_FULLY;

	while ((ch = rcs_getopt(argc, argv, CI_OPTSTRING)) != -1) {
		switch (ch) {
		case 'd':
			if (rcs_optarg == NULL)
				pb.date = DATE_MTIME;
			else if ((pb.date = cvs_date_parse(rcs_optarg)) <= 0)
				fatal("invalid date");
			break;
		case 'f':
			rcs_set_rev(rcs_optarg, &pb.newrev);
			pb.flags |= FORCE;
			break;
		case 'h':
			(usage)();
			exit(0);
		case 'i':
			rcs_set_rev(rcs_optarg, &pb.newrev);
			pb.openflags |= RCS_CREATE;
			pb.flags |= CI_INIT;
			break;
		case 'j':
			rcs_set_rev(rcs_optarg, &pb.newrev);
			pb.openflags &= ~RCS_CREATE;
			pb.flags &= ~CI_INIT;
			break;
		case 'k':
			rcs_set_rev(rcs_optarg, &pb.newrev);
			pb.flags |= CI_KEYWORDSCAN;
			break;
		case 'l':
			rcs_set_rev(rcs_optarg, &pb.newrev);
			pb.flags |= CO_LOCK;
			break;
		case 'M':
			rcs_set_rev(rcs_optarg, &pb.newrev);
			pb.flags |= CO_REVDATE;
			break;
		case 'm':
			pb.rcs_msg = rcs_optarg;
			if (pb.rcs_msg == NULL)
				fatal("missing message for -m option");
			pb.flags &= ~INTERACTIVE;
			break;
		case 'N':
			pb.symbol = xstrdup(rcs_optarg);
			if (rcs_sym_check(pb.symbol) != 1)
				fatal("invalid symbol `%s'", pb.symbol);
			pb.flags |= CI_SYMFORCE;
			break;
		case 'n':
			pb.symbol = xstrdup(rcs_optarg);
			if (rcs_sym_check(pb.symbol) != 1)
				fatal("invalid symbol `%s'", pb.symbol);
			break;
		case 'q':
			verbose = 0;
			break;
		case 'r':
			rcs_set_rev(rcs_optarg, &pb.newrev);
			pb.flags |= CI_DEFAULT;
			break;
		case 's':
			pb.state = rcs_optarg;
			if (rcs_state_check(pb.state) < 0)
				fatal("invalid state `%s'", pb.state);
			break;
		case 'T':
			pb.flags |= PRESERVETIME;
			break;
		case 't':
			pb.description = xstrdup(rcs_optarg);
			break;
		case 'u':
			rcs_set_rev(rcs_optarg, &pb.newrev);
			pb.flags |= CO_UNLOCK;
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
		case 'w':
			pb.author = xstrdup(rcs_optarg);
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
		exit(1);
	}

	if ((pb.username = getlogin()) == NULL)
		fatal("getlogin failed");


	for (i = 0; i < argc; i++) {
		pb.filename = argv[i];

		/*
		 * Test for existence of ,v file. If we are expected to
		 * create one, set NEWFILE flag.
		 */
		if (rcs_statfile(pb.filename, pb.fpath, sizeof(pb.fpath)) < 0) {
			if (pb.openflags & RCS_CREATE)
				pb.flags |= NEWFILE;
			else {
				cvs_log(LP_ERR, "No existing RCS file");
				status = 1;
				continue;
			}
		} else {
			if (pb.flags & CI_INIT) {
				cvs_log(LP_ERR, "%s already exists", pb.fpath);
				status = 1;
				continue;
			}
			pb.openflags &= ~RCS_CREATE;
		}
		/*
		 * If we are to create a new ,v file, we must decide where it
		 * should go.
		 */
		if (pb.flags & NEWFILE) {
			char *fpath = checkin_choose_rcsfile(pb.filename);
			if (fpath == NULL) {
				status = 1;
				continue;
			}
			strlcpy(pb.fpath, fpath, sizeof(pb.fpath));
			xfree(fpath);
		}

		pb.file = rcs_open(pb.fpath, pb.openflags, pb.fmode);

		if (pb.file == NULL)
			fatal("failed to open rcsfile '%s'", pb.fpath);

		if (verbose == 1)
			printf("%s  <--  %s\n", pb.fpath, pb.filename);

		if (pb.flags & NEWFILE)
			status = checkin_init(&pb);
		else
			status = checkin_update(&pb);
	}

	if (verbose == 1)
		printf("done\n");

	return (status);
}

/*
 * checkin_diff_file()
 *
 * Generate the diff between the working file and a revision.
 * Returns pointer to a char array on success, NULL on failure.
 */
static char *
checkin_diff_file(struct checkin_params *pb)
{
	char path1[MAXPATHLEN], path2[MAXPATHLEN];
	BUF *b1, *b2, *b3;
	char rbuf[64], *deltatext;

	rcsnum_tostr(pb->frev, rbuf, sizeof(rbuf));

	if ((b1 = cvs_buf_load(pb->filename, BUF_AUTOEXT)) == NULL) {
		cvs_log(LP_ERR, "failed to load file: '%s'", pb->filename);
		return (NULL);
	}

	if ((b2 = rcs_getrev(pb->file, pb->frev)) == NULL) {
		cvs_log(LP_ERR, "failed to load revision");
		cvs_buf_free(b1);
		return (NULL);
	}

	if ((b3 = cvs_buf_alloc((size_t)128, BUF_AUTOEXT)) == NULL) {
		cvs_log(LP_ERR, "failed to allocated buffer for diff");
		cvs_buf_free(b1);
		cvs_buf_free(b2);
		return (NULL);
	}

	strlcpy(path1, rcs_tmpdir, sizeof(path1));
	strlcat(path1, "/diff1.XXXXXXXXXX", sizeof(path1));
	if (cvs_buf_write_stmp(b1, path1, 0600) == -1) {
		cvs_log(LP_ERRNO, "could not write temporary file");
		cvs_buf_free(b1);
		cvs_buf_free(b2);
		return (NULL);
	}
	cvs_buf_free(b1);

	strlcpy(path2, rcs_tmpdir, sizeof(path2));
	strlcat(path2, "/diff2.XXXXXXXXXX", sizeof(path2));
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
 * checkin_getlogmsg()
 *
 * Get log message from user interactively.
 * Returns pointer to a char array on success, NULL on failure.
 */
static char *
checkin_getlogmsg(RCSNUM *rev, RCSNUM *rev2)
{
	char   *rcs_msg, nrev[16], prev[16];
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

	if (verbose == 1)
		printf("new revision: %s; previous revision: %s\n", nrev,
		    prev);

	rcs_msg = checkin_getinput("enter log message, terminated with a "
	    "single '.' or end of file:\n>> ");
	return (rcs_msg);
}


/*
 * checkin_getdesc()
 *
 * Get file description interactively.
 * Returns pointer to a char array on success, NULL on failure.
 */
static char *
checkin_getdesc()
{
	char *description;

	description = checkin_getinput("enter description, terminated with "
	    "single '.' or end of file:\n"
	    "NOTE: This is NOT the log message!\n>> ");
	return (description);
}

/*
 * checkin_getinput()
 *
 * Get some input from the user, in RCS style, prompting with message <prompt>.
 * Returns pointer to a char array on success, NULL on failure.
 */
static char *
checkin_getinput(const char *prompt)
{
	BUF *inputbuf;
	char *input, buf[128];

	if ((inputbuf = cvs_buf_alloc((size_t)64, BUF_AUTOEXT)) == NULL) {
		cvs_log(LP_ERR, "failed to allocate input buffer");
		return (NULL);
	}

	printf(prompt);
	for (;;) {
		fgets(buf, (int)sizeof(buf), stdin);
		if (feof(stdin) || ferror(stdin) || buf[0] == '.')
			break;
		cvs_buf_append(inputbuf, buf, strlen(buf));
		printf(">> ");
	}

	cvs_buf_putc(inputbuf, '\0');
	input = (char *)cvs_buf_release(inputbuf);

	return (input);
}

/*
 * checkin_update()
 *
 * Do a checkin to an existing RCS file.
 *
 * On success, return 0. On error return -1.
 */
static int
checkin_update(struct checkin_params *pb)
{
	char  *filec, numb1[64], numb2[64];
	BUF *bp;

	/*
	 * XXX this is wrong, we need to get the revision the user
	 * has the lock for. So we can decide if we want to create a
	 * branch or not. (if it's not current HEAD we need to branch).
	 */
	pb->frev = pb->file->rf_head;

	if (checkin_checklock(pb) < 0)
		return (-1);

	/* If revision passed on command line is less than HEAD, bail.
	 * XXX only applies to ci -r1.2 foo for example if HEAD is > 1.2 and
	 * there is no lock set for the user.
	 */
	if ((pb->newrev != NULL)
	    && (rcsnum_cmp(pb->newrev, pb->frev, 0) > 0)) {
		cvs_log(LP_ERR,
		    "%s: revision %s too low; must be higher than %s",
		    pb->file->rf_path,
		    rcsnum_tostr(pb->newrev, numb1, sizeof(numb1)),
		    rcsnum_tostr(pb->frev, numb2, sizeof(numb2)));
		rcs_close(pb->file);
		return (-1);
	}

	/*
	 * Set the date of the revision to be the last modification
	 * time of the working file if -d has no argument.
	 */
	if (pb->date == DATE_MTIME
	    && (checkin_mtimedate(pb) < 0))
		return (-1);

	/* Date from argv/mtime must be more recent than HEAD */
	if (pb->date != DATE_NOW) {
		time_t head_date = rcs_rev_getdate(pb->file, pb->frev);
		if (pb->date <= head_date) {
			char *head_date_str, *tdate;
			head_date_str = xstrdup(ctime(&head_date));
			head_date_str[strlen(head_date_str) - 1] = '\0';
			tdate = xstrdup(ctime(&pb->date));
			tdate[strlen(tdate) - 1] = '\0';
			cvs_log(LP_ERR,
			    "%s: Date %s preceeds %s in revision %s",
			    pb->file->rf_path, tdate, head_date_str,
			    rcsnum_tostr(pb->frev, numb2, sizeof(numb2)));
			rcs_close(pb->file);
			xfree(head_date_str);
			xfree(tdate);
			return (-1);
		}
	}

	/* Load file contents */
	if ((bp = cvs_buf_load(pb->filename, BUF_AUTOEXT)) == NULL) {
		cvs_log(LP_ERR, "failed to load '%s'", pb->filename);
		rcs_close(pb->file);
		return (-1);
	}

	if (cvs_buf_putc(bp, '\0') < 0) {
		rcs_close(pb->file);
		return (-1);
	}

	filec = (char *)cvs_buf_release(bp);

	/* Get RCS patch */
	if ((pb->deltatext = checkin_diff_file(pb)) == NULL) {
		cvs_log(LP_ERR, "failed to get diff");
		rcs_close(pb->file);
		return (-1);
	}

	/*
	 * If -f is not specified and there are no differences, tell
	 * the user and revert to latest version.
	 */
	if (!(pb->flags & FORCE) && (strlen(pb->deltatext) < 1)) {
		checkin_revert(pb);
		return (0);
	}

	/* If no log message specified, get it interactively. */
	if (pb->flags & INTERACTIVE)
		pb->rcs_msg = checkin_getlogmsg(pb->frev, pb->newrev);

	if (rcs_lock_remove(pb->file, pb->username, pb->frev) < 0) {
		if (rcs_errno != RCS_ERR_NOENT)
			cvs_log(LP_WARN, "failed to remove lock");
		else if (!(pb->flags & CO_LOCK))
			cvs_log(LP_WARN, "previous revision was not locked; "
			    "ignoring -l option");
	}

	/* Current head revision gets the RCS patch as rd_text */
	if (rcs_deltatext_set(pb->file, pb->frev, pb->deltatext) == -1)
		fatal("failed to set new rd_text for head rev");

	/* Now add our new revision */
	if (rcs_rev_add(pb->file,
	    (pb->newrev == NULL ? RCS_HEAD_REV : pb->newrev),
	    pb->rcs_msg, pb->date, pb->author) != 0) {
		cvs_log(LP_ERR, "failed to add new revision");
		rcs_close(pb->file);
		return (-1);
	}

	/*
	 * If we are checking in to a non-default (ie user-specified)
	 * revision, set head to this revision.
	 */
	if (pb->newrev != NULL)
		rcs_head_set(pb->file, pb->newrev);
	else
		pb->newrev = pb->file->rf_head;

	/* New head revision has to contain entire file; */
	if (rcs_deltatext_set(pb->file, pb->frev, filec) == -1)
		fatal("failed to set new head revision");

	/* Attach a symbolic name to this revision if specified. */
	if (pb->symbol != NULL
	    && (checkin_attach_symbol(pb) < 0))
		return (-1);

	/* Set the state of this revision if specified. */
	if (pb->state != NULL)
		(void)rcs_state_set(pb->file, pb->newrev, pb->state);

	xfree(pb->deltatext);
	xfree(filec);
	(void)unlink(pb->filename);

	/* Do checkout if -u or -l are specified. */
	if (((pb->flags & CO_LOCK) || (pb->flags & CO_UNLOCK))
	    && !(pb->flags & CI_DEFAULT))
		checkout_rev(pb->file, pb->newrev, pb->filename, pb->flags,
		    pb->username, pb->author, NULL, NULL);

	/* File will NOW be synced */
	rcs_close(pb->file);

	if (pb->flags & INTERACTIVE) {
		xfree(pb->rcs_msg);
		pb->rcs_msg = NULL;
	}
	return (0);
}

/*
 * checkin_init()
 *
 * Does an initial check in, just enough to create the new ,v file
 * On success, return 0. On error return -1.
 */
static int
checkin_init(struct checkin_params *pb)
{
	BUF *bp, *dp;
	char *filec, numb[64];
	const char *rcs_desc;

	/* Load file contents */
	if ((bp = cvs_buf_load(pb->filename, BUF_AUTOEXT)) == NULL) {
		cvs_log(LP_ERR, "failed to load '%s'", pb->filename);
		rcs_close(pb->file);
		return (-1);
	}

	if (cvs_buf_putc(bp, '\0') < 0) {
		rcs_close(pb->file);
		return (-1);
	}

	filec = (char *)cvs_buf_release(bp);

	/* Get default values from working copy if -k specified */
	if (pb->flags & CI_KEYWORDSCAN)
		checkin_keywordscan(filec, &pb->newrev, &pb->date, &pb->state,
		    &pb->author);

	/* Get description from user */
	if (pb->description == NULL)
		rcs_desc = (const char *)checkin_getdesc();
	else {
		if (*pb->description == '-') {
			pb->description++;
			rcs_desc = (const char *)pb->description;
		} else {
			dp = cvs_buf_load(pb->description, BUF_AUTOEXT);
			if (dp == NULL) {
				cvs_log(LP_ERR,
				    "failed to load description file '%s'",
				    pb->description);
				xfree(filec);
				rcs_close(pb->file);
				return (-1);
			}
			if (cvs_buf_putc(dp, '\0') < 0) {
				xfree(filec);
				rcs_close(pb->file);
				return (-1);
			}
			rcs_desc = (const char *)cvs_buf_release(dp);
		}
	}
	rcs_desc_set(pb->file, rcs_desc);

	/*
	 * Set the date of the revision to be the last modification
	 * time of the working file if -d has no argument.
	 */
	if (pb->date == DATE_MTIME
	    && (checkin_mtimedate(pb) < 0))
		return (-1);

	/* Now add our new revision */
	if (rcs_rev_add(pb->file,
	    (pb->newrev == NULL ? RCS_HEAD_REV : pb->newrev),
	    (pb->rcs_msg == NULL ? "Initial revision" : pb->rcs_msg),
	    pb->date, pb->author) != 0) {
		cvs_log(LP_ERR, "failed to add new revision");
		rcs_close(pb->file);
		return (-1);
	}
	/*
	 * If we are checking in to a non-default (ie user-specified)
	 * revision, set head to this revision.
	 */
	if (pb->newrev != NULL)
		rcs_head_set(pb->file, pb->newrev);
	else
		pb->newrev = pb->file->rf_head;

	/* New head revision has to contain entire file; */
	if (rcs_deltatext_set(pb->file, pb->file->rf_head, filec) == -1) {
		cvs_log(LP_ERR, "failed to set new head revision");
		rcs_close(pb->file);
		return (-1);
	}
	/* Attach a symbolic name to this revision if specified. */
	if (pb->symbol != NULL
	    && (checkin_attach_symbol(pb) < 0))
		return (-1);

	/* Set the state of this revision if specified. */
	if (pb->state != NULL)
		(void)rcs_state_set(pb->file, pb->newrev, pb->state);

	xfree(filec);
	(void)unlink(pb->filename);

	/* Do checkout if -u or -l are specified. */
	if (((pb->flags & CO_LOCK) || (pb->flags & CO_UNLOCK))
	    && !(pb->flags & CI_DEFAULT))
		checkout_rev(pb->file, pb->newrev, pb->filename, pb->flags,
		    pb->username, pb->author, NULL, NULL);

	printf("initial revision: %s\n",
	    rcsnum_tostr(pb->newrev, numb, sizeof(numb)));

	/* File will NOW be synced */
	rcs_close(pb->file);

	return (0);
}

/*
 * checkin_attach_symbol()
 *
 * Attempt to attach the specified symbol to the revision.
 * On success, return 0. On error return -1.
 */
static int
checkin_attach_symbol(struct checkin_params *pb)
{
	char rbuf[16];
	int ret;
	if (verbose == 1)
		printf("symbol: %s\n", pb->symbol);
	if (pb->flags & CI_SYMFORCE)
		rcs_sym_remove(pb->file, pb->symbol);
	if ((ret = rcs_sym_add(pb->file, pb->symbol, pb->newrev) == -1)
	    && (rcs_errno == RCS_ERR_DUPENT)) {
		rcsnum_tostr(rcs_sym_getrev(pb->file, pb->symbol),
		    rbuf, sizeof(rbuf));
		cvs_log(LP_ERR,
		    "symbolic name %s already bound to %s",
		    pb->symbol, rbuf);
		rcs_close(pb->file);
		return (-1);
	} else if (ret == -1) {
		cvs_log(LP_ERR, "problem adding symbol: %s",
		    pb->symbol);
		rcs_close(pb->file);
		return (-1);
	}
	return (0);
}

/*
 * checkin_revert()
 *
 * If there are no differences between the working file and the latest revision
 * and the -f flag is not specified, simply revert to the latest version and
 * warn the user.
 *
 */
static void
checkin_revert(struct checkin_params *pb)
{
	char rbuf[16];

	rcsnum_tostr(pb->frev, rbuf, sizeof(rbuf));
	cvs_log(LP_WARN,
	    "file is unchanged; reverting to previous revision %s",
	    rbuf);
	(void)unlink(pb->filename);
	if ((pb->flags & CO_LOCK) || (pb->flags & CO_UNLOCK))
		checkout_rev(pb->file, pb->frev, pb->filename,
		    pb->flags, pb->username, pb->author, NULL, NULL);
	rcs_lock_remove(pb->file, pb->username, pb->frev);
	rcs_close(pb->file);
}

/*
 * checkin_checklock()
 *
 * Check for the existence of a lock on the file.  If there are no locks, or it
 * is not locked by the correct user, return -1.  Otherwise, return 0.
 */
static int
checkin_checklock(struct checkin_params *pb)
{
	struct rcs_lock *lkp;

	TAILQ_FOREACH(lkp, &(pb->file->rf_locks), rl_list) {
		if ((!strcmp(lkp->rl_name, pb->username)) &&
		    (!rcsnum_cmp(lkp->rl_num, pb->frev, 0)))
			return (0);
	}

	cvs_log(LP_ERR,
	    "%s: no lock set by %s", pb->file->rf_path, pb->username);
	rcs_close(pb->file);
	return (-1);
}

/*
 * checkin_mtimedate()
 *
 * Set the date of the revision to be the last modification
 * time of the working file.
 *
 * On success, return 0. On error return -1.
 */
static int
checkin_mtimedate(struct checkin_params *pb)
{
	struct stat sb;
	if (stat(pb->filename, &sb) != 0) {
		cvs_log(LP_ERRNO, "failed to stat `%s'", pb->filename);
		rcs_close(pb->file);
		return (-1);
	}
	pb->date = (time_t)sb.st_mtimespec.tv_sec;
	return (0);
}

/*
 * checkin_choose_rcsfile()
 *
 * Given a relative filename, decide where the corresponding ,v file
 * should be.
 *
 * Returns pointer to a char array on success, NULL on failure.
 */
static char *
checkin_choose_rcsfile(const char *filename)
{
	char name[MAXPATHLEN], *basepath;
	const char *ptr;
	size_t len;
	struct stat sb;

	basepath = xmalloc(MAXPATHLEN);
	basepath[0] = '\0';
	if (strchr(filename, '/') == NULL) {
		strlcat(basepath, RCSDIR"/", MAXPATHLEN);
		if ((stat(basepath, &sb) == 0) && (sb.st_mode & S_IFDIR)) {
			/* <path>/RCS/<filename>,v */
			strlcat(basepath, filename, MAXPATHLEN);
			strlcat(basepath, RCS_FILE_EXT, MAXPATHLEN);
		} else {
			/* <path>/<filename>,v */
			strlcpy(basepath, filename, MAXPATHLEN);
			strlcat(basepath, RCS_FILE_EXT, MAXPATHLEN);
		}
	} else {
		ptr = filename;
		/* Walk backwards till we find the base directory */
		len = strlen(filename);
		ptr += len + 1;
		while (filename[len] != '/') {
			len--;
			ptr--;
		}
		/*
		 * Need two bytes extra for trailing slash and
		 * NUL-termination.
		 */
		len += 2;
		if (len > MAXPATHLEN) {
			xfree(basepath);
			return (NULL);
		}
		strlcpy(basepath, filename, len);
		strlcpy(name, ptr, MAXPATHLEN);
		strlcat(basepath, RCSDIR"/", MAXPATHLEN);
		if ((stat(basepath, &sb) == 0) && (sb.st_mode & S_IFDIR)) {
			/* <path>/RCS/<filename>,v */
			strlcat(basepath, name, MAXPATHLEN);
			strlcat(basepath, RCS_FILE_EXT, MAXPATHLEN);
		} else {
			/* <path>/<filename>,v */
			strlcpy(basepath, filename, MAXPATHLEN);
			strlcat(basepath, RCS_FILE_EXT, MAXPATHLEN);
		}
	}
	return (basepath);
}

/*
 * checkin_keywordscan()
 *
 * Searches working file for keyword values to determine its revision
 * number, creation date and author, and uses these values instead of
 * calculating them locally.
 *
 * Params: The data buffer to scan and pointers to pointers of variables in
 * which to store the outputs.
 *
 * On success, return 0. On error return -1.
 */
static int
checkin_keywordscan(char *data, RCSNUM **rev, time_t *date, char **author,
    char **state)
{
	size_t len;
	u_int i, j, found, end;
	char *c, *kwstr, *start, buf[128];

	c = start = kwstr = NULL;

	i =  found =  0;

	len = strlen(data);
	for (c = data; *c != '\0' && i < len; *c++) {
		if (*c == '$') {
			start = c;
			*c++;
			if (!isalpha(*c)) {
				c = start;
				continue;
			}
			/* look for any matching keywords */
			found = 0;
			for (j = 0; j < 10; j++) {
				if (!strncmp(c, rcs_expkw[j].kw_str,
				    strlen(rcs_expkw[j].kw_str))) {
					found = 1;
					kwstr = rcs_expkw[j].kw_str;
					break;
				}
			}

			/* unknown keyword, continue looking */
			if (found == 0) {
				c = start;
				continue;
			}

			c += strlen(kwstr);
			if (*c != ':' && *c != '$') {
				c = start;
				continue;
			}

			if (*c == ':') {
				while (*c++) {
					if (*c == '$') {
						end = c - start + 2;
						if (end >= sizeof(buf))
							fatal("keyword buffer"
							    " too small!");
						strlcpy(buf, start, end);
						checkin_parsekeyword(buf, rev,
						    date, author, state);
						break;
					}
				}

				if (*c != '$') {
					c = start;
					continue;
				}
			}
		}
	}
	if (found == 0)
		return (-1);
	else
		return (0);
}

/*
 * checkin_keywordtype()
 *
 * Given an RCS keyword string, determine what type of string it is.
 * This enables us to know what data should be in it.
 *
 * Returns type on success, or -1 on failure.
 */
static int
checkin_keywordtype(char *keystring)
{
	char *p;

	p = keystring;
	*p++;
	if (strncmp(p, KW_ID, strlen(KW_ID)) == 0)
		return (KW_TYPE_ID);
	else if (strncmp(p, KW_AUTHOR, strlen(KW_AUTHOR)) == 0)
		return (KW_TYPE_AUTHOR);
	else if (strncmp(p, KW_DATE, strlen(KW_DATE)) == 0)
		return (KW_TYPE_DATE);
	else if (strncmp(p, KW_STATE, strlen(KW_STATE)) == 0)
		return (KW_TYPE_STATE);
	else if (strncmp(p, KW_REVISION, strlen(KW_REVISION)) == 0)
		return (KW_TYPE_REVISION);
	else
		return (-1);
}

/*
 * checkin_parsekeyword()
 *
 * Do the actual parsing of an RCS keyword string, setting the values passed
 * to the function to whatever is found.
 *
 */
static void
checkin_parsekeyword(char *keystring,  RCSNUM **rev, time_t *date,
    char **author, char **state)
{
	char *tokens[10], *p, *datestring;
	size_t len = 0;
	int i = 0;

	/* Parse data out of the expanded keyword */
	switch (checkin_keywordtype(keystring)) {
	case KW_TYPE_ID:
		for ((p = strtok(keystring, " ")); p;
		    (p = strtok(NULL, " "))) {
			if (i < KW_NUMTOKS_ID - 1)
				tokens[i++] = p;
		}
		tokens[i] = NULL;
		if (*author != NULL)
			xfree(*author);
		if (*state != NULL)
			xfree(*state);
		/* only parse revision if one is not already set */
		if (*rev == NULL) {
			if ((*rev = rcsnum_parse(tokens[2])) == NULL)
				fatal("could not parse rcsnum");
		}
		len = strlen(tokens[5]) + 1;
		*author = xmalloc(len);
		strlcpy(*author, tokens[5], len);
		len = strlen(tokens[6]) + 1;
		*state = xmalloc(len);
		strlcpy(*state, tokens[6], len);
		len = strlen(tokens[3]) + strlen(tokens[4]) + 2;
		datestring = xmalloc(len);
		strlcpy(datestring, tokens[3], len);
		strlcat(datestring, " ", len);
		strlcat(datestring, tokens[4], len);
		if ((*date = cvs_date_parse(datestring)) <= 0)
		    fatal("could not parse date\n");
		xfree(datestring);
		break;
	case KW_TYPE_AUTHOR:
		for ((p = strtok(keystring, " ")); p;
		    (p = strtok(NULL, " "))) {
			if (i < KW_NUMTOKS_AUTHOR - 1)
				tokens[i++] = p;
		}
		if (*author != NULL)
			xfree(*author);
		len = strlen(tokens[1]) + 1;
		*author = xmalloc(len);
		strlcpy(*author, tokens[1], len);
		break;
	case KW_TYPE_DATE:
		for ((p = strtok(keystring, " ")); p;
		    (p = strtok(NULL, " "))) {
			if (i < KW_NUMTOKS_DATE - 1)
				tokens[i++] = p;
		}
		len = strlen(tokens[1]) + strlen(tokens[2]) + 2;
		datestring = xmalloc(len);
		strlcpy(datestring, tokens[1], len);
		strlcat(datestring, " ", len);
		strlcat(datestring, tokens[2], len);
		if ((*date = cvs_date_parse(datestring)) <= 0)
		    fatal("could not parse date\n");
		xfree(datestring);
		break;
	case KW_TYPE_STATE:
		for ((p = strtok(keystring, " ")); p;
		    (p = strtok(NULL, " "))) {
			if (i < KW_NUMTOKS_STATE - 1)
				tokens[i++] = p;
		}
		if (*state != NULL)
			xfree(*state);
		len = strlen(tokens[1]) + 1;
		*state = xmalloc(len);
		strlcpy(*state, tokens[1], len);
		break;
	case KW_TYPE_REVISION:
		/* only parse revision if one is not already set */
		if (*rev != NULL)
			break;
		for ((p = strtok(keystring, " ")); p;
		    (p = strtok(NULL, " "))) {
			if (i < KW_NUMTOKS_REVISION - 1)
				tokens[i++] = p;
		}
		if ((*rev = rcsnum_parse(tokens[1])) == NULL)
			fatal("could not parse rcsnum");
		break;
	}
}
