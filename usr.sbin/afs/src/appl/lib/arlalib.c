/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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

#include "appl_locl.h"

RCSID("$KTH: arlalib.c,v 1.41.2.2 2001/10/02 16:13:00 jimmy Exp $");


static struct rx_securityClass *secureobj = NULL ; 
int secureindex = -1 ; 

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
arlalib_get_cred_krb (const char *cell, const char *host, CREDENTIALS *c,
		      arlalib_authflags_t auth)
{
    char krealm[REALM_SZ];
    char *rrealm;
    int k_errno;
    int ret;

    if (auth & (AUTHFLAGS_TICKET|AUTHFLAGS_ANY)) {
	rrealm = krb_realmofhost(host);
	strlcpy(krealm, rrealm, REALM_SZ);
	
	k_errno = get_cred("afs", cell ? cell : "", krealm, c);
	if (k_errno != KSUCCESS)
	    k_errno = get_cred("afs", "", krealm, c);
	
	if (k_errno != KSUCCESS)
	    return -1;
	ret = k_errno;
    } else
	ret = EOPNOTSUPP;

    return ret;
}

static int
find_token (const char *secret, size_t secret_sz, 
	    const struct ClearToken *ct, 
	    const char *cell, void *arg)
{
    CREDENTIALS *c = (CREDENTIALS *)arg;

    memcpy(c->ticket_st.dat, secret, secret_sz) ;
    c->ticket_st.length = secret_sz;

    strlcpy (c->realm, cell, sizeof(c->realm));
    strupr(c->realm);

    c->kvno = ct->AuthHandle;
    memcpy (c->session, ct->HandShakeKey, sizeof(c->session));
    c->issue_date = ct->BeginTimestamp - 1;

    return 0;
}

static int
arlalib_get_cred_afs (const char *cell, CREDENTIALS *c, 
		      arlalib_authflags_t auth)
{
    if (cell == NULL)
	cell = cell_getthiscell();

    return arlalib_token_iter (cell, find_token, c);
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
    struct rx_securityClass* sec = NULL;

    if (secureobj != NULL) 
	return secureobj;
    
#ifdef KERBEROS
    if (auth) {
	int ret;

	ret = arlalib_get_cred_krb (cell, host, &c, auth);
	if (ret == KSUCCESS) {
	    ret = 0;
	} else {
	    ret = arlalib_get_cred_afs (cell, &c, auth);
	}
	if (ret == 0) {
	    sec = rxkad_NewClientSecurityObject(rxkad_auth,
						&c.session,
						c.kvno,
						c.ticket_st.length,
						c.ticket_st.dat);
	    secureindex = 2;
	} else {
	    fprintf(stderr, "Can't get a token for cell %s\n",
		    cell ? cell : cell_getthiscell());
	}
    }
#endif /* KERBEROS */

    if (sec == NULL) {
	sec = rxnull_NewClientSecurityObject();
	secureindex = 0;
    }
    
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

    rx_Init(0);

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
    struct addrinfo hints, *res;
    int error;
    int32_t addr;

    memset (&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    error = getaddrinfo(host, NULL, &hints, &res);
    if (error) {
	fprintf (stderr, "Cannot find host %s\n", host);
	return  NULL;
    }

    assert (res->ai_family == PF_INET);
    addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr;
    freeaddrinfo(res);

    return arlalib_getconnbyaddr(cell, addr, host, port, servid, auth);
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
    if (!error) {
	if (db.amSyncSite)
	    *synchost = rx_HostOf(rx_PeerOf(conn));
	else
	    *synchost = htonl(db.syncHost);
    }
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
 * set `viceId' to the(a) id for username@cellname
 * return 0 or error
 */

int
arlalib_get_viceid (const char *username, const char *cellname,
		    int32_t *viceId)
{
    const cell_db_entry *db_entry;
    int32_t cell;
    int num;
    const char **servers;
    int i;
    int ret;

    cell = cell_name2num (cellname);
    if (cell == -1)
	return -1;

    db_entry = cell_dbservers_by_id (cell, &num);
    if (NULL)
	return -1;

    servers = malloc (num * sizeof(*servers));
    if (servers == NULL)
	return -1;
    
    for (i = 0; i < num; ++i)
	servers[i] = db_entry[i].name;

    ret = arlalib_get_viceid_servers (username, cellname,
				      num, servers, viceId);
    free (servers);
    return ret;
}

/*
 * * set `viceId' to the(a) id for username@cellname
 * use nservers, servers to query for pt database.
 * return 0 or error
 */

int
arlalib_get_viceid_servers (const char *username, const char *cellname,
			    int nservers, const char *servers[],
			    int32_t *viceId)
{
    int i=0;
    int32_t returned_id;
    int32_t res;

/* FIXME: Should we use authorization when connecting to the dbserver?
	  noauth=0 sometimes gives warnings, e.g. if the realm name is
	  not the same as the name of the cell...
*/
    int noauth = 1;

    struct rx_connection *connptdb = NULL;
    namelist nlist;
    idlist ilist;
    prname pr_name_buf;

    /* set up necessary crap to use PR_NameToID */
    nlist.len = 1;
    nlist.val = &pr_name_buf;

    ilist.len = 1;
    ilist.val = &returned_id;

    strlcpy (pr_name_buf, username, sizeof(pr_name_buf));

    /* try all known servers :) */
    for (i = 0; i < nservers; i++) {
	connptdb = arlalib_getconnbyname(cellname, 
					 servers[i],
					 afsprport, 
					 PR_SERVICE_ID,
					 arlalib_getauthflag (noauth, 0,
							      0, 0));
	if (connptdb == NULL)
	    return ENETDOWN;
    }

    if (connptdb) {
	res = PR_NameToID(connptdb, &nlist, &ilist);
	
	arlalib_destroyconn(connptdb);
	
	if (res == 0) {
	    *viceId = ilist.val[0];
	    return 0;
	}
    }

    return res;
}

/*
 * try to come with a reasonable uid to use for tokens
 */

static void
fallback_vice_id (const char *username, const char *cellname,
		  int32_t *token_id)
{
    struct passwd *pwd;

    pwd = getpwnam(username);
    if(pwd == NULL) {
	*token_id = getuid();
	warnx ("Couldn't get AFS ID for %s@%s, using current UID (%d)",
	       username, cellname, (int)*token_id);
    } else {
	*token_id = pwd->pw_uid;
	warnx ("Couldn't get AFS ID for %s@%s, using %d from /etc/passwd",
	       username, cellname, (int)*token_id);
    }
}

/*
 * Come with a number to use in a token for for username@cellname
 * in token_id
 */

int
arlalib_get_token_id (const char *username, const char *cellname,
		      int32_t *token_id)
{
    int ret;

    ret = arlalib_get_viceid (username, cellname, token_id);
    if (ret == 0)
	return ret;

    fallback_vice_id (username, cellname, token_id);
    return 0;
}

/*
 * Come with a number to use in a token for for username@cellname
 * in token_id
 * use nservers, servers for querying
 */

int
arlalib_get_token_id_servers (const char *username, const char *cellname,
			      int nservers, const char *servers[],
			      int32_t *token_id)
{
    int ret;

    ret = arlalib_get_viceid_servers (username, cellname,
				      nservers, servers, token_id);
    if (ret == 0)
	return ret;

    fallback_vice_id (username, cellname, token_id);
    return 0;
}

/*
 * Initialize `context' for a db connection to cell `cell', looping
 * over all db servers if `host' == NULL, and else just try that host.
 * `port', `serv_id', and `auth' specify where and how the connection works.
 * Return a rx connection or NULL
 */

struct rx_connection *
arlalib_first_db(struct db_server_context *context,
		 const char *cell,
		 const char *host,
		 int port,
		 int serv_id,
		 arlalib_authflags_t auth)
{
  const cell_entry *c_entry;
  int i;
  
  /* Set struct values from args */
  context->cell    = cell;
  context->port    = port;
  context->serv_id = serv_id;
  context->auth    = auth;
  
  if (host != NULL) {
      context->nhosts = 1;
      context->hosts  = malloc (sizeof (char *));
      if (context->hosts == NULL)
	  return NULL;
      context->hosts[0] = host;
  } else {
      /* Calculate missing context values */
      c_entry = cell_get_by_name(cell);
      if (c_entry == NULL) {
	  warn("Cannot find cell %s", cell);
	  return NULL;
      }
      if (c_entry->ndbservers == 0) {
	  warn("No DB servers for cell %s", cell);
	  return NULL;
      }
      context->nhosts = c_entry->ndbservers;
      context->hosts  = malloc(context->nhosts * sizeof(char *));
      if (context->hosts == NULL)
	  return NULL;
      for(i = 0; i < context->nhosts; i++)
	  context->hosts[i] = c_entry->dbservers[i].name;
  }

  /* Try to get connection handles until we have one */
  context->conn = malloc(context->nhosts * sizeof(struct rx_connection*));
  if (context->conn == NULL) {
    free (context->hosts);
    return NULL;
  }
  for (i = 0; i < context->nhosts; ++i)
    context->conn[i] = NULL;

  context->curhost = -1;
  return arlalib_next_db(context);
}

/*
 * Return next connection from `context' or NULL.
 */

struct rx_connection*
arlalib_next_db(struct db_server_context *context)
{
  int i;

  for(i = context->curhost + 1; i < context->nhosts; i++) {
    context->curhost = i;
    context->conn[i] = arlalib_getconnbyname(context->cell,
					     context->hosts[i],
					     context->port,
					     context->serv_id,
					     context->auth);
    if(context->conn[i] != NULL)
      return context->conn[i];
  }
  return NULL;
}

/*
 * return TRUE iff error makes it worthwhile to try the next db.
 */

int
arlalib_try_next_db (int error)
{
    switch (error) {
    case ARLA_CALL_DEAD:
    case UNOTSYNC :
    case ENETDOWN :
	return TRUE;
    case 0 :
    default :
	return FALSE;
    }
}

/*
 * free all memory associated with `context'
 */

void
free_db_server_context(struct db_server_context *context)
{
  int i;

  for (i = 0; i < context->nhosts; i++) {
      if (context->conn[i] != NULL) 
	  arlalib_destroyconn(context->conn[i]);
  }
  free(context->hosts);
  free(context->conn);
}   

/*
 * give a name for the server `addr'
 */

void
arlalib_host_to_name (u_int32_t addr, char *str, size_t str_sz)
{
    struct sockaddr_in sock;
    int error;

    memset (&sock, 0, sizeof(sock));
#if HAVE_STRUCT_SOCKADDR_IN_SIN_LEN
    sock.sin_len = sizeof(sock);
#endif
    sock.sin_family = AF_INET;
    sock.sin_port = 0;
    sock.sin_addr.s_addr = addr;

    error = getnameinfo((struct sockaddr *)&sock, sizeof(sock),
			str, str_sz, NULL, 0, 0);
    if (error)
	strlcpy (str, "<unknown>", str_sz);
}
