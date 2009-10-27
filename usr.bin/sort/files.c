/*	$OpenBSD: files.c,v 1.13 2009/10/27 23:59:43 deraadt Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Peter McIlroy.
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

#include "sort.h"
#include "fsort.h"

#include <string.h>

static int	seq(FILE *, DBT *, DBT *);

/*
 * this is the subroutine for file management for fsort().
 * It keeps the buffers for all temporary files.
 */
int
getnext(int binno, union f_handle infl0, int nfiles, RECHEADER *pos, u_char *end,
    struct field *dummy)
{
	int i;
	u_char *hp;
	static size_t nleft = 0;
	static int cnt = 0, flag = -1;
	static u_char maxb = 0;
	static FILE *fp;

	if (nleft == 0) {
		if (binno < 0)	/* reset files. */ {
			for (i = 0; i < nfiles; i++) {
				rewind(fstack[infl0.top + i].fp);
				fstack[infl0.top + i].max_o = 0;
			}
			flag = -1;
			nleft = cnt = 0;
			return (-1);
		}
		maxb = fstack[infl0.top].maxb;
		for (; nleft == 0; cnt++) {
			if (cnt >= nfiles) {
				cnt = 0;
				return (EOF);
			}
			fp = fstack[infl0.top + cnt].fp;
			fread(&nleft, sizeof(nleft), 1, fp);
			if (binno < maxb)
				fstack[infl0.top+cnt].max_o
					+= sizeof(nleft) + nleft;
			else if (binno == maxb) {
				if (binno != fstack[infl0.top].lastb) {
					fseek(fp, fstack[infl0.top+
						cnt].max_o, SEEK_SET);
					fread(&nleft, sizeof(nleft), 1, fp);
				}
				if (nleft == 0)
					fclose(fp);
			} else if (binno == maxb + 1) {		/* skip a bin */
				fseek(fp, nleft, SEEK_CUR);
				fread(&nleft, sizeof(nleft), 1, fp);
				flag = cnt;
			}
		}
	}
	if ((u_char *) pos > end - sizeof(TRECHEADER))
		return (BUFFEND);
	fread(pos, sizeof(TRECHEADER), 1, fp);
	if (end - pos->data < pos->length) {
		hp = ((u_char *)pos) + sizeof(TRECHEADER);
		for (i = sizeof(TRECHEADER); i ;  i--)
			ungetc(*--hp, fp);
		return (BUFFEND);
	}
	fread(pos->data, pos->length, 1, fp);
	nleft -= pos->length + sizeof(TRECHEADER);
	if (nleft == 0 && binno == fstack[infl0.top].maxb)
		fclose(fp);
	return (0);
}

/*
 * this is called when there is no special key. It's only called
 * in the first fsort pass.
 */
int
makeline(int flno, union f_handle filelist, int nfiles, RECHEADER *buffer,
    u_char *bufend, struct field *dummy2)
{
	static u_char *obufend;
	static size_t osz;
	char *pos;
	static int fileno = 0, overflow = 0;
	static FILE *fp = 0;
	int c;

	pos = (char *) buffer->data;
	if (overflow) {
		/*
		 * Buffer shortage is solved by either of two ways:
	 	 * * Flush previous buffered data and start using the 
		 *   buffer from start (see fsort())
		 * * realloc buffer and bump bufend
		 *
		 * The former is preferred, realloc is only done when
		 * there is exactly one item in buffer which does not fit.
		 */
		if (bufend == obufend)
			memmove(pos, bufend - osz, osz);
		pos+=osz;
		overflow = 0;
	}
	for (;;) {
		if (flno >= 0 && (fp = fstack[flno].fp) == NULL)
			return (EOF);
		else if (fp == 0) {
			if (fileno  >= nfiles)
				return (EOF);
			if (!(fp = fopen(filelist.names[fileno], "r")))
				err(2, "%s", filelist.names[fileno]);
			fileno++;
		}
		while ((pos < (char *)bufend) && ((c = getc(fp)) != EOF)) {
			if ((*pos++ = c) == REC_D) {
				buffer->offset = 0;
				buffer->length = pos - (char *) buffer->data;
				return (0);
			}
		}
		if (pos >= (char *)bufend) {
			if (buffer->data < bufend) {
				overflow = 1;
				obufend = bufend;
				osz = (pos - (char *)buffer->data);
			}
			return (BUFFEND);
		} else if (c == EOF) {
			if (buffer->data != (u_char *) pos) {
				*pos++ = REC_D;
				buffer->offset = 0;
				buffer->length = pos - (char *) buffer->data;
				return (0);
			}
			FCLOSE(fp);
			fp = 0;
			if (flno >= 0)
				fstack[flno].fp = 0;
		} else {
			warnx("line too long: ignoring %100s...", buffer->data);
			
			/* Consume the rest of the line from input */
			while((c = getc(fp)) != REC_D && c != EOF)
				;

			buffer->offset = 0;
			buffer->length = 0;

			return (BUFFEND);
		}
	}
}

/*
 * This generates keys. It's only called in the first fsort pass
 */
int
makekey(int flno, union f_handle filelist, int nfiles, RECHEADER *buffer,
    u_char *bufend, struct field *ftbl)
{
	static int fileno = 0;
	static FILE *dbdesc = 0;
	static DBT dbkey[1], line[1];
	static int overflow = 0;
	static int c;

	if (overflow) {
		overflow = enterkey(buffer, line, bufend - (u_char *)buffer,
									ftbl);
		if (overflow)
			return (BUFFEND);
		else
			return (0);
	}

	for (;;) {
		if (flno >= 0) {
			if (!(dbdesc = fstack[flno].fp))
				return (EOF);
		} else if (!dbdesc) {
			if (fileno  >= nfiles)
				return (EOF);
			dbdesc = fopen(filelist.names[fileno], "r");
			if (!dbdesc)
				err(2, "%s", filelist.names[fileno]);
			fileno++;
		}
		if (!(c = seq(dbdesc, line, dbkey))) {
			if ((signed)line->size > bufend - buffer->data) {
				overflow = 1;
			} else {
				overflow = enterkey(buffer, line,
				    bufend - (u_char *) buffer, ftbl);
			}
			if (overflow)
				return (BUFFEND);
			else
				return (0);
		}
		if (c == EOF) {
			FCLOSE(dbdesc);
			dbdesc = 0;
			if (flno >= 0)
				fstack[flno].fp = 0;
		} else {
			((char *) line->data)[60] = '\000';
			warnx("line too long: ignoring %.100s...",
			    (char *)line->data);
		}
	}
}

/*
 * get a key/line pair from fp
 */
static int
seq(FILE *fp, DBT *line, DBT *key)
{
	static char *buf, flag = 1;
	char *end, *pos;
	int c;

	if (flag) {
		flag = 0;
		buf = (char *) linebuf;
		line->data = buf;
	}
	pos = buf;
	end = buf + linebuf_size;
	while ((c = getc(fp)) != EOF) {
		if ((*pos++ = c) == REC_D) {
			line->size = pos - buf;
			return (0);
		}
		if (pos == end) {
			linebuf_size *= 2;
			linebuf = realloc(linebuf, linebuf_size);
			if (!linebuf)
				err(2, "realloc of linebuf to %lu bytes failed",
					(unsigned long)linebuf_size);
			end = linebuf + linebuf_size;
			pos = linebuf + (pos - buf);
			line->data = buf = (char *)linebuf;
			continue;
		}
	}
	if (pos != buf) {
		*pos++ = REC_D;
		line->size = pos - buf;
		return (0);
	} else
		return (EOF);
}

/*
 * write a key/line pair to a temporary file
 */
void
putrec(RECHEADER *rec, FILE *fp)
{
	EWRITE(rec, 1, rec->length + sizeof(TRECHEADER), fp);
}

/*
 * write a line to output
 */
void
putline(RECHEADER *rec, FILE *fp)
{
	EWRITE(rec->data+rec->offset, 1, rec->length - rec->offset, fp);
}

/*
 * get a record from a temporary file. (Used by merge sort.)
 */
int
geteasy(int flno, union f_handle filelist, int nfiles, RECHEADER *rec,
    u_char *end, struct field *dummy2)
{
	int i;
	FILE *fp;

	fp = fstack[flno].fp;
	if ((u_char *) rec > end - sizeof(TRECHEADER))
		return (BUFFEND);
	if (!fread(rec, 1, sizeof(TRECHEADER), fp)) {
		fclose(fp);
		fstack[flno].fp = 0;
		return (EOF);
	}
	if (end - rec->data < rec->length) {
		for (i = sizeof(TRECHEADER) - 1; i >= 0;  i--)
			ungetc(*((char *) rec + i), fp);
		return (BUFFEND);
	}
	fread(rec->data, rec->length, 1, fp);
	return (0);
}
