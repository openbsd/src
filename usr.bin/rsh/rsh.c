/*	$OpenBSD: rsh.c,v 1.30 2002/08/12 02:31:43 itojun Exp $	*/

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
"@(#) Copyright (c) 1983, 1990 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)rsh.c	5.24 (Berkeley) 7/1/91";*/
static char rcsid[] = "$OpenBSD: rsh.c,v 1.30 2002/08/12 02:31:43 itojun Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/file.h>

#include <netinet/in.h>
#include <netdb.h>

#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
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
		(void)fprintf(stderr, "rsh: can't exec %s.\n", _PATH_TELNET);
		exit(1);
	}

	argc -= optind;
	argv += optind;

	if (geteuid()) {
		(void)fprintf(stderr, "rsh: must be setuid root.\n");
		exit(1);
	}
	if (!(pw = getpwuid(uid = getuid()))) {
		(void)fprintf(stderr, "rsh: unknown user id.\n");
		exit(1);
	}
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
	if (sp == NULL) {
		(void)fprintf(stderr, "rsh: shell/tcp: unknown service.\n");
		exit(1);
	}

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
			if (sp == NULL) {
				(void)fprintf(stderr,
				    "rsh: unknown service shell/tcp.\n");
				exit(1);
			}
			if (errno == ECONNREFUSED)
				warning("remote host doesn't support Kerberos");
			if (errno == ENOENT)
				warning("can't provide Kerberos auth data");
			goto try_connect;
		}
	} else {
		if (doencrypt) {
			(void)fprintf(stderr,
			    "rsh: the -x flag requires Kerberos authentication.\n");
			exit(1);
		}
		rem = rcmd_af(&host, sp->s_port, pw->pw_name, user, args,
		    &rfd2, PF_UNSPEC);
	}
#else
	rem = rcmd_af(&host, sp->s_port, pw->pw_name, user, args, &rfd2,
	    PF_UNSPEC);
#endif

	if (rem < 0)
		exit(1);

	if (rfd2 < 0) {
		(void)fprintf(stderr, "rsh: can't establish stderr.\n");
		exit(1);
	}
	if (dflag) {
		if (setsockopt(rem, SOL_SOCKET, SO_DEBUG, &one,
		    sizeof(one)) < 0)
			(void)fprintf(stderr, "rsh: setsockopt: %s.\n",
			    strerror(errno));
		if (setsockopt(rfd2, SOL_SOCKET, SO_DEBUG, &one,
		    sizeof(one)) < 0)
			(void)fprintf(stderr, "rsh: setsockopt: %s.\n",
			    strerror(errno));
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
		pid = fork();
		if (pid < 0) {
			(void)fprintf(stderr,
			    "rsh: fork: %s.\n", strerror(errno));
			exit(1);
		}
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
	fd_set readfrom, ready, rembits;
	char buf[BUFSIZ];

	if (!nflag && pid == 0) {
		(void)close(rfd2);

reread:		errno = 0;
		if ((cc = read(0, buf, sizeof buf)) <= 0)
			goto done;
		bp = buf;

rewrite:	FD_ZERO(&rembits);
		if (rem >= FD_SETSIZE)
			errx(1, "descriptor too large");
                FD_SET(rem, &rembits);
		if (select(rem + 1, 0, &rembits, 0, 0) < 0) {
			if (errno != EINTR) {
				(void)fprintf(stderr,
				    "rsh: select: %s.\n", strerror(errno));
				exit(1);
			}
			goto rewrite;
		}
		if (!FD_ISSET(rem, &rembits))
			goto rewrite;
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
	FD_ZERO(&readfrom);
	if (rfd2 >= FD_SETSIZE)
		errx(1, "descriptor too large");
	FD_SET(rfd2, &readfrom);
	if (rem >= FD_SETSIZE)
		errx(1, "descriptor too large");
	FD_SET(rem, &readfrom);
	do {
		FD_COPY(&readfrom, &ready);
		if (select(MAX(rfd2, rem) + 1, &ready, 0, 0, 0) < 0) {
			if (errno != EINTR) {
				(void)fprintf(stderr,
				    "rsh: select: %s.\n", strerror(errno));
				exit(1);
			}
			continue;
		}
		if (FD_ISSET(rfd2, &ready)) {
			errno = 0;
#ifdef KERBEROS
			if (doencrypt)
				cc = des_read(rfd2, buf, sizeof buf);
			else
#endif
				cc = read(rfd2, buf, sizeof buf);
			if (cc <= 0) {
				if (errno != EWOULDBLOCK)
					FD_CLR(rfd2, &readfrom);
			} else
				(void)write(2, buf, cc);
		}
		if (FD_ISSET(rem, &ready)) {
			errno = 0;
#ifdef KERBEROS
			if (doencrypt)
				cc = des_read(rem, buf, sizeof buf);
			else
#endif
				cc = read(rem, buf, sizeof buf);
			if (cc <= 0) {
				if (errno != EWOULDBLOCK)
					FD_CLR(rem, &readfrom);
			} else
				(void)write(1, buf, cc);
		}
	} while (FD_ISSET(rem, &readfrom) || FD_ISSET(rfd2, &readfrom));
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
	int cc;

	cc = 0;
	for (ap = argv; *ap; ++ap)
		cc += strlen(*ap) + 1;
	if (!(args = malloc((u_int)cc))) {
		(void)fprintf(stderr, "rsh: %s.\n", strerror(ENOMEM));
		exit(1);
	}
	for (p = args, ap = argv; *ap; ++ap) {
		(void)strcpy(p, *ap);
		for (p = strcpy(p, *ap); *p; ++p);
		if (ap[1])
			*p++ = ' ';
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
