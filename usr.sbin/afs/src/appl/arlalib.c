/*	$OpenBSD: arlalib.c,v 1.2 1999/04/30 01:59:04 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

#include "appl_locl.h"

RCSID("$KTH: arlalib.c,v 1.19 1999/04/06 18:31:30 lha Exp $");


static struct rx_securityClass *secureobj = NULL ; 
int secureindex = -1 ; 
int rx_initlizedp = 0;

#ifdef KERBEROS

static int
get_cred(const char *princ, const char *inst, const char *krealm, 
         CREDENTIALS *c)
{
  KTEXT_ST foo;
  int k_errno;

  k_errno = krb_get_cred((char*)princ, (char*)inst, (char*)krealm, c);

  if(k_errno != KSUCCESS) {
    k_errno = krb_mk_req(&foo, (char*)princ, (char*)inst, (char*)krealm, 0);
    if (k_errno == KSUCCESS)
      k_errno = krb_get_cred((char*)princ, (char*)inst, (char*)krealm, c);
  }
  return k_errno;
}

static int
arlalib_get_cred(const char *cell, const char *host, CREDENTIALS *c,
		 arlalib_authflags_t auth)
{
    char krealm[REALM_SZ];
    char *rrealm;
    int k_errno;
    int ret;

    if (auth & (AUTHFLAGS_TICKET|AUTHFLAGS_ANY)) {
	rrealm = krb_realmofhost(host);
	strncpy(krealm, rrealm, REALM_SZ);
	krealm[REALM_SZ-1] = '\0';
	
	k_errno = get_cred("afs", cell ? cell : "", krealm, c);
	if (k_errno != KSUCCESS)
	    k_errno = get_cred("afs", "", krealm, c);
	
	if (k_errno != KSUCCESS) {
	    fprintf(stderr,
		    "Can't get a ticket for realm %s: %s\n",
		    krealm, krb_get_err_text(k_errno));
	    return -1;
	} 
	ret = k_errno;
    }

    return ret;
}
#endif /* KERBEROS */

int
arlalib_getservername(u_int32_t serverNumber, char **servername)
{
    struct hostent *he;

    he = gethostbyaddr((char*) &serverNumber, sizeof(serverNumber), AF_INET);

    if (he != NULL)
	*servername = strdup(he->h_name);
    else {
	struct in_addr addr;
	addr.s_addr = serverNumber;
	    
	*servername = strdup(inet_ntoa(addr));
    }

    return (*servername == NULL);
}


struct rx_securityClass*
arlalib_getsecurecontext(const char *cell, const char *host, 
			 arlalib_authflags_t auth)
{
#ifdef KERBEROS
    CREDENTIALS c;
#endif /* KERBEROS */
    struct rx_securityClass* sec;

    if (secureobj != NULL) 
	return secureobj;
    
#ifdef KERBEROS
    
    if (auth  && 
	arlalib_get_cred(cell, host, &c, auth) == KSUCCESS) {

	sec = rxkad_NewClientSecurityObject(rxkad_auth,
					    &c.session,
					    c.kvno,
					    c.ticket_st.length,
					    c.ticket_st.dat);
	secureindex = 2;
    } else {
#endif /* KERBEROS */
	
	sec = rxnull_NewClientSecurityObject();
	secureindex = 0;

#ifdef KERBEROS
    }
#endif /* KERBEROS */
    
    secureobj = sec;

    return sec;

}


struct rx_connection *
arlalib_getconnbyaddr(const char *cell, int32_t addr,
		      const char *host, int32_t port, int32_t servid,
		      arlalib_authflags_t auth)
{
    struct rx_connection *conn;
    int allocedhost = 0;
    char *serv;

    if (rx_initlizedp ==0) {
	rx_Init(0);
	rx_initlizedp = 1;
    }

    if (host == NULL) {
	arlalib_getservername(addr, &serv);
	allocedhost= 1;
	host = serv;
    }
    
    if (arlalib_getsecurecontext(cell, host, auth)== NULL) 
	return NULL;

    conn = rx_NewConnection (addr, 
			     htons (port), 
			     servid,
			     secureobj,
			     secureindex);
    
    if (conn == NULL) 
	fprintf (stderr, "Cannot start rx-connection, something is WRONG\n");
    
    if (allocedhost)
	free(serv);

    return conn;
}

struct rx_connection *
arlalib_getconnbyname(const char *cell, const char *host,
		      int32_t port, int32_t servid, 
		      arlalib_authflags_t auth)
{
    struct in_addr server;

    if (ipgetaddr (host, &server) == NULL ) {
	fprintf (stderr, "Cannot find host %s\n", host);
	return  NULL;
    }

    return arlalib_getconnbyaddr(cell, server.s_addr, host, port, servid,
				 auth);
}

int
arlalib_destroyconn(struct rx_connection *conn)
{
    if (conn == NULL)
	return 0 ;

    rx_DestroyConnection(conn);
    return 0;
}

/*
 * arlalib_getsyncsite
 *
 * if cell and host is NULL, local cell is assumed and a local dbserver is used
 * if cell is NULL and host not, cell is figured out 
 *          (if that fail, localcell is assumed)
 * if cell is set but not host, host is found i CellServerDB or DNS
 *
 *
 *  RETURNS: 0 is ok, otherwise an error that should be handled to 
 *  koerr_gettext()
 */

int 
arlalib_getsyncsite(const char *cell, const char *host, int32_t port, 
		    u_int32_t *synchost, arlalib_authflags_t auth)
{
    struct rx_connection *conn;
    ubik_debug db;
    int error;

    if (synchost == NULL)
	return EINVAL;

    if (cell == NULL && host != NULL) 
	cell = cell_getcellbyhost(host);
    if (cell == NULL) {
	cell = cell_getthiscell();
	if (cell == NULL)
	    return ENOENT;
    }
    if (host == NULL) {
	host = cell_findnamedbbyname (cell);
	if (host == NULL)
	    return ENOENT;
    }

    conn = arlalib_getconnbyname(cell,
				 host,
				 port,
				 VOTE_SERVICE_ID,
				 auth);

    if (conn == NULL)
	return ENETDOWN;

    error = Ubik_Debug(conn, &db);
    *synchost = htonl(db.syncHost);

    arlalib_destroyconn(conn);

    return error;
}


/*
 * get a arlalib_authflags_t type
 */

arlalib_authflags_t 
arlalib_getauthflag (int noauth,
		     int localauth,
		     int ticket,
		     int token)
{
    arlalib_authflags_t ret = AUTHFLAGS_ANY;

    if (noauth)
	ret = AUTHFLAGS_NOAUTH;
    if (localauth)
	ret |= AUTHFLAGS_LOCALAUTH;
    if (ticket)
	ret |= AUTHFLAGS_TICKET;
    if (token)
	ret |= AUTHFLAGS_TOKEN;
    
    return ret;
}

/*
 *
 */


