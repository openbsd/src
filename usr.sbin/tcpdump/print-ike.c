/*	$OpenBSD: print-ike.c,v 1.15 2002/09/23 04:10:14 millert Exp $	*/

/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 1999
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2001 Håkan Olsson.  All rights reserved.
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
    "@(#) $Header: /home/cvs/src/usr.sbin/tcpdump/print-ike.c,v 1.15 2002/09/23 04:10:14 millert Exp $ (XXX)";
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>

struct mbuf;
struct rtentry;
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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

static void ike_pl_print(u_char, u_char *, u_char);

int ike_tab_level = 0;
u_char xform_proto;

static const char *ike[] = IKE_PROTO_INITIALIZER;

#define SMALL_TABS 4
#define SPACES "                                                   "
const char *
ike_tab_offset(void)
{
	const char *p, *endline;
	static const char line[] = SPACES;

	endline = line + sizeof line - 1;
	p = endline - SMALL_TABS * (ike_tab_level);

	return (p > line ? p : line);
}

static char *
ike_get_cookie (u_char *ic, u_char *rc)
{
	static char cookie_jar[35];
	int i;

	for (i = 0; i < 8; i++)
		snprintf(cookie_jar + i*2, sizeof(cookie_jar) - i*2,
		    "%02x", *(ic + i));
	strlcat(cookie_jar, "->", sizeof(cookie_jar));
	for (i = 0; i < 8; i++)
		snprintf(cookie_jar + 18 + i*2, sizeof(cookie_jar) - 18 - i*2,
		    "%02x", *(rc + i));
	return cookie_jar;
}

/*
 * Print isakmp requests
 */
void
ike_print (const u_char *cp, u_int length)
{
	struct isakmp_header *ih;
	const u_char *ep;
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

	if (length < 20)
		(void)printf(" [len=%d]", length);

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

	printf("\n\tcookie: %s", ike_get_cookie (ih->init_cookie,
						 ih->resp_cookie));

	TCHECK(ih->msgid, sizeof(ih->msgid));
	printf(" msgid: %02x%02x%02x%02x", ih->msgid[0], ih->msgid[1],
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
		ike_pl_print(nextpayload, payload, ISAKMP_DOI);

	return;

trunc:
	fputs(" [|isakmp]", stdout);
}

void
ike_pl_sa_print (u_char *buf, int len)
{
	u_int32_t situation = ntohl(*(u_int32_t *)(buf + 4));
	u_char ike_doi = ntohl((*(u_int32_t *)buf));
	printf(" DOI: %d", ike_doi);
	if (ike_doi == IPSEC_DOI) {
		printf("(IPSEC) situation: ");
		if (situation & IKE_SITUATION_IDENTITY_ONLY)
			printf("IDENTITY_ONLY ");
		if (situation & IKE_SITUATION_SECRECY)
			printf("SECRECY ");
		if (situation & IKE_SITUATION_INTEGRITY)
			printf("INTEGRITY ");
		if ((situation & IKE_SITUATION_MASK) == 0)
			printf("0x%x (unknown)", situation);
		ike_pl_print (PAYLOAD_PROPOSAL, buf + 8, IPSEC_DOI);
	} else
		printf(" situation: (unknown)");
}

int
ike_attribute_print (u_char *buf, u_char doi, int maxlen)
{
	static char *attrs[] = IKE_ATTR_INITIALIZER;
	static char *attr_enc[] = IKE_ATTR_ENCRYPT_INITIALIZER;
	static char *attr_hash[] = IKE_ATTR_HASH_INITIALIZER;
	static char *attr_auth[] = IKE_ATTR_AUTH_INITIALIZER;
	static char *attr_gdesc[] = IKE_ATTR_GROUP_DESC_INITIALIZER;
	static char *attr_gtype[] = IKE_ATTR_GROUP_INITIALIZER;
	static char *attr_ltype[] = IKE_ATTR_SA_DURATION_INITIALIZER;
	static char *ipsec_attrs[] = IPSEC_ATTR_INITIALIZER;
	static char *ipsec_attr_encap[] = IPSEC_ATTR_ENCAP_INITIALIZER;
	static char *ipsec_attr_auth[] = IPSEC_ATTR_AUTH_INITIALIZER;
	static char *ipsec_attr_ltype[] = IPSEC_ATTR_DURATION_INITIALIZER;

	u_char    af   = buf[0] >> 7;
	u_int16_t type = (buf[0] & 0x7f) << 8 | buf[1];
	u_int16_t len  = buf[2] << 8 | buf[3], val;

	if (doi == ISAKMP_DOI)
		printf("\n\t%sattribute %s = ", ike_tab_offset(),
		    (type < sizeof attrs / sizeof attrs[0] ?
		    attrs[type] : "<unknown>"));
	else
		printf("\n\t%sattribute %s = ", ike_tab_offset(),
		    (type < (sizeof ipsec_attrs / sizeof ipsec_attrs[0]) ?
		    ipsec_attrs[type] : "<unknown>"));

	if ((af == 1 && maxlen < 4) || (af == 0 && maxlen < (len + 4))) {
		printf("\n\t%s[|attr]", ike_tab_offset());
		return maxlen;
	}

	if (af == 0) {
		/* AF=0; print the variable length attribute value */
		for (val = 0; val < len; val++)
			printf("%02x", (char)*(buf + 4 + val));
		return len + 4;
	}

	val = len;	/* For AF=1, this field is the "VALUE" */
	len = 4; 	/* and with AF=1, length is always 4 */

#define CASE_PRINT(TYPE, var) \
	case TYPE : \
		if (val < sizeof var / sizeof var [0]) \
			printf("%s", var [val]); \
		else \
			printf("%d (unknown)", val); \
		break;

	if (doi == ISAKMP_DOI)
		switch(type) {
			CASE_PRINT(IKE_ATTR_ENCRYPTION_ALGORITHM, attr_enc);
			CASE_PRINT(IKE_ATTR_HASH_ALGORITHM, attr_hash);
			CASE_PRINT(IKE_ATTR_AUTHENTICATION_METHOD, attr_auth);
			CASE_PRINT(IKE_ATTR_GROUP_DESC, attr_gdesc);
			CASE_PRINT(IKE_ATTR_GROUP_TYPE, attr_gtype);
			CASE_PRINT(IKE_ATTR_LIFE_TYPE, attr_ltype);
		default:
			printf("%d", val);
		}
	else
		switch(type) {
			CASE_PRINT(IPSEC_ATTR_SA_LIFE_TYPE, ipsec_attr_ltype);
			CASE_PRINT(IPSEC_ATTR_ENCAPSULATION_MODE,
			    ipsec_attr_encap);
			CASE_PRINT(IPSEC_ATTR_AUTHENTICATION_ALGORITHM,
			    ipsec_attr_auth);
		default:
			printf("%d", val);
		}

#undef CASE_PRINT
	return len;
}

void
ike_pl_transform_print (u_char *buf, int len, u_char doi)
{
	const char *ah[] = IPSEC_AH_INITIALIZER;
	const char *esp[] = IPSEC_ESP_INITIALIZER;
	const char *ipcomp[] = IPCOMP_INITIALIZER;
	u_char *attr = buf + 4;

	printf("\n\t%stransform: %u ID: ", ike_tab_offset(), buf[0]);

	switch (doi) {
	case ISAKMP_DOI:
		if (buf[1] < (sizeof ike / sizeof ike[0]))
			printf("%s", ike[buf[1]]);
		else
			printf("%d(unknown)", buf[1]);
		break;

	default: /* IPSEC_DOI */
		switch (xform_proto) {	/* from ike_proposal_print */
		case PROTO_IPSEC_AH:
			if (buf[1] < (sizeof ah / sizeof ah[0]))
				printf("%s", ah[buf[1]]);
			else
				printf("%d(unknown)", buf[1]);
			break;
		case PROTO_IPSEC_ESP:
			if (buf[1] < (sizeof esp / sizeof esp[0]))
				printf("%s", esp[buf[1]]);
			else
				printf("%d(unknown)", buf[1]);
			break;
		case PROTO_IPCOMP:
			if (buf[1] < (sizeof ipcomp / sizeof ipcomp[0]))
				printf("%s", ipcomp[buf[1]]);
			else
				printf("%d(unknown)", buf[1]);
			break;
		default:
			printf("%d(unknown)", buf[1]);
		}
		break;
	}

	ike_tab_level++;
	while ((int)(attr - buf) < len - 4)  /* Skip last 'NONE' attr */
		attr += ike_attribute_print(attr, doi, len - (attr-buf));
	ike_tab_level--;
}

void
ike_pl_proposal_print (u_char *buf, int len, u_char doi)
{
	u_int8_t i, p_id = buf[1], spisz = buf[2];

	printf(" proposal: %d proto: %s spisz: %d xforms: %d",
	    buf[0], (p_id < (sizeof ike / sizeof ike[0]) ? ike[p_id] :
	    "(unknown)"), spisz, buf[3]);

	/* We need to store this for upcoming ike_attribute_print call. */
	xform_proto = p_id;

	if (spisz) {
		if (p_id == PROTO_IPCOMP)
			printf(" CPI: 0x");
		else
			printf(" SPI: 0x");
		for (i = 0; i < spisz && (i + 4) < len; i++)
			printf("%02x", buf[i + 4]);
		doi = IPSEC_DOI;
	} else
		doi = ISAKMP_DOI;

	if ((char)buf[3] > 0)
		ike_pl_print(PAYLOAD_TRANSFORM, buf + 4 + buf[2], doi);
}

void
ike_pl_ke_print (u_char *buf, int len, u_char doi)
{
	if (doi != IPSEC_DOI)
		return;

	/* XXX ... */
}

void
ipsec_id_print (u_char *buf, int len, u_char doi)
{
	static const char *idtypes[] = IPSEC_ID_TYPE_INITIALIZER;
	char ntop_buf[INET6_ADDRSTRLEN];
	struct in_addr in;
	u_char *p;

	if (doi != ISAKMP_DOI)
		return;

	/* Don't print proto+port unless actually used */
	if (buf[1] | buf[2] | buf[3])
		printf(" proto: %d port: %d", buf[1], (buf[2] << 8) + buf[3]);

	printf(" type: %s = ", buf[0] < (sizeof idtypes/sizeof idtypes[0]) ?
	    idtypes[buf[0]] : "<unknown>");

	switch (buf[0]) {
	case IPSEC_ID_IPV4_ADDR:
		memcpy (&in.s_addr, buf + 4, sizeof in);
		printf("%s", inet_ntoa (in));
		break;
	case IPSEC_ID_IPV4_ADDR_SUBNET:
	case IPSEC_ID_IPV4_ADDR_RANGE:
		memcpy (&in.s_addr, buf + 4, sizeof in);
		printf("%s%s", inet_ntoa (in),
		    buf[0] == IPSEC_ID_IPV4_ADDR_SUBNET ? "/" : "-");
		memcpy (&in.s_addr, buf + 8, sizeof in);
		printf("%s", inet_ntoa (in));
		break;

	case IPSEC_ID_IPV6_ADDR:
		printf("%s", inet_ntop (AF_INET6, buf + 4, ntop_buf,
		    sizeof ntop_buf));
		break;
	case IPSEC_ID_IPV6_ADDR_SUBNET:
	case IPSEC_ID_IPV6_ADDR_RANGE:
		printf("%s%s", inet_ntop (AF_INET6, buf + 4, ntop_buf,
		    sizeof ntop_buf),
		    buf[0] == IPSEC_ID_IPV6_ADDR_SUBNET ? "/" : "-");
		printf("%s", inet_ntop (AF_INET6, buf + 4 + sizeof ntop_buf,
		    ntop_buf, sizeof ntop_buf));

	case IPSEC_ID_FQDN:
	case IPSEC_ID_USER_FQDN:
		printf("\"");
		for(p = buf + 4; (int)(p - buf) < len - 4; p++)
			printf("%c",(isprint(*p) ? *p : '.'));
		printf("\"");
		break;

	case IPSEC_ID_DER_ASN1_DN:
	case IPSEC_ID_DER_ASN1_GN:
	case IPSEC_ID_KEY_ID:
	default:
		printf("\"(not shown)\"");
		break;
	}
}

void
ike_pl_notification_print (u_char *buf, int len)
{
  	static const char *nftypes[] = IKE_NOTIFY_TYPES_INITIALIZER;
  	struct notification_payload *np = (struct notification_payload *)buf;
	u_int32_t *replay;
	u_char *attr;

	if (len < sizeof (struct notification_payload)) {
		printf(" (|len)");
		return;
	}

	np->doi  = ntohl (np->doi);
	np->type = ntohs (np->type);

	if (np->doi != ISAKMP_DOI && np->doi != IPSEC_DOI) {
		printf(" (unknown DOI)");
		return;
	}

	printf("\n\t%snotification: ", ike_tab_offset());

	if (np->type > 0 && np->type < (sizeof nftypes / sizeof nftypes[0])) {
		printf("%s", nftypes[np->type]);
		return;
	}
	switch (np->type) {

	case NOTIFY_IPSEC_RESPONDER_LIFETIME:
		printf("RESPONDER LIFETIME");
		if (np->spi_size == 16)
			printf("(%s)", ike_get_cookie (&np->data[0],
			    &np->data[8]));
		else
			printf("SPI: 0x%08x", np->data[0]<<24 |
			    np->data[1]<<16 | np->data[2]<<8 | np->data[3]);
		attr = &np->data[np->spi_size];
		ike_tab_level++;
		while ((int)(attr - buf) < len - 4)  /* Skip last 'NONE' attr */
			attr += ike_attribute_print(attr, IPSEC_DOI,
			    len - (attr-buf));
		ike_tab_level--;
		break;

	case NOTIFY_IPSEC_REPLAY_STATUS:
		replay = (u_int32_t *)&np->data[np->spi_size];
		printf("REPLAY STATUS [%sabled] ", *replay ? "en" : "dis");
		if (np->spi_size == 16)
			printf("(%s)", ike_get_cookie (&np->data[0],
			    &np->data[8]));
		else
			printf("SPI: 0x%08x", np->data[0]<<24 |
			    np->data[1]<<16 | np->data[2]<<8 | np->data[3]);
		break;

	case NOTIFY_IPSEC_INITIAL_CONTACT:
		printf("INITIAL CONTACT (%s)", ike_get_cookie (&np->data[0],
		    &np->data[8]));
		break;

	default:
	  	printf("%d (unknown)", np->type);
		break;
	}
}

void
ike_pl_vendor_print (u_char *buf, int len, u_char doi)
{
	u_char *p = buf;

	if (doi != IPSEC_DOI)
		return;

	printf(" \"");
	for (p = buf; (int)(p - buf) < len; p++)
		printf("%c",(isprint(*p) ? *p : '.'));
	printf("\"");
}

/* IKE mode-config. */
int
ike_cfg_attribute_print (u_char *buf, int attr_type, int maxlen)
{
	static char *attrs[] = IKE_CFG_ATTRIBUTE_INITIALIZER;
	char ntop_buf[INET6_ADDRSTRLEN];
	struct in_addr in;

	u_char    af   = buf[0] >> 7;
	u_int16_t type = (buf[0] & 0x7f) << 8 | buf[1];
	u_int16_t len  = af ? 2 : buf[2] << 8 | buf[3], p;
	u_char   *val  = af ? buf + 2 : buf + 4;

	printf("\n\t\%sattribute %s = ", ike_tab_offset(),
	    type < (sizeof attrs / sizeof attrs[0]) ? attrs[type] :
	    "<unknown>");

	if ((af == 1 && maxlen < 4) ||
	    (af == 0 && maxlen < (len + 4))) {
		printf("\n\t%s[|attr]", ike_tab_offset());
		return maxlen;
	}

	/* XXX The 2nd term is for bug compatibility with PGPnet.  */
	if (len == 0 || (af && !val[0] && !val[1])) {
		printf("<none>");
		return 4;
	}

	/* XXX Generally lengths are not checked well below.  */
	switch (type) {
	case IKE_CFG_ATTR_INTERNAL_IP4_ADDRESS:
	case IKE_CFG_ATTR_INTERNAL_IP4_NETMASK:
	case IKE_CFG_ATTR_INTERNAL_IP4_DNS:
	case IKE_CFG_ATTR_INTERNAL_IP4_NBNS:
	case IKE_CFG_ATTR_INTERNAL_IP4_DHCP:
		memcpy (&in.s_addr, val, sizeof in);
		printf("%s", inet_ntoa (in));
		break;

	case IKE_CFG_ATTR_INTERNAL_IP6_ADDRESS:
	case IKE_CFG_ATTR_INTERNAL_IP6_NETMASK:
	case IKE_CFG_ATTR_INTERNAL_IP6_DNS:
	case IKE_CFG_ATTR_INTERNAL_IP6_NBNS:
	case IKE_CFG_ATTR_INTERNAL_IP6_DHCP:
		printf("%s", inet_ntop (AF_INET6, val, ntop_buf,
		    sizeof ntop_buf));
		break;

	case IKE_CFG_ATTR_INTERNAL_IP4_SUBNET:
		memcpy(&in.s_addr, val, sizeof in);
		printf("%s/", inet_ntoa (in));
		memcpy(&in.s_addr, val + sizeof in, sizeof in);
		printf("%s", inet_ntoa (in));
		break;

	case IKE_CFG_ATTR_INTERNAL_IP6_SUBNET:
		printf("%s/%u", inet_ntop (AF_INET6, val, ntop_buf,
		    sizeof ntop_buf), val[16]);
		break;

	case IKE_CFG_ATTR_INTERNAL_ADDRESS_EXPIRY:
		printf("%u seconds",
		    val[0] << 24 | val[1] << 16 | val[2] << 8 | val[3]);
		break;

	case IKE_CFG_ATTR_APPLICATION_VERSION:
		for (p = 0; p < len; p++)
			printf("%c", isprint(val[p]) ? val[p] : '.');
		break;

	case IKE_CFG_ATTR_SUPPORTED_ATTRIBUTES:
		printf("<%d attributes>", len / 2);
		ike_tab_level++;
		for (p = 0; p < len; p += 2) {
			type = (val[p] << 8 | val[p + 1]) & 0x7fff;
			printf("\n\t%s%s", ike_tab_offset(),
			    type < (sizeof attrs/sizeof attrs[0]) ?
			    attrs[type] : "<unknown>");
		}
		ike_tab_level--;
		break;

	default:
		break;
	}
	return af ? 4 : len + 4;
}

void
ike_pl_attribute_print (u_char *buf, int len)
{
	static const char *pl_attr[] = IKE_CFG_ATTRIBUTE_TYPE_INITIALIZER;
	u_char type, *attr;
	u_int16_t id;

	type = buf[0];
	id = buf[2]<<8 | buf[3];
	attr = buf + 4;

	printf(" type: %s Id: %d",
	    type < (sizeof pl_attr/sizeof pl_attr[0]) ? pl_attr[type] :
	    "<unknown>", id);

	while ((int)(attr - buf) < len - 4)
		attr += ike_cfg_attribute_print(attr, type, len - (attr-buf));
}

void
ike_pl_print (u_char type, u_char *buf, u_char doi)
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
	switch (type) {
	case PAYLOAD_NONE:
		return;

	case PAYLOAD_SA:
		ike_pl_sa_print(buf+4, this_len);
		break;

	case PAYLOAD_PROPOSAL:
		ike_pl_proposal_print(buf+4, this_len, doi);
		break;

	case PAYLOAD_TRANSFORM:
		ike_pl_transform_print(buf+4, this_len, doi);
		break;

	case PAYLOAD_KE:
		ike_pl_ke_print(buf+4, this_len, doi);
		break;

	case PAYLOAD_ID:
		/* Should only happen with IPsec DOI */
		ipsec_id_print(buf+4, this_len, doi);
		break;

	case PAYLOAD_CERT:
	case PAYLOAD_CERTREQUEST:
	case PAYLOAD_HASH:
	case PAYLOAD_SIG:
	case PAYLOAD_NONCE:
	case PAYLOAD_DELETE:
		break;

	case PAYLOAD_NOTIFICATION:
	  	ike_pl_notification_print(buf, this_len);
		break;

	case PAYLOAD_VENDOR:
		ike_pl_vendor_print(buf+4, this_len, doi);
		break;

	case PAYLOAD_ATTRIBUTE:
		ike_pl_attribute_print(buf+4, this_len);
		break;

	default:
		break;
	}
	ike_tab_level--;

	if (next_type)  /* Recurse over next payload */
		ike_pl_print(next_type, buf + this_len, doi);

	return;

pltrunc:
	if (doi == ISAKMP_DOI)
		fputs(" [|isakmp]", stdout);
	else
		fputs(" [|ipsec]", stdout);
}
