/*	$OpenBSD: print-ike.c,v 1.4 2000/10/03 14:25:47 ho Exp $	*/

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
 *
 * Rewritten and extended (quite a lot, too) by Hakan Olsson <ho@openbsd.org>
 *
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /home/cvs/src/usr.sbin/tcpdump/print-ike.c,v 1.4 2000/10/03 14:25:47 ho Exp $ (XXX)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

#ifdef __STDC__
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
#include "ike.h"

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

struct notification_payload {
	u_char    next_payload;
	u_char    reserved;
	u_int16_t payload_length;
	u_int32_t doi;
	u_char    protocol_id;
  	u_char    spi_size;
  	u_int16_t type;
	u_char    data[0];
};

static int isakmp_doi;

static void isakmp_pl_print(register u_char type, register u_char *payload);

int ike_tab_level = 0;

#define SMALL_TABS 4
#define SPACES "                                                   "
const char *
ike_tab_offset (void)
{
  const char *p, *endline;
  static const char line[] = SPACES;

  endline = line + sizeof line - 1;
  p = endline - SMALL_TABS * (ike_tab_level);  

  return (p > line ? p : line);
}

/*
 * Print isakmp requests
 */
void
isakmp_print (register const u_char *cp, register u_int length)
{
	struct isakmp_header *ih;
	register const u_char *ep;
	u_char *payload;
	u_char  nextpayload;
	int encrypted;
	static const char *exgtypes[] = IKE_EXCHANGE_TYPES_INITIALIZER;

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

	printf(" v%d.%d", ih->version >> 4, ih->version & 0xf);

	printf(" exchange ");
	if (ih->exgtype < (sizeof exgtypes/sizeof exgtypes[0]))
	        printf("%s", exgtypes[ih->exgtype]);
	else
	        printf("%d (unknown)", ih->exgtype);

	if (ih->flags & FLAGS_ENCRYPTION) {
		printf(" encrypted");
		encrypted = 1;
	}
	
	if (ih->flags & FLAGS_COMMIT) {
		printf(" commit");
	}

	printf("\n\tcookie: %02x%02x%02x%02x%02x%02x%02x%02x->"
	       "%02x%02x%02x%02x%02x%02x%02x%02x",
	       ih->init_cookie[0], ih->init_cookie[1],
	       ih->init_cookie[2], ih->init_cookie[3], 
	       ih->init_cookie[4], ih->init_cookie[5], 
	       ih->init_cookie[6], ih->init_cookie[7], 
	       ih->resp_cookie[0], ih->resp_cookie[1], 
	       ih->resp_cookie[2], ih->resp_cookie[3], 
	       ih->resp_cookie[4], ih->resp_cookie[5], 
	       ih->resp_cookie[6], ih->resp_cookie[7]);

	TCHECK(ih->msgid, sizeof(ih->msgid));
	printf(" msgid: %02x%02x%02x%02x",
	       ih->msgid[0], ih->msgid[1],
	       ih->msgid[2], ih->msgid[3]);

	TCHECK(ih->length, sizeof(ih->length));
	printf(" len: %d", ntohl(ih->length));
	
	if (ih->version > 16) {
		printf(" new version");
		return;
	}

	payload = ih->payloads;
	nextpayload = ih->nextpayload;

	/* if encrypted, then open special file for encryption keys */
	if (encrypted) {
		/* decrypt XXX */
		return;
	}

	/* if verbose, print payload data */
	if (vflag)
	        isakmp_pl_print(nextpayload, payload);

	return;

trunc:
	fputs(" [|isakmp]", stdout);
}

void
isakmp_sa_print (register u_char *buf, register int len)
{
	u_int32_t situation = ntohl(*(u_int32_t *)(buf + 4));
	isakmp_doi = ntohl((*(u_int32_t *)buf));
	printf(" DOI: %d", isakmp_doi);
	if (isakmp_doi == IPSEC_DOI) {
	        printf("(IPSEC) situation: ");
		if (situation & IKE_SITUATION_IDENTITY_ONLY)
		        printf("IDENTITY_ONLY ");
		if (situation & IKE_SITUATION_SECRECY)
		        printf("SECRECY ");
		if (situation & IKE_SITUATION_INTEGRITY)
		        printf("INTEGRITY ");
		if ((situation & IKE_SITUATION_MASK) == 0)
		        printf("0x%x (unknown)", situation);
	        isakmp_pl_print (PAYLOAD_PROPOSAL, buf + 8);
	}
	else
	        printf(" situation: (unknown)");
}

int
isakmp_attribute_print (register u_char *buf)
{
	static char *attrs[] = IKE_ATTR_INITIALIZER;
	static char *attr_enc[] = IKE_ATTR_ENCRYPT_INITIALIZER;
	static char *attr_hash[] = IKE_ATTR_HASH_INITIALIZER;
	static char *attr_auth[] = IKE_ATTR_AUTH_INITIALIZER;
	static char *attr_gdesc[] = IKE_ATTR_GROUP_DESC_INITIALIZER;
	static char *attr_gtype[] = IKE_ATTR_GROUP_INITIALIZER;
	static char *attr_ltype[] = IKE_ATTR_SA_DURATION_INITIALIZER;

	unsigned short type = buf[0]<<8 | buf[1];
	unsigned short length = 0, p;

	printf("\n\t%sattribute %s = ", ike_tab_offset(),
	       ((type & 0x7fff) < sizeof attrs / sizeof attrs[0] ?
		attrs[type & 0x7fff] : "unknown"));
	if (!(type >> 15)) {
	        length = buf[2]<<8 | buf[3];
	        for (p = 0; p < length; p++)
	               printf("%02x", (char)*(buf + 4 + p));
	}
	else {
	        p = buf[2]<<8 | buf[3];

#define CASE_PRINT(TYPE,var) \
        case TYPE : \
                if (p < sizeof var / sizeof var [0]) \
                        printf("%s", var [p]); \
                else \
                        printf("%d (unknown)", p); \
                break;
 
		switch(type & 0x7fff) {
		CASE_PRINT(IKE_ATTR_ENCRYPTION_ALGORITHM, attr_enc);
		CASE_PRINT(IKE_ATTR_HASH_ALGORITHM, attr_hash);
		CASE_PRINT(IKE_ATTR_AUTHENTICATION_METHOD, attr_auth);
		CASE_PRINT(IKE_ATTR_GROUP_DESC, attr_gdesc);
		CASE_PRINT(IKE_ATTR_GROUP_TYPE, attr_gtype);
		CASE_PRINT(IKE_ATTR_LIFE_TYPE, attr_ltype);
		default:
		  printf("%d", p);
		}
	}
	return length + 4;
}

void
isakmp_transform_print (register u_char *buf, register int len)
{
	u_char *attr = buf + 4;

	printf("\n\t%stransform: %d ID: %d", ike_tab_offset(),
	       (char)buf[0], (char)buf[1]);
	
	ike_tab_level++;
	while((int)(attr - buf) < len - 4)  /* Skip last 'NONE' attr */
	        attr += isakmp_attribute_print(attr);
	ike_tab_level--;
}

void
isakmp_proposal_print (register u_char *buf, register int len)
{
	printf(" proposal: %d proto: %d(%s) spisz: %d xforms: %d", 
	       buf[0], buf[1], (buf[1] == PROTO_ISAKMP ? "ISAKMP" : "unknown"),
	       buf[2], buf[3]);

	if((char)buf[3] > 0)
	       isakmp_pl_print(PAYLOAD_TRANSFORM, buf+4);
}

void
isakmp_ke_print (register u_char *buf, register int len)
{
	if (isakmp_doi != IPSEC_DOI)
		return;

	printf(" <KE payload data (not shown)> len: %d", len);
}
	
void
isakmp_id_print (register u_char *buf, register int len)
{
	if (isakmp_doi != IPSEC_DOI) 
		return;

	printf(" <ID payload data (not shown)> len: %d", len);
}

void
isakmp_notification_print (register u_char *buf, register int len)
{
  	static const char *nftypes[] = IKE_NOTIFY_TYPES_INITIALIZER;
  	struct notification_payload *np = (struct notification_payload *)buf;

	if (len < sizeof (struct notification_payload)) {
		printf (" (|len)");
		return;
	}

	np->doi  = ntohl (np->doi);
	np->type = ntohs (np->type);

	if (np->doi != ISAKMP_DOI && np->doi != IPSEC_DOI) {
		printf (" (unknown DOI)");
		return;
	}

	printf ("\n\t%snotification: ", ike_tab_offset());

	if (np->type > 0 && np->type < (sizeof nftypes / sizeof nftypes[0]))
		printf("%s", nftypes[np->type]);
	else
	  	printf("%d (unknown)", np->type);
	
  	return;
}
	
void
isakmp_vendor_print (register u_char *buf, register int len)
{
	u_char *p = buf;

	if (isakmp_doi != IPSEC_DOI) 
		return;

	printf(" \"");
	for(p = buf; (int)(p - buf) < len; p++)
	        printf("%c",(isprint(*p) ? *p : '.'));
	printf("\"");
}

void
isakmp_pl_print (register u_char type, register u_char *buf)
{
	static const char *pltypes[] = IKE_PAYLOAD_TYPES_INITIALIZER;
	int next_type = buf[0];
	int this_len = buf[2]<<8 | buf[3];

	printf("\n\t%spayload: %s len: %d", ike_tab_offset(),
	       (type < (sizeof pltypes/sizeof pltypes[0]) ?
		pltypes[type] : "<unknown>"), this_len);

	if ((u_char *)&(buf[0]) > snapend - this_len) 
	  goto pltrunc;

	ike_tab_level++;
	switch(type) {
	case PAYLOAD_NONE:
		return;

	case PAYLOAD_SA:
		isakmp_sa_print(buf+4, this_len);
		break;
	    
	case PAYLOAD_PROPOSAL:
		isakmp_proposal_print(buf+4, this_len);
		break;
	    
	case PAYLOAD_TRANSFORM:
	        isakmp_transform_print(buf+4, this_len);
		break;
	    
	case PAYLOAD_KE:
		isakmp_ke_print(buf+4, this_len);
		break;
	    
	case PAYLOAD_ID:
	        isakmp_id_print(buf+4, this_len);
		break;

	case PAYLOAD_CERT:
	case PAYLOAD_CERTREQUEST:
	case PAYLOAD_HASH:
	case PAYLOAD_SIG:
	case PAYLOAD_NONCE:
	case PAYLOAD_DELETE:
		break;
	    
	case PAYLOAD_NOTIFICATION:
	  	isakmp_notification_print(buf, this_len);
		break;

	case PAYLOAD_VENDOR:
	        isakmp_vendor_print(buf+4, this_len);
		break;

	default:
	}
	ike_tab_level--;

	if(next_type)  /* Recurse over next payload */
	        isakmp_pl_print(next_type, buf + this_len);

	return;

pltrunc:
	fputs(" [|isakmp]", stdout);
}
