/*	$Id: ascii.c,v 1.2 2009/06/14 23:00:57 schwarze Exp $ */
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
#include <err.h>
#include <stdlib.h>
#include <string.h>

#include "term.h"

#define	ASCII_PRINT_HI	 126
#define	ASCII_PRINT_LO	 32

struct	line {
	const char	 *code;
	const char	 *out;
	/* 32- and 64-bit alignment safe. */
	size_t		  codesz;
	size_t		  outsz;
};

struct	linep {
	const struct line *line;
	struct linep	 *next;
};

#define LINE(w, x, y, z) \
	{ (w), (y), (x), (z) },
static	const struct line lines[] = {
#include "ascii.in"
};

struct	asciitab {
	struct linep	 *lines;
	void		**htab;
};


static	inline int	  match(const struct line *,
				const char *, size_t);


void
term_asciifree(void *arg)
{
	struct asciitab	*tab;

	tab = (struct asciitab *)arg;

	free(tab->lines);
	free(tab->htab);
	free(tab);
}


void *
term_ascii2htab(void)
{
	struct asciitab  *tab;
	void		**htab;
	struct linep	 *pp, *p;
	int		  i, len, hash;

	/*
	 * Constructs a very basic chaining hashtable.  The hash routine
	 * is simply the integral value of the first character.
	 * Subsequent entries are chained in the order they're processed
	 * (they're in-line re-ordered during lookup).
	 */

	if (NULL == (tab = malloc(sizeof(struct asciitab))))
		err(1, "malloc");

	len = sizeof(lines) / sizeof(struct line);

	if (NULL == (p = calloc((size_t)len, sizeof(struct linep))))
		err(1, "malloc");

	htab = calloc(ASCII_PRINT_HI - ASCII_PRINT_LO + 1, 
			sizeof(struct linep **));

	if (NULL == htab)
		err(1, "malloc");

	for (i = 0; i < len; i++) {
		assert(lines[i].codesz > 0);
		assert(lines[i].code);
		assert(lines[i].out);

		p[i].line = &lines[i];

		hash = (int)lines[i].code[0] - ASCII_PRINT_LO;

		if (NULL == (pp = ((struct linep **)htab)[hash])) {
			htab[hash] = &p[i];
			continue;
		}

		for ( ; pp->next; pp = pp->next)
			/* Scan ahead. */ ;

		pp->next = &p[i];
	}

	tab->htab = htab;
	tab->lines = p;

	return(tab);
}


const char *
term_a2ascii(void *arg, const char *p, size_t sz, size_t *rsz)
{
	struct asciitab	 *tab;
	struct linep	 *pp, *prev;
	void		**htab;
	int		  hash;

	tab = (struct asciitab *)arg;
	htab = tab->htab;

	assert(p);
	assert(sz > 0);

	if (p[0] < ASCII_PRINT_LO || p[0] > ASCII_PRINT_HI)
		return(NULL);


	/*
	 * Lookup the symbol in the symbol hash.  See ascii2htab for the
	 * hashtable specs.  This dynamically re-orders the hash chain
	 * to optimise for repeat hits.
	 */

	hash = (int)p[0] - ASCII_PRINT_LO;

	if (NULL == (pp = ((struct linep **)htab)[hash]))
		return(NULL);

	if (NULL == pp->next) {
		if ( ! match(pp->line, p, sz)) 
			return(NULL);
		*rsz = pp->line->outsz;
		return(pp->line->out);
	}

	for (prev = NULL; pp; pp = pp->next) {
		if ( ! match(pp->line, p, sz)) {
			prev = pp;
			continue;
		}

		/* Re-order the hash chain. */

		if (prev) {
			prev->next = pp->next;
			pp->next = ((struct linep **)htab)[hash];
			htab[hash] = pp;
		}

		*rsz = pp->line->outsz;
		return(pp->line->out);
	}

	return(NULL);
}


static inline int
match(const struct line *line, const char *p, size_t sz)
{

	if (line->codesz != sz)
		return(0);
	return(0 == strncmp(line->code, p, sz));
}
