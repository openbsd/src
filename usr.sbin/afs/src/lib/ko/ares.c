/*
 * Copyright (c) 2002, Stockholms Universitet
 * (Stockholm University, Stockholm Sweden)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* Ares implementation of resolver api */

#include "ko_locl.h"
#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#include <ares.h>

RCSID("$arla: ares.c,v 1.1 2002/07/24 21:22:51 lha Exp $");

static ares_channel achannel;
static PROCESS ares_pid;

static void ares_worker_thread(char *);

void
_ko_resolve_init(void)
{
    char *errmem;
    int ret;

    IOMGR_Initialize();

    if (LWP_CreateProcess(ares_worker_thread, AFS_LWP_MINSTACKSIZE, 0, 0, 
			  "ares resolver daemon", &ares_pid))
	errx(1, "Couldn't initialize resolver, helper thread didn't start");

    /* XXX use ARES_FLAG_NOSEARCH */
    ret = ares_init(&achannel);
    if (ret != ARES_SUCCESS)
	errx(1, "Couldn't initialize resolver: %s", 
	     ares_strerror(ret, &errmem));
}

struct ko_dns_query {
    const char *domain;
    int flags;
#define QUERY_DONE	1
#define CELL_QUERY	2
#define HOST_QUERY	4
    cell_db_entry *dbservers;
    int max_num;
    int dbnum;
    int lowest_ttl;
    int error;
};

static void
ares_worker_thread(char *ptr)
{
    struct timeval tv, max_tv = { 30, 0 };
    fd_set readset, writeset;
    int nfds, ret;

    while (1) {
	
	FD_ZERO(&readset);
	FD_ZERO(&writeset);
	nfds = ares_fds(achannel, &readset, &writeset);
	if (nfds == 0) {
	    tv = max_tv;
	    IOMGR_Sleep(max_tv.tv_sec);
	} else {
	    struct timeval *tvp;

	    tvp = ares_timeout(achannel, &max_tv, &tv);
	    ret = IOMGR_Select(nfds, &readset, &writeset, NULL, tvp);
	    if (ret < 0)
		/* XXX some error, lets ignore that for now */;
	    else if (ret == 0)
		/* timeout */;
	    else 
		ares_process(achannel, &readset, &writeset);
	}
    }
}

static void
callback(void *arg, int status, unsigned char *abuf, int alen)
{
    struct ko_dns_query *q = arg;
    struct ares_record *rr0, *rr;
    int i;

    if (status != 0) {
	q->error = status;
	goto out;
    }

    q->error = ares_parse_reply(abuf, alen, &rr0);
    if (q->error)
	goto out;

    if (q->flags & CELL_QUERY) {
	for(rr = rr0; rr;rr=rr->next){
	    if(rr->type == T_AFSDB) {
		struct ares_mx_record *mx = rr->u.mx;
		
		if (q->dbnum >= q->max_num)
		    break;
		
		if (strcasecmp(q->domain, rr->domain) != 0)
		    continue;
		
		if (mx->preference != 1)
		    continue;
		
		if (q->lowest_ttl > rr->ttl)
		    q->lowest_ttl = rr->ttl;
		q->dbservers[q->dbnum].name = strdup (mx->domain);
		if (q->dbservers[q->dbnum].name == NULL)
		    err (1, "strdup");
		q->dbservers[q->dbnum].timeout = CELL_INVALID_HOST;
		q->dbnum++;
	    }
	}
    }

    for(rr = rr0; rr; rr = rr->next){
	if (rr->type == T_A) {
	    for (i = 0; i < q->dbnum; i++) {
		if (strcasecmp(q->dbservers[i].name, rr->domain) != 0)
		    continue;
		q->dbservers[i].addr = *(rr->u.a);
		q->dbservers[i].timeout = rr->ttl;
		if (q->flags & HOST_QUERY)
		    goto out_free;
		break;
	    }
	}
    }

 out_free:
    ares_free_reply(rr0);
 out:

    q->flags |= QUERY_DONE;
    LWP_NoYieldSignal(q);
}


static int
query(const char *domain, int type, int flags, 
      cell_db_entry *dbservers, int max_num, 
      int *ret_num, int *lowest_ttl)
{
    struct ko_dns_query q;

    q.domain = domain;
    q.flags = flags & (CELL_QUERY|HOST_QUERY);
    q.dbservers = dbservers;
    q.max_num = max_num;
    if (type & CELL_QUERY)
	q.dbnum = 0;
    else
	q.dbnum = max_num;
    if (lowest_ttl)
	q.lowest_ttl = *lowest_ttl;

    IOMGR_Cancel(ares_pid);
    
    ares_query(achannel, domain, C_IN, type, callback, &q);

    while((q.flags & QUERY_DONE) == 0)
	LWP_WaitProcess(&q);

    if (lowest_ttl)
	*lowest_ttl = q.lowest_ttl;
    if (ret_num)
	*ret_num = q.dbnum;
    return q.error;
}

int
_ko_resolve_cell(const char *cell, cell_db_entry *dbservers, int max_num, 
		 int *ret_num, int *lowest_ttl)
{
    return query(cell, T_AFSDB, CELL_QUERY, 
		 dbservers, max_num, ret_num, lowest_ttl);
}

int
_ko_resolve_host(const char *name, cell_db_entry *host)
{
    return query(name, T_A, HOST_QUERY, 
		 host, 1, NULL, NULL);
}
