/*	$OpenBSD: expand.c,v 1.15 2004/04/05 14:30:51 aaron Exp $	*/

/*
 * Copyright (c) 1991 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator   or   Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the rights
 * to redistribute these changes.
 */
/*
 *  expand - expand wildcard filename specifications
 *
 *  Usage:
 *	int expand(spec, buffer, bufsize);
 *	char *spec, **buffer;
 *	int bufsize;
 *
 *  Expand takes a file specification, and expands it into filenames
 *  by resolving the characters '*', '?', '[', ']', '{', '}' and '~'
 *  in the same manner as the shell.  You provide "buffer", which is
 *  an array of char *'s, and you tell how big it is in bufsize.
 *  Expand will compute the corresponding filenames, and will fill up
 *  the entries of buffer with pointers to malloc'd strings.
 *
 *  The value returned by expand is the number of filenames found.  If
 *  this value is -1, then malloc failed to allocate a string.  If the
 *  value is bufsize + 1, then too many names were found and you can try
 *  again with a bigger buffer.
 *
 *  This routine was basically created from the csh sh.glob.c file with
 *  the following intended differences:
 *
 *	Filenames are not sorted.
 *	All expanded filenames returned exist.
 *
 **********************************************************************
 * HISTORY
 * 13-Nov-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Replaced a stat() with lstat() and changed glob() to only call
 *	matchdir() for directories.
 *
 * 20-Oct-85  Glenn Marcy (gm0w) at Carnegie-Mellon University
 *	Created from csh glob() function and 4.1 expand() function.
 *
 **********************************************************************
 */
#include <sys/param.h>
#include <sys/stat.h>
#ifdef HAS_POSIX_DIR
#include <dirent.h>
#else
#include <sys/dir.h>
#endif
#include <pwd.h>
#include <ctype.h>
#include <libc.h>
#include <setjmp.h>
#include <stdlib.h>
#include <unistd.h>

static	jmp_buf	sjbuf;

static	char	pathbuf[MAXPATHLEN];
static	char	*path, *pathp, *lastpathp;

static	char	*globchars = "{[*?";	/* meta characters */
static	char	*entp;			/* current dir entry pointer */

static	char	**BUFFER;		/* pointer to the buffer */
static	int	BUFSIZE;		/* maximum number in buffer */
static	int	bufcnt;			/* current number in buffer */

#define fixit(a) (a[0] ? a : ".")

int expand(char *, char **, int);
static void glob(char *);
static void matchdir(char *);
static int execbrc(char *, char *);
static int match(char *, char *);
static int amatch(char *, char *);
static void addone(char *, char *);
static int addpath(int);
static int gethdir(char *, size_t);

int
expand(spec, buffer, bufsize)
	char *spec;
	char **buffer;
	int bufsize;
{
	pathp = path = pathbuf;
	*pathp = 0;
	lastpathp = &path[MAXPATHLEN - 2];
	BUFFER = buffer;
	BUFSIZE = bufsize;
	bufcnt = 0;
	if (setjmp(sjbuf) == 0)
		glob(spec);
	return(bufcnt);
}

static void
glob(as)
	char *as;
{
	char *cs;
	char *spathp, *oldcs;
	char *home;
	struct stat stb;

	if ((home = getenv("HOME")) != NULL && *home == '\0')
		home = NULL;

	spathp = pathp;
	cs = as;
	if (*cs == '~' && home && pathp == path) {
		if (addpath('~'))
			goto endit;
		for (cs++; isalnum((unsigned char) *cs) || *cs == '_' || *cs == '-';)
			if (addpath(*cs++))
				goto endit;
		if (!*cs || *cs == '/') {
			if (pathp != path + 1) {
				*pathp = 0;
				if (gethdir(path + 1, sizeof pathbuf - 1))
					goto endit;
				memmove(path, path + 1, strlen(path));
			} else
				strlcpy(path, home, sizeof pathbuf);
			pathp = path + strlen(path);
		}
	}
	while (*cs == 0 || strchr(globchars, *cs) == 0) {
		if (*cs == 0) {
			if (lstat(fixit(path), &stb) >= 0)
				addone(path, "");
			goto endit;
		}
		if (addpath(*cs++))
			goto endit;
	}
	oldcs = cs;
	while (cs > as && *cs != '/')
		cs--, pathp--;
	if (*cs == '/')
		cs++, pathp++;
	*pathp = 0;
	if (*oldcs == '{') {
		execbrc(cs, NULL);
		return;
	}
	/* this should not be an lstat */
	if (stat(fixit(path), &stb) >= 0 && S_ISDIR(stb.st_mode))
		matchdir(cs);
endit:
	pathp = spathp;
	*pathp = 0;
	return;
}

static void matchdir(pattern)
	char *pattern;
{
#ifdef HAS_POSIX_DIR
	struct dirent *dp;
#else
	struct direct *dp;
#endif
	DIR *dirp;

	dirp = opendir(fixit(path));
	if (dirp == NULL)
		return;
	while ((dp = readdir(dirp)) != NULL) {
#if defined(HAS_POSIX_DIR) && !defined(__SVR4)
		if (dp->d_fileno == 0)
			continue;
#else
		if (dp->d_ino == 0)
			continue;
#endif
		if (match(dp->d_name, pattern))
			addone(path, dp->d_name);
	}
	closedir(dirp);
	return;
}

static int execbrc(p, s)
	char *p, *s;
{
	char restbuf[MAXPATHLEN + 1];
	char *pe, *pm, *pl;
	int brclev = 0;
	char *lm, savec, *spathp;

	for (lm = restbuf; *p != '{'; *lm++ = *p++)
		continue;
	for (pe = ++p; *pe; pe++) {
		switch (*pe) {
		case '{':
			brclev++;
			continue;
		case '}':
			if (brclev == 0)
				goto pend;
			brclev--;
			continue;
		case '[':
			for (pe++; *pe && *pe != ']'; pe++)
				continue;
			if (!*pe)
				break;
			continue;
		}
	}
pend:
	if (brclev || !*pe)
		return (0);
	for (pl = pm = p; pm <= pe; pm++) {
		switch (*pm & 0177) {
		case '{':
			brclev++;
			continue;
		case '}':
			if (brclev) {
				brclev--;
				continue;
			}
			goto doit;
		case ',':
			if (brclev) continue;
doit:
			savec = *pm;
			*pm = 0;
			snprintf(lm, sizeof(restbuf) - (lm - restbuf),
			    "%s%s", pl, pe + 1);
			*pm = savec;
			if (s == 0) {
				spathp = pathp;
				glob(restbuf);
				pathp = spathp;
				*pathp = 0;
			} else if (amatch(s, restbuf))
				return (1);
			pl = pm + 1;
			continue;

		case '[':
			for (pm++; *pm && *pm != ']'; pm++)
				continue;
			if (!*pm)
				break;
			continue;
		}
	}
	return (0);
}

static int
match(s, p)
	char *s, *p;
{
	int c;
	char *sentp;

	if (*s == '.' && *p != '.')
		return(0);
	sentp = entp;
	entp = s;
	c = amatch(s, p);
	entp = sentp;
	return (c);
}

static int
amatch(s, p)
	char *s, *p;
{
	int scc;
	int ok, lc;
	char *spathp;
	struct stat stb;
	int c, cc;

	for (;;) {
		scc = *s++ & 0177;
		switch (c = *p++) {
		case '{':
			return (execbrc(p - 1, s - 1));
		case '[':
			ok = 0;
			lc = 077777;
			while ((cc = *p++) != 0) {
				if (cc == ']') {
					if (ok)
						break;
					return (0);
				}
				if (cc == '-') {
					if (lc <= scc && scc <= *p++)
						ok++;
				} else
					if (scc == (lc = cc))
						ok++;
			}
			if (cc == 0)
				return (0);
			continue;
		case '*':
			if (!*p)
				return (1);
			if (*p == '/') {
				p++;
				goto slash;
			}
			for (s--; *s; s++)
				if (amatch(s, p))
					return (1);
			return (0);
		case 0:
			return (scc == 0);
		default:
			if (c != scc)
				return (0);
			continue;
		case '?':
			if (scc == 0)
				return (0);
			continue;
		case '/':
			if (scc)
				return (0);
slash:
			s = entp;
			spathp = pathp;
			while (*s) {
				if (addpath(*s++))
					goto pathovfl;
			}
			if (addpath('/'))
				goto pathovfl;
			if (stat(fixit(path), &stb) >= 0 &&
			    S_ISDIR(stb.st_mode)) {
				if (*p == 0)
					addone(path, "");
				else
					glob(p);
			}
pathovfl:
			pathp = spathp;
			*pathp = 0;
			return (0);
		}
	}
}

static void
addone(s1, s2)
	char *s1, *s2;
{
	char *ep;

	if (bufcnt >= BUFSIZE) {
		bufcnt = BUFSIZE + 1;
		longjmp(sjbuf, 1);
	}
	ep = (char *)malloc(strlen(s1) + strlen(s2) + 1);
	if (ep == 0) {
		bufcnt = -1;
		longjmp(sjbuf, 1);
	}
	BUFFER[bufcnt++] = ep;
	while (*s1)
		*ep++ = *s1++;
	while ((*ep++ = *s2++) != '\0')
		continue;
}

static int
addpath(c)
	char c;
{
	if (pathp >= lastpathp)
		return(1);
	*pathp++ = c;
	*pathp = 0;
	return(0);
}

static int gethdir(home, homelen)
	char *home;
	size_t homelen;
{
	struct passwd *pp = getpwnam(home);

	if (pp == 0)
		return(1);
	strlcpy(home, pp->pw_dir, homelen);
	return(0);
}
