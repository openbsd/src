/*
 * This software may now be redistributed outside the US.
 *
 * $Source: /home/cvs/src/kerberosIV/krb/Attic/get_ad_tkt.c,v $
 *
 * $Locker:  $
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

#include "krb_locl.h"

static int swap_bytes;

/*
 * Given a pointer to an AUTH_MSG_KDC_REPLY packet, return the length of
 * its ciphertext portion.  The external variable "swap_bytes" is assumed
 * to have been set to indicate whether or not the packet is in local
 * byte order.  pkt_clen() takes this into account when reading the
 * ciphertext length out of the packet.
 */

static int
pkt_clen(pkt)
	KTEXT pkt;
{
    static unsigned short temp,temp2;
    int clen = 0;

    /* Start of ticket list */
    unsigned char *ptr = pkt_a_realm(pkt) + 10
	+ strlen((char *)pkt_a_realm(pkt));

    /* Finally the length */
    bcopy((char *)(++ptr),(char *)&temp,2); /* alignment */
    if (swap_bytes) {
        /* assume a short is 2 bytes?? */
        swab((char *)&temp,(char *)&temp2,2);
        temp = temp2;
    }

    clen = (int) temp;

    if (krb_debug)
	printf("Clen is %d\n",clen);
    return(clen);
}

/* use the bsd time.h struct defs for PC too! */
#include <sys/time.h>
#include <sys/types.h>

static struct timeval tt_local = { 0, 0 };
static unsigned long rep_err_code;

/*
 * get_ad_tkt obtains a new service ticket from Kerberos, using
 * the ticket-granting ticket which must be in the ticket file.
 * It is typically called by krb_mk_req() when the client side
 * of an application is creating authentication information to be
 * sent to the server side.
 *
 * get_ad_tkt takes four arguments: three pointers to strings which
 * contain the name, instance, and realm of the service for which the
 * ticket is to be obtained; and an integer indicating the desired
 * lifetime of the ticket.
 *
 * It returns an error status if the ticket couldn't be obtained,
 * or AD_OK if all went well.  The ticket is stored in the ticket
 * cache.
 *
 * The request sent to the Kerberos ticket-granting service looks
 * like this:
 *
 * pkt->dat
 *
 * TEXT			original contents of	authenticator+ticket
 *			pkt->dat		built in krb_mk_req call
 * 
 * 4 bytes		time_ws			always 0 (?)
 * char			lifetime		lifetime argument passed
 * string		service			service name argument
 * string		sinstance		service instance arg.
 *
 * See "prot.h" for the reply packet layout and definitions of the
 * extraction macros like pkt_version(), pkt_msg_type(), etc.
 */

int
get_ad_tkt(service, sinstance, realm, lifetime)
	char *service;
	char *sinstance;
	char *realm;
	int lifetime;
{
    static KTEXT_ST pkt_st;
    KTEXT pkt = & pkt_st;	/* Packet to KDC */
    static KTEXT_ST rpkt_st;
    KTEXT rpkt = &rpkt_st;	/* Returned packet */
    static KTEXT_ST cip_st;
    KTEXT cip = &cip_st;	/* Returned Ciphertext */
    static KTEXT_ST tkt_st;
    KTEXT tkt = &tkt_st;	/* Current ticket */
    des_cblock ses;                /* Session key for tkt */
    CREDENTIALS cr;
    int kvno;			/* Kvno for session key */
    char lrealm[REALM_SZ];
    des_cblock key;		/* Key for decrypting cipher */
    des_key_schedule key_s;
    long time_ws = 0;

    char s_name[SNAME_SZ];
    char s_instance[INST_SZ];
    int msg_byte_order;
    int kerror;
    char rlm[REALM_SZ];
    char *ptr;

    unsigned long kdc_time;   /* KDC time */

    if ((kerror = krb_get_tf_realm(TKT_FILE, lrealm)) != KSUCCESS)
	return(kerror);

    /* Create skeleton of packet to be sent */
    (void) gettimeofday(&tt_local,(struct timezone *) 0);

    pkt->length = 0;

    /*
     * Look for the session key (and other stuff we don't need)
     * in the ticket file for krbtgt.realm@lrealm where "realm" 
     * is the service's realm (passed in "realm" argument) and 
     * lrealm is the realm of our initial ticket.  If we don't 
     * have this, we will try to get it.
     */
    
    if ((kerror = krb_get_cred("krbtgt",realm,lrealm,&cr)) != KSUCCESS) {
	/*
	 * If realm == lrealm, we have no hope, so let's not even try.
	 */
	if ((strncmp(realm, lrealm, REALM_SZ)) == 0)
	    return(AD_NOTGT);
	else{
	    if ((kerror = 
		 get_ad_tkt("krbtgt",realm,lrealm,lifetime)) != KSUCCESS)
		return(kerror);
	    if ((kerror = krb_get_cred("krbtgt",realm,lrealm,&cr)) != KSUCCESS)
		return(kerror);
	}
    }
    
    /*
     * Make up a request packet to the "krbtgt.realm@lrealm".
     * Start by calling krb_mk_req() which puts ticket+authenticator
     * into "pkt".  Then tack other stuff on the end.
     */
    
    kerror = krb_mk_req(pkt,"krbtgt",realm,lrealm,0L);

    if (kerror)
	return(AD_NOTGT);

    /* timestamp */
    bcopy((char *) &time_ws,(char *) (pkt->dat+pkt->length),4);
    pkt->length += 4;
    *(pkt->dat+(pkt->length)++) = (char) lifetime;
    (void) strcpy((char *) (pkt->dat+pkt->length),service);
    pkt->length += 1 + strlen(service);
    (void) strcpy((char *)(pkt->dat+pkt->length),sinstance);
    pkt->length += 1 + strlen(sinstance);

    rpkt->length = 0;

    /* Send the request to the local ticket-granting server */
    if ((kerror = send_to_kdc(pkt, rpkt, realm))) return(kerror);

    /* check packet version of the returned packet */
    if (pkt_version(rpkt) != KRB_PROT_VERSION )
        return(INTK_PROT);

    /* Check byte order */
    msg_byte_order = pkt_msg_type(rpkt) & 1;
    swap_bytes = 0;
    if (msg_byte_order != HOST_BYTE_ORDER)
	swap_bytes++;

    switch (pkt_msg_type(rpkt) & ~1) {
    case AUTH_MSG_KDC_REPLY:
	break;
    case AUTH_MSG_ERR_REPLY:
	bcopy(pkt_err_code(rpkt), (char *) &rep_err_code, 4);
	if (swap_bytes)
	    swap_u_long(rep_err_code);
	return(rep_err_code);

    default:
	return(INTK_PROT);
    }

    /* Extract the ciphertext */
    cip->length = pkt_clen(rpkt);       /* let clen do the swap */

    bcopy((char *) pkt_cipher(rpkt),(char *) (cip->dat),cip->length);

#ifndef NOENCRYPTION
    /* Attempt to decrypt it */

    des_key_sched(&cr.session,key_s);
    if (krb_debug)  printf("About to do decryption ...");
    des_pcbc_encrypt((des_cblock *)cip->dat,(des_cblock *)cip->dat,
                 (long) cip->length,key_s,&cr.session,0);
#endif /* !NOENCRYPTION */
    /* Get rid of all traces of key */
    bzero((char *) cr.session, sizeof(key));
    bzero((char *) key_s, sizeof(key_s));

    ptr = (char *) cip->dat;

    bcopy(ptr,(char *)ses,8);
    ptr += 8;

    (void) strcpy(s_name,ptr);
    ptr += strlen(s_name) + 1;

    (void) strcpy(s_instance,ptr);
    ptr += strlen(s_instance) + 1;

    (void) strcpy(rlm,ptr);
    ptr += strlen(rlm) + 1;

    lifetime = (unsigned char) ptr[0];
    kvno = (unsigned long) ptr[1];
    tkt->length = (int) ptr[2];
    ptr += 3;
    bcopy(ptr,(char *)(tkt->dat),tkt->length);
    ptr += tkt->length;

    if (strcmp(s_name, service) || strcmp(s_instance, sinstance) ||
        strcmp(rlm, realm))	/* not what we asked for */
	return(INTK_ERR);	/* we need a better code here XXX */

    /* check KDC time stamp */
    bcopy(ptr,(char *)&kdc_time,4); /* Time (coarse) */
    if (swap_bytes) swap_u_long(kdc_time);

    ptr += 4;

    (void) gettimeofday(&tt_local,(struct timezone *) 0);
    if (abs((int)(tt_local.tv_sec - kdc_time)) > CLOCK_SKEW) {
        return(RD_AP_TIME);		/* XXX should probably be better
					   code */
    }

    if ((kerror = save_credentials(s_name,s_instance,rlm,ses,lifetime,
				  kvno,tkt,tt_local.tv_sec)))
	return(kerror);

    return(AD_OK);
}
