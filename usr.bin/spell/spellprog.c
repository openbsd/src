/*	$OpenBSD: spellprog.c,v 1.6 2009/10/27 23:59:43 deraadt Exp $	*/

/*
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
 *	@(#)spell.h	8.1 (Berkeley) 6/6/93
 */
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

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DLEV 2

int	 an(char *, char *, char *, int);
int	 bility(char *, char *, char *, int);
int	 es(char *, char *, char *, int);
int	 dict(char *, char *);
int	 i_to_y(char *, char *, char *, int);
int	 ily(char *, char *, char *, int);
int	 ize(char *, char *, char *, int);
int	 metry(char *, char *, char *, int);
int	 monosyl(char *, char *);
int	 ncy(char *, char *, char *, int);
int	 nop(void);
int	 trypref(char *, char *, int);
int	 tryword(char *, char *, int);
int	 s(char *, char *, char *, int);
int	 strip(char *, char *, char *, int);
int	 suffix(char *, int);
int	 tion(char *, char *, char *, int);
int	 vowel(int);
int	 y_to_e(char *, char *, char *, int);
int	 CCe(char *, char *, char *, int);
int	 VCe(char *, char *, char *, int);
char	*lookuppref(char **, char *);
char	*skipv(char *);
char	*estrdup(const char *);
void	 ise(void);
void	 print_word(FILE *);
void	 ztos(char *);
__dead void usage(void);

/* from look.c */
int	 look(unsigned char *, unsigned char *, unsigned char *);

struct suftab {
	char *suf;
	int (*p1)();	/* XXX - variable args */
	int n1;
	char *d1;
	char *a1;
	int (*p2)();	/* XXX - variable args */
	int n2;
	char *d2;
	char *a2;
} suftab[] = {
	{"ssen", ily, 4, "-y+iness", "+ness" },
	{"ssel", ily, 4, "-y+i+less", "+less" },
	{"se", s, 1, "", "+s", es, 2, "-y+ies", "+es" },
	{"s'", s, 2, "", "+'s"},
	{"s", s, 1, "", "+s"},
	{"ecn", ncy, 1, "", "-t+ce"},
	{"ycn", ncy, 1, "", "-cy+t"},
	{"ytilb", nop, 0, "", ""},
	{"ytilib", bility, 5, "-le+ility", ""},
	{"elbaif", i_to_y, 4, "-y+iable", ""},
	{"elba", CCe, 4, "-e+able", "+able"},
	{"yti", CCe, 3, "-e+ity", "+ity"},
	{"ylb", y_to_e, 1, "-e+y", ""},
	{"yl", ily, 2, "-y+ily", "+ly"},
	{"laci", strip, 2, "", "+al"},
	{"latnem", strip, 2, "", "+al"},
	{"lanoi", strip, 2, "", "+al"},
	{"tnem", strip, 4, "", "+ment"},
	{"gni", CCe, 3, "-e+ing", "+ing"},
	{"reta", nop, 0, "", ""},
	{"re", strip, 1, "", "+r", i_to_y, 2, "-y+ier", "+er"},
	{"de", strip, 1, "", "+d", i_to_y, 2, "-y+ied", "+ed"},
	{"citsi", strip, 2, "", "+ic"},
	{"cihparg", i_to_y, 1, "-y+ic", ""},
	{"tse", strip, 2, "", "+st", i_to_y, 3, "-y+iest", "+est"},
	{"cirtem", i_to_y, 1, "-y+ic", ""},
	{"yrtem", metry, 0, "-ry+er", ""},
	{"cigol", i_to_y, 1, "-y+ic", ""},
	{"tsigol", i_to_y, 2, "-y+ist", ""},
	{"tsi", VCe, 3, "-e+ist", "+ist"},
	{"msi", VCe, 3, "-e+ism", "+ist"},
	{"noitacif", i_to_y, 6, "-y+ication", ""},
	{"noitazi", ize, 5, "-e+ation", ""},
	{"rota", tion, 2, "-e+or", ""},
	{"noit", tion, 3, "-e+ion", "+ion"},
	{"naino", an, 3, "", "+ian"},
	{"na", an, 1, "", "+n"},
	{"evit", tion, 3, "-e+ive", "+ive"},
	{"ezi", CCe, 3, "-e+ize", "+ize"},
	{"pihs", strip, 4, "", "+ship"},
	{"dooh", ily, 4, "-y+hood", "+hood"},
	{"ekil", strip, 4, "", "+like"},
	{ NULL }
};

char *preftab[] = {
	"anti",
	"bio",
	"dis",
	"electro",
	"en",
	"fore",
	"hyper",
	"intra",
	"inter",
	"iso",
	"kilo",
	"magneto",
	"meta",
	"micro",
	"milli",
	"mis",
	"mono",
	"multi",
	"non",
	"out",
	"over",
	"photo",
	"poly",
	"pre",
	"pseudo",
	"re",
	"semi",
	"stereo",
	"sub",
	"super",
	"thermo",
	"ultra",
	"under",	/* must precede un */
	"un",
	NULL
};

struct wlist {
	int fd;
	unsigned char *front;
	unsigned char *back;
} *wlists;

int vflag;
int xflag;
char word[LINE_MAX];
char original[LINE_MAX];
char *deriv[40];
char affix[40];

/*
 * The spellprog utility accepts a newline-delimited list of words
 * on stdin.  For arguments it expects the path to a word list and
 * the path to a file in which to store found words.
 *
 * In normal usage, spell is called twice.  The first time it is
 * called with a stop list to flag commonly mispelled words.  The
 * remaining words are then passed to spell again, this time with
 * the dictionary file as the first (non-flag) argument.
 *
 * Unlike historic versions of spellprog, this one does not use
 * hashed files.  Instead it simply requires that files be sorted
 * lexigraphically and uses the same algorithm as the look utility.
 *
 * Note that spellprog should be called via the spell shell script
 * and is not meant to be invoked directly by the user.
 */

int
main(int argc, char **argv)
{
	char *ep, *cp, *dp;
	char *outfile;
	int ch, fold, i;
	struct stat sb;
	FILE *file, *found;

	setlocale(LC_ALL, "");

	outfile = NULL;
	while ((ch = getopt(argc, argv, "bvxo:")) != -1) {
		switch (ch) {
		case 'b':
			/* Use British dictionary and convert ize -> ise. */
			ise();
			break;
		case 'o':
			outfile = optarg;
			break;
		case 'v':
			/* Also write derivations to "found" file. */
			vflag++;
			break;
		case 'x':
			/* Print plausible stems to stdout. */
			xflag++;
			break;
		default:
			usage();
		}

	}
	argc -= optind;
	argv += optind;
	if (argc < 1)
		usage();

	/* Open and mmap the word/stop lists. */
	if ((wlists = calloc(sizeof(struct wlist), (argc + 1))) == NULL)
		err(1, "malloc");
	for (i = 0; argc--; i++) {
		wlists[i].fd = open(argv[i], O_RDONLY, 0);
		if (wlists[i].fd == -1 || fstat(wlists[i].fd, &sb) != 0)
			err(1, "%s", argv[i]);
		if (sb.st_size > SIZE_T_MAX)
			errx(1, "%s: %s", argv[i], strerror(EFBIG));
		wlists[i].front = mmap(NULL, (size_t)sb.st_size, PROT_READ,
		    MAP_PRIVATE, wlists[i].fd, (off_t)0);
		if (wlists[i].front == MAP_FAILED)
			err(1, "%s", argv[i]);
		wlists[i].back = wlists[i].front + sb.st_size;
	}
	wlists[i].fd = -1;

	/* Open file where found words are to be saved. */
	if (outfile == NULL)
		found = NULL;
	else if ((found = fopen(outfile, "w")) == NULL)
		err(1, "cannot open %s", outfile);

	for (;; print_word(file)) {
		affix[0] = '\0';
		file = found;
		for (ep = word; (*ep = ch = getchar()) != '\n'; ep++) {
			if (ep - word == sizeof(word) - 1) {
				*ep = '\0';
				warnx("word too long (%s)", word);
				while ((ch = getchar()) != '\n')
					;	/* slurp until EOL */
			}
			if (ch == EOF) {
				if (found != NULL)
					fclose(found);
				exit(0);
			}
		}
		for (cp = word, dp = original; cp < ep; )
			*dp++ = *cp++;
		*dp = '\0';
		fold = 0;
		for (cp = word; cp < ep; cp++)
			if (islower(*cp))
				goto lcase;
		if (trypref(ep, ".", 0))
			continue;
		++fold;
		for (cp = original + 1, dp = word + 1; dp < ep; dp++, cp++)
			*dp = tolower(*cp);
lcase:
		if (trypref(ep, ".", 0) || suffix(ep, 0))
			continue;
		if (isupper(word[0])) {
			for (cp = original, dp = word; (*dp = *cp++); dp++) {
				if (fold)
					*dp = tolower(*dp);
			}
			word[0] = tolower(word[0]);
			goto lcase;
		}
		file = stdout;
	}

	exit(0);
}

void
print_word(FILE *f)
{

	if (f != NULL) {
		if (vflag && affix[0] != '\0' && affix[0] != '.')
			fprintf(f, "%s\t%s\n", affix, original);
		else
			fprintf(f, "%s\n", original);
	}
}

/*
 * For each matching suffix in suftab, call the function associated
 * with that suffix (p1 and p2).
 */
int
suffix(char *ep, int lev)
{
	struct suftab *t;
	char *cp, *sp;

	lev += DLEV;
	deriv[lev] = deriv[lev-1] = 0;
	for (t = suftab; (sp = t->suf); t++) {
		cp = ep;
		while (*sp) {
			if (*--cp != *sp++)
				goto next;
		}
		for (sp = cp; --sp >= word && !vowel(*sp);)
			;	/* nothing */
		if (sp < word)
			return (0);
		if ((*t->p1)(ep-t->n1, t->d1, t->a1, lev+1))
			return (1);
		if (t->p2 != NULL) {
			deriv[lev] = deriv[lev+1] = '\0';
			return ((*t->p2)(ep-t->n2, t->d2, t->a2, lev));
		}
		return (0);
next:		;
	}
	return (0);
}

int
nop(void)
{

	return (0);
}

int
strip(char *ep, char *d, char *a, int lev)
{

	return (trypref(ep, a, lev) || suffix(ep, lev));
}

int
s(char *ep, char *d, char *a, int lev)
{

	if (lev > DLEV + 1)
		return (0);
	if (*ep == 's' && ep[-1] == 's')
		return (0);
	return (strip(ep, d, a, lev));
}

int
an(char *ep, char *d, char *a, int lev)
{

	if (!isupper(*word))	/* must be proper name */
		return (0);
	return (trypref(ep,a,lev));
}

int
ize(char *ep, char *d, char *a, int lev)
{

	*ep++ = 'e';
	return (strip(ep ,"", d, lev));
}

int
y_to_e(char *ep, char *d, char *a, int lev)
{
	char c = *ep;

	*ep++ = 'e';
	if (strip(ep, "", d, lev))
		return (1);
	ep[-1] = c;
	return (0);
}

int
ily(char *ep, char *d, char *a, int lev)
{

	if (ep[-1] == 'i')
		return (i_to_y(ep, d, a, lev));
	else
		return (strip(ep, d, a, lev));
}

int
ncy(char *ep, char *d, char *a, int lev)
{

	if (skipv(skipv(ep-1)) < word)
		return (0);
	ep[-1] = 't';
	return (strip(ep, d, a, lev));
}

int
bility(char *ep, char *d, char *a, int lev)
{

	*ep++ = 'l';
	return (y_to_e(ep, d, a, lev));
}

int
i_to_y(char *ep, char *d, char *a, int lev)
{

	if (ep[-1] == 'i') {
		ep[-1] = 'y';
		a = d;
	}
	return (strip(ep, "", a, lev));
}

int
es(char *ep, char *d, char *a, int lev)
{

	if (lev > DLEV)
		return (0);

	switch (ep[-1]) {
	default:
		return (0);
	case 'i':
		return (i_to_y(ep, d, a, lev));
	case 's':
	case 'h':
	case 'z':
	case 'x':
		return (strip(ep, d, a, lev));
	}
}

int
metry(char *ep, char *d, char *a, int lev)
{

	ep[-2] = 'e';
	ep[-1] = 'r';
	return (strip(ep, d, a, lev));
}

int
tion(char *ep, char *d, char *a, int lev)
{

	switch (ep[-2]) {
	case 'c':
	case 'r':
		return (trypref(ep, a, lev));
	case 'a':
		return (y_to_e(ep, d, a, lev));
	}
	return (0);
}

/*
 * Possible consonant-consonant-e ending.
 */
int
CCe(char *ep, char *d, char *a, int lev)
{

	switch (ep[-1]) {
	case 'l':
		if (vowel(ep[-2]))
			break;
		switch (ep[-2]) {
		case 'l':
		case 'r':
		case 'w':
			break;
		default:
			return (y_to_e(ep, d, a, lev));
		}
		break;
	case 's':
		if (ep[-2] == 's')
			break;
	case 'c':
	case 'g':
		if (*ep == 'a')
			return (0);
	case 'v':
	case 'z':
		if (vowel(ep[-2]))
			break;
	case 'u':
		if (y_to_e(ep, d, a, lev))
			return (1);
		if (!(ep[-2] == 'n' && ep[-1] == 'g'))
			return (0);
	}
	return (VCe(ep, d, a, lev));
}

/*
 * Possible consonant-vowel-consonant-e ending.
 */
int
VCe(char *ep, char *d, char *a, int lev)
{
	char c;

	c = ep[-1];
	if (c == 'e')
		return (0);
	if (!vowel(c) && vowel(ep[-2])) {
		c = *ep;
		*ep++ = 'e';
		if (trypref(ep, d, lev) || suffix(ep, lev))
			return (1);
		ep--;
		*ep = c;
	}
	return (strip(ep, d, a, lev));
}

char *
lookuppref(char **wp, char *ep)
{
	char **sp;
	char *bp,*cp;

	for (sp = preftab; *sp; sp++) {
		bp = *wp;
		for (cp = *sp; *cp; cp++, bp++) {
			if (tolower(*bp) != *cp)
				goto next;
		}
		for (cp = bp; cp < ep; cp++) {
			if (vowel(*cp)) {
				*wp = bp;
				return (*sp);
			}
		}
next:		;
	}
	return (0);
}

/*
 * If the word is not in the dictionary, try stripping off prefixes
 * until the word is found or we run out of prefixes to check.
 */
int
trypref(char *ep, char *a, int lev)
{
	char *cp;
	char *bp;
	char *pp;
	int val = 0;
	char space[20];

	deriv[lev] = a;
	if (tryword(word, ep, lev))
		return (1);
	bp = word;
	pp = space;
	deriv[lev+1] = pp;
	while ((cp = lookuppref(&bp, ep))) {
		*pp++ = '+';
		while ((*pp = *cp++))
			pp++;
		if (tryword(bp, ep, lev+1)) {
			val = 1;
			break;
		}
		if (pp - space >= sizeof(space))
			return (0);
	}
	deriv[lev+1] = deriv[lev+2] = '\0';
	return (val);
}

int
tryword(char *bp, char *ep, int lev)
{
	int i, j;
	char duple[3];

	if (ep-bp <= 1)
		return (0);
	if (vowel(*ep) && monosyl(bp, ep))
		return (0);

	i = dict(bp, ep);
	if (i == 0 && vowel(*ep) && ep[-1] == ep[-2] && monosyl(bp, ep-1)) {
		ep--;
		deriv[++lev] = duple;
		duple[0] = '+';
		duple[1] = *ep;
		duple[2] = '\0';
		i = dict(bp, ep);
	}
	if (vflag == 0 || i == 0)
		return (i);

	/* Also tack on possible derivations. (XXX - warn on truncation?) */
	for (j = lev; j > 0; j--) {
		if (deriv[j])
			strlcat(affix, deriv[j], sizeof(affix));
	}
	return (i);
}

int
monosyl(char *bp, char *ep)
{

	if (ep < bp + 2)
		return (0);
	if (vowel(*--ep) || !vowel(*--ep) || ep[1] == 'x' || ep[1] == 'w')
		return (0);
	while (--ep >= bp)
		if (vowel(*ep))
			return (0);
	return (1);
}

char *
skipv(char *s)
{

	if (s >= word && vowel(*s))
		s--;
	while (s >= word && !vowel(*s))
		s--;
	return (s);
}

int
vowel(int c)
{

	switch (tolower(c)) {
	case 'a':
	case 'e':
	case 'i':
	case 'o':
	case 'u':
	case 'y':
		return (1);
	}
	return (0);
}

/*
 * Crummy way to Britishise.
 */
void
ise(void)
{
	struct suftab *tab;

	for (tab = suftab; tab->suf; tab++) {
		/* Assume that suffix will contain 'z' if a1 or d1 do */
		if (strchr(tab->suf, 'z')) {
			tab->suf = estrdup(tab->suf);
			ztos(tab->suf);
			if (strchr(tab->d1, 'z')) {
				tab->d1 = estrdup(tab->d1);
				ztos(tab->d1);
			}
			if (strchr(tab->a1, 'z')) {
				tab->a1 = estrdup(tab->a1);
				ztos(tab->a1);
			}
		}
	}
}

void
ztos(char *s)
{

	for (; *s; s++)
		if (*s == 'z')
			*s = 's';
}

char *
estrdup(const char *s)
{
	char *d;

	if ((d = strdup(s)) == NULL)
		err(1, "strdup");
	return (d);
}

/*
 * Look up a word in the dictionary.
 * Returns 1 if found, 0 if not.
 */
int
dict(char *bp, char *ep)
{
	char c;
	int i, rval;

	c = *ep;
	*ep = '\0';
	if (xflag)
		printf("=%s\n", bp);
	for (i = rval = 0; wlists[i].fd != -1; i++) {
		if ((rval = look((unsigned char *)bp, wlists[i].front,
		    wlists[i].back)) == 1)
			break;
	}
	*ep = c;
	return (rval);
}

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-bvx] [-o found-words] word-list ...\n",
	    __progname);
	exit(1);
}
