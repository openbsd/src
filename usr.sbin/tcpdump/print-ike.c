/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Format and print ike (isakmp) packets.
 *	By Tero Kivinen <kivinen@ssh.fi>, Tero Mononen <tmo@ssh.fi>,
 *         Tatu Ylonen <ylo@ssh.fi> and Timo J. Rinne <tri@ssh.fi>
 *         in co-operation with SSH Communications Security, Espoo, Finland
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /home/cvs/src/usr.sbin/tcpdump/print-ike.c,v 1.1 1999/07/28 20:41:36 jakob Exp $ (XXX)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#if __STDC__
struct mbuf;
struct rtentry;
#endif
#include <net/if.h>

#include <netinet/in.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "interface.h"
#include "addrtoname.h"
#ifdef MODEMASK
#undef MODEMASK					/* Solaris sucks */
#endif

struct isakmp_header {
	u_char  init_cookie[8];
	u_char  resp_cookie[8];
	u_char  nextpayload;
	u_char  version;
	u_char  exgtype;
	u_char  flags;
	u_char  msgid[4];
	u_int32_t length;
	u_char  payloads[0];  
};

static int isakmp_doi;

#define FLAGS_ENCRYPTION	1
#define FLAGS_COMMIT		2

#define PAYLOAD_NONE		0
#define PAYLOAD_SA		1
#define PAYLOAD_PROPOSAL	2
#define PAYLOAD_TRANSFORM	3
#define PAYLOAD_KE		4
#define PAYLOAD_ID		5
#define PAYLOAD_CERT		6
#define PAYLOAD_CERTREQUEST	7
#define PAYLOAD_HASH		8
#define PAYLOAD_SIG		9
#define PAYLOAD_NONCE		10
#define PAYLOAD_NOTIFICATION	11
#define PAYLOAD_DELETE		12

#define IPSEC_DOI		1

static void isakmp_pl_print(register u_char type, 
			    register u_char *payload, 
			    register int paylen);

/*
 * Print isakmp requests
 */
void isakmp_print(register const u_char *cp, register int length)
{
	struct isakmp_header *ih;
	register const u_char *ep;
	int mode, version, leapind;
	u_char *payload;
	u_char  nextpayload, np1;
	u_int   paylen;
	int encrypted;

	encrypted = 0;

#ifdef TCHECK
#undef TCHECK
#endif
#define TCHECK(var, l) if ((u_char *)&(var) > ep - l) goto trunc
	
	ih = (struct isakmp_header *)cp;
	/* Note funny sized packets */
	if (length < 20) {
		(void)printf(" [len=%d]", length);
	}

	/* 'ep' points to the end of avaible data. */
	ep = snapend;

	printf(" isakmp");

	printf(" v%d.%d\n\t", ih->version >> 4, ih->version & 0xf);

	if (ih->flags & FLAGS_ENCRYPTION) {
		printf(" encrypted");
		encrypted = 1;
	}
	
	if (ih->flags & FLAGS_COMMIT) {
		printf(" commit");
	}

	printf(" cookie: %02x%02x%02x%02x%02x%02x%02x%02x->%02x%02x%02x%02x%02x%02x%02x%02x\n\t",
	       ih->init_cookie[0], ih->init_cookie[1],
	       ih->init_cookie[2], ih->init_cookie[3], 
	       ih->init_cookie[4], ih->init_cookie[5], 
	       ih->init_cookie[6], ih->init_cookie[7], 
	       ih->resp_cookie[0], ih->resp_cookie[1], 
	       ih->resp_cookie[2], ih->resp_cookie[3], 
	       ih->resp_cookie[4], ih->resp_cookie[5], 
	       ih->resp_cookie[6], ih->resp_cookie[7]);

	TCHECK(ih->msgid, sizeof(ih->msgid));
	printf(" msgid:%02x%02x%02x%02x",
	       ih->msgid[0], ih->msgid[1],
	       ih->msgid[2], ih->msgid[3]);

	TCHECK(ih->length, sizeof(ih->length));
	printf(" length %d", ntohl(ih->length));
	
	if (ih->version > 16) {
		printf(" new version");
		return;
	}

	/* now, process payloads */
	payload = ih->payloads;
	nextpayload = ih->nextpayload;

	/* if encrypted, then open special file for encryption keys */
	if (encrypted) {
		/* decrypt XXX */
		return;
	}

	while (nextpayload != 0) {
		np1 = payload[0];
		paylen = (payload[2] << 8) + payload[3];
		printf("\n\t\tload: %02x len: %04x",
		       nextpayload, paylen);
		TCHECK(payload[0], paylen);  
		isakmp_pl_print(nextpayload, payload, paylen);
		payload += paylen;
		nextpayload = np1;
	}

	return;

trunc:
	fputs(" [|isakmp]", stdout);
}

void isakmp_sa_print(register u_char *buf, register int len)
{
	isakmp_doi = ntohl((*(u_int32_t *)(buf+4)));
	printf(" SA doi: %d",
	       isakmp_doi, (isakmp_doi == IPSEC_DOI ? "(ipsec)" : ""));
	printf(" situation\n");
}
	
void isakmp_proposal_print(register u_char *buf, register int len)
{
	u_char *spis;
	int spisize, numspi, i;

	spisize = buf[6];
	numspi  = buf[7];
	  

	printf(" proposal number: %d protocol: %d spisize: %d #spi: %d", 
	       buf[4], buf[5], spisize, numspi);

	spis = buf+8;
	while (numspi) {
		printf("\n\t ");
		for (i=0; i<spisize; i++) {
			printf("%02x", *spis);
			spis++;
		}
	}
}
	
void isakmp_ke_print(register u_char *buf, register int len)
{
	if (isakmp_doi != IPSEC_DOI) {
		printf("KE unknown doi\n");
		return;
	}
}
	
void isakmp_pl_print(register u_char type, 
		     register u_char *buf, 
		     register int len)
{
	switch(type) {
	case PAYLOAD_NONE:
		return;
	case PAYLOAD_SA:
		isakmp_sa_print(buf, len);
		break;
	    
	case PAYLOAD_PROPOSAL:
		isakmp_proposal_print(buf, len);
		break;
	    
	case PAYLOAD_TRANSFORM:
		break;
	    
	case PAYLOAD_KE:
		isakmp_ke_print(buf, len);
		break;
	    
	case PAYLOAD_ID:
	case PAYLOAD_CERT:
	case PAYLOAD_CERTREQUEST:
	case PAYLOAD_HASH:
	case PAYLOAD_SIG:
		break;
	    
	case PAYLOAD_NONCE:
#if 0
		isakmp_nonce_print(buf, len);
#endif
		break;
	    
	case PAYLOAD_NOTIFICATION:
	case PAYLOAD_DELETE:
	default:
	}
}
