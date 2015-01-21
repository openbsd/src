/*	$OpenBSD: abuf.h,v 1.26 2015/01/21 08:43:55 ratchov Exp $	*/
/*
 * Copyright (c) 2008-2012 Alexandre Ratchov <alex@caoua.org>
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
#ifndef ABUF_H
#define ABUF_H

struct abuf {
	int start;	/* offset (frames) where stored data starts */
	int used;	/* frames stored in the buffer */
	int len;	/* total size of the buffer (frames) */
	unsigned char *data;
};

void abuf_init(struct abuf *, unsigned int);
void abuf_done(struct abuf *);
void abuf_log(struct abuf *);
unsigned char *abuf_rgetblk(struct abuf *, int *);
unsigned char *abuf_wgetblk(struct abuf *, int *);
void abuf_rdiscard(struct abuf *, int);
void abuf_wcommit(struct abuf *, int);

#endif /* !defined(ABUF_H) */
