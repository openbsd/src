/*	$Id: chars.c,v 1.18 2011/04/24 16:22:02 schwarze Exp $ */
/*
 * Copyright (c) 2009, 2010 Kristaps Dzonsons <kristaps@bsd.lv>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mandoc.h"
#include "out.h"

#define	PRINT_HI	 126
#define	PRINT_LO	 32

struct	ln {
	struct ln	 *next;
	const char	 *code;
	const char	 *ascii;
	int		  unicode;
	int		  type;
#define	CHARS_CHAR	 (1 << 0)
#define	CHARS_STRING	 (1 << 1)
#define CHARS_BOTH	 (CHARS_CHAR | CHARS_STRING)
};

#define	LINES_MAX	  353

#define CHAR(in, ch, code) \
	{ NULL, (in), (ch), (code), CHARS_CHAR },
#define STRING(in, ch, code) \
	{ NULL, (in), (ch), (code), CHARS_STRING },
#define BOTH(in, ch, code) \
	{ NULL, (in), (ch), (code), CHARS_BOTH },

#define	CHAR_TBL_START	  static struct ln lines[LINES_MAX] = {
#define	CHAR_TBL_END	  };

#include "chars.in"

struct	ctab {
	enum chars	  type;
	struct ln	**htab;
};

static	inline int	  match(const struct ln *,
				const char *, size_t, int);
static	const struct ln	 *find(struct ctab *, const char *, size_t, int);


void
chars_free(void *arg)
{
	struct ctab	*tab;

	tab = (struct ctab *)arg;

	free(tab->htab);
	free(tab);
}


void *
chars_init(enum chars type)
{
	struct ctab	 *tab;
	struct ln	**htab;
	struct ln	 *pp;
	int		  i, hash;

	/*
	 * Constructs a very basic chaining hashtable.  The hash routine
	 * is simply the integral value of the first character.
	 * Subsequent entries are chained in the order they're processed
	 * (they're in-line re-ordered during lookup).
	 */

	tab = mandoc_malloc(sizeof(struct ctab));
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
	tab->type = type;
	return(tab);
}


/* 
 * Special character to Unicode codepoint.
 */
int
chars_spec2cp(void *arg, const char *p, size_t sz)
{
	const struct ln	*ln;

	ln = find((struct ctab *)arg, p, sz, CHARS_CHAR);
	if (NULL == ln)
		return(-1);
	return(ln->unicode);
}


/* 
 * Reserved word to Unicode codepoint.
 */
int
chars_res2cp(void *arg, const char *p, size_t sz)
{
	const struct ln	*ln;

	ln = find((struct ctab *)arg, p, sz, CHARS_STRING);
	if (NULL == ln)
		return(-1);
	return(ln->unicode);
}


/*
 * Numbered character to literal character,
 * represented as a null-terminated string for additional safety.
 */
const char *
chars_num2char(const char *p, size_t sz)
{
	int		  i;
	static char	  c[2];

	if (sz > 3)
		return(NULL);
	i = atoi(p);
	if (i < 0 || i > 255)
		return(NULL);
	c[0] = (char)i;
	c[1] = '\0';
	return(c);
}


/* 
 * Special character to string array.
 */
const char *
chars_spec2str(void *arg, const char *p, size_t sz, size_t *rsz)
{
	const struct ln	*ln;

	ln = find((struct ctab *)arg, p, sz, CHARS_CHAR);
	if (NULL == ln)
		return(NULL);

	*rsz = strlen(ln->ascii);
	return(ln->ascii);
}


/* 
 * Reserved word to string array.
 */
const char *
chars_res2str(void *arg, const char *p, size_t sz, size_t *rsz)
{
	const struct ln	*ln;

	ln = find((struct ctab *)arg, p, sz, CHARS_STRING);
	if (NULL == ln)
		return(NULL);

	*rsz = strlen(ln->ascii);
	return(ln->ascii);
}


static const struct ln *
find(struct ctab *tab, const char *p, size_t sz, int type)
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
		if ( ! match(pp, p, sz, type)) {
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
match(const struct ln *ln, const char *p, size_t sz, int type)
{

	if ( ! (ln->type & type))
		return(0);
	if (strncmp(ln->code, p, sz))
		return(0);
	return('\0' == ln->code[(int)sz]);
}
