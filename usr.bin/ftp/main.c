/*	$OpenBSD: main.c,v 1.6 1996/09/03 18:00:06 deraadt Exp $	*/

/*
 * Copyright (c) 1985, 1989, 1993, 1994
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
"@(#) Copyright (c) 1985, 1989, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)main.c	8.6 (Berkeley) 10/9/94";
#else
static char rcsid[] = "$OpenBSD: main.c,v 1.6 1996/09/03 18:00:06 deraadt Exp $";
#endif
#endif /* not lint */

/*
 * FTP User Program -- Command Interface.
 */
/*#include <sys/ioctl.h>*/
#include <sys/types.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <arpa/ftp.h>

#include <ctype.h>
#include <err.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ftp_var.h"

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, top;
	struct passwd *pw = NULL;
	char *cp, homedir[MAXPATHLEN];
	int force_port = 0;

	sp = getservbyname("ftp", "tcp");
	if (sp == 0)
		errx(1, "ftp/tcp: unknown service");
	doglob = 1;
	interactive = 1;
	autologin = 1;

	while ((ch = getopt(argc, argv, "p:dgintv")) != EOF) {
		switch (ch) {
		case 'd':
			options |= SO_DEBUG;
			debug++;
			break;
			
		case 'g':
			doglob = 0;
			break;

		case 'i':
			interactive = 0;
			break;

		case 'n':
			autologin = 0;
			break;

		case 'p':
			force_port = atoi(optarg);
			break;

		case 't':
			trace++;
			break;

		case 'v':
			verbose++;
			break;

		default:
			(void)fprintf(stderr,
				"usage: ftp [-dgintv] [host [port]]\n");
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	fromatty = isatty(fileno(stdin));
	if (fromatty)
		verbose++;
	cpend = 0;	/* no pending replies */
	proxy = 0;	/* proxy not active */
	passivemode = 0; /* passive mode not active */
	crflag = 1;	/* strip c.r. on ascii gets */
	sendport = -1;	/* not using ports */
	/*
	 * Set up the home directory in case we're globbing.
	 */
	cp = getlogin();
	if (cp != NULL) {
		pw = getpwnam(cp);
	}
	if (pw == NULL)
		pw = getpwuid(getuid());
	if (pw != NULL) {
		home = homedir;
		(void) strcpy(home, pw->pw_dir);
	}
	if (argc > 0 && strchr(argv[0], ':')) {
		int ret = 0;
		anonftp = 1;

		while (argc > 0 && strchr(argv[0], ':')) {
			char *xargv[5];
			extern char *__progname;
			char portstr[20], *p, *bufp = NULL;
			char *host = NULL, *dir = NULL, *file = NULL;
			int xargc = 2;

			if (setjmp(toplevel))
				exit(0);
			(void) signal(SIGINT, intr);
			(void) signal(SIGPIPE, lostpeer);
			xargv[0] = __progname;

			host = strdup(argv[0]);
			if (host == NULL) {
				ret = 1;
				goto bail;
			}
			if (!strncmp(host, "http://", strlen("http://"))) {
				http_fetch(host);
				goto bail;
			}
			if (strncmp(host, "ftp://", sizeof("ftp://")-1) == 0) {
				host += sizeof("ftp://") - 1;
				p = strchr(host, '/');
			} else
				p = strchr(host, ':');
			*p = '\0';
			dir = ++p;
			p = strrchr(p, '/');
			if (p) {
				*p = '\0';
				file = ++p;
			} else {
				file = dir;
				dir = NULL;
			}

			xargv[1] = host;
			xargv[2] = NULL;
			xargc = 2;
			if (force_port) {
				xargv[2] = portstr;
				snprintf(portstr, sizeof portstr, "%d",
				    force_port);
				xargc++;
			}
			setpeer(xargc, xargv);
			if (!connected) {
				printf("failed to connect to %s\n", host);
				ret = 1;
				goto bail;
			}

			setbinary(NULL, 0);

			if (dir) {
				xargv[1] = dir;
				xargv[2] = NULL;
				xargc = 2;
				cd(xargc, xargv);
			}

			/* fetch file */
			xargv[1] = file;
			xargv[2] = NULL;
			xargc = 2;
			get(xargc, xargv);

			/* get ready for the next file */
bail:
			if (bufp) {
				free(bufp);
				bufp = NULL;
			}
			if (connected)
				disconnect(1, xargv);
			--argc;
			argv++;
		}
		exit(ret);
	}
	if (argc > 0) {
		char *xargv[5];
		extern char *__progname;

		if (setjmp(toplevel))
			exit(0);
		(void) signal(SIGINT, intr);
		(void) signal(SIGPIPE, lostpeer);
		xargv[0] = __progname;
		xargv[1] = argv[0];
		xargv[2] = argv[1];
		xargv[3] = argv[2];
		xargv[4] = NULL;
		setpeer(argc+1, xargv);
	}
	top = setjmp(toplevel) == 0;
	if (top) {
		(void) signal(SIGINT, intr);
		(void) signal(SIGPIPE, lostpeer);
	}
	for (;;) {
		cmdscanner(top);
		top = 1;
	}
}

void
intr()
{

	longjmp(toplevel, 1);
}

void
lostpeer()
{

	if (connected) {
		if (cout != NULL) {
			(void) shutdown(fileno(cout), 1+1);
			(void) fclose(cout);
			cout = NULL;
		}
		if (data >= 0) {
			(void) shutdown(data, 1+1);
			(void) close(data);
			data = -1;
		}
		connected = 0;
	}
	pswitch(1);
	if (connected) {
		if (cout != NULL) {
			(void) shutdown(fileno(cout), 1+1);
			(void) fclose(cout);
			cout = NULL;
		}
		connected = 0;
	}
	proxflag = 0;
	pswitch(0);
}

/*
char *
tail(filename)
	char *filename;
{
	char *s;
	
	while (*filename) {
		s = strrchr(filename, '/');
		if (s == NULL)
			break;
		if (s[1])
			return (s + 1);
		*s = '\0';
	}
	return (filename);
}
*/

/*
 * Command parser.
 */
void
cmdscanner(top)
	int top;
{
	struct cmd *c;
	int l;

	if (!top)
		(void) putchar('\n');
	for (;;) {
		if (fromatty) {
			printf("ftp> ");
			(void) fflush(stdout);
		}
		if (fgets(line, sizeof line, stdin) == NULL)
			quit(0, 0);
		l = strlen(line);
		if (l == 0)
			break;
		if (line[--l] == '\n') {
			if (l == 0)
				break;
			line[l] = '\0';
		} else if (l == sizeof(line) - 2) {
			printf("sorry, input line too long\n");
			while ((l = getchar()) != '\n' && l != EOF)
				/* void */;
			break;
		} /* else it was a line without a newline */
		makeargv();
		if (margc == 0) {
			continue;
		}
		c = getcmd(margv[0]);
		if (c == (struct cmd *)-1) {
			printf("?Ambiguous command\n");
			continue;
		}
		if (c == 0) {
			printf("?Invalid command\n");
			continue;
		}
		if (c->c_conn && !connected) {
			printf("Not connected.\n");
			continue;
		}
		(*c->c_handler)(margc, margv);
		if (bell && c->c_bell)
			(void) putchar('\007');
		if (c->c_handler != help)
			break;
	}
	(void) signal(SIGINT, intr);
	(void) signal(SIGPIPE, lostpeer);
}

struct cmd *
getcmd(name)
	char *name;
{
	char *p, *q;
	struct cmd *c, *found;
	int nmatches, longest;

	if (name == NULL)
		return (0);

	longest = 0;
	nmatches = 0;
	found = 0;
	for (c = cmdtab; p = c->c_name; c++) {
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

int slrflag;

void
makeargv()
{
	char **argp;

	argp = margv;
	stringbase = line;		/* scan from first of buffer */
	argbase = argbuf;		/* store from first of buffer */
	slrflag = 0;
	for (margc = 0; ; margc++) {
		/* Expand array if necessary */
		if (margc == margvlen) {
			margv = (margvlen == 0)
				? (char **)malloc(20 * sizeof(char *))
				: (char **)realloc(margv,
					(margvlen + 20)*sizeof(char *));
			if (margv == NULL)
				errx(1, "cannot realloc argv array");
			margvlen += 20;
			argp = margv + margc;
		}

		if ((*argp++ = slurpstring()) == NULL)
			break;
	}

}

/*
 * Parse string into argbuf;
 * implemented with FSM to
 * handle quoting and strings
 */
char *
slurpstring()
{
	int got_one = 0;
	char *sb = stringbase;
	char *ap = argbase;
	char *tmp = argbase;		/* will return this if token found */

	if (*sb == '!' || *sb == '$') {	/* recognize ! as a token for shell */
		switch (slrflag) {	/* and $ as token for macro invoke */
			case 0:
				slrflag++;
				stringbase++;
				return ((*sb == '!') ? "!" : "$");
				/* NOTREACHED */
			case 1:
				slrflag++;
				altarg = stringbase;
				break;
			default:
				break;
		}
	}

S0:
	switch (*sb) {

	case '\0':
		goto OUT;

	case ' ':
	case '\t':
		sb++; goto S0;

	default:
		switch (slrflag) {
			case 0:
				slrflag++;
				break;
			case 1:
				slrflag++;
				altarg = sb;
				break;
			default:
				break;
		}
		goto S1;
	}

S1:
	switch (*sb) {

	case ' ':
	case '\t':
	case '\0':
		goto OUT;	/* end of token */

	case '\\':
		sb++; goto S2;	/* slurp next character */

	case '"':
		sb++; goto S3;	/* slurp quoted string */

	default:
		*ap++ = *sb++;	/* add character to token */
		got_one = 1;
		goto S1;
	}

S2:
	switch (*sb) {

	case '\0':
		goto OUT;

	default:
		*ap++ = *sb++;
		got_one = 1;
		goto S1;
	}

S3:
	switch (*sb) {

	case '\0':
		goto OUT;

	case '"':
		sb++; goto S1;

	default:
		*ap++ = *sb++;
		got_one = 1;
		goto S3;
	}

OUT:
	if (got_one)
		*ap++ = '\0';
	argbase = ap;			/* update storage pointer */
	stringbase = sb;		/* update scan pointer */
	if (got_one) {
		return (tmp);
	}
	switch (slrflag) {
		case 0:
			slrflag++;
			break;
		case 1:
			slrflag++;
			altarg = (char *) 0;
			break;
		default:
			break;
	}
	return ((char *)0);
}

#define HELPINDENT ((int) sizeof ("directory"))

/*
 * Help command.
 * Call each command handler with argc == 0 and argv[0] == name.
 */
void
help(argc, argv)
	int argc;
	char *argv[];
{
	struct cmd *c;

	if (argc == 1) {
		int i, j, w, k;
		int columns, width = 0, lines;

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
				if (c->c_name && (!proxy || c->c_proxy)) {
					printf("%s", c->c_name);
				}
				else if (c->c_name) {
					for (k=0; k < strlen(c->c_name); k++) {
						(void) putchar(' ');
					}
				}
				if (c + lines >= &cmdtab[NCMDS]) {
					printf("\n");
					break;
				}
				w = strlen(c->c_name);
				while (w < width) {
					w = (w + 8) &~ 7;
					(void) putchar('\t');
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
			printf("%-*s\t%s\n", HELPINDENT,
				c->c_name, c->c_help);
	}
}
