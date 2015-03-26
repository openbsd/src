/*	$OpenBSD: misc.c,v 1.46 2015/03/26 14:08:12 krw Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/disklabel.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>

#include "disk.h"
#include "misc.h"
#include "part.h"

struct unit_type unit_types[] = {
	{"b", 1			, "Bytes"},
	{" ", 0			, "Sectors"},	/* Filled in from disklabel. */
	{"K", 1024		, "Kilobytes"},
	{"M", 1024 * 1024	, "Megabytes"},
	{"G", 1024 * 1024 *1024	, "Gigabytes"},
	{NULL, 0		, NULL },
};

int
unit_lookup(char *units)
{
	int i = 0;

	if (units == NULL)
		return (SECTORS);

	while (unit_types[i].abbr != NULL) {
		if (strncasecmp(unit_types[i].abbr, units, 1) == 0)
			break;
		i++;
	}
	/* default */
	if (unit_types[i].abbr == NULL)
		return (SECTORS);

	return (i);
}

int
ask_cmd(char **cmd, char **args)
{
	static char lbuf[100];
	char *cp, *buf;
	size_t lbuflen;

	/* Get input */
	if (fgets(lbuf, sizeof lbuf, stdin) == NULL)
		errx(1, "eof");
	lbuflen = strlen(lbuf);
	if (lbuflen > 0 && lbuf[lbuflen - 1] == '\n')
		lbuf[lbuflen - 1] = '\0';

	/* Parse input */
	buf = lbuf;
	buf = &buf[strspn(buf, " \t")];
	cp = &buf[strcspn(buf, " \t")];
	*cp++ = '\0';
	*cmd = buf;
	*args = &cp[strspn(cp, " \t")];

	return (0);
}

int
ask_num(const char *str, int dflt, int low, int high)
{
	char lbuf[100];
	const char *errstr;
	size_t lbuflen;
	int num;

	if (dflt < low)
		dflt = low;
	else if (dflt > high)
		dflt = high;

	do {
		printf("%s [%d - %d]: [%d] ", str, low, high, dflt);

		if (fgets(lbuf, sizeof lbuf, stdin) == NULL)
			errx(1, "eof");

		lbuflen = strlen(lbuf);
		if (lbuflen > 0 && lbuf[lbuflen - 1] == '\n')
			lbuf[lbuflen - 1] = '\0';

		if (lbuf[0] == '\0') {
			num = dflt;
			errstr = NULL;
		} else {
			num = (int)strtonum(lbuf, low, high, &errstr);
			if (errstr)
				printf("%s is %s: %s.\n", str, errstr, lbuf);
		}
	} while (errstr);

	return (num);
}

int
ask_pid(int dflt)
{
	char lbuf[100], *cp;
	size_t lbuflen;
	int num = -1;
	const int low = 0, high = 0xff;

	if (dflt < low)
		dflt = low;
	else if (dflt > high)
		dflt = high;

	do {
		printf("Partition id ('0' to disable) [%X - %X]: [%X] ", low,
		    high, dflt);
		printf("(? for help) ");

		if (fgets(lbuf, sizeof lbuf, stdin) == NULL)
			errx(1, "eof");
		lbuflen = strlen(lbuf);
		if (lbuflen > 0 && lbuf[lbuflen - 1] == '\n')
			lbuf[lbuflen - 1] = '\0';

		if (lbuf[0] == '?') {
			PRT_printall();
			continue;
		}

		/* Convert */
		cp = lbuf;
		num = strtol(lbuf, &cp, 16);

		/* Make sure only number present */
		if (cp == lbuf)
			num = dflt;
		if (*cp != '\0') {
			printf("'%s' is not a valid number.\n", lbuf);
			num = low - 1;
		} else if (num < low || num > high) {
			printf("'%x' is out of range.\n", num);
		}
	} while (num < low || num > high);

	return (num);
}

int
ask_yn(const char *str)
{
	int ch, first;
	extern int y_flag;

	if (y_flag)
		return (1);

	printf("%s [n] ", str);
	fflush(stdout);

	first = ch = getchar();
	while (ch != '\n' && ch != EOF)
		ch = getchar();

	if (ch == EOF || first == EOF)
		errx(1, "eof");

	return (first == 'y' || first == 'Y');
}

/*
 * adapted from sbin/disklabel/editor.c
 */
u_int64_t
getuint64(char *prompt, u_int64_t oval, u_int64_t maxval)
{
	char buf[BUFSIZ], *endptr, *p, operator = '\0';
	size_t n;
	int mult = 1, secsize = unit_types[SECTORS].conversion;
	double d, d2;
	int secpercyl, saveerr;
	char unit;

	if (oval > maxval)
		oval = maxval;

	secpercyl = disk.sectors * disk.heads;

	do {
		printf("%s: [%llu] ", prompt, oval);

		if (fgets(buf, sizeof(buf), stdin) == NULL)
			errx(1, "eof");

		n = strlen(buf);
		if (n > 0 && buf[n-1] == '\n')
			buf[--n] = '\0';

		if (buf[0] == '\0') {
			return (oval);
		} else if (buf[0] == '*' && buf[1] == '\0') {
			return (maxval);
		}

		/* deal with units */
		switch (tolower((unsigned char)buf[n-1])) {
		case 'c':
			unit = 'c';
			mult = secpercyl;
			buf[--n] = '\0';
			break;
		case 'b':
			unit = 'b';
			mult = -(int64_t)secsize;
			buf[--n] = '\0';
			break;
		case 's':
			unit = 's';
			mult = 1;
			buf[--n] = '\0';
			break;
		case 'k':
			unit = 'k';
			if (secsize > 1024)
				mult = -(int64_t)secsize / 1024LL;
			else
				mult = 1024LL / secsize;
			buf[--n] = '\0';
			break;
		case 'm':
			unit = 'm';
			mult = (1024LL * 1024) / secsize;
			buf[--n] = '\0';
			break;
		case 'g':
			unit = 'g';
			mult = (1024LL * 1024 * 1024) / secsize;
			buf[--n] = '\0';
			break;
		case 't':
			unit = 't';
			mult = (1024LL * 1024 * 1024 * 1024) / secsize;
			buf[--n] = '\0';
			break;
		default:
			unit = ' ';
			mult = 1;
			break;
		}

		/* deal with the operator */
		p = &buf[0];
		if (*p == '+' || *p == '-')
			operator = *p++;
		else
			operator = ' ';

		endptr = p;
		errno = 0;
		d = strtod(p, &endptr);
		saveerr = errno;
		d2 = d;
		if (mult > 0)
			d *= mult;
		else {
			d /= (-mult);
			d2 = d;
		}

		/* Apply the operator */
		if (operator == '+')
			d = oval + d;
		else if (operator == '-') {
			d = oval - d;
			d2 = d;
		}

		if (saveerr == ERANGE || d > maxval || d < 0 || d < d2) {
			printf("%s is out of range: %c%s%c\n", prompt, operator,
			    p, unit); 
		} else if (*endptr != '\0') {
			printf("%s is invalid: %c%s%c\n", prompt, operator,
			    p, unit); 
		} else {
			break;
		}
	} while (1);

	return((u_int64_t)d);
}
