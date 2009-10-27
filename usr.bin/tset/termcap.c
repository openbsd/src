/*	$OpenBSD: termcap.c,v 1.7 2009/10/27 23:59:46 deraadt Exp $	*/
/*	$NetBSD: termcap.c,v 1.7 1995/06/05 19:45:52 pk Exp $	*/

/*
 * Copyright (c) 1980, 1991, 1993
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

#define	PVECSIZ		32	/* max number of names in path */
#define	_PATH_DEF	".termcap /usr/share/misc/termcap"

#include <sys/param.h>
#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Get an entry for terminal name in buffer bp from the termcap file.
 */
int
tcgetent(bp, name)
	char *bp;
	const char *name;
{
	char  *p;
	char  *cp;
	char  *dummy;
	char **fname;
	char  *home;
	int    i;
	char   pathbuf[MAXPATHLEN];	/* holds raw path of filenames */
	char  *pathvec[PVECSIZ];	/* to point to names in pathbuf */
	char **pvec;			/* holds usable tail of path vector */
	char  *termpath;

	fname = pathvec;
	pvec = pathvec;

	if (!issetugid()) {
		cp = getenv("TERMCAP");
		/*
		 * TERMCAP can have one of two things in it. It can be the name
		 * of a file to use instead of /usr/share/misc/termcap. In this
		 * case it better start with a "/". Or it can be an entry to
		 * use so we don't have to read the file. In this case it
		 * has to already have the newlines crunched out.  If TERMCAP
		 * does not hold a file name then a path of names is searched
		 * instead.  The path is found in the TERMPATH variable, or
		 * becomes "$HOME/.termcap /usr/share/misc/termcap" if no
		 * TERMPATH exists.
		 */
		if (!cp || *cp != '/') { /* no TERMCAP or it holds an entry */
			if ((termpath = getenv("TERMPATH")) != NULL)
				strlcpy(pathbuf, termpath, sizeof(pathbuf));
			else {
				if ((home = getenv("HOME")) != NULL &&
				    *home != '\0' &&
				    strlen(home) + sizeof(_PATH_DEF) <
				    sizeof(pathbuf)) {
					snprintf(pathbuf, sizeof pathbuf,
					    "%s/%s", home, _PATH_DEF);
				} else {
					strlcpy(pathbuf, _PATH_DEF,
					    sizeof(pathbuf));
				}
			}
		} else {		/* user-defined path in TERMCAP */
			/* still can be tokenized */
			strlcpy(pathbuf, cp, sizeof(pathbuf));
		}
		*fname++ = pathbuf;	/* tokenize path into vector of names */
	}

	/* split pathbuf into a vector of paths */
	p = pathbuf;
	while (*++p)
		if (*p == ' ' || *p == ':') {
			*p = '\0';
			while (*++p)
				if (*p != ' ' && *p != ':')
					break;
			if (*p == '\0')
				break;
			*fname++ = p;
			if (fname >= pathvec + PVECSIZ) {
				fname--;
				break;
			}
		}
	*fname = (char *) 0;			/* mark end of vector */
	if (cp && *cp && *cp != '/')
		if (cgetset(cp) < 0)
			return (-2);

	dummy = NULL;
	i = cgetent(&dummy, pathvec, (char *)name);
	
	if (i == 0 && bp != NULL) {
		strlcpy(bp, dummy, 1024);
		if ((cp = strrchr(bp, ':')) != NULL)
			if (cp[1] != '\0')
				cp[1] = '\0';
	}
	else if (i == 0 && bp == NULL)
		bp = dummy;
	else if (dummy != NULL)
		free(dummy);

	/* no tc reference loop return code in libterm XXX */
	if (i == -3)
		return (-1);
	return (i + 1);
}

/*
 * Output termcap entry to stdout, quoting characters that would give the
 * shell problems and omitting empty fields.
 */
void
wrtermcap(bp)
	char *bp;
{
	int ch;
	char *p;
	char *t, *sep;

	/* Find the end of the terminal names. */
	if ((t = strchr(bp, ':')) == NULL)
		err(1, "termcap names not colon terminated");
	*t++ = '\0';

	/* Output terminal names that don't have whitespace. */
	sep = "";
	while ((p = strsep(&bp, "|")) != NULL)
		if (*p != '\0' && strpbrk(p, " \t") == NULL) {
			(void)printf("%s%s", sep, p);
			sep = "|";
		}
	(void)putchar(':');

	/*
	 * Output fields, transforming any dangerous characters.  Skip
	 * empty fields or fields containing only whitespace.
	 */
	while ((p = strsep(&t, ":")) != NULL) {
		while (isspace(*p))
			++p;
		if (*p == '\0')
			continue;
		while ((ch = *p++) != '\0')
			switch(ch) {
			case '\033':
				(void)printf("\\E");
			case  ' ':		/* No spaces. */
				(void)printf("\\040");
				break;
			case '!':		/* No csh history chars. */
				(void)printf("\\041");
				break;
			case ',':		/* No csh history chars. */
				(void)printf("\\054");
				break;
			case '"':		/* No quotes. */
				(void)printf("\\042");
				break;
			case '\'':		/* No quotes. */
				(void)printf("\\047");
				break;
			case '`':		/* No quotes. */
				(void)printf("\\140");
				break;
			case '\\':		/* Anything following is OK. */
			case '^':
				(void)putchar(ch);
				if ((ch = *p++) == '\0')
					break;
				/* FALLTHROUGH */
			default:
				(void)putchar(ch);
		}
		(void)putchar(':');
	}
}
