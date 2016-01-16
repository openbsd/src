/*
 * Copyright (c) 2016 Ingo Schwarze <schwarze@openbsd.org>
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

/*
 * Glue for unit tests of ksh(1) vi line editing mode.
 * Takes input keystrokes from stdin.
 * Only checks the final content of the line buffer
 * and the bytes sent down to edit.c and shf.c,
 * but not the operation of those low-level output routines.
 */

#include <err.h>
#include <stdio.h>
#include <string.h>

#include "edit.h"
#include "sh.h"

/* sh.h version.c */
const char ksh_version[] = "@(#)PD KSH v5.2.14 99/07/13.2";

/* table.h table.c */
const char *prompt = " $ ";

/* lex.h lex.c */
static struct source __source;
struct source *source = &__source;

/* sh.h history.c */
static char *history = NULL;

/* edit.h edit.c */
X_chars edchars = { 0x7f, 0x15, 0x17, 0x03, 0x1c, 0x04 };

int
main(void)
{
	char	 buf[2048];	/* vi.c CMDLEN */
	int	 len;

	if ((len = x_vi(buf, sizeof(buf))) == -1)
		errx(1, "x_vi failed");

	buf[len] = '\0';
	fputs(buf, stdout);

	return 0;
}

/* edit.h lex.c, used in edit_reset() */
int
promptlen(const char *cp, const char **spp)
{
	*spp = cp;
	return strlen(cp);
}

/* lex.h lex.c, used in vi_pprompt() */
void
pprompt(const char *cp, int ntruncate)
{
	while (ntruncate-- > 0 && *cp != '\0')
		cp++;
	fputs(cp, stdout);
}

/* sh.h history.c, used in vi_cmd() grabhist() grabsearch() */
char **
histpos(void)
{
	return &history;
}
