/*	$OpenBSD: rwalld.c,v 1.8 2002/09/05 00:21:24 deraadt Exp $	*/

/*
 * Copyright (c) 1993 Christopher G. Demetriou
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
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: rwalld.c,v 1.8 2002/09/05 00:21:24 deraadt Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <rpc/rpc.h>
#include <rpcsvc/rwall.h>

#ifdef OSF
#define WALL_CMD "/usr/sbin/wall"
#else
#define WALL_CMD "/usr/bin/wall -n"
#endif

void wallprog_1();

int from_inetd = 1;

void
cleanup(int signo)
{
	(void) pmap_unset(WALLPROG, WALLVERS);		/* XXX signal race */
	_exit(0);
}

int
main(int argc, char *argv[])
{
	int sock = 0, proto = 0, fromlen;
	struct sockaddr_in from;
	SVCXPRT *transp;

	if (geteuid() == 0) {
		struct passwd *pw = getpwnam("nobody");

		if (pw) {
			setgroups(1, &pw->pw_gid);
			setegid(pw->pw_gid);
			setgid(pw->pw_gid);
			seteuid(pw->pw_uid);
			setuid(pw->pw_uid);
		} else {
			seteuid(getuid());
			setuid(getuid());
		}
	}

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

		(void) pmap_unset(WALLPROG, WALLVERS);

		(void) signal(SIGINT, cleanup);
		(void) signal(SIGTERM, cleanup);
		(void) signal(SIGHUP, cleanup);
	}

	openlog("rpc.rwalld", LOG_CONS|LOG_PID, LOG_DAEMON);

	transp = svcudp_create(sock);
	if (transp == NULL) {
		syslog(LOG_ERR, "cannot create udp service.");
		exit(1);
	}
	if (!svc_register(transp, WALLPROG, WALLVERS, wallprog_1, proto)) {
		syslog(LOG_ERR, "unable to register (WALLPROG, WALLVERS, %s).",
		    proto ? "udp" : "(inetd)");
		exit(1);
	}

	svc_run();
	syslog(LOG_ERR, "svc_run returned");
	exit(1);

}

void *
wallproc_wall_1_svc(char **s, struct svc_req *rqstp)
{
	FILE *pfp;

	pfp = popen(WALL_CMD, "w");
	if (pfp != NULL) {
		fprintf(pfp, "\007\007%s", *s);
		pclose(pfp);
	}

	return (*s);
}

void
wallprog_1(struct svc_req *rqstp, SVCXPRT *transp)
{
	char *(*local)(char **, struct svc_req *);
	xdrproc_t xdr_argument, xdr_result;
	union {
		char *wallproc_wall_1_arg;
	} argument;
	char *result;

	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void)svc_sendreply(transp, xdr_void, (char *)NULL);
		goto leave;

	case WALLPROC_WALL:
		xdr_argument = (xdrproc_t)xdr_wrapstring;
		xdr_result = (xdrproc_t)xdr_void;
		local = (char *(*)(char **, struct svc_req *))
		    wallproc_wall_1_svc;
		break;

	default:
		svcerr_noproc(transp);
		goto leave;
	}
	bzero((char *)&argument, sizeof(argument));
	if (!svc_getargs(transp, xdr_argument, (caddr_t)&argument)) {
		svcerr_decode(transp);
		goto leave;
	}
	result = (*local)((char **)&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, (caddr_t)&argument)) {
		syslog(LOG_ERR, "unable to free arguments");
		exit(1);
	}
leave:
	if (from_inetd)
		exit(0);
}
