/*
 * Copyright (c) 1998 - 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
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
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Implementation of resolver api using roken resolve.h */

#include "ko_locl.h"

#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif

#include "resolve.h"

RCSID("$arla: resolve.c,v 1.4 2002/07/24 08:46:48 lha Exp $");

void
_ko_resolve_init(void)
{
#ifdef HAVE_RES_INIT
    res_init();
#endif

}


int
_ko_resolve_cell(const char *cell, cell_db_entry *dbservers, int max_num, 
		 int *ret_num, int *lowest_ttl)
{
    struct dns_reply *r;
    struct resource_record *rr;
    int i, dbnum;

    r = dns_lookup(cell, "AFSDB");
    if (r == NULL)
	return 1;

    dbnum = 0;
    for(rr = r->head; rr;rr=rr->next){
	if(rr->type == T_AFSDB) {
	    struct mx_record *mx = (struct mx_record*)rr->u.data;

	    if (dbnum >= max_num)
	        break;

	    if (strcasecmp(cell, rr->domain) != 0)
		continue;

	    if (mx->preference != 1)
		continue;

	    if (*lowest_ttl > rr->ttl)
		*lowest_ttl = rr->ttl;
	    dbservers[dbnum].name = strdup (mx->domain);
	    if (dbservers[dbnum].name == NULL)
		err (1, "strdup");
	    dbservers[dbnum].timeout = CELL_INVALID_HOST;
	    dbnum++;
	}
    }
    for(rr = r->head; rr;rr=rr->next){
	if (rr->type == T_A) {
	    for (i = 0; i < dbnum; i++) {
		if (strcasecmp(dbservers[i].name,rr->domain) != 0)
		    continue;
		dbservers[i].addr = *(rr->u.a);
		dbservers[i].timeout = rr->ttl;
		break;
	    }
	}
    }
    dns_free_data(r);

    *ret_num = dbnum;

    return 0;
}

int
_ko_resolve_host(const char *name, cell_db_entry *host)
{
    struct resource_record *rr;
    struct dns_reply *r;
    int ret;

    r = dns_lookup(host->name, "A");
    if (r == NULL)
	return 1;

    ret = 1;
    for(rr = r->head; rr;rr=rr->next){
	if (rr->type == T_A && strcasecmp(host->name,rr->domain) == 0) {
	    host->addr = *(rr->u.a);
	    host->timeout = rr->ttl;
	    ret = 0;
	    break;
	}
    }
    dns_free_data(r);
    return ret;
}
