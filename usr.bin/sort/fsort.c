/*	$OpenBSD: fsort.c,v 1.12 2004/09/14 22:57:58 deraadt Exp $	*/

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

#ifndef lint
#if 0
static char sccsid[] = "@(#)fsort.c	8.1 (Berkeley) 6/6/93";
#else
static char rcsid[] = "$OpenBSD: fsort.c,v 1.12 2004/09/14 22:57:58 deraadt Exp $";
#endif
#endif /* not lint */

/*
 * Read in the next bin.  If it fits in one segment sort it;
 * otherwise refine it by segment deeper by one character,
 * and try again on smaller bins.  Sort the final bin at this level
 * of recursion to keep the head of fstack at 0.
 * After PANIC passes, abort to merge sort.
 */
#include "sort.h"
#include "fsort.h"

#include <stdlib.h>
#include <string.h>

u_char **keylist = 0, *buffer = 0, *linebuf = 0;
size_t bufsize, linebuf_size;
struct tempfile fstack[MAXFCT];
extern char toutpath[];
#define FSORTMAX 4
int PANIC = FSORTMAX;

void
fsort(int binno, int depth, union f_handle infiles, int nfiles, FILE *outfp,
    struct field *ftbl)
{
	u_char *bufend, **keypos, *tmpbuf;
	u_char *weights;
	int ntfiles, mfct = 0, total, i, maxb, lastb, panic = 0;
	int c, nelem;
	long sizes[NBINS+1];
	union f_handle tfiles, mstart = {MAXFCT-16};
	int (*get)(int, union f_handle, int, RECHEADER *,
		u_char *, struct field *);
	RECHEADER *crec;
	struct field tfield[2];
	FILE *prevfp, *tailfp[FSORTMAX+1];

	memset(tailfp, 0, sizeof(tailfp));
	prevfp = outfp;
	memset(tfield, 0, sizeof(tfield));
	if (ftbl[0].flags & R)
		tfield[0].weights = Rascii;
	else
		tfield[0].weights = ascii;
	tfield[0].icol.num = 1;
	weights = ftbl[0].weights;
	if (!buffer) {
		bufsize = BUFSIZE;
		if ((buffer = malloc(bufsize + 1)) == NULL ||
		    (keylist = calloc(MAXNUM, sizeof(u_char *))) == NULL)
			errx(2, "cannot allocate memory");
		if (!SINGL_FLD) {
			linebuf_size = MAXLLEN;
			if ((linebuf = malloc(linebuf_size)) == NULL)
				errx(2, "cannot allocate memory");
		}
	}
	bufend = buffer + bufsize;
	if (binno >= 0) {
		tfiles.top = infiles.top + nfiles;
		get = getnext;
	} else {
		tfiles.top = 0;
		if (SINGL_FLD)
			get = makeline;
		else
			get = makekey;
	}				
	for (;;) {
		memset(sizes, 0, sizeof(sizes));
		c = ntfiles = 0;
		if (binno == weights[REC_D] &&
		    !(SINGL_FLD && ftbl[0].flags & F)) {	/* pop */
			rd_append(weights[REC_D],
			    infiles, nfiles, prevfp, buffer, bufend);
			break;
		} else if (binno == weights[REC_D]) {
			depth = 0;		/* start over on flat weights */
			ftbl = tfield;
			weights = ftbl[0].weights;
		}
		while (c != EOF) {
			keypos = keylist;
			nelem = 0;
			crec = (RECHEADER *) buffer;
			while((c = get(binno, infiles, nfiles, crec, bufend,
			    ftbl)) == 0) {
				*keypos++ = crec->data + depth;
				if (++nelem == MAXNUM) {
					c = BUFFEND;
					break;
				}
				crec =(RECHEADER *)	((char *) crec +
				SALIGN(crec->length) + sizeof(TRECHEADER));
			}
			/*
			 * buffer was too small for data, allocate
			 * a bigger buffer.
			 */
			if (c == BUFFEND && nelem == 0) {
				bufsize *= 2;
				buffer = realloc(buffer, bufsize);
				if (!buffer)
					err(2, "failed to realloc buffer");
				bufend = buffer + bufsize;
				continue;
			}
			if (c == BUFFEND || ntfiles || mfct) {	/* push */
				if (panic >= PANIC) {
					fstack[MAXFCT-16+mfct].fp = ftmp();
					if (radixsort((const u_char **)keylist,
					    nelem, weights, REC_D))
						err(2, NULL);
					append(keylist, nelem, depth, fstack[
					 MAXFCT-16+mfct].fp, putrec, ftbl);
					mfct++;
					/* reduce number of open files */
					if (mfct == 16 ||(c == EOF && ntfiles)) {
						/*
						 * Only copy extra incomplete
						 * crec data if there is any.
						 */
						int nodata = (bufend
						    >= (u_char *)crec
						    && bufend <= crec->data);
						size_t sz = 0;

						if (!nodata) {
							sz = bufend
							    - crec->data;
							tmpbuf = malloc(sz);
							if (tmpbuf == NULL)
								errx(2, "cannot"
								    " allocate"
								    " memory");
							memmove(tmpbuf,
							    crec->data, sz);
						}

						fstack[tfiles.top + ntfiles].fp
						    = ftmp();
						fmerge(0, mstart, mfct, geteasy,
						  fstack[tfiles.top+ntfiles].fp,
						  putrec, ftbl);
						ntfiles++;
						mfct = 0;

						if (!nodata) {
							memmove(crec->data,
							    tmpbuf, sz);
							free(tmpbuf);
						}
					}
				} else {
					fstack[tfiles.top + ntfiles].fp= ftmp();
					onepass(keylist, depth, nelem, sizes,
					weights, fstack[tfiles.top+ntfiles].fp);
					ntfiles++;
				}
			}
		}
		get = getnext;
		if (!ntfiles && !mfct) {	/* everything in memory--pop */
			if (nelem > 1 && radixsort((const u_char **)keylist,
			    nelem, weights, REC_D))
				err(2, NULL);
			append(keylist, nelem, depth, outfp, putline, ftbl);
			break;					/* pop */
		}
		if (panic >= PANIC) {
			if (!ntfiles)
				fmerge(0, mstart, mfct, geteasy,
				    outfp, putline, ftbl);
			else
				fmerge(0, tfiles, ntfiles, geteasy,
				    outfp, putline, ftbl);
			break;
				
		}
		total = maxb = lastb = 0;	/* find if one bin dominates */
		for (i = 0; i < NBINS; i++)
		  if (sizes[i]) {
			if (sizes[i] > sizes[maxb])
				maxb = i;
			lastb = i;
			total += sizes[i];
		}
		if (sizes[maxb] < max((total / 2) , BUFSIZE))
			maxb = lastb;	/* otherwise pop after last bin */
		fstack[tfiles.top].lastb = lastb;
		fstack[tfiles.top].maxb = maxb;

			/* start refining next level. */
		get(-1, tfiles, ntfiles, crec, bufend, 0);	/* rewind */
		for (i = 0; i < maxb; i++) {
			if (!sizes[i])	/* bin empty; step ahead file offset */
				get(i, tfiles, ntfiles, crec, bufend, 0);
			else
				fsort(i, depth+1, tfiles, ntfiles, outfp, ftbl);
		}
		if (lastb != maxb) {
			if (prevfp != outfp)
				tailfp[panic] = prevfp;
			prevfp = ftmp();
			for (i = maxb+1; i <= lastb; i++)
				if (!sizes[i])
					get(i, tfiles, ntfiles, crec, bufend,0);
				else
					fsort(i, depth+1, tfiles, ntfiles,
					    prevfp, ftbl);
		}

		/* sort biggest (or last) bin at this level */
		depth++;
		panic++;
		binno = maxb;
		infiles.top = tfiles.top;	/* getnext will free tfiles, */
		nfiles = ntfiles;		/* so overwrite them */
	}
	if (prevfp != outfp) {
		concat(outfp, prevfp);
		fclose(prevfp);
	}
	for (i = panic; i >= 0; --i)
		if (tailfp[i]) {
			concat(outfp, tailfp[i]);
			fclose(tailfp[i]);
		}
}

/*
 * This is one pass of radix exchange, dumping the bins to disk.
 */
#define swap(a, b, t) t = a, a = b, b = t
void
onepass(u_char **a, int depth, long n, long sizes[], u_char *tr, FILE *fp)
{
	size_t tsizes[NBINS+1];
	u_char **bin[257], **top[256], ***bp, ***bpmax, ***tp;
	static int histo[256];
	int *hp;
	int c;
	u_char **an, *t, **aj;
	u_char **ak, *r;

	memset(tsizes, 0, sizeof(tsizes));
	depth += sizeof(TRECHEADER);
	an = &a[n];
	for (ak = a; ak < an; ak++) {
		histo[c = tr[**ak]]++;
		tsizes[c] += ((RECHEADER *) (*ak -= depth))->length;
	}

	bin[0] = a;
	bpmax = bin + 256;
	tp = top, hp = histo;
	for (bp = bin; bp < bpmax; bp++) {
		*tp++ = *(bp+1) = *bp + (c = *hp);
		*hp++ = 0;
		if (c <= 1)
			continue;
	}
	for (aj = a; aj < an; *aj = r, aj = bin[c+1]) 
		for(r = *aj; aj < (ak = --top[c = tr[r[depth]]]) ;)			
			swap(*ak, r, t);

	for (ak = a, c = 0; c < 256; c++) {
		an = bin[c+1];
		n = an - ak;
		tsizes[c] += n * sizeof(TRECHEADER);
		/* tell getnext how many elements in this bin, this segment. */
		EWRITE(&tsizes[c], sizeof(size_t), 1, fp);
		sizes[c] += tsizes[c];
		for (; ak < an; ++ak)
			putrec((RECHEADER *) *ak, fp);
	}
}
