/*	$OpenBSD: sprayd.c,v 1.9 2004/06/02 02:21:15 brad Exp $*/

/*
 * Copyright (c) 1994 Christos Zoulas
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
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: sprayd.c,v 1.9 2004/06/02 02:21:15 brad Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include <rpc/rpc.h>
#include <rpcsvc/spray.h>

static void spray_service(struct svc_req *, SVCXPRT *);

static int from_inetd = 1;

#define TIMEOUT 120

static void
cleanup(int signo)
{
	(void) pmap_unset(SPRAYPROG, SPRAYVERS);	/* XXX signal race */
	_exit(0);
}

static void
die(int signo)
{
	_exit(0);
}

int
main(int argc, char *argv[])
{
	SVCXPRT *transp;
	int sock = 0;
	int proto = 0;
	struct sockaddr_storage from;
	socklen_t fromlen;

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

		(void) pmap_unset(SPRAYPROG, SPRAYVERS);

		(void) signal(SIGINT, cleanup);
		(void) signal(SIGTERM, cleanup);
		(void) signal(SIGHUP, cleanup);
	} else {
		(void) signal(SIGALRM, die);
		alarm(TIMEOUT);
	}

	openlog("rpc.sprayd", LOG_CONS|LOG_PID, LOG_DAEMON);

	transp = svcudp_create(sock);
	if (transp == NULL) {
		syslog(LOG_ERR, "cannot create udp service.");
		return 1;
	}
	if (!svc_register(transp, SPRAYPROG, SPRAYVERS, spray_service, proto)) {
		syslog(LOG_ERR,
		    "unable to register (SPRAYPROG, SPRAYVERS, %s).",
		    proto ? "udp" : "(inetd)");
		return 1;
	}

	svc_run();
	syslog(LOG_ERR, "svc_run returned");
	return 1;
}


static void
spray_service(struct svc_req *rqstp, SVCXPRT *transp)
{
	static struct timeval clear, get;
	static spraycumul scum;

	switch (rqstp->rq_proc) {
	case SPRAYPROC_CLEAR:
		scum.counter = 0;
		(void) gettimeofday(&clear, 0);
		/*FALLTHROUGH*/

	case NULLPROC:
		(void)svc_sendreply(transp, xdr_void, (char *)NULL);
		return;

	case SPRAYPROC_SPRAY:
		scum.counter++;
		return;

	case SPRAYPROC_GET:
		(void) gettimeofday(&get, 0);
		timersub(&get, &clear, &get);
		scum.clock.sec = get.tv_sec;
		scum.clock.usec = get.tv_usec;
		break;

	default:
		svcerr_noproc(transp);
		return;
	}

	if (!svc_sendreply(transp, xdr_spraycumul, (caddr_t)&scum)) {
		svcerr_systemerr(transp);
		syslog(LOG_ERR, "bad svc_sendreply");
	}
}
