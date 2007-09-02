/*	$OpenBSD: bm.c,v 1.7 2007/09/02 15:19:18 deraadt Exp $ */
/*-
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Andrew Hume of AT&T Bell Laboratories.
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

#include <bm.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* 
 * XXX
 * The default frequency table starts at 99 and counts down.  The default
 * table should probably be oriented toward text, and will necessarily be
 * locale specific.  This one is for English.  It was derived from the
 * OSF/1 and 4.4BSD formatted and unformatted manual pages, and about 100Mb
 * of email and random text.  Change it if you can find something better.
 */
static u_char const freq_def[256] = {
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0, 77, 90,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
	99, 28, 42, 27, 16, 14, 20, 51,
	66, 65, 59, 24, 75, 76, 84, 56,
	72, 74, 64, 55, 54, 47, 41, 37,
	44, 61, 70, 43, 23, 53, 49, 22,
	33, 58, 40, 46, 45, 57, 60, 26,
	30, 63, 21, 12, 32, 50, 38, 39,
	34, 11, 48, 67, 62, 35, 15, 29,
	71, 18,  9, 17, 25, 13, 10, 52,
	36, 95, 78, 86, 87, 98, 82, 80,
	88, 94, 19, 68, 89, 83, 93, 96,
	81,  7, 91, 92, 97, 85, 69, 73,
	31, 79,  8,  5,  4,  6,  3,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,
};

bm_pat *
bm_comp(u_char const *pb, size_t len, u_char const *freq)
{
	u_char const *pe, *p;
	size_t *d, r;
	int j;
	int sv_errno;
	bm_pat *pat;

	if (len == 0) {
		errno = EINVAL;
		return (NULL);
	}
	if ((pat = malloc(sizeof(*pat))) == NULL)
		return (NULL);
	pat->pat = NULL;
	pat->delta = NULL;

	pat->patlen = len;			/* copy pattern */
	if ((pat->pat = malloc(pat->patlen)) == NULL)
		goto mem;
	memcpy(pat->pat, pb, pat->patlen);
						/* get skip delta */
	if ((pat->delta = calloc(256, sizeof(*d))) == NULL)
		goto mem;
	for (j = 0, d = pat->delta; j < 256; j++)
		d[j] = pat->patlen;
	for (pe = pb + pat->patlen - 1; pb <= pe; pb++)
		d[*pb] = pe - pb;

	if (freq == NULL)			/* default freq table */
		freq = freq_def;
	r = 0;					/* get guard */
	for (pb = pat->pat, pe = pb + pat->patlen - 1; pb < pe; pb++)
		if (freq[*pb] < freq[pat->pat[r]])
			r = pb - pat->pat;
	pat->rarec = pat->pat[r];
	pat->rareoff = r - (pat->patlen - 1);

						/* get md2 shift */
	for (pe = pat->pat + pat->patlen - 1, p = pe - 1; p >= pat->pat; p--)
		if (*p == *pe)
			break;

	/* *p is first leftward reoccurrence of *pe */
	pat->md2 = pe - p;
	return (pat);

mem:	sv_errno = errno;
	bm_free(pat);
	errno = sv_errno;
	return (NULL);
}

void
bm_free(bm_pat *pat)
{
	if (pat->pat != NULL)
		free(pat->pat);
	if (pat->delta != NULL)
		free(pat->delta);
	free(pat);
}

u_char *
bm_exec(bm_pat *pat, u_char *base, size_t n)
{
	u_char *e, *ep, *p, *q, *s;
	size_t *d0, k, md2, n1, ro;
	int rc;

	if (n == 0)
		return (NULL);

	d0 = pat->delta;
	n1 = pat->patlen - 1;
	md2 = pat->md2;
	ro = pat->rareoff;
	rc = pat->rarec;
	ep = pat->pat + pat->patlen - 1;
	s = base + (pat->patlen - 1);

	/* fast loop up to n - 3 * patlen */
	e = base + n - 3 * pat->patlen;
	while (s < e) {
		k = d0[*s];		/* ufast skip loop */
		while (k) {
			k = d0[*(s += k)];
			k = d0[*(s += k)];
		}
		if (s >= e)
			break;
		if (s[ro] != rc)	/* guard test */
			goto mismatch1;
					/* fwd match */
		for (p = pat->pat, q = s - n1; p < ep;)
			if (*q++ != *p++)
				goto mismatch1;
		return (s - n1);

mismatch1:	s += md2;		/* md2 shift */
	}

	/* slow loop up to end */
	e = base + n;
	while (s < e) {
		s += d0[*s];		/* step */
		if (s >= e)
			break;
		if (s[ro] != rc)	/* guard test */
			goto mismatch2;
					/* fwd match */
		for (p = pat->pat, q = s - n1; p <= ep;)
			if (*q++ != *p++)
				goto mismatch2;
		return (s - n1);

mismatch2:	s += md2;		/* md2 shift */
	}

	return (NULL);
}
