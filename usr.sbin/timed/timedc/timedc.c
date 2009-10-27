/*	$OpenBSD: timedc.c,v 1.14 2009/10/27 23:59:57 deraadt Exp $	*/

/*-
 * Copyright (c) 1985, 1993 The Regents of the University of California.
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

#include "timedc.h"
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>

int	trace = 0;
FILE	*fd = NULL;
int	margc;
int	fromatty;
#define	MAX_MARGV 20
char	*margv[MAX_MARGV];
char	cmdline[200];

static struct cmd *getcmd(char *);
volatile sig_atomic_t gotintr;

int
main(int argc, char *argv[])
{
	extern int sock_raw, sock;
	struct sockaddr_in sin;
	struct cmd *c;

	sock_raw = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (sock_raw < 0) {
		perror("opening raw socket");
		exit(1);
	}

	openlog("timedc", LOG_ODELAY, LOG_AUTH);

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("opening socket");
		(void)close(sock_raw);
		return (-1);
	}

	memset(&sin, 0, sizeof sin);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		fprintf(stderr, "all reserved ports in use\n");
		(void)close(sock_raw);
		exit(1);
	}

	if (--argc > 0) {
		c = getcmd(*++argv);
		if (c == (struct cmd *)-1) {
			printf("?Ambiguous command\n");
			exit(1);
		}
		if (c == 0) {
			printf("?Invalid command\n");
			exit(1);
		}
		if (c->c_priv && getuid()) {
			printf("?Privileged command\n");
			exit(1);
		}
		(*c->c_handler)(argc, argv);
		exit(0);
	}

	fromatty = isatty(fileno(stdin));
	(void) signal(SIGINT, sigintr);
	for (;;) {
		if (gotintr) {
			putchar('\n');
			gotintr = 0;
		}
		if (fromatty) {
			printf("timedc> ");
			(void) fflush(stdout);
		}

		siginterrupt(SIGINT, 1);
		if (fgets(cmdline, sizeof(cmdline), stdin) == NULL) {
			if (errno == EINTR && gotintr) {
				siginterrupt(SIGINT, 0);
				continue;
			}
			quit(0, NULL);
		}
		siginterrupt(SIGINT, 0);

		if (cmdline[0] == 0)
			break;
		if (makeargv()) {
			printf("?Too many arguments\n");
			continue;
		}
		if (margv[0] == 0)
			continue;
		c = getcmd(margv[0]);
		if (c == (struct cmd *)-1) {
			printf("?Ambiguous command\n");
			continue;
		}
		if (c == 0) {
			printf("?Invalid command\n");
			continue;
		}
		if (c->c_priv && getuid()) {
			printf("?Privileged command\n");
			continue;
		}
		(*c->c_handler)(margc, margv);
	}
	return 0;
}

void
sigintr(int signo)
{
	if (!fromatty)
		_exit(0);
	gotintr = 1;
}

static struct cmd *
getcmd(char *name)
{
	char *p, *q;
	struct cmd *c, *found;
	int nmatches, longest;
	extern struct cmd cmdtab[];
	extern int NCMDS;

	longest = 0;
	nmatches = 0;
	found = 0;
	for (c = cmdtab; c < &cmdtab[NCMDS]; c++) {
		p = c->c_name;
		for (q = name; *q == *p++; q++)
			if (*q == 0)		/* exact match? */
				return (c);
		if (!*q) {			/* the name was a prefix */
			if (q - name > longest) {
				longest = q - name;
				nmatches = 1;
				found = c;
			} else if (q - name == longest)
				nmatches++;
		}
	}
	if (nmatches > 1)
		return ((struct cmd *)-1);
	return (found);
}

/*
 * Slice a string up into argc/argv.
 */
int
makeargv(void)
{
	char **argp = margv;
	char *cp;

	margc = 0;
	for (cp = cmdline; margc < MAX_MARGV - 1 && *cp; ) {
		while (isspace(*cp))
			cp++;
		if (*cp == '\0')
			break;
		*argp++ = cp;
		margc += 1;
		while (*cp != '\0' && !isspace(*cp))
			cp++;
		if (*cp == '\0')
			break;
		*cp++ = '\0';
	}
	if (margc == MAX_MARGV - 1)
		return 1;
	*argp++ = 0;
	return 0;
}

#define HELPINDENT (sizeof ("directory"))

/*
 * Help command.
 */
void
help(int argc, char *argv[])
{
	extern struct cmd cmdtab[];
	struct cmd *c;

	if (argc == 1) {
		int columns, width = 0, lines;
		extern int NCMDS;
		int i, j, w;

		printf("Commands may be abbreviated.  Commands are:\n\n");
		for (c = cmdtab; c < &cmdtab[NCMDS]; c++) {
			int len = strlen(c->c_name);

			if (len > width)
				width = len;
		}
		width = (width + 8) &~ 7;
		columns = 80 / width;
		if (columns == 0)
			columns = 1;
		lines = (NCMDS + columns - 1) / columns;
		for (i = 0; i < lines; i++) {
			for (j = 0; j < columns; j++) {
				c = cmdtab + j * lines + i;
				printf("%s", c->c_name);
				if (c + lines >= &cmdtab[NCMDS]) {
					printf("\n");
					break;
				}
				w = strlen(c->c_name);
				while (w < width) {
					w = (w + 8) &~ 7;
					putchar('\t');
				}
			}
		}
		return;
	}
	while (--argc > 0) {
		char *arg;
		arg = *++argv;
		c = getcmd(arg);
		if (c == (struct cmd *)-1)
			printf("?Ambiguous help command %s\n", arg);
		else if (c == (struct cmd *)0)
			printf("?Invalid help command %s\n", arg);
		else
			printf("%-*s\t%s\n", (int)HELPINDENT,
			    c->c_name, c->c_help);
	}
}
