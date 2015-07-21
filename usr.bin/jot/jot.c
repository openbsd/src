/*	$OpenBSD: jot.c,v 1.24 2015/07/21 04:04:06 jasper Exp $	*/
/*	$NetBSD: jot.c,v 1.3 1994/12/02 20:29:43 pk Exp $	*/

/*-
 * Copyright (c) 1993
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
 */

/*
 * jot - print sequential or random data
 *
 * Author:  John Kunze, Office of Comp. Affairs, UCB
 */

#include <err.h>
#include <stdbool.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define	REPS_DEF	100
#define	BEGIN_DEF	1
#define	ENDER_DEF	100
#define	STEP_DEF	1

#define	is_default(s)	(strcmp((s), "-") == 0)

static double	begin;
static double	ender;
static double	s;
static long	reps;
static bool	randomize;
static bool	infinity;
static bool	boring;
static int	prec = -1;
static bool	intdata;
static bool	longdata;
static bool	chardata;
static bool	nosign;
static bool	finalnl = true;
static char	sepstring[BUFSIZ] = "\n";
static char	format[BUFSIZ];

static void	getformat(void);
static int	getprec(char *);
static int	putdata(double, bool);
static void	usage(void);

int
main(int argc, char *argv[])
{
	double		x;
	double		y;
	long		i;
	unsigned int	mask = 0;
	int		n = 0;
	int		ch;
	const	char	*errstr;

	while ((ch = getopt(argc, argv, "rb:w:cs:np:")) != -1)
		switch (ch) {
		case 'r':
			randomize = true;
			break;
		case 'c':
			chardata = true;
			break;
		case 'n':
			finalnl = false;
			break;
		case 'b':
			boring = true;
			if (strlcpy(format, optarg, sizeof(format)) >=
			    sizeof(format))
				errx(1, "-b word too long");
			break;
		case 'w':
			if (strlcpy(format, optarg, sizeof(format)) >=
			    sizeof(format))
				errx(1, "-w word too long");
			break;
		case 's':
			if (strlcpy(sepstring, optarg, sizeof(sepstring)) >=
			    sizeof(sepstring))
				errx(1, "-s string too long");
			break;
		case 'p':
			prec = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "bad precision value, %s: %s", errstr,
					optarg);
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	switch (argc) {	/* examine args right to left, falling thru cases */
	case 4:
		if (!is_default(argv[3])) {
			if (!sscanf(argv[3], "%lf", &s))
				errx(1, "Bad s value:  %s", argv[3]);
			mask |= 01;
			if (randomize)
				warnx("random seeding not supported");
		}
	case 3:
		if (!is_default(argv[2])) {
			if (!sscanf(argv[2], "%lf", &ender))
				ender = argv[2][strlen(argv[2])-1];
			mask |= 02;
			if (prec == -1)
				n = getprec(argv[2]);
		}
	case 2:
		if (!is_default(argv[1])) {
			if (!sscanf(argv[1], "%lf", &begin))
				begin = argv[1][strlen(argv[1])-1];
			mask |= 04;
			if (prec == -1)
				prec = getprec(argv[1]);
			if (n > prec)		/* maximum precision */
				prec = n;
		}
	case 1:
		if (!is_default(argv[0])) {
			if (!sscanf(argv[0], "%ld", &reps))
				errx(1, "Bad reps value:  %s", argv[0]);
			mask |= 010;
			if (prec == -1)
				prec = 0;
		}
		break;
	case 0:
		usage();
		break;
	default:
		errx(1, "Too many arguments.  What do you mean by %s?",
		    argv[4]);
	}
	getformat();
	while (mask)	/* 4 bit mask has 1's where last 4 args were given */
		switch (mask) {	/* fill in the 0's by default or computation */
		case 001:
			reps = REPS_DEF;
			mask = 011;
			break;
		case 002:
			reps = REPS_DEF;
			mask = 012;
			break;
		case 003:
			reps = REPS_DEF;
			mask = 013;
			break;
		case 004:
			reps = REPS_DEF;
			mask = 014;
			break;
		case 005:
			reps = REPS_DEF;
			mask = 015;
			break;
		case 006:
			reps = REPS_DEF;
			mask = 016;
			break;
		case 007:
			if (randomize) {
				reps = REPS_DEF;
				mask = 0;
				break;
			}
			if (s == 0.0) {
				reps = 0;
				mask = 0;
				break;
			}
			reps = (ender - begin + s) / s;
			if (reps <= 0)
				errx(1, "Impossible stepsize");
			mask = 0;
			break;
		case 010:
			begin = BEGIN_DEF;
			mask = 014;
			break;
		case 011:
			begin = BEGIN_DEF;
			mask = 015;
			break;
		case 012:
			s = STEP_DEF;
			mask = 013;
			break;
		case 013:
			if (randomize)
				begin = BEGIN_DEF;
			else if (reps == 0)
				errx(1, "Must specify begin if reps == 0");
			begin = ender - reps * s + s;
			mask = 0;
			break;
		case 014:
			s = STEP_DEF;
			mask = 015;
			break;
		case 015:
			if (randomize)
				ender = ENDER_DEF;
			else
				ender = begin + reps * s - s;
			mask = 0;
			break;
		case 016:
			if (reps == 0)
				errx(1, "Infinite sequences cannot be bounded");
			else if (reps == 1)
				s = 0.0;
			else
				s = (ender - begin) / (reps - 1);
			mask = 0;
			break;
		case 017:		/* if reps given and implied, */
			if (!randomize && s != 0.0) {
				long t = (ender - begin + s) / s;
				if (t <= 0)
					errx(1, "Impossible stepsize");
				if (t < reps)		/* take lesser */
					reps = t;
			}
			mask = 0;
			break;
		default:
			errx(1, "bad mask");
		}
	if (reps == 0)
		infinity = true;
	if (randomize) {
		x = (ender - begin) * (ender > begin ? 1 : -1);
		for (i = 1; i <= reps || infinity; i++) {
			double v;
			y = arc4random() / ((double)0xffffffff + 1);
			v = y * x + begin;
			if (putdata(v, reps == i && !infinity))
				errx(1, "range error in conversion: %f", v);
		}
	}
	else
		for (i = 1, x = begin; i <= reps || infinity; i++, x += s)
			if (putdata(x, reps == i && !infinity))
				errx(1, "range error in conversion: %f", x);
	if (finalnl)
		putchar('\n');
	exit(0);
}

static int
putdata(double x, bool last)
{
	if (boring)
		printf("%s", format);
	else if (longdata && nosign) {
		if (x <= (double)ULONG_MAX && x >= 0.0)
			printf(format, (unsigned long)x);
		else
			return (1);
	} else if (longdata) {
		if (x <= (double)LONG_MAX && x >= (double)LONG_MIN)
			printf(format, (long)x);
		else
			return (1);
	} else if (chardata || (intdata && !nosign)) {
		if (x <= (double)INT_MAX && x >= (double)INT_MIN)
			printf(format, (int)x);
		else
			return (1);
	} else if (intdata) {
		if (x <= (double)UINT_MAX && x >= 0.0)
			printf(format, (unsigned int)x);
		else
			return (1);
	} else
		printf(format, x);
	if (!last)
		fputs(sepstring, stdout);

	return (0);
}

static void
usage(void)
{
	(void)fprintf(stderr, "usage: jot [-cnr] [-b word] [-p precision] "
	    "[-s string] [-w word]\n"
	    "	   [reps [begin [end [s]]]]\n");
	exit(1);
}

static int
getprec(char *s)
{
	char	*p;
	char	*q;

	for (p = s; *p != '\0'; p++)
		if (*p == '.')
			break;
	if (*p == '\0')
		return (0);
	for (q = ++p; *p != '\0'; p++)
		if (!isdigit((unsigned char)*p))
			break;
	return (p - q);
}

static void
getformat(void)
{
	char	*p, *p2;
	int dot, hash, space, sign, numbers = 0;
	size_t sz;

	if (boring)				/* no need to bother */
		return;
	for (p = format; *p != '\0'; p++)	/* look for '%' */
		if (*p == '%' && *(p+1) != '%')	/* leave %% alone */
			break;
	sz = sizeof(format) - strlen(format) - 1;
	if (*p == '\0' && !chardata) {
		int n;

		n = snprintf(p, sz, "%%.%df", prec);
		if (n == -1 || n >= (int)sz)
			errx(1, "-w word too long");
	} else if (*p == '\0' && chardata) {
		if (strlcpy(p, "%c", sz) >= sz)
			errx(1, "-w word too long");
		intdata = true;
	} else if (*(p+1) == '\0') {
		if (sz <= 0)
			errx(1, "-w word too long");
		/* cannot end in single '%' */
		strlcat(format, "%", sizeof format);
	} else {
		/*
		 * Allow conversion format specifiers of the form
		 * %[#][ ][{+,-}][0-9]*[.[0-9]*]? where ? must be one of
		 * [l]{d,i,o,u,x} or {f,e,g,E,G,d,o,x,D,O,U,X,c,u}
		 */
		p2 = p++;
		dot = hash = space = sign = numbers = 0;
		while (!isalpha((unsigned char)*p)) {
			if (isdigit((unsigned char)*p)) {
				numbers++;
				p++;
			} else if ((*p == '#' && !(numbers|dot|sign|space|
			    hash++)) ||
			    (*p == ' ' && !(numbers|dot|space++)) ||
			    ((*p == '+' || *p == '-') && !(numbers|dot|sign++))
			    || (*p == '.' && !(dot++)))
				p++;
			else
				goto fmt_broken;
		}
		if (*p == 'l') {
			longdata = true;
			if (*++p == 'l') {
				if (p[1] != '\0')
					p++;
				goto fmt_broken;
			}
		}
		switch (*p) {
		case 'o': case 'u': case 'x': case 'X':
			intdata = nosign = true;
			break;
		case 'd': case 'i':
			intdata = true;
			break;
		case 'D':
			if (!longdata) {
				intdata = true;
				break;
			}
		case 'O': case 'U':
			if (!longdata) {
				intdata = nosign = true;
				break;
			}
		case 'c':
			if (!(intdata | longdata)) {
				chardata = true;
				break;
			}
		case 'h': case 'n': case 'p': case 'q': case 's': case 'L':
		case '$': case '*':
			goto fmt_broken;
		case 'f': case 'e': case 'g': case 'E': case 'G':
			if (!longdata)
				break;
			/* FALLTHROUGH */
		default:
fmt_broken:
			*++p = '\0';
			errx(1, "illegal or unsupported format '%s'", p2);
			/* NOTREACHED */
		}
		while (*++p != '\0')
			if (*p == '%' && *(p+1) != '\0' && *(p+1) != '%')
				errx(1, "too many conversions");
			else if (*p == '%' && *(p+1) == '%')
				p++;
			else if (*p == '%' && *(p+1) == '\0') {
				strlcat(format, "%", sizeof format);
				break;
			}
	}
}
