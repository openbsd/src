/*      $Id: man_conf.c,v 1.1 2011/11/17 14:52:32 schwarze Exp $ */
/*
 * Copyright (c) 2011 Ingo Schwarze <schwarze@openbsd.org>
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

#include <sys/types.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "man_conf.h"

#define	MAN_CONF_FILE	"/etc/man.conf"

static void add_dir(struct man_conf *, const char *);

static void
add_dir(struct man_conf *dirs, const char *dir) {

	if (dirs->maxargs == dirs->argc) {
		dirs->maxargs += 8;
		assert(0 < dirs->maxargs);
		dirs->argv = mandoc_realloc(dirs->argv,
				(size_t)dirs->maxargs);
	}
	dirs->argv[dirs->argc++] = mandoc_strdup(dir);
}

void
manpath_parse(struct man_conf *dirs, char *path) {
	char	*dir;

	for (dir = strtok(path, ":"); dir; dir = strtok(NULL, ":"))
		add_dir(dirs, dir);
}

void
man_conf_parse(struct man_conf *dirs) {
	FILE	*stream;
	char	*p, *q;
	size_t	 len;

	if (NULL == (stream = fopen(MAN_CONF_FILE, "r")))
		return;

	while (NULL != (p = fgetln(stream, &len)) && '\n' == p[--len]) {
		p[len] = '\0';
		while (isspace(*p))
			p++;
		if (strncmp("_whatdb", p, 7))
			continue;
		p += 7;
		while (isspace(*p))
			p++;
		if ('\0' == *p)
			continue;
		if (NULL == (q = strrchr(p, '/')))
			continue;
		*q = '\0';
		add_dir(dirs, p);
	}

	fclose(stream);
}

void
man_conf_free(struct man_conf *dirs) {

	while (dirs->argc--)
		free(dirs->argv[dirs->argc]);
	free(dirs->argv);
	dirs->maxargs = 0;
}
