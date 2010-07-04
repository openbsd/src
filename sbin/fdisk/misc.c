/*	$OpenBSD: misc.c,v 1.23 2010/07/04 22:15:31 halex Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <err.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/disklabel.h>
#include <limits.h>
#include "misc.h"

struct unit_type unit_types[] = {
	{"b", 1			, "Bytes"},
	{" ", DEV_BSIZE		, "Sectors"},
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
		return (UNIT_TYPE_DEFAULT);

	while (unit_types[i].abbr != NULL) {
		if (strncasecmp(unit_types[i].abbr, units, 1) == 0)
			break;
		i++;
	}
	/* default */
	if (unit_types[i].abbr == NULL)
		return (UNIT_TYPE_DEFAULT);

	return (i);
}

int
ask_cmd(cmd_t *cmd)
{
	char lbuf[100], *cp, *buf;
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
	strncpy(cmd->cmd, buf, sizeof(cmd->cmd));
	buf = &cp[strspn(cp, " \t")];
	strncpy(cmd->args, buf, sizeof(cmd->args));

	return (0);
}

int
ask_num(const char *str, int flags, int dflt, int low, int high,
    void (*help)(void))
{
	char lbuf[100], *cp;
	size_t lbuflen;
	int num;

	if (dflt < low)
		dflt = low;
	else if (dflt > high)
		dflt = high;

	do {
again:
		if (flags == ASK_HEX)
			printf("%s [%X - %X]: [%X] ", str, low, high, dflt);
		else
			printf("%s [%d - %d]: [%d] ", str, low, high, dflt);
		if (help)
			printf("(? for help) ");

		if (fgets(lbuf, sizeof lbuf, stdin) == NULL)
			errx(1, "eof");
		lbuflen = strlen(lbuf);
		if (lbuflen > 0 && lbuf[lbuflen - 1] == '\n')
			lbuf[lbuflen - 1] = '\0';

		if (help && lbuf[0] == '?') {
			(*help)();
			goto again;
		}

		/* Convert */
		cp = lbuf;
		num = strtol(lbuf, &cp, ((flags==ASK_HEX)?16:10));

		/* Make sure only number present */
		if (cp == lbuf)
			num = dflt;
		if (*cp != '\0') {
			printf("'%s' is not a valid number.\n", lbuf);
			num = low - 1;
		} else if (num < low || num > high) {
			printf("'%d' is out of range.\n", num);
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

u_int16_t
getshort(void *p)
{
	unsigned char *cp = p;

	return (cp[0] | (cp[1] << 8));
}

void
putshort(void *p, u_int16_t l)
{
	unsigned char *cp = p;

	*cp++ = l;
	*cp++ = l >> 8;
}

u_int32_t
getlong(void *p)
{
	unsigned char *cp = p;

	return (cp[0] | (cp[1] << 8) | (cp[2] << 16) | (cp[3] << 24));
}

void
putlong(void *p, u_int32_t l)
{
	unsigned char *cp = p;

	*cp++ = l;
	*cp++ = l >> 8;
	*cp++ = l >> 16;
	*cp++ = l >> 24;
}

/*
 * adapted from sbin/disklabel/editor.c
 * Returns UINT_MAX on error
 */
u_int32_t
getuint(disk_t *disk, char *prompt, char *helpstring, u_int32_t oval,
    u_int32_t maxval, u_int32_t offset,	int flags)
{
	char buf[BUFSIZ], *endptr, *p, operator = '\0';
	u_int32_t rval = oval;
	size_t n;
	int mult = 1, secsize = unit_types[SECTORS].conversion;
	double d;
	int secpercyl;

	secpercyl = disk->real->sectors * disk->real->heads;

	/* We only care about the remainder */
	offset = offset % secpercyl;

	buf[0] = '\0';
	do {
		printf("%s: [%u] ", prompt, oval);
		if (fgets(buf, sizeof(buf), stdin) == NULL)
			errx(1, "eof");
		n = strlen(buf);
		if (n > 0 && buf[n-1] == '\n')
			buf[--n] = '\0';
		if (buf[0] == '?')
			puts(helpstring);
	} while (buf[0] == '?');

	if (buf[0] == '*' && buf[1] == '\0') {
		rval = maxval;
	} else {
		/* deal with units */
		if (buf[0] != '\0' && n > 0) {
			if ((flags & DO_CONVERSIONS)) {
				switch (tolower(buf[n-1])) {

				case 'c':
					mult = secpercyl;
					buf[--n] = '\0';
					break;
				case 'b':
					mult = -secsize;
					buf[--n] = '\0';
					break;
				case 's':
					buf[--n] = '\0';
					break;
				case 'k':
					if (secsize > 1024)
						mult = -secsize / 1024;
					else
						mult = 1024 / secsize;
					buf[--n] = '\0';
					break;
				case 'm':
					mult = 1048576 / secsize;
					buf[--n] = '\0';
					break;
				case 'g':
					mult = 1073741824 / secsize;
					buf[--n] = '\0';
					break;
				}
			}

			/* Did they give us an operator? */
			p = &buf[0];
			if (*p == '+' || *p == '-')
				operator = *p++;

			endptr = p;
			errno = 0;
			d = strtod(p, &endptr);
			if (errno == ERANGE)
				rval = UINT_MAX;	/* too big/small */
			else if (*endptr != '\0') {
				errno = EINVAL;		/* non-numbers in str */
				rval = UINT_MAX;
			} else {
				/* XXX - should check for overflow */
				if (mult > 0)
					rval = d * mult;
				else
					/* Negative mult means divide (fancy) */
					rval = d / (-mult);

				/* Apply the operator */
				if (operator == '+')
					rval += oval;
				else if (operator == '-')
					rval = oval - rval;
			}
		}
	}
	if ((flags & DO_ROUNDING) && rval < UINT_MAX) {
#ifndef CYLCHECK
		/* Round to nearest cylinder unless given in sectors */
		if (mult != 1)
#endif
		{
			u_int32_t cyls;

			/* If we round up past the end, round down instead */
			cyls = (u_int32_t)((rval / (double)secpercyl)
			    + 0.5);
			if (cyls != 0 && secpercyl != 0) {
				if ((cyls * secpercyl) - offset > maxval)
					cyls--;

				if (rval != (cyls * secpercyl) - offset) {
					rval = (cyls * secpercyl) - offset;
					printf("Rounding to nearest cylinder: %u\n",
					    rval);
				}
			}
		}
	}

	return(rval);
}
