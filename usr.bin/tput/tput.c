/*	$OpenBSD: tput.c,v 1.16 2004/10/05 14:46:11 jaredy Exp $	*/

/*
 * Copyright (c) 1999 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1980, 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)tput.c	8.3 (Berkeley) 4/28/95";
#endif
static char rcsid[] = "$OpenBSD: tput.c,v 1.16 2004/10/05 14:46:11 jaredy Exp $";
#endif /* not lint */

#include <sys/param.h>

#include <ctype.h>
#include <err.h>
#include <curses.h>
#include <term.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

#include <sys/wait.h>

static void   init(void);
static char **process(char *, char *, char **);
static void   reset(void);
static void   set_margins(void);
static void   usage(void);

extern char  *__progname;

int
main(int argc, char *argv[])
{
	int ch, exitval, n, Sflag;
	size_t len;
	char *p, *term, *str;
	char **oargv;

	oargv = argv;
	term = NULL;
	Sflag = exitval = 0;
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
		errx(2, "No value for $TERM and no -T specified");

	/*
	 * NOTE: tgetent() will call setupterm() and set ospeed for us
	 * (this is ncurses-specific behavior)
	 */
	if (tgetent(NULL, term) != 1)
		errx(3, "Unknown terminal type `%s'", term);

	if (strcmp(__progname, "clear") == 0) {
		if (Sflag)
			usage();
		argv = oargv;
		*argv = __progname;
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
			while ((p = strsep(&str, " \t")) != NULL) {
				/* grow av as needed */
				if (argc + 1 >= n) {
					n += 64;
					av = (char **)realloc(av, sizeof(char *) * n);
					if (av == NULL)
						errx(1, "out of memory");
				}
				if (*p != '\0' &&
				    (av[argc++] = strdup(p)) == NULL)
					errx(1, "out of memory");
			}
		}
		if (argc > 0) {
			av[argc] = NULL;
			argv = av;
		}
	}
	while ((p = *argv++)) {
		switch (*p) {
		case 'i':
			if (!strcmp(p, "init")) {
				init();
				continue;
			}
			break;
		case 'l':
			if (!strcmp(p, "longname")) {
				puts(longname());
				continue;
			}
			break;
		case 'r':
			if (!strcmp(p, "reset")) {
				reset();
				continue;
			}
			break;
		}

		/* First try as terminfo */
		if ((str = tigetstr(p)) && str != (char *)-1)
			argv = process(p, str, argv);
		else if ((n = tigetnum(p)) != -2)
			(void)printf("%d\n", n);
		else if ((n = tigetflag(p)) != -1)
			exitval = !n;
		/* Then fall back on termcap */
		else if ((str = tgetstr(p, NULL)))
			argv = process(p, str, argv);
		else if ((n = tgetnum(p)) != -1)
			(void)printf("%d\n", n);
		else if ((exitval = tgetflag(p)) != 0)
			exitval = !exitval;
		else {
			warnx("Unknown terminfo capability `%s'", p);
			exitval = 4;
		}
	}
	exit(exitval);
}

static char **
process(char *cap, char *str, char **argv)
{
	char *cp, *s, *nargv[9];
	int arg_need, popcount, i;

	/* Count how many values we need for this capability. */
	for (cp = str, arg_need = popcount = 0; *cp != '\0'; cp++) {
		if (*cp == '%') {
			switch (*++cp) {
			case '%':
			   	cp++;
				break;
			case 'i':
				if (popcount < 2)
					popcount = 2;
				break;
			case 'p':
				cp++;
				if (isdigit(cp[1]) && popcount < cp[1] - '0')
					popcount = cp[1] - '0';
				break;
			case 'd':
			case 's':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
			case '.':
			case '+':
				arg_need++;
				break;
			default:
				break;
			}
		}
	}
	arg_need = MAX(arg_need, popcount);
	if (arg_need > 9)
		errx(2, "too many arguments (%d) for capability `%s'",
		    arg_need, cap);
	
	for (i = 0; i < arg_need; i++) {
		long l;

		if (argv[i] == NULL)
			errx(2, "not enough arguments (%d) for capability `%s'",
			    arg_need, cap);

		/* convert ascii representation of numbers to longs */
		if (isdigit(argv[i][0]) && (l = strtol(argv[i], &cp, 10)) >= 0
		    && l < LONG_MAX && *cp == '\0')
			nargv[i] = (char *)l;
		else
			nargv[i] = argv[i];
	}

	s = tparm(str, nargv[0], nargv[1], nargv[2], nargv[3],
	    nargv[4], nargv[5], nargv[6], nargv[7], nargv[8]);
	putp(s);
	fflush(stdout);

	return (argv + arg_need);
}

static void
init(void)
{
	FILE *ifile;
	size_t len;
	char *buf;
	int wstatus;

	if (init_prog && !issetugid()) {
		switch (vfork()) {
		case -1:
			err(4, "vfork");
			break;
		case 0:
			/* child */
			execl(init_prog, init_prog, (char *)NULL);
			_exit(127);
			break;
		default:
			wait(&wstatus);
			/* parent */
			break;
		}
	}
	if (init_1string)
		putp(init_1string);
	if (init_2string)
		putp(init_2string);
	set_margins();
	/* always use 8 space tabs */
	if (init_tabs != 8 && clear_all_tabs && set_tab) {
		int i;

		putp(clear_all_tabs);
		for (i = 0; i < (columns - 1) / 8; i++) {
			if (parm_right_cursor)
				putp(tparm(parm_right_cursor, 8));
			else
				fputs("        ", stdout);
			putp(set_tab);
		}
	}
	if (init_file && !issetugid() && (ifile = fopen(init_file, "r"))) {
		while ((buf = fgetln(ifile, &len)) != NULL) {
			if (buf[len-1] != '\n')
				errx(1, "premature EOF reading %s", init_file);
			buf[len-1] = '\0';
			putp(buf);
		}
		fclose(ifile);
	}
	if (init_3string)
		putp(init_3string);
	fflush(stdout);
}

static void
reset(void)
{
	FILE *rfile;
	size_t len;
	char *buf;

	if (reset_1string)
		putp(reset_1string);
	if (reset_2string)
		putp(reset_2string);
	set_margins();
	if (reset_file && !issetugid() && (rfile = fopen(reset_file, "r"))) {
		while ((buf = fgetln(rfile, &len)) != NULL) {
			if (buf[len-1] != '\n')
				errx(1, "premature EOF reading %s", reset_file);
			buf[len-1] = '\0';
			putp(buf);
		}
		fclose(rfile);
	}
	if (reset_3string)
		putp(reset_3string);
	fflush(stdout);
}

static void
set_margins(void)
{

	/*
	 * Four possibilities:
	 *	1) we have set_lr_margin and can set things with one call
	 *	2) we have set_{left,right}_margin_parm, use two calls
	 *	3) we have set_{left,right}_margin, set based on position
	 *	4) none of the above, leave things the way they are
	 */
	if (set_lr_margin) {
		putp(tparm(set_lr_margin, 0, columns - 1));
	} else if (set_left_margin_parm && set_right_margin_parm) {
		putp(tparm(set_left_margin_parm, 0));
		putp(tparm(set_right_margin_parm, columns - 1));
	} else if (set_left_margin && set_right_margin && clear_margins) {
		putp(clear_margins);

		/* go to column 0 and set the left margin */
		putp(carriage_return ? carriage_return : "\r");
		putp(set_left_margin);

		/* go to last column and set the right margin */
		if (parm_right_cursor)
			putp(tparm(parm_right_cursor, columns - 1));
		else
			printf("%*s", columns - 1, " ");
		putp(set_right_margin);
		putp(carriage_return ? carriage_return : "\r");
	}
	fflush(stdout);
}

static void
usage(void)
{

	if (strcmp(__progname, "clear") == 0)
		(void)fprintf(stderr, "usage: %s [-T term]\n", __progname);
	else
		(void)fprintf(stderr,
		    "usage: %s [-T term] attribute [attribute-args] ...\n"
		    "       %s [-T term] -S\n", __progname, __progname);
	exit(1);
}
