/*	$OpenBSD: expand.c,v 1.14 2011/07/22 18:26:18 matthew Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#include "defs.h"

#define	GAVSIZ	NCARGS / 6
#define LC '{'
#define RC '}'

static char	shchars[] = "${[*?";

int	which;		/* bit mask of types to expand */
int	eargc;		/* expanded arg count */
char	**eargv;	/* expanded arg vectors */
char	pathbuf[BUFSIZ];
char	*path;
char	*pathp;
char	*lastpathp;
char	*tilde;		/* "~user" if not expanding tilde, else "" */
char	*tpathp;
int	nleft;

int	expany;		/* any expansions done? */
char	*entp;
char	**sortbase;

#define sort()	qsort((char *)sortbase, &eargv[eargc] - sortbase, \
		      sizeof(*sortbase), argcmp), sortbase = &eargv[eargc]

static void	Cat(char *, char *);
static void	addpath(int);
static int	amatch(char *, char *);
static int	argcmp(const void *, const void *);
static int	execbrc(char *, char *);
static void	expsh(char *);
static void	expstr(char *);
static int	match(char *, char *);
static void	matchdir(char *);
static int	smatch(char *, char *);

/*
 * Take a list of names and expand any macros, etc.
 * wh = E_VARS if expanding variables.
 * wh = E_SHELL if expanding shell characters.
 * wh = E_TILDE if expanding `~'.
 * or any of these or'ed together.
 *
 * Major portions of this were snarfed from csh/sh.glob.c.
 */
struct namelist *
expand(list, wh)
	struct namelist *list;
	int wh;
{
	struct namelist *nl, *prev;
	int n;
	char *argvbuf[GAVSIZ];

	if (debug) {
		printf("expand(%lx, %d)\nlist = ", (long)list, wh);
		prnames(list);
	}

	if (wh == 0) {
		char *cp;

		for (nl = list; nl != NULL; nl = nl->n_next)
			for (cp = nl->n_name; *cp; cp++)
				*cp = *cp & TRIM;
		return(list);
	}

	which = wh;
	path = tpathp = pathp = pathbuf;
	*pathp = '\0';
	lastpathp = &path[sizeof pathbuf - 2];
	tilde = "";
	eargc = 0;
	eargv = sortbase = argvbuf;
	*eargv = 0;
	nleft = NCARGS - 4;
	/*
	 * Walk the name list and expand names into eargv[];
	 */
	for (nl = list; nl != NULL; nl = nl->n_next)
		expstr(nl->n_name);
	/*
	 * Take expanded list of names from eargv[] and build a new list.
	 */
	list = prev = NULL;
	for (n = 0; n < eargc; n++) {
		nl = makenl(NULL);
		nl->n_name = eargv[n];
		if (prev == NULL)
			list = prev = nl;
		else {
			prev->n_next = nl;
			prev = nl;
		}
	}
	if (debug) {
		printf("expanded list = ");
		prnames(list);
	}
	return(list);
}

static void
expstr(s)
	char *s;
{
	char *cp, *cp1;
	struct namelist *tp;
	char *tail;
	char buf[BUFSIZ];
	int savec, oeargc;
	extern char homedir[];

	if (s == NULL || *s == '\0')
		return;

	if ((which & E_VARS) && (cp = strchr(s, '$')) != NULL) {
		*cp++ = '\0';
		if (*cp == '\0') {
			yyerror("no variable name after '$'");
			return;
		}
		if (*cp == LC) {
			cp++;
			if ((tail = strchr(cp, RC)) == NULL) {
				yyerror("unmatched '{'");
				return;
			}
			*tail++ = savec = '\0';
			if (*cp == '\0') {
				yyerror("no variable name after '$'");
				return;
			}
		} else {
			tail = cp + 1;
			savec = *tail;
			*tail = '\0';
		}
		tp = lookup(cp, LOOKUP, 0);
		if (savec != '\0')
			*tail = savec;
		if (tp != NULL) {
			for (; tp != NULL; tp = tp->n_next) {
				snprintf(buf, sizeof(buf), "%s%s%s", s,
					tp->n_name, tail);
				expstr(buf);
			}
			return;
		}
		snprintf(buf, sizeof(buf), "%s%s", s, tail);
		expstr(buf);
		return;
	}
	if ((which & ~E_VARS) == 0 || !strcmp(s, "{") || !strcmp(s, "{}")) {
		Cat(s, "");
		sort();
		return;
	}
	if (*s == '~') {
		cp = ++s;
		if (*cp == '\0' || *cp == '/') {
			tilde = "~";
			cp1 = homedir;
		} else {
			tilde = cp1 = buf;
			*cp1++ = '~';
			do
				*cp1++ = *cp++;
			while (*cp && *cp != '/');
			*cp1 = '\0';
			if (pw == NULL || strcmp(pw->pw_name, buf+1) != 0) {
				if ((pw = getpwnam(buf+1)) == NULL) {
					strlcat(buf, ": unknown user name",
						sizeof buf);
					yyerror(buf+1);
					return;
				}
			}
			cp1 = pw->pw_dir;
			s = cp;
		}
		for (cp = path; *cp++ = *cp1++; )
			;
		tpathp = pathp = cp - 1;
	} else {
		tpathp = pathp = path;
		tilde = "";
	}
	*pathp = '\0';
	if (!(which & E_SHELL)) {
		if (which & E_TILDE)
			Cat(path, s);
		else
			Cat(tilde, s);
		sort();
		return;
	}
	oeargc = eargc;
	expany = 0;
	expsh(s);
	if (eargc == oeargc)
		Cat(s, "");		/* "nonomatch" is set */
	sort();
}

static int
argcmp(a1, a2)
	const void *a1, *a2;
{

	return (strcmp(*(char **)a1, *(char **)a2));
}

/*
 * If there are any Shell meta characters in the name,
 * expand into a list, after searching directory
 */
static void
expsh(s)
	char *s;
{
	char *cp;
	char *spathp, *oldcp;
	struct stat stb;

	spathp = pathp;
	cp = s;
	while (!any(*cp, shchars)) {
		if (*cp == '\0') {
			if (!expany || stat(path, &stb) >= 0) {
				if (which & E_TILDE)
					Cat(path, "");
				else
					Cat(tilde, tpathp);
			}
			goto endit;
		}
		addpath(*cp++);
	}
	oldcp = cp;
	while (cp > s && *cp != '/')
		cp--, pathp--;
	if (*cp == '/')
		cp++, pathp++;
	*pathp = '\0';
	if (*oldcp == '{') {
		execbrc(cp, NULL);
		return;
	}
	matchdir(cp);
endit:
	pathp = spathp;
	*pathp = '\0';
}

static void
matchdir(pattern)
	char *pattern;
{
	struct stat stb;
	struct direct *dp;
	DIR *dirp;

	dirp = opendir(path);
	if (dirp == NULL) {
		if (expany)
			return;
		goto patherr2;
	}
	if (fstat(dirfd(dirp), &stb) < 0)
		goto patherr1;
	if (!ISDIR(stb.st_mode)) {
		errno = ENOTDIR;
		goto patherr1;
	}
	while ((dp = readdir(dirp)) != NULL)
		if (match(dp->d_name, pattern)) {
			if (which & E_TILDE)
				Cat(path, dp->d_name);
			else {
				strlcpy(pathp, dp->d_name,
					pathbuf + sizeof pathbuf - pathp);
				Cat(tilde, tpathp);
				*pathp = '\0';
			}
		}
	closedir(dirp);
	return;

patherr1:
	closedir(dirp);
patherr2:
	strlcat(path, ": ", pathbuf + sizeof pathbuf - path);
	strlcat(path, strerror(errno), pathbuf + sizeof pathbuf - path);
	yyerror(path);
}

static int
execbrc(p, s)
	char *p, *s;
{
	char restbuf[BUFSIZ + 2];
	char *pe, *pm, *pl;
	int brclev = 0;
	char *lm, savec, *spathp;

	for (lm = restbuf; *p != '{'; *lm++ = *p++)
		continue;
	for (pe = ++p; *pe; pe++)
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
				yyerror("Missing ']'");
			continue;
		}
pend:
	if (brclev || !*pe) {
		yyerror("Missing '}'");
		return (0);
	}
	for (pl = pm = p; pm <= pe; pm++)
		switch (*pm & (QUOTE|TRIM)) {

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
			if (brclev)
				continue;
doit:
			savec = *pm;
			*pm = 0;
			strlcpy(lm, pl, restbuf + sizeof restbuf - lm);
			strlcat(restbuf, pe + 1, sizeof restbuf);
			*pm = savec;
			if (s == 0) {
				spathp = pathp;
				expsh(restbuf);
				pathp = spathp;
				*pathp = 0;
			} else if (amatch(s, restbuf))
				return (1);
			sort();
			pl = pm + 1;
			continue;

		case '[':
			for (pm++; *pm && *pm != ']'; pm++)
				continue;
			if (!*pm)
				yyerror("Missing ']'");
			continue;
		}
	return (0);
}

static int
match(s, p)
	char *s, *p;
{
	int c;
	char *sentp;
	char sexpany = expany;

	if (*s == '.' && *p != '.')
		return (0);
	sentp = entp;
	entp = s;
	c = amatch(s, p);
	entp = sentp;
	expany = sexpany;
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

	expany = 1;
	for (;;) {
		scc = *s++ & TRIM;
		switch (c = *p++) {

		case '{':
			return (execbrc(p - 1, s - 1));

		case '[':
			ok = 0;
			lc = 077777;
			while (cc = *p++) {
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
			if (cc == 0) {
				yyerror("Missing ']'");
				return (0);
			}
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

		case '\0':
			return (scc == '\0');

		default:
			if ((c & TRIM) != scc)
				return (0);
			continue;

		case '?':
			if (scc == '\0')
				return (0);
			continue;

		case '/':
			if (scc)
				return (0);
slash:
			s = entp;
			spathp = pathp;
			while (*s)
				addpath(*s++);
			addpath('/');
			if (stat(path, &stb) == 0 && ISDIR(stb.st_mode))
				if (*p == '\0') {
					if (which & E_TILDE)
						Cat(path, "");
					else
						Cat(tilde, tpathp);
				} else
					expsh(p);
			pathp = spathp;
			*pathp = '\0';
			return (0);
		}
	}
}

static int
smatch(s, p)
	char *s, *p;
{
	int scc;
	int ok, lc;
	int c, cc;

	for (;;) {
		scc = *s++ & TRIM;
		switch (c = *p++) {

		case '[':
			ok = 0;
			lc = 077777;
			while (cc = *p++) {
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
			if (cc == 0) {
				yyerror("Missing ']'");
				return (0);
			}
			continue;

		case '*':
			if (!*p)
				return (1);
			for (s--; *s; s++)
				if (smatch(s, p))
					return (1);
			return (0);

		case '\0':
			return (scc == '\0');

		default:
			if ((c & TRIM) != scc)
				return (0);
			continue;

		case '?':
			if (scc == 0)
				return (0);
			continue;

		}
	}
}

static void
Cat(s1, s2)
	char *s1, *s2;
{
	int len = strlen(s1) + strlen(s2) + 1;
	char *s;

	nleft -= len;
	if (nleft <= 0 || ++eargc >= GAVSIZ)
		yyerror("Arguments too long");
	eargv[eargc] = 0;
	eargv[eargc - 1] = s = malloc(len);
	if (s == NULL)
		fatal("ran out of memory\n");
	while (*s++ = *s1++ & TRIM)
		;
	s--;
	while (*s++ = *s2++ & TRIM)
		;
}

static void
addpath(c)
	int c;
{

	if (pathp >= lastpathp)
		yyerror("Pathname too long");
	else {
		*pathp++ = c & TRIM;
		*pathp = '\0';
	}
}

/*
 * Expand file names beginning with `~' into the
 * user's home directory path name. Return a pointer in buf to the
 * part corresponding to `file'.
 */
char *
exptilde(buf, file, maxlen)
	char buf[];
	char *file;
	int maxlen;
{
	char *s1, *s2, *s3;
	extern char homedir[];

	if (*file != '~') {
		strlcpy(buf, file, maxlen);
		return(buf);
	}
	if (*++file == '\0') {
		s2 = homedir;
		s3 = NULL;
	} else if (*file == '/') {
		s2 = homedir;
		s3 = file;
	} else {
		s3 = file;
		while (*s3 && *s3 != '/')
			s3++;
		if (*s3 == '/')
			*s3 = '\0';
		else
			s3 = NULL;
		if (pw == NULL || strcmp(pw->pw_name, file) != 0) {
			if ((pw = getpwnam(file)) == NULL) {
				error("%s: unknown user name\n", file);
				if (s3 != NULL)
					*s3 = '/';
				return(NULL);
			}
		}
		if (s3 != NULL)
			*s3 = '/';
		s2 = pw->pw_dir;
	}
	for (s1 = buf; (*s1++ = *s2++) && s1 < buf+maxlen; )
		;
	s2 = --s1;
	if (s3 != NULL && s1 < buf+maxlen) {
		s2++;
		while ((*s1++ = *s3++) && s1 < buf+maxlen)
			;
	}
	if (s1 == buf+maxlen)
		return (NULL);
	return(s2);
}
