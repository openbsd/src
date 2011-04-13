/*	$OpenBSD: pfkeyv2_convert.c,v 1.35 2011/04/13 11:28:47 markus Exp $	*/
/*
 * The author of this code is Angelos D. Keromytis (angelos@keromytis.org)
 *
 * Part of this code is based on code written by Craig Metz (cmetz@inner.net)
 * for NRL. Those licenses follow this one.
 *
 * Copyright (c) 2001 Angelos D. Keromytis.
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

/*
 *	@(#)COPYRIGHT	1.1 (NRL) 17 January 1995
 * 
 * NRL grants permission for redistribution and use in source and binary
 * forms, with or without modification, of the software and documentation
 * created at NRL provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgements:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 	This product includes software developed at the Information
 * 	Technology Division, US Naval Research Laboratory.
 * 4. Neither the name of the NRL nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THE SOFTWARE PROVIDED BY NRL IS PROVIDED BY NRL AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL NRL OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of the US Naval
 * Research Laboratory (NRL).
 */

/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Craig Metz. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "pf.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <net/route.h>
#include <net/if.h>

#if NPF > 0
#include <net/pfvar.h>
#endif

#include <netinet/ip_ipsp.h>
#ifdef INET6
#include <netinet6/in6_var.h>
#endif
#include <net/pfkeyv2.h>
#include <crypto/cryptodev.h>
#include <crypto/xform.h>

/*
 * (Partly) Initialize a TDB based on an SADB_SA payload. Other parts
 * of the TDB will be initialized by other import routines, and tdb_init().
 */
void
import_sa(struct tdb *tdb, struct sadb_sa *sadb_sa, struct ipsecinit *ii)
{
	if (!sadb_sa)
		return;

	if (ii) {
		ii->ii_encalg = sadb_sa->sadb_sa_encrypt;
		ii->ii_authalg = sadb_sa->sadb_sa_auth;
		ii->ii_compalg = sadb_sa->sadb_sa_encrypt; /* Yeurk! */

		tdb->tdb_spi = sadb_sa->sadb_sa_spi;
		tdb->tdb_wnd = sadb_sa->sadb_sa_replay;

		if (sadb_sa->sadb_sa_flags & SADB_SAFLAGS_PFS)
			tdb->tdb_flags |= TDBF_PFS;

		if (sadb_sa->sadb_sa_flags & SADB_X_SAFLAGS_HALFIV)
			tdb->tdb_flags |= TDBF_HALFIV;

		if (sadb_sa->sadb_sa_flags & SADB_X_SAFLAGS_TUNNEL)
			tdb->tdb_flags |= TDBF_TUNNELING;

		if (sadb_sa->sadb_sa_flags & SADB_X_SAFLAGS_RANDOMPADDING)
			tdb->tdb_flags |= TDBF_RANDOMPADDING;

		if (sadb_sa->sadb_sa_flags & SADB_X_SAFLAGS_NOREPLAY)
			tdb->tdb_flags |= TDBF_NOREPLAY;

		if (sadb_sa->sadb_sa_flags & SADB_X_SAFLAGS_UDPENCAP)
			tdb->tdb_flags |= TDBF_UDPENCAP;
	}

	if (sadb_sa->sadb_sa_state != SADB_SASTATE_MATURE)
		tdb->tdb_flags |= TDBF_INVALID;
}

/*
 * Export some of the information on a TDB.
 */
void
export_sa(void **p, struct tdb *tdb)
{
	struct sadb_sa *sadb_sa = (struct sadb_sa *) *p;

	sadb_sa->sadb_sa_len = sizeof(struct sadb_sa) / sizeof(uint64_t);

	sadb_sa->sadb_sa_spi = tdb->tdb_spi;
	sadb_sa->sadb_sa_replay = tdb->tdb_wnd;

	if (tdb->tdb_flags & TDBF_INVALID)
		sadb_sa->sadb_sa_state = SADB_SASTATE_LARVAL;
	else
		sadb_sa->sadb_sa_state = SADB_SASTATE_MATURE;

	if (tdb->tdb_sproto == IPPROTO_IPCOMP &&
	    tdb->tdb_compalgxform != NULL) {
		switch (tdb->tdb_compalgxform->type) {
		case CRYPTO_DEFLATE_COMP:
			sadb_sa->sadb_sa_encrypt = SADB_X_CALG_DEFLATE;
			break;
		case CRYPTO_LZS_COMP:
			sadb_sa->sadb_sa_encrypt = SADB_X_CALG_LZS;
			break;
		}
	}

	if (tdb->tdb_authalgxform) {
		switch (tdb->tdb_authalgxform->type) {
		case CRYPTO_MD5_HMAC:
			sadb_sa->sadb_sa_auth = SADB_AALG_MD5HMAC;
			break;

		case CRYPTO_SHA1_HMAC:
			sadb_sa->sadb_sa_auth = SADB_AALG_SHA1HMAC;
			break;

		case CRYPTO_RIPEMD160_HMAC:
			sadb_sa->sadb_sa_auth = SADB_X_AALG_RIPEMD160HMAC;
			break;

		case CRYPTO_SHA2_256_HMAC:
			sadb_sa->sadb_sa_auth = SADB_X_AALG_SHA2_256;
			break;

		case CRYPTO_SHA2_384_HMAC:
			sadb_sa->sadb_sa_auth = SADB_X_AALG_SHA2_384;
			break;

		case CRYPTO_SHA2_512_HMAC:
			sadb_sa->sadb_sa_auth = SADB_X_AALG_SHA2_512;
			break;

		case CRYPTO_AES_128_GMAC:
			sadb_sa->sadb_sa_auth = SADB_X_AALG_AES128GMAC;
			break;

		case CRYPTO_AES_192_GMAC:
			sadb_sa->sadb_sa_auth = SADB_X_AALG_AES192GMAC;
			break;

		case CRYPTO_AES_256_GMAC:
			sadb_sa->sadb_sa_auth = SADB_X_AALG_AES256GMAC;
			break;

		case CRYPTO_MD5_KPDK:
			sadb_sa->sadb_sa_auth = SADB_X_AALG_MD5;
			break;

		case CRYPTO_SHA1_KPDK:
			sadb_sa->sadb_sa_auth = SADB_X_AALG_SHA1;
			break;
		}
	}

	if (tdb->tdb_encalgxform) {
		switch (tdb->tdb_encalgxform->type) {
		case CRYPTO_NULL:
			sadb_sa->sadb_sa_encrypt = SADB_EALG_NULL;
			break;

		case CRYPTO_DES_CBC:
			sadb_sa->sadb_sa_encrypt = SADB_EALG_DESCBC;
			break;

		case CRYPTO_3DES_CBC:
			sadb_sa->sadb_sa_encrypt = SADB_EALG_3DESCBC;
			break;

		case CRYPTO_AES_CBC:
			sadb_sa->sadb_sa_encrypt = SADB_X_EALG_AES;
			break;

		case CRYPTO_AES_CTR:
			sadb_sa->sadb_sa_encrypt = SADB_X_EALG_AESCTR;
			break;

		case CRYPTO_AES_GCM_16:
			sadb_sa->sadb_sa_encrypt = SADB_X_EALG_AESGCM16;
			break;

		case CRYPTO_AES_GMAC:
			sadb_sa->sadb_sa_encrypt = SADB_X_EALG_AESGMAC;
			break;

		case CRYPTO_CAST_CBC:
			sadb_sa->sadb_sa_encrypt = SADB_X_EALG_CAST;
			break;

		case CRYPTO_BLF_CBC:
			sadb_sa->sadb_sa_encrypt = SADB_X_EALG_BLF;
			break;
		}
	}

	if (tdb->tdb_flags & TDBF_PFS)
		sadb_sa->sadb_sa_flags |= SADB_SAFLAGS_PFS;

	/* Only relevant for the "old" IPsec transforms. */
	if (tdb->tdb_flags & TDBF_HALFIV)
		sadb_sa->sadb_sa_flags |= SADB_X_SAFLAGS_HALFIV;

	if (tdb->tdb_flags & TDBF_TUNNELING)
		sadb_sa->sadb_sa_flags |= SADB_X_SAFLAGS_TUNNEL;

	if (tdb->tdb_flags & TDBF_RANDOMPADDING)
		sadb_sa->sadb_sa_flags |= SADB_X_SAFLAGS_RANDOMPADDING;

	if (tdb->tdb_flags & TDBF_NOREPLAY)
		sadb_sa->sadb_sa_flags |= SADB_X_SAFLAGS_NOREPLAY;

	if (tdb->tdb_flags & TDBF_UDPENCAP)
		sadb_sa->sadb_sa_flags |= SADB_X_SAFLAGS_UDPENCAP;

	*p += sizeof(struct sadb_sa);
}

/*
 * Initialize expirations and counters based on lifetime payload.
 */
void
import_lifetime(struct tdb *tdb, struct sadb_lifetime *sadb_lifetime, int type)
{
	struct timeval tv;

	if (!sadb_lifetime)
		return;

	getmicrotime(&tv);

	switch (type) {
	case PFKEYV2_LIFETIME_HARD:
		if ((tdb->tdb_exp_allocations =
		    sadb_lifetime->sadb_lifetime_allocations) != 0)
			tdb->tdb_flags |= TDBF_ALLOCATIONS;
		else
			tdb->tdb_flags &= ~TDBF_ALLOCATIONS;

		if ((tdb->tdb_exp_bytes =
		    sadb_lifetime->sadb_lifetime_bytes) != 0)
			tdb->tdb_flags |= TDBF_BYTES;
		else
			tdb->tdb_flags &= ~TDBF_BYTES;

		if ((tdb->tdb_exp_timeout =
		    sadb_lifetime->sadb_lifetime_addtime) != 0) {
			tdb->tdb_flags |= TDBF_TIMER;
			if (tv.tv_sec + tdb->tdb_exp_timeout < tv.tv_sec)
				tv.tv_sec = ((unsigned long) -1) / 2; /* XXX */
			else
				tv.tv_sec += tdb->tdb_exp_timeout;
			timeout_add(&tdb->tdb_timer_tmo, hzto(&tv));
		} else
			tdb->tdb_flags &= ~TDBF_TIMER;

		if ((tdb->tdb_exp_first_use =
		    sadb_lifetime->sadb_lifetime_usetime) != 0)
			tdb->tdb_flags |= TDBF_FIRSTUSE;
		else
			tdb->tdb_flags &= ~TDBF_FIRSTUSE;
		break;

	case PFKEYV2_LIFETIME_SOFT:
		if ((tdb->tdb_soft_allocations =
		    sadb_lifetime->sadb_lifetime_allocations) != 0)
			tdb->tdb_flags |= TDBF_SOFT_ALLOCATIONS;
		else
			tdb->tdb_flags &= ~TDBF_SOFT_ALLOCATIONS;

		if ((tdb->tdb_soft_bytes =
		    sadb_lifetime->sadb_lifetime_bytes) != 0)
			tdb->tdb_flags |= TDBF_SOFT_BYTES;
		else
			tdb->tdb_flags &= ~TDBF_SOFT_BYTES;

		if ((tdb->tdb_soft_timeout =
		    sadb_lifetime->sadb_lifetime_addtime) != 0) {
			tdb->tdb_flags |= TDBF_SOFT_TIMER;
			if (tv.tv_sec + tdb->tdb_soft_timeout < tv.tv_sec)
				tv.tv_sec = ((unsigned long) -1) / 2; /* XXX */
			else
				tv.tv_sec += tdb->tdb_soft_timeout;
			timeout_add(&tdb->tdb_stimer_tmo, hzto(&tv));
		} else
			tdb->tdb_flags &= ~TDBF_SOFT_TIMER;

		if ((tdb->tdb_soft_first_use =
		    sadb_lifetime->sadb_lifetime_usetime) != 0)
			tdb->tdb_flags |= TDBF_SOFT_FIRSTUSE;
		else
			tdb->tdb_flags &= ~TDBF_SOFT_FIRSTUSE;
		break;

	case PFKEYV2_LIFETIME_CURRENT:  /* Nothing fancy here. */
		tdb->tdb_cur_allocations =
		    sadb_lifetime->sadb_lifetime_allocations;
		tdb->tdb_cur_bytes = sadb_lifetime->sadb_lifetime_bytes;
		tdb->tdb_established = sadb_lifetime->sadb_lifetime_addtime;
		tdb->tdb_first_use = sadb_lifetime->sadb_lifetime_usetime;
	}
}

/*
 * Export TDB expiration information.
 */
void
export_lifetime(void **p, struct tdb *tdb, int type)
{
	struct sadb_lifetime *sadb_lifetime = (struct sadb_lifetime *) *p;

	sadb_lifetime->sadb_lifetime_len = sizeof(struct sadb_lifetime) /
	    sizeof(uint64_t);

	switch (type) {
	case PFKEYV2_LIFETIME_HARD:
		if (tdb->tdb_flags & TDBF_ALLOCATIONS)
			sadb_lifetime->sadb_lifetime_allocations =
			    tdb->tdb_exp_allocations;

		if (tdb->tdb_flags & TDBF_BYTES)
			sadb_lifetime->sadb_lifetime_bytes =
			    tdb->tdb_exp_bytes;

		if (tdb->tdb_flags & TDBF_TIMER)
			sadb_lifetime->sadb_lifetime_addtime =
			    tdb->tdb_exp_timeout;

		if (tdb->tdb_flags & TDBF_FIRSTUSE)
			sadb_lifetime->sadb_lifetime_usetime =
			    tdb->tdb_exp_first_use;
		break;

	case PFKEYV2_LIFETIME_SOFT:
		if (tdb->tdb_flags & TDBF_SOFT_ALLOCATIONS)
			sadb_lifetime->sadb_lifetime_allocations =
			    tdb->tdb_soft_allocations;

		if (tdb->tdb_flags & TDBF_SOFT_BYTES)
			sadb_lifetime->sadb_lifetime_bytes =
			    tdb->tdb_soft_bytes;

		if (tdb->tdb_flags & TDBF_SOFT_TIMER)
			sadb_lifetime->sadb_lifetime_addtime =
			    tdb->tdb_soft_timeout;

		if (tdb->tdb_flags & TDBF_SOFT_FIRSTUSE)
			sadb_lifetime->sadb_lifetime_usetime =
			    tdb->tdb_soft_first_use;
		break;

	case PFKEYV2_LIFETIME_CURRENT:
		sadb_lifetime->sadb_lifetime_allocations =
		    tdb->tdb_cur_allocations;
		sadb_lifetime->sadb_lifetime_bytes = tdb->tdb_cur_bytes;
		sadb_lifetime->sadb_lifetime_addtime = tdb->tdb_established;
		sadb_lifetime->sadb_lifetime_usetime = tdb->tdb_first_use;
		break;

	case PFKEYV2_LIFETIME_LASTUSE:
		sadb_lifetime->sadb_lifetime_allocations = 0;
		sadb_lifetime->sadb_lifetime_bytes = 0;
		sadb_lifetime->sadb_lifetime_addtime = 0;
		sadb_lifetime->sadb_lifetime_usetime = tdb->tdb_last_used;
		break;
	}

	*p += sizeof(struct sadb_lifetime);
}

/*
 * Import flow information to two struct sockaddr_encap's. Either
 * all or none of the address arguments are NULL.
 */
void
import_flow(struct sockaddr_encap *flow, struct sockaddr_encap *flowmask,
    struct sadb_address *ssrc, struct sadb_address *ssrcmask,
    struct sadb_address *ddst, struct sadb_address *ddstmask,
    struct sadb_protocol *sab, struct sadb_protocol *ftype)
{
	u_int8_t transproto = 0;
	union sockaddr_union *src = (union sockaddr_union *)(ssrc + 1);
	union sockaddr_union *dst = (union sockaddr_union *)(ddst + 1);
	union sockaddr_union *srcmask = (union sockaddr_union *)(ssrcmask + 1);
	union sockaddr_union *dstmask = (union sockaddr_union *)(ddstmask + 1);

	if (ssrc == NULL)
		return; /* There wasn't any information to begin with. */

	bzero(flow, sizeof(*flow));
	bzero(flowmask, sizeof(*flowmask));

	if (sab != NULL)
		transproto = sab->sadb_protocol_proto;

	/*
	 * Check that all the address families match. We know they are
	 * valid and supported because pfkeyv2_parsemessage() checked that.
	 */
	if ((src->sa.sa_family != dst->sa.sa_family) ||
	    (src->sa.sa_family != srcmask->sa.sa_family) ||
	    (src->sa.sa_family != dstmask->sa.sa_family))
		return;

	/*
	 * We set these as an indication that tdb_filter/tdb_filtermask are
	 * in fact initialized.
	 */
	flow->sen_family = flowmask->sen_family = PF_KEY;
	flow->sen_len = flowmask->sen_len = SENT_LEN;

	switch (src->sa.sa_family)
	{
#ifdef INET
	case AF_INET:
		/* netmask handling */
		rt_maskedcopy(&src->sa, &src->sa, &srcmask->sa);
		rt_maskedcopy(&dst->sa, &dst->sa, &dstmask->sa);

		flow->sen_type = SENT_IP4;
		flow->sen_direction = ftype->sadb_protocol_direction;
		flow->sen_ip_src = src->sin.sin_addr;
		flow->sen_ip_dst = dst->sin.sin_addr;
		flow->sen_proto = transproto;
		flow->sen_sport = src->sin.sin_port;
		flow->sen_dport = dst->sin.sin_port;

		flowmask->sen_type = SENT_IP4;
		flowmask->sen_direction = 0xff;
		flowmask->sen_ip_src = srcmask->sin.sin_addr;
		flowmask->sen_ip_dst = dstmask->sin.sin_addr;
		flowmask->sen_sport = srcmask->sin.sin_port;
		flowmask->sen_dport = dstmask->sin.sin_port;
		if (transproto)
			flowmask->sen_proto = 0xff;
		break;
#endif /* INET */

#ifdef INET6
	case AF_INET6:
		in6_embedscope(&src->sin6.sin6_addr, &src->sin6,
		    NULL, NULL);
		in6_embedscope(&dst->sin6.sin6_addr, &dst->sin6,
		    NULL, NULL);

		/* netmask handling */
		rt_maskedcopy(&src->sa, &src->sa, &srcmask->sa);
		rt_maskedcopy(&dst->sa, &dst->sa, &dstmask->sa);

		flow->sen_type = SENT_IP6;
		flow->sen_ip6_direction = ftype->sadb_protocol_direction;
		flow->sen_ip6_src = src->sin6.sin6_addr;
		flow->sen_ip6_dst = dst->sin6.sin6_addr;
		flow->sen_ip6_proto = transproto;
		flow->sen_ip6_sport = src->sin6.sin6_port;
		flow->sen_ip6_dport = dst->sin6.sin6_port;

		flowmask->sen_type = SENT_IP6;
		flowmask->sen_ip6_direction = 0xff;
		flowmask->sen_ip6_src = srcmask->sin6.sin6_addr;
		flowmask->sen_ip6_dst = dstmask->sin6.sin6_addr;
		flowmask->sen_ip6_sport = srcmask->sin6.sin6_port;
		flowmask->sen_ip6_dport = dstmask->sin6.sin6_port;
		if (transproto)
			flowmask->sen_ip6_proto = 0xff;
		break;
#endif /* INET6 */
	}
}

/*
 * Helper to export addresses from an struct sockaddr_encap.
 */
static void
export_encap(void **p, struct sockaddr_encap *encap, int type)
{
	struct sadb_address *saddr = (struct sadb_address *)*p;
	union sockaddr_union *sunion;

	*p += sizeof(struct sadb_address);
	sunion = (union sockaddr_union *)*p;

	switch (encap->sen_type) {
	case SENT_IP4:
		saddr->sadb_address_len = (sizeof(struct sadb_address) +
		    PADUP(sizeof(struct sockaddr_in))) / sizeof(uint64_t);
		sunion->sa.sa_len = sizeof(struct sockaddr_in);
		sunion->sa.sa_family = AF_INET;
		if (type == SADB_X_EXT_SRC_FLOW ||
		    type == SADB_X_EXT_SRC_MASK) {
			sunion->sin.sin_addr = encap->sen_ip_src;
			sunion->sin.sin_port = encap->sen_sport;
		} else {
			sunion->sin.sin_addr = encap->sen_ip_dst;
			sunion->sin.sin_port = encap->sen_dport;
		}
		*p += PADUP(sizeof(struct sockaddr_in));
		break;
        case SENT_IP6:
		saddr->sadb_address_len = (sizeof(struct sadb_address)
		    + PADUP(sizeof(struct sockaddr_in6))) / sizeof(uint64_t);
		sunion->sa.sa_len = sizeof(struct sockaddr_in6);
		sunion->sa.sa_family = AF_INET6;
		if (type == SADB_X_EXT_SRC_FLOW ||
		    type == SADB_X_EXT_SRC_MASK) {
			sunion->sin6.sin6_addr = encap->sen_ip6_src;
			sunion->sin6.sin6_port = encap->sen_ip6_sport;
		} else {
			sunion->sin6.sin6_addr = encap->sen_ip6_dst;
			sunion->sin6.sin6_port = encap->sen_ip6_dport;
		}
		*p += PADUP(sizeof(struct sockaddr_in6));
		break;
	}
}

/*
 * Export flow information from two struct sockaddr_encap's.
 */
void
export_flow(void **p, u_int8_t ftype, struct sockaddr_encap *flow,
    struct sockaddr_encap *flowmask, void **headers)
{
	struct sadb_protocol *sab;

	headers[SADB_X_EXT_FLOW_TYPE] = *p;
	sab = (struct sadb_protocol *)*p;
	sab->sadb_protocol_len = sizeof(struct sadb_protocol) /
	    sizeof(uint64_t);

	switch (ftype) {
	case IPSP_IPSEC_USE:
		sab->sadb_protocol_proto = SADB_X_FLOW_TYPE_USE;
		break;
	case IPSP_IPSEC_ACQUIRE:
		sab->sadb_protocol_proto = SADB_X_FLOW_TYPE_ACQUIRE;
		break;
	case IPSP_IPSEC_REQUIRE:
		sab->sadb_protocol_proto = SADB_X_FLOW_TYPE_REQUIRE;
		break;
	case IPSP_DENY:
		sab->sadb_protocol_proto = SADB_X_FLOW_TYPE_DENY;
		break;
	case IPSP_PERMIT:
		sab->sadb_protocol_proto = SADB_X_FLOW_TYPE_BYPASS;
		break;
	case IPSP_IPSEC_DONTACQ:
		sab->sadb_protocol_proto = SADB_X_FLOW_TYPE_DONTACQ;
		break;
	default:
		sab->sadb_protocol_proto = 0;
		break;
	}
	
	switch (flow->sen_type) {
#ifdef INET
	case SENT_IP4:
		sab->sadb_protocol_direction = flow->sen_direction;
		break;
#endif /* INET */
#ifdef INET6
	case SENT_IP6:
		sab->sadb_protocol_direction = flow->sen_ip6_direction;
		break;
#endif /* INET6 */
	}
	*p += sizeof(struct sadb_protocol);

	headers[SADB_X_EXT_PROTOCOL] = *p;
	sab = (struct sadb_protocol *)*p;
	sab->sadb_protocol_len = sizeof(struct sadb_protocol) /
	    sizeof(uint64_t);
	switch (flow->sen_type) {
#ifdef INET
	case SENT_IP4:
		sab->sadb_protocol_proto = flow->sen_proto;
		break;
#endif /* INET */
#ifdef INET6
	case SENT_IP6:
		sab->sadb_protocol_proto = flow->sen_ip6_proto;
		break;
#endif /* INET6 */
	}
	*p += sizeof(struct sadb_protocol);

	headers[SADB_X_EXT_SRC_FLOW] = *p;
	export_encap(p, flow, SADB_X_EXT_SRC_FLOW);

	headers[SADB_X_EXT_SRC_MASK] = *p;
	export_encap(p, flowmask, SADB_X_EXT_SRC_MASK);

	headers[SADB_X_EXT_DST_FLOW] = *p;
	export_encap(p, flow, SADB_X_EXT_DST_FLOW);

	headers[SADB_X_EXT_DST_MASK] = *p;
	export_encap(p, flowmask, SADB_X_EXT_DST_MASK);
}

/*
 * Copy an SADB_ADDRESS payload to a struct sockaddr.
 */
void
import_address(struct sockaddr *sa, struct sadb_address *sadb_address)
{
	int salen;
	struct sockaddr *ssa = (struct sockaddr *)((void *) sadb_address +
	    sizeof(struct sadb_address));

	if (!sadb_address)
		return;

	if (ssa->sa_len)
		salen = ssa->sa_len;
	else
		switch (ssa->sa_family) {
#ifdef INET
		case AF_INET:
			salen = sizeof(struct sockaddr_in);
			break;
#endif /* INET */

#ifdef INET6
		case AF_INET6:
			salen = sizeof(struct sockaddr_in6);
			break;
#endif /* INET6 */

		default:
			return;
		}

	bcopy(ssa, sa, salen);
	sa->sa_len = salen;
}

/*
 * Export a struct sockaddr as an SADB_ADDRESS payload.
 */
void
export_address(void **p, struct sockaddr *sa)
{
	struct sadb_address *sadb_address = (struct sadb_address *) *p;

	sadb_address->sadb_address_len = (sizeof(struct sadb_address) +
	    PADUP(SA_LEN(sa))) / sizeof(uint64_t);

	*p += sizeof(struct sadb_address);
	bcopy(sa, *p, SA_LEN(sa));
	((struct sockaddr *) *p)->sa_family = sa->sa_family;
	*p += PADUP(SA_LEN(sa));
}

/*
 * Import authentication information into the TDB.
 */
void
import_auth(struct tdb *tdb, struct sadb_x_cred *sadb_auth, int dstauth)
{
	struct ipsec_ref **ipr;

	if (!sadb_auth)
		return;

	if (dstauth == PFKEYV2_AUTH_REMOTE)
		ipr = &tdb->tdb_remote_auth;
	else
		ipr = &tdb->tdb_local_auth;

	*ipr = malloc(EXTLEN(sadb_auth) - sizeof(struct sadb_x_cred) +
	    sizeof(struct ipsec_ref), M_CREDENTIALS, M_WAITOK);
	(*ipr)->ref_len = EXTLEN(sadb_auth) - sizeof(struct sadb_x_cred);

	switch (sadb_auth->sadb_x_cred_type) {
	case SADB_X_AUTHTYPE_PASSPHRASE:
		(*ipr)->ref_type = IPSP_AUTH_PASSPHRASE;
		break;
	case SADB_X_AUTHTYPE_RSA:
		(*ipr)->ref_type = IPSP_AUTH_RSA;
		break;
	default:
		free(*ipr, M_CREDENTIALS);
		*ipr = NULL;
		return;
	}
	(*ipr)->ref_count = 1;
	(*ipr)->ref_malloctype = M_CREDENTIALS;
	bcopy((void *) sadb_auth + sizeof(struct sadb_x_cred),
	    (*ipr) + 1, (*ipr)->ref_len);
}

/*
 * Import a set of credentials into the TDB.
 */
void
import_credentials(struct tdb *tdb, struct sadb_x_cred *sadb_cred, int dstcred)
{
	struct ipsec_ref **ipr;

	if (!sadb_cred)
		return;

	if (dstcred == PFKEYV2_CRED_REMOTE)
		ipr = &tdb->tdb_remote_cred;
	else
		ipr = &tdb->tdb_local_cred;

	*ipr = malloc(EXTLEN(sadb_cred) - sizeof(struct sadb_x_cred) +
	    sizeof(struct ipsec_ref), M_CREDENTIALS, M_WAITOK);
	(*ipr)->ref_len = EXTLEN(sadb_cred) - sizeof(struct sadb_x_cred);

	switch (sadb_cred->sadb_x_cred_type) {
	case SADB_X_CREDTYPE_X509:
		(*ipr)->ref_type = IPSP_CRED_X509;
		break;
	case SADB_X_CREDTYPE_KEYNOTE:
		(*ipr)->ref_type = IPSP_CRED_KEYNOTE;
		break;
	default:
		free(*ipr, M_CREDENTIALS);
		*ipr = NULL;
		return;
	}
	(*ipr)->ref_count = 1;
	(*ipr)->ref_malloctype = M_CREDENTIALS;
	bcopy((void *) sadb_cred + sizeof(struct sadb_x_cred),
	    (*ipr) + 1, (*ipr)->ref_len);
}

/*
 * Import an identity payload into the TDB.
 */
void
import_identity(struct tdb *tdb, struct sadb_ident *sadb_ident, int type)
{
	struct ipsec_ref **ipr;

	if (!sadb_ident)
		return;

	if (type == PFKEYV2_IDENTITY_SRC)
		ipr = &tdb->tdb_srcid;
	else
		ipr = &tdb->tdb_dstid;

	*ipr = malloc(EXTLEN(sadb_ident) - sizeof(struct sadb_ident) +
	    sizeof(struct ipsec_ref), M_CREDENTIALS, M_WAITOK);
	(*ipr)->ref_len = EXTLEN(sadb_ident) - sizeof(struct sadb_ident);

	switch (sadb_ident->sadb_ident_type) {
	case SADB_IDENTTYPE_PREFIX:
		(*ipr)->ref_type = IPSP_IDENTITY_PREFIX;
		break;
	case SADB_IDENTTYPE_FQDN:
		(*ipr)->ref_type = IPSP_IDENTITY_FQDN;
		break;
	case SADB_IDENTTYPE_USERFQDN:
		(*ipr)->ref_type = IPSP_IDENTITY_USERFQDN;
		break;
	case SADB_X_IDENTTYPE_CONNECTION:
		(*ipr)->ref_type = IPSP_IDENTITY_CONNECTION;
		break;
	default:
		free(*ipr, M_CREDENTIALS);
		*ipr = NULL;
		return;
	}
	(*ipr)->ref_count = 1;
	(*ipr)->ref_malloctype = M_CREDENTIALS;
	bcopy((void *) sadb_ident + sizeof(struct sadb_ident), (*ipr) + 1,
	    (*ipr)->ref_len);
}

void
export_credentials(void **p, struct tdb *tdb, int dstcred)
{
	struct ipsec_ref **ipr;
	struct sadb_x_cred *sadb_cred = (struct sadb_x_cred *) *p;

	if (dstcred == PFKEYV2_CRED_REMOTE)
		ipr = &tdb->tdb_remote_cred;
	else
		ipr = &tdb->tdb_local_cred;

	sadb_cred->sadb_x_cred_len = (sizeof(struct sadb_x_cred) +
	    PADUP((*ipr)->ref_len)) / sizeof(uint64_t);

	switch ((*ipr)->ref_type) {
	case IPSP_CRED_KEYNOTE:
		sadb_cred->sadb_x_cred_type = SADB_X_CREDTYPE_KEYNOTE;
		break;
	case IPSP_CRED_X509:
		sadb_cred->sadb_x_cred_type = SADB_X_CREDTYPE_X509;
		break;
	}
	*p += sizeof(struct sadb_x_cred);
	bcopy((*ipr) + 1, *p, (*ipr)->ref_len);
	*p += PADUP((*ipr)->ref_len);
}

void
export_auth(void **p, struct tdb *tdb, int dstauth)
{
	struct ipsec_ref **ipr;
	struct sadb_x_cred *sadb_auth = (struct sadb_x_cred *) *p;

	if (dstauth == PFKEYV2_AUTH_REMOTE)
		ipr = &tdb->tdb_remote_auth;
	else
		ipr = &tdb->tdb_local_auth;

	sadb_auth->sadb_x_cred_len = (sizeof(struct sadb_x_cred) +
	    PADUP((*ipr)->ref_len)) / sizeof(uint64_t);

	switch ((*ipr)->ref_type) {
	case IPSP_AUTH_PASSPHRASE:
		sadb_auth->sadb_x_cred_type = SADB_X_AUTHTYPE_PASSPHRASE;
		break;
	case IPSP_AUTH_RSA:
		sadb_auth->sadb_x_cred_type = SADB_X_AUTHTYPE_RSA;
		break;
	}
	*p += sizeof(struct sadb_x_cred);
	bcopy((*ipr) + 1, *p, (*ipr)->ref_len);
	*p += PADUP((*ipr)->ref_len);
}

void
export_identity(void **p, struct tdb *tdb, int type)
{
	struct ipsec_ref **ipr;
	struct sadb_ident *sadb_ident = (struct sadb_ident *) *p;

	if (type == PFKEYV2_IDENTITY_SRC)
		ipr = &tdb->tdb_srcid;
	else
		ipr = &tdb->tdb_dstid;

	sadb_ident->sadb_ident_len = (sizeof(struct sadb_ident) +
	    PADUP((*ipr)->ref_len)) / sizeof(uint64_t);

	switch ((*ipr)->ref_type) {
	case IPSP_IDENTITY_PREFIX:
		sadb_ident->sadb_ident_type = SADB_IDENTTYPE_PREFIX;
		break;
	case IPSP_IDENTITY_FQDN:
		sadb_ident->sadb_ident_type = SADB_IDENTTYPE_FQDN;
		break;
	case IPSP_IDENTITY_USERFQDN:
		sadb_ident->sadb_ident_type = SADB_IDENTTYPE_USERFQDN;
		break;
	case IPSP_IDENTITY_CONNECTION:
		sadb_ident->sadb_ident_type = SADB_X_IDENTTYPE_CONNECTION;
		break;
	}
	*p += sizeof(struct sadb_ident);
	bcopy((*ipr) + 1, *p, (*ipr)->ref_len);
	*p += PADUP((*ipr)->ref_len);
}

/* ... */
void
import_key(struct ipsecinit *ii, struct sadb_key *sadb_key, int type)
{
	if (!sadb_key)
		return;

	if (type == PFKEYV2_ENCRYPTION_KEY) { /* Encryption key */
		ii->ii_enckeylen = sadb_key->sadb_key_bits / 8;
		ii->ii_enckey = (void *)sadb_key + sizeof(struct sadb_key);
	} else {
		ii->ii_authkeylen = sadb_key->sadb_key_bits / 8;
		ii->ii_authkey = (void *)sadb_key + sizeof(struct sadb_key);
	}
}

void
export_key(void **p, struct tdb *tdb, int type)
{
	struct sadb_key *sadb_key = (struct sadb_key *) *p;

	if (type == PFKEYV2_ENCRYPTION_KEY) {
		sadb_key->sadb_key_len = (sizeof(struct sadb_key) +
		    PADUP(tdb->tdb_emxkeylen)) /
		    sizeof(uint64_t);
		sadb_key->sadb_key_bits = tdb->tdb_emxkeylen * 8;
		*p += sizeof(struct sadb_key);
		bcopy(tdb->tdb_emxkey, *p, tdb->tdb_emxkeylen);
		*p += PADUP(tdb->tdb_emxkeylen);
	} else {
		sadb_key->sadb_key_len = (sizeof(struct sadb_key) +
		    PADUP(tdb->tdb_amxkeylen)) /
		    sizeof(uint64_t);
		sadb_key->sadb_key_bits = tdb->tdb_amxkeylen * 8;
		*p += sizeof(struct sadb_key);
		bcopy(tdb->tdb_amxkey, *p, tdb->tdb_amxkeylen);
		*p += PADUP(tdb->tdb_amxkeylen);
	}
}

/* Import/Export remote port for UDP Encapsulation */
void
import_udpencap(struct tdb *tdb, struct sadb_x_udpencap *sadb_udpencap)
{
	if (sadb_udpencap)
		tdb->tdb_udpencap_port = sadb_udpencap->sadb_x_udpencap_port;
}

void
export_udpencap(void **p, struct tdb *tdb)
{
	struct sadb_x_udpencap *sadb_udpencap = (struct sadb_x_udpencap *) *p;

	sadb_udpencap->sadb_x_udpencap_port = tdb->tdb_udpencap_port;
	sadb_udpencap->sadb_x_udpencap_reserved = 0;
	sadb_udpencap->sadb_x_udpencap_len =
	    sizeof(struct sadb_x_udpencap) / sizeof(uint64_t);
	*p += sizeof(struct sadb_x_udpencap);
}

#if NPF > 0
/* Import PF tag information for SA */
void
import_tag(struct tdb *tdb, struct sadb_x_tag *stag)
{
	char *s;

	if (stag) {
		s = (char *)(stag + 1);
		tdb->tdb_tag = pf_tagname2tag(s);
	}
}

/* Export PF tag information for SA */
void
export_tag(void **p, struct tdb *tdb)
{
	struct sadb_x_tag *stag = (struct sadb_x_tag *)*p;
	char *s = (char *)(stag + 1);

	pf_tag2tagname(tdb->tdb_tag, s);
	stag->sadb_x_tag_taglen = strlen(s) + 1;
	stag->sadb_x_tag_len = (sizeof(struct sadb_x_tag) +
	    PADUP(stag->sadb_x_tag_taglen)) / sizeof(uint64_t);
	*p += PADUP(stag->sadb_x_tag_taglen) + sizeof(struct sadb_x_tag);
}

/* Import enc(4) tap device information for SA */
void
import_tap(struct tdb *tdb, struct sadb_x_tap *stap)
{
	if (stap)
		tdb->tdb_tap = stap->sadb_x_tap_unit;
}

/* Export enc(4) tap device information for SA */
void
export_tap(void **p, struct tdb *tdb)
{
	struct sadb_x_tap *stag = (struct sadb_x_tap *)*p;

	stag->sadb_x_tap_unit = tdb->tdb_tap;
	stag->sadb_x_tap_len = sizeof(struct sadb_x_tap) / sizeof(uint64_t);
	*p += sizeof(struct sadb_x_tap);
}
#endif
