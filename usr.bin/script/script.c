/*	$OpenBSD: script.c,v 1.25 2009/10/27 23:59:43 deraadt Exp $	*/
/*	$NetBSD: script.c,v 1.3 1994/12/21 08:55:43 jtc Exp $	*/

/*
 * Copyright (c) 2001 Theo de Raadt
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
volatile sig_atomic_t child;
pid_t	subchild;
char	*fname;

volatile sig_atomic_t dead;
volatile sig_atomic_t sigdeadstatus;
volatile sig_atomic_t flush;

struct	termios tt;

__dead void done(int);
void dooutput(void);
void doshell(void);
void fail(void);
void finish(int);
void scriptflush(int);
void handlesigwinch(int);

int
main(int argc, char *argv[])
{
	extern char *__progname;
	struct sigaction sa;
	struct termios rtt;
	struct winsize win;
	char ibuf[BUFSIZ];
	ssize_t cc, off;
	int aflg, ch;

	aflg = 0;
	while ((ch = getopt(argc, argv, "a")) != -1)
		switch(ch) {
		case 'a':
			aflg = 1;
			break;
		default:
			fprintf(stderr, "usage: %s [-a] [file]\n", __progname);
			exit(1);
		}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		fname = argv[0];
	else
		fname = "typescript";

	if ((fscript = fopen(fname, aflg ? "a" : "w")) == NULL)
		err(1, "%s", fname);

	(void)tcgetattr(STDIN_FILENO, &tt);
	(void)ioctl(STDIN_FILENO, TIOCGWINSZ, &win);
	if (openpty(&master, &slave, NULL, &tt, &win) == -1)
		err(1, "openpty");

	(void)printf("Script started, output file is %s\n", fname);
	rtt = tt;
	cfmakeraw(&rtt);
	rtt.c_lflag &= ~ECHO;
	(void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &rtt);

	bzero(&sa, sizeof sa);
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = finish;
	(void)sigaction(SIGCHLD, &sa, NULL);

	sa.sa_handler = handlesigwinch;
	sa.sa_flags = SA_RESTART;
	(void)sigaction(SIGWINCH, &sa, NULL);

	child = fork();
	if (child < 0) {
		warn("fork");
		fail();
	}
	if (child == 0) {
		subchild = child = fork();
		if (child < 0) {
			warn("fork");
			fail();
		}
		if (child)
			dooutput();
		else
			doshell();
	}

	(void)fclose(fscript);
	while (1) {
		if (dead)
			break;
		cc = read(STDIN_FILENO, ibuf, BUFSIZ);
		if (cc == -1 && errno == EINTR)
			continue;
		if (cc <= 0)
			break;
		for (off = 0; off < cc; ) {
			ssize_t n = write(master, ibuf + off, cc - off);
			if (n == -1 && errno != EAGAIN)
				break;
			if (n == 0)
				break;	/* skip writing */
			if (n > 0)
				off += n;
		}
	}
	done(sigdeadstatus);
}

/* ARGSUSED */
void
finish(int signo)
{
	int save_errno = errno;
	int status, e = 1;
	pid_t pid;

	while ((pid = wait3(&status, WNOHANG, 0)) > 0) {
		if (pid == (pid_t)child) {
			if (WIFEXITED(status))
				e = WEXITSTATUS(status);
		}
	}
	dead = 1;
	sigdeadstatus = e;
	errno = save_errno;
}

/* ARGSUSED */
void
handlesigwinch(int signo)
{
	int save_errno = errno;
	struct winsize win;
	pid_t pgrp;

	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &win) != -1) {
		ioctl(slave, TIOCSWINSZ, &win);
		if (ioctl(slave, TIOCGPGRP, &pgrp) != -1)
			killpg(pgrp, SIGWINCH);
	}
	errno = save_errno;
}

void
dooutput(void)
{
	struct sigaction sa;
	struct itimerval value;
	sigset_t blkalrm;
	char obuf[BUFSIZ];
	time_t tvec;
	ssize_t outcc = 0, cc, off;

	(void)close(STDIN_FILENO);
	tvec = time(NULL);
	(void)fprintf(fscript, "Script started on %s", ctime(&tvec));

	sigemptyset(&blkalrm);
	sigaddset(&blkalrm, SIGALRM);
	bzero(&sa, sizeof sa);
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = scriptflush;
	(void)sigaction(SIGALRM, &sa, NULL);

	value.it_interval.tv_sec = SECSPERMIN / 2;
	value.it_interval.tv_usec = 0;
	value.it_value = value.it_interval;
	(void)setitimer(ITIMER_REAL, &value, NULL);
	for (;;) {
		if (flush) {
			if (outcc) {
				(void)fflush(fscript);
				outcc = 0;
			}
			flush = 0;
		}
		cc = read(master, obuf, sizeof (obuf));
		if (cc == -1 && errno == EINTR)
			continue;
		if (cc <= 0)
			break;
		sigprocmask(SIG_BLOCK, &blkalrm, NULL);
		for (off = 0; off < cc; ) {
			ssize_t n = write(STDOUT_FILENO, obuf + off, cc - off);
			if (n == -1 && errno != EAGAIN)
				break;
			if (n == 0)
				break;	/* skip writing */
			if (n > 0)
				off += n;
		}
		(void)fwrite(obuf, 1, cc, fscript);
		outcc += cc;
		sigprocmask(SIG_UNBLOCK, &blkalrm, NULL);
	}
	done(0);
}

/* ARGSUSED */
void
scriptflush(int signo)
{
	flush = 1;
}

void
doshell(void)
{
	char *shell;

	shell = getenv("SHELL");
	if (shell == NULL)
		shell = _PATH_BSHELL;

	(void)close(master);
	(void)fclose(fscript);
	login_tty(slave);
	execl(shell, shell, "-i", (char *)NULL);
	warn("%s", shell);
	fail();
}

void
fail(void)
{

	(void)kill(0, SIGTERM);
	done(1);
}

void
done(int eval)
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
	exit(eval);
}
