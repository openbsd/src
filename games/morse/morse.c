/*	$OpenBSD: morse.c,v 1.7 1998/12/13 07:53:03 pjanzen Exp $	*/

/*
 * Copyright (c) 1988, 1993
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)morse.c	8.1 (Berkeley) 5/31/93";
#else
static char rcsid[] = "$OpenBSD: morse.c,v 1.7 1998/12/13 07:53:03 pjanzen Exp $";
#endif
#endif /* not lint */

#include <ctype.h>
#include <stdio.h>
#include <unistd.h>

static char
	*digit[] = {
	"-----",
	".----",
	"..---",
	"...--",
	"....-",
	".....",
	"-....",
	"--...",
	"---..",
	"----.",
},
	*alph[] = {
	".-",
	"-...",
	"-.-.",
	"-..",
	".",
	"..-.",
	"--.",
	"....",
	"..",
	".---",
	"-.-",
	".-..",
	"--",
	"-.",
	"---",
	".--.",
	"--.-",
	".-.",
	"...",
	"-",
	"..-",
	"...-",
	".--",
	"-..-",
	"-.--",
	"--..",
};

struct punc {
	char c;
	char *morse;
} other[] = {
	{ ',', "--..--" },
	{ '.', ".-.-.-" },
	{ '?', "..--.." },
	{ '/', "-..-." },
	{ '-', "-....-" },
	{ ':', "---..." },
	{ ';', "-.-.-." },
	{ '(', "-.--.-." },	/* When converting from Morse, can't tell */
	{ ')', "-.--.-." },	/* '(' and ')' apart                      */
	{ '"', ".-..-." },
	{ '`', ".-..-." },
	{ '\'', ".----." },
	{ '+', ".-.-." },	/* AR */
	{ '=', "-...-" },	/* BT */
	{ '@', "...-.-" },	/* SK */
	{ '\0', NULL }
};

void	morse __P((int));
void	decode __P((char *));
void	show __P((char *));

static int sflag = 0;
static int dflag = 0;

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch;
	char *p;

	/* revoke */
	setegid(getgid());
	setgid(getgid());

	while ((ch = getopt(argc, argv, "dsh")) != -1)
		switch((char)ch) {
		case 'd':
			dflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case '?': case 'h':
		default:
			fprintf(stderr, "usage: morse [-ds] [string ...]\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	if (dflag) {
		if (*argv) {
			do {
				decode(*argv);
			} while (*++argv);
		} else {
			char foo[10];	/* All morse chars shorter than this */
			int isblank, i;

			i = 0;
			isblank = 0;
			while ((ch = getchar()) != EOF) {
				if (ch == '-' || ch == '.') {
					foo[i++] = ch;
					if (i == 10) {
						/* overrun means gibberish--print 'x' and
						 * advance */
						i = 0;
						putchar('x');
						while ((ch = getchar()) != EOF &&
						    (ch == '.' || ch == '-'))
							;
						isblank = 1;
					}
				} else if (i) {
					foo[i] = '\0';
					decode(foo);
					i = 0;
					isblank = 0;
				} else if (isspace(ch)) {
					if (isblank) {
						/* print whitespace for each double blank */
						putchar(' ');
						isblank = 0;
					} else
						isblank = 1;
				}
			}
		}
		putchar('\n');
	} else {
		if (*argv)
			do {
				for (p = *argv; *p; ++p)
					morse((int)*p);
				show("");
			} while (*++argv);
		else while ((ch = getchar()) != EOF)
			morse(ch);
		show("...-.-");	/* SK */
	}
	exit(0);
}

void
morse(c)
	int c;
{
	int i;

	if (isalpha(c))
		show(alph[c - (isupper(c) ? 'A' : 'a')]);
	else if (isdigit(c))
		show(digit[c - '0']);
	else if (isspace(c))
		show("");  /* could show BT for a pause */
	else {
		i = 0;
		while (other[i].c) {
			if (other[i].c == c) {
				show(other[i].morse);
				break;
			}
			i++;
		}
	}
}

void
decode(s)
	char *s;
{
	int i;
	
	for (i = 0; i < 10; i++)
		if (strcmp(digit[i], s) == 0) {
			putchar('0' + i);
			return;
		}
	
	for (i = 0; i < 26; i++)
		if (strcmp(alph[i], s) == 0) {
			putchar('A' + i);
			return;
		}
	i = 0;
	while (other[i].c) {
		if (strcmp(other[i].morse, s) == 0) {
			putchar(other[i].c);
			return;
		}
		i++;
	}
	putchar('x');	/* line noise */
}



void
show(s)
	char *s;
{
	if (sflag)
		printf(" %s", s);
	else for (; *s; ++s)
		printf(" %s", *s == '.' ? "dit" : "daw");
	printf("\n");
}
