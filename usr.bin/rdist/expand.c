/*	$OpenBSD: expand.c,v 1.12 2009/10/27 23:59:42 deraadt Exp $	*/

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
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

#define	MAXEARGS	2048
#define LC 		'{'
#define RC 		'}'

static char	shchars[] = "${[*?";

int		which;		/* bit mask of types to expand */
int		eargc;		/* expanded arg count */
char	      **eargv;		/* expanded arg vectors */
char	       *path;
char	       *pathp;
char	       *lastpathp;
char	       *tilde;		/* "~user" if not expanding tilde, else "" */
char	       *tpathp;

int		expany;		/* any expansions done? */
char	       *entp;
char	      **sortbase;
char 	       *argvbuf[MAXEARGS];

#define sort()	qsort((char *)sortbase, &eargv[eargc] - sortbase, \
		      sizeof(*sortbase), argcmp), sortbase = &eargv[eargc]

static void Cat(u_char *, u_char *);
static void addpath(int);
static int argcmp(const void *, const void *);

static void
Cat(u_char *s1, u_char *s2)		/* quote in s1 and s2 */
{
	char *cp;
	int len = strlen((char *)s1) + strlen((char *)s2) + 2;

	if ((eargc + 1) >= MAXEARGS) {
		yyerror("Too many names");
		return;
	}

	eargv[++eargc] = NULL;
	eargv[eargc - 1] = cp = xmalloc(len);

	do { 
		if (*s1 == QUOTECHAR) 
			s1++; 
	} while ((*cp++ = *s1++) != '\0');
	cp--;
	do { 
		if (*s2 == QUOTECHAR) 
			s2++; 
	} while ((*cp++ = *s2++) != '\0');
}

static void
addpath(int c)
{
	if (pathp >= lastpathp) {
		yyerror("Pathname too long");
		return;
	} else {
		*pathp++ = c;
		*pathp = CNULL;
	}
}

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
expand(struct namelist *list, int wh)		/* quote in list->n_name */
{
	struct namelist *nl, *prev;
	int n;
	char pathbuf[BUFSIZ];

	if (debug)
		debugmsg(DM_CALL, "expand(%x, %d) start, list = %s", 
			 list, wh, getnlstr(list));

	if (wh == 0)
		fatalerr("expand() contains invalid 'wh' argument.");

	which = wh;
	path = tpathp = pathp = pathbuf;
	*pathp = CNULL;
	lastpathp = &pathbuf[sizeof pathbuf - 2];
	tilde = "";
	eargc = 0;
	eargv = sortbase = argvbuf;
	*eargv = NULL;

	/*
	 * Walk the name list and expand names into eargv[];
	 */
	for (nl = list; nl != NULL; nl = nl->n_next)
		expstr((u_char *)nl->n_name);
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

	return(list);
}

/*
 * xstrchr() is a version of strchr() that
 * handles u_char buffers.
 */
u_char *
xstrchr(u_char *str, int ch)
{
	u_char *cp;

	for (cp = str; cp && *cp != CNULL; ++cp)
		if (ch == *cp)
			return(cp);

	return(NULL);
}

void
expstr(u_char *s)
{
	u_char *cp, *cp1;
	struct namelist *tp;
	u_char *tail;
	u_char ebuf[BUFSIZ];
	u_char varbuff[BUFSIZ];
	int savec, oeargc;
	extern char *homedir;

	if (s == NULL || *s == CNULL)
		return;

	/*
	 * Remove quoted characters
	 */
	if (IS_ON(which, E_VARS)) {
		if (strlen((char *)s) > sizeof(varbuff)) {
			yyerror("Variable is too large.");
			return;
		}
		for (cp = s, cp1 = varbuff; cp && *cp; ++cp) {
			/* 
			 * remove quoted character if the next
			 * character is not $
			 */
			if (*cp == QUOTECHAR && *(cp+1) != '$')
				++cp;
			else
				*cp1++ = *cp;
		}
		*cp1 = CNULL;
		s = varbuff;
	}

	/*
	 * Consider string 's' a variable that should be expanded if
	 * there is a '$' in 's' that is not quoted.
	 */
	if (IS_ON(which, E_VARS) && 
	    ((cp = xstrchr(s, '$')) && !(cp > s && *(cp-1) == QUOTECHAR))) {
		*cp++ = CNULL;
		if (*cp == CNULL) {
			yyerror("no variable name after '$'");
			return;
		}
		if (*cp == LC) {
			cp++;
			for (cp1 = cp; ; cp1 = tail + 1) {
				if ((tail = xstrchr(cp1, RC)) == NULL) {
					yyerror("unmatched '{'");
					return;
				}
				if (tail[-1] != QUOTECHAR) break;
			}
			*tail++ = savec = CNULL;
			if (*cp == CNULL) {
				yyerror("no variable name after '$'");
				return;
			}
		} else {
			tail = cp + 1;
			savec = *tail;
			*tail = CNULL;
		}
		tp = lookup((char *)cp, LOOKUP, NULL);
		if (savec != CNULL)
			*tail = savec;
		if (tp != NULL) {
			for (; tp != NULL; tp = tp->n_next) {
				(void) snprintf((char *)ebuf, sizeof(ebuf),
					        "%s%s%s", s, tp->n_name, tail);
				expstr(ebuf);
			}
			return;
		}
		(void) snprintf((char *)ebuf, sizeof(ebuf), "%s%s", s, tail);
		expstr(ebuf);
		return;
	}
	if ((which & ~E_VARS) == 0 || !strcmp((char *)s, "{") || 
	    !strcmp((char *)s, "{}")) {
		Cat(s, (u_char *)"");
		sort();
		return;
	}
	if (*s == '~') {
		cp = ++s;
		if (*cp == CNULL || *cp == '/') {
			tilde = "~";
			cp1 = (u_char *)homedir;
		} else {
			tilde = (char *)(cp1 = ebuf);
			*cp1++ = '~';
			do
				*cp1++ = *cp++;
			while (*cp && *cp != '/');
			*cp1 = CNULL;
			if (pw == NULL || strcmp(pw->pw_name, 
						 (char *)ebuf+1) != 0) {
				if ((pw = getpwnam((char *)ebuf+1)) == NULL) {
					strlcat((char *)ebuf, 
					        ": unknown user name",
					        sizeof(ebuf));
					yyerror((char *)ebuf+1);
					return;
				}
			}
			cp1 = (u_char *)pw->pw_dir;
			s = cp;
		}
		for (cp = (u_char *)path; (*cp++ = *cp1++) != '\0'; )
			continue;
		tpathp = pathp = (char *)cp - 1;
	} else {
		tpathp = pathp = path;
		tilde = "";
	}
	*pathp = CNULL;
	if (!(which & E_SHELL)) {
		if (which & E_TILDE)
			Cat((u_char *)path, s);
		else
			Cat((u_char *)tilde, s);
		sort();
		return;
	}
	oeargc = eargc;
	expany = 0;
	expsh(s);
	if (eargc == oeargc)
		Cat(s, (u_char *)"");		/* "nonomatch" is set */
	sort();
}

static int
argcmp(const void *v1, const void *v2)
{
	const char *const *a1 = v1, *const *a2 = v2;

	return (strcmp(*a1, *a2));
}

/*
 * If there are any Shell meta characters in the name,
 * expand into a list, after searching directory
 */
void
expsh(u_char *s)			/* quote in s */
{
	u_char *cp, *oldcp;
	char *spathp;
	struct stat stb;

	spathp = pathp;
	cp = s;
	while (!any(*cp, shchars)) {
		if (*cp == CNULL) {
			if (!expany || stat(path, &stb) >= 0) {
				if (which & E_TILDE)
					Cat((u_char *)path, (u_char *)"");
				else
					Cat((u_char *)tilde, (u_char *)tpathp);
			}
			goto endit;
		}
		if (*cp == QUOTECHAR) cp++;
		addpath(*cp++);
	}
	oldcp = cp;
	while (cp > s && *cp != '/')
		cp--, pathp--;
	if (*cp == '/')
		cp++, pathp++;
	*pathp = CNULL;
	if (*oldcp == '{') {
		(void) execbrc(cp, NULL);
		return;
	}
	matchdir((char *)cp);
endit:
	pathp = spathp;
	*pathp = CNULL;
}

void
matchdir(char *pattern)			/* quote in pattern */
{
	struct stat stb;
	DIRENTRY *dp;
	DIR *dirp;

	dirp = opendir(path);
	if (dirp == NULL) {
		if (expany)
			return;
		goto patherr2;
	}
	if (fstat(dirfd(dirp), &stb) < 0)
		goto patherr1;
	if (!S_ISDIR(stb.st_mode)) {
		errno = ENOTDIR;
		goto patherr1;
	}
	while ((dp = readdir(dirp)) != NULL)
		if (match(dp->d_name, pattern)) {
			if (which & E_TILDE)
				Cat((u_char *)path, (u_char *)dp->d_name);
			else {
				(void) strlcpy(pathp, dp->d_name,
				    lastpathp - pathp + 2);
				Cat((u_char *)tilde, (u_char *)tpathp);
				*pathp = CNULL;
			}
		}
	closedir(dirp);
	return;

patherr1:
	closedir(dirp);
patherr2:
	(void) strlcat(path, ": ", lastpathp - path + 2);
	(void) strlcat(path, SYSERR, lastpathp - path + 2);
	yyerror(path);
}

int
execbrc(u_char *p, u_char *s)		/* quote in p */
{
	u_char restbuf[BUFSIZ + 2];
	u_char *pe, *pm, *pl;
	int brclev = 0;
	u_char *lm, savec;
	char *spathp;

	for (lm = restbuf; *p != '{'; *lm++ = *p++)
		if (*p == QUOTECHAR) *lm++ = *p++;

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
				if (*p == QUOTECHAR) pe++;
			if (!*pe)
				yyerror("Missing ']'");
			continue;

		case QUOTECHAR:		/* skip this character */
			pe++;
			continue;
		}
pend:
	if (brclev || !*pe) {
		yyerror("Missing '}'");
		return (0);
	}
	for (pl = pm = p; pm <= pe; pm++)
		/* the strip code was a noop */
		switch (*pm) {

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
			*lm = 0;
			(void) strlcat((char *)restbuf, (char *)pl,
			    sizeof(restbuf));
			(void) strlcat((char *)restbuf, (char *)pe + 1,
			    sizeof(restbuf));
			*pm = savec;
			if (s == 0) {
				spathp = pathp;
				expsh(restbuf);
				pathp = spathp;
				*pathp = 0;
			} else if (amatch((char *)s, restbuf))
				return (1);
			sort();
			pl = pm + 1;
			continue;

		case '[':
			for (pm++; *pm && *pm != ']'; pm++)
				if (*pm == QUOTECHAR) pm++;
			if (!*pm)
				yyerror("Missing ']'");
			continue;

		case QUOTECHAR:			/* skip one character */
			pm++;
			continue;
		}
	return (0);
}

int
match(char *s, char *p)				/* quote in p */
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

int
amatch(char *s, u_char *p)			/* quote in p */
{
	int scc;
	int ok, lc;
	char *spathp;
	struct stat stb;
	int c, cc;

	expany = 1;
	for (;;) {
		scc = *s++;
		switch (c = *p++) {

		case '{':
			return (execbrc((u_char *)p - 1, (u_char *)s - 1));

		case '[':
			ok = 0;
			lc = 077777;
			while ((cc = *p++) != '\0') {
				if (cc == ']') {
					if (ok)
						break;
					return (0);
				}
				if (cc == QUOTECHAR) cc = *p++;
				if (cc == '-') {
					if (lc <= scc && scc <= (int)*p++)
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

		case CNULL:
			return (scc == CNULL);

		default:
			if (c != scc)
				return (0);
			continue;

		case '?':
			if (scc == CNULL)
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
			if (stat(path, &stb) == 0 && S_ISDIR(stb.st_mode)) {
				if (*p == CNULL) {
					if (which & E_TILDE) {
						Cat((u_char *)path, 
						    (u_char *)"");
					} else {
						Cat((u_char *)tilde, 
						    (u_char *)tpathp);
					}
				} else
					expsh(p);
			}
			pathp = spathp;
			*pathp = CNULL;
			return (0);
		}
	}
}
