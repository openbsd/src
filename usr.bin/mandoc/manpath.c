/*	$Id: manpath.c,v 1.3 2011/12/19 02:26:33 schwarze Exp $ */
/*
 * Copyright (c) 2011 Ingo Schwarze <schwarze@openbsd.org>
 * Copyright (c) 2011 Kristaps Dzonsons <kristaps@bsd.lv>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "manpath.h"

#define MAN_CONF_FILE	"/etc/man.conf"
#define MAN_CONF_KEY	"_whatdb"

static	void	 manpath_add(struct manpaths *, const char *);
static	void	 manpath_parseline(struct manpaths *, char *);

void
manpath_parse(struct manpaths *dirs, const char *file,
		char *defp, char *auxp)
{

	manpath_parseline(dirs, auxp);

	if (NULL == defp)
		defp = getenv("MANPATH");

	if (NULL == defp)
		manpath_manconf(dirs, file ? file : MAN_CONF_FILE);
	else
		manpath_parseline(dirs, defp);
}

/*
 * Parse a FULL pathname from a colon-separated list of arrays.
 */
static void
manpath_parseline(struct manpaths *dirs, char *path)
{
	char	*dir;

	if (NULL == path)
		return;

	for (dir = strtok(path, ":"); dir; dir = strtok(NULL, ":"))
		manpath_add(dirs, dir);
}

/*
 * Add a directory to the array, ignoring bad directories.
 * Grow the array one-by-one for simplicity's sake.
 */
static void
manpath_add(struct manpaths *dirs, const char *dir)
{
	char		 buf[PATH_MAX];
	char		*cp;
	int		 i;

	if (NULL == (cp = realpath(dir, buf)))
		return;

	for (i = 0; i < dirs->sz; i++)
		if (0 == strcmp(dirs->paths[i], dir))
			return;

	dirs->paths = mandoc_realloc
		(dirs->paths,
		 ((size_t)dirs->sz + 1) * sizeof(char *));

	dirs->paths[dirs->sz++] = mandoc_strdup(cp);
}

void
manpath_free(struct manpaths *p)
{
	int		 i;

	for (i = 0; i < p->sz; i++)
		free(p->paths[i]);

	free(p->paths);
}

void
manpath_manconf(struct manpaths *dirs, const char *file)
{
	FILE		*stream;
	char		*p, *q;
	size_t		 len, keysz;

	keysz = strlen(MAN_CONF_KEY);
	assert(keysz > 0);

	if (NULL == (stream = fopen(file, "r")))
		return;

	while (NULL != (p = fgetln(stream, &len))) {
		if (0 == len || '\n' != p[--len])
			break;
		p[len] = '\0';
		while (isspace((unsigned char)*p))
			p++;
		if (strncmp(MAN_CONF_KEY, p, keysz))
			continue;
		p += keysz;
		while (isspace(*p))
			p++;
		if ('\0' == *p)
			continue;
		if (NULL == (q = strrchr(p, '/')))
			continue;
		*q = '\0';
		manpath_add(dirs, p);
	}

	fclose(stream);
}
