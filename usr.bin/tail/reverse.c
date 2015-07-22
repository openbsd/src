/*	$OpenBSD: reverse.c,v 1.20 2015/07/22 16:37:04 tobias Exp $	*/
/*	$NetBSD: reverse.c,v 1.6 1994/11/23 07:42:10 jtc Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Sze-Tyan Wang.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/stat.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "extern.h"

static void r_buf(FILE *);
static int r_reg(FILE *, enum STYLE, off_t, struct stat *);

#define COPYCHAR(fp, ch)				\
	do {						\
		if ((ch = getc(fp)) == EOF) {		\
			ierr();				\
			return (0);			\
		}					\
		if (putchar(ch) == EOF) {		\
			oerr();				\
			return (0);			\
		}					\
	} while (0)

/*
 * reverse -- display input in reverse order by line.
 *
 * There are six separate cases for this -- regular and non-regular
 * files by bytes, lines or the whole file.
 *
 * BYTES	display N bytes
 *	REG	reverse scan and display the lines
 *	NOREG	cyclically read characters into a wrap-around buffer
 *
 * LINES	display N lines
 *	REG	reverse scan and display the lines
 *	NOREG	cyclically read lines into a wrap-around array of buffers
 *
 * FILE		display the entire file
 *	REG	reverse scan and display the lines
 *	NOREG	cyclically read input into a linked list of buffers
 */
void
reverse(FILE *fp, enum STYLE style, off_t off, struct stat *sbp)
{
	if (style != REVERSE && off == 0)
		return;

	if (!S_ISREG(sbp->st_mode) || r_reg(fp, style, off, sbp) != 0)
		switch(style) {
		case FBYTES:
		case RBYTES:
			(void)bytes(fp, off);
			break;
		case FLINES:
		case RLINES:
			(void)lines(fp, off);
			break;
		case REVERSE:
			r_buf(fp);
			break;
		}
}

/*
 * r_reg -- display a regular file in reverse order by line.
 */
static int
r_reg(FILE *fp, enum STYLE style, off_t off, struct stat *sbp)
{
	off_t start, pos, end;
	int ch;

	end = sbp->st_size;
	if (end == 0)
		return (0);

	/* Position before char, ignore last char whether newline or not */
	pos = end-2;
	ch = EOF;
	start = 0;

	if (style == RBYTES && off < end)
		start = end - off;

	for (; pos >= start; pos--) {
		/* A seek per char isn't a problem with a smart stdio */
		if (fseeko(fp, pos, SEEK_SET) != 0) {
			ierr();
			return (0);
		}
		if ((ch = getc(fp)) == '\n') {
			while (--end > pos) 
				COPYCHAR(fp, ch);
			end++;
			if (style == RLINES && --off == 0)
				break;
		}
		else if (ch == EOF) {
			ierr();
			return (0);
		}
	}
	if (pos < start) {
		if (ch != EOF && ungetc(ch, fp) == EOF) {
			ierr();
			return (0);
		}
		while (--end >= start)
			COPYCHAR(fp, ch);
	}
	return (0);
}

#define	BSZ	(128 * 1024)
struct bf {
	struct bf *next;
	struct bf *prev;
	size_t len;
	char l[BSZ];
};

/*
 * r_buf -- display a non-regular file in reverse order by line.
 *
 * This is the function that saves the entire input, storing the data in a
 * doubly linked list of buffers and then displays them in reverse order.
 * It has the usual nastiness of trying to find the newlines, as there's no
 * guarantee that a newline occurs anywhere in the file, let alone in any
 * particular buffer.  If we run out of memory, input is discarded (and the
 * user warned).
 */
static void
r_buf(FILE *fp)
{
	struct bf *mark, *tr, *tl = NULL;
	int ch;
	size_t len, llen;
	char *p;
	off_t enomem;

	for (mark = NULL, enomem = 0;;) {
		/*
		 * Allocate a new block and link it into place in a doubly
		 * linked list.  If out of memory, toss the LRU block and
		 * keep going.
		 */
		if (enomem || (tl = malloc(sizeof(*tl))) == NULL) {
			if (!mark)
				err(1, NULL);
			tl = enomem ? tl->next : mark;
			enomem += tl->len;
		} else if (mark) {
			tl->next = mark;
			tl->prev = mark->prev;
			mark->prev->next = tl;
			mark->prev = tl;
		} else {
			mark = tl;
			mark->next = mark->prev = mark;
		}

		if (!enomem)
			tl->len = 0;

		/* Fill the block with input data. */
		for (p = tl->l, len = 0;
		    len < BSZ && (ch = getc(fp)) != EOF; ++len)
			*p++ = ch;

		/*
		 * If no input data for this block and we tossed some data,
		 * recover it.
		 */
		if (!len) {
			if (enomem)
				enomem -= tl->len;
			tl = tl->prev;
			break;
		}

		tl->len = len;
		if (ch == EOF)
			break;
	}

	if (enomem) {
		(void)fprintf(stderr,
		    "tail: warning: %lld bytes discarded\n", (long long)enomem);
		rval = 1;
	}

	/*
	 * Step through the blocks in the reverse order read.  The last char
	 * is special, ignore whether newline or not.
	 */
	for (mark = tl;;) {
		for (p = tl->l + (len = tl->len) - 1, llen = 0; len--;
		    --p, ++llen)
			if (*p == '\n') {
				if (llen) {
					WR(p + 1, llen);
					llen = 0;
				}
				if (tl == mark)
					continue;
				for (tr = tl->next; tr->len; tr = tr->next) {
					WR(tr->l, tr->len);
					tr->len = 0;
					if (tr == mark)
						break;
				}
			}
		tl->len = llen;
		if ((tl = tl->prev) == mark)
			break;
	}
	tl = tl->next;
	if (tl->len) {
		WR(tl->l, tl->len);
		tl->len = 0;
	}
	while ((tl = tl->next)->len) {
		WR(tl->l, tl->len);
		tl->len = 0;
	}

	tl->prev->next = NULL;
	while (tl != NULL) {
		tr = tl->next;
		free(tl);
		tl = tr;
	}
}
