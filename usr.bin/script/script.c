/*	$OpenBSD: script.c,v 1.10 1998/12/19 23:52:03 deraadt Exp $	*/
/*	$NetBSD: script.c,v 1.3 1994/12/21 08:55:43 jtc Exp $	*/

/*
 * Copyright (c) 1980, 1992, 1993
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
"@(#) Copyright (c) 1980, 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)script.c	8.1 (Berkeley) 6/6/93";
#endif
static char rcsid[] = "$OpenBSD: script.c,v 1.10 1998/12/19 23:52:03 deraadt Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <tzfile.h>
#include <unistd.h>

#include <util.h>
#include <err.h>

FILE	*fscript;
int	master, slave;
int	child, subchild;
int	outcc;
char	*fname;

struct	termios tt;

__dead	void done __P((void));
	void dooutput __P((void));
	void doshell __P((void));
	void fail __P((void));
	void finish __P((int));
	void scriptflush __P((int));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	register int cc;
	struct termios rtt;
	struct winsize win;
	int aflg, ch;
	char ibuf[BUFSIZ];

	aflg = 0;
	while ((ch = getopt(argc, argv, "a")) != -1)
		switch(ch) {
		case 'a':
			aflg = 1;
			break;
		case '?':
		default:
			(void)fprintf(stderr, "usage: script [-a] [file]\n");
			exit(1);
		}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		fname = argv[0];
	else
		fname = "typescript";

	if ((fscript = fopen(fname, aflg ? "a" : "w")) == NULL)
		err(1, fname);

	(void)tcgetattr(STDIN_FILENO, &tt);
	(void)ioctl(STDIN_FILENO, TIOCGWINSZ, &win);
	if (openpty(&master, &slave, NULL, &tt, &win) == -1)
		err(1, "openpty");

	(void)printf("Script started, output file is %s\n", fname);
	rtt = tt;
	cfmakeraw(&rtt);
	rtt.c_lflag &= ~ECHO;
	(void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &rtt);

	(void)signal(SIGCHLD, finish);
	child = fork();
	if (child < 0) {
		perror("fork");
		fail();
	}
	if (child == 0) {
		subchild = child = fork();
		if (child < 0) {
			perror("fork");
			fail();
		}
		if (child)
			dooutput();
		else
			doshell();
	}

	(void)fclose(fscript);
	while ((cc = read(STDIN_FILENO, ibuf, BUFSIZ)) > 0)
		(void)write(master, ibuf, cc);
	done();
	exit(0);
}

void
finish(signo)
	int signo;
{
	register int die, pid;
	int save_errno = errno;
	int status;

	die = 0;
	while ((pid = wait3(&status, WNOHANG, 0)) > 0)
		if (pid == child)
			die = 1;

	if (die)
		done();
	errno = save_errno;
}

void
dooutput()
{
	struct itimerval value;
	register int cc;
	time_t tvec;
	char obuf[BUFSIZ];

	(void)close(STDIN_FILENO);
	tvec = time(NULL);
	(void)fprintf(fscript, "Script started on %s", ctime(&tvec));

	(void)signal(SIGALRM, scriptflush);
	value.it_interval.tv_sec = SECSPERMIN / 2;
	value.it_interval.tv_usec = 0;
	value.it_value = value.it_interval;
	(void)setitimer(ITIMER_REAL, &value, NULL);
	for (;;) {
		cc = read(master, obuf, sizeof (obuf));
		if (cc <= 0)
			break;
		(void)write(1, obuf, cc);
		(void)fwrite(obuf, 1, cc, fscript);
		outcc += cc;
	}
	done();
}

void
scriptflush(signo)
	int signo;
{
	int save_errno = errno;

	if (outcc) {
		(void)fflush(fscript);
		outcc = 0;
	}
	errno = save_errno;
}

void
doshell()
{
	char *shell;

	shell = getenv("SHELL");
	if (shell == NULL)
		shell = _PATH_BSHELL;

	(void)close(master);
	(void)fclose(fscript);
	login_tty(slave);
	execl(shell, shell, "-i", NULL);
	perror(shell);
	fail();
}

void
fail()
{

	(void)kill(0, SIGTERM);
	done();
}

void
done()
{
	time_t tvec;

	if (subchild) {
		tvec = time(NULL);
		(void)fprintf(fscript,"\nScript done on %s", ctime(&tvec));
		(void)fclose(fscript);
		(void)close(master);
	} else {
		(void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &tt);
		(void)printf("Script done, output file is %s\n", fname);
	}
	exit(0);
}

