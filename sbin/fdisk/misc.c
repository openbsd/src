/*	$OpenBSD: misc.c,v 1.6 1997/10/19 23:30:48 deraadt Exp $	*/

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
#include <sys/disklabel.h>
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
