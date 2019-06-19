/*	$OpenBSD: function.c,v 1.46 2018/09/16 02:44:06 millert Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Cimarron D. Taylor of the University of California, Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <fts.h>
#include <grp.h>
#include <libgen.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "find.h"
#include "extern.h"

#define	COMPARE(a, b) {							\
	switch (plan->flags) {						\
	case F_EQUAL:							\
		return (a == b);					\
	case F_LESSTHAN:						\
		return (a < b);						\
	case F_GREATER:							\
		return (a > b);						\
	default:							\
		abort();						\
	}								\
}

static PLAN *palloc(enum ntype, int (*)(PLAN *, FTSENT *));
static long long find_parsenum(PLAN *plan, char *option, char *vp, char *endch);
static void run_f_exec(PLAN *plan);
static PLAN *palloc(enum ntype t, int (*f)(PLAN *, FTSENT *));

int	f_amin(PLAN *, FTSENT *);
int	f_atime(PLAN *, FTSENT *);
int	f_cmin(PLAN *, FTSENT *);
int	f_ctime(PLAN *, FTSENT *);
int	f_always_true(PLAN *, FTSENT *);
int	f_empty(PLAN *, FTSENT *);
int	f_exec(PLAN *, FTSENT *);
int	f_execdir(PLAN *, FTSENT *);
int	f_flags(PLAN *, FTSENT *);
int	f_fstype(PLAN *, FTSENT *);
int	f_group(PLAN *, FTSENT *);
int	f_inum(PLAN *, FTSENT *);
int	f_empty(PLAN *, FTSENT *);
int	f_links(PLAN *, FTSENT *);
int	f_ls(PLAN *, FTSENT *);
int	f_maxdepth(PLAN *, FTSENT *);
int	f_mindepth(PLAN *, FTSENT *);
int	f_mtime(PLAN *, FTSENT *);
int	f_mmin(PLAN *, FTSENT *);
int	f_name(PLAN *, FTSENT *);
int	f_iname(PLAN *, FTSENT *);
int	f_newer(PLAN *, FTSENT *);
int	f_anewer(PLAN *, FTSENT *);
int	f_cnewer(PLAN *, FTSENT *);
int	f_nogroup(PLAN *, FTSENT *);
int	f_nouser(PLAN *, FTSENT *);
int	f_path(PLAN *, FTSENT *);
int	f_perm(PLAN *, FTSENT *);
int	f_print(PLAN *, FTSENT *);
int	f_print0(PLAN *, FTSENT *);
int	f_prune(PLAN *, FTSENT *);
int	f_size(PLAN *, FTSENT *);
int	f_type(PLAN *, FTSENT *);
int	f_user(PLAN *, FTSENT *);
int	f_expr(PLAN *, FTSENT *);
int	f_not(PLAN *, FTSENT *);
int	f_or(PLAN *, FTSENT *);

extern int dotfd;
extern time_t now;
extern FTS *tree;

/*
 * find_parsenum --
 *	Parse a string of the form [+-]# and return the value.
 */
static long long
find_parsenum(PLAN *plan, char *option, char *vp, char *endch)
{
	long long value;
	char *endchar, *str;	/* Pointer to character ending conversion. */
    
	/* Determine comparison from leading + or -. */
	str = vp;
	switch (*str) {
	case '+':
		++str;
		plan->flags = F_GREATER;
		break;
	case '-':
		++str;
		plan->flags = F_LESSTHAN;
		break;
	default:
		plan->flags = F_EQUAL;
		break;
	}
    
	/*
	 * Convert the string with strtoll().  Note, if strtoll() returns
	 * zero and endchar points to the beginning of the string we know
	 * we have a syntax error.
	 */
	value = strtoll(str, &endchar, 10);
	if (value == 0 && endchar == str)
		errx(1, "%s: %s: illegal numeric value", option, vp);
	if (endchar[0] && (endch == NULL || endchar[0] != *endch))
		errx(1, "%s: %s: illegal trailing character", option, vp);
	if (endch)
		*endch = endchar[0];
	return (value);
}

/*
 * The value of n for the inode times (atime, ctime, and mtime) is a range,
 * i.e. n matches from (n - 1) to n 24 hour periods.  This interacts with
 * -n, such that "-mtime -1" would be less than 0 days, which isn't what the
 * user wanted.  Correct so that -1 is "less than 1".
 */
#define	TIME_CORRECT(p, ttype)						\
	if ((p)->type == ttype && (p)->flags == F_LESSTHAN)		\
		++((p)->sec_data);

/*
 * -amin n functions --
 *
 *     True if the difference between the file access time and the
 *     current time is n min periods.
 */
int
f_amin(PLAN *plan, FTSENT *entry)
{
	extern time_t now;

	COMPARE((now - entry->fts_statp->st_atime +
	    60 - 1) / 60, plan->sec_data);
}

PLAN *
c_amin(char *arg, char ***ignored, int unused)
{
	PLAN *new;

	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_AMIN, f_amin);
	new->sec_data = find_parsenum(new, "-amin", arg, NULL);
	TIME_CORRECT(new, N_AMIN);
	return (new);
}

/*
 * -atime n functions --
 *
 *	True if the difference between the file access time and the
 *	current time is n 24 hour periods.
 */
int
f_atime(PLAN *plan, FTSENT *entry)
{

	COMPARE((now - entry->fts_statp->st_atime +
	    SECSPERDAY - 1) / SECSPERDAY, plan->sec_data);
}
 
PLAN *
c_atime(char *arg, char ***ignored, int unused)
{
	PLAN *new;

	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_ATIME, f_atime);
	new->sec_data = find_parsenum(new, "-atime", arg, NULL);
	TIME_CORRECT(new, N_ATIME);
	return (new);
}

/*
 * -cmin n functions --
 *
 *     True if the difference between the last change of file
 *     status information and the current time is n min periods.
 */
int
f_cmin(PLAN *plan, FTSENT *entry)
{
	extern time_t now;

	COMPARE((now - entry->fts_statp->st_ctime +
	    60 - 1) / 60, plan->sec_data);
}

PLAN *
c_cmin(char *arg, char ***ignored, int unused)
{
	PLAN *new;

	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_CMIN, f_cmin);
	new->sec_data = find_parsenum(new, "-cmin", arg, NULL);
	TIME_CORRECT(new, N_CMIN);
	return (new);
}

/*
 * -ctime n functions --
 *
 *	True if the difference between the last change of file
 *	status information and the current time is n 24 hour periods.
 */
int
f_ctime(PLAN *plan, FTSENT *entry)
{

	COMPARE((now - entry->fts_statp->st_ctime +
	    SECSPERDAY - 1) / SECSPERDAY, plan->sec_data);
}
 
PLAN *
c_ctime(char *arg, char ***ignored, int unused)
{
	PLAN *new;

	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_CTIME, f_ctime);
	new->sec_data = find_parsenum(new, "-ctime", arg, NULL);
	TIME_CORRECT(new, N_CTIME);
	return (new);
}

/*
 * -depth functions --
 *
 *	Always true, causes descent of the directory hierarchy to be done
 *	so that all entries in a directory are acted on before the directory
 *	itself.
 */
int
f_always_true(PLAN *plan, FTSENT *entry)
{
	return (1);
}
 
PLAN *
c_depth(char *ignore, char ***ignored, int unused)
{
	isdepth = 1;

	return (palloc(N_DEPTH, f_always_true));
}

/*
 * -delete functions
 */
int
f_delete(PLAN *plan, FTSENT *entry)
{

	/* can't delete these */
	if (strcmp(entry->fts_accpath, ".") == 0 ||
	    strcmp(entry->fts_accpath, "..") == 0)
		return 1;

	/* sanity check */
	if (isdepth == 0 ||                     /* depth off */
	    (ftsoptions & FTS_NOSTAT))          /* not stat()ing */
		errx(1, "-delete: insecure options got turned on");
	if (!(ftsoptions & FTS_PHYSICAL) ||     /* physical off */
	    (ftsoptions & FTS_LOGICAL))         /* or finally, logical on */
		errx(1, "-delete: forbidden when symlinks are followed");

	/* Potentially unsafe - do not accept relative paths whatsoever */
	if (entry->fts_level > FTS_ROOTLEVEL &&
	    strchr(entry->fts_accpath, '/') != NULL)
		errx(1, "-delete: %s: relative path potentially not safe",
		    entry->fts_accpath);
#if 0
	/* Turn off user immutable bits if running as root */
	if ((entry->fts_statp->st_flags & (UF_APPEND|UF_IMMUTABLE)) &&
	    !(entry->fts_statp->st_flags & (SF_APPEND|SF_IMMUTABLE)) &&
	    geteuid() == 0)
		lchflags(entry->fts_accpath,
		    entry->fts_statp->st_flags &= ~(UF_APPEND|UF_IMMUTABLE));
#endif
	/* rmdir directories, unlink everything else */
	if (S_ISDIR(entry->fts_statp->st_mode)) {
		if (rmdir(entry->fts_accpath) < 0 && errno != ENOTEMPTY)
			warn("-delete: rmdir(%s)", entry->fts_path);
	} else {
		if (unlink(entry->fts_accpath) < 0)
			warn("-delete: unlink(%s)", entry->fts_path);

	}

	return 1;
}

PLAN *
c_delete(char *ignore, char ***ignored, int unused)
{
	ftsoptions &= ~FTS_NOSTAT;
	isoutput = 1;
	isdelete = 1;
	isdepth = 1;

	return (palloc(N_DELETE, f_delete));
}
 
/*
 * -empty functions --
 *
 *	True if the file or directory is empty
 */
int
f_empty(PLAN *plan, FTSENT *entry)
{
	if (S_ISREG(entry->fts_statp->st_mode) && entry->fts_statp->st_size == 0)
		return (1);
	if (S_ISDIR(entry->fts_statp->st_mode)) {
		struct dirent *dp;
		int empty;
		DIR *dir;

		empty = 1;
		dir = opendir(entry->fts_accpath);
		if (dir == NULL)
			return (0);
		for (dp = readdir(dir); dp; dp = readdir(dir))
			if (dp->d_name[0] != '.' ||
			    (dp->d_name[1] != '\0' &&
			     (dp->d_name[1] != '.' || dp->d_name[2] != '\0'))) {
				empty = 0;
				break;
			}
		closedir(dir);
		return (empty);
	}
	return (0);
}

PLAN *
c_empty(char *ignore, char ***ignored, int unused)
{
	ftsoptions &= ~FTS_NOSTAT;

	return (palloc(N_EMPTY, f_empty));
}

/*
 * [-exec | -ok] utility [arg ... ] ; functions --
 * [-exec | -ok] utility [arg ... ] {} + functions --
 *
 *	If the end of the primary expression is delimited by a
 *	semicolon: true if the executed utility returns a zero value
 *	as exit status.  If "{}" occurs anywhere, it gets replaced by
 *	the current pathname.
 *
 *	If the end of the primary expression is delimited by a plus
 *	sign: always true. Pathnames for which the primary is
 *	evaluated shall be aggregated into sets. The utility will be
 *	executed once per set, with "{}" replaced by the entire set of
 *	pathnames (as if xargs). "{}" must appear last.
 *
 *	The current directory for the execution of utility is the same
 *	as the current directory when the find utility was started.
 *
 *	The primary -ok is different in that it requests affirmation
 *	of the user before executing the utility.
 */
int
f_exec(PLAN *plan, FTSENT *entry)
{
	int cnt, l;
	pid_t pid;
	int status;

	if (plan->flags & F_PLUSSET) {
		/*
		 * Confirm sufficient buffer space, then copy the path
		 * to the buffer.
		 */
		l = strlen(entry->fts_path);
		if (plan->ep_p + l < plan->ep_ebp) {
			plan->ep_bxp[plan->ep_narg++] = plan->ep_p;
			strlcpy(plan->ep_p, entry->fts_path, l + 1);
			plan->ep_p += l + 1;

			if (plan->ep_narg == plan->ep_maxargs)
				run_f_exec(plan);
		} else {
			/*
			 * Without sufficient space to copy in the next
			 * argument, run the command to empty out the
			 * buffer before re-attepting the copy.
			 */
			run_f_exec(plan);
			if (plan->ep_p + l < plan->ep_ebp) {
				plan->ep_bxp[plan->ep_narg++] = plan->ep_p;
				strlcpy(plan->ep_p, entry->fts_path, l + 1);
				plan->ep_p += l + 1;
			} else
				errx(1, "insufficient space for argument");
		}
		return (1);
	} else {
		for (cnt = 0; plan->e_argv[cnt]; ++cnt)
			if (plan->e_len[cnt])
				brace_subst(plan->e_orig[cnt],
				    &plan->e_argv[cnt],
				    entry->fts_path,
				    plan->e_len[cnt]);
		if (plan->flags & F_NEEDOK && !queryuser(plan->e_argv))
			return (0);

		/* don't mix output of command with find output */
		fflush(stdout);
		fflush(stderr);

		switch (pid = vfork()) {
		case -1:
			err(1, "fork");
			/* NOTREACHED */
		case 0:
			if (fchdir(dotfd)) {
				warn("chdir");
				_exit(1);
			}
			execvp(plan->e_argv[0], plan->e_argv);
			warn("%s", plan->e_argv[0]);
			_exit(1);
		}
		pid = waitpid(pid, &status, 0);
		return (pid != -1 && WIFEXITED(status) && !WEXITSTATUS(status));
	}
}

static void
run_f_exec(PLAN *plan)
{
	pid_t pid;
	int rval, status;

	/* Ensure arg list is null terminated. */
	plan->ep_bxp[plan->ep_narg] = NULL;

	/* Don't mix output of command with find output. */
 	fflush(stdout);
 	fflush(stderr);

	switch (pid = vfork()) {
	case -1:
		err(1, "vfork");
		/* NOTREACHED */
	case 0:
		if (fchdir(dotfd)) {
			warn("chdir");
			_exit(1);
		}
		execvp(plan->e_argv[0], plan->e_argv);
		warn("%s", plan->e_argv[0]);
		_exit(1);
	}

	/* Clear out the argument list. */
	plan->ep_narg = 0;
	plan->ep_bxp[plan->ep_narg] = NULL;
	/* As well as the argument buffer. */
	plan->ep_p = plan->ep_bbp;
	*plan->ep_p = '\0';

	pid = waitpid(pid, &status, 0);
	if (WIFEXITED(status))
		rval = WEXITSTATUS(status);
	else
		rval = -1;

	/*
	 * If we have a non-zero exit status, preserve it so find(1) can
	 * later exit with it.
	 */
	if (rval)
		plan->ep_rval = rval;
}
 
/*
 * c_exec --
 *	build three parallel arrays, one with pointers to the strings passed
 *	on the command line, one with (possibly duplicated) pointers to the
 *	argv array, and one with integer values that are lengths of the
 *	strings, but also flags meaning that the string has to be massaged.
 *
 *	If -exec ... {} +, use only the first array, but make it large
 *	enough to hold 5000 args (cf. src/usr.bin/xargs/xargs.c for a
 *	discussion), and then allocate ARG_MAX - 4K of space for args.
 */
PLAN *
c_exec(char *unused, char ***argvp, int isok)
{
	PLAN *new;			/* node returned */
	int cnt, brace, lastbrace;
	char **argv, **ap, *p;

	/* make sure the current directory is readable */
	if (dotfd == -1)
		errx(1, "%s: cannot open \".\"", isok ? "-ok" : "-exec");

	isoutput = 1;
    
	new = palloc(N_EXEC, f_exec);
	if (isok)
		new->flags |= F_NEEDOK;

	/*
	 * Terminate if we encounter an arg exactly equal to ";", or an
	 * arg exactly equal to "+" following an arg exactly equal to
	 * "{}".
	 */
	for (ap = argv = *argvp, brace = 0;; ++ap) {
		if (!*ap)
			errx(1, "%s: no terminating \";\" or \"+\"",
			    isok ? "-ok" : "-exec");
		lastbrace = brace;
		brace = 0;
		if (strcmp(*ap, "{}") == 0)
			brace = 1;
		if (strcmp(*ap, ";") == 0)
			break;
		if (strcmp(*ap, "+") == 0 && lastbrace) {
			new->flags |= F_PLUSSET;
			break;
		}
	}


	/*
	 * POSIX says -ok ... {} + "need not be supported," and it does
	 * not make much sense anyway.
	 */
	if (new->flags & F_NEEDOK && new->flags & F_PLUSSET)
		errx(1, "-ok: terminating \"+\" not permitted.");

	if (new->flags & F_PLUSSET) {
		u_int c, bufsize;

		cnt = ap - *argvp - 1;			/* units are words */
		new->ep_maxargs = 5000;
		new->e_argv = ereallocarray(NULL,
		    (size_t)(cnt + new->ep_maxargs), sizeof(char **));

		/* We start stuffing arguments after the user's last one. */
		new->ep_bxp = &new->e_argv[cnt];
		new->ep_narg = 0;

		/*
		 * Count up the space of the user's arguments, and
		 * subtract that from what we allocate.
		 */
		for (argv = *argvp, c = 0, cnt = 0;
		     argv < ap;
		     ++argv, ++cnt) {
			c += strlen(*argv) + 1;
 			new->e_argv[cnt] = *argv;
 		}
		bufsize = ARG_MAX - 4 * 1024 - c;


		/*
		 * Allocate, and then initialize current, base, and
		 * end pointers.
		 */
		new->ep_p = new->ep_bbp = malloc(bufsize + 1);
		new->ep_ebp = new->ep_bbp + bufsize - 1;
		new->ep_rval = 0;
	} else { /* !F_PLUSSET */
		cnt = ap - *argvp + 1;
		new->e_argv = ereallocarray(NULL, cnt, sizeof(char *));
		new->e_orig = ereallocarray(NULL, cnt, sizeof(char *));
		new->e_len = ereallocarray(NULL, cnt, sizeof(int));

		for (argv = *argvp, cnt = 0; argv < ap; ++argv, ++cnt) {
			new->e_orig[cnt] = *argv;
			for (p = *argv; *p; ++p)
				if (p[0] == '{' && p[1] == '}') {
					new->e_argv[cnt] =
						emalloc((u_int)PATH_MAX);
					new->e_len[cnt] = PATH_MAX;
					break;
				}
			if (!*p) {
				new->e_argv[cnt] = *argv;
				new->e_len[cnt] = 0;
			}
		}
		new->e_orig[cnt] = NULL;
 	}

	new->e_argv[cnt] = NULL;
	*argvp = argv + 1;
	return (new);
}
 
/*
 * -execdir utility [arg ... ] ; functions --
 *
 *	True if the executed utility returns a zero value as exit status.
 *	The end of the primary expression is delimited by a semicolon.  If
 *	"{}" occurs anywhere, it gets replaced by the unqualified pathname.
 *	The current directory for the execution of utility is the same as
 *	the directory where the file lives.
 */
int
f_execdir(PLAN *plan, FTSENT *entry)
{
	int cnt;
	pid_t pid;
	int status, fd;
	char base[PATH_MAX];

	/* fts(3) does not chdir for the root level so we do it ourselves. */
	if (entry->fts_level == FTS_ROOTLEVEL) {
		if ((fd = open(".", O_RDONLY)) == -1) {
			warn("cannot open \".\"");
			return (0);
		}
		if (chdir(entry->fts_accpath)) {
			(void) close(fd);
			return (0);
		}
	}

	/* Substitute basename(path) for {} since cwd is it's parent dir */
	(void)strncpy(base, basename(entry->fts_path), sizeof(base) - 1);
	base[sizeof(base) - 1] = '\0';
	for (cnt = 0; plan->e_argv[cnt]; ++cnt)
		if (plan->e_len[cnt])
			brace_subst(plan->e_orig[cnt], &plan->e_argv[cnt],
			    base, plan->e_len[cnt]);

	/* don't mix output of command with find output */
	fflush(stdout);
	fflush(stderr);

	switch (pid = vfork()) {
	case -1:
		err(1, "fork");
		/* NOTREACHED */
	case 0:
		execvp(plan->e_argv[0], plan->e_argv);
		warn("%s", plan->e_argv[0]);
		_exit(1);
	}

	/* Undo the above... */
	if (entry->fts_level == FTS_ROOTLEVEL) {
		if (fchdir(fd) == -1) {
			warn("unable to chdir back to starting directory");
			(void) close(fd);
			return (0);
		}
		(void) close(fd);
	}

	pid = waitpid(pid, &status, 0);
	return (pid != -1 && WIFEXITED(status) && !WEXITSTATUS(status));
}
 
/*
 * c_execdir --
 *	build three parallel arrays, one with pointers to the strings passed
 *	on the command line, one with (possibly duplicated) pointers to the
 *	argv array, and one with integer values that are lengths of the
 *	strings, but also flags meaning that the string has to be massaged.
 */
PLAN *
c_execdir(char *ignored, char ***argvp, int unused)
{
	PLAN *new;			/* node returned */
	int cnt;
	char **argv, **ap, *p;

	ftsoptions &= ~FTS_NOSTAT;
	isoutput = 1;
    
	new = palloc(N_EXECDIR, f_execdir);

	for (ap = argv = *argvp;; ++ap) {
		if (!*ap)
			errx(1,
			    "-execdir: no terminating \";\"");
		if (**ap == ';')
			break;
	}

	cnt = ap - *argvp + 1;
	new->e_argv = ereallocarray(NULL, cnt, sizeof(char *));
	new->e_orig = ereallocarray(NULL, cnt, sizeof(char *));
	new->e_len = ereallocarray(NULL, cnt, sizeof(int));

	for (argv = *argvp, cnt = 0; argv < ap; ++argv, ++cnt) {
		new->e_orig[cnt] = *argv;
		for (p = *argv; *p; ++p)
			if (p[0] == '{' && p[1] == '}') {
				new->e_argv[cnt] = emalloc((u_int)PATH_MAX);
				new->e_len[cnt] = PATH_MAX;
				break;
			}
		if (!*p) {
			new->e_argv[cnt] = *argv;
			new->e_len[cnt] = 0;
		}
	}
	new->e_argv[cnt] = new->e_orig[cnt] = NULL;

	*argvp = argv + 1;
	return (new);
}

/*
 * -flags functions --
 *
 *	The flags argument is used to represent file flags bits.
 */
int
f_flags(PLAN *plan, FTSENT *entry)
{
	u_int flags;

	flags = entry->fts_statp->st_flags &
	    (UF_NODUMP | UF_IMMUTABLE | UF_APPEND | UF_OPAQUE |
	     SF_ARCHIVED | SF_IMMUTABLE | SF_APPEND);
	if (plan->flags == F_ATLEAST)
		/* note that plan->fl_flags always is a subset of
		   plan->fl_mask */
		return ((flags & plan->fl_mask) == plan->fl_flags);
	else
		return (flags == plan->fl_flags);
	/* NOTREACHED */
}

PLAN *
c_flags(char *flags_str, char ***ignored, int unused)
{
	PLAN *new;
	u_int32_t flags, notflags;

	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_FLAGS, f_flags);

	if (*flags_str == '-') {
		new->flags = F_ATLEAST;
		++flags_str;
	}

	if (strtofflags(&flags_str, &flags, &notflags) == 1)
		errx(1, "-flags: %s: illegal flags string", flags_str);

	new->fl_flags = flags;
	new->fl_mask = flags | notflags;
	return (new);
}
 
/*
 * -follow functions --
 *
 *	Always true, causes symbolic links to be followed on a global
 *	basis.
 */
PLAN *
c_follow(char *ignore, char ***ignored, int unused)
{
	ftsoptions &= ~FTS_PHYSICAL;
	ftsoptions |= FTS_LOGICAL;

	return (palloc(N_FOLLOW, f_always_true));
}
 
/*
 * -fstype functions --
 *
 *	True if the file is of a certain type.
 */
int
f_fstype(PLAN *plan, FTSENT *entry)
{
	static dev_t curdev;	/* need a guaranteed illegal dev value */
	static int first = 1;
	struct statfs sb;
	static short val;
	static char fstype[MFSNAMELEN];
	char *p, save[2];

	/* Only check when we cross mount point. */
	if (first || curdev != entry->fts_statp->st_dev) {
		curdev = entry->fts_statp->st_dev;

		/*
		 * Statfs follows symlinks; find wants the link's file system,
		 * not where it points.
		 */
		if (entry->fts_info == FTS_SL ||
		    entry->fts_info == FTS_SLNONE) {
			if ((p = strrchr(entry->fts_accpath, '/')))
				++p;
			else
				p = entry->fts_accpath;
			save[0] = p[0];
			p[0] = '.';
			save[1] = p[1];
			p[1] = '\0';
			
		} else 
			p = NULL;

		if (statfs(entry->fts_accpath, &sb))
			err(1, "%s", entry->fts_accpath);

		if (p) {
			p[0] = save[0];
			p[1] = save[1];
		}

		first = 0;

		/*
		 * Further tests may need both of these values, so
		 * always copy both of them.
		 */
		val = sb.f_flags;
		strncpy(fstype, sb.f_fstypename, MFSNAMELEN);
	}
	switch (plan->flags) {
	case F_MTFLAG:
		return (val & plan->mt_data);	
	case F_MTTYPE:
		return (strncmp(fstype, plan->c_data, MFSNAMELEN) == 0);
	default:
		abort();
	}
}
 
PLAN *
c_fstype(char *arg, char ***ignored, int unused)
{
	PLAN *new;
    
	ftsoptions &= ~FTS_NOSTAT;
    
	new = palloc(N_FSTYPE, f_fstype);
	switch (*arg) {
	case 'l':
		if (!strcmp(arg, "local")) {
			new->flags = F_MTFLAG;
			new->mt_data = MNT_LOCAL;
			return (new);
		}
		break;
	case 'r':
		if (!strcmp(arg, "rdonly")) {
			new->flags = F_MTFLAG;
			new->mt_data = MNT_RDONLY;
			return (new);
		}
		break;
	}

	new->flags = F_MTTYPE;
	new->c_data = arg;
	return (new);
}
 
/*
 * -group gname functions --
 *
 *	True if the file belongs to the group gname.  If gname is numeric and
 *	an equivalent of the getgrnam() function does not return a valid group
 *	name, gname is taken as a group ID.
 */
int
f_group(PLAN *plan, FTSENT *entry)
{
	return (entry->fts_statp->st_gid == plan->g_data);
}
 
PLAN *
c_group(char *gname, char ***ignored, int unused)
{
	PLAN *new;
	gid_t gid;
    
	ftsoptions &= ~FTS_NOSTAT;

	if (gid_from_group(gname, &gid) == -1) {
		const char *errstr;

		gid = strtonum(gname, 0, GID_MAX, &errstr);
		if (errstr)
			errx(1, "-group: %s: no such group", gname);
	}
    
	new = palloc(N_GROUP, f_group);
	new->g_data = gid;
	return (new);
}

/*
 * -inum n functions --
 *
 *	True if the file has inode # n.
 */
int
f_inum(PLAN *plan, FTSENT *entry)
{
	COMPARE(entry->fts_statp->st_ino, plan->i_data);
}
 
PLAN *
c_inum(char *arg, char ***ignored, int unused)
{
	long long inum;
	PLAN *new;
    
	ftsoptions &= ~FTS_NOSTAT;
    
	new = palloc(N_INUM, f_inum);
	inum = find_parsenum(new, "-inum", arg, NULL);
	if (inum != (ino_t)inum)
		errx(1, "-inum: %s: number too great", arg);
	new->i_data = inum;
	return (new);
}
 
/*
 * -links n functions --
 *
 *	True if the file has n links.
 */
int
f_links(PLAN *plan, FTSENT *entry)
{
	COMPARE(entry->fts_statp->st_nlink, plan->l_data);
}
 
PLAN *
c_links(char *arg, char ***ignored, int unused)
{
	PLAN *new;
	long long nlink;
    
	ftsoptions &= ~FTS_NOSTAT;
    
	new = palloc(N_LINKS, f_links);
	nlink = find_parsenum(new, "-links", arg, NULL);
	if (nlink != (nlink_t)nlink)
		errx(1, "-links: %s: number too great", arg);
	new->l_data = nlink;
	return (new);
}
 
/*
 * -ls functions --
 *
 *	Always true - prints the current entry to stdout in "ls" format.
 */
int
f_ls(PLAN *plan, FTSENT *entry)
{
	printlong(entry->fts_path, entry->fts_accpath, entry->fts_statp);
	return (1);
}
 
PLAN *
c_ls(char *ignore, char ***ignored, int unused)
{
	ftsoptions &= ~FTS_NOSTAT;
	isoutput = 1;
    
	return (palloc(N_LS, f_ls));
}

/*
 * - maxdepth n functions --
 *
 *	True if the current search depth is less than or equal to the
 *	maximum depth specified
 */
int
f_maxdepth(PLAN *plan, FTSENT *entry)
{

	if (entry->fts_level >= plan->max_data)
		fts_set(tree, entry, FTS_SKIP);
	return (entry->fts_level <= plan->max_data);
}

PLAN *
c_maxdepth(char *arg, char ***ignored, int unused)
{
	PLAN *new;
	const char *errstr = NULL;

	new = palloc(N_MAXDEPTH, f_maxdepth);
	new->max_data = strtonum(arg, 0, FTS_MAXLEVEL, &errstr);
	if (errstr)
		errx(1, "%s: maxdepth value %s", arg, errstr);
	return (new);
}

/*
 * - mindepth n functions --
 *
 *	True if the current search depth is greater than or equal to the
 *	minimum depth specified
 */
int
f_mindepth(PLAN *plan, FTSENT *entry)
{

	return (entry->fts_level >= plan->min_data);
}

PLAN *
c_mindepth(char *arg, char ***ignored, int unused)
{
	PLAN *new;
	const char *errstr = NULL;

	new = palloc(N_MINDEPTH, f_mindepth);
	new->min_data = strtonum(arg, 0, INT_MAX, &errstr);
	if (errstr)
		errx(1, "-mindepth: %s: value %s", arg, errstr);
	return (new);
}

/*
 * -mtime n functions --
 *
 *	True if the difference between the file modification time and the
 *	current time is n 24 hour periods.
 */
int
f_mtime(PLAN *plan, FTSENT *entry)
{

	COMPARE((now - entry->fts_statp->st_mtime + SECSPERDAY - 1) /
	    SECSPERDAY, plan->sec_data);
}
 
PLAN *
c_mtime(char *arg, char ***ignored, int unused)
{
	PLAN *new;

	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_MTIME, f_mtime);
	new->sec_data = find_parsenum(new, "-mtime", arg, NULL);
	TIME_CORRECT(new, N_MTIME);
	return (new);
}

/*
 * -mmin n functions --
 *
 *     True if the difference between the file modification time and the
 *     current time is n min periods.
 */
int
f_mmin(PLAN *plan, FTSENT *entry)
{
	extern time_t now;

	COMPARE((now - entry->fts_statp->st_mtime + 60 - 1) /
	    60, plan->sec_data);
}

PLAN *
c_mmin(char *arg, char ***ignored, int unused)
{
	PLAN *new;

	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_MMIN, f_mmin);
	new->sec_data = find_parsenum(new, "-mmin", arg, NULL);
	TIME_CORRECT(new, N_MMIN);
	return (new);
}

/*
 * -name functions --
 *
 *	True if the basename of the filename being examined
 *	matches pattern using Pattern Matching Notation S3.14
 */
int
f_name(PLAN *plan, FTSENT *entry)
{
	return (!fnmatch(plan->c_data, entry->fts_name, 0));
}
 
PLAN *
c_name(char *pattern, char ***ignored, int unused)
{
	PLAN *new;

	new = palloc(N_NAME, f_name);
	new->c_data = pattern;
	return (new);
}

/*
 * -iname functions --
 *
 *	Similar to -name, but does case insensitive matching
 *	
 */
int
f_iname(PLAN *plan, FTSENT *entry)
{
	return (!fnmatch(plan->c_data, entry->fts_name, FNM_CASEFOLD));
}
 
PLAN *
c_iname(char *pattern, char ***ignored, int unused)
{
	PLAN *new;

	new = palloc(N_INAME, f_iname);
	new->c_data = pattern;
	return (new);
}
 
/*
 * -newer file functions --
 *
 *	True if the current file has been modified more recently
 *	then the modification time of the file named by the pathname
 *	file.
 */
int
f_newer(PLAN *plan, FTSENT *entry)
{

	return (entry->fts_statp->st_mtimespec.tv_sec > plan->t_data.tv_sec ||
	    (entry->fts_statp->st_mtimespec.tv_sec == plan->t_data.tv_sec &&
	    entry->fts_statp->st_mtimespec.tv_nsec > plan->t_data.tv_nsec));
}
 
PLAN *
c_newer(char *filename, char ***ignored, int unused)
{
	PLAN *new;
	struct stat sb;
    
	ftsoptions &= ~FTS_NOSTAT;

	if (stat(filename, &sb))
		err(1, "%s", filename);
	new = palloc(N_NEWER, f_newer);
	memcpy(&new->t_data, &sb.st_mtimespec, sizeof(struct timespec));
	return (new);
}
 
/*
 * -anewer file functions --
 *
 *	True if the current file has been accessed more recently
 *	then the access time of the file named by the pathname
 *	file.
 */
int
f_anewer(PLAN *plan, FTSENT *entry)
{

	return (entry->fts_statp->st_atimespec.tv_sec > plan->t_data.tv_sec ||
	    (entry->fts_statp->st_atimespec.tv_sec == plan->t_data.tv_sec &&
	    entry->fts_statp->st_atimespec.tv_nsec > plan->t_data.tv_nsec));
}
 
PLAN *
c_anewer(char *filename, char ***ignored, int unused)
{
	PLAN *new;
	struct stat sb;
    
	ftsoptions &= ~FTS_NOSTAT;

	if (stat(filename, &sb))
		err(1, "%s", filename);
	new = palloc(N_NEWER, f_anewer);
	memcpy(&new->t_data, &sb.st_atimespec, sizeof(struct timespec));
	return (new);
}
 
/*
 * -cnewer file functions --
 *
 *	True if the current file has been changed more recently
 *	then the inode change time of the file named by the pathname
 *	file.
 */
int
f_cnewer(PLAN *plan, FTSENT *entry)
{

	return (entry->fts_statp->st_ctimespec.tv_sec > plan->t_data.tv_sec ||
	    (entry->fts_statp->st_ctimespec.tv_sec == plan->t_data.tv_sec &&
	    entry->fts_statp->st_ctimespec.tv_nsec > plan->t_data.tv_nsec));
}
 
PLAN *
c_cnewer(char *filename, char ***ignored, int unused)
{
	PLAN *new;
	struct stat sb;
    
	ftsoptions &= ~FTS_NOSTAT;

	if (stat(filename, &sb))
		err(1, "%s", filename);
	new = palloc(N_NEWER, f_cnewer);
	memcpy(&new->t_data, &sb.st_ctimespec, sizeof(struct timespec));
	return (new);
}
 
/*
 * -nogroup functions --
 *
 *	True if file belongs to a user ID for which the equivalent
 *	of the getgrnam() 9.2.1 [POSIX.1] function returns NULL.
 */
int
f_nogroup(PLAN *plan, FTSENT *entry)
{
	return (group_from_gid(entry->fts_statp->st_gid, 1) ? 0 : 1);
}
 
PLAN *
c_nogroup(char *ignore, char ***ignored, int unused)
{
	ftsoptions &= ~FTS_NOSTAT;

	return (palloc(N_NOGROUP, f_nogroup));
}
 
/*
 * -nouser functions --
 *
 *	True if file belongs to a user ID for which the equivalent
 *	of the getpwuid() 9.2.2 [POSIX.1] function returns NULL.
 */
int
f_nouser(PLAN *plan, FTSENT *entry)
{
	return (user_from_uid(entry->fts_statp->st_uid, 1) ? 0 : 1);
}
 
PLAN *
c_nouser(char *ignore, char ***ignored, int unused)
{
	ftsoptions &= ~FTS_NOSTAT;

	return (palloc(N_NOUSER, f_nouser));
}
 
/*
 * -path functions --
 *
 *	True if the path of the filename being examined
 *	matches pattern using Pattern Matching Notation S3.14
 */
int
f_path(PLAN *plan, FTSENT *entry)
{
	return (!fnmatch(plan->c_data, entry->fts_path, 0));
}
 
PLAN *
c_path(char *pattern, char ***ignored, int unused)
{
	PLAN *new;

	new = palloc(N_NAME, f_path);
	new->c_data = pattern;
	return (new);
}
 
/*
 * -perm functions --
 *
 *	The mode argument is used to represent file mode bits.  If it starts
 *	with a leading digit, it's treated as an octal mode, otherwise as a
 *	symbolic mode.
 */
int
f_perm(PLAN *plan, FTSENT *entry)
{
	mode_t mode;

	mode = entry->fts_statp->st_mode &
	    (S_ISUID|S_ISGID|S_ISTXT|S_IRWXU|S_IRWXG|S_IRWXO);
	if (plan->flags == F_ATLEAST)
		return ((plan->m_data | mode) == mode);
	else
		return (mode == plan->m_data);
	/* NOTREACHED */
}
 
PLAN *
c_perm(char *perm, char ***ignored, int unused)
{
	PLAN *new;
	void *set;

	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_PERM, f_perm);

	if (*perm == '-') {
		new->flags = F_ATLEAST;
		++perm;
	}

	if ((set = setmode(perm)) == NULL)
		errx(1, "-perm: %s: illegal mode string", perm);

	new->m_data = getmode(set, 0);
	free(set);
	return (new);
}
 
/*
 * -print functions --
 *
 *	Always true, causes the current pathame to be written to
 *	standard output.
 */
int
f_print(PLAN *plan, FTSENT *entry)
{
	(void)printf("%s\n", entry->fts_path);
	return(1);
}

/* ARGSUSED */
int
f_print0(PLAN *plan, FTSENT *entry)
{
	(void)fputs(entry->fts_path, stdout);
	(void)fputc('\0', stdout);
	return(1);
}
 
PLAN *
c_print(char *ignore, char ***ignored, int unused)
{
	isoutput = 1;

	return(palloc(N_PRINT, f_print));
}

PLAN *
c_print0(char *ignore, char ***ignored, int unused)
{
	isoutput = 1;

	return(palloc(N_PRINT0, f_print0));
}
 
/*
 * -prune functions --
 *
 *	Prune a portion of the hierarchy.
 */
int
f_prune(PLAN *plan, FTSENT *entry)
{

	if (fts_set(tree, entry, FTS_SKIP))
		err(1, "%s", entry->fts_path);
	return (1);
}
 
PLAN *
c_prune(char *ignore, char ***ignored, int unused)
{
	return (palloc(N_PRUNE, f_prune));
}
 
/*
 * -size n[c] functions --
 *
 *	True if the file size in bytes, divided by an implementation defined
 *	value and rounded up to the next integer, is n.  If n is followed by
 *	a c, the size is in bytes.
 */
#define	FIND_SIZE	512
static int divsize = 1;

int
f_size(PLAN *plan, FTSENT *entry)
{
	off_t size;

	size = divsize ? (entry->fts_statp->st_size + FIND_SIZE - 1) /
	    FIND_SIZE : entry->fts_statp->st_size;
	COMPARE(size, plan->o_data);
}
 
PLAN *
c_size(char *arg, char ***ignored, int unused)
{
	PLAN *new;
	char endch;
    
	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_SIZE, f_size);
	endch = 'c';
	new->o_data = find_parsenum(new, "-size", arg, &endch);
	if (endch == 'c')
		divsize = 0;
	return (new);
}
 
/*
 * -type c functions --
 *
 *	True if the type of the file is c, where c is b, c, d, p, or f for
 *	block special file, character special file, directory, FIFO, or
 *	regular file, respectively.
 */
int
f_type(PLAN *plan, FTSENT *entry)
{
	return ((entry->fts_statp->st_mode & S_IFMT) == plan->m_data);
}
 
PLAN *
c_type(char *typestring, char ***ignored, int unused)
{
	PLAN *new;
	mode_t  mask;
    
	ftsoptions &= ~FTS_NOSTAT;

	switch (typestring[0]) {
	case 'b':
		mask = S_IFBLK;
		break;
	case 'c':
		mask = S_IFCHR;
		break;
	case 'd':
		mask = S_IFDIR;
		break;
	case 'f':
		mask = S_IFREG;
		break;
	case 'l':
		mask = S_IFLNK;
		break;
	case 'p':
		mask = S_IFIFO;
		break;
	case 's':
		mask = S_IFSOCK;
		break;
	default:
		errx(1, "-type: %s: unknown type", typestring);
	}
    
	new = palloc(N_TYPE, f_type);
	new->m_data = mask;
	return (new);
}
 
/*
 * -user uname functions --
 *
 *	True if the file belongs to the user uname.  If uname is numeric and
 *	an equivalent of the getpwnam() S9.2.2 [POSIX.1] function does not
 *	return a valid user name, uname is taken as a user ID.
 */
int
f_user(PLAN *plan, FTSENT *entry)
{
	return (entry->fts_statp->st_uid == plan->u_data);
}
 
PLAN *
c_user(char *username, char ***ignored, int unused)
{
	PLAN *new;
	uid_t uid;
    
	ftsoptions &= ~FTS_NOSTAT;

	if (uid_from_user(username, &uid) == -1) {
		const char *errstr;

		uid = strtonum(username, 0, UID_MAX, &errstr);
		if (errstr)
			errx(1, "-user: %s: no such user", username);
	}

	new = palloc(N_USER, f_user);
	new->u_data = uid;
	return (new);
}
 
/*
 * -xdev functions --
 *
 *	Always true, causes find not to decend past directories that have a
 *	different device ID (st_dev, see stat() S5.6.2 [POSIX.1])
 */
PLAN *
c_xdev(char *ignore, char ***ignored, int unused)
{
	ftsoptions |= FTS_XDEV;

	return (palloc(N_XDEV, f_always_true));
}

/*
 * ( expression ) functions --
 *
 *	True if expression is true.
 */
int
f_expr(PLAN *plan, FTSENT *entry)
{
	PLAN *p;
	int state;

	for (p = plan->p_data[0];
	    p && (state = (p->eval)(p, entry)); p = p->next);
	return (state);
}
 
/*
 * N_OPENPAREN and N_CLOSEPAREN nodes are temporary place markers.  They are
 * eliminated during phase 2 of find_formplan() --- the '(' node is converted
 * to a N_EXPR node containing the expression and the ')' node is discarded.
 */
PLAN *
c_openparen(char *ignore, char ***ignored, int unused)
{
	return (palloc(N_OPENPAREN, (int (*)(PLAN *, FTSENT *))-1));
}
 
PLAN *
c_closeparen(char *ignore, char ***ignored, int unused)
{
	return (palloc(N_CLOSEPAREN, (int (*)(PLAN *, FTSENT *))-1));
}
 
/*
 * ! expression functions --
 *
 *	Negation of a primary; the unary NOT operator.
 */
int
f_not(PLAN *plan, FTSENT *entry)
{
	PLAN *p;
	int state;

	for (p = plan->p_data[0];
	    p && (state = (p->eval)(p, entry)); p = p->next);
	return (!state);
}
 
PLAN *
c_not(char *ignore, char ***ignored, int unused)
{
	return (palloc(N_NOT, f_not));
}
 
/*
 * expression -o expression functions --
 *
 *	Alternation of primaries; the OR operator.  The second expression is
 * not evaluated if the first expression is true.
 */
int
f_or(PLAN *plan, FTSENT *entry)
{
	PLAN *p;
	int state;

	for (p = plan->p_data[0];
	    p && (state = (p->eval)(p, entry)); p = p->next);

	if (state)
		return (1);

	for (p = plan->p_data[1];
	    p && (state = (p->eval)(p, entry)); p = p->next);
	return (state);
}

PLAN *
c_or(char *ignore, char ***ignored, int unused)
{
	return (palloc(N_OR, f_or));
}


/*
 * plan_cleanup --
 *	Check and see if the specified plan has any residual state,
 *	and if so, clean it up as appropriate.
 *
 *	At the moment, only N_EXEC has state. Two kinds: 1)
 * 	lists of files to feed to subprocesses 2) State on exit
 *	statusses of past subprocesses.
 */
/* ARGSUSED1 */
int
plan_cleanup(PLAN *plan, void *arg)
{
	if (plan->type==N_EXEC && plan->ep_narg)
		run_f_exec(plan);

	return plan->ep_rval;		/* Passed save exit-status up chain */
}


static PLAN *
palloc(enum ntype t, int (*f)(PLAN *, FTSENT *))
{
	PLAN *new;

	if ((new = calloc(1, sizeof(PLAN)))) {
		new->type = t;
		new->eval = f;
		return (new);
	}
	err(1, NULL);
	/* NOTREACHED */
}
