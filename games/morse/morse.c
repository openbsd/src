/*	$OpenBSD: morse.c,v 1.6 1998/03/12 09:09:20 pjanzen Exp $	*/

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
static char rcsid[] = "$OpenBSD: morse.c,v 1.6 1998/03/12 09:09:20 pjanzen Exp $";
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

void	morse __P((int));
void	show __P((char *));

static int sflag = 0;

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

	while ((ch = getopt(argc, argv, "hs")) != -1)
		switch((char)ch) {
		case 's':
			sflag = 1;
			break;
		case '?': case 'h':
		default:
			fprintf(stderr, "usage: morse [-s] [string ...]");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	if (*argv)
		do {
			for (p = *argv; *p; ++p)
				morse((int)*p);
			show("");
		} while (*++argv);
	else while ((ch = getchar()) != EOF)
		morse(ch);
	show("...-.-");	/* SK */
	exit(0);
}

void
morse(c)
	int c;
{
	if (isalpha(c))
		show(alph[c - (isupper(c) ? 'A' : 'a')]);
	else if (isdigit(c))
		show(digit[c - '0']);
	else if (isspace(c))
		show("");  /* could show BT for a pause */
	else switch((char)c) {
		case ',':
			show("--..--");
			break;
		case '.':
			show(".-.-.-");
			break;
		case '?':
			show("..--..");
			break;
		case '/':
			show("-..-.");
			break;
		case '-':
			show("-....-");
			break;
		case ':':
			show("---...");
			break;
		case ';':
			show("-.-.-.");
			break;
		case '(':	case ')':
			show("-.--.-.");
			break;
		case '`': case '"':
			show(".-..-.");
			break;
		case '\'':
			show(".----.");
			break;
		case '+':   /* AR */
			show(".-.-.");
			break;
		case '=':   /* BT */
			show("-...-");
			break;
		case '@':   /* SK */
			show("...-.-");
			break;
		}
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
