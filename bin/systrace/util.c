/*	$OpenBSD: util.c,v 1.9 2002/10/09 03:52:10 itojun Exp $	*/
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

#include "util.h"

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
		if (!strncmp(p, match, len) && !isalnum(p[len]))
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
#ifdef OS2
# define PATHSEP	';'
# define DIRSEP		'/'	/* even though \ is native */
# define DIRSEPSTR	"\\"
# define ISDIRSEP(c)    ((c) == '\\' || (c) == '/')
# define ISABSPATH(s)	(((s)[0] && (s)[1] == ':' && ISDIRSEP((s)[2])))
# define ISROOTEDPATH(s) (ISDIRSEP((s)[0]) || ISABSPATH(s))
# define ISRELPATH(s)	(!(s)[0] || ((s)[1] != ':' && !ISDIRSEP((s)[0])))
# define FILECHCONV(c)	(isascii(c) && isupper(c) ? tolower(c) : c)
# define FILECMP(s1, s2) stricmp(s1, s2)
# define FILENCMP(s1, s2, n) strnicmp(s1, s2, n)
extern char *ksh_strchr_dirsep(const char *path);
extern char *ksh_strrchr_dirsep(const char *path);
# define chdir		_chdir2
# define getcwd		_getcwd2
#else
# define PATHSEP	':'
# define DIRSEP		'/'
# define DIRSEPSTR	"/"
# define ISDIRSEP(c)    ((c) == '/')
#ifdef __CYGWIN__
#  define ISABSPATH(s) \
	(((s)[0] && (s)[1] == ':' && ISDIRSEP((s)[2])) || ISDIRSEP((s)[0]))
#  define ISRELPATH(s) (!(s)[0] || ((s)[1] != ':' && !ISDIRSEP((s)[0])))
#else /* __CYGWIN__ */
# define ISABSPATH(s)	ISDIRSEP((s)[0])
# define ISRELPATH(s)	(!ISABSPATH(s))
#endif /* __CYGWIN__ */
# define ISROOTEDPATH(s) ISABSPATH(s)
# define FILECHCONV(c)	c
# define FILECMP(s1, s2) strcmp(s1, s2)
# define FILENCMP(s1, s2, n) strncmp(s1, s2, n)
# define ksh_strchr_dirsep(p)   strchr(p, DIRSEP)
# define ksh_strrchr_dirsep(p)  strrchr(p, DIRSEP)
#endif

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
#if defined (OS2) || defined (__CYGWIN__)
	if (path[0] && path[1] == ':')	/* skip a: */
		very_start += 2;
#endif /* OS2 || __CYGWIN__ */

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

#ifdef __CYGWIN__
	/* preserve leading double-slash on pathnames (for UNC paths) */
	if (path[0] && ISDIRSEP(path[0]) && path[1] && ISDIRSEP(path[1]))
		very_start++;
#endif /* __CYGWIN__ */

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
