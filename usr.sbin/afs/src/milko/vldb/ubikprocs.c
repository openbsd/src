/*
 * Copyright (c) 1999 Kungliga Tekniska Högskolan
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

#include "ubik.ss.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#ifdef KERBEROS
#include <krb.h>
#endif

RCSID("$arla: ubikprocs.c,v 1.3 2000/10/03 00:21:02 lha Exp $");

int Ubik_Beacon(struct rx_call *call, 
		const int32_t state, 
		const int32_t voteStart, 
		const net_version *Version, 
		const struct net_tid *tid)
{
    return -1;
}

int Ubik_Debug(struct rx_call *call, 
	       struct ubik_debug *db)
{
#ifdef KERBEROS
    {
	struct in_addr *ina;
	int ret;

	ret = k_get_all_addrs(&ina);
	if (ret < 1)
	    return -1;
	db->syncHost = ntohl(ina[0].s_addr); /* XXX */
    }
#else
    {
	char name[MAXHOSTNAMELEN];
	struct hostent *he;
	struct in_addr tmp;

	if (gethostname (name, sizeof(name) < 0))
	    return -1;
	he = gethostbyname (name);
	if (he == NULL)
	    return -1;
	memcpy (&tmp, he->h_addr_list[0], sizeof(struct in_addr));
	db->syncHost = ntohl(tmp.s_addr);
    }
#endif

    return 0;
}

int Ubik_SDebug(struct rx_call *call, 
		const int32_t which, 
		struct ubik_sdebug *db)
{
    return -1;
}

int Ubik_GetSyncSite(struct rx_call *call, 
		     int32_t *site)
{
    return -1;
}
