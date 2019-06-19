/*	$OpenBSD: apply.c,v 1.29 2018/04/01 17:45:05 bluhm Exp $	*/
/*	$NetBSD: apply.c,v 1.3 1995/03/25 03:38:23 glass Exp $	*/

/*-
 * Copyright (c) 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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

#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ISMAGICNO(p) \
	    (p)[0] == magic && isdigit((unsigned char)(p)[1]) && (p)[1] != '0'

__dead	void	usage(void);
static	int	mysystem(const char *);

char	*str;
size_t	 sz;

void
stradd(char *p)
{
	size_t n;

	n = strlen(p);
	if (str == NULL) {
		sz = (n / 1024 + 1) * 1024;
		if ((str = malloc(sz)) == NULL)
			err(1, "malloc");
		*str = '\0';
	} else if (sz - strlen(str) <= n) {
		sz += (n / 1024 + 1) * 1024;
		if ((str = realloc(str, sz)) == NULL)
			err(1, "realloc");
	}
	strlcat(str, p, sz);
}

void
strset(char *p)
{
	if (str != NULL)
		str[0] = '\0';
	stradd(p);
}

int
main(int argc, char *argv[])
{
	int ch, debug, i, magic, n, nargs, rval;
	char buf[4], *cmd, *p;

	if (pledge("stdio proc exec", NULL) == -1)
		err(1, "pledge");

	debug = 0;
	magic = '%';		/* Default magic char is `%'. */
	nargs = -1;
	while ((ch = getopt(argc, argv, "a:d0123456789")) != -1)
		switch (ch) {
		case 'a':
			if (optarg[0] == '\0' || optarg[1] != '\0')
				errx(1,
				    "illegal magic character specification.");
			magic = optarg[0];
			break;
		case 'd':
			debug = 1;
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			if (nargs != -1)
				errx(1,
				    "only one -# argument may be specified.");
			nargs = ch - '0';
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc < 2)
		usage();

	/*
	 * The command to run is argv[0], and the args are argv[1..].
	 * Look for %digit references in the command, remembering the
	 * largest one.
	 */
	for (n = 0, p = argv[0]; *p != '\0'; ++p)
		if (ISMAGICNO(p)) {
			++p;
			if (p[0] - '0' > n)
				n = p[0] - '0';
		}

	/*
	 * If there were any %digit references, then use those, otherwise
	 * build a new command string with sufficient %digit references at
	 * the end to consume (nargs) arguments each time round the loop.
	 * Allocate enough space to hold the maximum command.
	 */
	strset(argv[0]);
	if (n == 0) {
		/* If nargs not set, default to a single argument. */
		if (nargs == -1)
			nargs = 1;

		for (i = 1; i <= nargs; i++) {
			snprintf(buf, sizeof(buf), " %c%d", magic, i);
			stradd(buf);
		}

		/*
		 * If nargs set to the special value 0, eat a single
		 * argument for each command execution.
		 */
		if (nargs == 0)
			nargs = 1;
	} else
		nargs = n;
	if ((cmd = strdup(str)) == NULL)
		err(1, "strdup");

	/*
	 * (argc) and (argv) are still offset by one to make it simpler to
	 * expand %digit references.  At the end of the loop check for (argc)
	 * equals 1 means that all the (argv) has been consumed.
	 */
	for (rval = 0; argc > nargs; argc -= nargs, argv += nargs) {
		strset("exec ");

		/* Expand command argv references. */
		for (p = cmd; *p != '\0'; ++p)
			if (ISMAGICNO(p))
				stradd(argv[*(++p) - '0']);
			else {
				strlcpy(buf, p, 2);
				stradd(buf);
			}

		/* Run the command. */
		if (debug)
			(void)printf("%s\n", str);
		else if (mysystem(str))
			rval = 1;
	}

	if (argc != 1)
		errx(1, "expecting additional argument%s after \"%s\"",
		    (nargs - argc) ? "s" : "", argv[argc - 1]);
	exit(rval);
}

/*
 * mysystem --
 * 	Private version of system(3).  Use the user's SHELL environment
 *	variable as the shell to execute.
 */
static int
mysystem(const char *command)
{
	static const char *name, *shell;
	pid_t pid;
	int pstat;
	sigset_t mask, omask;
	sig_t intsave, quitsave;

	if (shell == NULL) {
		if ((shell = getenv("SHELL")) == NULL)
			shell = _PATH_BSHELL;
		if ((name = strrchr(shell, '/')) == NULL)
			name = shell;
		else
			++name;
	}
	if (!command)		/* just checking... */
		return(1);

	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &mask, &omask);
	switch(pid = fork()) {
	case -1:			/* error */
		err(1, "fork");
	case 0:				/* child */
		sigprocmask(SIG_SETMASK, &omask, NULL);
		execl(shell, name, "-c", command, (char *)NULL);
		err(1, "%s", shell);
	}
	intsave = signal(SIGINT, SIG_IGN);
	quitsave = signal(SIGQUIT, SIG_IGN);
	pid = waitpid(pid, &pstat, 0);
	sigprocmask(SIG_SETMASK, &omask, NULL);
	(void)signal(SIGINT, intsave);
	(void)signal(SIGQUIT, quitsave);
	return(pid == -1 ? -1 : pstat);
}

__dead void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: apply [-#] [-d] [-a magic] command argument ...\n");
	exit(1);
}
