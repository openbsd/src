/*	$OpenBSD: misc.c,v 1.7 2002/01/18 08:33:10 kjell Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Tobias Weingartner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <machine/limits.h>
#include "misc.h"


int
ask_cmd(cmd)
	cmd_t *cmd;
{
	char lbuf[100], *cp, *buf;

	/* Get input */
	if (fgets(lbuf, sizeof lbuf, stdin) == NULL)
		errx(1, "eof");
	lbuf[strlen(lbuf)-1] = '\0';

	/* Parse input */
	buf = lbuf;
	buf = &buf[strspn(buf, " \t")];
	cp = &buf[strcspn(buf, " \t")];
	*cp++ = '\0';
	strncpy(cmd->cmd, buf, 10);
	buf = &cp[strspn(cp, " \t")];
	strncpy(cmd->args, buf, 100);

	return (0);
}

int
ask_num(str, flags, dflt, low, high, help)
	const char *str;
	int flags;
	int dflt;
	int low;
	int high;
	void (*help) __P((void));
{
	char lbuf[100], *cp;
	int num;

	do {
again:
		num = dflt;
		if (flags == ASK_HEX)
			printf("%s [%X - %X]: [%X] ", str, low, high, num);
		else
			printf("%s [%d - %d]: [%d] ", str, low, high, num);
		if (help)
			printf("(? for help) ");

		if (fgets(lbuf, sizeof lbuf, stdin) == NULL)
			errx(1, "eof");
		lbuf[strlen(lbuf)-1] = '\0';

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
ask_yn(str)
	const char *str;
{
	int ch, first;

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
getshort(p)
	void *p;
{
	unsigned char *cp = p;

	return (cp[0] | (cp[1] << 8));
}

void
putshort(p, l)
	void *p;
	u_int16_t l;
{
	unsigned char *cp = p;

	*cp++ = l;
	*cp++ = l >> 8;
}

u_int32_t
getlong(p)
	void *p;
{
	unsigned char *cp = p;

	return (cp[0] | (cp[1] << 8) | (cp[2] << 16) | (cp[3] << 24));
}

void
putlong(p, l)
	void *p;
	u_int32_t l;
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
getuint(disk, prompt, helpstring, oval, maxval, offset, flags)
	disk_t *disk;
	char *prompt;
	char *helpstring;
	u_int32_t oval;
	u_int32_t maxval;
	u_int32_t offset;
	int flags;
{
	char buf[BUFSIZ], *endptr, *p, operator = '\0';
	u_int32_t rval = oval;
	size_t n;
	int mult = 1;
	double d;
	int secpercyl;
	
	secpercyl = disk->real->sectors * disk->real->heads;

	/* We only care about the remainder */
	offset = offset % secpercyl;

	buf[0] = '\0';
	do {
		printf("%s: [%u] ", prompt, oval);
		if (fgets(buf, sizeof(buf), stdin) == NULL) {
			buf[0] = '\0';
			if (feof(stdin)) {
				clearerr(stdin);
				putchar('\n');
				return(UINT_MAX - 1);
			}
		}
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
					mult = -DEV_BSIZE;
					buf[--n] = '\0';
					break;
				case 's':
					buf[--n] = '\0';
					break;
				case 'k':
					mult = 1024 / DEV_BSIZE;
					buf[--n] = '\0';
					break;
				case 'm':
					mult = 1048576 / DEV_BSIZE;
					buf[--n] = '\0';
					break;
				case 'g':
					mult = 1073741824 / DEV_BSIZE;
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
