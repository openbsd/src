/*
 * Copyright (c) 1994 Mats O Jansson <moj@stacken.kth.se>
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

#ifndef LINT
static char rcsid[] = "$Id: rpc.yppasswdd.c,v 1.2 1995/11/01 17:40:34 deraadt Exp $";
#endif

#include <stdio.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

#include "yppasswd.h"

static void yppasswddprog_1();
void    sig_child();

int     noshell, nogecos, nopw, domake;
char    make_arg[1024] = "make";
char   *progname = "yppasswdd";
char   *tempname;

void
usage()
{
	fprintf(stderr, "%s%s",
	    "usage: rpc.yppasswdd ",
	    "[-noshell] [-nogecos] [-nopw] [-m arg1 arg2 ... ]\n");
	exit(1);
}

int
main(argc, argv)
	int     argc;
	char   *argv[];
{
	SVCXPRT *transp;
	int     i = 1;

	while (i < argc) {
		if (argv[i][0] == '-') {
			if (strcmp("-noshell", argv[i]) == 0) {
				noshell = 1;
			} else if (strcmp("-nogecos", argv[i]) == 0) {
				nogecos = 1;
			} else if (strcmp("-nopw", argv[i]) == 0) {
				nopw = 1;
			} else if (strcmp("-m", argv[i]) == 0) {
				domake = 1;
				while (i < argc) {
					strcat(make_arg, " ");
					strcat(make_arg, argv[i]);
					i++;
				}
			} else
				usage();
			i++;
		} else
			usage();
	}

	switch (fork()) {
	case 0:
		break;
	case -1:
		perror("fork");
		exit(1);
	default:
		exit(0);
	}

	setsid();
	chdir("/etc");

/*
	freopen("/dev/null", "r", stdin);
	freopen("/var/yp/stderr", "w", stderr);
	freopen("/var/yp/stdout", "w", stdout);
*/
	(void) pmap_unset(YPPASSWDPROG, YPPASSWDVERS);

	(void) signal(SIGCHLD, sig_child);

	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL) {
		(void) fprintf(stderr, "cannot create udp service.\n");
		exit(1);
	}
	if (!svc_register(transp, YPPASSWDPROG, YPPASSWDVERS, yppasswddprog_1,
	    IPPROTO_UDP)) {
		fprintf(stderr, "unable to register YPPASSWDPROG, YPPASSWDVERS, udp\n");
		exit(1);
	}
	transp = svctcp_create(RPC_ANYSOCK, 0, 0);
	if (transp == NULL) {
		(void) fprintf(stderr, "cannot create tcp service.\n");
		exit(1);
	}
	if (!svc_register(transp, YPPASSWDPROG, YPPASSWDVERS, yppasswddprog_1,
	    IPPROTO_TCP)) {
		fprintf(stderr, "unable to register YPPASSWDPROG, YPPASSWDVERS, tcp\n");
		exit(1);
	}
	svc_run();
	(void) fprintf(stderr, "svc_run returned\n");
	exit(1);
}

static void
yppasswddprog_1(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	union {
		yppasswd yppasswdproc_update_1_arg;
	}       argument;
	char   *result;
	bool_t(*xdr_argument) (), (*xdr_result) ();
	char   *(*local) ();

	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void) svc_sendreply(transp, xdr_void, (char *) NULL);
		return;
	case YPPASSWDPROC_UPDATE:
		xdr_argument = xdr_yppasswd;
		xdr_result = xdr_int;
		local = (char *(*) ()) yppasswdproc_update_1_svc;
		break;
	default:
		svcerr_noproc(transp);
		return;
	}
	bzero((char *) &argument, sizeof(argument));
	if (!svc_getargs(transp, xdr_argument, (caddr_t) & argument)) {
		svcerr_decode(transp);
		return;
	}
	result = (*local) (&argument, rqstp, transp);
}

void
sig_child()
{
	while (wait3((int *) NULL, WNOHANG, (struct rusage *) NULL) > 0)
		;
}
