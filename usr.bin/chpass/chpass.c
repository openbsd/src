/*	$OpenBSD: chpass.c,v 1.40 2014/10/26 20:38:13 guenther Exp $	*/
/*	$NetBSD: chpass.c,v 1.8 1996/05/15 21:50:43 jtc Exp $	*/

/*-
 * Copyright (c) 1988, 1993, 1994
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

#include <sys/param.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "chpass.h"

extern char *__progname;

enum { NEWSH, LOADENTRY, EDITENTRY } op;
uid_t uid;
#ifdef	YP
int	use_yp;
int	force_yp = 0;
#endif

void	baduser(void);
void	kbintr(int);
void	usage(void);

int
main(int argc, char *argv[])
{
	struct passwd *pw = NULL, *opw = NULL, lpw;
	int i, ch, pfd, tfd, dfd;
	char *tz, *arg = NULL;
	sigset_t fullset;

#ifdef	YP
	use_yp = _yp_check(NULL);
#endif
	/* We need to use the system timezone for date conversions. */
	if ((tz = getenv("TZ")) != NULL) {
	    unsetenv("TZ");
	    tzset();
	    setenv("TZ", tz, 1);
	}

	op = EDITENTRY;
	while ((ch = getopt(argc, argv, "a:s:ly")) != -1)
		switch(ch) {
		case 'a':
			op = LOADENTRY;
			arg = optarg;
			break;
		case 's':
			op = NEWSH;
			arg = optarg;
			break;
#ifdef	YP
		case 'l':
			use_yp = 0;
			break;
		case 'y':
			if (!use_yp) {
				warnx("YP not in use.");
				usage();
			}
			force_yp = 1;
			break;
#endif
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

#ifdef	YP
	if (op == LOADENTRY && use_yp)
		errx(1, "cannot load using YP, use -l to load local.");
#endif
	uid = getuid();

	if (op == EDITENTRY || op == NEWSH)
		switch(argc) {
		case 0:
			pw = getpwuid(uid);
#ifdef	YP
			if (pw && !force_yp)
				use_yp = 0;
			else if (use_yp)
				pw = ypgetpwuid(uid);
#endif	/* YP */
			if (!pw)
				errx(1, "unknown user: uid %u", uid);
			break;
		case 1:
			pw = getpwnam(*argv);
#ifdef	YP
			if (pw && !force_yp)
				use_yp = 0;
			else if (use_yp)
				pw = ypgetpwnam(*argv);
#endif	/* YP */
			if (!pw)
				errx(1, "unknown user: %s", *argv);
			if (uid && uid != pw->pw_uid)
				baduser();
			break;
		default:
			usage();
		}

	if (op == LOADENTRY) {
		if (argc != 0)
			errx(1, "option -a does not accept user argument");
		if (uid)
			baduser();
		pw = &lpw;
		if (!pw_scan(arg, pw, NULL))
			exit(1);
		opw = getpwnam(pw->pw_name);
	}
	if (opw == NULL && (opw = pw_dup(pw)) == NULL)
		err(1, NULL);

	/* Edit the user passwd information if requested. */
	if (op == EDITENTRY) {
		char tempname[] = _PATH_VARTMP "pw.XXXXXXXXXX";
		int edit_status;

		if ((pw = pw_dup(pw)) == NULL)
			pw_error(NULL, 1, 1);
		dfd = mkostemp(tempname, O_CLOEXEC);
		if (dfd == -1)
			pw_error(tempname, 1, 1);
		display(tempname, dfd, pw);
		edit_status = edit(tempname, pw);
		close(dfd);
		unlink(tempname);

		switch (edit_status) {
		case EDIT_OK:
			break;
		case EDIT_NOCHANGE:
			pw_error(NULL, 0, 0);
			break;
		case EDIT_ERROR:
		default:
			pw_error(tempname, 1, 1);
			break;
		}
	}

	if (op == NEWSH) {
		/* protect p_shell -- it thinks NULL is /bin/sh */
		if (!arg[0])
			usage();
		if (p_shell(arg, pw, NULL))
			pw_error(NULL, 0, 1);
	}

	/* Drop user's real uid and block all signals to avoid a DoS. */
	setuid(0);
	sigfillset(&fullset);
	sigdelset(&fullset, SIGINT);
	sigprocmask(SIG_BLOCK, &fullset, NULL);

	/* Get the passwd lock file and open the passwd file for reading. */
	pw_init();
	for (i = 1; (tfd = pw_lock(0)) == -1; i++) {
		if (i == 4)
			(void)fputs("Attempting to lock password file, "
			    "please wait or press ^C to abort", stderr);
		(void)signal(SIGINT, kbintr);
		if (i % 16 == 0)
			fputc('.', stderr);
		usleep(250000);
		(void)signal(SIGINT, SIG_IGN);
	}
	if (i >= 4)
		fputc('\n', stderr);
	pfd = open(_PATH_MASTERPASSWD, O_RDONLY|O_CLOEXEC, 0);
	if (pfd == -1)
		pw_error(_PATH_MASTERPASSWD, 1, 1);

#ifdef	YP
	if (use_yp) {
		if (pw_yp(pw, uid))
			pw_error(NULL, 0, 1);
		else {
			pw_abort();
			exit(0);
		}
	} else
#endif	/* YP */
	{
		/* Copy the passwd file to the lock file, updating pw. */
		pw_copy(pfd, tfd, pw, opw);

		/* If username changed we need to rebuild the entire db. */
		arg = !strcmp(opw->pw_name, pw->pw_name) ? pw->pw_name : NULL;

		/* Now finish the passwd file update. */
		if (pw_mkdb(arg, 0) == -1)
			pw_error(NULL, 0, 1);
	}

	exit(0);
}

void
baduser(void)
{

	errx(1, "%s", strerror(EACCES));
}

/* ARGSUSED */
void
kbintr(int signo)
{
	struct iovec iv[5];

	iv[0].iov_base = "\n";
	iv[0].iov_len = 1;
	iv[1].iov_base = __progname;
	iv[1].iov_len = strlen(__progname);
	iv[2].iov_base = ": ";
	iv[2].iov_len = 2;
	iv[3].iov_base = _PATH_MASTERPASSWD;
	iv[3].iov_len = sizeof(_PATH_MASTERPASSWD) - 1;
	iv[4].iov_base = " unchanged\n";
	iv[4].iov_len = 11;
	writev(STDERR_FILENO, iv, 5);

	_exit(1);
}

void
usage(void)
{

#ifdef	YP
	(void)fprintf(stderr,
	    "usage: %s [-l%s] [-s newshell] [user]\n",
	    __progname, use_yp ? "y" : "");
	(void)fprintf(stderr,
	    "       %s [-l] -a list\n", __progname);
#else
	(void)fprintf(stderr, "usage: %s [-s newshell] [user]\n", __progname);
	(void)fprintf(stderr, "       %s -a list\n", __progname);
#endif
	exit(1);
}
