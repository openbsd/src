/*	$NetBSD: shlib.c,v 1.13 1998/04/04 01:00:29 fvdl Exp $	*/

/*
 * Copyright (c) 1993 Paul Kranenburg
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
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 */

#ifdef sun
char	*strsep();
int	isdigit();
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <a.out.h>
#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ld.h"

/*
 * Standard directories to search for files specified by -l.
 */
#ifndef STANDARD_SEARCH_DIRS
#define	STANDARD_SEARCH_DIRS	"/usr/lib"
#endif

/*
 * Actual vector of library search directories,
 * including `-L'ed and LD_LIBRARY_PATH spec'd ones.
 */
char	 **search_dirs;
int	n_search_dirs;

char	*standard_search_dirs[] = {
	STANDARD_SEARCH_DIRS
};


void
add_search_dir(name)
	char	*name;
{
	n_search_dirs++;
	search_dirs = (char **)
		xrealloc(search_dirs, n_search_dirs * sizeof search_dirs[0]);
	search_dirs[n_search_dirs - 1] = strdup(name);
}

void
remove_search_dir(name)
	char	*name;
{
	int	n;

	for (n = 0; n < n_search_dirs; n++) {
		if (strcmp(search_dirs[n], name))
			continue;
		free(search_dirs[n]);
		if (n < (n_search_dirs - 1))
			bcopy(&search_dirs[n+1], &search_dirs[n],
			      (n_search_dirs - n - 1) * sizeof search_dirs[0]);
		n_search_dirs--;
	}
}

void
add_search_path(path)
char	*path;
{
	register char	*cp, *dup;

	if (path == NULL)
		return;

	/* Add search directories from `path' */
	path = dup = strdup(path);
	while ((cp = strsep(&path, ":")) != NULL)
		add_search_dir(cp);
	free(dup);
}

void
remove_search_path(path)
char	*path;
{
	register char	*cp, *dup;

	if (path == NULL)
		return;

	/* Remove search directories from `path' */
	path = dup = strdup(path);
	while ((cp = strsep(&path, ":")) != NULL)
		remove_search_dir(cp);
	free(dup);
}

void
std_search_path()
{
	int	i, n;

	/* Append standard search directories */
	n = sizeof standard_search_dirs / sizeof standard_search_dirs[0];
	for (i = 0; i < n; i++)
		add_search_dir(standard_search_dirs[i]);
}

/*
 * Return true if CP points to a valid dewey number.
 * Decode and leave the result in the array DEWEY.
 * Return the number of decoded entries in DEWEY.
 */

int
getdewey(dewey, cp)
int	dewey[];
char	*cp;
{
	int	i, n;

	for (n = 0, i = 0; i < MAXDEWEY; i++) {
		if (*cp == '\0')
			break;

		if (*cp == '.') cp++;
#ifdef SUNOS_LIB_COMPAT
		if (!(isdigit)(*cp))
#else
		if (!isdigit(*cp))
#endif
			return 0;

		dewey[n++] = strtol(cp, &cp, 10);
	}

	return n;
}

/*
 * Compare two dewey arrays.
 * Return -1 if `d1' represents a smaller value than `d2'.
 * Return  1 if `d1' represents a greater value than `d2'.
 * Return  0 if equal.
 */
int
cmpndewey(d1, n1, d2, n2)
int	d1[], d2[];
int	n1, n2;
{
	register int	i;

	for (i = 0; i < n1 && i < n2; i++) {
		if (d1[i] < d2[i])
			return -1;
		if (d1[i] > d2[i])
			return 1;
	}

	if (n1 == n2)
		return 0;

	if (i == n1)
		return -1;

	if (i == n2)
		return 1;

	errx(1, "cmpndewey: cant happen");
	return 0;
}

/*
 * Search directories for a shared library matching the given
 * major and minor version numbers.
 *
 * MAJOR == -1 && MINOR == -1	--> find highest version
 * MAJOR != -1 && MINOR == -1   --> find highest minor version
 * MAJOR == -1 && MINOR != -1   --> invalid
 * MAJOR != -1 && MINOR != -1   --> find highest micro version
 */

/* Not interested in devices right now... */
#undef major
#undef minor

char *
findshlib(name, majorp, minorp, do_dot_a)
char	*name;
int	*majorp, *minorp;
int	do_dot_a;
{
	int		dewey[MAXDEWEY];
	int		ndewey;
	int		tmp[MAXDEWEY];
	int		i;
	int		len;
	char		*lname, *path = NULL;
	int		major = *majorp, minor = *minorp;

	len = strlen(name);
	lname = (char *)alloca(len + sizeof("lib"));
	sprintf(lname, "lib%s", name);
	len += 3;

	ndewey = 0;

	for (i = 0; i < n_search_dirs; i++) {
		DIR		*dd = opendir(search_dirs[i]);
		struct dirent	*dp;
		int		found_dot_a = 0;
		int		might_take_it;

		if (dd == NULL)
			continue;

		might_take_it = 0;
		while ((dp = readdir(dd)) != NULL) {
			int	n;

			if (do_dot_a && path == NULL &&
					dp->d_namlen == len + 2 &&
					strncmp(dp->d_name, lname, len) == 0 &&
					(dp->d_name+len)[0] == '.' &&
					(dp->d_name+len)[1] == 'a') {

				path = concat(search_dirs[i], "/", dp->d_name);
				found_dot_a = 1;
			}

			if (dp->d_namlen < len + 4)
				continue;
			if (strncmp(dp->d_name, lname, len) != 0)
				continue;
			if (strncmp(dp->d_name+len, ".so.", 4) != 0)
				continue;

			if ((n = getdewey(tmp, dp->d_name+len+4)) == 0)
				continue;

			if (major != -1 && found_dot_a) { /* XXX */
				free(path);
				path = NULL;
				found_dot_a = 0;
			}

			if (major == -1 && minor == -1) {
				might_take_it = 1;
			} else if (major != -1 && minor == -1) {
				if (tmp[0] == major)
					might_take_it = 1;
			} else if (major != -1 && minor != -1) {
				if (tmp[0] == major)
					if (n == 1 || tmp[1] >= minor)
						might_take_it = 1;
			}

			if (!might_take_it)
				continue;

			if (cmpndewey(tmp, n, dewey, ndewey) <= 0)
				continue;

			/* We have a better version */
			if (path)
				free(path);
			path = concat(search_dirs[i], "/", dp->d_name);
			found_dot_a = 0;
			bcopy(tmp, dewey, sizeof(dewey));
			ndewey = n;
			*majorp = dewey[0];
			*minorp = dewey[1];
		}
		closedir(dd);

		if (found_dot_a || might_take_it)
			/*
			 * There's a lib in this dir; take it.
			 */
			return path;
	}

	return path;
}
