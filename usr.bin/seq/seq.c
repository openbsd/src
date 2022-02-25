/*	$OpenBSD: seq.c,v 1.6 2022/02/25 16:00:39 tb Exp $	*/

/*-
 * Copyright (c) 2005 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Brian Ginsbach.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VERSION	"1.0"
#define ZERO	'0'
#define SPACE	' '

#define MAXIMUM(a, b)	(((a) < (b))? (b) : (a))
#define ISSIGN(c)	((int)(c) == '-' || (int)(c) == '+')
#define ISEXP(c)	((int)(c) == 'e' || (int)(c) == 'E')
#define ISODIGIT(c)	((int)(c) >= '0' && (int)(c) <= '7')

/* Globals */

static const char *decimal_point = ".";	/* default */
static char default_format[] = { "%g" };	/* default */

static const struct option long_opts[] = {
	{"format",	required_argument,	NULL, 'f'},
	{"help",	no_argument,		NULL, 'h'},
	{"separator",	required_argument,	NULL, 's'},
	{"version",	no_argument,		NULL, 'v'},
	{"equal-width",	no_argument,		NULL, 'w'},
	{NULL,		no_argument,		NULL, 0}
};

/* Prototypes */

static double e_atof(const char *);

static int decimal_places(const char *);
static int numeric(const char *);
static int valid_format(const char *);

static char *generate_format(double, double, double, int, char);

static __dead void usage(int error);

/*
 * The seq command will print out a numeric sequence from 1, the default,
 * to a user specified upper limit by 1.  The lower bound and increment
 * maybe indicated by the user on the command line.  The sequence can
 * be either whole, the default, or decimal numbers.
 */
int
main(int argc, char *argv[])
{
	int c = 0;
	int equalize = 0;
	double first = 1.0;
	double last = 0.0;
	double incr = 0.0;
	double last_shown_value = 0.0;
	double cur, step;
	struct lconv *locale;
	char *fmt = NULL;
	const char *sep = "\n";
	const char *term = "\n";
	char *cur_print, *last_print;
	char pad = ZERO;

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	/* Determine the locale's decimal point. */
	locale = localeconv();
	if (locale && locale->decimal_point && locale->decimal_point[0] != '\0')
		decimal_point = locale->decimal_point;

	/*
	 * Process options, but handle negative numbers separately
	 * least they trip up getopt(3).
	 */
	while ((optind < argc) && !numeric(argv[optind]) &&
	    (c = getopt_long(argc, argv, "+f:s:w", long_opts, NULL)) != -1) {

		switch (c) {
		case 'f':	/* format (plan9/GNU) */
			fmt = optarg;
			equalize = 0;
			break;
		case 's':	/* separator (GNU) */
			sep = optarg;
			break;
		case 'v':	/* version (GNU) */
			printf("seq version %s\n", VERSION);
			return 0;
		case 'w':	/* equal width (plan9/GNU) */
			if (fmt == NULL) {
				if (equalize++)
					pad = SPACE;
			}
			break;
		case 'h':	/* help (GNU) */
			usage(0);
			break;
		default:
			usage(1);
			break;
		}
	}

	argc -= optind;
	argv += optind;
	if (argc < 1 || argc > 3)
		usage(1);

	last = e_atof(argv[argc - 1]);

	if (argc > 1)
		first = e_atof(argv[0]);

	if (argc > 2) {
		incr = e_atof(argv[1]);
		/* Plan 9/GNU don't do zero */
		if (incr == 0.0)
			errx(1, "zero %screment", (first < last) ? "in" : "de");
	}

	/* default is one for Plan 9/GNU work alike */
	if (incr == 0.0)
		incr = (first < last) ? 1.0 : -1.0;

	if (incr <= 0.0 && first < last)
		errx(1, "needs positive increment");

	if (incr >= 0.0 && first > last)
		errx(1, "needs negative decrement");

	if (fmt != NULL) {
		if (!valid_format(fmt))
			errx(1, "invalid format string: `%s'", fmt);
		/*
		 * XXX to be bug for bug compatible with Plan 9 add a
		 * newline if none found at the end of the format string.
		 */
	} else
		fmt = generate_format(first, incr, last, equalize, pad);

	for (step = 1, cur = first; incr > 0 ? cur <= last : cur >= last;
	    cur = first + incr * step++) {
		if (cur != first)
			fputs(sep, stdout);
		printf(fmt, cur);
		last_shown_value = cur;
	}

	/*
	 * Did we miss the last value of the range in the loop above?
	 *
	 * We might have, so check if the printable version of the last
	 * computed value ('cur') and desired 'last' value are equal.  If they
	 * are equal after formatting truncation, but 'cur' and
	 * 'last_shown_value' are not equal, it means the exit condition of the
	 * loop held true due to a rounding error and we still need to print
	 * 'last'.
	 */
	if (asprintf(&cur_print, fmt, cur) == -1 ||
	    asprintf(&last_print, fmt, last) == -1)
		err(1, "asprintf");
	if (strcmp(cur_print, last_print) == 0 && cur != last_shown_value) {
		if (cur != first)
			fputs(sep, stdout);
		fputs(last_print, stdout);
	}
	free(cur_print);
	free(last_print);

	fputs(term, stdout);

	return 0;
}

/*
 * numeric - verify that string is numeric
 */
static int
numeric(const char *s)
{
	int seen_decimal_pt, decimal_pt_len;

	/* skip any sign */
	if (ISSIGN((unsigned char)*s))
		s++;

	seen_decimal_pt = 0;
	decimal_pt_len = strlen(decimal_point);
	while (*s) {
		if (!isdigit((unsigned char)*s)) {
			if (!seen_decimal_pt &&
			    strncmp(s, decimal_point, decimal_pt_len) == 0) {
				s += decimal_pt_len;
				seen_decimal_pt = 1;
				continue;
			}
			if (ISEXP((unsigned char)*s)) {
				s++;
				if (ISSIGN((unsigned char)*s) ||
				    isdigit((unsigned char)*s)) {
					s++;
					continue;
				}
			}
			break;
		}
		s++;
	}
	return *s == '\0';
}

/*
 * valid_format - validate user specified format string
 */
static int
valid_format(const char *fmt)
{
	unsigned conversions = 0;

	while (*fmt != '\0') {
		/* scan for conversions */
		if (*fmt != '%') {
			fmt++;
			continue;
		}
		fmt++;

		/* allow %% but not things like %10% */
		if (*fmt == '%') {
			fmt++;
			continue;
		}

		/* flags */
		while (*fmt != '\0' && strchr("#0- +'", *fmt)) {
			fmt++;
		}

		/* field width */
		while (*fmt != '\0' && strchr("0123456789", *fmt)) {
			fmt++;
		}

		/* precision */
		if (*fmt == '.') {
			fmt++;
			while (*fmt != '\0' && strchr("0123456789", *fmt)) {
				fmt++;
			}
		}

		/* conversion */
		switch (*fmt) {
		case 'A':
		case 'a':
		case 'E':
		case 'e':
		case 'F':
		case 'f':
		case 'G':
		case 'g':
			/* floating point formats are accepted */
			conversions++;
			break;
		default:
			/* anything else is not */
			return 0;
		}
	}

	/* PR 236347 -- user format strings must have a conversion */
	return conversions == 1;
}

/*
 * e_atof - convert an ASCII string to a double
 *	exit if string is not a valid double, or if converted value would
 *	cause overflow or underflow
 */
static double
e_atof(const char *num)
{
	char *endp;
	double dbl;

	errno = 0;
	dbl = strtod(num, &endp);

	if (errno == ERANGE)
		/* under or overflow */
		err(2, "%s", num);
	else if (*endp != '\0')
		/* "junk" left in number */
		errx(2, "invalid floating point argument: %s", num);

	/* zero shall have no sign */
	if (dbl == -0.0)
		dbl = 0;
	return dbl;
}

/*
 * decimal_places - count decimal places in a number (string)
 */
static int
decimal_places(const char *number)
{
	int places = 0;
	char *dp;

	/* look for a decimal point */
	if ((dp = strstr(number, decimal_point))) {
		dp += strlen(decimal_point);

		while (isdigit((unsigned char)*dp++))
			places++;
	}
	return places;
}

/*
 * generate_format - create a format string
 *
 * XXX to be bug for bug compatible with Plan9 and GNU return "%g"
 * when "%g" prints as "%e" (this way no width adjustments are made)
 */
static char *
generate_format(double first, double incr, double last, int equalize, char pad)
{
	static char buf[256];
	char cc = '\0';
	int precision, width1, width2, places;

	if (equalize == 0)
		return default_format;

	/* figure out "last" value printed */
	if (first > last)
		last = first - incr * floor((first - last) / incr);
	else
		last = first + incr * floor((last - first) / incr);

	snprintf(buf, sizeof(buf), "%g", incr);
	if (strchr(buf, 'e'))
		cc = 'e';
	precision = decimal_places(buf);

	width1 = snprintf(buf, sizeof(buf), "%g", first);
	if (strchr(buf, 'e'))
		cc = 'e';
	if ((places = decimal_places(buf)))
		width1 -= (places + strlen(decimal_point));

	precision = MAXIMUM(places, precision);

	width2 = snprintf(buf, sizeof(buf), "%g", last);
	if (strchr(buf, 'e'))
		cc = 'e';
	if ((places = decimal_places(buf)))
		width2 -= (places + strlen(decimal_point));

	/* XXX if incr is floating point fix the precision */
	if (precision) {
		snprintf(buf, sizeof(buf), "%%%c%d.%d%c", pad,
		    MAXIMUM(width1, width2) + (int)strlen(decimal_point) +
		    precision, precision, (cc) ? cc : 'f');
	} else {
		snprintf(buf, sizeof(buf), "%%%c%d%c", pad,
		    MAXIMUM(width1, width2), (cc) ? cc : 'g');
	}

	return buf;
}

static __dead void
usage(int error)
{
	fprintf(stderr,
	    "usage: %s [-w] [-f format] [-s string] [first [incr]] last\n",
	    getprogname());
	exit(error);
}
