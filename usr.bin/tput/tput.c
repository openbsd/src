/*	$OpenBSD: tput.c,v 1.6 1999/03/06 20:19:22 millert Exp $	*/
/*	$NetBSD: tput.c,v 1.8 1995/08/31 22:11:37 jtc Exp $	*/

/*-
 * Copyright (c) 1980, 1988, 1993
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
"@(#) Copyright (c) 1980, 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)tput.c	8.3 (Berkeley) 4/28/95";
#endif
static char rcsid[] = "$OpenBSD: tput.c,v 1.6 1999/03/06 20:19:22 millert Exp $";
#endif /* not lint */

#include <termios.h>

#include <err.h>
#include <curses.h>
#include <term.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static void   prlongname __P((char *));
static void   setospeed __P((void));
static void   usage __P((void));
static char **process __P((char *, char *, char **));

extern char  *__progname;

int
main(argc, argv)
	int argc;
	char **argv;
{
	int ch, exitval, n, Sflag = 0;
	size_t len;
	char *p, *term, *str;

	term = NULL;
	while ((ch = getopt(argc, argv, "ST:")) != -1)
		switch(ch) {
		case 'T':
			term = optarg;
			break;
		case 'S':
			Sflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (Sflag && argc > 0)
		usage();

	if (!term && !(term = getenv("TERM")))
errx(2, "no terminal type specified and no TERM environmental variable.");
	if (setupterm(term, STDOUT_FILENO, NULL) == ERR)
		err(2, "setupterm failure");
	setospeed();
	if (strcmp(__progname, "clear") == 0) {
		if (Sflag)
			usage();
		*argv = "clear";
		*(argv+1) = NULL;
	}
	if (Sflag) {
		char **av;

		/* Build new argv based on stdin */
		argc = n = 0;
		av = NULL;
		while ((str = fgetln(stdin, &len)) != NULL) {
			if (str[len-1] != '\n')
				errx(1, "premature EOF");
			str[len-1] = '\0';
			/* grow av as needed */
			if (argc + 1 >= n) {
				n += 64;
				av = (char **)realloc(av, sizeof(char *) * n);
				if (av == NULL)
					errx(1, "out of memory");
				av = &av[argc];
			}
			while ((p = strsep(&str, " \t")) != NULL)
				if ((av[argc++] = strdup(p)) == NULL)
					errx(1, "out of memory");
		}
		if (argc > 0) {
			av[argc] = NULL;
			argv = av;
		}
	}
	for (exitval = 0; (p = *argv) != NULL; ++argv) {
		switch (*p) {
		case 'i':
			if (!strcmp(p, "init"))
				p = "is2";	/* XXX - is1 as well? */
			break;
		case 'l':
			if (!strcmp(p, "longname")) {
				prlongname(CUR term_names);
				continue;
			}
			break;
		case 'r':
			if (!strcmp(p, "reset"))
				p = "rs2";	/* XXX - rs1 as well? */
			break;
		}
		/* XXX - check termcap names too */
		if ((str = tigetstr(p)) != NULL && str != (char *)-1)
			argv = process(p, str, argv);
		else if ((n = tigetnum(p)) != -1 && n != -2)
			(void)printf("%d\n", n);
		else
			exitval = (tigetflag(p) == -1);

		if (argv == NULL)
			break;
	}
	exit(argv ? exitval : 2);
}

static void
prlongname(buf)
	char *buf;
{
	int savech;
	char *p, *savep;

	for (p = buf; *p && *p != ':'; ++p)
		continue;
	savech = *(savep = p);
	for (*p = '\0'; p >= buf && *p != '|'; --p)
		continue;
	(void)printf("%s\n", p + 1);
	*savep = savech;
}

static char **
process(cap, str, argv)
	char *cap, *str, **argv;
{
	static char errfew[] =
	    "not enough arguments (%d) for capability `%s'";
	static char errmany[] =
	    "too many arguments (%d) for capability `%s'";
	static char erresc[] =
	    "unknown %% escape `%c' for capability `%s'";
	char *cp;
	int arg_need, arg_rows, arg_cols;

	/* Count how many values we need for this capability. */
	for (cp = str, arg_need = 0; *cp != '\0'; cp++)
		if (*cp == '%')
			    switch (*++cp) {
			    case 'd':
			    case '2':
			    case '3':
			    case '.':
			    case '+':
				    arg_need++;
				    break;
			    case '%':
			    case '>':
			    case 'i':
			    case 'r':
			    case 'n':
			    case 'B':
			    case 'D':
				    break;
			    default:
				/*
				 * HP-UX has lots of them, but we complain
				 */
				 errx(2, erresc, *cp, cap);
			    }

	/* And print them. */
	switch (arg_need) {
	case 0:
		(void)putp(str);
		break;
	case 1:
		arg_cols = 0;

		if (*++argv == NULL || *argv[0] == '\0')
			errx(2, errfew, 1, cap);
		arg_rows = atoi(*argv);

		(void)putp(tparm(str, arg_cols, arg_rows));
		break;
	case 2:
		if (*++argv == NULL || *argv[0] == '\0')
			errx(2, errfew, 2, cap);
		arg_rows = atoi(*argv);

		if (*++argv == NULL || *argv[0] == '\0')
			errx(2, errfew, 2, cap);
		arg_cols = atoi(*argv);

		(void) tputs(tparm(str, arg_cols, arg_rows), arg_rows, putchar);
		break;

	default:
		errx(2, errmany, arg_need, cap);
	}
	return (argv);
}

static void
setospeed()
{
#undef ospeed
	extern short ospeed;
	struct termios t;

	if (tcgetattr(STDOUT_FILENO, &t) != -1)
		ospeed = 0;
	else
		ospeed = cfgetospeed(&t);
}

static void
usage()
{
	(void)fprintf(stderr,
	    "usage: %s [-T term] attribute [attribute-args] ...\n"
	    "       %s [-T term] -S\n", __progname, __progname);
	exit(1);
}
