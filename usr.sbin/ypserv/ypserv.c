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
static char rcsid[] = "$Id: ypserv.c,v 1.1 1995/10/23 07:46:44 deraadt Exp $";
#endif

#include <stdio.h>
#include <rpc/rpc.h>
#include <rpc/auth.h>
#include <rpcsvc/yp.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <rpc/pmap_clnt.h>
#include "acl.h"
#include "yplog.h"

#define YP_SECURENET_FILE "/var/yp/securenet"

static void ypprog_2();
void sig_child();

int	usedns = FALSE;
int	acl_access_ok;
char   *progname = "ypserv";

int
main (argc,argv)
int argc;
char *argv[];
{
	SVCXPRT *transp;
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

#ifdef DAEMON
	switch(fork()) {
	case 0:
	  break;
	case -1:
	  (void)fprintf(stderr,"%s: fork failure\n",progname);
	  exit(1);
	default:
	  exit(0);
	}
	setsid();
#endif
	
	yplog_init(progname);

	chdir("/");
	
	(void)pmap_unset(YPPROG, YPVERS);
	
	(void)signal(SIGCHLD, sig_child);

	transp = svctcp_create(RPC_ANYSOCK, 0, 0);
	if (transp == NULL) {
		(void)fprintf(stderr, "cannot create tcp service.\n");
		exit(1);
	}
	if (!svc_register(transp, YPPROG, YPVERS, ypprog_2, IPPROTO_TCP)) {
		(void)fprintf(stderr, "unable to register (YPPROG, YPVERS, tcp).\n");
		exit(1);
	}

	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL) {
		(void)fprintf(stderr, "cannot create udp service.\n");
		exit(1);
	}
	if (!svc_register(transp, YPPROG, YPVERS, ypprog_2, IPPROTO_UDP)) {
		(void)fprintf(stderr, "unable to register (YPPROG, YPVERS, udp).\n");
		exit(1);
	}
	svc_run();
	(void)fprintf(stderr, "svc_run returned\n");
	exit(1);
}

static void
ypprog_2(rqstp, transp)
        struct svc_req *rqstp;
	SVCXPRT *transp;
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
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();
	struct sockaddr_in *caller;

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
	    xdr_argument = xdr_void;
	    xdr_result = xdr_void;
	    local = (char *(*)()) ypproc_null_2;
	    break;
	    
	case YPPROC_DOMAIN:
	    xdr_argument = xdr_domainname;
	    xdr_result = xdr_bool;
	    local = (char *(*)()) ypproc_domain_2;
	    break;
	    
	case YPPROC_DOMAIN_NONACK:
	    xdr_argument = xdr_domainname;
	    xdr_result = xdr_bool;
	    local = (char *(*)()) ypproc_domain_nonack_2;
	    break;
	    
	case YPPROC_MATCH:
	    xdr_argument = xdr_ypreq_key;
	    xdr_result = xdr_ypresp_val;
	    local = (char *(*)()) ypproc_match_2;
	    break;
	    
	case YPPROC_FIRST:
	    xdr_argument = xdr_ypreq_nokey;
	    xdr_result = xdr_ypresp_key_val;
	    local = (char *(*)()) ypproc_first_2;
	    break;
	    
	case YPPROC_NEXT:
	    xdr_argument = xdr_ypreq_key;
	    xdr_result = xdr_ypresp_key_val;
	    local = (char *(*)()) ypproc_next_2;
	    break;
	    
	case YPPROC_XFR:
	    xdr_argument = xdr_ypreq_xfr;
	    xdr_result = xdr_ypresp_xfr;
	    local = (char *(*)()) ypproc_xfr_2;
	    break;
	    
	case YPPROC_CLEAR:
	    xdr_argument = xdr_void;
	    xdr_result = xdr_void;
	    local = (char *(*)()) ypproc_clear_2;
	    break;
	    
	case YPPROC_ALL:
	    xdr_argument = xdr_ypreq_nokey;
	    xdr_result = xdr_ypresp_all;
	    local = (char *(*)()) ypproc_all_2;
	    break;
	    
	case YPPROC_MASTER:
	    xdr_argument = xdr_ypreq_nokey;
	    xdr_result = xdr_ypresp_master;
	    local = (char *(*)()) ypproc_master_2;
	    break;
	    
	case YPPROC_ORDER:
	    xdr_argument = xdr_ypreq_nokey;
	    xdr_result = xdr_ypresp_order;
	    local = (char *(*)()) ypproc_order_2;
	    break;
	    
	case YPPROC_MAPLIST:
	    xdr_argument = xdr_domainname;
	    xdr_result = xdr_ypresp_maplist;
	    local = (char *(*)()) ypproc_maplist_2;
	    break;
	    
	default:
	    printf("switch default: %d\n",(int) rqstp->rq_proc);
	    svcerr_noproc(transp);
	    return;
	}

	bzero((char *)&argument, sizeof(argument));
	if (!svc_getargs(transp, xdr_argument, &argument)) {
		svcerr_decode(transp);
		return;
	}
	result = (*local)(&argument, rqstp, transp);

}

void
sig_child()
{
	while (wait3((int *)NULL, WNOHANG, (struct rusage *)NULL) > 0);
}
