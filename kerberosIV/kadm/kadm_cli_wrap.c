/*	$OpenBSD: kadm_cli_wrap.c,v 1.7 1999/03/03 08:41:27 csapuntz Exp $	*/
/*	$KTH: kadm_cli_wrap.c,v 1.22 1997/08/17 07:30:04 assar Exp $	*/

/*
 * This source code is no longer held under any constraint of USA
 * `cryptographic laws' since it was exported legally.  The cryptographic
 * functions were removed from the code and a "Bones" distribution was
 * made.  A Commodity Jurisdiction Request #012-94 was filed with the
 * USA State Department, who handed it to the Commerce department.  The
 * code was determined to fall under General License GTDA under ECCN 5D96G,
 * and hence exportable.  The cryptographic interfaces were re-added by Eric
 * Young, and then KTH proceeded to maintain the code in the free world.
 *
 */

/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

/*
 * Kerberos administration server client-side routines
 */

/*
 * kadm_cli_wrap.c the client side wrapping of the calls to the admin server 
 */

#include "kadm_locl.h"

#ifndef NULL
#define NULL 0
#endif

static Kadm_Client client_parm;

/* Macros for use in returning data... used in kadm_cli_send */
#define RET_N_FREE(r) {clear_secrets(); free((char *)act_st); act_st = NULL; free((char *)priv_pak); priv_pak = NULL; return r;}

/* Keys for use in the transactions */
static des_cblock sess_key;	       /* to be filled in by kadm_cli_keyd */
static des_key_schedule sess_sched;

static void
clear_secrets(void)
{
    memset(sess_key, 0, sizeof(sess_key));
    memset(sess_sched, 0, sizeof(sess_sched));
}

static void (*opipe)();

static void
kadm_cli_disconn(void)
{
    close(client_parm.admin_fd);
    signal(SIGPIPE, opipe);
}

/*
 * kadm_init_link
 *	receives    : name, inst, realm
 *
 * initializes client parm, the Kadm_Client structure which holds the 
 * data about the connection between the server and client, the services 
 * used, the locations and other fun things 
 */

int
kadm_init_link(char *n, char *i, char *r)
{
    struct hostent *hop;	       /* host we will talk to */
    char adm_hostname[MAXHOSTNAMELEN];
    
    init_kadm_err_tbl();
    init_krb_err_tbl();
    strncpy(client_parm.sname, n, ANAME_SZ - 1);
    client_parm.sname[ANAME_SZ - 1] = '\0';
    strncpy(client_parm.sinst, i, INST_SZ - 1);
    client_parm.sinst[INST_SZ - 1] = '\0';
    strncpy(client_parm.krbrlm, r, REALM_SZ - 1);
    client_parm.krbrlm[REALM_SZ - 1] = '\0';
    client_parm.admin_fd = -1;
    
    /* set up the admin_addr - fetch name of admin host */
    if (krb_get_admhst(adm_hostname, client_parm.krbrlm, 1) != KSUCCESS)
	return KADM_NO_HOST;
    if ((hop = gethostbyname(adm_hostname)) == NULL)
	return KADM_UNK_HOST;
    memset(&client_parm.admin_addr, 0, sizeof(client_parm.admin_addr));
    client_parm.admin_addr.sin_port = 
	k_getportbyname(KADM_SNAME, "tcp", htons(KADM_PORT));
    client_parm.admin_addr.sin_family = hop->h_addrtype;
    memcpy(&client_parm.admin_addr.sin_addr, hop->h_addr,
	   sizeof(client_parm.admin_addr.sin_addr));
    
    return KADM_SUCCESS;
}

static int
kadm_cli_conn(void)
{					/* this connects and sets my_addr */
    int on = 1;
    
    if ((client_parm.admin_fd =
	 socket(client_parm.admin_addr.sin_family, SOCK_STREAM,0)) < 0)
	return KADM_NO_SOCK;		/* couldnt create the socket */

    if (connect(client_parm.admin_fd,
		(struct sockaddr *) & client_parm.admin_addr,
		sizeof(client_parm.admin_addr))) {
	close(client_parm.admin_fd);
	client_parm.admin_fd = -1;
	return KADM_NO_CONN;		/* couldnt get the connect */
    }

    opipe = signal(SIGPIPE, SIG_IGN);

    client_parm.my_addr_len = sizeof(client_parm.my_addr);
    if (getsockname(client_parm.admin_fd,
		    (struct sockaddr *) & client_parm.my_addr,
		    &client_parm.my_addr_len) < 0) {
	close(client_parm.admin_fd);
	client_parm.admin_fd = -1;
	signal(SIGPIPE, opipe);
	return KADM_NO_HERE;		/* couldnt find out who we are */
    }

    if (setsockopt(client_parm.admin_fd, SOL_SOCKET, SO_KEEPALIVE,
		   (void *)&on,
		   sizeof(on)) < 0) {
	close(client_parm.admin_fd);
	client_parm.admin_fd = -1;
	signal(SIGPIPE, opipe);
	return KADM_NO_CONN;		/* XXX */
    }

    return KADM_SUCCESS;
}

/* takes in the sess_key and key_schedule and sets them appropriately */
static int
kadm_cli_keyd(des_cblock (*s_k), struct des_ks_struct *s_s)
                			       /* session key */
                                               /* session key schedule */
{
    CREDENTIALS cred;	       /* to get key data */
    int stat;

    /* want .sname and .sinst here.... */
    if ((stat = krb_get_cred(client_parm.sname, client_parm.sinst,
			     client_parm.krbrlm, &cred)))
	return stat + krb_err_base;
    memcpy(s_k, cred.session, sizeof(des_cblock));
    memset(cred.session, 0, sizeof(des_cblock));
#ifdef NOENCRYPTION
    memset(s_s, 0, sizeof(des_key_schedule));
#else
    if ((stat = des_key_sched(s_k,s_s)))
	return(stat+krb_err_base);
#endif
    return KADM_SUCCESS;
}				       /* This code "works" */

static int
kadm_cli_out(u_char *dat, int dat_len, u_char **ret_dat, int *ret_siz)
{
    u_int16_t dlen;
    int retval;
    char tmp[4];

    dlen = (u_int16_t) dat_len;

    if (dat_len != (int)dlen)
	return (KADM_NO_ROOM);

    tmp[0] = (dlen >> 8) & 0xff;
    tmp[1] = dlen & 0xff;
    if (krb_net_write(client_parm.admin_fd, tmp, 2) != 2)
	return (errno);	       /* XXX */

    if (krb_net_write(client_parm.admin_fd, (char *) dat, dat_len) < 0)
	return (errno);	       /* XXX */

	
    if ((retval = krb_net_read(client_parm.admin_fd, tmp, 2)) != 2){
	if (retval < 0)
	    return(errno);		/* XXX */
	else
	    return(EPIPE);		/* short read ! */
    }
    dlen = (tmp[0] << 8) | tmp[1];

    *ret_dat = (u_char *)malloc((unsigned)dlen);
    if (*ret_dat == NULL)
	return(KADM_NOMEM);

    if ((retval = krb_net_read(client_parm.admin_fd,  *ret_dat,
			       dlen) != dlen)) {
	if (retval < 0)
	    return(errno);		/* XXX */
	else
	    return(EPIPE);		/* short read ! */
    }
    *ret_siz = (int) dlen;
    return KADM_SUCCESS;
}

/*
 * kadm_cli_send
 *	recieves   : opcode, packet, packet length, serv_name, serv_inst
 *	returns    : return code from the packet build, the server, or
 *			 something else 
 *
 * It assembles a packet as follows:
 *	 8 bytes    : VERSION STRING
 *	 4 bytes    : LENGTH OF MESSAGE DATA and OPCODE
 *		    : KTEXT
 *		    : OPCODE       \
 *		    : DATA          > Encrypted (with make priv)
 *		    : ......       / 
 *
 * If it builds the packet and it is small enough, then it attempts to open the
 * connection to the admin server.  If the connection is succesfully open
 * then it sends the data and waits for a reply. 
 */
static int
kadm_cli_send(u_char *st_dat, int st_siz, u_char **ret_dat, int *ret_siz)
               				/* the actual data */
           				/* length of said data */
                 			/* to give return info */
             				/* length of returned info */
{
    int act_len, retdat;	       /* current offset into packet, return
				        * data */
    KTEXT_ST authent;	       /* the authenticator we will build */
    u_char *act_st;		       /* the pointer to the complete packet */
    u_char *temp;
    u_char *priv_pak;	       /* private version of the packet */
    int priv_len;		       /* length of private packet */
    u_int32_t cksum;		       /* checksum of the packet */
    MSG_DAT mdat;
    u_char *return_dat;

    act_st = (u_char *) malloc(KADM_VERSIZE); /* verstr stored first */
    if (act_st == NULL) {
	clear_secrets();
	return KADM_NOMEM;
    }
    
    strncpy((char *)act_st, KADM_VERSTR, KADM_VERSIZE);
    act_len = KADM_VERSIZE;

    if ((retdat = kadm_cli_keyd(&sess_key, sess_sched)) != KADM_SUCCESS) {
	free(act_st);
	act_st = NULL;
	clear_secrets();
	return retdat;	       /* couldnt get key working */
    }

    priv_pak = (u_char *) malloc((unsigned)(st_siz + 200));
    if (priv_pak == NULL) {
	free(act_st);
	act_st = NULL;
	clear_secrets();
	return KADM_NOMEM;
    }

    /* 200 bytes for extra info case */
    if ((priv_len = krb_mk_priv(st_dat, priv_pak, (u_int32_t)st_siz,
				sess_sched, &sess_key, &client_parm.my_addr,
				&client_parm.admin_addr)) < 0)
	RET_N_FREE(KADM_NO_ENCRYPT);	/* whoops... we got a lose
					 * here */

    /* here is the length of priv data.  receiver calcs
       size of authenticator by subtracting vno size, priv size, and
       sizeof(u_int32_t) (for the size indication) from total size */

    act_len += vts_long((u_int32_t) priv_len, &act_st, act_len);
#ifdef NOENCRYPTION
    cksum = 0;
#else
    cksum = des_quad_cksum((des_cblock *)priv_pak, (des_cblock *)0,
			   (long)priv_len, 0, &sess_key);
#endif
    if ((retdat = krb_mk_req(&authent, client_parm.sname, client_parm.sinst,
			     client_parm.krbrlm, cksum))) {
	/* authenticator? */
	RET_N_FREE(retdat + krb_err_base);
    }

    temp = (u_char *) realloc(act_st,
				act_len + authent.length + priv_len);
    if (temp == NULL) {
	clear_secrets();
	free(priv_pak);
	free(act_st);
	priv_pak = NULL;
	return KADM_NOMEM;
    }
    act_st = temp;

    memcpy((char *)act_st + act_len, authent.dat, authent.length);
    memcpy((char *)act_st + act_len + authent.length, priv_pak, priv_len);
    free(priv_pak);
    priv_pak = NULL;

    if ((retdat = kadm_cli_out(act_st,
			       act_len + authent.length + priv_len,
			       ret_dat, ret_siz)) != KADM_SUCCESS)
	RET_N_FREE(retdat);
    free(act_st);
    act_st = NULL;
#define RET_N_FREE2(r) {free(*ret_dat); *ret_dat = NULL; *ret_siz = 0; clear_secrets(); return(r);}

    /* first see if it's a YOULOUSE */
    if ((*ret_siz >= KADM_VERSIZE) &&
	!strncmp(KADM_ULOSE, (char *)*ret_dat, KADM_VERSIZE)) {
	unsigned char *p;
	/* it's a youlose packet */
	if (*ret_siz < KADM_VERSIZE + 4)
	    RET_N_FREE2(KADM_BAD_VER);
	p = (*ret_dat)+KADM_VERSIZE;
	retdat = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
	RET_N_FREE2(retdat);
    }
    /* need to decode the ret_dat */
    if ((retdat = krb_rd_priv(*ret_dat, (u_int32_t)*ret_siz, sess_sched,
			      &sess_key, &client_parm.admin_addr,
			      &client_parm.my_addr, &mdat)))
	RET_N_FREE2(retdat+krb_err_base);
    if (mdat.app_length < KADM_VERSIZE + 4)
	/* too short! */
	RET_N_FREE2(KADM_BAD_VER);
    if (strncmp((char *)mdat.app_data, KADM_VERSTR, KADM_VERSIZE))
	/* bad version */
	RET_N_FREE2(KADM_BAD_VER);
    {
	unsigned char *p = mdat.app_data+KADM_VERSIZE;
	retdat = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    }
    {
	int s=mdat.app_length - KADM_VERSIZE - 4;
	if(s<=0) s=1;
	if (!(return_dat = (u_char *)malloc(s)))
	    RET_N_FREE2(KADM_NOMEM);
    }
    memcpy(return_dat,
	   (char *) mdat.app_data + KADM_VERSIZE + 4,
	   mdat.app_length - KADM_VERSIZE - 4);
    free(*ret_dat);
    *ret_dat = NULL;
    clear_secrets();
    *ret_dat = return_dat;
    *ret_siz = mdat.app_length - KADM_VERSIZE - 4;
    return retdat;
}



/* 
 * kadm_change_pw_plain
 *
 * see kadm_change_pw
 *
 */
int kadm_change_pw_plain(unsigned char *newkey, char *password, char **pw_msg)
{
    int stsize, retc;	       /* stream size and return code */
    u_char *send_st;	       /* send stream */
    u_char *temp;
    u_char *ret_st;
    int ret_sz;
    int status;
    static char msg[128];

    if ((retc = kadm_cli_conn()) != KADM_SUCCESS)
	return retc ;
    /* possible problem with vts_long on a non-multiple of four boundary */

    stsize = 0;		       /* start of our output packet */
    send_st = (u_char *) malloc(1);/* to make it reallocable */
    if (send_st == NULL)
	return KADM_NOMEM;

    send_st[stsize++] = (u_char) CHANGE_PW;

    /* change key to stream */

    temp = realloc(send_st, stsize + 8);
    if (temp == NULL) {
        free(send_st);
	return KADM_NOMEM;
    }
    send_st = temp;

    memcpy(send_st + stsize + 4, newkey, 4); /* yes, this is backwards */
    memcpy(send_st + stsize, newkey + 4, 4);
    stsize += 8;
	
    if(password != NULL && password[0] != '\0')
	stsize += vts_string(password, &send_st, stsize);

    retc = kadm_cli_send(send_st, stsize, &ret_st, &ret_sz);
    free(send_st);
    send_st = NULL;
	
    if(retc != KADM_SUCCESS){
	status = stv_string(ret_st, msg, 0, sizeof(msg), ret_sz);
	if(status<0)
	    msg[0]=0;
	*pw_msg=msg;
    }

    if (ret_st != NULL) {
	free(ret_st);
	ret_st = NULL;
    }
	
    kadm_cli_disconn();
    return(retc);
}

/*
 * This function is here for compatibility with CNS
 */

int kadm_change_pw2(unsigned char *newkey, char *password, char **pw_msg)
{
    return kadm_change_pw_plain (newkey, password, pw_msg);
}


/*
 * kadm_change_pw
 * recieves    : key 
 *
 * Replaces the password (i.e. des key) of the caller with that specified in
 * key. Returns no actual data from the master server, since this is called
 * by a user 
 */

int kadm_change_pw(unsigned char *newkey)
{
    char *pw_msg;
    return kadm_change_pw_plain(newkey, "", &pw_msg);
}

/*
 * kadm_add
 * 	receives    : vals
 * 	returns     : vals 
 *
 * Adds and entry containing values to the database returns the values of the
 * entry, so if you leave certain fields blank you will be able to determine
 * the default values they are set to 
 */
int
kadm_add(Kadm_vals *vals)
{
    u_char *st, *st2;	       /* st will hold the stream of values */
    int st_len;		       /* st2 the final stream with opcode */
    int retc;		       /* return code from call */
    u_char *ret_st;
    int ret_sz;

    if ((retc = kadm_cli_conn()) != KADM_SUCCESS)
	return(retc);
    st_len = vals_to_stream(vals, &st);
    st2 = (u_char *) malloc((unsigned)(1 + st_len));
    if (st2 == NULL)
	return KADM_NOMEM;
    
    *st2 = (u_char) ADD_ENT;       /* here's the opcode */
    memcpy((char *) st2 + 1, st, st_len);	/* append st on */
    retc = kadm_cli_send(st2, st_len + 1, &ret_st, &ret_sz);
    free(st);
    st = NULL;
    free(st2);
    st2 = NULL;

    if (retc == KADM_SUCCESS) {
	/* ret_st has vals */
	if (stream_to_vals(ret_st, vals, ret_sz) < 0)
	    retc = KADM_LENGTH_ERROR;
	free(ret_st);
	ret_st = NULL;
    }
    kadm_cli_disconn();
    return(retc);
}

/*
 * kadm_mod
 * 	receives    : KTEXT, {values, values}
 *	returns     : CKSUM,  RETCODE, {values} 
 *	acl         : su, sms (as register or dealloc) 
 *
 * Modifies all entries corresponding to the first values so they match the
 * second values. returns the values for the changed entries in vals2
 */
int
kadm_mod(Kadm_vals *vals1, Kadm_vals *vals2)
{
    u_char *st, *st2;	       /* st will hold the stream of values */
    int st_len, nlen;	       /* st2 the final stream with opcode */
    u_char *ret_st;
    u_char *temp;
    int ret_sz;

    /* nlen is the length of second vals */
    int retc;		       /* return code from call */

    if ((retc = kadm_cli_conn()) != KADM_SUCCESS)
	return(retc);

    st_len = vals_to_stream(vals1, &st);
    st2 = (u_char *) malloc((unsigned)(1 + st_len));
    if (st2 == NULL)
	return KADM_NOMEM;
    
    *st2 = (u_char) MOD_ENT;       /* here's the opcode */
    memcpy((char *)st2 + 1, st, st_len++); /* append st on */
    free(st);
    st = NULL;

    nlen = vals_to_stream(vals2, &st);
    temp = (u_char *) realloc((char *) st2, (unsigned)(st_len + nlen));
    if (temp == NULL) {
         free(st2);
	 return KADM_NOMEM;
    }
    st2 = temp;
        
    memcpy((char *) st2 + st_len, st, nlen); /* append st on */
    retc = kadm_cli_send(st2, st_len + nlen, &ret_st, &ret_sz);

    free(st);
    st = NULL;
    free(st2);
    st2 = NULL;

    if (retc == KADM_SUCCESS) {
	/* ret_st has vals */
	if (stream_to_vals(ret_st, vals2, ret_sz) < 0)
	    retc = KADM_LENGTH_ERROR;
	free(ret_st);
	ret_st = NULL;
    }
    kadm_cli_disconn();
    return retc;
}


int
kadm_del(Kadm_vals *vals)
{
    unsigned char *st, *st2;	       /* st will hold the stream of values */
    int st_len;		       /* st2 the final stream with opcode */
    int retc;		       /* return code from call */
    u_char *ret_st;
    int ret_sz;
    
    if ((retc = kadm_cli_conn()) != KADM_SUCCESS)
	return retc;
    st_len = vals_to_stream(vals, &st);
    st2 = (unsigned char *) malloc(st_len + 1);
    if (st2 == NULL)
	return KADM_NOMEM;

    *st2 = DEL_ENT;       /* here's the opcode */
    memcpy(st2 + 1, st, st_len);	/* append st on */
    retc = kadm_cli_send(st2, st_len + 1, &ret_st, &ret_sz);
    free(st);
    st = NULL;
    free(st2);
    st2 = NULL;
    kadm_cli_disconn();
    return(retc);
}


/*
 * kadm_get
 * 	receives   : KTEXT, {values, flags} 
 *	returns    : CKSUM, RETCODE, {count, values, values, values}
 *	acl        : su 
 *
 * gets the fields requested by flags from all entries matching values returns
 * this data for each matching recipient, after a count of how many such
 * matches there were 
 */
int
kadm_get(Kadm_vals *vals, u_char *fl)
{
    int loop;		       /* for copying the fields data */
    u_char *st, *st2;	       /* st will hold the stream of values */
    int st_len;		       /* st2 the final stream with opcode */
    int retc;		       /* return code from call */
    u_char *ret_st;
    int ret_sz;

    if ((retc = kadm_cli_conn()) != KADM_SUCCESS)
	return(retc);
    st_len = vals_to_stream(vals, &st);
    st2 = (u_char *) malloc((unsigned)(1 + st_len + FLDSZ));
    if (st2 == NULL)
	return KADM_NOMEM;

    *st2 = (u_char) GET_ENT;       /* here's the opcode */
    memcpy((char *)st2 + 1, st, st_len); /* append st on */
    for (loop = FLDSZ - 1; loop >= 0; loop--)
	*(st2 + st_len + FLDSZ - loop) = fl[loop]; /* append the flags */
    retc = kadm_cli_send(st2, st_len + 1 + FLDSZ,  &ret_st, &ret_sz);
    free(st);
    st = NULL;
    free(st2);
    st2 = NULL;
    if (retc == KADM_SUCCESS) {
	/* ret_st has vals */
	if (stream_to_vals(ret_st, vals, ret_sz) < 0)
	    retc = KADM_LENGTH_ERROR;
	free(ret_st);
	ret_st = NULL;
    }
    kadm_cli_disconn();
    return(retc);
}
