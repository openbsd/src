/*	$OpenBSD: ci.c,v 1.212 2011/04/20 19:34:16 nicm Exp $	*/
/*
 * Copyright (c) 2005, 2006 Niall O'Higgins <niallo@openbsd.org>
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

#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rcsprog.h"
#include "diff.h"

#define CI_OPTSTRING	"d::f::I::i::j::k::l::M::m::N:n:qr::s:Tt::u::Vw:x::z::"
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

/* Maximum number of tokens in a keyword. */
#define KW_NUMTOKS_MAX		10

#define RCSNUM_ZERO_ENDING(x) (x->rn_id[x->rn_len - 1] == 0)

extern struct rcs_kw rcs_expkw[];

static int workfile_fd;

struct checkin_params {
	int flags, openflags;
	mode_t fmode;
	time_t date;
	RCSFILE *file;
	RCSNUM *frev, *newrev;
	const char *description, *symbol;
	char fpath[MAXPATHLEN], *rcs_msg, *username, *filename;
	char *author, *state;
	BUF *deltatext;
};

static int	 checkin_attach_symbol(struct checkin_params *);
static int	 checkin_checklock(struct checkin_params *);
static BUF	*checkin_diff_file(struct checkin_params *);
static char	*checkin_getlogmsg(RCSNUM *, RCSNUM *, int);
static int	 checkin_init(struct checkin_params *);
static int	 checkin_keywordscan(BUF *, RCSNUM **, time_t *, char **,
    char **);
static int	 checkin_keywordtype(char *);
static void	 checkin_mtimedate(struct checkin_params *);
static void	 checkin_parsekeyword(char *, RCSNUM **, time_t *, char **,
    char **);
static int	 checkin_update(struct checkin_params *);
static int	 checkin_revert(struct checkin_params *);

void
checkin_usage(void)
{
	fprintf(stderr,
	    "usage: ci [-qV] [-d[date]] [-f[rev]] [-I[rev]] [-i[rev]]\n"
	    "          [-j[rev]] [-k[rev]] [-l[rev]] [-M[rev]] [-mmsg]\n"
	    "          [-Nsymbol] [-nsymbol] [-r[rev]] [-sstate] [-tstr]\n"
	    "          [-u[rev]] [-wusername] [-xsuffixes] [-ztz] file ...\n");
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
	int fd;
	int i, ch, status;
	int base_flags, base_openflags;
	char *rev_str;
	struct checkin_params pb;

	pb.date = DATE_NOW;
	pb.file = NULL;
	pb.rcs_msg = pb.username = pb.author = pb.state = NULL;
	pb.description = pb.symbol = NULL;
	pb.deltatext = NULL;
	pb.newrev =  NULL;
	pb.fmode = S_IRUSR|S_IRGRP|S_IROTH;
	status = 0;
	base_flags = INTERACTIVE;
	base_openflags = RCS_RDWR|RCS_CREATE|RCS_PARSE_FULLY;
	rev_str = NULL;

	while ((ch = rcs_getopt(argc, argv, CI_OPTSTRING)) != -1) {
		switch (ch) {
		case 'd':
			if (rcs_optarg == NULL)
				pb.date = DATE_MTIME;
			else if ((pb.date = date_parse(rcs_optarg)) == -1)
				errx(1, "invalid date");
			break;
		case 'f':
			rcs_setrevstr(&rev_str, rcs_optarg);
			base_flags |= FORCE;
			break;
		case 'I':
			rcs_setrevstr(&rev_str, rcs_optarg);
			base_flags |= INTERACTIVE;
			break;
		case 'i':
			rcs_setrevstr(&rev_str, rcs_optarg);
			base_openflags |= RCS_CREATE;
			base_flags |= CI_INIT;
			break;
		case 'j':
			rcs_setrevstr(&rev_str, rcs_optarg);
			base_openflags &= ~RCS_CREATE;
			base_flags &= ~CI_INIT;
			break;
		case 'k':
			rcs_setrevstr(&rev_str, rcs_optarg);
			base_flags |= CI_KEYWORDSCAN;
			break;
		case 'l':
			rcs_setrevstr(&rev_str, rcs_optarg);
			base_flags |= CO_LOCK;
			break;
		case 'M':
			rcs_setrevstr(&rev_str, rcs_optarg);
			base_flags |= CO_REVDATE;
			break;
		case 'm':
			pb.rcs_msg = rcs_optarg;
			if (pb.rcs_msg == NULL)
				errx(1, "missing message for -m option");
			base_flags &= ~INTERACTIVE;
			break;
		case 'N':
			base_flags |= CI_SYMFORCE;
			/* FALLTHROUGH */
		case 'n':
			pb.symbol = rcs_optarg;
			if (rcs_sym_check(pb.symbol) != 1)
				errx(1, "invalid symbol `%s'", pb.symbol);
			break;
		case 'q':
			base_flags |= QUIET;
			break;
		case 'r':
			rcs_setrevstr(&rev_str, rcs_optarg);
			base_flags |= CI_DEFAULT;
			break;
		case 's':
			pb.state = rcs_optarg;
			if (rcs_state_check(pb.state) < 0)
				errx(1, "invalid state `%s'", pb.state);
			break;
		case 'T':
			base_flags |= PRESERVETIME;
			break;
		case 't':
			/* Ignore bare -t; kept for backwards compatibility. */
			if (rcs_optarg == NULL)
				break;
			pb.description = rcs_optarg;
			base_flags |= DESCRIPTION;
			break;
		case 'u':
			rcs_setrevstr(&rev_str, rcs_optarg);
			base_flags |= CO_UNLOCK;
			break;
		case 'V':
			printf("%s\n", rcs_version);
			exit(0);
		case 'w':
			if (pb.author != NULL)
				xfree(pb.author);
			pb.author = xstrdup(rcs_optarg);
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
		warnx("no input file");
		(usage)();
		exit(1);
	}

	if ((pb.username = getlogin()) == NULL)
		err(1, "getlogin");

	for (i = 0; i < argc; i++) {
		/*
		 * The pb.flags and pb.openflags may change during
		 * loop iteration so restore them for each file.
		 */
		pb.flags = base_flags;
		pb.openflags = base_openflags;

		pb.filename = argv[i];
		rcs_strip_suffix(pb.filename);

		if ((workfile_fd = open(pb.filename, O_RDONLY)) == -1)
			err(1, "%s", pb.filename);

		/* Find RCS file path. */
		fd = rcs_choosefile(pb.filename, pb.fpath, sizeof(pb.fpath));

		if (fd < 0) {
			if (pb.openflags & RCS_CREATE)
				pb.flags |= NEWFILE;
			else {
				/* XXX - Check if errno == ENOENT. */
				warnx("No existing RCS file");
				status = 1;
				(void)close(workfile_fd);
				continue;
			}
		} else {
			if (pb.flags & CI_INIT) {
				warnx("%s already exists", pb.fpath);
				status = 1;
				(void)close(fd);
				(void)close(workfile_fd);
				continue;
			}
			pb.openflags &= ~RCS_CREATE;
		}

		pb.file = rcs_open(pb.fpath, fd, pb.openflags, pb.fmode);
		if (pb.file == NULL)
			errx(1, "failed to open rcsfile `%s'", pb.fpath);

		if ((pb.flags & DESCRIPTION) &&
		    rcs_set_description(pb.file, pb.description) == -1)
			err(1, "%s", pb.filename);

		if (!(pb.flags & QUIET))
			(void)fprintf(stderr,
			    "%s  <--  %s\n", pb.fpath, pb.filename);

		/* XXX - Should we rcsnum_free(pb.newrev)? */
		if (rev_str != NULL)
			if ((pb.newrev = rcs_getrevnum(rev_str, pb.file)) ==
			    NULL)
				errx(1, "invalid revision: %s", rev_str);

		if (!(pb.flags & NEWFILE))
			pb.flags |= CI_SKIPDESC;

		/* XXX - support for committing to a file without revisions */
		if (pb.file->rf_ndelta == 0) {
			pb.flags |= NEWFILE;
			pb.file->rf_flags |= RCS_CREATE;
		}

		/*
		 * workfile_fd will be closed in checkin_init or
		 * checkin_update
		 */
		if (pb.flags & NEWFILE) {
			if (checkin_init(&pb) == -1)
				status = 1;
		} else {
			if (checkin_update(&pb) == -1)
				status = 1;
		}

		rcs_close(pb.file);
		pb.newrev = NULL;
	}

	if (!(base_flags & QUIET) && status == 0)
		(void)fprintf(stderr, "done\n");

	return (status);
}

/*
 * checkin_diff_file()
 *
 * Generate the diff between the working file and a revision.
 * Returns pointer to a BUF on success, NULL on failure.
 */
static BUF *
checkin_diff_file(struct checkin_params *pb)
{
	char *path1, *path2;
	BUF *b1, *b2, *b3;

	b1 = b2 = b3 = NULL;
	path1 = path2 = NULL;

	if ((b1 = buf_load(pb->filename)) == NULL) {
		warnx("failed to load file: `%s'", pb->filename);
		goto out;
	}

	if ((b2 = rcs_getrev(pb->file, pb->frev)) == NULL) {
		warnx("failed to load revision");
		goto out;
	}
	b2 = rcs_kwexp_buf(b2, pb->file, pb->frev);
	b3 = buf_alloc(128);

	(void)xasprintf(&path1, "%s/diff1.XXXXXXXXXX", rcs_tmpdir);
	buf_write_stmp(b1, path1);

	buf_free(b1);
	b1 = NULL;

	(void)xasprintf(&path2, "%s/diff2.XXXXXXXXXX", rcs_tmpdir);
	buf_write_stmp(b2, path2);

	buf_free(b2);
	b2 = NULL;

	diff_format = D_RCSDIFF;
	if (diffreg(path1, path2, b3, D_FORCEASCII) == D_ERROR)
		goto out;

	return (b3);
out:
	if (b1 != NULL)
		buf_free(b1);
	if (b2 != NULL)
		buf_free(b2);
	if (b3 != NULL)
		buf_free(b3);
	if (path1 != NULL)
		xfree(path1);
	if (path2 != NULL)
		xfree(path2);

	return (NULL);
}

/*
 * checkin_getlogmsg()
 *
 * Get log message from user interactively.
 * Returns pointer to a char array on success, NULL on failure.
 */
static char *
checkin_getlogmsg(RCSNUM *rev, RCSNUM *rev2, int flags)
{
	char   *rcs_msg, nrev[RCS_REV_BUFSZ], prev[RCS_REV_BUFSZ];
	const char *prompt =
	    "enter log message, terminated with a single '.' or end of file:\n";
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

	if (!(flags & QUIET))
		(void)fprintf(stderr, "new revision: %s; "
		    "previous revision: %s\n", nrev, prev);

	rcs_msg = rcs_prompt(prompt);

	return (rcs_msg);
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
	char numb1[RCS_REV_BUFSZ], numb2[RCS_REV_BUFSZ];
	struct stat st;
	BUF *bp;

	/*
	 * XXX this is wrong, we need to get the revision the user
	 * has the lock for. So we can decide if we want to create a
	 * branch or not. (if it's not current HEAD we need to branch).
	 */
	pb->frev = pb->file->rf_head;

	/* Load file contents */
	if ((bp = buf_load(pb->filename)) == NULL)
		return (-1);

	/* If this is a zero-ending RCSNUM eg 4.0, increment it (eg to 4.1) */
	if (pb->newrev != NULL && RCSNUM_ZERO_ENDING(pb->newrev))
		pb->newrev = rcsnum_inc(pb->newrev);

	if (checkin_checklock(pb) < 0)
		return (-1);

	/* If revision passed on command line is less than HEAD, bail.
	 * XXX only applies to ci -r1.2 foo for example if HEAD is > 1.2 and
	 * there is no lock set for the user.
	 */
	if (pb->newrev != NULL &&
	    rcsnum_cmp(pb->newrev, pb->frev, 0) != -1) {
		warnx("%s: revision %s too low; must be higher than %s",
		    pb->file->rf_path,
		    rcsnum_tostr(pb->newrev, numb1, sizeof(numb1)),
		    rcsnum_tostr(pb->frev, numb2, sizeof(numb2)));
		return (-1);
	}

	/*
	 * Set the date of the revision to be the last modification
	 * time of the working file if -d has no argument.
	 */
	if (pb->date == DATE_MTIME)
		checkin_mtimedate(pb);

	/* Date from argv/mtime must be more recent than HEAD */
	if (pb->date != DATE_NOW) {
		time_t head_date = rcs_rev_getdate(pb->file, pb->frev);
		if (pb->date <= head_date) {
			char dbuf1[256], dbuf2[256], *fmt;
			struct tm *t, *t_head;

			fmt = "%Y/%m/%d %H:%M:%S";

			t = gmtime(&pb->date);
			strftime(dbuf1, sizeof(dbuf1), fmt, t);
			t_head = gmtime(&head_date);
			strftime(dbuf2, sizeof(dbuf2), fmt, t_head);

			errx(1, "%s: Date %s precedes %s in revision %s.",
			    pb->file->rf_path, dbuf1, dbuf2,
			    rcsnum_tostr(pb->frev, numb2, sizeof(numb2)));
		}
	}

	/* Get RCS patch */
	if ((pb->deltatext = checkin_diff_file(pb)) == NULL) {
		warnx("failed to get diff");
		return (-1);
	}

	/*
	 * If -f is not specified and there are no differences, tell
	 * the user and revert to latest version.
	 */
	if (!(pb->flags & FORCE) && (buf_len(pb->deltatext) < 1)) {
		if (checkin_revert(pb) == -1)
			return (-1);
		else
			return (0);
	}

	/* If no log message specified, get it interactively. */
	if (pb->flags & INTERACTIVE) {
		if (pb->rcs_msg != NULL) {
			fprintf(stderr,
			    "reuse log message of previous file? [yn](y): ");
			if (rcs_yesno('y') != 'y') {
				xfree(pb->rcs_msg);
				pb->rcs_msg = NULL;
			}
		}
		if (pb->rcs_msg == NULL)
			pb->rcs_msg = checkin_getlogmsg(pb->frev, pb->newrev,
			    pb->flags);
	}

	if ((rcs_lock_remove(pb->file, pb->username, pb->frev) < 0) &&
	    (rcs_lock_getmode(pb->file) != RCS_LOCK_LOOSE)) {
		if (rcs_errno != RCS_ERR_NOENT)
			warnx("failed to remove lock");
		else if (!(pb->flags & CO_LOCK))
			warnx("previous revision was not locked; "
			    "ignoring -l option");
	}

	/* Current head revision gets the RCS patch as rd_text */
	if (rcs_deltatext_set(pb->file, pb->frev, pb->deltatext) == -1)
		errx(1, "failed to set new rd_text for head rev");

	/* Now add our new revision */
	if (rcs_rev_add(pb->file,
	    (pb->newrev == NULL ? RCS_HEAD_REV : pb->newrev),
	    pb->rcs_msg, pb->date, pb->author) != 0) {
		warnx("failed to add new revision");
		return (-1);
	}

	/*
	 * If we are checking in to a non-default (ie user-specified)
	 * revision, set head to this revision.
	 */
	if (pb->newrev != NULL) {
		if (rcs_head_set(pb->file, pb->newrev) < 0)
			errx(1, "rcs_head_set failed");
	} else
		pb->newrev = pb->file->rf_head;

	/* New head revision has to contain entire file; */
	if (rcs_deltatext_set(pb->file, pb->frev, bp) == -1)
		errx(1, "failed to set new head revision");

	/* Attach a symbolic name to this revision if specified. */
	if (pb->symbol != NULL &&
	    (checkin_attach_symbol(pb) < 0))
		return (-1);

	/* Set the state of this revision if specified. */
	if (pb->state != NULL)
		(void)rcs_state_set(pb->file, pb->newrev, pb->state);

	/* Maintain RCSFILE permissions */
	if (fstat(workfile_fd, &st) == -1)
		err(1, "%s", pb->filename);

	/* Strip all the write bits */
	pb->file->rf_mode = st.st_mode & ~(S_IWUSR|S_IWGRP|S_IWOTH);

	(void)close(workfile_fd);
	(void)unlink(pb->filename);

	/* Write out RCSFILE before calling checkout_rev() */
	rcs_write(pb->file);

	/* Do checkout if -u or -l are specified. */
	if (((pb->flags & CO_LOCK) || (pb->flags & CO_UNLOCK)) &&
	    !(pb->flags & CI_DEFAULT))
		checkout_rev(pb->file, pb->newrev, pb->filename, pb->flags,
		    pb->username, pb->author, NULL, NULL);

	if ((pb->flags & INTERACTIVE) && (pb->rcs_msg[0] == '\0')) {
		xfree(pb->rcs_msg);	/* free empty log message */
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
	BUF *bp;
	char numb[RCS_REV_BUFSZ];
	int fetchlog = 0;
	struct stat st;

	/* If this is a zero-ending RCSNUM eg 4.0, increment it (eg to 4.1) */
	if (pb->newrev != NULL && RCSNUM_ZERO_ENDING(pb->newrev)) {
		pb->frev = rcsnum_alloc();
		rcsnum_cpy(pb->newrev, pb->frev, 0);
		pb->newrev = rcsnum_inc(pb->newrev);
		fetchlog = 1;
	}

	/* Load file contents */
	if ((bp = buf_load(pb->filename)) == NULL)
		return (-1);

	/* Get default values from working copy if -k specified */
	if (pb->flags & CI_KEYWORDSCAN)
		checkin_keywordscan(bp, &pb->newrev,
		    &pb->date, &pb->state, &pb->author);

	if (pb->flags & CI_SKIPDESC)
		goto skipdesc;

	/* Get description from user */
	if (pb->description == NULL &&
	    rcs_set_description(pb->file, NULL) == -1) {
		warn("%s", pb->filename);
		return (-1);
	}

skipdesc:

	/*
	 * If the user had specified a zero-ending revision number e.g. 4.0
	 * emulate odd GNU behaviour and fetch log message.
	 */
	if (fetchlog == 1) {
		pb->rcs_msg = checkin_getlogmsg(pb->frev, pb->newrev,
		    pb->flags);
		rcsnum_free(pb->frev);
	}

	/*
	 * Set the date of the revision to be the last modification
	 * time of the working file if -d has no argument.
	 */
	if (pb->date == DATE_MTIME)
		checkin_mtimedate(pb);

	/* Now add our new revision */
	if (rcs_rev_add(pb->file,
	    (pb->newrev == NULL ? RCS_HEAD_REV : pb->newrev),
	    (pb->rcs_msg == NULL ? "Initial revision" : pb->rcs_msg),
	    pb->date, pb->author) != 0) {
		warnx("failed to add new revision");
		return (-1);
	}

	/*
	 * If we are checking in to a non-default (ie user-specified)
	 * revision, set head to this revision.
	 */
	if (pb->newrev != NULL) {
		if (rcs_head_set(pb->file, pb->newrev) < 0)
			errx(1, "rcs_head_set failed");
	} else
		pb->newrev = pb->file->rf_head;

	/* New head revision has to contain entire file; */
	if (rcs_deltatext_set(pb->file, pb->file->rf_head, bp) == -1) {
		warnx("failed to set new head revision");
		return (-1);
	}

	/* Attach a symbolic name to this revision if specified. */
	if (pb->symbol != NULL && checkin_attach_symbol(pb) < 0)
		return (-1);

	/* Set the state of this revision if specified. */
	if (pb->state != NULL)
		(void)rcs_state_set(pb->file, pb->newrev, pb->state);

	/* Inherit RCSFILE permissions from file being checked in */
	if (fstat(workfile_fd, &st) == -1)
		err(1, "%s", pb->filename);

	/* Strip all the write bits */
	pb->file->rf_mode = st.st_mode & ~(S_IWUSR|S_IWGRP|S_IWOTH);

	(void)close(workfile_fd);
	(void)unlink(pb->filename);

	/* Write out RCSFILE before calling checkout_rev() */
	rcs_write(pb->file);

	/* Do checkout if -u or -l are specified. */
	if (((pb->flags & CO_LOCK) || (pb->flags & CO_UNLOCK)) &&
	    !(pb->flags & CI_DEFAULT)) {
		checkout_rev(pb->file, pb->newrev, pb->filename, pb->flags,
		    pb->username, pb->author, NULL, NULL);
	}

	if (!(pb->flags & QUIET)) {
		fprintf(stderr, "initial revision: %s\n",
		    rcsnum_tostr(pb->newrev, numb, sizeof(numb)));
	}

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
	char rbuf[RCS_REV_BUFSZ];
	int ret;
	if (!(pb->flags & QUIET))
		printf("symbol: %s\n", pb->symbol);
	if (pb->flags & CI_SYMFORCE) {
		if (rcs_sym_remove(pb->file, pb->symbol) < 0) {
			if (rcs_errno != RCS_ERR_NOENT) {
				warnx("problem removing symbol: %s",
				    pb->symbol);
				return (-1);
			}
		}
	}
	if ((ret = rcs_sym_add(pb->file, pb->symbol, pb->newrev) == -1) &&
	    (rcs_errno == RCS_ERR_DUPENT)) {
		rcsnum_tostr(rcs_sym_getrev(pb->file, pb->symbol),
		    rbuf, sizeof(rbuf));
		warnx("symbolic name %s already bound to %s", pb->symbol, rbuf);
		return (-1);
	} else if (ret == -1) {
		warnx("problem adding symbol: %s", pb->symbol);
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
static int
checkin_revert(struct checkin_params *pb)
{
	char rbuf[RCS_REV_BUFSZ];

	rcsnum_tostr(pb->frev, rbuf, sizeof(rbuf));

	if (!(pb->flags & QUIET))
		(void)fprintf(stderr, "file is unchanged; reverting "
		    "to previous revision %s\n", rbuf);

	/* Attach a symbolic name to this revision if specified. */
	if (pb->symbol != NULL) {
		if (checkin_checklock(pb) == -1)
			return (-1);

		pb->newrev = pb->frev;
		if (checkin_attach_symbol(pb) == -1)
			return (-1);
	}

	pb->flags |= CO_REVERT;
	(void)close(workfile_fd);
	(void)unlink(pb->filename);
	
	/* If needed, write out RCSFILE before calling checkout_rev() */
	if (pb->symbol != NULL)
		rcs_write(pb->file);

	if ((pb->flags & CO_LOCK) || (pb->flags & CO_UNLOCK))
		checkout_rev(pb->file, pb->frev, pb->filename,
		    pb->flags, pb->username, pb->author, NULL, NULL);

	return (0);
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

	if (rcs_lock_getmode(pb->file) == RCS_LOCK_LOOSE)
		return (0);

	TAILQ_FOREACH(lkp, &(pb->file->rf_locks), rl_list) {
		if (!strcmp(lkp->rl_name, pb->username) &&
		    !rcsnum_cmp(lkp->rl_num, pb->frev, 0))
			return (0);
	}

	warnx("%s: no lock set by %s", pb->file->rf_path, pb->username);
	return (-1);
}

/*
 * checkin_mtimedate()
 *
 * Set the date of the revision to be the last modification
 * time of the working file.
 */
static void
checkin_mtimedate(struct checkin_params *pb)
{
	struct stat sb;

	if (fstat(workfile_fd, &sb) == -1)
		err(1, "%s", pb->filename);

	pb->date = (time_t)sb.st_mtimespec.tv_sec;
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
checkin_keywordscan(BUF *data, RCSNUM **rev, time_t *date, char **author,
    char **state)
{
	BUF *buf;
	size_t left;
	u_int j;
	char *kwstr;
	unsigned char *c, *end, *start;

	end = buf_get(data) + buf_len(data) - 1;
	kwstr = NULL;

	left = buf_len(data);
	for (c = buf_get(data);
	    c <= end && (c = memchr(c, '$', left)) != NULL;
	    left = end - c + 1) {
		size_t len;

		start = c;
		c++;
		if (!isalpha(*c))
			continue;

		/* look for any matching keywords */
		for (j = 0; j < 10; j++) {
			len = strlen(rcs_expkw[j].kw_str);
			if (left < len)
				continue;
			if (memcmp(c, rcs_expkw[j].kw_str, len) != 0) {
				kwstr = rcs_expkw[j].kw_str;
				break;
			}
		}

		/* unknown keyword, continue looking */
		if (kwstr == NULL)
			continue;

		c += len;
		if (c > end) {
			kwstr = NULL;
			break;
		}
		if (*c != ':') {
			kwstr = NULL;
			continue;
		}

		/* Find end of line or end of keyword. */
		while (++c <= end) {
			if (*c == '\n') {
				/* Skip newline since it is definitely not `$'. */
				++c;
				goto loopend;
			}
			if (*c == '$')
				break;
		}

		len = c - start + 1;
		buf = buf_alloc(len + 1);
		buf_append(buf, start, len);

		/* XXX - Not binary safe. */
		buf_putc(buf, '\0');
		checkin_parsekeyword(buf_get(buf), rev, date, author, state);
		buf_free(buf);
loopend:;
	}
	if (kwstr == NULL)
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
	p++;
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
 * XXX - Don't error out on malformed keywords.
 */
static void
checkin_parsekeyword(char *keystring, RCSNUM **rev, time_t *date,
    char **author, char **state)
{
	char *tokens[KW_NUMTOKS_MAX], *p, *datestring;
	int i = 0;

	for ((p = strtok(keystring, " ")); p; (p = strtok(NULL, " "))) {
		if (i < KW_NUMTOKS_MAX - 1)
			tokens[i++] = p;
		else
			break;
	}

	/* Parse data out of the expanded keyword */
	switch (checkin_keywordtype(keystring)) {
	case KW_TYPE_ID:
		if (i < 3)
			break;
		/* only parse revision if one is not already set */
		if (*rev == NULL) {
			if ((*rev = rcsnum_parse(tokens[2])) == NULL)
				errx(1, "could not parse rcsnum");
		}

		if (i < 5)
			break;
		(void)xasprintf(&datestring, "%s %s", tokens[3], tokens[4]);
		if ((*date = date_parse(datestring)) == -1)
			errx(1, "could not parse date");
		xfree(datestring);

		if (i < 6)
			break;
		if (*author != NULL)
			xfree(*author);
		*author = xstrdup(tokens[5]);

		if (i < 7)
			break;
		if (*state != NULL)
			xfree(*state);
		*state = xstrdup(tokens[6]);
		break;
	case KW_TYPE_AUTHOR:
		if (i < 2)
			break;
		if (*author != NULL)
			xfree(*author);
		*author = xstrdup(tokens[1]);
		break;
	case KW_TYPE_DATE:
		if (i < 3)
			break;
		(void)xasprintf(&datestring, "%s %s", tokens[1], tokens[2]);
		if ((*date = date_parse(datestring)) == -1)
			errx(1, "could not parse date");
		xfree(datestring);
		break;
	case KW_TYPE_STATE:
		if (i < 2)
			break;
		if (*state != NULL)
			xfree(*state);
		*state = xstrdup(tokens[1]);
		break;
	case KW_TYPE_REVISION:
		if (i < 2)
			break;
		/* only parse revision if one is not already set */
		if (*rev != NULL)
			break;
		if ((*rev = rcsnum_parse(tokens[1])) == NULL)
			errx(1, "could not parse rcsnum");
		break;
	}
}
