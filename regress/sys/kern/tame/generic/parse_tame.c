/*	$OpenBSD: parse_tame.c,v 1.1 2015/08/24 09:21:10 semarie Exp $ */
/*
 * Copyright (c) 2015 Sebastien Marie <semarie@openbsd.org>
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

#include <sys/tame.h>

#include <err.h>
#include <stdlib.h>
#include <string.h>

#define tameflag(x) { #x, x }

/* list of defined TAME_ flags */
struct {
	char *name;
	int flag;
} flags_list[] = {
	tameflag(TAME_MALLOC),
	tameflag(TAME_RW),
	tameflag(TAME_STDIO),
	tameflag(TAME_RPATH),
	tameflag(TAME_WPATH),
	tameflag(TAME_TMPPATH),
	tameflag(TAME_INET),
	tameflag(TAME_UNIX),
	tameflag(TAME_CMSG),
	tameflag(TAME_DNS),
	tameflag(TAME_IOCTL),
	tameflag(TAME_GETPW),
	tameflag(TAME_PROC),
	tameflag(TAME_CPATH),
	tameflag(TAME_ABORT),
	{ NULL, 0 },
};


int
parse_flags(char *str)
{
	int flags = 0;
	char *current = str;
	char *next = str;
	int i;

	if (str == NULL || *str == '\0')
		return (0);

	while (next) {
		/* get only the current word */
		next = strchr(current, ',');
		if (next == '\0')
			next = NULL;
		else
			*next = '\0';

		/* search word in flags_list */
		for (i = 0; (flags_list[i].name != NULL)
		    && (strcmp(current, flags_list[i].name) != 0); i++);

		if (flags_list[i].name != NULL) {
			if (flags & flags_list[i].flag) 
				errx(1, "parse_flags: flag already setted: %s",
				    flags_list[i].name);
			else
				flags |= flags_list[i].flag;
		} else
			errx(1, "parse_flags: unknown flag: %s", current);

		/* advance to next word */
		if (next)
			current = next + 1;
	}

	return (flags);
}
