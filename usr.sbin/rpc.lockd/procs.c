/*	$OpenBSD: procs.c,v 1.8 1998/07/10 08:06:52 deraadt Exp $	*/

/*
 * Copyright (c) 1995
 *	A.R. Gordon (andrew.gordon@net-tel.co.uk).  All rights reserved.
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
 *	This product includes software developed for the FreeBSD project
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ANDREW GORDON AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpcsvc/sm_inter.h>
#include "nlm_prot.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

#include "lockd.h"

#define	CLIENT_CACHE_SIZE	64	/* No. of client sockets cached	 */
#define	CLIENT_CACHE_LIFETIME	120	/* In seconds			 */

static void 
log_from_addr(fun_name, req)
	char *fun_name;
	struct svc_req *req;
{
	struct	sockaddr_in *addr;
	struct	hostent *host;
	char	hostname_buf[MAXHOSTNAMELEN];

	addr = svc_getcaller(req->rq_xprt);
	host = gethostbyaddr((char *) &(addr->sin_addr), addr->sin_len, AF_INET);
	if (host) {
		strncpy(hostname_buf, host->h_name, sizeof(hostname_buf) - 1);
		hostname_buf[sizeof(hostname_buf) - 1] = '\0';
	} else
		strcpy(hostname_buf, inet_ntoa(addr->sin_addr));
	syslog(LOG_DEBUG, "%s from %s", fun_name, hostname_buf);
}


static CLIENT *clnt_cache_ptr[CLIENT_CACHE_SIZE];
static long clnt_cache_time[CLIENT_CACHE_SIZE];	/* time entry created	 */
static struct in_addr clnt_cache_addr[CLIENT_CACHE_SIZE];
static int clnt_cache_next_to_use = 0;

static CLIENT *
get_client(host_addr)
	struct sockaddr_in *host_addr;
{
	CLIENT *client;
	int     sock_no, i;
	struct timeval retry_time, time_now;

	gettimeofday(&time_now, NULL);

	/* Search for the given client in the cache, zapping any expired	 */
	/* entries that we happen to notice in passing.			 */
	for (i = 0; i < CLIENT_CACHE_SIZE; i++) {
		client = clnt_cache_ptr[i];
		if (client &&
		    ((clnt_cache_time[i] + CLIENT_CACHE_LIFETIME) < time_now.tv_sec)) {
			/* Cache entry has expired. */
			if (debug_level > 3)
				syslog(LOG_DEBUG, "Expired CLIENT* in cache");
			clnt_cache_time[i] = 0L;
			clnt_destroy(client);
			clnt_cache_ptr[i] = NULL;
			client = NULL;
		}
		if (client && !memcmp(&clnt_cache_addr[i], &host_addr->sin_addr,
			sizeof(struct in_addr))) {
			/* Found it! */
			if (debug_level > 3)
				syslog(LOG_DEBUG, "Found CLIENT* in cache");
			return (client);
		}
	}

	/* Not found in cache.  Free the next entry if it is in use */
	if (clnt_cache_ptr[clnt_cache_next_to_use]) {
		clnt_destroy(clnt_cache_ptr[clnt_cache_next_to_use]);
		clnt_cache_ptr[clnt_cache_next_to_use] = NULL;
	}

	sock_no = RPC_ANYSOCK;
	retry_time.tv_sec = 5;
	retry_time.tv_usec = 0;
	host_addr->sin_port = 0;
	client = clntudp_create(host_addr, NLM_PROG, NLM_VERS, retry_time, &sock_no);
	if (!client) {
		syslog(LOG_ERR, clnt_spcreateerror("clntudp_create"));
		syslog(LOG_ERR, "Unable to return result to %s",
		    inet_ntoa(host_addr->sin_addr));
		return (NULL);
	}
	clnt_cache_ptr[clnt_cache_next_to_use] = client;
	clnt_cache_addr[clnt_cache_next_to_use] = host_addr->sin_addr;
	clnt_cache_time[clnt_cache_next_to_use] = time_now.tv_sec;
	if (++clnt_cache_next_to_use > CLIENT_CACHE_SIZE)
		clnt_cache_next_to_use = 0;

	retry_time.tv_sec = -1;
	retry_time.tv_usec = -1;
	clnt_control(client, CLSET_TIMEOUT, &retry_time);

	if (debug_level > 3)
		syslog(LOG_DEBUG, "Created CLIENT* for %s",
		    inet_ntoa(host_addr->sin_addr));
	return (client);
}


static void 
transmit_result(opcode, result, req)
	int opcode;
	nlm_res *result;
	struct svc_req *req;
{
	static char dummy;
	struct sockaddr_in *addr;
	CLIENT *cli;
	int     success;
	struct timeval timeo;

	addr = svc_getcaller(req->rq_xprt);
	if ((cli = get_client(addr))) {
		timeo.tv_sec = 0;
		timeo.tv_usec = 0;

		success = clnt_call(cli, opcode, xdr_nlm_res, result, xdr_void,
		    &dummy, timeo);
		if (debug_level > 2)
			syslog(LOG_DEBUG, "clnt_call returns %d", success);
	}
}

nlm_testres *
nlm_test_1_svc(arg, rqstp)
	nlm_testargs *arg;
	struct svc_req *rqstp;
{
	static nlm_testres res;

	if (debug_level)
		log_from_addr("nlm_test", rqstp);
	res.cookie = arg->cookie;
	res.stat.stat = nlm_granted;
	return (&res);
}

void *
nlm_test_msg_1_svc(arg, rqstp)
	nlm_testargs *arg;
	struct svc_req *rqstp;
{
	nlm_testres res;
	static char dummy;
	struct sockaddr_in *addr;
	CLIENT *cli;
	int     success;
	struct timeval timeo;

	if (debug_level)
		log_from_addr("nlm_test_msg", rqstp);

	res.cookie = arg->cookie;
	res.stat.stat = nlm_granted;

	addr = svc_getcaller(rqstp->rq_xprt);
	if ((cli = get_client(addr))) {
		timeo.tv_sec = 0;
		timeo.tv_usec = 0;
		success = clnt_call(cli, NLM_TEST_RES, xdr_nlm_testres, &res, xdr_void,
		    &dummy, timeo);
		if (debug_level > 2)
			syslog(LOG_DEBUG, "clnt_call returns %d", success);
	}
	return (NULL);
}

nlm_res *
nlm_lock_1_svc(arg, rqstp)
	nlm_lockargs *arg;
	struct svc_req *rqstp;
{
	static nlm_res res;

	if (debug_level)
		log_from_addr("nlm_lock", rqstp);
	res.cookie = arg->cookie;
	res.stat.stat = nlm_granted;
	return (&res);
}

void *
nlm_lock_msg_1_svc(arg, rqstp)
	nlm_lockargs *arg;
	struct svc_req *rqstp;
{
	static nlm_res res;

	if (debug_level)
		log_from_addr("nlm_lock_msg", rqstp);
	res.cookie = arg->cookie;
	res.stat.stat = nlm_granted;
	transmit_result(NLM_LOCK_RES, &res, rqstp);
	return (NULL);
}

nlm_res *
nlm_cancel_1_svc(arg, rqstp)
	nlm_cancargs *arg;
	struct svc_req *rqstp;
{
	static nlm_res res;

	if (debug_level)
		log_from_addr("nlm_cancel", rqstp);
	res.cookie = arg->cookie;
	res.stat.stat = nlm_denied;
	return (&res);
}

void *
nlm_cancel_msg_1_svc(arg, rqstp)
	nlm_cancargs *arg;
	struct svc_req *rqstp;
{
	static nlm_res res;

	if (debug_level)
		log_from_addr("nlm_cancel_msg", rqstp);
	res.cookie = arg->cookie;
	res.stat.stat = nlm_denied;
	transmit_result(NLM_CANCEL_RES, &res, rqstp);
	return (NULL);
}

nlm_res *
nlm_unlock_1_svc(arg, rqstp)
	nlm_unlockargs *arg;
	struct svc_req *rqstp;
{
	static nlm_res res;

	if (debug_level)
		log_from_addr("nlm_unlock", rqstp);
	res.stat.stat = nlm_granted;
	res.cookie = arg->cookie;
	return (&res);
}

void *
nlm_unlock_msg_1_svc(arg, rqstp)
	nlm_unlockargs *arg;
	struct svc_req *rqstp;
{
	static nlm_res res;

	if (debug_level)
		log_from_addr("nlm_unlock_msg", rqstp);
	res.stat.stat = nlm_granted;
	res.cookie = arg->cookie;
	transmit_result(NLM_UNLOCK_RES, &res, rqstp);
	return (NULL);
}

nlm_res *
nlm_granted_1_svc(arg, rqstp)
	nlm_testargs *arg;
	struct svc_req *rqstp;
{
	static nlm_res res;

	if (debug_level)
		log_from_addr("nlm_granted", rqstp);
	res.cookie = arg->cookie;
	res.stat.stat = nlm_granted;
	return (&res);
}

void *
nlm_granted_msg_1_svc(arg, rqstp)
	nlm_testargs *arg;
	struct svc_req *rqstp;
{
	nlm_res res;

	if (debug_level)
		log_from_addr("nlm_granted_msg", rqstp);
	res.cookie = arg->cookie;
	res.stat.stat = nlm_granted;
	transmit_result(NLM_GRANTED_RES, &res, rqstp);
	return (NULL);
}

void *
nlm_test_res_1_svc(arg, rqstp)
	nlm_testres *arg;
	struct svc_req *rqstp;
{
	if (debug_level)
		log_from_addr("nlm_test_res", rqstp);
	return (NULL);
}

void *
nlm_lock_res_1_svc(arg, rqstp)
	nlm_res *arg;
	struct svc_req *rqstp;
{
	if (debug_level)
		log_from_addr("nlm_lock_res", rqstp);

	return (NULL);
}

void *
nlm_cancel_res_1_svc(arg, rqstp)
        nlm_res *arg;
        struct svc_req *rqstp;
{
	if (debug_level)
		log_from_addr("nlm_cancel_res", rqstp);
	return (NULL);
}

void *
nlm_unlock_res_1_svc(arg, rqstp)
	nlm_res *arg;
	struct svc_req *rqstp;
{
	if (debug_level)
		log_from_addr("nlm_unlock_res", rqstp);
	return (NULL);
}

void *
nlm_granted_res_1_svc(arg, rqstp)
	nlm_res *arg;
	struct svc_req *rqstp;
{
	if (debug_level)
		log_from_addr("nlm_granted_res", rqstp);
	return (NULL);
}

nlm_shareres *
nlm_share_3_svc(arg, rqstp)
	nlm_shareargs *arg;
	struct svc_req *rqstp;
{
	static nlm_shareres res;

	if (debug_level)
		log_from_addr("nlm_share", rqstp);
	res.cookie = arg->cookie;
	res.stat = nlm_granted;
	res.sequence = 1234356;	/* X/Open says this field is ignored?	 */
	return (&res);
}

nlm_shareres *
nlm_unshare_3_svc(arg, rqstp)
	nlm_shareargs *arg;
	struct svc_req *rqstp;
{
	static nlm_shareres res;

	if (debug_level)
		log_from_addr("nlm_unshare", rqstp);
	res.cookie = arg->cookie;
	res.stat = nlm_granted;
	res.sequence = 1234356;	/* X/Open says this field is ignored?	 */
	return (&res);
}

nlm_res *
nlm_nm_lock_3_svc(arg, rqstp)
	nlm_lockargs *arg;
	struct svc_req *rqstp;
{
	static nlm_res res;

	if (debug_level)
		log_from_addr("nlm_nm_lock", rqstp);
	res.cookie = arg->cookie;
	res.stat.stat = nlm_granted;
	return (&res);
}

void *
nlm_free_all_3_svc(arg, rqstp)
	nlm_notify *arg;
	struct svc_req *rqstp;
{
	static char dummy;

	if (debug_level)
		log_from_addr("nlm_free_all", rqstp);
	return (&dummy);
}
