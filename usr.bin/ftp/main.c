/*	$NetBSD: main.c,v 1.17 1997/02/01 10:45:07 lukem Exp $	*/

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
static char rcsid[] = "$NetBSD: main.c,v 1.17 1997/02/01 10:45:07 lukem Exp $";
#endif
#endif /* not lint */

/*
 * FTP User Program -- Command Interface.
 */
#include <sys/types.h>
#include <sys/socket.h>

#include <err.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ftp_var.h"

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct servent *sp;
	int ch, top, port, rval;
	struct passwd *pw = NULL;
	char *cp, homedir[MAXPATHLEN];

	sp = getservbyname("ftp", "tcp");
	if (sp == 0)
		ftpport = htons(FTP_PORT);	/* good fallback */
	else
		ftpport = sp->s_port;
	sp = getservbyname("http", "tcp");
	if (sp == 0)
		httpport = htons(HTTP_PORT);	/* good fallback */
	else
		httpport = sp->s_port;
	doglob = 1;
	interactive = 1;
	autologin = 1;
	passivemode = 0;
	preserve = 1;
	verbose = 0;
	progress = 0;
	mark = HASHBYTES;
	marg_sl = sl_init();

	cp = strrchr(argv[0], '/');
	cp = (cp == NULL) ? argv[0] : cp + 1;
	if (strcmp(cp, "pftp") == 0)
		passivemode = 1;

	fromatty = isatty(fileno(stdin));
	if (fromatty)
		verbose = 1;		/* verbose if from a tty */
	if (isatty(fileno(stdout)))
		progress = 1;		/* progress bar on if going to a tty */

	while ((ch = getopt(argc, argv, "adginpP:tvV")) != EOF) {
		switch (ch) {
		case 'a':
			anonftp = 1;
			break;

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
			passivemode = 1;
			break;

		case 'P':
			port = atoi(optarg);
			if (port <= 0)
				warnx("bad port number: %s", optarg);
			else
				ftpport = htons(port);
			break;

		case 't':
			trace = 1;
			break;

		case 'v':
			verbose = 1;
			break;

		case 'V':
			verbose = 0;
			break;

		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	cpend = 0;	/* no pending replies */
	proxy = 0;	/* proxy not active */
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

#ifndef SMALLFTP
	editing = 0;			/* command line editing off */
	if (fromatty) {
		editing = 1;		/* editing mode on if a tty */
		el = el_init(__progname, stdin, stdout); /* init editline */

		hist = history_init();		/* init the builtin history */
		history(hist, H_EVENT, 100);	/* remember 100 events */
		el_set(el, EL_HIST, history, hist);	/* use history */

		el_set(el, EL_EDITOR, "emacs");	/* default editor is emacs */
		el_set(el, EL_PROMPT, prompt);	/* set the prompt function */

		/* add local file completion, bind to TAB */
		el_set(el, EL_ADDFN, "ftp-complete",
		    "Context sensitive argument completion",
		    complete);
		el_set(el, EL_BIND, "^I", "ftp-complete", NULL);

		el_source(el, NULL);	/* read ~/.editrc */
	}
#endif /* !SMALLFTP */

	setttywidth(0);
	(void) signal(SIGWINCH, setttywidth);

	if (argc > 0) {
		if (strchr(argv[0], ':') != NULL) {
			anonftp = 1;	/* Handle "automatic" transfers. */
			rval = auto_fetch(argc, argv);
			if (rval >= 0)		/* -1 == connected and cd-ed */
				exit(rval);
		} else {
			char *xargv[5];

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

	alarmtimer(0);
	longjmp(toplevel, 1);
}

void
lostpeer()
{

	alarmtimer(0);
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
 * Generate a prompt
 */
char *
prompt()
{
	return ("ftp> ");
}

/*
 * Command parser.
 */
void
cmdscanner(top)
	int top;
{
	struct cmd *c;
	int num;

	if (!top 
#ifndef SMALLFTP
	    && !editing
#endif /* !SMALLFTP */
	    )
		(void) putchar('\n');
	for (;;) {
#ifndef SMALLFTP
		if (!editing) {
#endif /* !SMALLFTP */
			if (fromatty) {
				printf("%s", prompt());
				(void) fflush(stdout);
			}
			if (fgets(line, sizeof(line), stdin) == NULL)
				quit(0, 0);
			num = strlen(line);
			if (num == 0)
				break;
			if (line[--num] == '\n') {
				if (num == 0)
					break;
				line[num] = '\0';
			} else if (num == sizeof(line) - 2) {
				printf("sorry, input line too long\n");
				while ((num = getchar()) != '\n' && num != EOF)
					/* void */;
				break;
			} /* else it was a line without a newline */
#ifndef SMALLFTP
		} else {
			const char *buf;
			cursor_pos = NULL;

			if ((buf = el_gets(el, &num)) == NULL || num == 0)
				quit(0, 0);
			if (line[--num] == '\n') {
				if (num == 0)
					break;
			} else if (num >= sizeof(line)) {
				printf("sorry, input line too long\n");
				break;
			}
			memcpy(line, buf, num);
			line[num] = '\0';
			history(hist, H_ENTER, buf);
		}
#endif /* !SMALLFTP */

		makeargv();
		if (margc == 0)
			continue;
#if 0 && !defined(SMALLFTP)	/* XXX: don't want el_parse */
		/*
		 * el_parse returns -1 to signal that it's not been handled
		 * internally.
		 */
		if (el_parse(el, margc, margv) != -1)
			continue;
#endif /* !SMALLFTP */
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
		confirmrest = 0;
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
	const char *name;
{
	const char *p, *q;
	struct cmd *c, *found;
	int nmatches, longest;

	if (name == NULL)
		return (0);

	longest = 0;
	nmatches = 0;
	found = 0;
	for (c = cmdtab; (p = c->c_name) != NULL; c++) {
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
	char *argp;

	stringbase = line;		/* scan from first of buffer */
	argbase = argbuf;		/* store from first of buffer */
	slrflag = 0;
	marg_sl->sl_cur = 0;		/* reset to start of marg_sl */
	for (margc = 0; ; margc++) {
		argp = slurpstring();
		sl_add(marg_sl, argp);
		if (argp == NULL)
			break;
	}
#ifndef SMALLFTP
	if (cursor_pos == line) {
		cursor_argc = 0;
		cursor_argo = 0;
	} else if (cursor_pos != NULL) {
		cursor_argc = margc;
		cursor_argo = strlen(margv[margc-1]);
	}
#endif /* !SMALLFTP */
}

#ifdef SMALLFTP
#define INC_CHKCURSOR(x)	(x)++
#else  /* !SMALLFTP */
#define INC_CHKCURSOR(x)	{ (x)++ ; \
				if (x == cursor_pos) { \
					cursor_argc = margc; \
					cursor_argo = ap-argbase; \
					cursor_pos = NULL; \
				} }
						
#endif /* !SMALLFTP */

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
				INC_CHKCURSOR(stringbase);
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
		INC_CHKCURSOR(sb);
		goto S0;

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
		INC_CHKCURSOR(sb);
		goto S2;	/* slurp next character */

	case '"':
		INC_CHKCURSOR(sb);
		goto S3;	/* slurp quoted string */

	default:
		*ap = *sb;	/* add character to token */
		ap++;
		INC_CHKCURSOR(sb);
		got_one = 1;
		goto S1;
	}

S2:
	switch (*sb) {

	case '\0':
		goto OUT;

	default:
		*ap = *sb;
		ap++;
		INC_CHKCURSOR(sb);
		got_one = 1;
		goto S1;
	}

S3:
	switch (*sb) {

	case '\0':
		goto OUT;

	case '"':
		INC_CHKCURSOR(sb);
		goto S1;

	default:
		*ap = *sb;
		ap++;
		INC_CHKCURSOR(sb);
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
		StringList *buf;

		buf = sl_init();
		printf("%sommands may be abbreviated.  Commands are:\n\n",
		    proxy ? "Proxy c" : "C");
		for (c = cmdtab; c < &cmdtab[NCMDS]; c++)
			if (c->c_name && (!proxy || c->c_proxy))
				sl_add(buf, c->c_name);
		list_vertical(buf);
		sl_free(buf, 0);
		return;
	}

#define HELPINDENT ((int) sizeof ("disconnect"))

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

void
usage()
{
	(void)fprintf(stderr,
	    "usage: %s [-adginptvV] [host [port]]\n"
	    "       %s host:path[/]\n"
	    "       %s ftp://host[:port]/path[/]\n"
	    "       %s http://host[:port]/file\n",
	    __progname, __progname, __progname, __progname);
	exit(1);
}
