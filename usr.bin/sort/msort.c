/*	$OpenBSD: msort.c,v 1.23 2009/12/22 19:47:02 schwarze Exp $	*/

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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Subroutines using comparisons: merge sort and check order */
#define DELETE (1)
#define LALIGN(n) ((n+(sizeof(long)-1)) & ~(sizeof(long)-1))

typedef struct mfile {
	u_char *end;
	short flno;
	RECHEADER rec[1];
} MFILE;
static u_char *wts, *wts1;
static struct mfile *cfilebuf;
static void *buffer;
static size_t bufsize;

static int cmp(RECHEADER *, RECHEADER *);
static int insert(struct mfile **, struct mfile **, int, int);

void
fmerge(int binno, union f_handle files, int nfiles,
    int (*get)(int, union f_handle, int, RECHEADER *, u_char *, struct field *),
    FILE *outfp, void (*fput)(RECHEADER *, FILE *), struct field *ftbl)
{
	FILE *tout;
	int i, j, last;
	void (*put)(RECHEADER *, FILE *);
	struct tempfile *l_fstack;

	wts = ftbl->weights;
	if (!UNIQUE && SINGL_FLD && (ftbl->flags & F))
		wts1 = (ftbl->flags & R) ? Rascii : ascii;
	if (cfilebuf == NULL) {
		cfilebuf = malloc(MAXLLEN + sizeof(MFILE));
		if (cfilebuf == NULL)
			errx(2, "cannot allocate memory");
	}

	i = min(16, nfiles) * LALIGN(MAXLLEN + sizeof(MFILE));
	if (i > bufsize) {
		bufsize = i;
		if ((buffer = realloc(buffer, bufsize)) == NULL)
			err(2, NULL);
	}

	if (binno >= 0)
		l_fstack = fstack + files.top;
	else
		l_fstack = fstack;

	while (nfiles) {
		put = putrec;
		for (j = 0; j < nfiles; j += 16) {
			if (nfiles <= 16) {
				tout = outfp;
				put = fput;
			}
			else
				tout = ftmp();
			last = min(16, nfiles - j);
			if (binno < 0) {
				for (i = 0; i < last; i++)
					if (!(l_fstack[i+MAXFCT-1-16].fp =
					    fopen(files.names[j + i], "r")))
						err(2, "%s", files.names[j+i]);
				merge(MAXFCT-1-16, last, get, tout, put, ftbl);
			} else {
				for (i = 0; i< last; i++)
					rewind(l_fstack[i+j].fp);
				merge(files.top+j, last, get, tout, put, ftbl);
			}
			if (nfiles > 16)
				l_fstack[j/16].fp = tout;
		}
		nfiles = (nfiles + 15) / 16;
		if (nfiles == 1)
			nfiles = 0;
		if (binno < 0) {
			binno = 0;
			get = geteasy;
			files.top = 0;
		}
	}
}

void
merge(int infl0, int nfiles,
    int (*get)(int, union f_handle, int, RECHEADER *, u_char *, struct field *),
    FILE *outfp, void (*put)(RECHEADER *, FILE *), struct field *ftbl)
{
	int c, i, j;
	union f_handle dummy = {0};
	struct mfile *flist[16], *cfile;

	for (i = j = 0; i < nfiles; i++) {
		cfile = (MFILE *) (buffer +
		    i * LALIGN(MAXLLEN + sizeof(MFILE)));
		cfile->flno = j + infl0;
		cfile->end = cfile->rec->data + MAXLLEN;
		for (c = 1; c == 1;) {
			if (EOF == (c = get(j+infl0, dummy, nfiles,
			   cfile->rec, cfile->end, ftbl))) {
				i--;
				nfiles--;
				break;
			}
			if (i)
				c = insert(flist, &cfile, i, !DELETE);
			else
				flist[0] = cfile;
		}
		j++;
	}
	if (nfiles > 0) {
		cfile = cfilebuf;
		cfile->flno = flist[0]->flno;
		cfile->end = cfile->rec->data + MAXLLEN;
		while (nfiles) {
			for (c = 1; c == 1;) {
				if (EOF == (c = get(cfile->flno, dummy, nfiles,
				   cfile->rec, cfile->end, ftbl))) {
					put(flist[0]->rec, outfp);
					memmove(flist, flist + 1,
					    sizeof(MFILE *) * (--nfiles));
					cfile->flno = flist[0]->flno;
					break;
				}
				if (!(c = insert(flist, &cfile, nfiles, DELETE)))
					put(cfile->rec, outfp);
			}
		}	
	}	
}

/*
 * if delete: inserts *rec in flist, deletes flist[0], and leaves it in *rec;
 * otherwise just inserts *rec in flist.
 */
static int
insert(struct mfile **flist, struct mfile **rec, int ttop,
    int delete)			/* delete = 0 or 1 */
{
	struct mfile *tmprec;
	int top, mid, bot = 0, cmpv = 1;
	tmprec = *rec;
	top = ttop;
	for (mid = top/2; bot +1 != top; mid = (bot+top)/2) {
		cmpv = cmp(tmprec->rec, flist[mid]->rec);
		if (cmpv < 0)
			top = mid;
		else if (cmpv > 0)
			bot = mid;
		else {
			if (!UNIQUE)
				bot = mid - 1;
			break;
		}
	}
	if (delete) {
		if (UNIQUE) {
			if (!bot && cmpv)
				cmpv = cmp(tmprec->rec, flist[0]->rec);
			if (!cmpv)
				return (1);
		}
		tmprec = flist[0];
		if (bot)
			memmove(flist, flist+1, bot * sizeof(MFILE **));
		flist[bot] = *rec;
		*rec = tmprec;
		(*rec)->flno = (*flist)->flno;
		return (0);
	}
	else {
		if (!bot && !(UNIQUE && !cmpv)) {
			cmpv = cmp(tmprec->rec, flist[0]->rec);
			if (cmpv < 0)
				bot = -1;
		}
		if (UNIQUE && !cmpv)
			return (1);
		bot++;
		memmove(flist + bot+1, flist + bot,
		    (ttop - bot) * sizeof(MFILE **));
		flist[bot] = *rec;
		return (0);
	}
}

/*
 * check order on one file
 */
void
order(union f_handle infile,
    int (*get)(int, union f_handle, int, RECHEADER *, u_char *, struct field *),
    struct field *ftbl,
    int c_warn)
{
	u_char *crec_end, *prec_end, *trec_end;
	int c;
	RECHEADER *crec, *prec, *trec;

	if (bufsize < 2 * ALIGN(MAXLLEN + sizeof(RECHEADER))) {
		bufsize = 2 * ALIGN(MAXLLEN + sizeof(RECHEADER));
		if ((buffer = realloc(buffer, bufsize)) == NULL)
			err(2, NULL);
	}
	crec = (RECHEADER *) buffer;
	crec_end = ((char *)crec) + ALIGN(MAXLLEN + sizeof(RECHEADER));
	prec = (RECHEADER *) crec_end;
	prec_end = ((char *)prec) + ALIGN(MAXLLEN + sizeof(RECHEADER));
	wts = ftbl->weights;
	if (SINGL_FLD && (ftbl->flags & F))
		wts1 = ftbl->flags & R ? Rascii : ascii;
	else
		wts1 = 0;
	if (get(-1, infile, 1, prec, prec_end, ftbl) == 0)
		while (get(-1, infile, 1, crec, crec_end, ftbl) == 0) {
			if (0 < (c = cmp(prec, crec))) {
				crec->data[crec->length-1] = 0;
				if (c_warn)
					errx(1, "found disorder: %s",
					    crec->data+crec->offset);
				else
					exit(1);
			}
			if (UNIQUE && !c) {
				crec->data[crec->length-1] = 0;
				if (c_warn)
					errx(1, "found non-uniqueness: %s",
					    crec->data+crec->offset);
				else
					exit(1);
			}
			/* Swap pointers so that this record is on place
			 * pointed to by prec and new record is read to place
			 * pointed to by crec.
			 */
			trec = prec;
			prec = crec;
			crec = trec;
			trec_end = prec_end;
			prec_end = crec_end;
			crec_end = trec_end;
		}
	exit(0);
}

static int
cmp(RECHEADER *rec1, RECHEADER *rec2)
{
	int r;
	u_char *pos1, *pos2, *end;
	u_char *cwts;
	for (cwts = wts; cwts; cwts = (cwts == wts1 ? 0 : wts1)) {
		pos1 = rec1->data;
		pos2 = rec2->data;
		if (!SINGL_FLD && (UNIQUE || STABLE))
			end = pos1 + min(rec1->offset, rec2->offset);
		else
			end = pos1 + min(rec1->length, rec2->length);
		for (; pos1 < end; ) {
			if ((r = cwts[*pos1++] - cwts[*pos2++]))
				return (r);
		}
	}
	return (0);
}
