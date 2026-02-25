/*	$OpenBSD: echo.c,v 1.12 2026/02/25 21:57:43 jcs Exp $	*/
/*	$NetBSD: echo.c,v 1.6 1995/03/21 09:04:27 cgd Exp $	*/

/*
 * Copyright (c) 1989, 1993
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

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

int escape(const char *);

int
main(int argc, char *argv[])
{
	int nflag = 0, eflag = 0;
	const char *p;

	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	/* This utility may NOT do getopt(3) option parsing. */
	for (++argv; argv[0] && *argv[0] == '-'; argv++) {
		for (p = *argv + 1; *p != '\0'; p++) {
			switch (*p) {
			case 'E':
				eflag = 0;
				break;
			case 'e':
				eflag = 1;
				break;
			case 'n':
				nflag = 1;
				break;
			default:
				eflag = nflag = 0;
				goto echoargs;
			}
		}
	}

echoargs:
	while (*argv) {
		if (eflag) {
			if (escape(*argv) != 0)
				/* \c encountered */
				return 0;
		} else
			(void)fputs(*argv, stdout);
		if (*++argv)
			putchar(' ');
	}
	if (!nflag)
		putchar('\n');

	return 0;
}

/* return -1 on \c to suppress further output */
int
escape(const char *s)
{
	int ch, n;

	while ((ch = *s++) != '\0') {
		if (ch != '\\') {
			putchar(ch);
			continue;
		}

		switch ((ch = *s++)) {
		case '\0':
			putchar('\\');
			return 0;
		case '\\':
			putchar('\\');
			break;
		case 'a':
			putchar('\a');
			break;
		case 'b':
			putchar('\b');
			break;
		case 'c':
			return -1;
		case 'e':
			putchar('\033');
			break;
		case 'f':
			putchar('\f');
			break;
		case 'n':
			putchar('\n');
			break;
		case 'r':
			putchar('\r');
			break;
		case 't':
			putchar('\t');
			break;
		case 'v':
			putchar('\v');
			break;
		case '0':
			/* octal: \0nnn */
			ch = 0;
			for (n = 0; n < 3 && *s >= '0' && *s <= '7'; n++)
				ch = ch * 8 + (*s++ - '0');
			putchar(ch);
			break;
		case 'x':
			/* hexadecimal: \xhh */
			if (isxdigit((unsigned char)*s)) {
				ch = 0;
				for (n = 0;
				    n < 2 && isxdigit(*s); n++) {
					ch *= 16;
					if (*s >= '0' && *s <= '9')
						ch += *s - '0';
					else if (*s >= 'a' && *s <= 'f')
						ch += *s - 'a' + 10;
					else
						ch += *s - 'A' + 10;
					s++;
				}
				putchar(ch);
			} else {
				putchar('\\');
				putchar('x');
			}
			break;
		default:
			putchar('\\');
			putchar(ch);
			break;
		}
	}

	return 0;
}
