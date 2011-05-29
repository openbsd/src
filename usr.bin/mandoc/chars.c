/*	$Id: chars.c,v 1.20 2011/05/29 21:26:57 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2010, 2011 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "libmandoc.h"

#define	PRINT_HI	 126
#define	PRINT_LO	 32

struct	ln {
	struct ln	 *next;
	const char	 *code;
	const char	 *ascii;
	int		  unicode;
};

#define	LINES_MAX	  325

#define CHAR(in, ch, code) \
	{ NULL, (in), (ch), (code) },

#define	CHAR_TBL_START	  static struct ln lines[LINES_MAX] = {
#define	CHAR_TBL_END	  };

#include "chars.in"

struct	mchars {
	struct ln	**htab;
};

static	inline int	  match(const struct ln *, const char *, size_t);
static	const struct ln	 *find(struct mchars *, const char *, size_t);

void
mchars_free(struct mchars *arg)
{

	free(arg->htab);
	free(arg);
}

struct mchars *
mchars_alloc(void)
{
	struct mchars	 *tab;
	struct ln	**htab;
	struct ln	 *pp;
	int		  i, hash;

	/*
	 * Constructs a very basic chaining hashtable.  The hash routine
	 * is simply the integral value of the first character.
	 * Subsequent entries are chained in the order they're processed
	 * (they're in-line re-ordered during lookup).
	 */

	tab = mandoc_malloc(sizeof(struct mchars));
	htab = mandoc_calloc(PRINT_HI - PRINT_LO + 1, sizeof(struct ln **));

	for (i = 0; i < LINES_MAX; i++) {
		hash = (int)lines[i].code[0] - PRINT_LO;

		if (NULL == (pp = htab[hash])) {
			htab[hash] = &lines[i];
			continue;
		}

		for ( ; pp->next; pp = pp->next)
			/* Scan ahead. */ ;
		pp->next = &lines[i];
	}

	tab->htab = htab;
	return(tab);
}


/* 
 * Special character to Unicode codepoint.
 */
int
mchars_spec2cp(struct mchars *arg, const char *p, size_t sz)
{
	const struct ln	*ln;

	ln = find(arg, p, sz);
	if (NULL == ln)
		return(-1);
	return(ln->unicode);
}

/*
 * Numbered character string to ASCII codepoint.
 * This can only be a printable character (i.e., alnum, punct, space) so
 * prevent the character from ruining our state (backspace, newline, and
 * so on).
 * If the character is illegal, returns '\0'.
 */
char
mchars_num2char(const char *p, size_t sz)
{
	int		  i;

	if ((i = mandoc_strntou(p, sz, 10)) < 0)
		return('\0');
	return(i > 0 && i < 256 && isprint(i) ? i : '\0');
}

/*
 * Hex character string to Unicode codepoint.
 * If the character is illegal, returns '\0'.
 */
int
mchars_num2uc(const char *p, size_t sz)
{
	int               i;

	if ((i = mandoc_strntou(p, sz, 16)) < 0)
		return('\0');
	/* FIXME: make sure we're not in a bogus range. */
	return(i > 0x80 && i <= 0x10FFFF ? i : '\0');
}

/* 
 * Special character to string array.
 */
const char *
mchars_spec2str(struct mchars *arg, const char *p, size_t sz, size_t *rsz)
{
	const struct ln	*ln;

	ln = find(arg, p, sz);
	if (NULL == ln) {
		*rsz = 1;
		return(NULL);
	}

	*rsz = strlen(ln->ascii);
	return(ln->ascii);
}

static const struct ln *
find(struct mchars *tab, const char *p, size_t sz)
{
	struct ln	 *pp, *prev;
	struct ln	**htab;
	int		  hash;

	assert(p);
	if (0 == sz)
		return(NULL);

	if (p[0] < PRINT_LO || p[0] > PRINT_HI)
		return(NULL);

	/*
	 * Lookup the symbol in the symbol hash.  See ascii2htab for the
	 * hashtable specs.  This dynamically re-orders the hash chain
	 * to optimise for repeat hits.
	 */

	hash = (int)p[0] - PRINT_LO;
	htab = tab->htab;

	if (NULL == (pp = htab[hash]))
		return(NULL);

	for (prev = NULL; pp; pp = pp->next) {
		if ( ! match(pp, p, sz)) {
			prev = pp;
			continue;
		}

		if (prev) {
			prev->next = pp->next;
			pp->next = htab[hash];
			htab[hash] = pp;
		}

		return(pp);
	}

	return(NULL);
}

static inline int
match(const struct ln *ln, const char *p, size_t sz)
{

	if (strncmp(ln->code, p, sz))
		return(0);
	return('\0' == ln->code[(int)sz]);
}
