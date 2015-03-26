/*	$OpenBSD: read.c,v 1.16 2015/03/26 21:26:43 tobias Exp $	*/
/*	$NetBSD: read.c,v 1.4 1994/11/23 07:42:07 jtc Exp $	*/

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

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

/*
 * bytes -- read bytes to an offset from the end and display.
 *
 * This is the function that reads to a byte offset from the end of the input,
 * storing the data in a wrap-around buffer which is then displayed.  If the
 * rflag is set, the data is displayed in lines in reverse order, and this
 * routine has the usual nastiness of trying to find the newlines.  Otherwise,
 * it is displayed from the character closest to the beginning of the input to
 * the end.
 *
 * A non-zero return means an (non-fatal) error occurred.
 *
 */
int
bytes(FILE *fp, off_t off)
{
	int ch;
	size_t len, tlen;
	char *ep, *p, *t;
	int wrap;
	char *sp;

	if (off > SIZE_MAX)
		errx(1, "offset too large");

	if ((sp = p = malloc(off)) == NULL)
		err(1, NULL);

	for (wrap = 0, ep = p + off; (ch = getc(fp)) != EOF;) {
		*p = ch;
		if (++p == ep) {
			wrap = 1;
			p = sp;
		}
	}
	if (ferror(fp)) {
		ierr();
		free(sp);
		return(1);
	}

	if (rflag) {
		for (t = p - 1, len = 0; t >= sp; --t, ++len)
			if (*t == '\n' && len) {
				WR(t + 1, len);
				len = 0;
			}
		if (wrap) {
			tlen = len;
			for (t = ep - 1, len = 0; t >= p; --t, ++len)
				if (*t == '\n') {
					if (len) {
						WR(t + 1, len);
						len = 0;
					}
					if (tlen) {
						WR(sp, tlen);
						tlen = 0;
					}
				}
			if (len)
				WR(t + 1, len);
			if (tlen)
				WR(sp, tlen);
		}
	} else {
		if (wrap && (len = ep - p))
			WR(p, len);
		if ((len = p - sp))
			WR(sp, len);
	}

	free(sp);
	return(0);
}

/*
 * lines -- read lines to an offset from the end and display.
 *
 * This is the function that reads to a line offset from the end of the input,
 * storing the data in an array of buffers which is then displayed.  If the
 * rflag is set, the data is displayed in lines in reverse order, and this
 * routine has the usual nastiness of trying to find the newlines.  Otherwise,
 * it is displayed from the line closest to the beginning of the input to
 * the end.
 *
 * A non-zero return means an (non-fatal) error occurred.
 *
 */
int
lines(FILE *fp, off_t off)
{
	struct {
		size_t blen;
		size_t len;
		char *l;
	} *lines;
	int ch, rc = 0;
	char *p = NULL;
	int wrap;
	size_t cnt, recno, blen, newsize;
	char *sp = NULL, *newp = NULL;

	if (off > SIZE_MAX)
		errx(1, "offset too large");

	if ((lines = calloc(off, sizeof(*lines))) == NULL)
		err(1, NULL);

	blen = cnt = recno = wrap = 0;

	while ((ch = getc(fp)) != EOF) {
		if (++cnt > blen) {
			newsize = blen + 1024;
			if ((newp = realloc(sp, newsize)) == NULL)
				err(1, NULL);
			sp = newp;
			blen = newsize;
			p = sp + cnt - 1;
		}
		*p++ = ch;
		if (ch == '\n') {
			if (lines[recno].blen < cnt) {
				newsize = cnt + 256;
				if ((newp = realloc(lines[recno].l,
				    newsize)) == NULL)
					err(1, NULL);
				lines[recno].l = newp;
				lines[recno].blen = newsize;
			}
			memcpy(lines[recno].l, sp, (lines[recno].len = cnt));
			cnt = 0;
			p = sp;
			if (++recno == off) {
				wrap = 1;
				recno = 0;
			}
		}
	}
	if (ferror(fp)) {
		ierr();
		rc = 1;
		goto done;
	}
	if (cnt) {
		lines[recno].l = sp;
		lines[recno].len = cnt;
		sp = NULL;
		if (++recno == off) {
			wrap = 1;
			recno = 0;
		}
	}

	if (rflag) {
		for (cnt = recno; cnt > 0; --cnt)
			WR(lines[cnt - 1].l, lines[cnt - 1].len);
		if (wrap)
			for (cnt = off; cnt > recno; --cnt)
				WR(lines[cnt - 1].l, lines[cnt - 1].len);
	} else {
		if (wrap)
			for (cnt = recno; cnt < off; ++cnt)
				WR(lines[cnt].l, lines[cnt].len);
		for (cnt = 0; cnt < recno; ++cnt)
			WR(lines[cnt].l, lines[cnt].len);
	}
done:
	for (cnt = 0; cnt < off; cnt++)
		free(lines[cnt].l);
	free(sp);
	free(lines);
	return(rc);
}
