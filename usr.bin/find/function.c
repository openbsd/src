/*	$OpenBSD: function.c,v 1.12 1998/09/26 09:04:43 deraadt Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#ifndef lint
/*static char sccsid[] = "from: @(#)function.c	8.1 (Berkeley) 6/6/93";*/
static char rcsid[] = "$OpenBSD: function.c,v 1.12 1998/09/26 09:04:43 deraadt Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mount.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fnmatch.h>
#include <fts.h>
#include <grp.h>
#include <libgen.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tzfile.h>
#include <unistd.h>

#include "find.h"

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

static PLAN *palloc __P((enum ntype, int (*) __P((PLAN *, FTSENT *))));

extern int dotfd;
extern time_t now;
extern FTS *tree;

/*
 * find_parsenum --
 *	Parse a string of the form [+-]# and return the value.
 */
static long
find_parsenum(plan, option, vp, endch)
	PLAN *plan;
	char *option, *vp, *endch;
{
	long value;
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
	 * Convert the string with strtol().  Note, if strtol() returns zero
	 * and endchar points to the beginning of the string we know we have
	 * a syntax error.
	 */
	value = strtol(str, &endchar, 10);
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
		++((p)->t_data);

/*
 * -amin n functions --
 *
 *     True if the difference between the file access time and the
 *     current time is n min periods.
 */
int
f_amin(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	extern time_t now;

	COMPARE((now - entry->fts_statp->st_atime +
	    60 - 1) / 60, plan->t_data);
}

PLAN *
c_amin(arg)
	char *arg;
{
	PLAN *new;

	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_AMIN, f_amin);
	new->t_data = find_parsenum(new, "-amin", arg, NULL);
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
f_atime(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{

	COMPARE((now - entry->fts_statp->st_atime +
	    SECSPERDAY - 1) / SECSPERDAY, plan->t_data);
}
 
PLAN *
c_atime(arg)
	char *arg;
{
	PLAN *new;

	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_ATIME, f_atime);
	new->t_data = find_parsenum(new, "-atime", arg, NULL);
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
f_cmin(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	extern time_t now;

	COMPARE((now - entry->fts_statp->st_ctime +
	    60 - 1) / 60, plan->t_data);
}

PLAN *
c_cmin(arg)
	char *arg;
{
	PLAN *new;

	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_CMIN, f_cmin);
	new->t_data = find_parsenum(new, "-cmin", arg, NULL);
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
f_ctime(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{

	COMPARE((now - entry->fts_statp->st_ctime +
	    SECSPERDAY - 1) / SECSPERDAY, plan->t_data);
}
 
PLAN *
c_ctime(arg)
	char *arg;
{
	PLAN *new;

	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_CTIME, f_ctime);
	new->t_data = find_parsenum(new, "-ctime", arg, NULL);
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
f_always_true(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	return (1);
}
 
PLAN *
c_depth()
{
	isdepth = 1;

	return (palloc(N_DEPTH, f_always_true));
}
 
/*
 * -empty functions --
 *
 *	True if the file or directory is empty
 */
int
f_empty(plan, entry)
	PLAN *plan;
	FTSENT *entry;
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
			err(1, "%s", entry->fts_accpath);
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
c_empty()
{
	ftsoptions &= ~FTS_NOSTAT;

	return (palloc(N_EMPTY, f_empty));
}

/*
 * [-exec | -ok] utility [arg ... ] ; functions --
 *
 *	True if the executed utility returns a zero value as exit status.
 *	The end of the primary expression is delimited by a semicolon.  If
 *	"{}" occurs anywhere, it gets replaced by the current pathname.
 *	The current directory for the execution of utility is the same as
 *	the current directory when the find utility was started.
 *
 *	The primary -ok is different in that it requests affirmation of the
 *	user before executing the utility.
 */
int
f_exec(plan, entry)
	register PLAN *plan;
	FTSENT *entry;
{
	register int cnt;
	pid_t pid;
	int status;

	for (cnt = 0; plan->e_argv[cnt]; ++cnt)
		if (plan->e_len[cnt])
			brace_subst(plan->e_orig[cnt], &plan->e_argv[cnt],
			    entry->fts_path, plan->e_len[cnt]);

	if (plan->flags == F_NEEDOK && !queryuser(plan->e_argv))
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
 
/*
 * c_exec --
 *	build three parallel arrays, one with pointers to the strings passed
 *	on the command line, one with (possibly duplicated) pointers to the
 *	argv array, and one with integer values that are lengths of the
 *	strings, but also flags meaning that the string has to be massaged.
 */
PLAN *
c_exec(argvp, isok)
	char ***argvp;
	int isok;
{
	PLAN *new;			/* node returned */
	register int cnt;
	register char **argv, **ap, *p;

	isoutput = 1;
    
	new = palloc(N_EXEC, f_exec);
	if (isok)
		new->flags = F_NEEDOK;

	for (ap = argv = *argvp;; ++ap) {
		if (!*ap)
			errx(1,
			    "%s: no terminating \";\"", isok ? "-ok" : "-exec");
		if (**ap == ';')
			break;
	}

	cnt = ap - *argvp + 1;
	new->e_argv = (char **)emalloc((u_int)cnt * sizeof(char *));
	new->e_orig = (char **)emalloc((u_int)cnt * sizeof(char *));
	new->e_len = (int *)emalloc((u_int)cnt * sizeof(int));

	for (argv = *argvp, cnt = 0; argv < ap; ++argv, ++cnt) {
		new->e_orig[cnt] = *argv;
		for (p = *argv; *p; ++p)
			if (p[0] == '{' && p[1] == '}') {
				new->e_argv[cnt] = emalloc((u_int)MAXPATHLEN);
				new->e_len[cnt] = MAXPATHLEN;
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
 * -execdir utility [arg ... ] ; functions --
 *
 *	True if the executed utility returns a zero value as exit status.
 *	The end of the primary expression is delimited by a semicolon.  If
 *	"{}" occurs anywhere, it gets replaced by the unqualified pathname.
 *	The current directory for the execution of utility is the same as
 *	the directory where the file lives.
 */
int
f_execdir(plan, entry)
	register PLAN *plan;
	FTSENT *entry;
{
	register int cnt;
	pid_t pid;
	int status;
	char base[MAXPATHLEN];

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
c_execdir(argvp)
	char ***argvp;
{
	PLAN *new;			/* node returned */
	register int cnt;
	register char **argv, **ap, *p;

	ftsoptions &= ~FTS_NOSTAT;
	ftsoptions |= FTS_CHDIRROOT;
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
	new->e_argv = (char **)emalloc((u_int)cnt * sizeof(char *));
	new->e_orig = (char **)emalloc((u_int)cnt * sizeof(char *));
	new->e_len = (int *)emalloc((u_int)cnt * sizeof(int));

	for (argv = *argvp, cnt = 0; argv < ap; ++argv, ++cnt) {
		new->e_orig[cnt] = *argv;
		for (p = *argv; *p; ++p)
			if (p[0] == '{' && p[1] == '}') {
				new->e_argv[cnt] = emalloc((u_int)MAXPATHLEN);
				new->e_len[cnt] = MAXPATHLEN;
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
 * -follow functions --
 *
 *	Always true, causes symbolic links to be followed on a global
 *	basis.
 */
PLAN *
c_follow()
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
f_fstype(plan, entry)
	PLAN *plan;
	FTSENT *entry;
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
c_fstype(arg)
	char *arg;
{
	register PLAN *new;
    
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
f_group(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	return (entry->fts_statp->st_gid == plan->g_data);
}
 
PLAN *
c_group(gname)
	char *gname;
{
	PLAN *new;
	struct group *g;
	gid_t gid;
    
	ftsoptions &= ~FTS_NOSTAT;

	g = getgrnam(gname);
	if (g == NULL) {
		gid = atoi(gname);
		if (gid == 0 && gname[0] != '0')
			errx(1, "-group: %s: no such group", gname);
	} else
		gid = g->gr_gid;
    
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
f_inum(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	COMPARE(entry->fts_statp->st_ino, plan->i_data);
}
 
PLAN *
c_inum(arg)
	char *arg;
{
	PLAN *new;
    
	ftsoptions &= ~FTS_NOSTAT;
    
	new = palloc(N_INUM, f_inum);
	new->i_data = find_parsenum(new, "-inum", arg, NULL);
	return (new);
}
 
/*
 * -links n functions --
 *
 *	True if the file has n links.
 */
int
f_links(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	COMPARE(entry->fts_statp->st_nlink, plan->l_data);
}
 
PLAN *
c_links(arg)
	char *arg;
{
	PLAN *new;
    
	ftsoptions &= ~FTS_NOSTAT;
    
	new = palloc(N_LINKS, f_links);
	new->l_data = (nlink_t)find_parsenum(new, "-links", arg, NULL);
	return (new);
}
 
/*
 * -ls functions --
 *
 *	Always true - prints the current entry to stdout in "ls" format.
 */
int
f_ls(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	printlong(entry->fts_path, entry->fts_accpath, entry->fts_statp);
	return (1);
}
 
PLAN *
c_ls()
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
f_maxdepth(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{

	if (entry->fts_level >= plan->max_data)
		fts_set(tree, entry, FTS_SKIP);
	return (entry->fts_level <= plan->max_data);
}

PLAN *
c_maxdepth(arg)
	char *arg;
{
	PLAN *new;

	new = palloc(N_MAXDEPTH, f_maxdepth);
	new->max_data = atoi(arg);
	return (new);
}

/*
 * - mindepth n functions --
 *
 *	True if the current search depth is greater than or equal to the
 *	minimum depth specified
 */
int
f_mindepth(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{

	return (entry->fts_level >= plan->min_data);
}

PLAN *
c_mindepth(arg)
	char *arg;
{
	PLAN *new;

	new = palloc(N_MINDEPTH, f_mindepth);
	new->min_data = atoi(arg);
	return (new);
}

/*
 * -mtime n functions --
 *
 *	True if the difference between the file modification time and the
 *	current time is n 24 hour periods.
 */
int
f_mtime(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{

	COMPARE((now - entry->fts_statp->st_mtime + SECSPERDAY - 1) /
	    SECSPERDAY, plan->t_data);
}
 
PLAN *
c_mtime(arg)
	char *arg;
{
	PLAN *new;

	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_MTIME, f_mtime);
	new->t_data = find_parsenum(new, "-mtime", arg, NULL);
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
f_mmin(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	extern time_t now;

	COMPARE((now - entry->fts_statp->st_mtime + 60 - 1) /
	    60, plan->t_data);
}

PLAN *
c_mmin(arg)
	char *arg;
{
	PLAN *new;

	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_MMIN, f_mmin);
	new->t_data = find_parsenum(new, "-mmin", arg, NULL);
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
f_name(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	return (!fnmatch(plan->c_data, entry->fts_name, 0));
}
 
PLAN *
c_name(pattern)
	char *pattern;
{
	PLAN *new;

	new = palloc(N_NAME, f_name);
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
f_newer(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	return (entry->fts_statp->st_mtime > plan->t_data);
}
 
PLAN *
c_newer(filename)
	char *filename;
{
	PLAN *new;
	struct stat sb;
    
	ftsoptions &= ~FTS_NOSTAT;

	if (stat(filename, &sb))
		err(1, "%s", filename);
	new = palloc(N_NEWER, f_newer);
	new->t_data = sb.st_mtime;
	return (new);
}
 
/*
 * -nogroup functions --
 *
 *	True if file belongs to a user ID for which the equivalent
 *	of the getgrnam() 9.2.1 [POSIX.1] function returns NULL.
 */
int
f_nogroup(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	char *group_from_gid();

	return (group_from_gid(entry->fts_statp->st_gid, 1) ? 0 : 1);
}
 
PLAN *
c_nogroup()
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
f_nouser(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	char *user_from_uid();

	return (user_from_uid(entry->fts_statp->st_uid, 1) ? 0 : 1);
}
 
PLAN *
c_nouser()
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
f_path(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	return (!fnmatch(plan->c_data, entry->fts_path, 0));
}
 
PLAN *
c_path(pattern)
	char *pattern;
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
f_perm(plan, entry)
	PLAN *plan;
	FTSENT *entry;
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
c_perm(perm)
	char *perm;
{
	PLAN *new;
	mode_t *set;

	ftsoptions &= ~FTS_NOSTAT;

	new = palloc(N_PERM, f_perm);

	if (*perm == '-') {
		new->flags = F_ATLEAST;
		++perm;
	}

	if ((set = setmode(perm)) == NULL)
		err(1, "-perm: %s: illegal mode string", perm);

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
f_print(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	(void)printf("%s\n", entry->fts_path);
	return(1);
}

/* ARGSUSED */
int
f_print0(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	(void)fputs(entry->fts_path, stdout);
	(void)fputc('\0', stdout);
	return(1);
}
 
PLAN *
c_print()
{
	isoutput = 1;

	return(palloc(N_PRINT, f_print));
}

PLAN *
c_print0()
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
f_prune(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{

	if (fts_set(tree, entry, FTS_SKIP))
		err(1, "%s", entry->fts_path);
	return (1);
}
 
PLAN *
c_prune()
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
f_size(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	off_t size;

	size = divsize ? (entry->fts_statp->st_size + FIND_SIZE - 1) /
	    FIND_SIZE : entry->fts_statp->st_size;
	COMPARE(size, plan->o_data);
}
 
PLAN *
c_size(arg)
	char *arg;
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
f_type(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	return ((entry->fts_statp->st_mode & S_IFMT) == plan->m_data);
}
 
PLAN *
c_type(typestring)
	char *typestring;
{
	PLAN *new;
	mode_t  mask;
    
	ftsoptions &= ~FTS_NOSTAT;

	switch (typestring[0]) {
#ifdef S_IFWHT
	case 'W':
		mask = S_IFWHT;
		break;
#endif
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
f_user(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	return (entry->fts_statp->st_uid == plan->u_data);
}
 
PLAN *
c_user(username)
	char *username;
{
	PLAN *new;
	struct passwd *p;
	uid_t uid;
    
	ftsoptions &= ~FTS_NOSTAT;

	p = getpwnam(username);
	if (p == NULL) {
		uid = atoi(username);
		if (uid == 0 && username[0] != '0')
			errx(1, "-user: %s: no such user", username);
	} else
		uid = p->pw_uid;

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
c_xdev()
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
f_expr(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	register PLAN *p;
	register int state;

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
c_openparen()
{
	return (palloc(N_OPENPAREN, (int (*)())-1));
}
 
PLAN *
c_closeparen()
{
	return (palloc(N_CLOSEPAREN, (int (*)())-1));
}
 
/*
 * ! expression functions --
 *
 *	Negation of a primary; the unary NOT operator.
 */
int
f_not(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	register PLAN *p;
	register int state;

	for (p = plan->p_data[0];
	    p && (state = (p->eval)(p, entry)); p = p->next);
	return (!state);
}
 
PLAN *
c_not()
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
f_or(plan, entry)
	PLAN *plan;
	FTSENT *entry;
{
	register PLAN *p;
	register int state;

	for (p = plan->p_data[0];
	    p && (state = (p->eval)(p, entry)); p = p->next);

	if (state)
		return (1);

	for (p = plan->p_data[1];
	    p && (state = (p->eval)(p, entry)); p = p->next);
	return (state);
}

PLAN *
c_or()
{
	return (palloc(N_OR, f_or));
}

static PLAN *
palloc(t, f)
	enum ntype t;
	int (*f) __P((PLAN *, FTSENT *));
{
	PLAN *new;

	if ((new = malloc(sizeof(PLAN)))) {
		new->type = t;
		new->eval = f;
		new->flags = 0;
		new->next = NULL;
		return (new);
	}
	err(1, NULL);
	/* NOTREACHED */
}
