/*	$OpenBSD: nat_traversal.c,v 1.2 2004/06/20 17:17:35 ho Exp $	*/

/*
 * Copyright (c) 2004 Håkan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <stdlib.h>

#include "sysdep.h"

#include "exchange.h"
#include "hash.h"
#include "ipsec.h"
#include "isakmp_fld.h"
#include "isakmp_num.h"
#include "ipsec_num.h"
#include "hash.h"
#include "log.h"
#include "message.h"
#include "nat_traversal.h"
#include "prf.h"
#include "sa.h"
#include "transport.h"
#include "util.h"

/*
 * XXX According to draft-ietf-ipsec-nat-t-ike-07.txt, the NAT-T
 * capability of the other peer is determined by a particular vendor ID
 * sent as the first message. This vendor ID string is supposed to be a
 * MD5 hash of "RFC XXXX", where XXXX is the future RFC number.
 *
 * These seem to be the "well" known variants of this string in use by
 * products today.
 */
static const char *isakmp_nat_t_cap_text[] = {
	"draft-ietf-ipsec-nat-t-ike-00",	/* V1 (XXX: may be obsolete) */
	"draft-ietf-ipsec-nat-t-ike-02\n",	/* V2 */
	"draft-ietf-ipsec-nat-t-ike-03",	/* V3 */
#ifdef notyet
	"RFC XXXX",
#endif
};

/* The MD5 hashes of the above strings is put in this array.  */
static char	**nat_t_hashes;
static size_t	  nat_t_hashsize;

static int	nat_t_setup_hashes(void);
static int	nat_t_add_vendor_payload(struct message *, char *);
static int	nat_t_add_nat_d(struct message *, struct sockaddr *);
static int	nat_t_match_nat_d_payload(struct message *, struct sockaddr *);

void
nat_t_init(void)
{
	nat_t_hashes = (char **)NULL;
}

/* Generate the NAT-T capability marker hashes. Executed only once.  */
static int
nat_t_setup_hashes(void)
{
	struct hash *hash;
	int n = sizeof isakmp_nat_t_cap_text / sizeof isakmp_nat_t_cap_text[0];
	int i;

	/* The draft says to use MD5.  */
	hash = hash_get(HASH_MD5);
	if (!hash) {
		/* Should never happen.  */
		log_print("nat_t_setup_hashes: "
		    "could not find MD5 hash structure!");
		return -1;
	}
	nat_t_hashsize = hash->hashsize;

	/* Allocate one more than is necessary, i.e NULL terminated.  */
	nat_t_hashes = (char **)calloc((size_t)(n + 1), sizeof(char *));
	if (!nat_t_hashes) {
		log_error("nat_t_setup_hashes: calloc (%lu,%lu) failed",
		    (unsigned long)n, (unsigned long)sizeof(char *));
		return -1;
	}

	/* Populate with hashes.  */
	for (i = 0; i < n; i++) {
		nat_t_hashes[i] = (char *)malloc(nat_t_hashsize);
		if (!nat_t_hashes[i]) {
			log_error("nat_t_setup_hashes: malloc (%lu) failed",
			    (unsigned long)nat_t_hashsize);
			goto errout;
		}

		hash->Init(hash->ctx);
		hash->Update(hash->ctx,
		    (unsigned char *)isakmp_nat_t_cap_text[i],
		    strlen(isakmp_nat_t_cap_text[i]));
		hash->Final(nat_t_hashes[i], hash->ctx);

		LOG_DBG((LOG_EXCHANGE, 50, "nat_t_setup_hashes: "
		    "MD5(\"%s\") (%d bytes)", isakmp_nat_t_cap_text[i],
		    nat_t_hashsize));
		LOG_DBG_BUF((LOG_EXCHANGE, 50, "nat_t_setup_hashes",
		    nat_t_hashes[i], nat_t_hashsize));
	}

	return 0;

  errout:
	for (i = 0; i < n; i++)
		if (nat_t_hashes[i])
			free(nat_t_hashes[i]);
	free(nat_t_hashes);
	nat_t_hashes = NULL;
	return -1;
}

/* Add one NAT-T VENDOR payload.  */
static int
nat_t_add_vendor_payload(struct message *msg, char *hash)
{
	size_t	 buflen = nat_t_hashsize + ISAKMP_GEN_SZ;
	u_int8_t *buf;

	buf = malloc(buflen);
	if (!buf) {
		log_error("nat_t_add_vendor_payload: malloc (%lu) failed",
		    (unsigned long)buflen);
		return -1;
	}

	SET_ISAKMP_GEN_LENGTH(buf, buflen);
	memcpy(buf + ISAKMP_VENDOR_ID_OFF, hash, nat_t_hashsize);
	if (message_add_payload(msg, ISAKMP_PAYLOAD_VENDOR, buf, buflen, 1)) {
		free(buf);
		return -1;
	}

	return 0;
}

/* Add the NAT-T capability markers (VENDOR payloads).  */
int
nat_t_add_vendor_payloads(struct message *msg)
{
	int i = 0;

	if (!nat_t_hashes)
		if (nat_t_setup_hashes())
			return 0;  /* XXX should this be an error?  */

	while (nat_t_hashes[i])
		if (nat_t_add_vendor_payload(msg, nat_t_hashes[i++]))
			return -1;

	return 0;
}

/*
 * Check an incoming message for NAT-T capability markers.
 */
void
nat_t_check_vendor_payload(struct message *msg, struct payload *p)
{
	u_int8_t *pbuf = p->p;
	size_t	  vlen;
	int	  i = 0;

	/* Already checked? */
	if (p->flags & PL_MARK ||
	    msg->exchange->flags & EXCHANGE_FLAG_NAT_T_CAP_PEER)
		return;

	if (!nat_t_hashes)
		if (nat_t_setup_hashes())
			return;

	vlen = GET_ISAKMP_GEN_LENGTH(pbuf) - ISAKMP_GEN_SZ;
	if (vlen != nat_t_hashsize) {
		LOG_DBG((LOG_EXCHANGE, 50, "nat_t_check_vendor_payload: "
		    "bad size %d != %d", vlen, nat_t_hashsize));
		return;
	}

	while (nat_t_hashes[i])
		if (memcmp(nat_t_hashes[i++], pbuf + ISAKMP_GEN_SZ,
		    vlen) == 0) {
			/* This peer is NAT-T capable.  */
			msg->exchange->flags |= EXCHANGE_FLAG_NAT_T_CAP_PEER;
			LOG_DBG((LOG_EXCHANGE, 10,
			    "nat_t_check_vendor_payload: "
			    "NAT-T capable peer detected"));
			p->flags |= PL_MARK;
			return;
		}

	return;
}

/* Generate the NAT-D payload hash : HASH(CKY-I | CKY-R | IP | Port).  */
static u_int8_t *
nat_t_generate_nat_d_hash(struct message *msg, struct sockaddr *sa,
    size_t *hashlen)
{
	struct ipsec_exch *ie = (struct ipsec_exch *)msg->exchange->data;
	struct hash	 *hash;
	struct prf	 *prf;
	u_int8_t	 *res;
	in_port_t	  port;
	int		  prf_type = PRF_HMAC; /* XXX */

	hash = hash_get(ie->hash->type);
	if (hash == NULL) {
		log_print ("nat_t_generate_nat_d_hash: no hash");
		return NULL;
	}

	prf = prf_alloc(prf_type, hash->type, msg->exchange->cookies,
	    ISAKMP_HDR_COOKIES_LEN);
	if(!prf) {
		log_print("nat_t_generate_nat_d_hash: prf_alloc failed");
		return NULL;
	}

	*hashlen = prf->blocksize;
	res = (u_int8_t *)malloc((unsigned long)*hashlen);
	if (!res) {
		log_print("nat_t_generate_nat_d_hash: malloc (%lu) failed",
		    (unsigned long)*hashlen);
		prf_free(prf);
		*hashlen = 0;
		return NULL;
	}

	port = sockaddr_port(sa);
	memset(res, 0, *hashlen);

	prf->Update(prf->prfctx, sockaddr_addrdata(sa), sockaddr_addrlen(sa));
	prf->Update(prf->prfctx, (unsigned char *)&port, sizeof port);
	prf->Final(res, prf->prfctx);
	prf_free (prf);

	return res;
}

/* Add a NAT-D payload to our message.  */
static int
nat_t_add_nat_d(struct message *msg, struct sockaddr *sa)
{
	u_int8_t *hbuf, *buf;
	size_t	  hbuflen, buflen;

	hbuf = nat_t_generate_nat_d_hash(msg, sa, &hbuflen);
	if (!hbuf) {
		log_print("nat_t_add_nat_d: NAT-D hash gen failed");
		return -1;
	}

	buflen = ISAKMP_NAT_D_DATA_OFF + hbuflen;
	buf = malloc(buflen);
	if (!buf) {
		log_error("nat_t_add_nat_d: malloc (%lu) failed",
		    (unsigned long)buflen);
		free(hbuf);
		return -1;
	}

	SET_ISAKMP_GEN_LENGTH(buf, buflen);
	memcpy(buf + ISAKMP_NAT_D_DATA_OFF, hbuf, hbuflen);
	free(hbuf);

	if (message_add_payload(msg, ISAKMP_PAYLOAD_NAT_D, buf, buflen, 1)) {
		free(buf);
		return -1;
	}

	return 0;
}

/* We add two NAT-D payloads, one each for src and dst.  */
int
nat_t_exchange_add_nat_d(struct message *msg)
{
	struct sockaddr *sa;

	msg->transport->vtbl->get_src(msg->transport, &sa);
	if (nat_t_add_nat_d(msg, sa))
		return -1;

	msg->transport->vtbl->get_dst(msg->transport, &sa);
	if (nat_t_add_nat_d(msg, sa))
		return -1;

	return 0;
}

/* Generate and match a NAT-D hash against the NAT-D payload (pl.) data.  */
static int
nat_t_match_nat_d_payload(struct message *msg, struct sockaddr *sa)
{
	struct payload *p;
	u_int8_t *hbuf;
	size_t	 hbuflen;
	int	 found = 0;

	hbuf = nat_t_generate_nat_d_hash(msg, sa, &hbuflen);
	if (!hbuf)
		return 0;

	for (p = payload_first(msg, ISAKMP_PAYLOAD_NAT_D); p;
	     p = TAILQ_NEXT(p, link)) {
		if (GET_ISAKMP_GEN_LENGTH (p->p) !=
		    hbuflen + ISAKMP_NAT_D_DATA_OFF)
			continue;

		if (memcmp(p->p + ISAKMP_NAT_D_DATA_OFF, hbuf, hbuflen) == 0) {
			found++;
			break;
		}
	}
	free(hbuf);
	return found;
}

/*
 * Check if we need to activate NAT-T, and if we need to send keepalive
 * messages to the other side, i.e if we are a nat:ed peer.
 */
int
nat_t_exchange_check_nat_d(struct message *msg)
{
	struct sockaddr *sa;
	int	 outgoing_path_is_clear, incoming_path_is_clear;

	/* Assume trouble, i.e NAT-boxes in our path.  */
	outgoing_path_is_clear = incoming_path_is_clear = 0;

	msg->transport->vtbl->get_src(msg->transport, &sa);
	if (nat_t_match_nat_d_payload(msg, sa))
		outgoing_path_is_clear = 1;

	msg->transport->vtbl->get_dst(msg->transport, &sa);
	if (nat_t_match_nat_d_payload(msg, sa))
		incoming_path_is_clear = 1;

	if (outgoing_path_is_clear && incoming_path_is_clear) {
		LOG_DBG((LOG_EXCHANGE, 40, "nat_t_exchange_check_nat_d: "
		    "no NAT"));
		return 0; /* No NAT-T required.  */
	}

	/* NAT-T handling required.  */
	msg->exchange->flags |= EXCHANGE_FLAG_NAT_T_ENABLE;

	if (!outgoing_path_is_clear) {
		msg->exchange->flags |= EXCHANGE_FLAG_NAT_T_KEEPALIVE;
		LOG_DBG((LOG_EXCHANGE, 10, "nat_t_exchange_check_nat_d: "
		    "NAT detected, we're behind it"));
	} else
		LOG_DBG ((LOG_EXCHANGE, 10,
		    "nat_t_exchange_check_nat_d: NAT detected"));
	return 1;
}
