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
static char rcsid[] = "$Id: ypserv_proc.c,v 1.1 1995/11/01 16:56:37 deraadt Exp $";
#endif

#include <rpc/rpc.h>
#include "yp.h"
#include <rpcsvc/ypclnt.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "ypdb.h"
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "yplog.h"
#include "ypdef.h"

extern ypresp_val ypdb_get_record();
extern ypresp_key_val ypdb_get_first();
extern ypresp_key_val ypdb_get_next();
extern ypresp_order ypdb_get_order();
extern ypresp_master ypdb_get_master();
extern bool_t ypdb_xdr_get_all();
extern void ypdb_close_all();
extern int acl_access_ok;

void *
ypproc_null_2_svc(argp, rqstp, transp)
	void *argp;
        struct svc_req *rqstp;
	SVCXPRT *transp;
{
	static char res;

	bzero((char *)&res, sizeof(res));

	yplog_date("ypproc_null_2: this code isn't tested");
	yplog_call(transp);
	
	if (!svc_sendreply(transp, xdr_void, (char *) &res)) {
		svcerr_systemerr(transp);
	}
	
	if (!svc_freeargs(transp, xdr_void, (caddr_t) argp)) {
		(void)fprintf(stderr, "unable to free arguments\n");
		exit(1);
	}
	
	return ((void *)&res);
}

bool_t *
ypproc_domain_2_svc(argp, rqstp, transp)
	domainname *argp;
        struct svc_req *rqstp;
	SVCXPRT *transp;
{
	static bool_t res;
	static bool_t domain_served;
	static char   domain_path[255];
	struct	stat	finfo;

	bzero((char *)&res, sizeof(res));

	if (acl_access_ok) {
	  sprintf(domain_path,"%s/%s",YP_DB_PATH,*argp);
	  domain_served = (bool_t) ((stat(domain_path, &finfo) == 0) &&
				    (finfo.st_mode & S_IFDIR));
	} else {
	  domain_served = FALSE;
	}

#ifdef DEBUG
	yplog_date("ypproc_domain_2:");
	yplog_call(transp);
	yplog_str("  domain: "); yplog_cat(*argp); yplog_cat("\n");
	yplog_str("  served: ");
	if (domain_served) {
	  yplog_cat("true\n");
	} else {
	  yplog_cat("false\n");
	}
#endif

	res = domain_served;
	  
	if (!svc_sendreply(transp, xdr_bool, (char *) &res)) {
	  svcerr_systemerr(transp);
	}

	if (!svc_freeargs(transp, xdr_domainname, (caddr_t) argp)) {
		(void)fprintf(stderr, "unable to free arguments\n");
		exit(1);
	}

	return (&res);
}

bool_t *
ypproc_domain_nonack_2_svc(argp, rqstp, transp)
	domainname *argp;
        struct svc_req *rqstp;
	SVCXPRT *transp;
{
	static bool_t res;
	static bool_t domain_served;
	static char   domain_path[255];
	struct	stat	finfo;

	bzero((char *)&res, sizeof(res));

	if (acl_access_ok) {
	  sprintf(domain_path,"%s/%s",YP_DB_PATH,*argp);
	  domain_served = (bool_t) ((stat(domain_path, &finfo) == 0) &&
				    (finfo.st_mode & S_IFDIR));
	} else {
	  domain_served = FALSE;
	}
	
#ifdef DEBUG
	yplog_date("ypproc_domain_nonack_2:");
	yplog_call(transp);
	yplog_str("  domain: "); yplog_cat(*argp); yplog_cat("\n");
	yplog_str("  served: ");
	if (domain_served) {
	  yplog_cat("true\n");
	} else {
	  yplog_cat("false\n");
	}
#endif

	if (domain_served) {
	  
	  res = domain_served;
	  
	  if (!svc_sendreply(transp, xdr_bool, (char *) &res)) {
	  	svcerr_systemerr(transp);
	  }
	
	} else {

	  res = (bool_t) FALSE;

	  svcerr_decode(transp);
	  
	}

	if (!svc_freeargs(transp, xdr_domainname, (caddr_t) argp)) {
		(void)fprintf(stderr, "unable to free arguments\n");
		exit(1);
	}

	return (&res);
}

ypresp_val *
ypproc_match_2_svc(argp, rqstp, transp)
	ypreq_key *argp;
        struct svc_req *rqstp;
	SVCXPRT *transp;
{
	static ypresp_val res;

	bzero((char *)&res, sizeof(res));
	
#ifdef DEBUG
	yplog_date("ypproc_match_2:");
	yplog_call(transp);
	yplog_str("  domain: "); yplog_cat(argp->domain); yplog_cat("\n");
	yplog_str("     map: "); yplog_cat(argp->map); yplog_cat("\n");
	yplog_str("     key: "); yplog_cat(argp->key.keydat_val);
	yplog_cat("\n");
#endif

	if (acl_access_ok) {
	  res = ypdb_get_record(argp->domain,argp->map,argp->key, FALSE);
	} else {
	  res.stat = YP_NODOM;
	}
	
#ifdef DEBUG
	yplog_str("  status: ");
	yplog_cat(yperr_string(ypprot_err(res.stat)));
	yplog_cat("\n");
#endif

	if (!svc_sendreply(transp, xdr_ypresp_val, (char *) &res)) {
		svcerr_systemerr(transp);
	}

	if (!svc_freeargs(transp, xdr_ypreq_key, (caddr_t) argp)) {
		(void)fprintf(stderr, "unable to free arguments\n");
		exit(1);
	}

	return (&res);
}

ypresp_key_val *
ypproc_first_2_svc(argp, rqstp, transp)
	ypreq_key *argp;
        struct svc_req *rqstp;
	SVCXPRT *transp;
{
	static ypresp_key_val res;

	bzero((char *)&res, sizeof(res));
	
#ifdef DEBUG
	yplog_date("ypproc_first_2:");
	yplog_call(transp);
	yplog_str("  domain: "); yplog_cat(argp->domain); yplog_cat("\n");
	yplog_str("     map: "); yplog_cat(argp->map); yplog_cat("\n");
#endif

	if (acl_access_ok) {
	  res = ypdb_get_first(argp->domain,argp->map,FALSE);
	} else {
	  res.stat = YP_NODOM;
	}

#ifdef DEBUG
	yplog_str("  status: ");
	yplog_cat(yperr_string(ypprot_err(res.stat)));
	yplog_cat("\n");
#endif

	if (!svc_sendreply(transp, xdr_ypresp_key_val, (char *) &res)) {
		svcerr_systemerr(transp);
	}

	if (!svc_freeargs(transp, xdr_ypreq_key, (caddr_t) argp)) {
		(void)fprintf(stderr, "unable to free arguments\n");
		exit(1);
	}

	return (&res);
}

ypresp_key_val *
ypproc_next_2_svc(argp, rqstp, transp)
	ypreq_key *argp;
        struct svc_req *rqstp;
	SVCXPRT *transp;
{
	static ypresp_key_val res;

	bzero((char *)&res, sizeof(res));
	
#ifdef DEBUG
	yplog_date("ypproc_next_2:");
	yplog_call(transp);
	yplog_str("  domain: "); yplog_cat(argp->domain); yplog_cat("\n");
	yplog_str("     map: "); yplog_cat(argp->map); yplog_cat("\n");
	yplog_str("     key: "); yplog_cat(argp->key.keydat_val);
	yplog_cat("\n");
#endif

	if (acl_access_ok) {
	  res = ypdb_get_next(argp->domain,argp->map,argp->key,FALSE);
	} else {
	  res.stat = YP_NODOM;
	}

#ifdef DEBUG
	yplog_str("  status: ");
	yplog_cat(yperr_string(ypprot_err(res.stat)));
	yplog_cat("\n");
#endif

	if (!svc_sendreply(transp, xdr_ypresp_key_val, (char *) &res)) {
		svcerr_systemerr(transp);
	}
	
	if (!svc_freeargs(transp, xdr_ypreq_key, (caddr_t) argp)) {
		(void)fprintf(stderr, "unable to free arguments\n");
		exit(1);
	}

	return (&res);
}

ypresp_xfr *
ypproc_xfr_2_svc(argp, rqstp, transp)
	ypreq_xfr *argp;
        struct svc_req *rqstp;
	SVCXPRT *transp;
{
	static ypresp_xfr res;
	pid_t	pid;
	char	tid[10];
	char	prog[10];
	char	port[10];
	char	ypxfr_proc[] = YPXFR_PROC;
	struct sockaddr_in *sin;
	char	*ipadd;

	bzero((char *)&res, sizeof(res));
	
	yplog_date("ypproc_xfr_2: this code isn't yet implemented");
	yplog_call(transp);
	
	pid = vfork();

	if (pid == -1) {
		
		/* An error has occurred */
		
		return(&res);
		
	}

	if (pid == 0) {
		
		sprintf(tid,"%d",argp->transid);
		sprintf(prog, "%d", argp->prog);
		sprintf(port, "%d", argp->port);
		sin = svc_getcaller(transp);
		ipadd = inet_ntoa(sin->sin_addr);

		execl(ypxfr_proc, "ypxfr", "-d", argp->map_parms.domain,
		      "-C",tid, prog, ipadd, port, argp->map_parms.map, NULL);
		exit(1);
	}
	
	if (!svc_sendreply(transp, xdr_void, (char *) &res)) {
		svcerr_systemerr(transp);
	}

	if (!svc_freeargs(transp, xdr_ypreq_xfr, (caddr_t) argp)) {
		(void)fprintf(stderr, "unable to free arguments\n");
		exit(1);
	}

	return (&res);
}

void *
ypproc_clear_2_svc(argp, rqstp, transp)
	void *argp;
        struct svc_req *rqstp;
	SVCXPRT *transp;
{
	static char res;

	bzero((char *)&res, sizeof(res));
	
#ifdef OPTDB
	yplog_date("ypproc_clear_2: DB open/close optimization");
#else
	yplog_date("ypproc_clear_2: No optimization");
#endif
	yplog_call(transp);
	
#ifdef OPTDB
        ypdb_close_all();
#endif

	if (!svc_sendreply(transp, xdr_void, (char *) &res)) {
		svcerr_systemerr(transp);
	}
	
	if (!svc_freeargs(transp, xdr_void, (caddr_t) argp)) {
		(void)fprintf(stderr, "unable to free arguments\n");
		exit(1);
	}
	
	return ((void *)&res);
}

ypresp_all *
ypproc_all_2_svc(argp, rqstp, transp)
	ypreq_nokey *argp;
        struct svc_req *rqstp;
	SVCXPRT *transp;
{
	static ypresp_all res;
	pid_t pid;

	bzero((char *)&res, sizeof(res));
	
#ifdef DEBUG
	yplog_date("ypproc_all_2:");
	yplog_call(transp);
	yplog_str("  domain: "); yplog_cat(argp->domain); yplog_cat("\n");
	yplog_str("     map: "); yplog_cat(argp->map); yplog_cat("\n");
	yplog_cat("\n");
#endif
	
	pid = fork();

	if (pid) {

	  if (pid == -1) {
	    /* An error has occurred */
	  }

	  return(&res);
	  
	}

	if (!svc_sendreply(transp, ypdb_xdr_get_all, (char *) argp)) {
		svcerr_systemerr(transp);
	}

	if (!svc_freeargs(transp, xdr_ypreq_nokey, (caddr_t) argp)) {
		(void)fprintf(stderr, "unable to free arguments\n");
		exit(1);
	}
	
	exit(0);
}

ypresp_master *
ypproc_master_2_svc(argp, rqstp, transp)
	ypreq_nokey *argp;
        struct svc_req *rqstp;
	SVCXPRT *transp;
{
	static ypresp_master res;
	static peername nopeer = "";

	bzero((char *)&res, sizeof(res));
	
#ifdef DEBUG
	yplog_date("ypproc_master_2:");
	yplog_call(transp);
	yplog_str("  domain: "); yplog_cat(argp->domain); yplog_cat("\n");
	yplog_str("     map: "); yplog_cat(argp->map); yplog_cat("\n");
	yplog_cat("\n");
#endif

	if (acl_access_ok) {
	  res = ypdb_get_master(argp->domain,argp->map);
	} else {
	  res.stat = YP_NODOM;
	}

#ifdef DEBUG
	yplog_str("  status: ");
	yplog_cat(yperr_string(ypprot_err(res.stat)));
	yplog_cat("\n");
#endif

	/* This code was added because a yppoll <unknown-domain> */
	/* from a sun crashed the server in xdr_string, trying   */
	/* to access the peer through a NULL-pointer. yppoll in  */
	/* NetBSD start asking for order. If order is ok then it */
	/* will ask for master. SunOS 4 asks for both always.    */
	/* I'm not sure this is the best place for the fix, but  */
	/* for now it will do. xdr_peername or xdr_string in     */
	/* ypserv_xdr.c may be a better place?                   */
	
	if (res.peer == NULL) {
	  res.peer = nopeer;
	}

	/* End of fix                                            */

	if (!svc_sendreply(transp, xdr_ypresp_master, (char *) &res)) {
		svcerr_systemerr(transp);
	}

	if (!svc_freeargs(transp, xdr_ypreq_nokey, (caddr_t) argp)) {
		(void)fprintf(stderr, "unable to free arguments\n");
		exit(1);
	}
	
	return (&res);
}


ypresp_order *
ypproc_order_2_svc(argp, rqstp, transp)
	ypreq_nokey *argp;
        struct svc_req *rqstp;
	SVCXPRT *transp;
{
	static ypresp_order res;

	bzero((char *)&res, sizeof(res));
	
#ifdef DEBUG
	yplog_date("ypproc_order_2:");
	yplog_call(transp);
	yplog_str("  domain: "); yplog_cat(argp->domain); yplog_cat("\n");
	yplog_str("     map: "); yplog_cat(argp->map); yplog_cat("\n");
	yplog_cat("\n");
#endif

	if (acl_access_ok) {
	  res = ypdb_get_order(argp->domain,argp->map);
	} else {
	  res.stat = YP_NODOM;
	}

#ifdef DEBUG
	yplog_str("  status: ");
	yplog_cat(yperr_string(ypprot_err(res.stat)));
	yplog_cat("\n");
#endif

	if (!svc_sendreply(transp, xdr_ypresp_order, (char *) &res)) {
		svcerr_systemerr(transp);
	}

	if (!svc_freeargs(transp, xdr_ypreq_nokey, (caddr_t) argp)) {
		(void)fprintf(stderr, "unable to free arguments\n");
		exit(1);
	}
	
	return (&res);
}


ypresp_maplist *
ypproc_maplist_2_svc(argp, rqstp, transp)
	domainname *argp;
        struct svc_req *rqstp;
	SVCXPRT *transp;
{
	static ypresp_maplist res;
	static char domain_path[255];
	struct stat finfo;
	DIR   *dirp = NULL;
	struct dirent *dp;
	char  *suffix;
	ypstat status;
	struct ypmaplist *m;
	char  *map_name;

	bzero((char *)&res, sizeof(res));
	
#ifdef DEBUG
	yplog_date("ypproc_maplist_2:");
	yplog_call(transp);
	yplog_str("  domain: "); yplog_cat(*argp); yplog_cat("\n");
#endif

	sprintf(domain_path,"%s/%s",YP_DB_PATH,*argp);

	status = YP_TRUE;

	res.maps = NULL;

	if (acl_access_ok) {
	  if (!((stat(domain_path, &finfo) == 0) &&
		((finfo.st_mode & S_IFMT) == S_IFDIR))) {
	    status = YP_NODOM;
	  }
	} else {
	  status = YP_NODOM;
	}

	if (status >= 0) {
	  if ((dirp = opendir(domain_path)) == NULL) {
	    status = YP_NODOM;
	  }
	}

	if (status >= 0) {
	  for(dp = readdir(dirp); dp != NULL; dp = readdir(dirp)) {
	    if ((!strcmp(dp->d_name, ".")) ||
		((!strcmp(dp->d_name, ".."))) ||
		(dp->d_namlen < 4))
	      continue;
	    suffix = (char *) &dp->d_name[dp->d_namlen-3];
	    if (strcmp(suffix,".db") == 0) {

	      if ((m = (struct ypmaplist *)
		   malloc((unsigned) sizeof(struct ypmaplist))) == NULL) {
		status = YP_YPERR;
		break;
	      }

	      if ((map_name = (char *)
		   malloc((unsigned) dp->d_namlen - 2)) == NULL) {
		status = YP_YPERR;
		break;
	      }

	      m->next = res.maps;
	      m->map = map_name;
	      res.maps = m;
	      strncpy(map_name, dp->d_name, dp->d_namlen - 3);
	      m->map[dp->d_namlen - 3] = '\0';
	      
	    }
	  }
	}
	
	if (dirp != NULL) {
	  closedir(dirp);
	}

	res.stat = status;
	
#ifdef DEBUG
	yplog_str("  status: ");
	yplog_cat(yperr_string(ypprot_err(res.stat)));
	yplog_cat("\n");
#endif

	if (!svc_sendreply(transp, xdr_ypresp_maplist, (char *) &res)) {
		svcerr_systemerr(transp);
	}
	
	if (!svc_freeargs(transp, xdr_domainname, (caddr_t) argp)) {
		(void)fprintf(stderr, "unable to free arguments\n");
		exit(1);
	}

	return (&res);
}

