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
static char rcsid[] = "$Id: ypserv.c,v 1.1 1995/11/01 16:56:36 deraadt Exp $";
#endif

#include "yp.h"
#include <stdio.h>
#include <stdlib.h>/* getenv, exit */
#include <rpc/pmap_clnt.h> /* for pmap_unset */
#include <string.h> /* strcmp */ 
#include <netdb.h>
#include <signal.h>
#include <sys/ttycom.h>/* TIOCNOTTY */
#ifdef __cplusplus
#include <sysent.h> /* getdtablesize, open */
#endif /* __cplusplus */
#include <memory.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef SYSLOG
#include <syslog.h>
#else
#define LOG_ERR 1
#define openlog(a, b, c)
#endif
#include "acl.h"
#include "yplog.h"
#include <sys/wait.h>

#define YP_SECURENET_FILE "/var/yp/securenet"

#ifdef __STDC__
#define SIG_PF void(*)(int)
#endif

#ifdef DEBUG
#define RPC_SVC_FG
#endif

#define _RPCSVC_CLOSEDOWN 120
static int _rpcpmstart;		/* Started by a port monitor ? */
static int _rpcfdtype;		/* Whether Stream or Datagram ? */
static int _rpcsvcdirty;	/* Still serving ? */

int	usedns = FALSE;
int	acl_access_ok;
char   *progname = "ypserv";

void sig_child();

static
void _msgout(char* msg)
{
#ifdef RPC_SVC_FG
	if (_rpcpmstart)
		syslog(LOG_ERR, msg);
	else
		(void) fprintf(stderr, "%s\n", msg);
#else
	syslog(LOG_ERR, msg);
#endif
}

static void
closedown()
{
	if (_rpcsvcdirty == 0) {
		extern fd_set svc_fdset;
		static int size;
		int i, openfd;

		if (_rpcfdtype == SOCK_DGRAM)
			exit(0);
		if (size == 0) {
			size = getdtablesize();
		}
		for (i = 0, openfd = 0; i < size && openfd < 2; i++)
			if (FD_ISSET(i, &svc_fdset))
				openfd++;
		if (openfd <= (_rpcpmstart?0:1))
			exit(0);
	}
	(void) alarm(_RPCSVC_CLOSEDOWN);
}

static void
ypprog_2(struct svc_req *rqstp, register SVCXPRT *transp)
{
	union {
		domainname ypproc_domain_2_arg;
		domainname ypproc_domain_nonack_2_arg;
		ypreq_key ypproc_match_2_arg;
		ypreq_key ypproc_first_2_arg;
		ypreq_key ypproc_next_2_arg;
		ypreq_xfr ypproc_xfr_2_arg;
		ypreq_nokey ypproc_all_2_arg;
		ypreq_nokey ypproc_master_2_arg;
		ypreq_nokey ypproc_order_2_arg;
		domainname ypproc_maplist_2_arg;
	} argument;
	char *result;
	xdrproc_t xdr_argument, xdr_result;
	char *(*local)(char *, struct svc_req *, SVCXPRT *);
	struct sockaddr_in *caller;

	_rpcsvcdirty = 1;
	
	caller = svc_getcaller(transp);
	acl_access_ok = acl_check_host(&caller->sin_addr);
	
	if (!acl_access_ok) {
		yplog_date("ypserv: access denied");
		yplog_call(transp);
		switch (rqstp->rq_proc) {
		case YPPROC_NULL:
			yplog_line("request: NULL");
			break;
		case YPPROC_DOMAIN:
			yplog_line("request: DOMAIN");
			break;
		case YPPROC_DOMAIN_NONACK:
			yplog_line("request: DOMAIN_NONACK");
			break;
		case YPPROC_MATCH:
			yplog_line("request: MATCH");
			break;
		case YPPROC_FIRST:
			yplog_line("request: FIRST");
			break;
		case YPPROC_NEXT:
			yplog_line("request: NEXT");
			break;
		case YPPROC_XFR:
			yplog_line("request: XFR");
			break;
		case YPPROC_CLEAR:
			yplog_line("request: CLEAR");
			break;
		case YPPROC_ALL:
			yplog_line("request: ALL");
			break;
		case YPPROC_MASTER:
			yplog_line("request: MASTER");
			break;
		case YPPROC_ORDER:
			yplog_line("request: ORDER");
			break;
		case YPPROC_MAPLIST:
			yplog_line("request: MAPLIST");
			break;
		default:
			yplog_line("request: unknown");
			break;
		}
	}
	
	switch (rqstp->rq_proc) {
	case YPPROC_NULL:
		xdr_argument = (xdrproc_t) xdr_void;
		xdr_result = (xdrproc_t) xdr_void;
		local = (char *(*)(char *, struct svc_req *, SVCXPRT *)) ypproc_null_2_svc;
		break;

	case YPPROC_DOMAIN:
		xdr_argument = (xdrproc_t) xdr_domainname;
		xdr_result = (xdrproc_t) xdr_bool;
		local = (char *(*)(char *, struct svc_req *, SVCXPRT *)) ypproc_domain_2_svc;
		break;

	case YPPROC_DOMAIN_NONACK:
		xdr_argument = (xdrproc_t) xdr_domainname;
		xdr_result = (xdrproc_t) xdr_bool;
		local = (char *(*)(char *, struct svc_req *, SVCXPRT *)) ypproc_domain_nonack_2_svc;
		break;

	case YPPROC_MATCH:
		xdr_argument = (xdrproc_t) xdr_ypreq_key;
		xdr_result = (xdrproc_t) xdr_ypresp_val;
		local = (char *(*)(char *, struct svc_req *, SVCXPRT *)) ypproc_match_2_svc;
		break;

	case YPPROC_FIRST:
		xdr_argument = (xdrproc_t) xdr_ypreq_key;
		xdr_result = (xdrproc_t) xdr_ypresp_key_val;
		local = (char *(*)(char *, struct svc_req *, SVCXPRT *)) ypproc_first_2_svc;
		break;

	case YPPROC_NEXT:
		xdr_argument = (xdrproc_t) xdr_ypreq_key;
		xdr_result = (xdrproc_t) xdr_ypresp_key_val;
		local = (char *(*)(char *, struct svc_req *, SVCXPRT *)) ypproc_next_2_svc;
		break;

	case YPPROC_XFR:
		xdr_argument = (xdrproc_t) xdr_ypreq_xfr;
		xdr_result = (xdrproc_t) xdr_ypresp_xfr;
		local = (char *(*)(char *, struct svc_req *, SVCXPRT *)) ypproc_xfr_2_svc;
		break;

	case YPPROC_CLEAR:
		xdr_argument = (xdrproc_t) xdr_void;
		xdr_result = (xdrproc_t) xdr_void;
		local = (char *(*)(char *, struct svc_req *, SVCXPRT *)) ypproc_clear_2_svc;
		break;

	case YPPROC_ALL:
		xdr_argument = (xdrproc_t) xdr_ypreq_nokey;
		xdr_result = (xdrproc_t) xdr_ypresp_all;
		local = (char *(*)(char *, struct svc_req *, SVCXPRT *)) ypproc_all_2_svc;
		break;

	case YPPROC_MASTER:
		xdr_argument = (xdrproc_t) xdr_ypreq_nokey;
		xdr_result = (xdrproc_t) xdr_ypresp_master;
		local = (char *(*)(char *, struct svc_req *, SVCXPRT *)) ypproc_master_2_svc;
		break;

	case YPPROC_ORDER:
		xdr_argument = (xdrproc_t) xdr_ypreq_nokey;
		xdr_result = (xdrproc_t) xdr_ypresp_order;
		local = (char *(*)(char *, struct svc_req *, SVCXPRT *)) ypproc_order_2_svc;
		break;

	case YPPROC_MAPLIST:
		xdr_argument = (xdrproc_t) xdr_domainname;
		xdr_result = (xdrproc_t) xdr_ypresp_maplist;
		local = (char *(*)(char *, struct svc_req *, SVCXPRT *)) ypproc_maplist_2_svc;
		break;

	default:
		svcerr_noproc(transp);
		_rpcsvcdirty = 0;
		return;
	}
	(void) memset((char *)&argument, 0, sizeof (argument));
	if (!svc_getargs(transp, xdr_argument, (caddr_t) &argument)) {
		svcerr_decode(transp);
		_rpcsvcdirty = 0;
		return;
	}
	result = (*local)((char *)&argument, rqstp, transp);
/*
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, (caddr_t) &argument)) {
		_msgout("unable to free arguments");
		exit(1);
	}
*/
	_rpcsvcdirty = 0;
	return;
}

int
main (argc,argv)
int argc;
char *argv[];
{
	register SVCXPRT *transp;
	int sock;
	int proto;
	struct sockaddr_in saddr;
	int asize = sizeof (saddr);
	int	 usage = 0;
	int	 xflag = 0;
	char	 ch;
	extern	 char *optarg;
	char	 *aclfile = NULL;
	
	while ((ch = getopt(argc, argv, "a:dx")) != EOF)
		switch (ch) {
		case 'a':
			aclfile = optarg;
			break;
		case 'd':
			usedns = TRUE;
			break;
		case 'x':
			xflag = TRUE;
			break;
		default:
			usage++;
			break;
		}
	
	if (usage) {
		(void)fprintf(stderr,"usage: %s [-a aclfile] [-d] [-x]\n",progname);
		exit(1);
	}

	if (geteuid() != 0) {
		(void)fprintf(stderr,"%s: must be root to run.\n",progname);
		exit(1);
	}

	if (aclfile != NULL) {
		(void)acl_init(aclfile);
	} else {
		(void)acl_securenet(YP_SECURENET_FILE);
	}
	if (xflag) {
		exit(1);
	};

	if (getsockname(0, (struct sockaddr *)&saddr, &asize) == 0) {
		int ssize = sizeof (int);

		if (saddr.sin_family != AF_INET)
			exit(1);
		if (getsockopt(0, SOL_SOCKET, SO_TYPE,
				(char *)&_rpcfdtype, &ssize) == -1)
			exit(1);
		sock = 0;
		_rpcpmstart = 1;
		proto = 0;
		openlog("ypserv", LOG_PID, LOG_DAEMON);
	} else {
#ifndef RPC_SVC_FG
		int size;
		int pid, i;

		pid = fork();
		if (pid < 0) {
			perror("cannot fork");
			exit(1);
		}
		if (pid)
			exit(0);
		size = getdtablesize();
		for (i = 0; i < size; i++)
			(void) close(i);
		i = open("/dev/console", 2);
		(void) dup2(i, 1);
		(void) dup2(i, 2);
		i = open("/dev/tty", 2);
		if (i >= 0) {
			(void) ioctl(i, TIOCNOTTY, (char *)NULL);
			(void) close(i);
		}
		openlog("ypserv", LOG_PID, LOG_DAEMON);
#endif
		sock = RPC_ANYSOCK;
		(void) pmap_unset(YPPROG, YPVERS);
	}

	yplog_init(progname);

	chdir("/");
	
	(void)signal(SIGCHLD, sig_child);

	if ((_rpcfdtype == 0) || (_rpcfdtype == SOCK_DGRAM)) {
		transp = svcudp_create(sock);
		if (transp == NULL) {
			_msgout("cannot create udp service.");
			exit(1);
		}
		if (!_rpcpmstart)
			proto = IPPROTO_UDP;
		if (!svc_register(transp, YPPROG, YPVERS, ypprog_2, proto)) {
			_msgout("unable to register (YPPROG, YPVERS, udp).");
			exit(1);
		}
	}

	if ((_rpcfdtype == 0) || (_rpcfdtype == SOCK_STREAM)) {
		if (_rpcpmstart)
			transp = svcfd_create(sock, 0, 0);
		else
			transp = svctcp_create(sock, 0, 0);
		if (transp == NULL) {
			_msgout("cannot create tcp service.");
			exit(1);
		}
		if (!_rpcpmstart)
			proto = IPPROTO_TCP;
		if (!svc_register(transp, YPPROG, YPVERS, ypprog_2, proto)) {
			_msgout("unable to register (YPPROG, YPVERS, tcp).");
			exit(1);
		}
	}

	if (transp == (SVCXPRT *)NULL) {
		_msgout("could not create a handle");
		exit(1);
	}
	if (_rpcpmstart) {
		(void) signal(SIGALRM, (SIG_PF) closedown);
		(void) alarm(_RPCSVC_CLOSEDOWN);
	}
	svc_run();
	_msgout("svc_run returned");
	exit(1);
	/* NOTREACHED */
}

void
sig_child()
{
	while (wait3((int *)NULL, WNOHANG, (struct rusage *)NULL) > 0);
}

