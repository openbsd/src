/*	$OpenBSD: xstr.c,v 1.17 2011/04/06 11:36:26 miod Exp $	*/
/*	$NetBSD: xstr.c,v 1.5 1994/12/24 16:57:59 cgd Exp $	*/

/*
 * Copyright (c) 1980, 1993
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

#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "pathnames.h"

/*
 * xstr - extract and hash strings in a C program
 *
 * Bill Joy UCB
 * November, 1978
 */

#define	BUCKETS	128

off_t	tellpt;
off_t	mesgpt;
char	*strings = "strings";
char	*array = 0;

int	cflg;
int	vflg;
int	readstd;

struct	hash {
	off_t	hpt;
	char	*hstr;
	struct	hash *hnext;
	short	hnew;
} bucket[BUCKETS];

void process(char *);
off_t yankstr(char **);
int octdigit(char);
void inithash(void);
int fgetNUL(char *, int, FILE *);
int xgetc(FILE *);
off_t hashit(char *, int);
void flushsh(void);
void found(int, off_t, char *);
void prstr(char *);
void xsdotc(void);
char lastchr(char *);
int istail(char *, char *);
void onintr(void);

int
main(int argc, char *argv[])
{
	int c;
	int fdesc;

	while ((c = getopt(argc, argv, "cvl:-")) != -1)
		switch (c) {
		case '-':
			readstd++;
			break;
		case 'c':
			cflg++;
			break;
		case 'v':
			vflg++;
			break;
		case 'l':
			array = optarg;
			break;
		default:
			fprintf(stderr,
			    "usage: xstr [-cv] [-l array] [-] [file ...]\n");
			exit(1);
		} 
	argc -= optind;
	argv += optind;

	if (array == 0)
		array = "xstr";

	if (signal(SIGINT, SIG_IGN) == SIG_DFL)
		signal(SIGINT, (void(*)(int))onintr);
	if (cflg || (argc == 0 && !readstd))
		inithash();
	else {
		strings = strdup (_PATH_TMPFILE);
		if (strings == NULL) {
			fprintf(stderr, "Unable to allocate memory: %s",
			    strerror (errno));
			exit(1);
		}
		fdesc = mkstemp (strings);
		if (fdesc < 0) {
			fprintf(stderr, "Unable to create temporary file.\n");
			exit(1);
		}
		close (fdesc);
	}

	while (readstd || argc > 0) {
		if (freopen("x.c", "w", stdout) == NULL) {
			perror("x.c");
			exit(1);
		}
		if (!readstd && freopen(argv[0], "r", stdin) == NULL) {
			perror(argv[0]);
			exit(2);
		}
		process("x.c");
		if (readstd == 0)
			argc--, argv++;
		else
			readstd = 0;
	}
	flushsh();
	if (cflg == 0)
		xsdotc();
	if (strings[0] == '/')
		unlink(strings);
	exit(0);
}

char linebuf[BUFSIZ];

void
process(char *name)
{
	char *cp;
	int c;
	int incomm = 0;
	int ret;

	printf("extern char\t%s[];\n", array);
	for (;;) {
		if (fgets(linebuf, sizeof linebuf, stdin) == NULL) {
			if (ferror(stdin)) {
				perror(name);
				exit(3);
			}
			break;
		}
		if (linebuf[0] == '#') {
			if (linebuf[1] == ' ' && isdigit(linebuf[2]))
				printf("#line%s", &linebuf[1]);
			else
				printf("%s", linebuf);
			continue;
		}
		for (cp = linebuf; (c = *cp++); )
			switch (c) {
			case '"':
				if (incomm)
					goto def;
				if ((ret = (int) yankstr(&cp)) == -1)
					goto out;
				printf("(&%s[%d])", array, ret);
				break;
			case '\'':
				if (incomm)
					goto def;
				putchar(c);
				if (*cp)
					putchar(*cp++);
				break;
			case '/':
				if (incomm || *cp != '*')
					goto def;
				incomm = 1;
				cp++;
				printf("/*");
				continue;
			case '*':
				if (incomm && *cp == '/') {
					incomm = 0;
					cp++;
					printf("*/");
					continue;
				}
				goto def;
			def:
			default:
				putchar(c);
				break;
			}
	}
out:
	if (ferror(stdout))
		perror("x.c"), onintr();
}

off_t
yankstr(char **cpp)
{
	char *cp = *cpp;
	int c, ch;
	char dbuf[BUFSIZ];
	char *dp = dbuf;
	char *tp;

	while ((c = *cp++)) {
		switch (c) {
		case '"':
			cp++;
			goto out;
		case '\\':
			c = *cp++;
			if (c == 0)
				break;
			if (c == '\n') {
				if (fgets(linebuf, sizeof linebuf, stdin)
				    == NULL) {
					if (ferror(stdin)) {
						perror("x.c");
						exit(3);
					}
					return(-1);
				}
				cp = linebuf;
				continue;
			}
			for (tp = "b\bt\tr\rn\nf\f\\\\\"\""; (ch = *tp++); tp++)
				if (c == ch) {
					c = *tp;
					goto gotc;
				}
			if (!octdigit(c)) {
				*dp++ = '\\';
				break;
			}
			c -= '0';
			if (!octdigit(*cp))
				break;
			c <<= 3, c += *cp++ - '0';
			if (!octdigit(*cp))
				break;
			c <<= 3, c += *cp++ - '0';
			break;
		}
gotc:
		*dp++ = c;
	}
out:
	*cpp = --cp;
	*dp = 0;
	return (hashit(dbuf, 1));
}

int
octdigit(char c)
{

	return (isdigit(c) && c != '8' && c != '9');
}

void
inithash(void)
{
	char buf[BUFSIZ];
	FILE *mesgread = fopen(strings, "r");

	if (mesgread == NULL)
		return;
	for (;;) {
		mesgpt = tellpt;
		if (fgetNUL(buf, sizeof buf, mesgread) == 0)
			break;
		hashit(buf, 0);
	}
	fclose(mesgread);
}

int
fgetNUL(char *obuf, int rmdr, FILE  *file)
{
	int c;
	char *buf = obuf;

	while (--rmdr > 0 && (c = xgetc(file)) != 0 && c != EOF)
		*buf++ = c;
	*buf++ = 0;
	return ((feof(file) || ferror(file)) ? 0 : 1);
}

int
xgetc(FILE *file)
{

	tellpt++;
	return (getc(file));
}


off_t
hashit(char *str, int new)
{
	int i;
	struct hash *hp, *hp0;

	hp = hp0 = &bucket[lastchr(str) & 0177];
	while (hp->hnext) {
		hp = hp->hnext;
		i = istail(str, hp->hstr);
		if (i >= 0)
			return (hp->hpt + i);
	}
	if ((hp = (struct hash *) calloc(1, sizeof (*hp))) == NULL) {
		perror("xstr");
		exit(8);
	}
	hp->hpt = mesgpt;
	if (!(hp->hstr = strdup(str))) {
		(void)fprintf(stderr, "xstr: %s\n", strerror(errno));
		exit(1);
	}
	mesgpt += strlen(hp->hstr) + 1;
	hp->hnext = hp0->hnext;
	hp->hnew = new;
	hp0->hnext = hp;
	return (hp->hpt);
}

void
flushsh(void)
{
	int i;
	struct hash *hp;
	FILE *mesgwrit;
	int old = 0, new = 0;

	for (i = 0; i < BUCKETS; i++)
		for (hp = bucket[i].hnext; hp != NULL; hp = hp->hnext)
			if (hp->hnew)
				new++;
			else
				old++;
	if (new == 0 && old != 0)
		return;
	mesgwrit = fopen(strings, old ? "r+" : "w");
	if (mesgwrit == NULL) {
		perror(strings);
		exit(4);
	}
	for (i = 0; i < BUCKETS; i++)
		for (hp = bucket[i].hnext; hp != NULL; hp = hp->hnext) {
			found(hp->hnew, hp->hpt, hp->hstr);
			if (hp->hnew) {
				fseek(mesgwrit, hp->hpt, SEEK_SET);
				fwrite(hp->hstr, strlen(hp->hstr) + 1, 1,
				    mesgwrit);
				if (ferror(mesgwrit)) {
					perror(strings);
					exit(4);
				}
			}
		}
	if (fclose(mesgwrit) == EOF) {
		perror(strings);
		exit(4);
	}
}

void
found(int new, off_t off, char *str)
{
	if (vflg == 0)
		return;
	if (!new)
		fprintf(stderr, "found at %d:", (int) off);
	else
		fprintf(stderr, "new at %d:", (int) off);
	prstr(str);
	fprintf(stderr, "\n");
}

void
prstr(char *cp)
{
	int c;

	while ((c = (*cp++ & 0377)))
		if (c < ' ')
			fprintf(stderr, "^%c", c + '`');
		else if (c == 0177)
			fprintf(stderr, "^?");
		else if (c > 0200)
			fprintf(stderr, "\\%03o", c);
		else
			fprintf(stderr, "%c", c);
}

void
xsdotc(void)
{
	FILE *strf = fopen(strings, "r");
	FILE *xdotcf;

	if (strf == NULL) {
		perror(strings);
		exit(5);
	}
	xdotcf = fopen("xs.c", "w");
	if (xdotcf == NULL) {
		perror("xs.c");
		exit(6);
	}
	fprintf(xdotcf, "char\t%s[] = {\n", array);
	for (;;) {
		int i, c;

		for (i = 0; i < 8; i++) {
			c = getc(strf);
			if (ferror(strf)) {
				perror(strings);
				onintr();
			}
			if (feof(strf)) {
				fprintf(xdotcf, "\n");
				goto out;
			}
			fprintf(xdotcf, "0x%02x,", c);
		}
		fprintf(xdotcf, "\n");
	}
out:
	fprintf(xdotcf, "};\n");
	fclose(xdotcf);
	fclose(strf);
}

char
lastchr(char *cp)
{

	while (cp[0] && cp[1])
		cp++;
	return (*cp);
}

int
istail(char *str, char *of)
{
	int d = strlen(of) - strlen(str);

	if (d < 0 || strcmp(&of[d], str) != 0)
		return (-1);
	return (d);
}

void
onintr(void)
{

	signal(SIGINT, SIG_IGN);
	if (strings[0] == '/')
		unlink(strings);
	unlink("x.c");
	unlink("xs.c");
	_exit(7);
}
