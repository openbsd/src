/*
 * Copyright (c) 1999 - 2000 Kungliga Tekniska Högskolan
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

#include "fsrv_locl.h"

#include "pts.cs.h"

RCSID("$KTH: connsec.c,v 1.12 2000/10/03 00:17:03 lha Exp $");

static struct rx_connection *
get_conn(int32_t addr, int32_t port, int32_t servid)
{
    struct rx_securityClass *sec;
    struct rx_connection *conn;

    sec = netinit_client_getcred();
    if (sec == NULL)
	return NULL;
    
    conn = rx_NewConnection (addr, 
			     htons (port), 
			     servid,
			     sec,
			     2 /* secureindex */);
    if (conn == NULL)
	return NULL;

    return conn;
}

static struct rx_connection *out_conn = NULL;

int
fs_connsec_nametoid(namelist *nlist, idlist *ilist)
{
    int error;
    int first = 0;

retry:
    if (out_conn == NULL)
	out_conn = get_conn(htonl(0x7f000001) /* XXX */,
			    afsprport,
			    PR_SERVICE_ID);
    
    if (out_conn == NULL)
	return ENETDOWN;
    
    error = PR_NameToID(out_conn, nlist, ilist);
    if (error == RXKADEXPIRED && first == 0) {
	++first;
	rx_DestroyConnection (out_conn);
	out_conn = NULL;
	goto retry;
    }
    
    
    if (error) {
	fprintf(stderr, "PR_NameToID error: %s(%d)\n",
		koerr_gettext(error), error);
	return error;
    }

    return 0;
}

int
fs_connsec_idtoname(idlist *ilist, namelist *nlist)
{
    int error;

    if (out_conn == NULL)
	out_conn = get_conn(htonl(0x7f000001) /* XXX */,
			    afsprport,
			    PR_SERVICE_ID);

    if (out_conn == NULL)
	return ENETDOWN;
    
    error = PR_IDToName(out_conn, ilist, nlist);
    
    if (error) {
	if (koerr_gettext(error))
	    fprintf(stderr, "PR_IDToName error: %s(%d)\n",
		    koerr_gettext(error), error);
	else
	    fprintf(stderr, "PR_IDToName error: %d\n", error);
	return error;
    }

    return 0;
}

static void
fs_connsec_anonymous(struct fs_security_context *sec)
{
    sec->uid = PR_ANONYMOUSID;
    sec->cps->len = 2;
    sec->cps->val = malloc(2*sizeof(u_int32_t));
    if (sec->cps->val == NULL) {
	sec->cps->len = 0;
	return; /* XXX */
    }
    sec->cps->val[0] = PR_ANYUSERID;
    sec->cps->val[1] = PR_ANONYMOUSID;
}

static void
fs_connsec_createconn(struct rx_connection *conn)
{
    struct fs_security_context *sec;
    namelist nlist;
    idlist ilist;
    char rname[PR_MAXNAMELEN];
    int error;
    char aname[ANAME_SZ];
    char inst[INST_SZ];
    char realm[REALM_SZ];
    int32_t over;

    if (conn->rock)
	return;

    sec = malloc(sizeof(struct fs_security_context));
    if (sec == NULL) 
	return; /* XXX */

    sec->ref = 1;
    sec->cps = malloc(sizeof(prlist));
    if (sec->cps == NULL)
	return; /* XXX */


    conn->rock = sec;

    if (sec_getname(conn, aname, inst, realm)) {
	fs_connsec_anonymous(sec);
	return;
    }

    /* 
     * XXX This should retry with a new connection if the first one
     * fails. It should also try all ptservers.
     */

    if (out_conn == NULL)
	out_conn = get_conn(htonl(0x7f000001) /* XXX */,
			    afsprport,
			    PR_SERVICE_ID);

    if (out_conn == NULL) {
	fs_connsec_anonymous(sec);
	return;
    }

    /* 
     * XXX It is not a good thing to truncate, allows for spoofing?
     * Perhaps we should just deny access if fullname is to long. 
     */

    strlcpy(rname, krb_unparse_name_long(aname, inst, realm), sizeof(rname));
    
    nlist.len = 1;
    nlist.val = &rname;
    ilist.val = NULL;

    error = PR_NameToID(out_conn, &nlist, &ilist);
    
    if (error) {
	fprintf(stderr, "PR_NameToID error: %s(%d)\n",
		koerr_gettext(error), error);
	free(ilist.val);

	fs_connsec_anonymous(sec);
	return;
    }

    fprintf(stderr, "ID is %d\n", ilist.val[0]);
    sec->uid = ilist.val[0];
    free(ilist.val);

    error = PR_GetCPS(out_conn, sec->uid, sec->cps, &over);

    if (error) {
	fprintf(stderr, "PR_GetCPS error: %s(%d)\n",
		koerr_gettext(error), error);
	fs_connsec_anonymous(sec);
	return;
    }

}

void
fs_connsec_destroyconn(struct rx_connection *conn)
{
    if (conn->rock == NULL)
	return;
    
    fs_connsec_context_put(conn->rock);
    conn->rock = NULL;
}

struct fs_security_context *
fs_connsec_context_get(struct rx_connection *conn)
{
    struct fs_security_context *sec;

    fs_connsec_createconn(conn);

    sec = conn->rock;

    assert(sec);

    sec->ref++;
    return sec;
}

void
fs_connsec_context_put(struct fs_security_context *sec)
{
    assert(sec->ref > 0);

    sec->ref--;

    if (sec->ref == 0) {
	free(sec->cps->val);
	free(sec->cps);
	free(sec);
    }
}
