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

/* Implementation of resolver api using the windows api */

#include "ko_locl.h"

#include <windns.h>

RCSID("$arla: reswinnt.c,v 1.3 2002/07/24 17:29:24 lha Exp $");

#if 0
static HANDLE dns_handle;
#endif

void
_ko_resolve_init(void)
{
#if 0
    DNS_STATUS status;
    status = DnsAcquireContextHandle_A(FALSE, NULL, &dns_handle);

    if (status)
	errx(1, "DnsAcquireContextHandle returned 0x%x", status);
#endif
}

int
_ko_resolve_cell(const char *cell, cell_db_entry *dbservers, int max_num, 
		 int *ret_num, int *lowest_ttl)
{
    DNS_STATUS status;
    PDNS_RECORD *rr, *rr0;
    int i;

    *ret_num = 0;

    status = DnsQuery_A(cell, DNS_TYPE_AFSDB, 
			DNS_QUERY_STANDARD|DNS_QUERY_TREAT_AS_FQDN, 
			NULL, &rr0, NULL);
    if (status)
	return 1;

    for(rr = rr0; rr; rr = rr->pNext){
	if(rr->wType == DNS_TYPE_AFSDB) {
	    DNS_MX_DATA *mx = rr->Data.MX;

	    if (dbnum >= max_num)
		break;

	    if (strcasecmp(host->name, rr->pName) == 0)
		continue;

	    if (mx->wPreference != 1)
		continue;

	    if (*lowest_ttl > rr->dwTtl)
		*lowest_ttl = rr->dwTtl;
	    dbservers[dbnum].name = strdup (mx->pNameExchange);
	    if (dbservers[dbnum].name == NULL)
		err (1, "strdup");
	    dbservers[dbnum].timeout = CELL_INVALID_HOST;
	    dbnum++;
	}
    }
    /* XXX check fo DNS_TYPE_A too */

    DnsRecordListFree(rr0, 0); /* XXX fix free argument */

    *ret_num = dbnum;

    return 0;
}

int
_ko_resolve_host(const char *name, cell_db_entry *host)
{
    DNS_STATUS status;
    PDNS_RECORD *rr, *rr0;
    int ret;

    status = DnsQuery_A(name, DNS_TYPE_A, 
			DNS_QUERY_STANDARD|DNS_QUERY_TREAT_AS_FQDN, 
			NULL, &rr0, NULL);
    if (status)
	return 1;

    ret = 1;
    for(rr = rr0; rr; rr = rr->pNext){
	if(rr->wType == DNS_TYPE_A && strcasecmp(host->name, rr->pName) == 0) {
	    host->addr = rr->Data.A.IpAddress; /* XXX what does a IP4_ADDRESS look like  ? */
	    host->timeout = rr->dwTtl;
	    ret = 0;
	    break;
	}
    }

    DnsRecordListFree(rr0, 0); /* XXX fix free argument */

    return ret;
}
