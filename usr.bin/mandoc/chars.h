/*	$Id: chars.h,v 1.6 2011/01/30 16:05:29 schwarze Exp $ */
/*
 * Copyright (c) 2008, 2009, 2010 Kristaps Dzonsons <kristaps@bsd.lv>
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
#ifndef CHARS_H
#define CHARS_H

__BEGIN_DECLS

enum	chars {
	CHARS_ASCII,
	CHARS_HTML
};

void		 *chars_init(enum chars);
const char	 *chars_num2char(const char *, size_t);
const char	 *chars_spec2str(void *, const char *, size_t, size_t *);
int		  chars_spec2cp(void *, const char *, size_t);
const char	 *chars_res2str(void *, const char *, size_t, size_t *);
int		  chars_res2cp(void *, const char *, size_t);
void		  chars_free(void *);

__END_DECLS

#endif /*!CHARS_H*/
