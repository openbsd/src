/*
 * Copyright (c) 1997 - 2003 Kungliga Tekniska Högskolan
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

/* $KTH: krb5-v4compat.h,v 1.6 2005/04/23 19:38:16 lha Exp $ */

#ifndef __KRB5_V4COMPAT_H__
#define __KRB5_V4COMPAT_H__

/* 
 * This file must only be included with v4 compat glue stuff in
 * heimdal sources.
 *
 * It MUST NOT be installed.
 */

#define		KRB_PROT_VERSION 	4

#define		AUTH_MSG_KDC_REQUEST			 (1<<1)
#define 	AUTH_MSG_KDC_REPLY			 (2<<1)
#define		AUTH_MSG_APPL_REQUEST			 (3<<1)
#define		AUTH_MSG_APPL_REQUEST_MUTUAL		 (4<<1)
#define		AUTH_MSG_ERR_REPLY			 (5<<1)
#define		AUTH_MSG_PRIVATE			 (6<<1)
#define		AUTH_MSG_SAFE				 (7<<1)
#define		AUTH_MSG_APPL_ERR			 (8<<1)
#define		AUTH_MSG_KDC_FORWARD			 (9<<1)
#define		AUTH_MSG_KDC_RENEW			(10<<1)
#define 	AUTH_MSG_DIE				(63<<1)

/* values for kerb error codes */

#define		KERB_ERR_OK				 0
#define		KERB_ERR_NAME_EXP			 1
#define		KERB_ERR_SERVICE_EXP			 2
#define		KERB_ERR_AUTH_EXP			 3
#define		KERB_ERR_PKT_VER			 4
#define		KERB_ERR_NAME_MAST_KEY_VER		 5
#define		KERB_ERR_SERV_MAST_KEY_VER		 6
#define		KERB_ERR_BYTE_ORDER			 7
#define		KERB_ERR_PRINCIPAL_UNKNOWN		 8
#define		KERB_ERR_PRINCIPAL_NOT_UNIQUE		 9
#define		KERB_ERR_NULL_KEY			10
#define		KERB_ERR_TIMEOUT			11


/* Error codes returned from the KDC */
#define		KDC_OK		0	/* Request OK */
#define		KDC_NAME_EXP	1	/* Principal expired */
#define		KDC_SERVICE_EXP	2	/* Service expired */
#define		KDC_AUTH_EXP	3	/* Auth expired */
#define		KDC_PKT_VER	4	/* Protocol version unknown */
#define		KDC_P_MKEY_VER	5	/* Wrong master key version */
#define		KDC_S_MKEY_VER 	6	/* Wrong master key version */
#define		KDC_BYTE_ORDER	7	/* Byte order unknown */
#define		KDC_PR_UNKNOWN	8	/* Principal unknown */
#define		KDC_PR_N_UNIQUE 9	/* Principal not unique */
#define		KDC_NULL_KEY   10	/* Principal has null key */
#define		KDC_GEN_ERR    20	/* Generic error from KDC */

/* General definitions */
#define		KSUCCESS	0
#define		KFAILURE	255

/* Values returned by rd_ap_req */
#define		RD_AP_OK	0	/* Request authentic */
#define		RD_AP_UNDEC    31	/* Can't decode authenticator */
#define		RD_AP_EXP      32	/* Ticket expired */
#define		RD_AP_NYV      33	/* Ticket not yet valid */
#define		RD_AP_REPEAT   34	/* Repeated request */
#define		RD_AP_NOT_US   35	/* The ticket isn't for us */
#define		RD_AP_INCON    36	/* Request is inconsistent */
#define		RD_AP_TIME     37	/* delta_t too big */
#define		RD_AP_BADD     38	/* Incorrect net address */
#define		RD_AP_VERSION  39	/* protocol version mismatch */
#define		RD_AP_MSG_TYPE 40	/* invalid msg type */
#define		RD_AP_MODIFIED 41	/* message stream modified */
#define		RD_AP_ORDER    42	/* message out of order */
#define		RD_AP_UNAUTHOR 43	/* unauthorized request */

/* */

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
    char    session[8];		/* Session key */
    int     lifetime;		/* Lifetime */
    int     kvno;		/* Key version number */
    struct ktext ticket_st;	/* The ticket itself */
    int32_t    issue_date;	/* The issue time */
    char    pname[ANAME_SZ];	/* Principal's name */
    char    pinst[INST_SZ];	/* Principal's instance */
};

#define TKTLIFENUMFIXED 64
#define TKTLIFEMINFIXED 0x80
#define TKTLIFEMAXFIXED 0xBF
#define TKTLIFENOEXPIRE 0xFF
#define MAXTKTLIFETIME	(30*24*3600)	/* 30 days */
#ifndef NEVERDATE
#define NEVERDATE ((time_t)0x7fffffffL)
#endif

#define		KERB_ERR_NULL_KEY	10

#define 	CLOCK_SKEW	5*60

#ifndef TKT_ROOT
#define TKT_ROOT "/tmp/tkt"
#endif

struct _krb5_krb_auth_data {
    int8_t  k_flags;		/* Flags from ticket */
    char    *pname;		/* Principal's name */
    char    *pinst;		/* His Instance */
    char    *prealm;		/* His Realm */
    u_int32_t checksum;		/* Data checksum (opt) */
    krb5_keyblock session;	/* Session Key */
    unsigned char life;		/* Life of ticket */
    u_int32_t time_sec;		/* Time ticket issued */
    u_int32_t address;		/* Address in ticket */
};

time_t		_krb5_krb_life_to_time (int, int);
int		_krb5_krb_time_to_life (time_t, time_t);
krb5_error_code	_krb5_krb_tf_setup (krb5_context, struct credentials *,
				    const char *, int);
krb5_error_code	_krb5_krb_dest_tkt(krb5_context, const char *);

#define krb_time_to_life	_krb5_krb_time_to_life
#define krb_life_to_time	_krb5_krb_life_to_time

#endif /*  __KRB5_V4COMPAT_H__ */
