/*	$OpenBSD: util.c,v 1.13 2015/09/29 20:11:36 tedu Exp $	*/
/*
 * Copyright 2002 Niels Provos <provos@citi.umich.edu>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <err.h>

#include "util.h"

char *
strescape(char *str)
{
	static char escape[8192];
	int i, p;

	for (p = i = 0; i < strlen(str) && p < sizeof(escape) - 1; i++) {
		char a = str[i];
		switch (a) {
		case '\r':
			a = 'r';
			goto doescape;
		case '\n':
			a = 'n';
			goto doescape;
		case '\\':
		case '\"':
		doescape:
			escape[p++] = '\\';
			if (p >= sizeof(escape) - 1)
				errx(1, "%s: string too long: %s",
				    __func__, str);
			/* FALLTHROUGH */
		default:
			escape[p++] = a;
		}
	}

	escape[p] = '\0';
	return (escape);
}

char *
strrpl(char *str, size_t size, char *match, char *value)
{
	char *p, *e;
	int len, rlen;

	p = str;
	e = p + strlen(p);
	len = strlen(match);

	/* Try to match against the variable */
	while ((p = strchr(p, match[0])) != NULL) {
		if (!strncmp(p, match, len) && !isalnum((unsigned char)p[len]))
			break;
		p += len;

		if (p >= e)
			return (NULL);
	}

	if (p == NULL)
		return (NULL);

	rlen = strlen(value);

	if (strlen(str) - len + rlen > size)
		return (NULL);

	memmove(p + rlen, p + len, strlen(p + len) + 1);
	memcpy(p, value, rlen);

	return (p);
}

/* simplify_path is from pdksh and apparently in the public domain */

/* ISABSPATH() means path is fully and completely specified,
 * ISROOTEDPATH() means a .. as the first component is a no-op,
 * ISRELPATH() means $PWD can be tacked on to get an absolute path.
 *
 * OS		Path		ISABSPATH	ISROOTEDPATH	ISRELPATH
 * unix		/foo		yes		yes		no
 * unix		foo		no		no		yes
 * unix		../foo		no		no		yes
 * os2+cyg	a:/foo		yes		yes		no
 * os2+cyg	a:foo		no		no		no
 * os2+cyg	/foo		no		yes		no
 * os2+cyg	foo		no		no		yes
 * os2+cyg	../foo		no		no		yes
 * cyg 		//foo		yes		yes		no
 */
# define PATHSEP	':'
# define DIRSEP		'/'
# define DIRSEPSTR	"/"
# define ISDIRSEP(c)    ((c) == '/')
# define ISABSPATH(s)	ISDIRSEP((s)[0])
# define ISRELPATH(s)	(!ISABSPATH(s))
# define ISROOTEDPATH(s) ISABSPATH(s)
# define FILECHCONV(c)	c
# define FILECMP(s1, s2) strcmp(s1, s2)
# define FILENCMP(s1, s2, n) strncmp(s1, s2, n)
# define ksh_strchr_dirsep(p)   strchr(p, DIRSEP)
# define ksh_strrchr_dirsep(p)  strrchr(p, DIRSEP)

/* simplify_path is from pdksh */

/*
 * Simplify pathnames containing "." and ".." entries.
 * ie, simplify_path("/a/b/c/./../d/..") returns "/a/b"
 */
void
simplify_path(path)
	char	*path;
{
	char	*cur;
	char	*t;
	int	isrooted;
	char	*very_start = path;
	char	*start;

	if (!*path)
		return;

	if ((isrooted = ISROOTEDPATH(path)))
		very_start++;

	/* Before			After
	 *  /foo/			/foo
	 *  /foo/../../bar		/bar
	 *  /foo/./blah/..		/foo
	 *  .				.
	 *  ..				..
	 *  ./foo			foo
	 *  foo/../../../bar		../../bar
	 * OS2 and CYGWIN:
	 *  a:/foo/../..		a:/
	 *  a:.				a:
	 *  a:..			a:..
	 *  a:foo/../../blah		a:../blah
	 */


	for (cur = t = start = very_start; ; ) {
		/* treat multiple '/'s as one '/' */
		while (ISDIRSEP(*t))
			t++;

		if (*t == '\0') {
			if (cur == path)
				/* convert empty path to dot */
				*cur++ = '.';
			*cur = '\0';
			break;
		}

		if (t[0] == '.') {
			if (!t[1] || ISDIRSEP(t[1])) {
				t += 1;
				continue;
			} else if (t[1] == '.' && (!t[2] || ISDIRSEP(t[2]))) {
				if (!isrooted && cur == start) {
					if (cur != very_start)
						*cur++ = DIRSEP;
					*cur++ = '.';
					*cur++ = '.';
					start = cur;
				} else if (cur != start)
					while (--cur > start && !ISDIRSEP(*cur))
						;
				t += 2;
				continue;
			}
		}

		if (cur != very_start)
			*cur++ = DIRSEP;

		/* find/copy next component of pathname */
		while (*t && !ISDIRSEP(*t))
			*cur++ = *t++;
	}
}
