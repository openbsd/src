/*	$OpenBSD: ypserv_proc.c,v 1.5 1996/08/15 21:47:30 chuck Exp $ */

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
static char rcsid[] = "$OpenBSD: ypserv_proc.c,v 1.5 1996/08/15 21:47:30 chuck Exp $";
#endif

#include <rpc/rpc.h>
#include "yp.h"
#include <rpcsvc/ypclnt.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/param.h>
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

#ifdef DEBUG
#define YPLOG yplog
#else /* DEBUG */
#define YPLOG if (!ok) yplog
#endif /* DEBUG */

extern ypresp_val ypdb_get_record();
extern ypresp_key_val ypdb_get_first();
extern ypresp_key_val ypdb_get_next();
extern ypresp_order ypdb_get_order();
extern ypresp_master ypdb_get_master();
extern bool_t ypdb_xdr_get_all();
extern void ypdb_close_all();

static char *True = "true";
static char *False = "FALSE";
#define TORF(N) ((N) ? True : False)
void *
ypproc_null_2_svc(argp, rqstp)
	void *argp;
        struct svc_req *rqstp;
{
	static char *result;
	struct sockaddr_in *caller = svc_getcaller(rqstp->rq_xprt);
	int ok = acl_check_host(&caller->sin_addr);

	YPLOG("null_2: caller=[%s].%d, auth_ok=%s",
	  inet_ntoa(caller->sin_addr), ntohs(caller->sin_port), TORF(ok));

	if (!ok) {
		svcerr_auth(rqstp->rq_xprt, AUTH_FAILED);
		return(NULL);
	}

	result = NULL;

	return ((void *)&result);
}

bool_t *
ypproc_domain_2_svc(argp, rqstp)
	domainname *argp;
        struct svc_req *rqstp;
{
	static bool_t result; /* is domain_served? */
	struct sockaddr_in *caller = svc_getcaller(rqstp->rq_xprt);
	int ok = acl_check_host(&caller->sin_addr);
	static char domain_path[MAXPATHLEN];
	struct stat finfo;

	snprintf(domain_path, sizeof(domain_path), "%s/%s", YP_DB_PATH, *argp);
	result = (bool_t) ((stat(domain_path, &finfo) == 0) &&
				    (finfo.st_mode & S_IFDIR));

	YPLOG("domain_2: caller=[%s].%d, auth_ok=%s, domain=%s, served=%s",
	  inet_ntoa(caller->sin_addr), ntohs(caller->sin_port), 
	  TORF(ok), *argp, TORF(result));

	if (!ok) {
		svcerr_auth(rqstp->rq_xprt, AUTH_FAILED);
		return(NULL);
	}

	return (&result);
}

bool_t *
ypproc_domain_nonack_2_svc(argp, rqstp)
	domainname *argp;
        struct svc_req *rqstp;
{
	static bool_t result; /* is domain served? */
	struct sockaddr_in *caller = svc_getcaller(rqstp->rq_xprt);
	int ok = acl_check_host(&caller->sin_addr);
	static char domain_path[MAXPATHLEN];
	struct stat finfo;

	snprintf(domain_path, sizeof(domain_path), "%s/%s", YP_DB_PATH, *argp);
	result = (bool_t) ((stat(domain_path, &finfo) == 0) &&
				    (finfo.st_mode & S_IFDIR));

	YPLOG(
	  "domain_nonack_2: caller=[%s].%d, auth_ok=%s, domain=%s, served=%s",
	  inet_ntoa(caller->sin_addr), ntohs(caller->sin_port), TORF(ok), 
	  *argp, TORF(result));

	if (!ok) {
		svcerr_auth(rqstp->rq_xprt, AUTH_FAILED);
		return(NULL);
	}

	if (!result) {
		return(NULL); /* don't send nack */
	}

	return (&result);
}

ypresp_val *
ypproc_match_2_svc(argp, rqstp)
	ypreq_key *argp;
        struct svc_req *rqstp;
{
	static ypresp_val res;
	struct sockaddr_in *caller = svc_getcaller(rqstp->rq_xprt);
	int ok = acl_check_host(&caller->sin_addr);

	YPLOG(
	  "match_2: caller=[%s].%d, auth_ok=%s, domain=%s, map=%s, key=%.*s",
	  inet_ntoa(caller->sin_addr), ntohs(caller->sin_port), TORF(ok), 
	  argp->domain, argp->map, argp->key.keydat_len, argp->key.keydat_val);

	if (!ok) {
		svcerr_auth(rqstp->rq_xprt, AUTH_FAILED);
		return(NULL);
	}

	res = ypdb_get_record(argp->domain,argp->map,argp->key, FALSE);
	
#ifdef DEBUG
	yplog("  match2_status: %s", yperr_string(ypprot_err(res.stat)));
#endif

	return (&res);
}

ypresp_key_val *
ypproc_first_2_svc(argp, rqstp)
	ypreq_nokey *argp;
        struct svc_req *rqstp;
{
	static ypresp_key_val res;
	struct sockaddr_in *caller = svc_getcaller(rqstp->rq_xprt);
	int ok = acl_check_host(&caller->sin_addr);

	YPLOG( "first_2: caller=[%s].%d, auth_ok=%s, domain=%s, map=%s",
	  inet_ntoa(caller->sin_addr), ntohs(caller->sin_port), TORF(ok), 
	  argp->domain, argp->map);
	
	if (!ok) {
		svcerr_auth(rqstp->rq_xprt, AUTH_FAILED);
		return(NULL);
	}

	res = ypdb_get_first(argp->domain,argp->map,FALSE);

#ifdef DEBUG
	yplog("  first2_status: %s", yperr_string(ypprot_err(res.stat)));
#endif

	return (&res);
}

ypresp_key_val *
ypproc_next_2_svc(argp, rqstp)
	ypreq_key *argp;
        struct svc_req *rqstp;
{
	static ypresp_key_val res;
	struct sockaddr_in *caller = svc_getcaller(rqstp->rq_xprt);
	int ok = acl_check_host(&caller->sin_addr);

	YPLOG(
	  "next_2: caller=[%s].%d, auth_ok=%s, domain=%s, map=%s, key=%.*s",
	  inet_ntoa(caller->sin_addr), ntohs(caller->sin_port), TORF(ok), 
	  argp->domain, argp->map, argp->key.keydat_len, argp->key.keydat_val);

	if (!ok) {
		svcerr_auth(rqstp->rq_xprt, AUTH_FAILED);
		return(NULL);
	}

	res = ypdb_get_next(argp->domain,argp->map,argp->key,FALSE);


#ifdef DEBUG
	yplog("  next2_status: %s", yperr_string(ypprot_err(res.stat)));
#endif

	return (&res);
}

ypresp_xfr *
ypproc_xfr_2_svc(argp, rqstp)
	ypreq_xfr *argp;
        struct svc_req *rqstp;
{
	static ypresp_xfr res;
	struct sockaddr_in *caller = svc_getcaller(rqstp->rq_xprt);
	int ok = acl_check_host(&caller->sin_addr);
	pid_t	pid;
	char	tid[11];
	char	prog[11];
	char	port[11];
	char	ypxfr_proc[] = YPXFR_PROC;
	char	*ipadd;

	bzero((char *)&res, sizeof(res));
	
	YPLOG("xfr_2: caller=[%s].%d, auth_ok=%s, domain=%s, tid=%d, prog=%d",
	  inet_ntoa(caller->sin_addr), ntohs(caller->sin_port), TORF(ok), 
	  argp->map_parms.domain, argp->transid, argp->prog);
	YPLOG("       ipadd=%s, port=%d, map=%s", inet_ntoa(caller->sin_addr),
	  argp->port, argp->map_parms.map);

	if (!ok) {
		svcerr_auth(rqstp->rq_xprt, AUTH_FAILED);
		return(NULL);
	}

	pid = vfork();

	if (pid == -1) {
		svcerr_systemerr(rqstp->rq_xprt);
		return(NULL);
		
	}

	if (pid == 0) {
		
		snprintf(tid, sizeof(tid), "%d",argp->transid);
		snprintf(prog, sizeof(prog), "%d", argp->prog);
		snprintf(port, sizeof(port), "%d", argp->port);
		ipadd = inet_ntoa(caller->sin_addr);

		execl(ypxfr_proc, "ypxfr", "-d", argp->map_parms.domain,
		      "-C",tid, prog, ipadd, port, argp->map_parms.map, NULL);
		exit(1);
	}
	
	/*
	 * XXX: fill in res
	 */

	return (&res);
}

void *
ypproc_clear_2_svc(argp, rqstp)
	void *argp;
        struct svc_req *rqstp;
{
	static char *res;
	struct sockaddr_in *caller = svc_getcaller(rqstp->rq_xprt);
	int ok = acl_check_host(&caller->sin_addr);

	YPLOG( "clear_2: caller=[%s].%d, auth_ok=%s, opt=%s",
	  inet_ntoa(caller->sin_addr), ntohs(caller->sin_port), TORF(ok),
#ifdef OPTDB
		True
#else
		False
#endif
	);

	if (!ok) {
		svcerr_auth(rqstp->rq_xprt, AUTH_FAILED);
		return(NULL);
	}

	res = NULL;
	
#ifdef OPTDB
        ypdb_close_all();
#endif

	return ((void *)&res);
}

ypresp_all *
ypproc_all_2_svc(argp, rqstp)
	ypreq_nokey *argp;
        struct svc_req *rqstp;
{
	static ypresp_all res;
	pid_t pid;
	struct sockaddr_in *caller = svc_getcaller(rqstp->rq_xprt);
	int ok = acl_check_host(&caller->sin_addr);

	YPLOG( "all_2: caller=[%s].%d, auth_ok=%s, domain=%s, map=%s",
	  inet_ntoa(caller->sin_addr), ntohs(caller->sin_port), TORF(ok),
	  argp->domain, argp->map);

	if (!ok) {
		svcerr_auth(rqstp->rq_xprt, AUTH_FAILED);
		return(NULL);
	}

	bzero((char *)&res, sizeof(res));
	
	pid = fork();

	if (pid) {

	  if (pid == -1) {
	    /* XXXCDC An error has occurred */
	  }

	  return(NULL); /* PARENT: continue */
	  
	}
	/* CHILD: send result, then exit */

	if (!svc_sendreply(rqstp->rq_xprt, ypdb_xdr_get_all, (char *) argp)) {
		svcerr_systemerr(rqstp->rq_xprt);
	}

	/* note: no need to free args, we are exiting */

	exit(0);
}

ypresp_master *
ypproc_master_2_svc(argp, rqstp)
	ypreq_nokey *argp;
        struct svc_req *rqstp;
{
	static ypresp_master res;
	static peername nopeer = "";
	struct sockaddr_in *caller = svc_getcaller(rqstp->rq_xprt);
	int ok = acl_check_host(&caller->sin_addr);

	YPLOG( "master_2: caller=[%s].%d, auth_ok=%s, domain=%s, map=%s",
	  inet_ntoa(caller->sin_addr), ntohs(caller->sin_port), TORF(ok),
	  argp->domain, argp->map);

	if (!ok) {
		svcerr_auth(rqstp->rq_xprt, AUTH_FAILED);
		return(NULL);
	}

	res = ypdb_get_master(argp->domain,argp->map);

#ifdef DEBUG
	yplog("  master2_status: %s", yperr_string(ypprot_err(res.stat)));
#endif

	/* This code was added because a yppoll <unknown-domain> */
	/* from a sun crashed the server in xdr_string, trying   */
	/* to access the peer through a NULL-pointer. yppoll in  */
	/* this server start asking for order. If order is ok    */
	/* then it will ask for master. SunOS 4 asks for both    */
	/* always. I'm not sure this is the best place for the   */
	/* fix, but for now it will do. xdr_peername or          */
	/* xdr_string in ypserv_xdr.c may be a better place?     */
	
	if (res.peer == NULL) {
	  res.peer = nopeer;
	}

	/* End of fix                                            */

	return (&res);
}


ypresp_order *
ypproc_order_2_svc(argp, rqstp)
	ypreq_nokey *argp;
        struct svc_req *rqstp;
{
	static ypresp_order res;
	struct sockaddr_in *caller = svc_getcaller(rqstp->rq_xprt);
	int ok = acl_check_host(&caller->sin_addr);

	YPLOG( "order_2: caller=[%s].%d, auth_ok=%s, domain=%s, map=%s",
	  inet_ntoa(caller->sin_addr), ntohs(caller->sin_port), TORF(ok),
	  argp->domain, argp->map);

	if (!ok) {
		svcerr_auth(rqstp->rq_xprt, AUTH_FAILED);
		return(NULL);
	}

	res = ypdb_get_order(argp->domain,argp->map);

#ifdef DEBUG
	yplog("  order2_status: %s", yperr_string(ypprot_err(res.stat)));
#endif

	return (&res);
}


ypresp_maplist *
ypproc_maplist_2_svc(argp, rqstp)
	domainname *argp;
        struct svc_req *rqstp;
{
	static ypresp_maplist res;
	struct sockaddr_in *caller = svc_getcaller(rqstp->rq_xprt);
	int ok = acl_check_host(&caller->sin_addr);
	static char domain_path[MAXPATHLEN];
	struct stat finfo;
	DIR   *dirp = NULL;
	struct dirent *dp;
	char  *suffix;
	ypstat status;
	struct ypmaplist *m;
	char  *map_name;

	YPLOG("maplist_2: caller=[%s].%d, auth_ok=%s, domain=%s",
	  inet_ntoa(caller->sin_addr), ntohs(caller->sin_port), TORF(ok),
	  *argp);

	if (!ok) {
		svcerr_auth(rqstp->rq_xprt, AUTH_FAILED);
		return(NULL);
	}

	bzero((char *)&res, sizeof(res));
	
	snprintf(domain_path,MAXPATHLEN, "%s/%s",YP_DB_PATH,*argp);

	status = YP_TRUE;

	res.maps = NULL;

	if (!((stat(domain_path, &finfo) == 0) &&
		((finfo.st_mode & S_IFMT) == S_IFDIR))) 
		status = YP_NODOM;

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
	yplog("  maplist_status: %s", yperr_string(ypprot_err(res.stat)));
#endif

	return (&res);
}

