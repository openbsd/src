/*	$OpenBSD: rsh.c,v 1.34 2003/08/11 20:43:31 millert Exp $	*/

/*-
 * Copyright (c) 1983, 1990 The Regents of the University of California.
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1983, 1990 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static const char sccsid[] = "from: @(#)rsh.c	5.24 (Berkeley) 7/1/91";*/
static const char rcsid[] = "$OpenBSD: rsh.c,v 1.34 2003/08/11 20:43:31 millert Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>

#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pathnames.h"

#ifdef KERBEROS
#include <des.h>
#include <kerberosIV/krb.h>

CREDENTIALS cred;
Key_schedule schedule;
int use_kerberos = 1, doencrypt;
char dst_realm_buf[REALM_SZ], *dest_realm;

void warning(const char *, ...);
void desrw_set_key(des_cblock *, des_key_schedule *);
int des_read(int, char *, int);
int des_write(int, void *, int);

int krcmd(char **, u_short, char *, char *, int *, char *);
int krcmd_mutual(char **, u_short, char *, char *, int *, char *,
    CREDENTIALS *, Key_schedule);
#endif

void usage(void);
void sendsig(int);
char *copyargs(char **argv);

void talk(int, sigset_t *, int, int);

/*
 * rsh - remote shell
 */
int rfd2;

int
main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	struct passwd *pw;
	struct servent *sp;
	sigset_t mask, omask;
	int argoff, asrsh, ch, dflag, nflag, one, rem, uid;
	char *args, *host, *user, *p;
	pid_t pid = 0;

	argoff = asrsh = dflag = nflag = 0;
	one = 1;
	host = user = NULL;

	/* if called as something other than "rsh", use it as the host name */
	if ((p = strrchr(argv[0], '/')))
		++p;
	else
		p = argv[0];
	if (strcmp(p, "rsh"))
		host = p;
	else
		asrsh = 1;

	/* handle "rsh host flags" */
	if (!host && argc > 2 && argv[1][0] != '-') {
		host = argv[1];
		argoff = 1;
	}

#ifdef KERBEROS
#define	OPTIONS	"8KLdek:l:nwx"
#else
#define	OPTIONS	"8KLdel:nw"
#endif
	while ((ch = getopt(argc - argoff, argv + argoff, OPTIONS)) != -1)
		switch(ch) {
		case 'K':
#ifdef KERBEROS
			use_kerberos = 0;
#endif
			break;
		case 'L':	/* -8Lew are ignored to allow rlogin aliases */
		case 'e':
		case 'w':
		case '8':
			break;
		case 'd':
			dflag = 1;
			break;
		case 'l':
			user = optarg;
			break;
#ifdef KERBEROS
		case 'k':
			dest_realm = dst_realm_buf;
			strncpy(dest_realm, optarg, REALM_SZ);
			break;
#endif
		case 'n':
			nflag = 1;
			break;
#ifdef KERBEROS
		case 'x':
			doencrypt = 1;
			desrw_set_key(&cred.session, &schedule);
			break;
#endif
		case '?':
		default:
			usage();
		}
	optind += argoff;

	/* if haven't gotten a host yet, do so */
	if (!host && !(host = argv[optind++]))
		usage();

	/* if no command, login to remote host via rlogin or telnet. */
	if (!argv[optind]) {
		seteuid(getuid());
		setuid(getuid());
		if (asrsh)
			*argv = "rlogin";
		execv(_PATH_RLOGIN, argv);
		if (errno == ENOENT) {
			if (asrsh)
				*argv = "telnet";
			execv(_PATH_TELNET, argv);
		}
		errx(1, "can't exec %s", _PATH_TELNET);
	}

	argc -= optind;
	argv += optind;

	if (geteuid() != 0)
		errx(1, "must be setuid root");
	if (!(pw = getpwuid(uid = getuid())))
		errx(1, "unknown user ID %u", uid);
	if (!user)
		user = pw->pw_name;

#ifdef KERBEROS
	/* -x turns off -n */
	if (doencrypt)
		nflag = 0;
#endif

	args = copyargs(argv);

	sp = NULL;
#ifdef KERBEROS
	if (use_kerberos) {
		sp = getservbyname((doencrypt ? "ekshell" : "kshell"), "tcp");
		if (sp == NULL) {
			use_kerberos = 0;
			warning("can't get entry for %s/tcp service",
			    doencrypt ? "ekshell" : "kshell");
		}
	}
#endif
	if (sp == NULL)
		sp = getservbyname("shell", "tcp");
	if (sp == NULL)
		errx(1, "shell/tcp: unknown service");

	(void) unsetenv("RSH");		/* no tricks with rcmd(3) */

#ifdef KERBEROS
try_connect:
	if (use_kerberos) {
		rem = KSUCCESS;
		errno = 0;
		if (dest_realm == NULL)
			dest_realm = krb_realmofhost(host);

		if (doencrypt)
			rem = krcmd_mutual(&host, sp->s_port, user, args,
			    &rfd2, dest_realm, &cred, schedule);
		else
			rem = krcmd(&host, sp->s_port, user, args, &rfd2,
			    dest_realm);
		if (rem < 0) {
			use_kerberos = 0;
			sp = getservbyname("shell", "tcp");
			if (sp == NULL)
				errx(1, "unknown service shell/tcp");
			if (errno == ECONNREFUSED)
				warning("remote host doesn't support Kerberos");
			if (errno == ENOENT)
				warning("can't provide Kerberos auth data");
			goto try_connect;
		}
	} else {
		if (doencrypt)
			errx("the -x flag requires Kerberos authentication");
		rem = rcmd_af(&host, sp->s_port, pw->pw_name, user, args,
		    &rfd2, PF_UNSPEC);
	}
#else
	rem = rcmd_af(&host, sp->s_port, pw->pw_name, user, args, &rfd2,
	    PF_UNSPEC);
#endif

	if (rem < 0)
		exit(1);

	if (rfd2 < 0)
		errx(1, "can't establish stderr");
	if (dflag) {
		if (setsockopt(rem, SOL_SOCKET, SO_DEBUG, &one,
		    sizeof(one)) < 0)
			warn("setsockopt");
		if (setsockopt(rfd2, SOL_SOCKET, SO_DEBUG, &one,
		    sizeof(one)) < 0)
			warn("setsockopt");
	}

	(void)seteuid(uid);
	(void)setuid(uid);
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGQUIT);
	sigaddset(&mask, SIGTERM);
	sigprocmask(SIG_BLOCK, &mask, &omask);
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		(void)signal(SIGINT, sendsig);
	if (signal(SIGQUIT, SIG_IGN) != SIG_IGN)
		(void)signal(SIGQUIT, sendsig);
	if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
		(void)signal(SIGTERM, sendsig);

	if (!nflag) {
		if ((pid = fork()) < 0)
			err(1, "fork");
	}

#ifdef KERBEROS
	if (!doencrypt)
#endif
	{
		(void)ioctl(rfd2, FIONBIO, &one);
		(void)ioctl(rem, FIONBIO, &one);
	}

	talk(nflag, &omask, pid, rem);

	if (!nflag)
		(void)kill(pid, SIGKILL);

	return 0;
}

void
talk(int nflag, sigset_t *omask, pid_t pid, int rem)
{
	int cc, wc;
	char *bp;
	struct pollfd pfd[2];
	char buf[BUFSIZ];

	if (!nflag && pid == 0) {
		(void)close(rfd2);

reread:		errno = 0;
		if ((cc = read(STDIN_FILENO, buf, sizeof buf)) <= 0)
			goto done;
		bp = buf;

		pfd[0].fd = rem;
		pfd[0].events = POLLOUT;
rewrite:
		if (poll(pfd, 1, INFTIM) < 0) {
			if (errno != EINTR)
				err(1, "poll");
			goto rewrite;
		}
#ifdef KERBEROS
		if (doencrypt)
			wc = des_write(rem, bp, cc);
		else
#endif
			wc = write(rem, bp, cc);
		if (wc < 0) {
			if (errno == EWOULDBLOCK)
				goto rewrite;
			goto done;
		}
		bp += wc;
		cc -= wc;
		if (cc == 0)
			goto reread;
		goto rewrite;
done:
		(void)shutdown(rem, 1);
		exit(0);
	}

	sigprocmask(SIG_SETMASK, omask, NULL);
	pfd[1].fd = rfd2;
	pfd[1].events = POLLIN;
	pfd[0].fd = rem;
	pfd[0].events = POLLIN;
	do {
		if (poll(pfd, 2, INFTIM) < 0) {
			if (errno != EINTR)
				err(1, "poll");
			continue;
		}
		if (pfd[1].revents & POLLIN) {
			errno = 0;
#ifdef KERBEROS
			if (doencrypt)
				cc = des_read(rfd2, buf, sizeof buf);
			else
#endif
				cc = read(rfd2, buf, sizeof buf);
			if (cc <= 0) {
				if (errno != EWOULDBLOCK)
					pfd[1].revents = 0;
			} else
				(void)write(STDERR_FILENO, buf, cc);
		}
		if (pfd[0].revents & POLLIN) {
			errno = 0;
#ifdef KERBEROS
			if (doencrypt)
				cc = des_read(rem, buf, sizeof buf);
			else
#endif
				cc = read(rem, buf, sizeof buf);
			if (cc <= 0) {
				if (errno != EWOULDBLOCK)
					pfd[0].revents = 0;
			} else
				(void)write(STDOUT_FILENO, buf, cc);
		}
	} while ((pfd[0].revents & POLLIN) || (pfd[1].revents & POLLIN));
}

void
sendsig(int signo)
{
	int save_errno = errno;

#ifdef KERBEROS
	if (doencrypt)
		(void)des_write(rfd2, &signo, 1);
	else
#endif
		(void)write(rfd2, &signo, 1);
	errno = save_errno;
}

#ifdef KERBEROS
/* VARARGS */
void
warning(const char *fmt, ...)
{
	va_list ap;
	char myrealm[REALM_SZ];

	if (krb_get_lrealm(myrealm, 0) != KSUCCESS)
		return;
	(void)fprintf(stderr, "rsh: warning, using standard rsh: ");
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	(void)fprintf(stderr, ".\n");
}
#endif

char *
copyargs(char **argv)
{
	char **ap, *p, *args;
	size_t cc, len;

	cc = 0;
	for (ap = argv; *ap; ++ap)
		cc += strlen(*ap) + 1;
	if ((args = malloc(cc)) == NULL)
		err(1, NULL);
	for (p = args, ap = argv; *ap; ++ap) {
		len = strlcpy(p, *ap, cc);
		if (len >= cc)
			errx(1, "copyargs overflow");
		p += len;
		cc -= len;
		if (ap[1]) {
			*p++ = ' ';
			cc--;
		}
	}
	return(args);
}

void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: rsh [-Kdn%s]%s[-l username] hostname [command]\n",
#ifdef KERBEROS
	    "x", " [-k realm] ");
#else
	    "", " ");
#endif
	exit(1);
}
