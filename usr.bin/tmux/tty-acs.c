/* $OpenBSD: tty-acs.c,v 1.6 2017/05/15 16:44:04 nicm Exp $ */

/*
 * Copyright (c) 2010 Nicholas Marriott <nicholas.marriott@gmail.com>
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

#include <stdlib.h>

#include "tmux.h"

static int	tty_acs_cmp(const void *, const void *);

/* Table mapping ACS entries to UTF-8. */
struct tty_acs_entry {
	u_char	 	 key;
	const char	*string;
};
static const struct tty_acs_entry tty_acs_table[] = {
	{ '+', "\342\206\222" },	/* arrow pointing right */
	{ ',', "\342\206\220" },	/* arrow pointing left */
	{ '-', "\342\206\221" },	/* arrow pointing up */
	{ '.', "\342\206\223" },	/* arrow pointing down */
	{ '0', "\342\226\256" },	/* solid square block */
	{ '`', "\342\227\206" },	/* diamond */
	{ 'a', "\342\226\222" },	/* checker board (stipple) */
	{ 'f', "\302\260" },		/* degree symbol */
	{ 'g', "\302\261" },		/* plus/minus */
	{ 'h', "\342\226\222" },	/* board of squares */
	{ 'i', "\342\230\203" },	/* lantern symbol */
	{ 'j', "\342\224\230" },	/* lower right corner */
	{ 'k', "\342\224\220" },	/* upper right corner */
	{ 'l', "\342\224\214" },	/* upper left corner */
	{ 'm', "\342\224\224" },	/* lower left corner */
	{ 'n', "\342\224\274" },	/* large plus or crossover */
	{ 'o', "\342\216\272" },	/* scan line 1 */
	{ 'p', "\342\216\273" },	/* scan line 3 */
	{ 'q', "\342\224\200" },	/* horizontal line */
	{ 'r', "\342\216\274" },	/* scan line 7 */
	{ 's', "\342\216\275" },	/* scan line 9 */
	{ 't', "\342\224\234" },	/* tee pointing right */
	{ 'u', "\342\224\244" },	/* tee pointing left */
	{ 'v', "\342\224\264" },	/* tee pointing up */
	{ 'w', "\342\224\254" },	/* tee pointing down */
	{ 'x', "\342\224\202" },	/* vertical line */
	{ 'y', "\342\211\244" },	/* less-than-or-equal-to */
	{ 'z', "\342\211\245" },	/* greater-than-or-equal-to */
	{ '{', "\317\200" },   		/* greek pi */
	{ '|', "\342\211\240" },	/* not-equal */
	{ '}', "\302\243" },		/* UK pound sign */
	{ '~', "\302\267" }		/* bullet */
};

static int
tty_acs_cmp(const void *key, const void *value)
{
	const struct tty_acs_entry	*entry = value;
	u_char				 ch;

	ch = *(u_char *) key;
	return (ch - entry->key);
}

/* Should this terminal use ACS instead of UTF-8 line drawing? */
int
tty_acs_needed(struct tty *tty)
{
	if (tty == NULL)
		return (0);

	/*
	 * If the U8 flag is present, it marks whether a terminal supports
	 * UTF-8 and ACS together.
	 *
	 * If it is present and zero, we force ACS - this gives users a way to
	 * turn off UTF-8 line drawing.
	 *
	 * If it is nonzero, we can fall through to the default and use UTF-8
	 * line drawing on UTF-8 terminals.
	 */
	if (tty_term_has(tty->term, TTYC_U8) &&
	    tty_term_number(tty->term, TTYC_U8) == 0)
		return (1);

	if (tty->flags & TTY_UTF8)
		return (0);
	return (1);
}

/* Retrieve ACS to output as a string. */
const char *
tty_acs_get(struct tty *tty, u_char ch)
{
	struct tty_acs_entry	*entry;

	/* Use the ACS set instead of UTF-8 if needed. */
	if (tty_acs_needed(tty)) {
		if (tty->term->acs[ch][0] == '\0')
			return (NULL);
		return (&tty->term->acs[ch][0]);
	}

	/* Otherwise look up the UTF-8 translation. */
	entry = bsearch(&ch, tty_acs_table, nitems(tty_acs_table),
	    sizeof tty_acs_table[0], tty_acs_cmp);
	if (entry == NULL)
		return (NULL);
	return (entry->string);
}
