/*	$OpenBSD: ypserv.c,v 1.24 2002/07/19 20:59:40 deraadt Exp $ */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Mats O Jansson
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
static char rcsid[] = "$OpenBSD: ypserv.c,v 1.24 2002/07/19 20:59:40 deraadt Exp $";
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/ttycom.h>
#include <netinet/in.h>
#include "yp.h"
#include "ypv1.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <signal.h>
#include <errno.h>
#include <util.h>
#include <unistd.h>
#include <fcntl.h>
#include <rpc/pmap_clnt.h>
#include <memory.h>
#include <syslog.h>
#include "acl.h"
#include "yplog.h"
#include "ypdef.h"
#include <sys/wait.h>

void ypdb_init(void);

#ifdef DEBUG
#define RPC_SVC_FG
#endif

#define _RPCSVC_CLOSEDOWN 120
static int _rpcpmstart;		/* Started by a port monitor ? */
static int _rpcfdtype;		/* Whether Stream or Datagram ? */
static int _rpcsvcdirty;	/* Still serving ? */

int	usedns = FALSE;
char   *aclfile = NULL;

void	sig_child(int);
void	sig_hup(int);
volatile sig_atomic_t wantsighup;

extern	int __svc_fdsetsize;
extern	fd_set *__svc_fdset;
extern	void svc_getreqset2(fd_set *, int);

static void
_msgout(char *msg)
{
#ifdef RPC_SVC_FG
	if (_rpcpmstart)
		syslog(LOG_ERR, "%s", msg);
	else
		(void) fprintf(stderr, "%s\n", msg);
#else
	syslog(LOG_ERR, "%s", msg);
#endif
}

static void
closedown(int sig)
{
	int save_errno = errno;

	if (_rpcsvcdirty == 0) {
		extern fd_set *__svc_fdset;
		extern int __svc_fdsetsize;
		int i, openfd;

		if (_rpcfdtype == SOCK_DGRAM)
			exit(0);
		for (i = 0, openfd = 0; i < __svc_fdsetsize && openfd < 2; i++)
			if (FD_ISSET(i, __svc_fdset))
				openfd++;
		if (openfd <= (_rpcpmstart?0:1))
			_exit(0);
	}
	(void) alarm(_RPCSVC_CLOSEDOWN);
	errno = save_errno;
}

static void
ypprog_1(struct svc_req *rqstp, SVCXPRT *transp)
{
	union {
		domainname ypproc_domain_1_arg;
		domainname ypproc_domain_nonack_1_arg;
		yprequest ypproc_match_1_arg;
		yprequest ypproc_first_1_arg;
		yprequest ypproc_next_1_arg;
		yprequest ypproc_poll_1_arg;
		yprequest ypproc_push_1_arg;
		yprequest ypproc_pull_1_arg;
		yprequest ypproc_get_1_arg;
	} argument;
	char *result;
	xdrproc_t xdr_argument, xdr_result;
	char *(*local)(char *, struct svc_req *);

	_rpcsvcdirty = 1;
	switch (rqstp->rq_proc) {
	case YPOLDPROC_NULL:
		xdr_argument = (xdrproc_t) xdr_void;
		xdr_result = (xdrproc_t) xdr_void;
		local = (char *(*)(char *, struct svc_req *)) ypproc_null_1_svc;
		break;

	case YPOLDPROC_DOMAIN:
		xdr_argument = (xdrproc_t) xdr_domainname;
		xdr_result = (xdrproc_t) xdr_bool;
		local = (char *(*)(char *, struct svc_req *)) ypproc_domain_1_svc;
		break;

	case YPOLDPROC_DOMAIN_NONACK:
		xdr_argument = (xdrproc_t) xdr_domainname;
		xdr_result = (xdrproc_t) xdr_bool;
		local = (char *(*)(char *, struct svc_req *)) ypproc_domain_nonack_1_svc;
		break;

	case YPOLDPROC_MATCH:
		xdr_argument = (xdrproc_t) xdr_yprequest;
		xdr_result = (xdrproc_t) xdr_ypresponse;
		local = (char *(*)(char *, struct svc_req *)) ypproc_match_1_svc;
		break;

	case YPOLDPROC_FIRST:
		xdr_argument = (xdrproc_t) xdr_yprequest;
		xdr_result = (xdrproc_t) xdr_ypresponse;
		local = (char *(*)(char *, struct svc_req *)) ypproc_first_1_svc;
		break;

	case YPOLDPROC_NEXT:
		xdr_argument = (xdrproc_t) xdr_yprequest;
		xdr_result = (xdrproc_t) xdr_ypresponse;
		local = (char *(*)(char *, struct svc_req *)) ypproc_next_1_svc;
		break;

	case YPOLDPROC_POLL:
		xdr_argument = (xdrproc_t) xdr_yprequest;
		xdr_result = (xdrproc_t) xdr_ypresponse;
		local = (char *(*)(char *, struct svc_req *)) ypproc_poll_1_svc;
		break;

	case YPOLDPROC_PUSH:
		xdr_argument = (xdrproc_t) xdr_yprequest;
		xdr_result = (xdrproc_t) xdr_void;
		local = (char *(*)(char *, struct svc_req *)) ypproc_push_1_svc;
		break;

	case YPOLDPROC_PULL:
		xdr_argument = (xdrproc_t) xdr_yprequest;
		xdr_result = (xdrproc_t) xdr_void;
		local = (char *(*)(char *, struct svc_req *)) ypproc_pull_1_svc;
		break;

	case YPOLDPROC_GET:
		xdr_argument = (xdrproc_t) xdr_yprequest;
		xdr_result = (xdrproc_t) xdr_void;
		local = (char *(*)(char *, struct svc_req *)) ypproc_get_1_svc;
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
	result = (*local)((char *)&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, (caddr_t) &argument)) {
		_msgout("unable to free arguments");
		exit(1);
	}
	_rpcsvcdirty = 0;
}

static void
ypprog_2(struct svc_req *rqstp, SVCXPRT *transp)
{
	union {
		domainname ypproc_domain_2_arg;
		domainname ypproc_domain_nonack_2_arg;
		ypreq_key ypproc_match_2_arg;
		ypreq_nokey ypproc_first_2_arg;
		ypreq_key ypproc_next_2_arg;
		ypreq_xfr ypproc_xfr_2_arg;
		ypreq_nokey ypproc_all_2_arg;
		ypreq_nokey ypproc_master_2_arg;
		ypreq_nokey ypproc_order_2_arg;
		domainname ypproc_maplist_2_arg;
	} argument;
	char *result;
	xdrproc_t xdr_argument, xdr_result;
	char *(*local)(char *, struct svc_req *);

	_rpcsvcdirty = 1;
	switch (rqstp->rq_proc) {
	case YPPROC_NULL:
		xdr_argument = (xdrproc_t) xdr_void;
		xdr_result = (xdrproc_t) xdr_void;
		local = (char *(*)(char *, struct svc_req *)) ypproc_null_2_svc;
		break;

	case YPPROC_DOMAIN:
		xdr_argument = (xdrproc_t) xdr_domainname;
		xdr_result = (xdrproc_t) xdr_bool;
		local = (char *(*)(char *, struct svc_req *)) ypproc_domain_2_svc;
		break;

	case YPPROC_DOMAIN_NONACK:
		xdr_argument = (xdrproc_t) xdr_domainname;
		xdr_result = (xdrproc_t) xdr_bool;
		local = (char *(*)(char *, struct svc_req *)) ypproc_domain_nonack_2_svc;
		break;

	case YPPROC_MATCH:
		xdr_argument = (xdrproc_t) xdr_ypreq_key;
		xdr_result = (xdrproc_t) xdr_ypresp_val;
		local = (char *(*)(char *, struct svc_req *)) ypproc_match_2_svc;
		break;

	case YPPROC_FIRST:
		xdr_argument = (xdrproc_t) xdr_ypreq_nokey;
		xdr_result = (xdrproc_t) xdr_ypresp_key_val;
		local = (char *(*)(char *, struct svc_req *)) ypproc_first_2_svc;
		break;

	case YPPROC_NEXT:
		xdr_argument = (xdrproc_t) xdr_ypreq_key;
		xdr_result = (xdrproc_t) xdr_ypresp_key_val;
		local = (char *(*)(char *, struct svc_req *)) ypproc_next_2_svc;
		break;

	case YPPROC_XFR:
		xdr_argument = (xdrproc_t) xdr_ypreq_xfr;
		xdr_result = (xdrproc_t) xdr_ypresp_xfr;
		local = (char *(*)(char *, struct svc_req *)) ypproc_xfr_2_svc;
		break;

	case YPPROC_CLEAR:
		xdr_argument = (xdrproc_t) xdr_void;
		xdr_result = (xdrproc_t) xdr_void;
		local = (char *(*)(char *, struct svc_req *)) ypproc_clear_2_svc;
		break;

	case YPPROC_ALL:
		xdr_argument = (xdrproc_t) xdr_ypreq_nokey;
		xdr_result = (xdrproc_t) xdr_ypresp_all;
		local = (char *(*)(char *, struct svc_req *)) ypproc_all_2_svc;
		break;

	case YPPROC_MASTER:
		xdr_argument = (xdrproc_t) xdr_ypreq_nokey;
		xdr_result = (xdrproc_t) xdr_ypresp_master;
		local = (char *(*)(char *, struct svc_req *)) ypproc_master_2_svc;
		break;

	case YPPROC_ORDER:
		xdr_argument = (xdrproc_t) xdr_ypreq_nokey;
		xdr_result = (xdrproc_t) xdr_ypresp_order;
		local = (char *(*)(char *, struct svc_req *)) ypproc_order_2_svc;
		break;

	case YPPROC_MAPLIST:
		xdr_argument = (xdrproc_t) xdr_domainname;
		xdr_result = (xdrproc_t) xdr_ypresp_maplist;
		local = (char *(*)(char *, struct svc_req *)) ypproc_maplist_2_svc;
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
	result = (*local)((char *)&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, (caddr_t) &argument)) {
		_msgout("unable to free arguments");
		exit(1);
	}
	_rpcsvcdirty = 0;
}

void
hup()
{
	/* Handle the log. */
	ypcloselog();
	ypopenlog();

	acl_reset();
	if (aclfile != NULL) {
		yplog("sig_hup: reread %s", aclfile);
		(void)acl_init(aclfile);
	} else {
		yplog("sig_hup: reread %s", YP_SECURENET_FILE);
		(void)acl_securenet(YP_SECURENET_FILE);
	}
}

void
my_svc_run()
{
	fd_set *fds;

	for (;;) {
		if (wantsighup) {
			hup();
			wantsighup = 0;
		}
		if (__svc_fdset) {
			int bytes = howmany(__svc_fdsetsize, NFDBITS) *
			    sizeof(fd_mask);
			fds = (fd_set *)malloc(bytes);	/* XXX */
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

void
usage(void)
{
	(void)fprintf(stderr, "usage: ypserv [-a aclfile] [-d] [-x]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int xflag = 0, allowv1 = 0, ch, sock, proto;
	struct sockaddr_in saddr;
	int asize = sizeof (saddr);
	extern char *optarg;
	SVCXPRT *transp;

	while ((ch = getopt(argc, argv, "1a:dx")) != -1)
		switch (ch) {
		case '1':
			allowv1 = TRUE;
			break;
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
			usage();
			break;
		}

	if (geteuid() != 0) {
		(void)fprintf(stderr, "ypserv: must be root to run.\n");
		exit(1);
	}

	if (aclfile != NULL)
		(void)acl_init(aclfile);
	else
		(void)acl_securenet(YP_SECURENET_FILE);

	if (xflag)
		exit(1);

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
		int size, i;
		pid_t pid;

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
		(void) pmap_unset(YPPROG, YPOLDVERS);
	}

	ypopenlog();	/* open log file */
	ypdb_init();	/* init db stuff */

	chdir("/");

	(void)signal(SIGCHLD, sig_child);
	(void)signal(SIGHUP, sig_hup);
	pidfile(NULL);

	if (_rpcfdtype == 0 || _rpcfdtype == SOCK_DGRAM) {
		transp = svcudp_create(sock);
		if (transp == NULL) {
			_msgout("cannot create udp service.");
			exit(1);
		}
		if (transp->xp_port >= IPPORT_RESERVED) {
			_msgout("cannot allocate udp privileged port.");
			exit(1);
		}
		if (!_rpcpmstart)
			proto = IPPROTO_UDP;
		if (allowv1) {
			if (!svc_register(transp, YPPROG, YPOLDVERS, ypprog_1, proto)) {
				_msgout("unable to register (YPPROG, YPOLDVERS, udp).");
				exit(1);
			}
		}
		if (!svc_register(transp, YPPROG, YPVERS, ypprog_2, proto)) {
			_msgout("unable to register (YPPROG, YPVERS, udp).");
			exit(1);
		}
	}

	if (_rpcfdtype == 0 || _rpcfdtype == SOCK_STREAM) {
		if (_rpcpmstart)
			transp = svcfd_create(sock, 0, 0);
		else
			transp = svctcp_create(sock, 0, 0);
		if (transp == NULL) {
			_msgout("cannot create tcp service.");
			exit(1);
		}
		if (transp->xp_port >= IPPORT_RESERVED) {
			_msgout("cannot allocate tcp privileged port.");
			exit(1);
		}
		if (!_rpcpmstart)
			proto = IPPROTO_TCP;
		if (allowv1) {
			if (!svc_register(transp, YPPROG, YPOLDVERS, ypprog_1, proto)) {
				_msgout("unable to register (YPPROG, YPOLDVERS, tcp).");
				exit(1);
			}
		}
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
		(void) signal(SIGALRM, closedown);
		(void) alarm(_RPCSVC_CLOSEDOWN);
	}
	my_svc_run();
	_msgout("svc_run returned");
	exit(1);
	/* NOTREACHED */
}

void
sig_child(int signo)
{
	int save_errno = errno;

	while (wait3((int *)NULL, WNOHANG, (struct rusage *)NULL) > 0)
		;
	errno = save_errno;
}

void
sig_hup(int signo)
{
	wantsighup = 1;
}
