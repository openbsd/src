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
static char rcsid[] = "$Id: yppasswdd_proc.c,v 1.1 1995/10/23 07:44:43 deraadt Exp $";
#endif

#include <rpc/rpc.h>
#include <rpcsvc/yppasswd.h>
#include <stdio.h>
#include <string.h>
#include "yplog.h"

extern int make_passwd();

int *
yppasswdproc_update_1(argp, rqstp, transp)
	yppasswd *argp;
        struct svc_req *rqstp;
	SVCXPRT *transp;
{
	static int res;
	char	numstr[20];

	bzero((char *)&res, sizeof(res));

	yplog_date("yppasswdd_update_1: this code isn't tested");
	yplog_call(transp);
#ifdef DEBUG
	yplog_str (" oldpass: "); yplog_cat(argp->oldpass); yplog_cat("\n");
	yplog_line("   newpw:");
	yplog_str ("        name: "); yplog_cat(argp->newpw.pw_name);
	yplog_cat ("\n");
	yplog_str ("      passwd: "); yplog_cat(argp->newpw.pw_passwd);
	yplog_cat ("\n");
	yplog_str ("         uid: ");
	sprintf(numstr,"%d\n",argp->newpw.pw_uid);
	yplog_cat (numstr);
	yplog_str ("         gid: ");
	sprintf(numstr,"%d\n",argp->newpw.pw_gid);
	yplog_cat (numstr);
	yplog_str ("       gecos: "); yplog_cat(argp->newpw.pw_gecos);
	yplog_cat ("\n");
	yplog_str ("         dir: "); yplog_cat(argp->newpw.pw_dir);
	yplog_cat ("\n");
	yplog_str ("       shell: "); yplog_cat(argp->newpw.pw_shell);
	yplog_cat ("\n");
#endif
	
	res = make_passwd(argp);

	yplog_line("after make_passwd");
	
	if (!svc_sendreply(transp, xdr_int, (char *) &res)) {
		svcerr_systemerr(transp);
	}
	
	if (!svc_freeargs(transp, xdr_yppasswd, argp)) {
		(void)fprintf(stderr, "unable to free arguments\n");
		exit(1);
	}
	
	yplog_line("exit yppasswdproc_update_1");
	
	return ((void *)&res);
}

