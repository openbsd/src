/*
 * Copyright (c) 1995 - 2003 Kungliga Tekniska Högskolan
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

RCSID("$arla: arlalib.c,v 1.60 2003/06/12 05:29:15 lha Exp $");

#ifdef HAVE_KRB4

static int
get_cred(const char *princ, const char *inst, const char *krealm, 
         struct ClearToken *ct, char *ticket, size_t ticket_len,
	 size_t *ticket_len_out)
{
    CREDENTIALS c;
    int kret;
    
    kret = krb_get_cred((char*)princ, (char*)inst, (char*)krealm, &c);
    
    if(kret != KSUCCESS) {
	KTEXT_ST foo;
	kret = krb_mk_req(&foo, (char*)princ, (char*)inst, (char*)krealm, 0);
	if (kret == KSUCCESS)
	    kret = krb_get_cred((char*)princ, (char*)inst, (char*)krealm, &c);
    }
    if (kret == KSUCCESS) {
	ct->AuthHandle = c.kvno;
	memcpy(ct->HandShakeKey, c.session, sizeof(ct->HandShakeKey));
	ct->BeginTimestamp = c.issue_date;
	ct->EndTimestamp = krb_life_to_time(c.issue_date, c.lifetime);
	ct->ViceId = getuid();
	
	if (ticket_len <= c.ticket_st.length)
	    return EINVAL;
	*ticket_len_out = c.ticket_st.length;
	memcpy(ticket, c.ticket_st.dat, c.ticket_st.length);
    }
    
    return kret;
}
#endif /* HAVE_KRB4 */

#ifdef HAVE_KRB5

#ifndef HAVE_KRB4

/* v4 glue */

#define		MAX_KTXT_LEN	1250

#define 	ANAME_SZ	40
#define		REALM_SZ	40
#define		SNAME_SZ	40
#define		INST_SZ		40

struct ktext {
    unsigned int length;		/* Length of the text */
    unsigned char dat[MAX_KTXT_LEN];	/* The data itself */
    u_int32_t mbz;		/* zero to catch runaway strings */
};

struct credentials {
    char    service[ANAME_SZ];	/* Service name */
    char    instance[INST_SZ];	/* Instance */
    char    realm[REALM_SZ];	/* Auth domain */
    des_cblock session;		/* Session key */
    int     lifetime;		/* Lifetime */
    int     kvno;		/* Key version number */
    struct ktext ticket_st;	/* The ticket itself */
    int32_t    issue_date;	/* The issue time */
    char    pname[ANAME_SZ];	/* Principal's name */
    char    pinst[INST_SZ];	/* Principal's instance */
};

typedef struct credentials CREDENTIALS;

#define TKTLIFENUMFIXED 64
#define TKTLIFEMINFIXED 0x80
#define TKTLIFEMAXFIXED 0xBF
#define TKTLIFENOEXPIRE 0xFF
#define MAXTKTLIFETIME	(30*24*3600)	/* 30 days */
#ifndef NEVERDATE
#define NEVERDATE ((time_t)0x7fffffffL)
#endif

static const int _tkt_lifetimes[TKTLIFENUMFIXED] = {
   38400,   41055,   43894,   46929,   50174,   53643,   57352,   61318,
   65558,   70091,   74937,   80119,   85658,   91581,   97914,  104684,
  111922,  119661,  127935,  136781,  146239,  156350,  167161,  178720,
  191077,  204289,  218415,  233517,  249664,  266926,  285383,  305116,
  326213,  348769,  372885,  398668,  426234,  455705,  487215,  520904,
  556921,  595430,  636601,  680618,  727680,  777995,  831789,  889303,
  950794, 1016537, 1086825, 1161973, 1242318, 1328218, 1420057, 1518247,
 1623226, 1735464, 1855462, 1983758, 2120925, 2267576, 2424367, 2592000
};

static time_t
_arla_krb_life_to_time(int start, int life_)
{
    unsigned char life = (unsigned char) life_;

    if (life == TKTLIFENOEXPIRE)
	return NEVERDATE;
    if (life < TKTLIFEMINFIXED)
	return start + life*5*60;
    if (life > TKTLIFEMAXFIXED)
	return start + MAXTKTLIFETIME;
    return start + _tkt_lifetimes[life - TKTLIFEMINFIXED];
}

#define krb_life_to_time _arla_krb_life_to_time


#endif

/*
 *
 */

static int
v4_to_kt(CREDENTIALS *c, struct ClearToken *ct,
	 char *ticket, size_t ticket_len, size_t *ticket_len_out)
{
    if (c->ticket_st.length > ticket_len)
	return EINVAL;

    *ticket_len_out = c->ticket_st.length;
    memcpy(ticket, c->ticket_st.dat, c->ticket_st.length);
    
    /*
     * Build a struct ClearToken
     */
    ct->AuthHandle = c->kvno;
    memcpy (ct->HandShakeKey, c->session, sizeof(c->session));
    ct->ViceId = getuid();
    ct->BeginTimestamp = c->issue_date;
    ct->EndTimestamp = krb_life_to_time(c->issue_date, c->lifetime);
    
    return 0;
}

/*
 *
 */

static int
get_cred5(const char *princ, const char *inst, const char *krealm, 
	  struct ClearToken *ct, char *ticket, 
	  size_t ticket_len, size_t *ticket_len_out)
{
    krb5_context context = NULL;
    krb5_error_code ret;
    krb5_creds in_creds, *out_creds;
    krb5_ccache id = NULL;
    CREDENTIALS cred4;

    ret = krb5_init_context(&context);
    if (ret)
	return ret;

    ret = krb5_cc_default(context, &id);
    if (ret)
	goto out;

    memset(&in_creds, 0, sizeof(in_creds));
    ret = krb5_425_conv_principal(context, princ, inst, krealm,
				  &in_creds.server);
    if(ret)
	goto out;

    ret = krb5_cc_get_principal(context, id, &in_creds.client);
    if(ret){
	krb5_free_principal(context, in_creds.server);
	goto out;
    }
    in_creds.session.keytype = KEYTYPE_DES;
    ret = krb5_get_credentials(context, 0, id, &in_creds, &out_creds);
    krb5_free_principal(context, in_creds.server);
    krb5_free_principal(context, in_creds.client);
    if(ret)
	goto out;

    ret = krb524_convert_creds_kdc_ccache(context, id, out_creds, &cred4);
    krb5_free_creds(context, out_creds);
    if (ret)
	goto out;

    ret = v4_to_kt(&cred4, ct, ticket, ticket_len, ticket_len_out);

 out:
    if (id)
	krb5_cc_close(context, id);
    if (context)
	krb5_free_context(context);

    return ret;
}

#endif /* HAVE_KRB5 */

static int
arlalib_get_cred_krb (const char *cell, const char *host, 
		      struct ClearToken *ct,
		      unsigned char *ticket,
		      size_t ticket_len,
		      size_t *ticket_len_out,
		      arlalib_authflags_t auth)
{
    char krealm[REALM_SZ];
#ifdef HAVE_KRB4
    char *rrealm;
#endif
    int ret;

    memset(ct, 0, sizeof(*ct));
	

#ifdef HAVE_KRB4
    if (auth & AUTHFLAGS_LOCALAUTH) {
	des_cblock key, session;
	KTEXT_ST kticket;
	char kcell[REALM_SZ];
	time_t t;

	rrealm = krb_realmofhost(host);
	strlcpy(krealm, rrealm, sizeof(krealm));
	strlcpy(kcell, cell, sizeof(kcell));

	ret = srvtab_to_key("afs", kcell, krealm,
			    SYSCONFDIR "/srvtab", &key);

	if (ret && strcasecmp(krealm, kcell) == 0) {

	    ret = srvtab_to_key("afs", "", krealm,
				SYSCONFDIR "/srvtab", &key);
	}

	if (ret)
	    return ret;

	des_random_key(&session);

	t = time(NULL);

	ret = krb_create_ticket(&kticket, 0,
				"afs", "", krealm,
				0 /* XXX flags */, session,
				0xFF, t, "afs", "", &key);
	if (ret)
	    return ret;

	if (kticket.length >= ticket_len)
	    errx(-1, "kticket length >= ticket_len");

	ct->ViceId = getuid();
	ct->AuthHandle = 0;
	memcpy(ct->HandShakeKey, &session, sizeof(ct->HandShakeKey));
	ct->BeginTimestamp = t;
	ct->EndTimestamp = t + 3600 * 10;

	memcpy(ticket, kticket.dat, kticket.length);
	*ticket_len_out = kticket.length;

	return 0;
    }
#endif

    if (auth & (AUTHFLAGS_TICKET|AUTHFLAGS_ANY)) {

	if(cell) {
	    strlcpy(krealm, cell, REALM_SZ);
	    strupr(krealm);
	    
#ifdef HAVE_KRB5
	    ret = get_cred5("afs", "", krealm, ct, ticket, ticket_len,
			    ticket_len_out);
	    if (ret == 0)
		return 0;
#endif
#ifdef HAVE_KRB4
	    ret = get_cred("afs", "", krealm, ct, ticket, ticket_len,
			   ticket_len_out);
	    if (ret == KSUCCESS)
		return ret;
#endif
	}

#ifdef HAVE_KRB5
	ret = get_cred5("afs", cell ? cell : "", krealm,
			ct, ticket, ticket_len, ticket_len_out);
	if (ret == 0)
	    return ret;
	else {
	    ret = get_cred5("afs", "", krealm,
			    ct, ticket, ticket_len, ticket_len_out);
	    if (ret == 0)
		return ret;
	}

#endif
#ifdef HAVE_KRB4
	rrealm = krb_realmofhost(host);
	strlcpy(krealm, rrealm, REALM_SZ);
	
	ret = get_cred("afs", cell ? cell : "", krealm,
		       ct, ticket, ticket_len, ticket_len_out);
	if (ret != KSUCCESS)
	    ret = get_cred("afs", "", krealm,
			   ct, ticket, ticket_len, ticket_len_out);
	if (ret == KSUCCESS)
	    return ret;
#endif
    }

    ret = EINVAL;

    return ret;
}

struct find_token_arg {
    struct ClearToken *ct;
    char *ticket;
    size_t ticket_len;
};

static int
find_token (const char *secret, size_t secret_sz, 
	    const struct ClearToken *ct, 
	    const char *cell, void *arg)
{
    struct find_token_arg *c = (struct find_token_arg *)arg;

    if (c->ticket_len <= secret_sz)
	return EINVAL;
    memcpy(c->ticket, secret, secret_sz) ;
    c->ticket_len = secret_sz;
    *c->ct = *ct;

    return 0;
}

static int
arlalib_get_cred_afs (const char *cell,
		      struct ClearToken *ct,
		      unsigned char *ticket,
		      size_t ticket_len,
		      size_t *ticket_len_out,
		      arlalib_authflags_t auth)
{
    struct find_token_arg c;
    int ret;

    c.ct = ct;
    c.ticket = ticket;
    c.ticket_len = ticket_len;

    if (cell == NULL)
	cell = cell_getthiscell();

    ret = arlalib_token_iter (cell, find_token, &c);
    *ticket_len_out = c.ticket_len;

    return ret;
}


int
arlalib_getservername(uint32_t serverNumber, char **servername)
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
			 arlalib_authflags_t auth, int *secidx)
{
    struct rx_securityClass* sec = NULL;

    if (auth) {
	int ret;
	struct ClearToken ct;
	char ticket[MAXKRB4TICKETLEN];
	size_t ticket_len, ticket_len_out;

	ticket_len = sizeof(ticket);

	ret = arlalib_get_cred_krb (cell, host, &ct,
				    ticket, 
				    ticket_len, &ticket_len_out,
				    auth);
	if (ret)
	    ret = arlalib_get_cred_afs (cell, &ct, 
					ticket,
					ticket_len, 
					&ticket_len_out,
					auth);

	if (ret == 0) {
	    sec = rxkad_NewClientSecurityObject(rxkad_auth,
						ct.HandShakeKey,
						ct.AuthHandle,
						ticket_len,
						ticket);
	    *secidx = 2;
	} else {
	    fprintf(stderr, "Can't get a token for cell %s\n",
		    cell ? cell : cell_getthiscell());
	}
    }

    if (sec == NULL) {
	sec = rxnull_NewClientSecurityObject();
	*secidx = 0;
    }
    
    return sec;
}


struct rx_connection *
arlalib_getconnbyaddr(const char *cell, int32_t addr,
		      const char *host, int32_t port, int32_t servid,
		      arlalib_authflags_t auth)
{
    struct rx_securityClass *secobj;
    struct rx_connection *conn;
    char *serv = NULL;
    int secidx;

    rx_Init(0);

    if (host == NULL) {
	arlalib_getservername(addr, &serv);
	host = serv;
    }
    
    if ((secobj = arlalib_getsecurecontext(cell, host, auth, &secidx)) == NULL)
	return NULL;

    conn = rx_NewConnection (addr, htons (port), servid,
			     secobj, secidx);
    
    if (conn == NULL) 
	fprintf (stderr, "Cannot start rx-connection, something is WRONG\n");
    
    if (serv)
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
		    uint32_t *synchost, arlalib_authflags_t auth)
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
    } else
	res = ENETDOWN;

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

  if (cell == NULL) {
      cell = cell_getthiscell();
      if (cell == NULL)
	  return NULL;
  }
  
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
arlalib_host_to_name (uint32_t addr, char *str, size_t str_sz)
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

/*
 * give an address for the server 'srv'
 */ 
int
arlalib_name_to_host (const char *str, uint32_t *addr)
{

    struct addrinfo *addr_info, *p;
    struct sockaddr_in *sin;
    int error;

    error = getaddrinfo(str, NULL, NULL, &addr_info);
    if(error)
	return error;
			
    p = addr_info;
    while(p != NULL) {
	if(p->ai_family == AF_INET) {
	    sin = (struct sockaddr_in *)p->ai_addr;
	    *addr = sin->sin_addr.s_addr;
	    return 0;
	}
    }

    return EAI_NONAME;
}

/*
 *
 */

int
arlalib_version_cmd(int argc, char **argv)
{
    print_version(NULL);
    return 0;
}

