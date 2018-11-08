/*	$OpenBSD: local_passwd.c,v 1.55 2018/11/08 15:41:41 mestre Exp $	*/

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
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <util.h>
#include <login_cap.h>
#include <readpassphrase.h>

#define UNCHANGED_MSG	"Password unchanged."

static uid_t uid;
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

	if (!(pw = getpwnam_shadow(uname))) {
		warnx("unknown user %s.", uname);
		return(1);
	}

	if (unveil(_PATH_MASTERPASSWD_LOCK, "wc") == -1)
		err(1, "unveil");
	if (unveil(_PATH_MASTERPASSWD, "r") == -1)
		err(1, "unveil");
	if (unveil(_PATH_LOGIN_CONF, "r") == -1)
		err(1, "unveil");
	if (unveil(_PATH_BSHELL, "x") == -1)
		err(1, "unveil");
	if (unveil(_PATH_PWD_MKDB, "x") == -1)
		err(1, "unveil");
	if (pledge("stdio rpath wpath cpath getpw tty id proc exec", NULL) == -1)
		err(1, "pledge");

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

	if (pledge("stdio rpath wpath cpath getpw id proc exec", NULL) == -1)
		err(1, "pledge");

	/* Reset password change time based on login.conf. */
	period = (time_t)login_getcaptime(lc, "passwordtime", 0, 0);
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

	if (pledge("stdio rpath wpath cpath proc exec", NULL) == -1)
		err(1, "pledge");

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
	static char hash[_PASSWORD_LEN];
	char newpass[1024];
	char *p, *pref;
	int tries, pwd_tries;
	sig_t saveint, savequit;

	saveint = signal(SIGINT, kbintr);
	savequit = signal(SIGQUIT, kbintr);

	if (!authenticated) {
		(void)printf("Changing password for %s.\n", pw->pw_name);
		if (uid != 0 && pw->pw_passwd[0] != '\0') {
			char oldpass[1024];

			p = readpassphrase("Old password:", oldpass,
			    sizeof(oldpass), RPP_ECHO_OFF);
			if (p == NULL || *p == '\0') {
				(void)printf("%s\n", UNCHANGED_MSG);
				pw_abort();
				exit(p == NULL ? 1 : 0);
			}
			if (crypt_checkpass(p, pw->pw_passwd) != 0) {
				errno = EACCES;
				explicit_bzero(oldpass, sizeof(oldpass));
				pw_error(NULL, 1, 1);
			}
			explicit_bzero(oldpass, sizeof(oldpass));
		}
	}

	pwd_tries = pwd_gettries(lc);

	for (newpass[0] = '\0', tries = 0;;) {
		char repeat[1024];

		p = readpassphrase("New password:", newpass, sizeof(newpass),
		    RPP_ECHO_OFF);
		if (p == NULL || *p == '\0') {
			(void)printf("%s\n", UNCHANGED_MSG);
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
		p = readpassphrase("Retype new password:", repeat, sizeof(repeat),
		    RPP_ECHO_OFF);
		if (p != NULL && strcmp(newpass, p) == 0) {
			explicit_bzero(repeat, sizeof(repeat));
			break;
		}
		(void)printf("Mismatch; try again, EOF to quit.\n");
		explicit_bzero(repeat, sizeof(repeat));
		explicit_bzero(newpass, sizeof(newpass));
	}

	(void)signal(SIGINT, saveint);
	(void)signal(SIGQUIT, savequit);

	pref = login_getcapstr(lc, "localcipher", NULL, NULL);
	if (crypt_newhash(newpass, pref, hash, sizeof(hash)) != 0) {
		(void)printf("Couldn't generate hash.\n");
		explicit_bzero(newpass, sizeof(newpass));
		pw_error(NULL, 0, 0);
	}
	explicit_bzero(newpass, sizeof(newpass));
	free(pref);
	return hash;
}

void
kbintr(int signo)
{
	dprintf(STDOUT_FILENO, "\n%s\n", UNCHANGED_MSG);
	_exit(0);
}
