/*
 * Copyright (c) 1983 The Regents of the University of California.
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
char copyright[] =
"@(#) Copyright (c) 1983 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)rexecd.c	5.12 (Berkeley) 2/25/91";*/
static char rcsid[] = "$Id: rexecd.c,v 1.11 1998/07/10 08:06:08 deraadt Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>
#include <netdb.h>
#include <pwd.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <paths.h>

#include <netinet/in.h>
#include <arpa/inet.h>

/*VARARGS1*/
void error __P(());

char	username[20] = "USER=";
char	homedir[sizeof("HOME=")+MAXPATHLEN] = "HOME=";
char	shell[sizeof("SHELL=")+MAXPATHLEN] = "SHELL=";
char	path[sizeof("PATH=") + sizeof(_PATH_DEFPATH)] = "PATH=";
char	*envinit[] = { homedir, shell, path, username, NULL };
char	**environ;
char	*remote;

struct	sockaddr_in asin = { AF_INET };

void doit __P((int, struct sockaddr_in *));
void getstr __P((char *buf, int cnt, char *err));

/*
 * remote execute server:
 *	username\0
 *	password\0
 *	command\0
 *	data
 */
/*ARGSUSED*/
int
main(argc, argv)
	int argc;
	char **argv;
{
	struct sockaddr_in from;
	struct hostent *hp;
	int fromlen;

	openlog(argv[0], LOG_PID, LOG_AUTH);
	fromlen = sizeof (from);
	if (getpeername(0, (struct sockaddr *)&from, &fromlen) < 0) {
		(void)fprintf(stderr,
		    "rexecd: getpeername: %s\n", strerror(errno));
		exit(1);
	}

	hp = gethostbyaddr((char *) &from.sin_addr, sizeof(from.sin_addr),
	    from.sin_family);
	remote = strdup(hp ? hp->h_name : inet_ntoa(from.sin_addr));

	doit(0, &from);
	exit(0);
}

void
doit(f, fromp)
	int f;
	struct sockaddr_in *fromp;
{
	char cmdbuf[NCARGS+1], *cp, *namep;
	char user[16], pass[16];
	struct passwd *pwd;
	int s;
	u_short port;
	int pv[2], pid, cc;
	fd_set ready, readfrom;
	char buf[BUFSIZ], sig;
	int one = 1;
	int maxfd;

	(void) signal(SIGINT, SIG_DFL);
	(void) signal(SIGQUIT, SIG_DFL);
	(void) signal(SIGTERM, SIG_DFL);
#ifdef DEBUG
	{ int t = open(_PATH_TTY, 2);
		if (t >= 0) {
			ioctl(t, TIOCNOTTY, (char *)0);
			(void) close(t);
		}
	}
#endif
	dup2(f, 0);
	dup2(f, 1);
	dup2(f, 2);
	(void) alarm(60);
	port = 0;
	for (;;) {
		char c;
		if (read(f, &c, 1) != 1)
			exit(1);
		if (c == 0)
			break;
		port = port * 10 + c - '0';
	}
	(void) alarm(0);
	getstr(user, sizeof(user), "username");
	getstr(pass, sizeof(pass), "password");
	getstr(cmdbuf, sizeof(cmdbuf), "command");
	setpwent();
	pwd = getpwnam(user);
	if (pwd == NULL) {
		error("Permission denied.\n");
		exit(1);
	}
	endpwent();
	if (*pwd->pw_passwd != '\0') {
		namep = crypt(pass, pwd->pw_passwd);
		if (strcmp(namep, pwd->pw_passwd)) {
			error("Permission denied.\n");
			exit(1);
		}
	}

	syslog(LOG_INFO, "login from %s as %s", remote, user);

	setegid(pwd->pw_gid);
	seteuid(pwd->pw_uid);
	if (chdir(pwd->pw_dir) < 0) {
		error("No remote directory.\n");
		exit(1);
	}
	if (port != 0) {
		if (port < IPPORT_RESERVED) {
			syslog(LOG_ERR, "client stderr port in reserved range");
			exit(1); 
		}
		s = socket(AF_INET, SOCK_STREAM, 0);
		if (s < 0)
			exit(1);
		if (bind(s, (struct sockaddr *)&asin, sizeof (asin)) < 0)
			exit(1);
		(void) alarm(60);
		fromp->sin_port = htons(port);
		if (connect(s, (struct sockaddr *)fromp, sizeof (*fromp)) < 0)
			exit(1);
		(void) alarm(0);
	}
	seteuid(0);
	setegid(0);	/* XXX use a saved gid instead? */

	(void) write(2, "\0", 1);
	if (port) {
		(void) pipe(pv);
		pid = fork();
		if (pid == -1)  {
			error("Try again.\n");
			exit(1);
		}
		if (pid) {
			(void) close(0); (void) close(1); (void) close(2);
			(void) close(f); (void) close(pv[1]);
			FD_ZERO(&readfrom);
			FD_SET(s, &readfrom);
			FD_SET(pv[0], &readfrom);
			maxfd = s;
			if (pv[0] > maxfd)
				maxfd = pv[0];
			ioctl(pv[1], FIONBIO, (char *)&one);
			/* should set s nbio! */
			do {
				ready = readfrom;
				switch (select(maxfd+1, &ready,
				    (fd_set *)NULL, (fd_set *)NULL,
				    (struct timeval *)NULL)) {
				case 0:
				case -1:
					if (errno == EINTR)
						continue;
					exit(0);
				default:
					break;
				}
					
				if (FD_ISSET(s, &ready)) {
					if (read(s, &sig, 1) <= 0)
						FD_CLR(s, &readfrom);
					else
						killpg(pid, sig);
				}
				if (FD_ISSET(pv[0], &ready)) {
					cc = read(pv[0], buf, sizeof (buf));
					if (cc <= 0) {
						shutdown(s, 1+1);
						FD_CLR(pv[0], &readfrom);
					} else
						(void) write(s, buf, cc);
				}
			} while (FD_ISSET(pv[0], &readfrom) ||
			    FD_ISSET(s, &readfrom));
			exit(0);
		}
		setpgrp(0, getpid());
		(void) close(s); (void)close(pv[0]);
		dup2(pv[1], 2);
	}
	if (*pwd->pw_shell == '\0')
		pwd->pw_shell = _PATH_BSHELL;
	if (f > 2)
		(void) close(f);
	if (setlogin(pwd->pw_name) == -1 ||
	    setegid((gid_t)pwd->pw_gid) == -1 ||
	    setgid((gid_t)pwd->pw_gid) == -1 ||
	    initgroups(pwd->pw_name, pwd->pw_gid) == -1 ||
	    seteuid((uid_t)pwd->pw_uid) == -1 ||
	    setuid((uid_t)pwd->pw_uid) == -1) {
		error("failed to setup.\n");
		exit(1);
	}
		
	(void)strcat(path, _PATH_DEFPATH);
	environ = envinit;
	strncat(homedir, pwd->pw_dir, sizeof(homedir)-6);
	strncat(shell, pwd->pw_shell, sizeof(shell)-7);
	strncat(username, pwd->pw_name, sizeof(username)-6);
	cp = strrchr(pwd->pw_shell, '/');
	if (cp)
		cp++;
	else
		cp = pwd->pw_shell;
	closelog();
	execl(pwd->pw_shell, cp, "-c", cmdbuf, 0);
	perror(pwd->pw_shell);
	exit(1);
}

/*VARARGS1*/
void
error(fmt, a1, a2, a3)
	char *fmt;
	int a1, a2, a3;
{
	char buf[BUFSIZ];

	buf[0] = 1;
	(void) snprintf(buf+1, sizeof buf-1, fmt, a1, a2, a3);
	(void) write(2, buf, strlen(buf));
}

void
getstr(buf, cnt, err)
	char *buf;
	int cnt;
	char *err;
{
	char c;

	do {
		if (read(0, &c, 1) != 1)
			exit(1);
		*buf++ = c;
		if (--cnt == 0) {
			error("%s too long\n", err);
			exit(1);
		}
	} while (c != 0);
}
