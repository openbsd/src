/*	$OpenBSD: rstatd.c,v 1.14 2003/01/20 19:51:36 deraadt Exp $	*/

/*-
 * Copyright (c) 1993, John Brezak
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
static char rcsid[] = "$OpenBSD: rstatd.c,v 1.14 2003/01/20 19:51:36 deraadt Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pwd.h>
#include <syslog.h>
#include <errno.h>
#include <stdlib.h>
#include <rpc/rpc.h>
#include <rpcsvc/rstat.h>

extern int __svc_fdsetsize;
extern fd_set *__svc_fdset;
extern void svc_getreqset2(fd_set *, int);
extern void rstat_service(struct svc_req *, SVCXPRT *);

void my_svc_run(void);

int from_inetd = 1;     /* started from inetd ? */
int closedown = 20;	/* how long to wait before going dormant */

volatile sig_atomic_t gotsig;

void
getsig(int signo)
{
	gotsig = 1;
}


void
cleanup(void)
{
	(void) pmap_unset(RSTATPROG, RSTATVERS_TIME);	/* XXX signal races */
	(void) pmap_unset(RSTATPROG, RSTATVERS_SWTCH);
	(void) pmap_unset(RSTATPROG, RSTATVERS_ORIG);
	_exit(0);
}

int
main(int argc, char *argv[])
{
	int sock = 0, proto = 0;
	socklen_t fromlen;
	struct passwd *pw;
	struct sockaddr_in from;
	SVCXPRT *transp;

	openlog("rpc.rstatd", LOG_NDELAY|LOG_CONS|LOG_PID, LOG_DAEMON);

	pw = getpwnam("_rstatd");
	if (!pw)
		pw = getpwnam("nobody");
	if (chroot("/var/empty") == -1) {
		syslog(LOG_ERR, "cannot chdir to /var/empty.");
		exit(1);
	}
	chdir("/");

	if (pw) {
		setgroups(1, &pw->pw_gid);
		setegid(pw->pw_gid);
		setgid(pw->pw_gid);
		seteuid(pw->pw_uid);
		setuid(pw->pw_uid);
	}

	if (argc == 2)
		closedown = atoi(argv[1]);
	if (closedown <= 0)
		closedown = 20;

	/*
	 * See if inetd started us
	 */
	fromlen = sizeof(from);
	if (getsockname(0, (struct sockaddr *)&from, &fromlen) < 0) {
		from_inetd = 0;
		sock = RPC_ANYSOCK;
		proto = IPPROTO_UDP;
	}

	if (!from_inetd) {
		daemon(0, 0);

		(void)pmap_unset(RSTATPROG, RSTATVERS_TIME);
		(void)pmap_unset(RSTATPROG, RSTATVERS_SWTCH);
		(void)pmap_unset(RSTATPROG, RSTATVERS_ORIG);

		(void) signal(SIGINT, getsig);
		(void) signal(SIGTERM, getsig);
		(void) signal(SIGHUP, getsig);
	}

	transp = svcudp_create(sock);
	if (transp == NULL) {
		syslog(LOG_ERR, "cannot create udp service.");
		exit(1);
	}
	if (!svc_register(transp, RSTATPROG, RSTATVERS_TIME, rstat_service, proto)) {
		syslog(LOG_ERR, "unable to register (RSTATPROG, RSTATVERS_TIME, udp).");
		exit(1);
	}
	if (!svc_register(transp, RSTATPROG, RSTATVERS_SWTCH, rstat_service, proto)) {
		syslog(LOG_ERR, "unable to register (RSTATPROG, RSTATVERS_SWTCH, udp).");
		exit(1);
	}
	if (!svc_register(transp, RSTATPROG, RSTATVERS_ORIG, rstat_service, proto)) {
		syslog(LOG_ERR, "unable to register (RSTATPROG, RSTATVERS_ORIG, udp).");
		exit(1);
	}

	my_svc_run();
	syslog(LOG_ERR, "svc_run returned");
	exit(1);
}

void
my_svc_run(void)
{
	extern volatile sig_atomic_t wantupdatestat;
	extern void updatestat(void);
	fd_set *fds;

	for (;;) {
		if (wantupdatestat) {
			updatestat();
			wantupdatestat = 0;
		}
		if (gotsig)
			cleanup();

		if (__svc_fdset) {
			int bytes = howmany(__svc_fdsetsize, NFDBITS) *
			    sizeof(fd_mask);
			fds = (fd_set *)malloc(bytes);  /* XXX */
			memcpy(fds, __svc_fdset, bytes);
		} else
			fds = NULL;
		switch (select(svc_maxfd+1, fds, 0, 0, (struct timeval *)0)) {
		case -1:
			if (errno == EINTR) {
				if (fds)
					free(fds);
				continue;
			}
			perror("svc_run: - select failed");
			if (fds)
				free(fds);
			return;
		case 0:
			if (fds)
				free(fds);
			continue;
		default:
			svc_getreqset2(fds, svc_maxfd+1);
			free(fds);
		}
	}
}
