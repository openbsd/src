/*	$OpenBSD: uucpd.c,v 1.29 2004/06/02 02:21:15 brad Exp $	*/

/*
 * Copyright (c) 1985 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Adams.
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
char copyright[] =
"@(#) Copyright (c) 1985 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)uucpd.c	5.10 (Berkeley) 2/26/91";*/
static char rcsid[] = "$OpenBSD: uucpd.c,v 1.29 2004/06/02 02:21:15 brad Exp $";
#endif /* not lint */

/*
 * 4.2BSD TCP/IP server for uucico
 * uucico's TCP channel causes this server to be run at the remote end.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <pwd.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <login_cap.h>
#include <utmp.h>
#include <fcntl.h>
#include "pathnames.h"

void doit(struct sockaddr *);
int readline(char *, int n);
void dologout(void);
void dologin(struct passwd *, struct sockaddr *);

struct	sockaddr_storage hisctladdr;
socklen_t hisaddrlen = sizeof hisctladdr;
pid_t	mypid;

char Username[64], Loginname[64];
char *nenv[] = {
	Username,
	Loginname,
	NULL,
};

extern char **environ;

char utline[UT_LINESIZE+1];

int
main(int argc, char *argv[])
{
#ifndef BSDINETD
	int s, tcp_socket;
	struct servent *sp;
#endif /* !BSDINETD */
	pid_t childpid;

	environ = nenv;
#ifdef BSDINETD
	close(1); close(2);
	dup(0); dup(0);
	hisaddrlen = sizeof (hisctladdr);
	if (getpeername(0, (struct sockaddr *)&hisctladdr, &hisaddrlen) < 0) {
		fprintf(stderr, "%s: ", argv[0]);
		perror("getpeername");
		_exit(1);
	}
	if ((childpid = fork()) == 0)
		doit((struct sockaddr *)&hisctladdr);
	snprintf(utline, sizeof(utline), "uucp%.4ld", (long)childpid);
	dologout();
	exit(1);
#else /* !BSDINETD */
	sp = getservbyname("uucp", "tcp");
	if (sp == NULL){
		perror("uucpd: getservbyname");
		exit(1);
	}
	if (fork())
		exit(0);
	snprintf(utline, sizeof(utline), "uucp%.4ld", (long)childpid);

	if ((s = open(_PATH_TTY, 2)) >= 0){
		ioctl(s, TIOCNOTTY, (char *)0);
		close(s);
	}

	bzero((char *)&myctladdr, sizeof (myctladdr));
	myctladdr.sin_len = sizeof(struct sockaddr_in);
	myctladdr.sin_family = AF_INET;
	myctladdr.sin_port = sp->s_port;
	tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (tcp_socket < 0) {
		perror("uucpd: socket");
		exit(1);
	}
	if (bind(tcp_socket, (char *)&myctladdr, sizeof (myctladdr)) < 0) {
		perror("uucpd: bind");
		exit(1);
	}
	listen(tcp_socket, 3);	/* at most 3 simultaneuos uucp connections */
	signal(SIGCHLD, dologout);

	for(;;) {
		s = accept(tcp_socket, &hisctladdr, &hisaddrlen);
		if (s < 0){
			if (errno == EINTR)
				continue;
			perror("uucpd: accept");
			exit(1);
		}
		if (fork() == 0) {
			close(0); close(1); close(2);
			dup(s); dup(s); dup(s);
			close(tcp_socket); close(s);
			doit(&hisctladdr);
			exit(1);
		}
		close(s);
	}
#endif	/* !BSDINETD */
}

void
doit(struct sockaddr *sa)
{
	char user[64], passwd[64];
	char *xpasswd;
	struct passwd *pw;

	alarm(60);
	do {
		printf("login: ");
		fflush(stdout);
		if (readline(user, sizeof user) < 0) {
			fprintf(stderr, "user read\n");
			return;
		}
	} while (user[0] == '\0');
	user[MAXLOGNAME] = '\0';

	pw = getpwnam(user);
	if (pw == NULL) {
		printf("Password: ");
		fflush(stdout);
		if (readline(passwd, sizeof passwd) < 0) {
			fprintf(stderr, "passwd read\n");
			return;
		}
		fprintf(stderr, "Login incorrect.");
		return;
	}
	if (pw->pw_passwd && *pw->pw_passwd != '\0') {
		printf("Password: ");
		fflush(stdout);
		if (readline(passwd, sizeof passwd) < 0) {
			fprintf(stderr, "passwd read\n");
			return;
		}
		xpasswd = crypt(passwd, pw->pw_passwd);
		if (strcmp(xpasswd, pw->pw_passwd)) {
			fprintf(stderr, "Login incorrect.");
			return;
		}
	}
	if (strcmp(pw->pw_shell, _PATH_UUCICO)) {
		fprintf(stderr, "Login incorrect.\n");
		return;
	}
	alarm(0);
	(void) snprintf(Username, sizeof(Username), "USER=%s", user);
	(void) snprintf(Loginname, sizeof(Loginname), "LOGNAME=%s", user);
	dologin(pw, sa);
	if (setusercontext(0, pw, pw->pw_uid, LOGIN_SETALL) != 0) {
		perror("unable to set user context");
		return;
	}
	chdir(pw->pw_dir);
	execl(_PATH_UUCICO, "uucico", (char *)NULL);
	perror("uucico server: execl");
}

int
readline(char *p, int n)
{
	char c;

	while (n-- > 0) {
		if (read(0, &c, 1) <= 0)
			return(-1);
		c &= 0177;
		if (c == '\r') {
			*p = '\0';
			return(0);
		}
		if (c != '\n')
			*p++ = c;
	}
	return(-1);
}

#define	SCPYN(a, b)	strncpy(a, b, sizeof (a))

struct	utmp utmp;

void
dologout(void)
{
	int save_errno = errno;
	int status, wtmp;
	pid_t pid;

#ifdef BSDINETD
	while ((pid=wait(&status)) > 0) {
#else  /* !BSDINETD */
	while ((pid=wait3(&status, WNOHANG, 0)) > 0) {
#endif /* !BSDINETD */
		wtmp = open(_PATH_WTMP, O_WRONLY|O_APPEND);
		if (wtmp >= 0) {
			SCPYN(utmp.ut_line, utline);
			SCPYN(utmp.ut_name, "");
			SCPYN(utmp.ut_host, "");
			(void) time(&utmp.ut_time);
			(void) write(wtmp, (char *)&utmp, sizeof (utmp));
			(void) close(wtmp);
		}
	}
	errno = save_errno;
}

/*
 * Record login in wtmp file.
 */
void
dologin(struct passwd *pw, struct sockaddr *sa)
{
	char line[32];
	char hbuf[NI_MAXHOST];
	int wtmp, f;

	if (getnameinfo(sa, sa->sa_len, hbuf, sizeof(hbuf), NULL, 0, 0))
		(void)strlcpy(hbuf, "?", sizeof(hbuf));
	wtmp = open(_PATH_WTMP, O_WRONLY|O_APPEND);
	if (wtmp >= 0) {
		/* hack, but must be unique and no tty line */
		(void) snprintf(line, sizeof line, "uucp%.4ld", (long)getpid());
		SCPYN(utmp.ut_line, line);
		SCPYN(utmp.ut_name, pw->pw_name);
		SCPYN(utmp.ut_host, hbuf);
		time(&utmp.ut_time);
		(void) write(wtmp, (char *)&utmp, sizeof (utmp));
		(void) close(wtmp);
	}
	if ((f = open(_PATH_LASTLOG, O_RDWR)) >= 0) {
		struct lastlog ll;

		time(&ll.ll_time);
		lseek(f, pw->pw_uid * sizeof(struct lastlog), 0);
		SCPYN(ll.ll_line, hbuf);
		SCPYN(ll.ll_host, hbuf);
		(void) write(f, (char *) &ll, sizeof ll);
		(void) close(f);
	}
}
