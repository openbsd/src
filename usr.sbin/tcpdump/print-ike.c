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
    "@(#) $Header: /home/cvs/src/usr.sbin/tcpdump/print-ike.c,v 1.3 1999/09/30 07:22:55 ho Exp $ (XXX)";
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

/* XXX Perhaps move these to an <ike.h> file? */

#define IPSEC_DOI		1

#define PROTO_ISAKMP            1

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
#define PAYLOAD_VENDOR          13

#define IKE_ATTR_ENCRYPTION_ALGORITHM	1
#define IKE_ATTR_HASH_ALGORITHM		2
#define IKE_ATTR_AUTHENTICATION_METHOD	3
#define IKE_ATTR_GROUP_DESC		4
#define IKE_ATTR_GROUP_TYPE		5
#define IKE_ATTR_LIFE_TYPE		11

#define IKE_ATTR_ENCRYPT_INITIALIZER					\
	{ "NONE", "DES_CBS", "IDEA_CBC", "BLOWFISH_CBC",		\
	  "RC5_R16_B64_CBC", "3DES_CBC", "CAST_CBC",			\
	}
#define IKE_ATTR_HASH_INITIALIZER					\
	{ "NONE", "MD5", "SHA", "TIGER",				\
	}
#define IKE_ATTR_AUTH_INITIALIZER					\
	{ "NONE", "PRE_SHARED", "DSS", "RSA_SIG",			\
	  "RSA_ENC", "RSA_ENC_REV",					\
	}
#define IKE_ATTR_GROUP_DESC_INITIALIZER					\
	{ "NONE", "MODP_768", "MODP_1024",				\
	  "E2CN_155", "E2CN_185", "MODP_1536",				\
	}
#define IKE_ATTR_GROUP_INITIALIZER					\
	{ "NONE", "MODP", "ECP", "E2CN",				\
	}
#define IKE_ATTR_SA_DURATION_INITIALIZER				\
	{ "NONE", "SECONDS", "KILOBYTES",				\
	}

#define IKE_ATTR_INITIALIZER						\
	{ "NONE", 			/* 0 (not in RFC) */		\
	  "ENCRYPTION_ALGORITHM", 	/* 1 */				\
	  "HASH_ALGORITHM",		/* 2 */				\
	  "AUTHENTICATION_METHOD",	/* 3 */				\
	  "GROUP_DESCRIPTION",		/* 4 */				\
	  "GROUP_TYPE",			/* 5 */				\
	  "GROUP_PRIME",		/* 6 */				\
	  "GROUP_GENERATOR_1",		/* 7 */				\
	  "GROUP_GENERATOR_2",		/* 8 */				\
	  "GROUP_CURVE_1",		/* 9 */				\
	  "GROUP_CURVE_2",		/* 10 */			\
	  "LIFE_TYPE",			/* 11 */			\
	  "LIFE_DURATION",		/* 12 */			\
	  "PRF",			/* 13 */			\
	  "KEY_LENGTH",			/* 14 */			\
	  "FIELD_SIZE",			/* 15 */			\
	  "GROUP_ORDER",		/* 16 */			\
	}

#define IKE_SITUATION_IDENTITY_ONLY	1
#define IKE_SITUATION_SECRECY		2
#define IKE_SITUATION_INTEGRITY		4
/* Mask is all the above, i.e 1+2+4 = 7 */
#define IKE_SITUATION_MASK		7

#define IKE_PAYLOAD_TYPES_INITIALIZER			\
	{ "NONE",		/*  0 */		\
	  "SA",			/*  1 */		\
	  "PROPOSAL",		/*  2 */		\
	  "TRANSFORM",		/*  3 */		\
	  "KEY_EXCH",		/*  4 */		\
	  "ID",			/*  5 */		\
	  "CERT",		/*  6 */		\
	  "CERTREQUEST",	/*  7 */		\
	  "HASH",		/*  8 */		\
	  "SIG",		/*  9 */		\
	  "NONCE",		/* 10 */		\
	  "NOTIFICATION",	/* 11 */		\
	  "DELETE",		/* 12 */		\
	  "VENDOR",		/* 13 */		\
	}

/* Exchange types */
#define EXCHANGE_NONE           0
#define EXCHANGE_BASE           1
#define EXCHANGE_ID_PROT        2
#define EXCHANGE_AUTH_ONLY      3
#define EXCHANGE_AGGRESSIVE     4
#define EXCHANGE_INFO           5
#define EXCHANGE_QUICK_MODE	32
#define EXCHANGE_NEW_GROUP_MODE	33

/* Exchange types */
#define IKE_EXCHANGE_TYPES_INITIALIZER			\
	{ "NONE",		/* 0 */			\
	  "BASE",		/* 1 */			\
	  "ID_PROT",		/* 2 */			\
	  "AUTH_ONLY",		/* 3 */			\
	  "AGGRESSIVE",		/* 4 */			\
	  "INFO",		/* 5 */			\
	  /* step up to type 32 with unknowns */	\
	  "unknown", "unknown", "unknown", "unknown",	\
	  "unknown", "unknown", "unknown", "unknown",	\
	  "unknown", "unknown", "unknown", "unknown",	\
	  "unknown", "unknown", "unknown", "unknown",	\
	  "unknown", "unknown", "unknown", "unknown",	\
	  "unknown", "unknown", "unknown", "unknown",	\
	  "unknown", "unknown",				\
	  "QUICK_MODE",		/* 32 */		\
	  "NEW_GROUP_MODE",	/* 33 */		\
	}

#define FLAGS_ENCRYPTION	1
#define FLAGS_COMMIT		2
#define FLAGS_AUTH_ONLY		4

#define CERT_NONE               0
#define CERT_PKCS               1
#define CERT_PGP                2
#define CERT_DNS                3
#define CERT_X509_SIG           4
#define CERT_X509_KE            5
#define CERT_KERBEROS           6
#define CERT_CRL                7
#define CERT_ARL                8
#define CERT_SPKI               9
#define CERT_X509_ATTR         10

#define NOTIFY_INVALID_PAYLOAD_TYPE          1
#define NOTIFY_DOI_NOT_SUPPORTED             2
#define NOTIFY_SITUATION_NOT_SUPPORTED       3
#define NOTIFY_INVALID_COOKIE                4
#define NOTIFY_INVALID_MAJOR_VERSION         5
#define NOTIFY_INVALID_MINOR_VERSION         6
#define NOTIFY_INVALID_EXCHANGE_TYPE         7
#define NOTIFY_INVALID_FLAGS                 8
#define NOTIFY_INVALID_MESSAGE_ID            9
#define NOTIFY_INVALID_PROTOCOL_ID           10
#define NOTIFY_INVALID_SPI                   11
#define NOTIFY_INVALID_TRANSFORM_ID          12
#define NOTIFY_ATTRIBUTES_NOT_SUPPORTED      13
#define NOTIFY_NO_PROPOSAL_CHOSEN            14
#define NOTIFY_BAD_PROPOSAL_SYNTAX           15
#define NOTIFY_PAYLOAD_MALFORMED             16
#define NOTIFY_INVALID_KEY_INFORMATION       17
#define NOTIFY_INVALID_ID_INFORMATION        18
#define NOTIFY_INVALID_CERT_ENCODING         19
#define NOTIFY_INVALID_CERTIFICATE           20
#define NOTIFY_CERT_TYPE_UNSUPPORTED         21
#define NOTIFY_INVALID_CERT_AUTHORITY        22
#define NOTIFY_INVALID_HASH_INFORMATION      23
#define NOTIFY_AUTHENTICATION_FAILED         24
#define NOTIFY_INVALID_SIGNATURE             25
#define NOTIFY_ADDRESS_NOTIFICATION          26
#define NOTIFY_NOTIFY_SA_LIFETIME            27
#define NOTIFY_CERTIFICATE_UNAVAILABLE       28
#define NOTIFY_UNSUPPORTED_EXCHANGE_TYPE     29
#define NOTIFY_UNEQUAL_PAYLOAD_LENGTHS       30

static void isakmp_pl_print(register u_char type, register u_char *payload);

int ike_tab_level = 0;

#define SMALL_TABS 4
#define SPACES "                                                   "
const char *ike_tab_offset(void)
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
void isakmp_print(register const u_char *cp, register int length)
{
	struct isakmp_header *ih;
	register const u_char *ep;
	u_char *payload;
	u_char  nextpayload, np1;
	u_int   paylen;
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

void isakmp_sa_print(register u_char *buf, register int len)
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

int isakmp_attribute_print(register u_char *buf)
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

void isakmp_transform_print(register u_char *buf, register int len)
{
	u_char *attr = buf + 4;

	printf("\n\t%stransform: %d ID: %d", ike_tab_offset(),
	       (char)buf[0], (char)buf[1]);
	
	ike_tab_level++;
	while((int)(attr - buf) < len - 4)  /* Skip last 'NONE' attr */
	        attr += isakmp_attribute_print(attr);
	ike_tab_level--;
}

void isakmp_proposal_print(register u_char *buf, register int len)
{
	printf(" proposal: %d proto: %d(%s) spisz: %d xforms: %d", 
	       buf[0], buf[1], (buf[1] == PROTO_ISAKMP ? "ISAKMP" : "unknown"),
	       buf[2], buf[3]);

	if((char)buf[3] > 0)
	       isakmp_pl_print(PAYLOAD_TRANSFORM, buf+4);
}

void isakmp_ke_print(register u_char *buf, register int len)
{
	if (isakmp_doi != IPSEC_DOI)
		return;

	printf(" <KE payload data (not shown)> len: %d", len);
}
	
void isakmp_id_print(register u_char *buf, register int len)
{
	if (isakmp_doi != IPSEC_DOI) 
		return;

	printf(" <ID payload data (not shown)> len: %d", len);
}
	
void isakmp_vendor_print(register u_char *buf, register int len)
{
	u_char *p = buf;

	if (isakmp_doi != IPSEC_DOI) 
		return;

	printf(" \"");
	for(p = buf; (int)(p - buf) < len; p++)
	        printf("%c",(isprint(*p) ? *p : '.'));
	printf("\"");
}

void isakmp_pl_print(register u_char type, register u_char *buf)
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
		break;
	    
	case PAYLOAD_NONCE:
#if 0
		isakmp_nonce_print(buf+4, this_len);
#endif
		break;
	    
	case PAYLOAD_NOTIFICATION:
	case PAYLOAD_DELETE:
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
