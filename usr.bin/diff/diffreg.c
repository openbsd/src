/*	$OpenBSD: diffreg.c,v 1.26 2003/07/04 17:37:07 millert Exp $	*/

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

#include <sys/types.h>

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "diff.h"
#include "pathnames.h"

#if 0
static char const sccsid[] = "@(#)diffreg.c 4.21 4/6/90";
#endif

/*
 * diff - compare two files.
 */

/*
 *	Uses an algorithm due to Harold Stone, which finds
 *	a pair of longest identical subsequences in the two
 *	files.
 *
 *	The major goal is to generate the match vector J.
 *	J[i] is the index of the line in file1 corresponding
 *	to line i file0. J[i] = 0 if there is no
 *	such line in file1.
 *
 *	Lines are hashed so as to work in core. All potential
 *	matches are located by sorting the lines of each file
 *	on the hash (called ``value''). In particular, this
 *	collects the equivalence classes in file1 together.
 *	Subroutine equiv replaces the value of each line in
 *	file0 by the index of the first element of its
 *	matching equivalence in (the reordered) file1.
 *	To save space equiv squeezes file1 into a single
 *	array member in which the equivalence classes
 *	are simply concatenated, except that their first
 *	members are flagged by changing sign.
 *
 *	Next the indices that point into member are unsorted into
 *	array class according to the original order of file0.
 *
 *	The cleverness lies in routine stone. This marches
 *	through the lines of file0, developing a vector klist
 *	of "k-candidates". At step i a k-candidate is a matched
 *	pair of lines x,y (x in file0 y in file1) such that
 *	there is a common subsequence of length k
 *	between the first i lines of file0 and the first y
 *	lines of file1, but there is no such subsequence for
 *	any smaller y. x is the earliest possible mate to y
 *	that occurs in such a subsequence.
 *
 *	Whenever any of the members of the equivalence class of
 *	lines in file1 matable to a line in file0 has serial number
 *	less than the y of some k-candidate, that k-candidate
 *	with the smallest such y is replaced. The new
 *	k-candidate is chained (via pred) to the current
 *	k-1 candidate so that the actual subsequence can
 *	be recovered. When a member has serial number greater
 *	that the y of all k-candidates, the klist is extended.
 *	At the end, the longest subsequence is pulled out
 *	and placed in the array J by unravel
 *
 *	With J in hand, the matches there recorded are
 *	check'ed against reality to assure that no spurious
 *	matches have crept in due to hashing. If they have,
 *	they are broken, and "jackpot" is recorded--a harmless
 *	matter except that a true match for a spuriously
 *	mated line may now be unnecessarily reported as a change.
 *
 *	Much of the complexity of the program comes simply
 *	from trying to minimize core utilization and
 *	maximize the range of doable problems by dynamically
 *	allocating what is needed and reusing what is not.
 *	The core requirements for problems larger than somewhat
 *	are (in words) 2*length(file0) + length(file1) +
 *	3*(number of k-candidates installed),  typically about
 *	6n words for files of length n.
 */

#define	prints(s)	fputs(s,stdout)

FILE *input[2];

struct cand {
	int x;
	int y;
	int pred;
} cand;

struct line {
	int serial;
	int value;
} *file[2];

int len[2];
struct line *sfile[2];	/* shortened by pruning common prefix and suffix */
int slen[2];
int pref, suff;			/* length of prefix and suffix */
int inifdef;			/* whether or not we are in a #ifdef block */
int *class;			/* will be overlaid on file[0] */
int *member;			/* will be overlaid on file[1] */
int *klist;			/* will be overlaid on file[0] after class */
struct cand *clist;		/* merely a free storage pot for candidates */
int clen = 0;
int *J;				/* will be overlaid on class */
long *ixold;			/* will be overlaid on klist */
long *ixnew;			/* will be overlaid on file[1] */
u_char *chrtran;		/* translation table for case-folding */

static void fetch(long *, int, int, FILE *, char *, int);
static void output(void);
static void check(void);
static void range(int, int, char *);
static void dump_context_vec(void);
static void dump_unified_vec(void);
static void prepare(int, FILE *);
static void prune(void);
static void equiv(struct line *, int, struct line *, int, int *);
static void unravel(int);
static void unsort(struct line *, int, int *);
static void change(int, int, int, int);
static void sort(struct line *, int);
static int newcand(int, int, int);
static int search(int *, int, int);
static int skipline(int);
static int asciifile(FILE *);
static int stone(int *, int, int *, int *);
static int readhash(FILE *);

/*
 * chrtran points to one of 2 translation tables: cup2low if folding upper to
 * lower case clow2low if not folding case
 */
u_char clow2low[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
	0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
	0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
	0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
	0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x41,
	0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c,
	0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
	0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62,
	0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d,
	0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
	0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83,
	0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e,
	0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
	0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4,
	0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba,
	0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5,
	0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0,
	0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb,
	0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6,
	0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1,
	0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc,
	0xfd, 0xfe, 0xff
};

u_char cup2low[256] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
	0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
	0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
	0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
	0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x60, 0x61,
	0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c,
	0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
	0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x60, 0x61, 0x62,
	0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d,
	0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
	0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83,
	0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e,
	0x8f, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
	0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4,
	0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
	0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba,
	0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5,
	0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0,
	0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb,
	0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6,
	0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1,
	0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc,
	0xfd, 0xfe, 0xff
};

void
diffreg(void)
{
	char buf1[BUFSIZ], buf2[BUFSIZ];
	FILE *f1, *f2;
	int i, j;

	chrtran = (iflag ? cup2low : clow2low);
	if (strcmp(file1, "-") == 0 && strcmp(file2, "-") == 0)
		errorx("can't specify - -");
	if (S_ISDIR(stb1.st_mode)) {
		file1 = splice(file1, file2);
		if (stat(file1, &stb1) < 0)
			error("%s", file1);
	} else if (strcmp(file1, "-") == 0 ||
	    (!S_ISREG(stb1.st_mode) && strcmp(file1, _PATH_DEVNULL) != 0)) {
		file1 = copytemp(file1, 1);
		if (stat(file1, &stb1) < 0)
			error("%s", file1);
	}
	if (S_ISDIR(stb2.st_mode)) {
		file2 = splice(file2, file1);
		if (stat(file2, &stb2) < 0)
			error("%s", file2);
	} else if (strcmp(file2, "-") == 0 ||
	    (!S_ISREG(stb2.st_mode) && strcmp(file2, _PATH_DEVNULL) != 0)) {
		file2 = copytemp(file2, 2);
		if (stat(file2, &stb2) < 0)
			error("%s", file2);
	}
	if ((f1 = fopen(file1, "r")) == NULL)
		error("%s", file1);
	if ((f2 = fopen(file2, "r")) == NULL)
		error("%s", file2);
	if ((stb1.st_mode & S_IFMT) != (stb2.st_mode & S_IFMT) ||
	    stb1.st_size != stb2.st_size)
		goto notsame;
	for (;;) {
		i = fread(buf1, 1, BUFSIZ, f1);
		j = fread(buf2, 1, BUFSIZ, f2);
		if (i < 0 || j < 0 || i != j)
			goto notsame;
		if (i == 0 && j == 0) {
			fclose(f1);
			fclose(f2);
			status = 0;	/* files don't differ */
			goto same;
		}
		for (j = 0; j < i; j++)
			if (buf1[j] != buf2[j])
				goto notsame;
	}
notsame:
	/*
	 * Files certainly differ at this point; set status accordingly
	 */
	status = 1;
	if (!asciifile(f1) || !asciifile(f2)) {
		printf("Binary files %s and %s differ\n", file1, file2);
		exit(status);
	}
	prepare(0, f1);
	prepare(1, f2);
	fclose(f1);
	fclose(f2);
	prune();
	sort(sfile[0], slen[0]);
	sort(sfile[1], slen[1]);

	member = (int *)file[1];
	equiv(sfile[0], slen[0], sfile[1], slen[1], member);
	member = erealloc(member, (slen[1] + 2) * sizeof(int));

	class = (int *)file[0];
	unsort(sfile[0], slen[0], class);
	class = erealloc(class, (slen[0] + 2) * sizeof(int));

	klist = emalloc((slen[0] + 2) * sizeof(int));
	clist = emalloc(sizeof(cand));
	i = stone(class, slen[0], member, klist);
	free(member);
	free(class);

	J = emalloc((len[0] + 2) * sizeof(int));
	unravel(klist[i]);
	free(clist);
	free(klist);

	ixold = emalloc((len[0] + 2) * sizeof(long));
	ixnew = emalloc((len[1] + 2) * sizeof(long));
	check();
	output();
	status = anychange;
same:
	if (anychange == 0 && (opt == D_CONTEXT || opt == D_UNIFIED))
		printf("No differences encountered\n");
}

char *tempfiles[2];

char *
copytemp(const char *file, int n)
{
	char buf[BUFSIZ], *tempdir, *tempfile;
	int i, ifd, ofd;

	if (n != 1 && n != 2)
		return (NULL);

	if (strcmp(file, "-") == 0)
		ifd = STDIN_FILENO;
	else if ((ifd = open(file, O_RDONLY, 0644)) < 0)
		error("%s", file);

	if ((tempdir = getenv("TMPDIR")) == NULL)
		tempdir = _PATH_TMP;
	if (asprintf(&tempfile, "%s/diff%d.XXXXXXXX", tempdir, n) == -1)
		error(NULL);
	tempfiles[n - 1] = tempfile;

	signal(SIGHUP, done);
	signal(SIGINT, done);
	signal(SIGPIPE, done);
	signal(SIGTERM, done);
	ofd = mkstemp(tempfile);
	if (ofd < 0)
		error("%s", tempfile);
	while ((i = read(ifd, buf, BUFSIZ)) > 0) {
		if (write(ofd, buf, i) != i)
			error("%s", tempfile);
	}
	close(ifd);
	close(ofd);
	return (tempfile);
}

char *
splice(char *dir, char *file)
{
	char *tail, *buf;
	size_t len;

	if (!strcmp(file, "-"))
		errorx("can't specify - with other arg directory");
	tail = strrchr(file, '/');
	if (tail == NULL)
		tail = file;
	else
		tail++;
	len = strlen(dir) + 1 + strlen(tail) + 1;
	buf = emalloc(len);
	snprintf(buf, len, "%s/%s", dir, tail);
	return (buf);
}

static void
prepare(int i, FILE *fd)
{
	struct line *p;
	int j, h;

	fseek(fd, 0L, SEEK_SET);
	p = emalloc(3 * sizeof(struct line));
	for (j = 0; (h = readhash(fd));) {
		p = erealloc(p, (++j + 3) * sizeof(struct line));
		p[j].value = h;
	}
	len[i] = j;
	file[i] = p;
}

static void
prune(void)
{
	int i, j;

	for (pref = 0; pref < len[0] && pref < len[1] &&
	    file[0][pref + 1].value == file[1][pref + 1].value;
	    pref++)
		;
	for (suff = 0; suff < len[0] - pref && suff < len[1] - pref &&
	    file[0][len[0] - suff].value == file[1][len[1] - suff].value;
	    suff++)
		;
	for (j = 0; j < 2; j++) {
		sfile[j] = file[j] + pref;
		slen[j] = len[j] - pref - suff;
		for (i = 0; i <= slen[j]; i++)
			sfile[j][i].serial = i;
	}
}

static void
equiv(struct line *a, int n, struct line *b, int m, int *c)
{
	int i, j;

	i = j = 1;
	while (i <= n && j <= m) {
		if (a[i].value < b[j].value)
			a[i++].value = 0;
		else if (a[i].value == b[j].value)
			a[i++].value = j;
		else
			j++;
	}
	while (i <= n)
		a[i++].value = 0;
	b[m + 1].value = 0;
	j = 0;
	while (++j <= m) {
		c[j] = -b[j].serial;
		while (b[j + 1].value == b[j].value) {
			j++;
			c[j] = b[j].serial;
		}
	}
	c[j] = -1;
}

static int
stone(int *a, int n, int *b, int *c)
{
	int i, k, y, j, l;
	int oldc, tc, oldl;

	k = 0;
	c[0] = newcand(0, 0, 0);
	for (i = 1; i <= n; i++) {
		j = a[i];
		if (j == 0)
			continue;
		y = -b[j];
		oldl = 0;
		oldc = c[0];
		do {
			if (y <= clist[oldc].y)
				continue;
			l = search(c, k, y);
			if (l != oldl + 1)
				oldc = c[l - 1];
			if (l <= k) {
				if (clist[c[l]].y <= y)
					continue;
				tc = c[l];
				c[l] = newcand(i, y, oldc);
				oldc = tc;
				oldl = l;
			} else {
				c[l] = newcand(i, y, oldc);
				k++;
				break;
			}
		} while ((y = b[++j]) > 0);
	}
	return (k);
}

static int
newcand(int x, int y, int pred)
{
	struct cand *q;

	clist = erealloc(clist, ++clen * sizeof(cand));
	q = clist + clen - 1;
	q->x = x;
	q->y = y;
	q->pred = pred;
	return (clen - 1);
}

static int
search(int *c, int k, int y)
{
	int i, j, l, t;

	if (clist[c[k]].y < y)	/* quick look for typical case */
		return (k + 1);
	i = 0;
	j = k + 1;
	while (1) {
		l = i + j;
		if ((l >>= 1) <= i)
			break;
		t = clist[c[l]].y;
		if (t > y)
			j = l;
		else if (t < y)
			i = l;
		else
			return (l);
	}
	return (l + 1);
}

static void
unravel(int p)
{
	struct cand *q;
	int i;

	for (i = 0; i <= len[0]; i++)
		J[i] = i <= pref ? i :
		    i > len[0] - suff ? i + len[1] - len[0] : 0;
	for (q = clist + p; q->y != 0; q = clist + q->pred)
		J[q->x + pref] = q->y + pref;
}

/*
 * Check does double duty:
 *  1.	ferret out any fortuitous correspondences due
 *	to confounding by hashing (which result in "jackpot")
 *  2.  collect random access indexes to the two files
 */
static void
check(void)
{
	int i, j, jackpot, c, d;
	long ctold, ctnew;

	if ((input[0] = fopen(file1, "r")) == NULL)
		error("%s", file1);
	if ((input[1] = fopen(file2, "r")) == NULL)
		error("%s", file2);
	j = 1;
	ixold[0] = ixnew[0] = 0;
	jackpot = 0;
	ctold = ctnew = 0;
	for (i = 1; i <= len[0]; i++) {
		if (J[i] == 0) {
			ixold[i] = ctold += skipline(0);
			continue;
		}
		while (j < J[i]) {
			ixnew[j] = ctnew += skipline(1);
			j++;
		}
		if (bflag || wflag || iflag) {
			for (;;) {
				c = getc(input[0]);
				d = getc(input[1]);
				ctold++;
				ctnew++;
				if (bflag && isspace(c) && isspace(d)) {
					do {
						if (c == '\n')
							break;
						ctold++;
					} while (isspace(c = getc(input[0])));
					do {
						if (d == '\n')
							break;
						ctnew++;
					} while (isspace(d = getc(input[1])));
				} else if (wflag) {
					while (isspace(c) && c != '\n') {
						c = getc(input[0]);
						ctold++;
					}
					while (isspace(d) && d != '\n') {
						d = getc(input[1]);
						ctnew++;
					}
				}
				if (chrtran[c] != chrtran[d]) {
					jackpot++;
					J[i] = 0;
					if (c != '\n')
						ctold += skipline(0);
					if (d != '\n')
						ctnew += skipline(1);
					break;
				}
				if (c == '\n')
					break;
			}
		} else {
			for (;;) {
				ctold++;
				ctnew++;
				if ((c = getc(input[0])) != (d = getc(input[1]))) {
					/* jackpot++; */
					J[i] = 0;
					if (c != '\n')
						ctold += skipline(0);
					if (d != '\n')
						ctnew += skipline(1);
					break;
				}
				if (c == '\n')
					break;
			}
		}
		ixold[i] = ctold;
		ixnew[j] = ctnew;
		j++;
	}
	for (; j <= len[1]; j++) {
		ixnew[j] = ctnew += skipline(1);
	}
	fclose(input[0]);
	fclose(input[1]);
	/*
	 * if (jackpot)
	 *	fprintf(stderr, "jackpot\n");
	 */
}

/* shellsort CACM #201 */
static void
sort(struct line *a, int n)
{
	struct line *ai, *aim, w;
	int j, m = 0, k;

	if (n == 0)
		return;
	for (j = 1; j <= n; j *= 2)
		m = 2 * j - 1;
	for (m /= 2; m != 0; m /= 2) {
		k = n - m;
		for (j = 1; j <= k; j++) {
			for (ai = &a[j]; ai > a; ai -= m) {
				aim = &ai[m];
				if (aim < ai)
					break;	/* wraparound */
				if (aim->value > ai[0].value ||
				    (aim->value == ai[0].value &&
					aim->serial > ai[0].serial))
					break;
				w.value = ai[0].value;
				ai[0].value = aim->value;
				aim->value = w.value;
				w.serial = ai[0].serial;
				ai[0].serial = aim->serial;
				aim->serial = w.serial;
			}
		}
	}
}

static void
unsort(struct line *f, int l, int *b)
{
	int *a, i;

	a = emalloc((l + 1) * sizeof(int));
	for (i = 1; i <= l; i++)
		a[f[i].serial] = f[i].value;
	for (i = 1; i <= l; i++)
		b[i] = a[i];
	free(a);
}

static int
skipline(int f)
{
	int i, c;

	for (i = 1; (c = getc(input[f])) != '\n'; i++)
		if (c < 0)
			return (i);
	return (i);
}

static void
output(void)
{
	int m, i0, i1, j0, j1;

	input[0] = fopen(file1, "r");
	input[1] = fopen(file2, "r");
	m = len[0];
	J[0] = 0;
	J[m + 1] = len[1] + 1;
	if (opt != D_EDIT) {
		for (i0 = 1; i0 <= m; i0 = i1 + 1) {
			while (i0 <= m && J[i0] == J[i0 - 1] + 1)
				i0++;
			j0 = J[i0 - 1] + 1;
			i1 = i0 - 1;
			while (i1 < m && J[i1 + 1] == 0)
				i1++;
			j1 = J[i1 + 1] - 1;
			J[i1] = j1;
			change(i0, i1, j0, j1);
		}
	} else {
		for (i0 = m; i0 >= 1; i0 = i1 - 1) {
			while (i0 >= 1 && J[i0] == J[i0 + 1] - 1 && J[i0] != 0)
				i0--;
			j0 = J[i0 + 1] - 1;
			i1 = i0 + 1;
			while (i1 > 1 && J[i1 - 1] == 0)
				i1--;
			j1 = J[i1 - 1] + 1;
			J[i1] = j1;
			change(i1, i0, j1, j0);
		}
	}
	if (m == 0)
		change(1, 0, 1, len[1]);
	if (opt == D_IFDEF) {
		for (;;) {
#define	c i0
			c = getc(input[0]);
			if (c < 0)
				return;
			putchar(c);
		}
#undef c
	}
	if (anychange != 0) {
		if (opt == D_CONTEXT)
			dump_context_vec();
		else if (opt == D_UNIFIED)
			dump_unified_vec();
	}
}

/*
 * The following struct is used to record change information when
 * doing a "context" diff.  (see routine "change" to understand the
 * highly mneumonic field names)
 */
struct context_vec {
	int a;			/* start line in old file */
	int b;			/* end line in old file */
	int c;			/* start line in new file */
	int d;			/* end line in new file */
};

struct context_vec *context_vec_start, *context_vec_end, *context_vec_ptr;

#define	MAX_CONTEXT	128

/*
 * indicate that there is a difference between lines a and b of the from file
 * to get to lines c to d of the to file. If a is greater then b then there
 * are no lines in the from file involved and this means that there were
 * lines appended (beginning at b). If c is greater than d then there are
 * lines missing from the to file.
 */
static void
change(int a, int b, int c, int d)
{
	struct stat stbuf;

	if (opt != D_IFDEF && a > b && c > d)
		return;
	if (anychange == 0) {
		anychange = 1;
		if (opt == D_CONTEXT || opt == D_UNIFIED) {
			stat(file1, &stbuf);
			printf("%s %s	%s", opt == D_CONTEXT ? "***" : "---",
			   file1, ctime(&stbuf.st_mtime));
			stat(file2, &stbuf);
			printf("%s %s	%s", opt == D_CONTEXT ? "---" : "+++",
			    file2, ctime(&stbuf.st_mtime));
			context_vec_start = emalloc(MAX_CONTEXT *
			    sizeof(struct context_vec));
			context_vec_end = context_vec_start + MAX_CONTEXT;
			context_vec_ptr = context_vec_start - 1;
		}
	}
	if (opt == D_CONTEXT || opt == D_UNIFIED) {
		/*
		 * If this new change is within 'context' lines of
		 * the previous change, just add it to the change
		 * record.  If the record is full or if this
		 * change is more than 'context' lines from the previous
		 * change, dump the record, reset it & add the new change.
		 */
		if (context_vec_ptr >= context_vec_end ||
		    (context_vec_ptr >= context_vec_start &&
		    a > (context_vec_ptr->b + 2 * context) &&
		    c > (context_vec_ptr->d + 2 * context))) {
			if (opt == D_CONTEXT)
				dump_context_vec();
			else
				dump_unified_vec();
		}
		context_vec_ptr++;
		context_vec_ptr->a = a;
		context_vec_ptr->b = b;
		context_vec_ptr->c = c;
		context_vec_ptr->d = d;
		return;
	}
	switch (opt) {

	case D_NORMAL:
	case D_EDIT:
		range(a, b, ",");
		putchar(a > b ? 'a' : c > d ? 'd' : 'c');
		if (opt == D_NORMAL)
			range(c, d, ",");
		putchar('\n');
		break;
	case D_REVERSE:
		putchar(a > b ? 'a' : c > d ? 'd' : 'c');
		range(a, b, " ");
		putchar('\n');
		break;
	case D_NREVERSE:
		if (a > b)
			printf("a%d %d\n", b, d - c + 1);
		else {
			printf("d%d %d\n", a, b - a + 1);
			if (!(c > d))
				/* add changed lines */
				printf("a%d %d\n", b, d - c + 1);
		}
		break;
	}
	if (opt == D_NORMAL || opt == D_IFDEF) {
		fetch(ixold, a, b, input[0], "< ", 1);
		if (a <= b && c <= d && opt == D_NORMAL)
			prints("---\n");
	}
	fetch(ixnew, c, d, input[1], opt == D_NORMAL ? "> " : "", 0);
	if ((opt == D_EDIT || opt == D_REVERSE) && c <= d)
		prints(".\n");
	if (inifdef) {
		fprintf(stdout, "#endif /* %s */\n", ifdefname);
		inifdef = 0;
	}
}

static void
range(int a, int b, char *separator)
{
	printf("%d", a > b ? b : a);
	if (a < b)
		printf("%s%d", separator, b);
}

static void
fetch(long *f, int a, int b, FILE *lb, char *s, int oldfile)
{
	int i, j, c, col, nc;

	/*
	 * When doing #ifdef's, copy down to current line
	 * if this is the first file, so that stuff makes it to output.
	 */
	if (opt == D_IFDEF && oldfile) {
		long curpos = ftell(lb);
		/* print through if append (a>b), else to (nb: 0 vs 1 orig) */
		nc = f[a > b ? b : a - 1] - curpos;
		for (i = 0; i < nc; i++)
			putchar(getc(lb));
	}
	if (a > b)
		return;
	if (opt == D_IFDEF) {
		if (inifdef) {
			fprintf(stdout, "#else /* %s%s */\n",
			    oldfile == 1 ? "!" : "", ifdefname);
		} else {
			if (oldfile)
				fprintf(stdout, "#ifndef %s\n", ifdefname);
			else
				fprintf(stdout, "#ifdef %s\n", ifdefname);
		}
		inifdef = 1 + oldfile;
	}
	for (i = a; i <= b; i++) {
		fseek(lb, f[i - 1], SEEK_SET);
		nc = f[i] - f[i - 1];
		if (opt != D_IFDEF)
			prints(s);
		col = 0;
		for (j = 0; j < nc; j++) {
			c = getc(lb);
			if (c == '\t' && tflag)
				do
					putchar(' ');
				while (++col & 7);
			else {
				putchar(c);
				col++;
			}
		}
	}
}

#define POW2			/* define only if HALFLONG is 2**n */
#define HALFLONG 16
#define low(x)	(x&((1L<<HALFLONG)-1))
#define high(x)	(x>>HALFLONG)

/*
 * hashing has the effect of
 * arranging line in 7-bit bytes and then
 * summing 1-s complement in 16-bit hunks
 */
static int
readhash(FILE *f)
{
	unsigned int shift;
	int t, space;
	long sum;

	sum = 1;
	space = 0;
	if (!bflag && !wflag) {
		if (iflag)
			for (shift = 0; (t = getc(f)) != '\n'; shift += 7) {
				if (t == -1)
					return (0);
				sum += (long)chrtran[t] << (shift
#ifdef POW2
				    &= HALFLONG - 1);
#else
				    %= HALFLONG);
#endif
			}
		else
			for (shift = 0; (t = getc(f)) != '\n'; shift += 7) {
				if (t == -1)
					return (0);
				sum += (long)t << (shift
#ifdef POW2
				    &= HALFLONG - 1);
#else
				    %= HALFLONG);
#endif
			}
	} else {
		for (shift = 0;;) {
			switch (t = getc(f)) {
			case -1:
				return (0);
			case '\t':
			case ' ':
				space++;
				continue;
			default:
				if (space && !wflag) {
					shift += 7;
					space = 0;
				}
				sum += (long)chrtran[t] << (shift
#ifdef POW2
				    &= HALFLONG - 1);
#else
				    %= HALFLONG);
#endif
				shift += 7;
				continue;
			case '\n':
				break;
			}
			break;
		}
	}
	sum = low(sum) + high(sum);
	return ((short) low(sum) + (short) high(sum));
}

static int
asciifile(FILE *f)
{
	char buf[BUFSIZ], *cp;
	int cnt;

	if (aflag)
		return (1);

	fseek(f, 0L, SEEK_SET);
	cnt = fread(buf, 1, BUFSIZ, f);
	cp = buf;
	while (--cnt >= 0)
		if (*cp++ & 0200)
			return (0);
	return (1);
}

/* dump accumulated "context" diff changes */
static void
dump_context_vec(void)
{
	struct context_vec *cvp = context_vec_start;
	int lowa, upb, lowc, upd, do_output;
	int a, b, c, d;
	char ch;

	if (context_vec_start > context_vec_ptr)
		return;

	b = d = 0;		/* gcc */
	lowa = max(1, cvp->a - context);
	upb = min(len[0], context_vec_ptr->b + context);
	lowc = max(1, cvp->c - context);
	upd = min(len[1], context_vec_ptr->d + context);

	printf("***************\n*** ");
	range(lowa, upb, ",");
	printf(" ****\n");

	/*
	 * output changes to the "old" file.  The first loop suppresses
	 * output if there were no changes to the "old" file (we'll see
	 * the "old" lines as context in the "new" list).
	 */
	do_output = 0;
	for (; cvp <= context_vec_ptr; cvp++)
		if (cvp->a <= cvp->b) {
			cvp = context_vec_start;
			do_output++;
			break;
		}
	if (do_output) {
		while (cvp <= context_vec_ptr) {
			a = cvp->a;
			b = cvp->b;
			c = cvp->c;
			d = cvp->d;

			if (a <= b && c <= d)
				ch = 'c';
			else
				ch = (a <= b) ? 'd' : 'a';

			if (ch == 'a')
				fetch(ixold, lowa, b, input[0], "  ", 0);
			else {
				fetch(ixold, lowa, a - 1, input[0], "  ", 0);
				fetch(ixold, a, b, input[0],
				    ch == 'c' ? "! " : "- ", 0);
			}
			lowa = b + 1;
			cvp++;
		}
		fetch(ixold, b + 1, upb, input[0], "  ", 0);
	}
	/* output changes to the "new" file */
	printf("--- ");
	range(lowc, upd, ",");
	printf(" ----\n");

	do_output = 0;
	for (cvp = context_vec_start; cvp <= context_vec_ptr; cvp++)
		if (cvp->c <= cvp->d) {
			cvp = context_vec_start;
			do_output++;
			break;
		}
	if (do_output) {
		while (cvp <= context_vec_ptr) {
			a = cvp->a;
			b = cvp->b;
			c = cvp->c;
			d = cvp->d;

			if (a <= b && c <= d)
				ch = 'c';
			else
				ch = (a <= b) ? 'd' : 'a';

			if (ch == 'd')
				fetch(ixnew, lowc, d, input[1], "  ", 0);
			else {
				fetch(ixnew, lowc, c - 1, input[1], "  ", 0);
				fetch(ixnew, c, d, input[1],
				    ch == 'c' ? "! " : "+ ", 0);
			}
			lowc = d + 1;
			cvp++;
		}
		fetch(ixnew, d + 1, upd, input[1], "  ", 0);
	}
	context_vec_ptr = context_vec_start - 1;
}

/* dump accumulated "unified" diff changes */
static void
dump_unified_vec(void)
{
	struct context_vec *cvp = context_vec_start;
	int lowa, upb, lowc, upd;
	int a, b, c, d;
	char ch;

	if (context_vec_start > context_vec_ptr)
		return;

	b = d = 0;		/* gcc */
	lowa = max(1, cvp->a - context);
	upb = min(len[0], context_vec_ptr->b + context);
	lowc = max(1, cvp->c - context);
	upd = min(len[1], context_vec_ptr->d + context);

	printf("@@ -%d,%d +%d,%d @@\n", lowa, upb - lowa + 1,
	    lowc, upd - lowc + 1);

	/*
	 * Output changes in "unified" diff format--the old and new lines
	 * are printed together.
	 */
	for (; cvp <= context_vec_ptr; cvp++) {
		a = cvp->a;
		b = cvp->b;
		c = cvp->c;
		d = cvp->d;

		/*
		 * c: both new and old changes
		 * d: only changes in the old file
		 * a: only changes in the new file
		 */
		if (a <= b && c <= d)
			ch = 'c';
		else
			ch = (a <= b) ? 'd' : 'a';

		switch (ch) {
		case 'c':
			fetch(ixold, lowa, a - 1, input[0], " ", 0);
			fetch(ixold, a, b, input[0], "-", 0);
			fetch(ixnew, c, d, input[1], "+", 0);
			break;
		case 'd':
			fetch(ixold, lowa, a - 1, input[0], " ", 0);
			fetch(ixold, a, b, input[0], "-", 0);
			break;
		case 'a':
			fetch(ixnew, lowc, c - 1, input[1], " ", 0);
			fetch(ixnew, c, d, input[1], "+", 0);
			break;
		}
		lowa = b + 1;
		lowc = d + 1;
	}
	fetch(ixnew, d + 1, upd, input[1], " ", 0);

	context_vec_ptr = context_vec_start - 1;
}
