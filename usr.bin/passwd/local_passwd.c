/*	$OpenBSD: local_passwd.c,v 1.42 2014/11/11 21:06:24 tedu Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <util.h>
#include <login_cap.h>

#define UNCHANGED_MSG	"Password unchanged.\n"

static uid_t uid;
extern int pwd_gensalt(char *, int, login_cap_t *, char);
extern int pwd_check(login_cap_t *, char *);
extern int pwd_gettries(login_cap_t *);

int local_passwd(char *, int);
char *getnewpasswd(struct passwd *, login_cap_t *, int);
void kbintr(int);

int
local_passwd(char *uname, int authenticated)
{
	struct passwd *pw, *opw;
	login_cap_t *lc;
	sigset_t fullset;
	time_t period;
	int i, pfd, tfd = -1;
	int pwflags = _PASSWORD_OMITV7;

	if (!(pw = getpwnam(uname))) {
#ifdef YP
		extern int use_yp;
		if (!use_yp)
#endif
		warnx("unknown user %s.", uname);
		return(1);
	}
	if ((opw = pw_dup(pw)) == NULL) {
		warn(NULL);
		return(1);
	}
	if ((lc = login_getclass(pw->pw_class)) == NULL) {
		warnx("unable to get login class for user %s.", uname);
		free(opw);
		return(1);
	}

	uid = authenticated ? pw->pw_uid : getuid();
	if (uid && uid != pw->pw_uid) {
		warnx("login/uid mismatch, username argument required.");
		free(opw);
		return(1);
	}

	/* Get the new password. */
	pw->pw_passwd = getnewpasswd(pw, lc, authenticated);

	/* Reset password change time based on login.conf. */
	period = (time_t)login_getcaptime(lc, "passwordtime",
	    (quad_t)0, (quad_t)0);
	if (period > 0) {
		pw->pw_change = time(NULL) + period;
	} else {
		/*
		 * If the pw change time is the same we only need
		 * to update the spwd.db file.
		 */
		if (pw->pw_change != 0)
			pw->pw_change = 0;
		else
			pwflags = _PASSWORD_SECUREONLY;
	}

	/* Drop user's real uid and block all signals to avoid a DoS. */
	setuid(0);
	sigfillset(&fullset);
	sigdelset(&fullset, SIGINT);
	sigprocmask(SIG_BLOCK, &fullset, NULL);

	/* Get a lock on the passwd file and open it. */
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
	pfd = open(_PATH_MASTERPASSWD, O_RDONLY | O_CLOEXEC, 0);
	if (pfd < 0)
		pw_error(_PATH_MASTERPASSWD, 1, 1);

	/* Update master.passwd file and rebuild spwd.db. */
	pw_copy(pfd, tfd, pw, opw);
	free(opw);
	if (pw_mkdb(uname, pwflags) < 0)
		pw_error(NULL, 0, 1);

	return(0);
}

char *
getnewpasswd(struct passwd *pw, login_cap_t *lc, int authenticated)
{
	char *p;
	int tries, pwd_tries;
	char buf[_PASSWORD_LEN+1], salt[_PASSWORD_LEN];
	sig_t saveint, savequit;

	saveint = signal(SIGINT, kbintr);
	savequit = signal(SIGQUIT, kbintr);

	if (!authenticated) {
		(void)printf("Changing local password for %s.\n", pw->pw_name);
		if (uid != 0 && pw->pw_passwd[0] != '\0') {
			p = getpass("Old password:");
			if (p == NULL || *p == '\0') {
				(void)printf(UNCHANGED_MSG);
				pw_abort();
				exit(p == NULL ? 1 : 0);
			}
			if (crypt_checkpass(p, pw->pw_passwd) != 0) {
				errno = EACCES;
				pw_error(NULL, 1, 1);
			}
		}
	}

	pwd_tries = pwd_gettries(lc);

	for (buf[0] = '\0', tries = 0;;) {
		p = getpass("New password:");
		if (p == NULL || *p == '\0') {
			(void)printf(UNCHANGED_MSG);
			pw_abort();
			exit(p == NULL ? 1 : 0);
		}
		if (strcmp(p, "s/key") == 0) {
			printf("That password collides with a system feature. Choose another.\n");
			continue;
		}

		if ((tries++ < pwd_tries || pwd_tries == 0) &&
		    pwd_check(lc, p) == 0)
			continue;
		strlcpy(buf, p, sizeof(buf));
		p = getpass("Retype new password:");
		if (p != NULL && strcmp(buf, p) == 0)
			break;
		(void)printf("Mismatch; try again, EOF to quit.\n");
	}
	if (!pwd_gensalt(salt, _PASSWORD_LEN, lc, 'l')) {
		(void)printf("Couldn't generate salt.\n");
		pw_error(NULL, 0, 0);
	}
	(void)signal(SIGINT, saveint);
	(void)signal(SIGQUIT, savequit);

	return(crypt(buf, salt));
}

/* ARGSUSED */
void
kbintr(int signo)
{
	write(STDOUT_FILENO, "\n", 1);
	write(STDOUT_FILENO, UNCHANGED_MSG, sizeof(UNCHANGED_MSG) - 1);
	_exit(0);
}
