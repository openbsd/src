/*	$Id: chars.c,v 1.7 2010/05/26 02:39:58 schwarze Exp $ */
/*
 * Copyright (c) 2009 Kristaps Dzonsons <kristaps@kth.se>
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
#include "chars.h"

#define	PRINT_HI	 126
#define	PRINT_LO	 32

struct	ln {
	struct ln	 *next;
	const char	 *code;
	const char	 *ascii;
	const char	 *html;
	size_t		  codesz;
	size_t		  asciisz;
	size_t		  htmlsz;
	int		  type;
#define	CHARS_CHAR	 (1 << 0)
#define	CHARS_STRING	 (1 << 1)
#define CHARS_BOTH	 (CHARS_CHAR | CHARS_STRING)
};

#define	LINES_MAX	  369

#define CHAR(w, x, y, z, a, b) \
	{ NULL, (w), (y), (a), (x), (z), (b), CHARS_CHAR },
#define STRING(w, x, y, z, a, b) \
	{ NULL, (w), (y), (a), (x), (z), (b), CHARS_STRING },
#define BOTH(w, x, y, z, a, b) \
	{ NULL, (w), (y), (a), (x), (z), (b), CHARS_BOTH },

#define	CHAR_TBL_START	  static struct ln lines[LINES_MAX] = {
#define	CHAR_TBL_END	  };

#include "chars.in"

struct	tbl {
	enum chars	  type;
	struct ln	**htab;
};

static	inline int	  match(const struct ln *,
				const char *, size_t, int);
static	const char	 *find(struct tbl *, const char *, 
				size_t, size_t *, int);


void
chars_free(void *arg)
{
	struct tbl	*tab;

	tab = (struct tbl *)arg;

	free(tab->htab);
	free(tab);
}


void *
chars_init(enum chars type)
{
	struct tbl	 *tab;
	struct ln	**htab;
	struct ln	 *pp;
	int		  i, hash;

	/*
	 * Constructs a very basic chaining hashtable.  The hash routine
	 * is simply the integral value of the first character.
	 * Subsequent entries are chained in the order they're processed
	 * (they're in-line re-ordered during lookup).
	 */

	tab = malloc(sizeof(struct tbl));
	if (NULL == tab) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

	htab = calloc(PRINT_HI - PRINT_LO + 1, sizeof(struct ln **));
	if (NULL == htab) {
		perror(NULL);
		exit(EXIT_FAILURE);
	}

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


const char *
chars_a2ascii(void *arg, const char *p, size_t sz, size_t *rsz)
{

	return(find((struct tbl *)arg, p, sz, rsz, CHARS_CHAR));
}


const char *
chars_a2res(void *arg, const char *p, size_t sz, size_t *rsz)
{

	return(find((struct tbl *)arg, p, sz, rsz, CHARS_STRING));
}


static const char *
find(struct tbl *tab, const char *p, size_t sz, size_t *rsz, int type)
{
	struct ln	 *pp, *prev;
	struct ln	**htab;
	int		  hash;

	assert(p);
	assert(sz > 0);

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

		if (CHARS_HTML == tab->type) {
			*rsz = pp->htmlsz;
			return(pp->html);
		}
		*rsz = pp->asciisz;
		return(pp->ascii);
	}

	return(NULL);
}


static inline int
match(const struct ln *ln, const char *p, size_t sz, int type)
{

	if ( ! (ln->type & type))
		return(0);
	if (ln->codesz != sz)
		return(0);
	return(0 == strncmp(ln->code, p, sz));
}
