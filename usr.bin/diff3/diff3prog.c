/*	$OpenBSD: diff3prog.c,v 1.11 2009/10/27 23:59:37 deraadt Exp $	*/

/*
 * Copyright (C) Caldera International Inc.  2001-2002.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code and documentation must retain the above
 *    copyright notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed or owned by Caldera
 *	International, Inc.
 * 4. Neither the name of Caldera International, Inc. nor the names of other
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * USE OF THE SOFTWARE PROVIDED FOR UNDER THIS LICENSE BY CALDERA
 * INTERNATIONAL, INC. AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL CALDERA INTERNATIONAL, INC. BE LIABLE FOR ANY DIRECT,
 * INDIRECT INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *
 *	@(#)diff3.c	8.1 (Berkeley) 6/6/93
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <err.h>

/* diff3 - 3-way differential file comparison */

/* diff3 [-ex3EX] d13 d23 f1 f2 f3 [m1 m3]
 *
 * d13 = diff report on f1 vs f3
 * d23 = diff report on f2 vs f3
 * f1, f2, f3 the 3 files
 * if changes in f1 overlap with changes in f3, m1 and m3 are used
 * to mark the overlaps; otherwise, the file names f1 and f3 are used
 * (only for options E and X).
 */

/*
 * "from" is first in range of changed lines; "to" is last+1
 * from=to=line after point of insertion for added lines.
 */
struct  range {
	int from;
	int to;
};
struct diff {
	struct range old;
	struct range new;
};

size_t szchanges;

struct diff *d13;
struct diff *d23;
/*
 * "de" is used to gather editing scripts.  These are later spewed out in
 * reverse order.  Its first element must be all zero, the "new" component
 * of "de" contains line positions or byte positions depending on when you
 * look (!?).  Array overlap indicates which sections in "de" correspond to
 * lines that are different in all three files.
 */
struct diff *de;
char *overlap;
int  overlapcnt;
FILE *fp[3];
int cline[3];		/* # of the last-read line in each file (0-2) */
/*
 * the latest known correspondence between line numbers of the 3 files
 * is stored in last[1-3];
 */
int last[4];
int eflag;
int oflag;		/* indicates whether to mark overlaps (-E or -X)*/
int debug  = 0;
char f1mark[40], f3mark[40];	/* markers for -E and -X */

int duplicate(struct range *, struct range *);
int edit(struct diff *, int, int);
char *getchange(FILE *);
char *getline(FILE *, size_t *);
int number(char **);
int readin(char *, struct diff **);
int skip(int, int, char *);
void change(int, struct range *, int);
void keep(int, struct range *);
void merge(int, int);
void prange(struct range *);
void repos(int);
void separate(const char *);
__dead void edscript(int);
__dead void trouble(void);
void increase(void);
__dead void usage(void);

int
main(int argc, char **argv)
{
	int ch, i, m, n;

	eflag = 0;
	oflag = 0;
	while ((ch = getopt(argc, argv, "EeXx3")) != -1) {
		switch (ch) {
		case 'E':
			eflag = 3;
			oflag = 1;
			break;
		case 'e':
			eflag = 3;
			break;
		case 'X':
			oflag = eflag = 1;
			break;
		case 'x':
			eflag = 1;
			break;
		case '3':
			eflag = 2;
			break;
		}
	}
	argc -= optind;
	argv += optind;
	/* XXX - argc usage seems wrong here */
	if (argc < 5)
		usage();

	if (oflag) {
		(void)snprintf(f1mark, sizeof(f1mark), "<<<<<<< %s",
		    argc >= 6 ? argv[5] : argv[2]);
		(void)snprintf(f3mark, sizeof(f3mark), ">>>>>>> %s",
		    argc >= 7 ? argv[6] : argv[4]);
	}

	increase();
	m = readin(argv[0], &d13);
	n = readin(argv[1], &d23);
	for (i = 0; i <= 2; i++) {
		if ((fp[i] = fopen(argv[i + 2], "r")) == NULL)
			err(EXIT_FAILURE, "can't open %s", argv[i + 2]);
	}
	merge(m, n);
	exit(EXIT_SUCCESS);
}

/*
 * Pick up the line numbers of all changes from one change file.
 * (This puts the numbers in a vector, which is not strictly necessary,
 * since the vector is processed in one sequential pass.
 * The vector could be optimized out of existence)
 */
int
readin(char *name, struct diff **dd)
{
	int a, b, c, d, i;
	char kind, *p;

	fp[0] = fopen(name, "r");
	if (fp[0] == NULL)
		err(EXIT_FAILURE, "can't open %s", name);
	for (i=0; (p = getchange(fp[0])); i++) {
		if (i >= szchanges - 1)
			increase();
		a = b = number(&p);
		if (*p == ',') {
			p++;
			b = number(&p);
		}
		kind = *p++;
		c = d = number(&p);
		if (*p==',') {
			p++;
			d = number(&p);
		}
		if (kind == 'a')
			a++;
		if (kind == 'd')
			c++;
		b++;
		d++;
		(*dd)[i].old.from = a;
		(*dd)[i].old.to = b;
		(*dd)[i].new.from = c;
		(*dd)[i].new.to = d;
	}
	if (i) {
		(*dd)[i].old.from = (*dd)[i-1].old.to;
		(*dd)[i].new.from = (*dd)[i-1].new.to;
	}
	(void)fclose(fp[0]);
	return (i);
}

int
number(char **lc)
{
	int nn;
	nn = 0;
	while (isdigit((unsigned char)(**lc)))
		nn = nn*10 + *(*lc)++ - '0';
	return (nn);
}

char *
getchange(FILE *b)
{
	char *line;

	while ((line = getline(b, NULL))) {
		if (isdigit((unsigned char)line[0]))
			return (line);
	}
	return (NULL);
}

char *
getline(FILE *b, size_t *n)
{
	char *cp;
	size_t len;
	static char *buf;
	static size_t bufsize;

	if ((cp = fgetln(b, &len)) == NULL)
		return (NULL);

	if (cp[len - 1] != '\n')
		len++;
	if (len + 1 > bufsize) {
		do {
			bufsize += 1024;
		} while (len + 1 > bufsize);
		if ((buf = realloc(buf, bufsize)) == NULL)
			err(EXIT_FAILURE, NULL);
	}
	memcpy(buf, cp, len - 1);
	buf[len - 1] = '\n';
	buf[len] = '\0';
	if (n != NULL)
		*n = len;
	return (buf);
}

void
merge(int m1, int m2)
{
	struct diff *d1, *d2, *d3;
	int dup, j, t1, t2;

	d1 = d13;
	d2 = d23;
	j = 0;
	while ((t1 = d1 < d13 + m1) | (t2 = d2 < d23 + m2)) {
		if (debug) {
			printf("%d,%d=%d,%d %d,%d=%d,%d\n",
			d1->old.from,d1->old.to,
			d1->new.from,d1->new.to,
			d2->old.from,d2->old.to,
			d2->new.from,d2->new.to);
		}
		/* first file is different from others */
		if (!t2 || (t1 && d1->new.to < d2->new.from)) {
			/* stuff peculiar to 1st file */
			if (eflag==0) {
				separate("1");
				change(1, &d1->old, 0);
				keep(2, &d1->new);
				change(3, &d1->new, 0);
			}
			d1++;
			continue;
		}
		/* second file is different from others */
		if (!t1 || (t2 && d2->new.to < d1->new.from)) {
			if (eflag==0) {
				separate("2");
				keep(1, &d2->new);
				change(2, &d2->old, 0);
				change(3, &d2->new, 0);
			}
			d2++;
			continue;
		}
		/*
		 * Merge overlapping changes in first file
		 * this happens after extension (see below).
		 */
		if (d1 + 1 < d13 + m1 && d1->new.to >= d1[1].new.from) {
			d1[1].old.from = d1->old.from;
			d1[1].new.from = d1->new.from;
			d1++;
			continue;
		}

		/* merge overlapping changes in second */
		if (d2 + 1 < d23 + m2 && d2->new.to >= d2[1].new.from) {
			d2[1].old.from = d2->old.from;
			d2[1].new.from = d2->new.from;
			d2++;
			continue;
		}
		/* stuff peculiar to third file or different in all */
		if (d1->new.from == d2->new.from && d1->new.to == d2->new.to) {
			dup = duplicate(&d1->old,&d2->old);
			/*
			 * dup = 0 means all files differ
			 * dup = 1 means files 1 and 2 identical
			 */
			if (eflag==0) {
				separate(dup ? "3" : "");
				change(1, &d1->old, dup);
				change(2, &d2->old, 0);
				d3 = d1->old.to > d1->old.from ? d1 : d2;
				change(3, &d3->new, 0);
			} else
				j = edit(d1, dup, j);
			d1++;
			d2++;
			continue;
		}
		/*
		 * Overlapping changes from file 1 and 2; extend changes
		 * appropriately to make them coincide.
		 */
		if (d1->new.from < d2->new.from) {
			d2->old.from -= d2->new.from-d1->new.from;
			d2->new.from = d1->new.from;
		} else if (d2->new.from < d1->new.from) {
			d1->old.from -= d1->new.from-d2->new.from;
			d1->new.from = d2->new.from;
		}
		if (d1->new.to > d2->new.to) {
			d2->old.to += d1->new.to - d2->new.to;
			d2->new.to = d1->new.to;
		} else if (d2->new.to > d1->new.to) {
			d1->old.to += d2->new.to - d1->new.to;
			d1->new.to = d2->new.to;
		}
	}
	if (eflag)
		edscript(j);
}

void
separate(const char *s)
{
	printf("====%s\n", s);
}

/*
 * The range of lines rold.from thru rold.to in file i is to be changed.
 * It is to be printed only if it does not duplicate something to be
 * printed later.
 */
void
change(int i, struct range *rold, int dup)
{
	printf("%d:", i);
	last[i] = rold->to;
	prange(rold);
	if (dup || debug)
		return;
	i--;
	(void)skip(i, rold->from, NULL);
	(void)skip(i, rold->to, "  ");
}

/*
 * print the range of line numbers, rold.from thru rold.to, as n1,n2 or n1
 */
void
prange(struct range *rold)
{
	if (rold->to <= rold->from)
		printf("%da\n", rold->from - 1);
	else {
		printf("%d", rold->from);
		if (rold->to > rold->from+1)
			printf(",%d", rold->to - 1);
		printf("c\n");
	}
}

/*
 * No difference was reported by diff between file 1 (or 2) and file 3,
 * and an artificial dummy difference (trange) must be ginned up to
 * correspond to the change reported in the other file.
 */
void
keep(int i, struct range *rnew)
{
	int delta;
	struct range trange;

	delta = last[3] - last[i];
	trange.from = rnew->from - delta;
	trange.to = rnew->to - delta;
	change(i, &trange, 1);
}

/*
 * skip to just before line number from in file "i".  If "pr" is non-NULL,
 * print all skipped stuff with string pr as a prefix.
 */
int
skip(int i, int from, char *pr)
{
	size_t j, n;
	char *line;

	for (n = 0; cline[i] < from - 1; n += j) {
		if ((line = getline(fp[i], &j)) == NULL)
			trouble();
		if (pr != NULL)
			printf("%s%s", pr, line);
		cline[i]++;
	}
	return ((int) n);
}

/*
 * Return 1 or 0 according as the old range (in file 1) contains exactly
 * the same data as the new range (in file 2).
 */
int
duplicate(struct range *r1, struct range *r2)
{
	int c,d;
	int nchar;
	int nline;

	if (r1->to-r1->from != r2->to-r2->from)
		return (0);
	(void)skip(0, r1->from, NULL);
	(void)skip(1, r2->from, NULL);
	nchar = 0;
	for (nline=0; nline < r1->to - r1->from; nline++) {
		do {
			c = getc(fp[0]);
			d = getc(fp[1]);
			if (c == -1 || d== -1)
				trouble();
			nchar++;
			if (c != d) {
				repos(nchar);
				return (0);
			}
		} while (c != '\n');
	}
	repos(nchar);
	return (1);
}

void
repos(int nchar)
{
	int i;

	for (i = 0; i < 2; i++)
		(void)fseek(fp[i], (long)-nchar, SEEK_CUR);
}

__dead void
trouble(void)
{
	errx(EXIT_FAILURE, "logic error");
}

/*
 * collect an editing script for later regurgitation
 */
int
edit(struct diff *diff, int dup, int j)
{
	if (((dup + 1) & eflag) == 0)
		return (j);
	j++;
	overlap[j] = !dup;
	if (!dup)
		overlapcnt++;
	de[j].old.from = diff->old.from;
	de[j].old.to = diff->old.to;
	de[j].new.from = de[j-1].new.to + skip(2, diff->new.from, NULL);
	de[j].new.to = de[j].new.from + skip(2, diff->new.to, NULL);
	return (j);
}

/* regurgitate */
__dead void
edscript(int n)
{
	int j,k;
	char block[BUFSIZ];

	for (n = n; n > 0; n--) {
		if (!oflag || !overlap[n])
			prange(&de[n].old);
		else
			printf("%da\n=======\n", de[n].old.to -1);
		(void)fseek(fp[2], (long)de[n].new.from, SEEK_SET);
		for (k = de[n].new.to-de[n].new.from; k > 0; k-= j) {
			j = k > BUFSIZ ? BUFSIZ : k;
			if (fread(block, 1, j, fp[2]) != j)
				trouble();
			(void)fwrite(block, 1, j, stdout);
		}
		if (!oflag || !overlap[n])
			printf(".\n");
		else {
			printf("%s\n.\n", f3mark);
			printf("%da\n%s\n.\n", de[n].old.from - 1, f1mark);
		}
	}
	exit(overlapcnt);
}

void
increase(void)
{
	struct diff *p;
	char *q;
	size_t newsz, incr;

	/* are the memset(3) calls needed? */
	newsz = szchanges == 0 ? 64 : 2 * szchanges;
	incr = newsz - szchanges;

	p = realloc(d13, newsz * sizeof(struct diff));
	if (p == NULL)
		err(1, NULL);
	memset(p + szchanges, 0, incr * sizeof(struct diff));
	d13 = p;
	p = realloc(d23, newsz * sizeof(struct diff));
	if (p == NULL)
		err(1, NULL);
	memset(p + szchanges, 0, incr * sizeof(struct diff));
	d23 = p;
	p = realloc(de, newsz * sizeof(struct diff));
	if (p == NULL)
		err(1, NULL);
	memset(p + szchanges, 0, incr * sizeof(struct diff));
	de = p;
	q = realloc(overlap, newsz * sizeof(char));
	if (q == NULL)
		err(1, NULL);
	memset(q + szchanges, 0, incr * sizeof(char));
	overlap = q;
	szchanges = newsz;
}


__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-exEX3] /tmp/d3a.?????????? "
	    "/tmp/d3b.?????????? file1 file2 file3\n", __progname);
	exit(EXIT_FAILURE);
}
