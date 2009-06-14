/*	$Id: term.h,v 1.2 2009/06/14 23:00:57 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009 Kristaps Dzonsons <kristaps@kth.se>
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
#ifndef TERM_H
#define TERM_H

/* FIXME - clean up tabs. */

#define	INDENT		  6

__BEGIN_DECLS

enum	termenc {
	TERMENC_ASCII,
	TERMENC_LATIN1,		/* Not implemented. */
	TERMENC_UTF8		/* Not implemented. */
};

struct	termp {
	size_t		  rmargin;	/* Current right margin. */
	size_t		  maxrmargin;	/* Max right margin. */
	size_t		  maxcols;	/* Max size of buf. */
	size_t		  offset;	/* Margin offest. */
	size_t		  col;		/* Bytes in buf. */
	int		  flags;
#define	TERMP_NOSPACE	 (1 << 0)	/* No space before words. */
#define	TERMP_NOLPAD	 (1 << 1)	/* No leftpad before flush. */
#define	TERMP_NOBREAK	 (1 << 2)	/* No break after flush. */
#define	TERMP_LITERAL	 (1 << 3)	/* Literal words. */
#define	TERMP_IGNDELIM	 (1 << 4)	/* Delims like regulars. */
#define	TERMP_NONOSPACE	 (1 << 5)	/* No space (no autounset). */
#define	TERMP_NONOBREAK	 (1 << 7)	/* Don't newln NOBREAK. */
#define	TERMP_STYLE	  0x0300	/* Style mask. */
#define	TERMP_BOLD	 (1 << 8)	/* Styles... */
#define	TERMP_UNDER	 (1 << 9)
	char		 *buf;		/* Output buffer. */
	enum termenc	  enc;		/* Type of encoding. */
	void		 *symtab;	/* Encoded-symbol table. */
};

void		 *term_ascii2htab(void);
const char	 *term_a2ascii(void *, const char *, size_t, size_t *);
void		  term_asciifree(void *);

void		  term_newln(struct termp *);
void		  term_vspace(struct termp *);
void		  term_word(struct termp *, const char *);
void		  term_flushln(struct termp *);

__END_DECLS

#endif /*!TERM_H*/
