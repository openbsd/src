/*	$OpenBSD: list.c,v 1.4 2010/06/27 20:00:58 phessler Exp $	*/

/*
 * Copyright (c) 2008 Martynas Venckus <martynas@openbsd.org>
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

#ifndef SMALL

#include <string.h>

void
parse_unix(char **line, char *type)
{
	char *tok;
	int field = 0;

	while ((tok = strsep(line, " \t")) != NULL) {
		if (*tok == '\0')
			continue;

		if (field == 0)
			*type = *tok;

		if (field == 7) {
			if (line == NULL || *line == NULL)
				break;
			while (**line == ' ' || **line == '\t')
				(*line)++;
			break;
		}

		field++;
	}
}

void
parse_windows(char **line, char *type)
{
	char *tok;
	int field = 0;

	*type = '-';
	while ((tok = strsep(line, " \t")) != NULL) {
		if (*tok == '\0')
			continue;

		if (field == 2 && strcmp(tok, "<DIR>") == 0)
			*type = 'd';

		if (field == 2) {
			if (line == NULL || *line == NULL)
				break;
			while (**line == ' ' || **line == '\t')
				(*line)++;
			break;
		}

		field++;
	}
}

void
parse_list(char **line, char *type)
{
	if (**line >= '0' && **line <= '9')
		return parse_windows(line, type);

	return parse_unix(line, type);
}

#endif /* !SMALL */

