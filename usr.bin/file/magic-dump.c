/* $OpenBSD: magic-dump.c,v 1.1 2015/04/24 16:24:11 nicm Exp $ */

/*
 * Copyright (c) 2015 Nicholas Marriott <nicm@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>

#include <stdio.h>

#include "magic.h"

static void
magic_dump_line(struct magic_line *ml, u_int depth)
{
	struct magic_line	*child;
	u_int			 i;

	printf("%u", ml->line);
	for (i = 0; i < depth; i++)
		printf(">");
	printf(" %s/%s%s%s%s [%u]%s\n", ml->type_string,
	    ml->result == NULL ? "" : ml->result,
	    ml->mimetype == NULL ? "" : " (",
	    ml->mimetype == NULL ? "" : ml->mimetype,
	    ml->mimetype == NULL ? "" : ")",
	    ml->strength, ml->text ? " (text)" : "");

	TAILQ_FOREACH(child, &ml->children, entry)
		magic_dump_line(child, depth + 1);

}

void
magic_dump(struct magic *m)
{
	struct magic_line	*ml;

	RB_FOREACH(ml, magic_tree, &m->tree)
		magic_dump_line(ml, 0);
}
