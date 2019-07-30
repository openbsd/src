/*	$OpenBSD: xargs.c,v 1.33 2017/10/16 13:10:50 anton Exp $	*/
/*	$FreeBSD: xargs.c,v 1.51 2003/05/03 19:09:11 obrien Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * John B. Roll Jr.
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
 *
 * $xMach: xargs.c,v 1.6 2002/02/23 05:27:47 tim Exp $
 */

#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <langinfo.h>
#include <locale.h>
#include <paths.h>
#include <regex.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "pathnames.h"

static void	parse_input(int, char *[]);
static void	prerun(int, char *[]);
static int	prompt(void);
static void	run(char **);
static void	usage(void);
void		strnsubst(char **, const char *, const char *, size_t);
static void	waitchildren(const char *, int);

static char **av, **bxp, **ep, **endxp, **xp;
static char *argp, *bbp, *ebp, *inpline, *p, *replstr;
static const char *eofstr;
static int count, insingle, indouble, oflag, pflag, tflag, Rflag, rval, zflag;
static int cnt, Iflag, jfound, Lflag, wasquoted, xflag, runeof = 1;
static int curprocs, maxprocs;
static size_t inpsize;

extern char **environ;

int
main(int argc, char *argv[])
{
	long arg_max;
	int ch, Jflag, nargs, nflag, nline;
	size_t linelen;
	char *endptr;
	const char *errstr;

	inpline = replstr = NULL;
	ep = environ;
	eofstr = "";
	Jflag = nflag = 0;

	(void)setlocale(LC_MESSAGES, "");

	/*
	 * POSIX.2 limits the exec line length to ARG_MAX - 2K.  Running that
	 * caused some E2BIG errors, so it was changed to ARG_MAX - 4K.  Given
	 * that the smallest argument is 2 bytes in length, this means that
	 * the number of arguments is limited to:
	 *
	 *	 (ARG_MAX - 4K - LENGTH(utility + arguments)) / 2.
	 *
	 * We arbitrarily limit the number of arguments to 5000.  This is
	 * allowed by POSIX.2 as long as the resulting minimum exec line is
	 * at least LINE_MAX.  Realloc'ing as necessary is possible, but
	 * probably not worthwhile.
	 */
	nargs = 5000;
	if ((arg_max = sysconf(_SC_ARG_MAX)) == -1)
		errx(1, "sysconf(_SC_ARG_MAX) failed");

	if (pledge("stdio rpath proc exec", NULL) == -1)
		err(1, "pledge");

	nline = arg_max - 4 * 1024;
	while (*ep != NULL) {
		/* 1 byte for each '\0' */
		nline -= strlen(*ep++) + 1 + sizeof(*ep);
	}
	maxprocs = 1;
	while ((ch = getopt(argc, argv, "0E:I:J:L:n:oP:pR:rs:tx")) != -1)
		switch (ch) {
		case 'E':
			eofstr = optarg;
			break;
		case 'I':
			Jflag = 0;
			Iflag = 1;
			Lflag = 1;
			replstr = optarg;
			break;
		case 'J':
			Iflag = 0;
			Jflag = 1;
			replstr = optarg;
			break;
		case 'L':
			Lflag = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr)
				errx(1, "-L %s: %s", optarg, errstr);
			break;
		case 'n':
			nflag = 1;
			nargs = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr)
				errx(1, "-n %s: %s", optarg, errstr);
			break;
		case 'o':
			oflag = 1;
			break;
		case 'P':
			maxprocs = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr)
				errx(1, "-P %s: %s", optarg, errstr);
			break;
		case 'p':
			pflag = 1;
			break;
		case 'r':
			runeof = 0;
			break;
		case 'R':
			Rflag = strtol(optarg, &endptr, 10);
			if (*endptr != '\0')
				errx(1, "replacements must be a number");
			break;
		case 's':
			nline = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr)
				errx(1, "-s %s: %s", optarg, errstr);
			break;
		case 't':
			tflag = 1;
			break;
		case 'x':
			xflag = 1;
			break;
		case '0':
			zflag = 1;
			break;
		case '?':
		default:
			usage();
	}
	argc -= optind;
	argv += optind;

	if (!Iflag && Rflag)
		usage();
	if (Iflag && !Rflag)
		Rflag = 5;
	if (xflag && !nflag)
		usage();
	if (Iflag || Lflag)
		xflag = 1;
	if (replstr != NULL && *replstr == '\0')
		errx(1, "replstr may not be empty");

	/*
	 * Allocate pointers for the utility name, the utility arguments,
	 * the maximum arguments to be read from stdin and the trailing
	 * NULL.
	 */
	linelen = 1 + argc + nargs + 1;
	if ((av = bxp = calloc(linelen, sizeof(char **))) == NULL)
		err(1, NULL);

	/*
	 * Use the user's name for the utility as argv[0], just like the
	 * shell.  Echo is the default.  Set up pointers for the user's
	 * arguments.
	 */
	if (*argv == NULL)
		cnt = strlen(*bxp++ = _PATH_ECHO);
	else {
		do {
			if (Jflag && strcmp(*argv, replstr) == 0) {
				char **avj;
				jfound = 1;
				argv++;
				for (avj = argv; *avj; avj++)
					cnt += strlen(*avj) + 1;
				break;
			}
			cnt += strlen(*bxp++ = *argv) + 1;
		} while (*++argv != NULL);
	}

	/*
	 * Set up begin/end/traversing pointers into the array.  The -n
	 * count doesn't include the trailing NULL pointer, so the malloc
	 * added in an extra slot.
	 */
	endxp = (xp = bxp) + nargs;

	/*
	 * Allocate buffer space for the arguments read from stdin and the
	 * trailing NULL.  Buffer space is defined as the default or specified
	 * space, minus the length of the utility name and arguments.  Set up
	 * begin/end/traversing pointers into the array.  The -s count does
	 * include the trailing NULL, so the malloc didn't add in an extra
	 * slot.
	 */
	nline -= cnt;
	if (nline <= 0)
		errx(1, "insufficient space for command");

	if ((bbp = malloc((size_t)(nline + 1))) == NULL)
		err(1, NULL);
	ebp = (argp = p = bbp) + nline - 1;
	for (;;)
		parse_input(argc, argv);
}

static void
parse_input(int argc, char *argv[])
{
	int hasblank = 0;
	static int hadblank = 0;
	int ch, foundeof = 0;
	char **avj;

	ch = getchar();
	if (isblank(ch)) {
		/* Quotes escape tabs and spaces. */
		if (insingle || indouble)
			goto addch;
		hasblank = 1;
		if (zflag)
			goto addch;
		goto arg2;
	}

	switch (ch) {
	case EOF:
		/* No arguments since last exec. */
		if (p == bbp) {
			if (runeof)
				prerun(0, av);
			waitchildren(*argv, 1);
			exit(rval);
		}
		goto arg1;
	case '\0':
		if (zflag) {
			/*
			 * Increment 'count', so that nulls will be treated
			 * as end-of-line, as well as end-of-argument.  This
			 * is needed so -0 works properly with -I and -L.
			 */
			count++;
			goto arg2;
		}
		goto addch;
	case '\n':
		if (zflag)
			goto addch;
		hasblank = 1;
		if (hadblank == 0)
			count++;

		/* Quotes do not escape newlines. */
arg1:		if (insingle || indouble)
			errx(1, "unterminated quote");
arg2:
		foundeof = *eofstr != '\0' &&
		    strcmp(argp, eofstr) == 0;

		/* Do not make empty args unless they are quoted */
		if ((argp != p || wasquoted) && !foundeof) {
			*p++ = '\0';
			*xp++ = argp;
			if (Iflag) {
				size_t curlen;

				if (inpline == NULL)
					curlen = 0;
				else {
					/*
					 * If this string is not zero
					 * length, append a space for
					 * separation before the next
					 * argument.
					 */
					if ((curlen = strlen(inpline)))
						strlcat(inpline, " ", inpsize);
				}
				curlen++;
				/*
				 * Allocate enough to hold what we will
				 * be holding in a second, and to append
				 * a space next time through, if we have
				 * to.
				 */
				inpsize = curlen + 2 + strlen(argp);
				inpline = realloc(inpline, inpsize);
				if (inpline == NULL)
					errx(1, "realloc failed");
				if (curlen == 1)
					strlcpy(inpline, argp, inpsize);
				else
					strlcat(inpline, argp, inpsize);
			}
		}

		/*
		 * If max'd out on args or buffer, or reached EOF,
		 * run the command.  If xflag and max'd out on buffer
		 * but not on args, object.  Having reached the limit
		 * of input lines, as specified by -L is the same as
		 * maxing out on arguments.
		 */
		if (xp == endxp || p > ebp || ch == EOF ||
		    (Lflag <= count && xflag) || foundeof) {
			if (xflag && xp != endxp && p > ebp)
				errx(1, "insufficient space for arguments");
			if (jfound) {
				for (avj = argv; *avj; avj++)
					*xp++ = *avj;
			}
			prerun(argc, av);
			if (ch == EOF || foundeof) {
				waitchildren(*argv, 1);
				exit(rval);
			}
			p = bbp;
			xp = bxp;
			count = 0;
		}
		argp = p;
		wasquoted = 0;
		break;
	case '\'':
		if (indouble || zflag)
			goto addch;
		insingle = !insingle;
		wasquoted = 1;
		break;
	case '"':
		if (insingle || zflag)
			goto addch;
		indouble = !indouble;
		wasquoted = 1;
		break;
	case '\\':
		if (zflag)
			goto addch;
		/* Backslash escapes anything, is escaped by quotes. */
		if (!insingle && !indouble && (ch = getchar()) == EOF)
			errx(1, "backslash at EOF");
		/* FALLTHROUGH */
	default:
addch:		if (p < ebp) {
			*p++ = ch;
			break;
		}

		/* If only one argument, not enough buffer space. */
		if (bxp == xp)
			errx(1, "insufficient space for argument");
		/* Didn't hit argument limit, so if xflag object. */
		if (xflag)
			errx(1, "insufficient space for arguments");

		if (jfound) {
			for (avj = argv; *avj; avj++)
				*xp++ = *avj;
		}
		prerun(argc, av);
		xp = bxp;
		cnt = ebp - argp;
		memmove(bbp, argp, (size_t)cnt);
		p = (argp = bbp) + cnt;
		*p++ = ch;
		break;
	}
	hadblank = hasblank;
}

/*
 * Do things necessary before run()'ing, such as -I substitution,
 * and then call run().
 */
static void
prerun(int argc, char *argv[])
{
	char **tmp, **tmp2, **avj;
	int repls;

	repls = Rflag;
	runeof = 0;

	if (argc == 0 || repls == 0) {
		*xp = NULL;
		run(argv);
		return;
	}

	avj = argv;

	/*
	 * Allocate memory to hold the argument list, and
	 * a NULL at the tail.
	 */
	tmp = calloc(argc + 1, sizeof(char**));
	if (tmp == NULL)
		err(1, NULL);
	tmp2 = tmp;

	/*
	 * Save the first argument and iterate over it, we
	 * cannot do strnsubst() to it.
	 */
	if ((*tmp++ = strdup(*avj++)) == NULL)
		err(1, NULL);

	/*
	 * For each argument to utility, if we have not used up
	 * the number of replacements we are allowed to do, and
	 * if the argument contains at least one occurrence of
	 * replstr, call strnsubst(), else just save the string.
	 * Iterations over elements of avj and tmp are done
	 * where appropriate.
	 */
	while (--argc) {
		*tmp = *avj++;
		if (repls && strstr(*tmp, replstr) != NULL) {
			strnsubst(tmp++, replstr, inpline, (size_t)255);
			if (repls > 0)
				repls--;
		} else {
			if ((*tmp = strdup(*tmp)) == NULL)
				err(1, NULL);
			tmp++;
		}
	}

	/*
	 * Run it.
	 */
	*tmp = NULL;
	run(tmp2);

	/*
	 * Walk from the tail to the head, free along the way.
	 */
	for (; tmp2 != tmp; tmp--)
		free(*tmp);
	/*
	 * Now free the list itself.
	 */
	free(tmp2);

	/*
	 * Free the input line buffer, if we have one.
	 */
	free(inpline);
	inpline = NULL;
}

static void
run(char **argv)
{
	pid_t pid;
	int fd;
	char **avec;

	/*
	 * If the user wants to be notified of each command before it is
	 * executed, notify them.  If they want the notification to be
	 * followed by a prompt, then prompt them.
	 */
	if (tflag || pflag) {
		(void)fprintf(stderr, "%s", *argv);
		for (avec = argv + 1; *avec != NULL; ++avec)
			(void)fprintf(stderr, " %s", *avec);
		/*
		 * If the user has asked to be prompted, do so.
		 */
		if (pflag)
			/*
			 * If they asked not to exec, return without execution
			 * but if they asked to, go to the execution.  If we
			 * could not open their tty, break the switch and drop
			 * back to -t behaviour.
			 */
			switch (prompt()) {
			case 0:
				return;
			case 1:
				goto exec;
			case 2:
				break;
			}
		(void)fprintf(stderr, "\n");
		(void)fflush(stderr);
	}
exec:
	switch (pid = vfork()) {
	case -1:
		err(1, "vfork");
	case 0:
		if (oflag) {
			if ((fd = open(_PATH_TTY, O_RDONLY)) == -1) {
				warn("can't open /dev/tty");
				_exit(1);
			}
		} else {
			fd = open(_PATH_DEVNULL, O_RDONLY);
		}
		if (fd > STDIN_FILENO) {
			if (dup2(fd, STDIN_FILENO) != 0) {
				warn("can't dup2 to stdin");
				_exit(1);
			}
			close(fd);
		}
		execvp(argv[0], argv);
		warn("%s", argv[0]);
		_exit(errno == ENOENT ? 127 : 126);
	}
	curprocs++;
	waitchildren(*argv, 0);
}

static void
waitchildren(const char *name, int waitall)
{
	pid_t pid;
	int status;

	while ((pid = waitpid(-1, &status, !waitall && curprocs < maxprocs ?
	    WNOHANG : 0)) > 0) {
		curprocs--;
		/*
		 * According to POSIX, we have to exit if the utility exits
		 * with a 255 status, or is interrupted by a signal.
		 * We are allowed to return any exit status between 1 and
		 * 125 in these cases, but we'll use 124 and 125, the same
		 * values used by GNU xargs.
		 */
		if (WIFEXITED(status)) {
			if (WEXITSTATUS(status) == 255) {
				warnx("%s exited with status 255", name);
				exit(124);
			} else if (WEXITSTATUS(status) == 127 ||
			    WEXITSTATUS(status) == 126) {
				exit(WEXITSTATUS(status));
			} else if (WEXITSTATUS(status) != 0) {
				rval = 123;
			}
		} else if (WIFSIGNALED(status)) {
			if (WTERMSIG(status) != SIGPIPE) {
				if (WTERMSIG(status) < NSIG)
					warnx("%s terminated by SIG%s", name,
					    sys_signame[WTERMSIG(status)]);
				else
					warnx("%s terminated by signal %d",
					    name, WTERMSIG(status));
			}
			exit(125);
		}
	}
	if (pid == -1 && errno != ECHILD)
		err(1, "waitpid");
}

/*
 * Prompt the user about running a command.
 */
static int
prompt(void)
{
	regex_t cre;
	size_t rsize;
	int match;
	char *response;
	FILE *ttyfp;

	if ((ttyfp = fopen(_PATH_TTY, "r")) == NULL)
		return (2);	/* Indicate that the TTY failed to open. */
	(void)fprintf(stderr, "?...");
	(void)fflush(stderr);
	if ((response = fgetln(ttyfp, &rsize)) == NULL ||
	    regcomp(&cre, nl_langinfo(YESEXPR), REG_BASIC) != 0) {
		(void)fclose(ttyfp);
		return (0);
	}
	response[rsize - 1] = '\0';
	match = regexec(&cre, response, 0, NULL, 0);
	(void)fclose(ttyfp);
	regfree(&cre);
	return (match == 0);
}

static void
usage(void)
{
	fprintf(stderr,
"usage: xargs [-0oprt] [-E eofstr] [-I replstr [-R replacements]] [-J replstr]\n"
"             [-L number] [-n number [-x]] [-P maxprocs] [-s size]\n"
"             [utility [argument ...]]\n");
	exit(1);
}
