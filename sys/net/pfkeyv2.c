/* $OpenBSD: pfkeyv2.c,v 1.140 2015/04/13 08:45:48 mpi Exp $ */

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
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/pool.h>
#include <net/route.h>
#include <netinet/ip_ipsp.h>
#include <net/pfkeyv2.h>
#include <netinet/ip_ah.h>
#include <netinet/ip_esp.h>
#include <netinet/ip_ipcomp.h>
#include <crypto/blf.h>

#if NPF > 0
#include <net/if.h>
#include <net/pfvar.h>
#endif

#define PFKEYV2_PROTOCOL 2
#define GETSPI_TRIES 10

/* Static globals */
static struct pfkeyv2_socket *pfkeyv2_sockets = NULL;
static struct pfkey_version pfkeyv2_version;
static uint32_t pfkeyv2_seq = 1;
static int nregistered = 0;
static int npromisc = 0;

static const struct sadb_alg ealgs[] = {
	{ SADB_EALG_NULL, 0, 0, 0 },
	{ SADB_EALG_DESCBC, 64, 64, 64 },
	{ SADB_EALG_3DESCBC, 64, 192, 192 },
	{ SADB_X_EALG_BLF, 64, 40, BLF_MAXKEYLEN * 8},
	{ SADB_X_EALG_CAST, 64, 40, 128},
	{ SADB_X_EALG_AES, 128, 128, 256},
	{ SADB_X_EALG_AESCTR, 128, 128 + 32, 256 + 32}
};

static const struct sadb_alg aalgs[] = {
	{ SADB_AALG_SHA1HMAC, 0, 160, 160 },
	{ SADB_AALG_MD5HMAC, 0, 128, 128 },
	{ SADB_X_AALG_RIPEMD160HMAC, 0, 160, 160 },
	{ SADB_X_AALG_SHA2_256, 0, 256, 256 },
	{ SADB_X_AALG_SHA2_384, 0, 384, 384 },
	{ SADB_X_AALG_SHA2_512, 0, 512, 512 }
};

static const struct sadb_alg calgs[] = {
	{ SADB_X_CALG_DEFLATE, 0, 0, 0},
	{ SADB_X_CALG_LZS, 0, 0, 0}
};

extern uint64_t sadb_exts_allowed_out[SADB_MAX+1];
extern uint64_t sadb_exts_required_out[SADB_MAX+1];

extern struct pool ipsec_policy_pool;

/*
 * Wrapper around m_devget(); copy data from contiguous buffer to mbuf
 * chain.
 */
int
pfdatatopacket(void *data, int len, struct mbuf **packet)
{
	if (!(*packet = m_devget(data, len, 0)))
		return (ENOMEM);

	/* Make sure, all data gets zeroized on free */
	(*packet)->m_flags |= M_ZEROIZE;

	return (0);
}

/*
 * Create a new PF_KEYv2 socket.
 */
int
pfkeyv2_create(struct socket *socket)
{
	struct pfkeyv2_socket *pfkeyv2_socket;

	if (!(pfkeyv2_socket = malloc(sizeof(struct pfkeyv2_socket),
	    M_PFKEY, M_NOWAIT | M_ZERO)))
		return (ENOMEM);

	pfkeyv2_socket->next = pfkeyv2_sockets;
	pfkeyv2_socket->socket = socket;
	pfkeyv2_socket->pid = curproc->p_p->ps_pid;

	/*
	 * XXX we should get this from the socket instead but
	 * XXX rawcb doesn't store the rdomain like inpcb does.
	 */
	pfkeyv2_socket->rdomain = rtable_l2(curproc->p_p->ps_rtableid);

	pfkeyv2_sockets = pfkeyv2_socket;

	return (0);
}

/*
 * Close a PF_KEYv2 socket.
 */
int
pfkeyv2_release(struct socket *socket)
{
	struct pfkeyv2_socket **pp;

	for (pp = &pfkeyv2_sockets; *pp && ((*pp)->socket != socket);
	    pp = &((*pp)->next))
		/*EMPTY*/;

	if (*pp) {
		struct pfkeyv2_socket *pfkeyv2_socket;

		pfkeyv2_socket = *pp;
		*pp = (*pp)->next;

		if (pfkeyv2_socket->flags & PFKEYV2_SOCKETFLAGS_REGISTERED)
			nregistered--;

		if (pfkeyv2_socket->flags & PFKEYV2_SOCKETFLAGS_PROMISC)
			npromisc--;

		free(pfkeyv2_socket, M_PFKEY, 0);
	}

	return (0);
}

/*
 * Send a PFKEYv2 message, possibly to many receivers, based on the
 * satype of the socket (which is set by the REGISTER message), and the
 * third argument.
 */
int
pfkeyv2_sendmessage(void **headers, int mode, struct socket *socket,
    u_int8_t satype, int count, u_int rdomain)
{
	int i, j, rval;
	void *p, *buffer = NULL;
	struct mbuf *packet;
	struct pfkeyv2_socket *s;
	struct sadb_msg *smsg;

	/* Find out how much space we'll need... */
	j = sizeof(struct sadb_msg);

	for (i = 1; i <= SADB_EXT_MAX; i++)
		if (headers[i])
			j += ((struct sadb_ext *)headers[i])->sadb_ext_len *
			    sizeof(uint64_t);

	/* ...and allocate it */
	if (!(buffer = malloc(j + sizeof(struct sadb_msg), M_PFKEY,
	    M_NOWAIT))) {
		rval = ENOMEM;
		goto ret;
	}

	p = buffer + sizeof(struct sadb_msg);
	bcopy(headers[0], p, sizeof(struct sadb_msg));
	((struct sadb_msg *) p)->sadb_msg_len = j / sizeof(uint64_t);
	p += sizeof(struct sadb_msg);

	/* Copy payloads in the packet */
	for (i = 1; i <= SADB_EXT_MAX; i++)
		if (headers[i]) {
			((struct sadb_ext *) headers[i])->sadb_ext_type = i;
			bcopy(headers[i], p, EXTLEN(headers[i]));
			p += EXTLEN(headers[i]);
		}

	if ((rval = pfdatatopacket(buffer + sizeof(struct sadb_msg),
	    j, &packet)) != 0)
		goto ret;

	switch (mode) {
	case PFKEYV2_SENDMESSAGE_UNICAST:
		/*
		 * Send message to the specified socket, plus all
		 * promiscuous listeners.
		 */
		pfkey_sendup(socket, packet, 0);

		/*
		 * Promiscuous messages contain the original message
		 * encapsulated in another sadb_msg header.
		 */
		bzero(buffer, sizeof(struct sadb_msg));
		smsg = (struct sadb_msg *) buffer;
		smsg->sadb_msg_version = PF_KEY_V2;
		smsg->sadb_msg_type = SADB_X_PROMISC;
		smsg->sadb_msg_len = (sizeof(struct sadb_msg) + j) /
		    sizeof(uint64_t);
		smsg->sadb_msg_seq = 0;

		/* Copy to mbuf chain */
		if ((rval = pfdatatopacket(buffer, sizeof(struct sadb_msg) + j,
		    &packet)) != 0)
			goto ret;

		/*
		 * Search for promiscuous listeners, skipping the
		 * original destination.
		 */
		for (s = pfkeyv2_sockets; s; s = s->next)
			if ((s->flags & PFKEYV2_SOCKETFLAGS_PROMISC) &&
			    (s->socket != socket) &&
			    (s->rdomain == rdomain))
				pfkey_sendup(s->socket, packet, 1);

		m_freem(packet);
		break;

	case PFKEYV2_SENDMESSAGE_REGISTERED:
		/*
		 * Send the message to all registered sockets that match
		 * the specified satype (e.g., all IPSEC-ESP negotiators)
		 */
		for (s = pfkeyv2_sockets; s; s = s->next)
			if ((s->flags & PFKEYV2_SOCKETFLAGS_REGISTERED) &&
			    (s->rdomain == rdomain)) {
				if (!satype)    /* Just send to everyone registered */
					pfkey_sendup(s->socket, packet, 1);
				else {
					/* Check for specified satype */
					if ((1 << satype) & s->registration)
						pfkey_sendup(s->socket, packet, 1);
				}
			}

		/* Free last/original copy of the packet */
		m_freem(packet);

		/* Encapsulate the original message "inside" an sadb_msg header */
		bzero(buffer, sizeof(struct sadb_msg));
		smsg = (struct sadb_msg *) buffer;
		smsg->sadb_msg_version = PF_KEY_V2;
		smsg->sadb_msg_type = SADB_X_PROMISC;
		smsg->sadb_msg_len = (sizeof(struct sadb_msg) + j) /
		    sizeof(uint64_t);
		smsg->sadb_msg_seq = 0;

		/* Convert to mbuf chain */
		if ((rval = pfdatatopacket(buffer, sizeof(struct sadb_msg) + j,
		    &packet)) != 0)
			goto ret;

		/* Send to all registered promiscuous listeners */
		for (s = pfkeyv2_sockets; s; s = s->next)
			if ((s->flags & PFKEYV2_SOCKETFLAGS_PROMISC) &&
			    !(s->flags & PFKEYV2_SOCKETFLAGS_REGISTERED) &&
			    (s->rdomain == rdomain))
				pfkey_sendup(s->socket, packet, 1);

		m_freem(packet);
		break;

	case PFKEYV2_SENDMESSAGE_BROADCAST:
		/* Send message to all sockets */
		for (s = pfkeyv2_sockets; s; s = s->next) {
			if (s->rdomain == rdomain)
				pfkey_sendup(s->socket, packet, 1);
		}
		m_freem(packet);
		break;
	}

ret:
	if (buffer != NULL) {
		bzero(buffer, j + sizeof(struct sadb_msg));
		free(buffer, M_PFKEY, 0);
	}

	return (rval);
}

/*
 * Get SPD information for an ACQUIRE. We setup the message such that
 * the SRC/DST payloads are relative to us (regardless of whether the
 * SPD rule was for incoming or outgoing packets).
 */
int
pfkeyv2_policy(struct ipsec_acquire *ipa, void **headers, void **buffer)
{
	union sockaddr_union sunion;
	struct sadb_protocol *sp;
	int rval, i, dir;
	void *p;

	/* Find out how big a buffer we need */
	i = 4 * sizeof(struct sadb_address) + sizeof(struct sadb_protocol);
	bzero(&sunion, sizeof(union sockaddr_union));

	switch (ipa->ipa_info.sen_type) {
	case SENT_IP4:
		i += 4 * PADUP(sizeof(struct sockaddr_in));
		sunion.sa.sa_family = AF_INET;
		sunion.sa.sa_len = sizeof(struct sockaddr_in);
		dir = ipa->ipa_info.sen_direction;
		break;

#ifdef INET6
	case SENT_IP6:
		i += 4 * PADUP(sizeof(struct sockaddr_in6));
		sunion.sa.sa_family = AF_INET6;
		sunion.sa.sa_len = sizeof(struct sockaddr_in6);
		dir = ipa->ipa_info.sen_ip6_direction;
		break;
#endif /* INET6 */

	default:
		return (EINVAL);
	}

	if (!(p = malloc(i, M_PFKEY, M_NOWAIT | M_ZERO))) {
		rval = ENOMEM;
		goto ret;
	} else
		*buffer = p;

	if (dir == IPSP_DIRECTION_OUT)
		headers[SADB_X_EXT_SRC_FLOW] = p;
	else
		headers[SADB_X_EXT_DST_FLOW] = p;
	switch (sunion.sa.sa_family) {
	case AF_INET:
		sunion.sin.sin_addr = ipa->ipa_info.sen_ip_src;
		sunion.sin.sin_port = ipa->ipa_info.sen_sport;
		break;

#ifdef INET6
	case AF_INET6:
		sunion.sin6.sin6_addr = ipa->ipa_info.sen_ip6_src;
		sunion.sin6.sin6_port = ipa->ipa_info.sen_ip6_sport;
		break;
#endif /* INET6 */
	}
	export_address(&p, (struct sockaddr *) &sunion);

	if (dir == IPSP_DIRECTION_OUT)
		headers[SADB_X_EXT_SRC_MASK] = p;
	else
		headers[SADB_X_EXT_DST_MASK] = p;
	switch (sunion.sa.sa_family) {
	case AF_INET:
		sunion.sin.sin_addr = ipa->ipa_mask.sen_ip_src;
		sunion.sin.sin_port = ipa->ipa_mask.sen_sport;
		break;

#ifdef INET6
	case AF_INET6:
		sunion.sin6.sin6_addr = ipa->ipa_mask.sen_ip6_src;
		sunion.sin6.sin6_port = ipa->ipa_mask.sen_ip6_sport;
		break;
#endif /* INET6 */
	}
	export_address(&p, (struct sockaddr *) &sunion);

	if (dir == IPSP_DIRECTION_OUT)
		headers[SADB_X_EXT_DST_FLOW] = p;
	else
		headers[SADB_X_EXT_SRC_FLOW] = p;
	switch (sunion.sa.sa_family) {
	case AF_INET:
		sunion.sin.sin_addr = ipa->ipa_info.sen_ip_dst;
		sunion.sin.sin_port = ipa->ipa_info.sen_dport;
		break;

#ifdef INET6
	case AF_INET6:
		sunion.sin6.sin6_addr = ipa->ipa_info.sen_ip6_dst;
		sunion.sin6.sin6_port = ipa->ipa_info.sen_ip6_dport;
		break;
#endif /* INET6 */
	}
	export_address(&p, (struct sockaddr *) &sunion);

	if (dir == IPSP_DIRECTION_OUT)
		headers[SADB_X_EXT_DST_MASK] = p;
	else
		headers[SADB_X_EXT_SRC_MASK] = p;
	switch (sunion.sa.sa_family) {
	case AF_INET:
		sunion.sin.sin_addr = ipa->ipa_mask.sen_ip_dst;
		sunion.sin.sin_port = ipa->ipa_mask.sen_dport;
		break;

#ifdef INET6
	case AF_INET6:
		sunion.sin6.sin6_addr = ipa->ipa_mask.sen_ip6_dst;
		sunion.sin6.sin6_port = ipa->ipa_mask.sen_ip6_dport;
		break;
#endif /* INET6 */
	}
	export_address(&p, (struct sockaddr *) &sunion);

	headers[SADB_X_EXT_FLOW_TYPE] = p;
	sp = p;
	sp->sadb_protocol_len = sizeof(struct sadb_protocol) /
	    sizeof(u_int64_t);
	switch (sunion.sa.sa_family) {
	case AF_INET:
		if (ipa->ipa_mask.sen_proto)
			sp->sadb_protocol_proto = ipa->ipa_info.sen_proto;
		sp->sadb_protocol_direction = ipa->ipa_info.sen_direction;
		break;

#ifdef INET6
	case AF_INET6:
		if (ipa->ipa_mask.sen_ip6_proto)
			sp->sadb_protocol_proto = ipa->ipa_info.sen_ip6_proto;
		sp->sadb_protocol_direction = ipa->ipa_info.sen_ip6_direction;
		break;
#endif /* INET6 */
	}

	rval = 0;

ret:
	return (rval);
}

/*
 * Get all the information contained in an SA to a PFKEYV2 message.
 */
int
pfkeyv2_get(struct tdb *sa, void **headers, void **buffer, int *lenp)
{
	int rval, i;
	void *p;

	/* Find how much space we need */
	i = sizeof(struct sadb_sa) + sizeof(struct sadb_lifetime);

	if (sa->tdb_soft_allocations || sa->tdb_soft_bytes ||
	    sa->tdb_soft_timeout || sa->tdb_soft_first_use)
		i += sizeof(struct sadb_lifetime);

	if (sa->tdb_exp_allocations || sa->tdb_exp_bytes ||
	    sa->tdb_exp_timeout || sa->tdb_exp_first_use)
		i += sizeof(struct sadb_lifetime);

	if (sa->tdb_last_used)
		i += sizeof(struct sadb_lifetime);

	i += sizeof(struct sadb_address) + PADUP(SA_LEN(&sa->tdb_src.sa));
	i += sizeof(struct sadb_address) + PADUP(SA_LEN(&sa->tdb_dst.sa));

	if (sa->tdb_srcid)
		i += sizeof(struct sadb_ident) + PADUP(sa->tdb_srcid->ref_len);

	if (sa->tdb_dstid)
		i += sizeof(struct sadb_ident) + PADUP(sa->tdb_dstid->ref_len);

	if (sa->tdb_local_cred)
		i += sizeof(struct sadb_x_cred) + PADUP(sa->tdb_local_cred->ref_len);

	if (sa->tdb_remote_cred)
		i += sizeof(struct sadb_x_cred) + PADUP(sa->tdb_remote_cred->ref_len);

	if (sa->tdb_local_auth)
		i += sizeof(struct sadb_x_cred) + PADUP(sa->tdb_local_auth->ref_len);

	if (sa->tdb_remote_auth)
		i += sizeof(struct sadb_x_cred) + PADUP(sa->tdb_remote_auth->ref_len);

	if (sa->tdb_amxkey)
		i += sizeof(struct sadb_key) + PADUP(sa->tdb_amxkeylen);

	if (sa->tdb_emxkey)
		i += sizeof(struct sadb_key) + PADUP(sa->tdb_emxkeylen);

	if (sa->tdb_filter.sen_type) {
		i += 2 * sizeof(struct sadb_protocol);

		/* We'll need four of them: src, src mask, dst, dst mask. */
		switch (sa->tdb_filter.sen_type) {
		case SENT_IP4:
			i += 4 * PADUP(sizeof(struct sockaddr_in));
			i += 4 * sizeof(struct sadb_address);
			break;
#ifdef INET6
		case SENT_IP6:
			i += 4 * PADUP(sizeof(struct sockaddr_in6));
			i += 4 * sizeof(struct sadb_address);
			break;
#endif /* INET6 */
		default:
			rval = EINVAL;
			goto ret;
		}
	}

	if (sa->tdb_udpencap_port)
		i += sizeof(struct sadb_x_udpencap);

#if NPF > 0
	if (sa->tdb_tag)
		i += sizeof(struct sadb_x_tag) + PADUP(PF_TAG_NAME_SIZE);
	if (sa->tdb_tap)
		i += sizeof(struct sadb_x_tap);
#endif

	if (lenp)
		*lenp = i;

	if (buffer == NULL) {
		rval = 0;
		goto ret;
	}

	if (!(p = malloc(i, M_PFKEY, M_NOWAIT | M_ZERO))) {
		rval = ENOMEM;
		goto ret;
	} else
		*buffer = p;

	headers[SADB_EXT_SA] = p;

	export_sa(&p, sa);  /* Export SA information (mostly flags) */

	/* Export lifetimes where applicable */
	headers[SADB_EXT_LIFETIME_CURRENT] = p;
	export_lifetime(&p, sa, PFKEYV2_LIFETIME_CURRENT);

	if (sa->tdb_soft_allocations || sa->tdb_soft_bytes ||
	    sa->tdb_soft_first_use || sa->tdb_soft_timeout) {
		headers[SADB_EXT_LIFETIME_SOFT] = p;
		export_lifetime(&p, sa, PFKEYV2_LIFETIME_SOFT);
	}

	if (sa->tdb_exp_allocations || sa->tdb_exp_bytes ||
	    sa->tdb_exp_first_use || sa->tdb_exp_timeout) {
		headers[SADB_EXT_LIFETIME_HARD] = p;
		export_lifetime(&p, sa, PFKEYV2_LIFETIME_HARD);
	}

	if (sa->tdb_last_used) {
		headers[SADB_X_EXT_LIFETIME_LASTUSE] = p;
		export_lifetime(&p, sa, PFKEYV2_LIFETIME_LASTUSE);
	}

	/* Export TDB source address */
	headers[SADB_EXT_ADDRESS_SRC] = p;
	export_address(&p, (struct sockaddr *) &sa->tdb_src);

	/* Export TDB destination address */
	headers[SADB_EXT_ADDRESS_DST] = p;
	export_address(&p, (struct sockaddr *) &sa->tdb_dst);

	/* Export source identity, if present */
	if (sa->tdb_srcid) {
		headers[SADB_EXT_IDENTITY_SRC] = p;
		export_identity(&p, sa, PFKEYV2_IDENTITY_SRC);
	}

	/* Export destination identity, if present */
	if (sa->tdb_dstid) {
		headers[SADB_EXT_IDENTITY_DST] = p;
		export_identity(&p, sa, PFKEYV2_IDENTITY_DST);
	}

	/* Export credentials, if present */
	if (sa->tdb_local_cred) {
		headers[SADB_X_EXT_LOCAL_CREDENTIALS] = p;
		export_credentials(&p, sa, PFKEYV2_CRED_LOCAL);
	}

	if (sa->tdb_remote_cred) {
		headers[SADB_X_EXT_REMOTE_CREDENTIALS] = p;
		export_credentials(&p, sa, PFKEYV2_CRED_REMOTE);
	}

	/* Export authentication information, if present */
	if (sa->tdb_local_auth) {
		headers[SADB_X_EXT_LOCAL_AUTH] = p;
		export_auth(&p, sa, PFKEYV2_AUTH_LOCAL);
	}

	if (sa->tdb_remote_auth) {
		headers[SADB_X_EXT_REMOTE_AUTH] = p;
		export_auth(&p, sa, PFKEYV2_AUTH_REMOTE);
	}

	/* Export authentication key, if present */
	if (sa->tdb_amxkey) {
		headers[SADB_EXT_KEY_AUTH] = p;
		export_key(&p, sa, PFKEYV2_AUTHENTICATION_KEY);
	}

	/* Export encryption key, if present */
	if (sa->tdb_emxkey) {
		headers[SADB_EXT_KEY_ENCRYPT] = p;
		export_key(&p, sa, PFKEYV2_ENCRYPTION_KEY);
	}

	/* Export flow/filter, if present */
	if (sa->tdb_filter.sen_type)
		export_flow(&p, IPSP_IPSEC_USE, &sa->tdb_filter,
		    &sa->tdb_filtermask, headers);

	/* Export UDP encapsulation port, if present */
	if (sa->tdb_udpencap_port) {
		headers[SADB_X_EXT_UDPENCAP] = p;
		export_udpencap(&p, sa);
	}

#if NPF > 0
	/* Export tag information, if present */
	if (sa->tdb_tag) {
		headers[SADB_X_EXT_TAG] = p;
		export_tag(&p, sa);
	}

	/* Export tap enc(4) device information, if present */
	if (sa->tdb_tap) {
		headers[SADB_X_EXT_TAP] = p;
		export_tap(&p, sa);
	}
#endif

	rval = 0;

 ret:
	return (rval);
}

/*
 * Dump a TDB.
 */
int
pfkeyv2_dump_walker(struct tdb *sa, void *state, int last)
{
	struct dump_state *dump_state = (struct dump_state *) state;
	void *headers[SADB_EXT_MAX+1], *buffer;
	int rval;

	/* If not satype was specified, dump all TDBs */
	if (!dump_state->sadb_msg->sadb_msg_satype ||
	    (sa->tdb_satype == dump_state->sadb_msg->sadb_msg_satype)) {
		bzero(headers, sizeof(headers));
		headers[0] = (void *) dump_state->sadb_msg;

		/* Get the information from the TDB to a PFKEYv2 message */
		if ((rval = pfkeyv2_get(sa, headers, &buffer, NULL)) != 0)
			return (rval);

		if (last)
			((struct sadb_msg *)headers[0])->sadb_msg_seq = 0;

		/* Send the message to the specified socket */
		rval = pfkeyv2_sendmessage(headers,
		    PFKEYV2_SENDMESSAGE_UNICAST, dump_state->socket, 0, 0,
		    sa->tdb_rdomain);

		free(buffer, M_PFKEY, 0);
		if (rval)
			return (rval);
	}

	return (0);
}

/*
 * Delete an SA.
 */
int
pfkeyv2_flush_walker(struct tdb *sa, void *satype_vp, int last)
{
	if (!(*((u_int8_t *) satype_vp)) ||
	    sa->tdb_satype == *((u_int8_t *) satype_vp))
		tdb_delete(sa);
	return (0);
}

/*
 * Convert between SATYPEs and IPsec protocols, taking into consideration
 * sysctl variables enabling/disabling ESP/AH and the presence of the old
 * IPsec transforms.
 */
int
pfkeyv2_get_proto_alg(u_int8_t satype, u_int8_t *sproto, int *alg)
{
	switch (satype) {
#ifdef IPSEC
	case SADB_SATYPE_AH:
		if (!ah_enable)
			return (EOPNOTSUPP);

		*sproto = IPPROTO_AH;

		if(alg != NULL)
			*alg = satype = XF_AH;

		break;

	case SADB_SATYPE_ESP:
		if (!esp_enable)
			return (EOPNOTSUPP);

		*sproto = IPPROTO_ESP;

		if(alg != NULL)
			*alg = satype = XF_ESP;

		break;

	case SADB_X_SATYPE_IPIP:
		*sproto = IPPROTO_IPIP;

		if (alg != NULL)
			*alg = XF_IP4;

		break;

	case SADB_X_SATYPE_IPCOMP:
		if (!ipcomp_enable)
			return (EOPNOTSUPP);

		*sproto = IPPROTO_IPCOMP;

		if(alg != NULL)
			*alg = satype = XF_IPCOMP;

		break;
#endif /* IPSEC */
#ifdef TCP_SIGNATURE
	case SADB_X_SATYPE_TCPSIGNATURE:
		*sproto = IPPROTO_TCP;

		if (alg != NULL)
			*alg = XF_TCPSIGNATURE;

		break;
#endif /* TCP_SIGNATURE */

	default: /* Nothing else supported */
		return (EOPNOTSUPP);
	}

	return (0);
}

/*
 * Handle all messages from userland to kernel.
 */
int
pfkeyv2_send(struct socket *socket, void *message, int len)
{
	int i, j, rval = 0, mode = PFKEYV2_SENDMESSAGE_BROADCAST;
	int delflag = 0, s;
	struct sockaddr_encap encapdst, encapnetmask, encapgw;
	struct ipsec_policy *ipo, *tmpipo;
	struct ipsec_acquire *ipa;

	struct pfkeyv2_socket *pfkeyv2_socket, *so = NULL;

	void *freeme = NULL, *bckptr = NULL;
	void *headers[SADB_EXT_MAX + 1];

	union sockaddr_union *sunionp;

	struct tdb *sa1 = NULL, *sa2 = NULL;

	struct sadb_msg *smsg;
	struct sadb_spirange *sprng;
	struct sadb_sa *ssa;
	struct sadb_supported *ssup;
	struct sadb_ident *sid;

	u_int rdomain;

	/* Verify that we received this over a legitimate pfkeyv2 socket */
	bzero(headers, sizeof(headers));

	for (pfkeyv2_socket = pfkeyv2_sockets; pfkeyv2_socket;
	    pfkeyv2_socket = pfkeyv2_socket->next)
		if (pfkeyv2_socket->socket == socket)
			break;

	if (!pfkeyv2_socket) {
		rval = EINVAL;
		goto ret;
	}

	rdomain = pfkeyv2_socket->rdomain;

	/* If we have any promiscuous listeners, send them a copy of the message */
	if (npromisc) {
		struct mbuf *packet;

		if (!(freeme = malloc(sizeof(struct sadb_msg) + len, M_PFKEY,
		    M_NOWAIT))) {
			rval = ENOMEM;
			goto ret;
		}

		/* Initialize encapsulating header */
		bzero(freeme, sizeof(struct sadb_msg));
		smsg = (struct sadb_msg *) freeme;
		smsg->sadb_msg_version = PF_KEY_V2;
		smsg->sadb_msg_type = SADB_X_PROMISC;
		smsg->sadb_msg_len = (sizeof(struct sadb_msg) + len) /
		    sizeof(uint64_t);
		smsg->sadb_msg_seq = curproc->p_p->ps_pid;

		bcopy(message, freeme + sizeof(struct sadb_msg), len);

		/* Convert to mbuf chain */
		if ((rval = pfdatatopacket(freeme,
		    sizeof(struct sadb_msg) + len, &packet)) != 0)
			goto ret;

		/* Send to all promiscuous listeners */
		for (so = pfkeyv2_sockets; so; so = so->next) {
			if ((so->flags & PFKEYV2_SOCKETFLAGS_PROMISC) &&
			    (so->rdomain == rdomain))
				pfkey_sendup(so->socket, packet, 1);
		}

		m_freem(packet);

		/* Paranoid */
		explicit_bzero(freeme, sizeof(struct sadb_msg) + len);
		free(freeme, M_PFKEY, 0);
		freeme = NULL;
	}

	/* Validate message format */
	if ((rval = pfkeyv2_parsemessage(message, len, headers)) != 0)
		goto ret;

	smsg = (struct sadb_msg *) headers[0];
	switch (smsg->sadb_msg_type) {
	case SADB_GETSPI:  /* Reserve an SPI */
		sa1 = malloc(sizeof (*sa1), M_PFKEY, M_NOWAIT | M_ZERO);
		if (sa1 == NULL) {
			rval = ENOMEM;
			goto ret;
		}

		sa1->tdb_satype = smsg->sadb_msg_satype;
		if ((rval = pfkeyv2_get_proto_alg(sa1->tdb_satype,
		    &sa1->tdb_sproto, 0)))
			goto ret;

		import_address((struct sockaddr *) &sa1->tdb_src,
		    headers[SADB_EXT_ADDRESS_SRC]);
		import_address((struct sockaddr *) &sa1->tdb_dst,
		    headers[SADB_EXT_ADDRESS_DST]);

		/* Find an unused SA identifier */
		sprng = (struct sadb_spirange *) headers[SADB_EXT_SPIRANGE];
		sa1->tdb_spi = reserve_spi(rdomain,
		    sprng->sadb_spirange_min, sprng->sadb_spirange_max,
		    &sa1->tdb_src, &sa1->tdb_dst, sa1->tdb_sproto, &rval);
		if (sa1->tdb_spi == 0)
			goto ret;

		/* Send a message back telling what the SA (the SPI really) is */
		if (!(freeme = malloc(sizeof(struct sadb_sa), M_PFKEY,
		    M_NOWAIT | M_ZERO))) {
			rval = ENOMEM;
			goto ret;
		}

		headers[SADB_EXT_SPIRANGE] = NULL;
		headers[SADB_EXT_SA] = freeme;
		bckptr = freeme;

		/* We really only care about the SPI, but we'll export the SA */
		export_sa((void **) &bckptr, sa1);
		break;

	case SADB_UPDATE:
		ssa = (struct sadb_sa *) headers[SADB_EXT_SA];
		sunionp = (union sockaddr_union *) (headers[SADB_EXT_ADDRESS_DST] +
		    sizeof(struct sadb_address));

		/* Either all or none of the flow must be included */
		if ((headers[SADB_X_EXT_SRC_FLOW] ||
		    headers[SADB_X_EXT_PROTOCOL] ||
		    headers[SADB_X_EXT_FLOW_TYPE] ||
		    headers[SADB_X_EXT_DST_FLOW] ||
		    headers[SADB_X_EXT_SRC_MASK] ||
		    headers[SADB_X_EXT_DST_MASK]) &&
		    !(headers[SADB_X_EXT_SRC_FLOW] &&
		    headers[SADB_X_EXT_PROTOCOL] &&
		    headers[SADB_X_EXT_FLOW_TYPE] &&
		    headers[SADB_X_EXT_DST_FLOW] &&
		    headers[SADB_X_EXT_SRC_MASK] &&
		    headers[SADB_X_EXT_DST_MASK])) {
			rval = EINVAL;
			goto ret;
		}
#ifdef IPSEC
		/* UDP encap has to be enabled and is only supported for ESP */
		if (headers[SADB_X_EXT_UDPENCAP] &&
		    (!udpencap_enable ||
		    smsg->sadb_msg_satype != SADB_SATYPE_ESP)) {
			rval = EINVAL;
			goto ret;
		}
#endif /* IPSEC */

		s = splsoftnet();

		/* Find TDB */
		sa2 = gettdb(rdomain, ssa->sadb_sa_spi, sunionp,
		    SADB_X_GETSPROTO(smsg->sadb_msg_satype));

		/* If there's no such SA, we're done */
		if (sa2 == NULL) {
			rval = ESRCH;
			goto splxret;
		}

		/* If this is a reserved SA */
		if (sa2->tdb_flags & TDBF_INVALID) {
			struct tdb *newsa;
			struct ipsecinit ii;
			int alg;

			/* Create new TDB */
			freeme = tdb_alloc(rdomain);
			bzero(&ii, sizeof(struct ipsecinit));

			newsa = (struct tdb *) freeme;
			newsa->tdb_satype = smsg->sadb_msg_satype;

			if ((rval = pfkeyv2_get_proto_alg(newsa->tdb_satype,
			    &newsa->tdb_sproto, &alg))) {
				tdb_free(freeme);
				freeme = NULL;
				goto splxret;
			}

			/* Initialize SA */
			import_sa(newsa, headers[SADB_EXT_SA], &ii);
			import_address((struct sockaddr *) &newsa->tdb_src,
			    headers[SADB_EXT_ADDRESS_SRC]);
			import_address((struct sockaddr *) &newsa->tdb_dst,
			    headers[SADB_EXT_ADDRESS_DST]);
			import_lifetime(newsa,
			    headers[SADB_EXT_LIFETIME_CURRENT],
			    PFKEYV2_LIFETIME_CURRENT);
			import_lifetime(newsa, headers[SADB_EXT_LIFETIME_SOFT],
			    PFKEYV2_LIFETIME_SOFT);
			import_lifetime(newsa, headers[SADB_EXT_LIFETIME_HARD],
			    PFKEYV2_LIFETIME_HARD);
			import_key(&ii, headers[SADB_EXT_KEY_AUTH],
			    PFKEYV2_AUTHENTICATION_KEY);
			import_key(&ii, headers[SADB_EXT_KEY_ENCRYPT],
			    PFKEYV2_ENCRYPTION_KEY);
			import_identity(newsa, headers[SADB_EXT_IDENTITY_SRC],
			    PFKEYV2_IDENTITY_SRC);
			import_identity(newsa, headers[SADB_EXT_IDENTITY_DST],
			    PFKEYV2_IDENTITY_DST);
			import_credentials(newsa,
			    headers[SADB_X_EXT_LOCAL_CREDENTIALS],
			    PFKEYV2_CRED_LOCAL);
			import_credentials(newsa,
			    headers[SADB_X_EXT_REMOTE_CREDENTIALS],
			    PFKEYV2_CRED_REMOTE);
			import_auth(newsa, headers[SADB_X_EXT_LOCAL_AUTH],
			    PFKEYV2_AUTH_LOCAL);
			import_auth(newsa, headers[SADB_X_EXT_REMOTE_AUTH],
			    PFKEYV2_AUTH_REMOTE);
			import_flow(&newsa->tdb_filter, &newsa->tdb_filtermask,
			    headers[SADB_X_EXT_SRC_FLOW],
			    headers[SADB_X_EXT_SRC_MASK],
			    headers[SADB_X_EXT_DST_FLOW],
			    headers[SADB_X_EXT_DST_MASK],
			    headers[SADB_X_EXT_PROTOCOL],
			    headers[SADB_X_EXT_FLOW_TYPE]);
			import_udpencap(newsa, headers[SADB_X_EXT_UDPENCAP]);
#if NPF > 0
			import_tag(newsa, headers[SADB_X_EXT_TAG]);
			import_tap(newsa, headers[SADB_X_EXT_TAP]);
#endif

			/* Exclude sensitive data from reply message. */
			headers[SADB_EXT_KEY_AUTH] = NULL;
			headers[SADB_EXT_KEY_ENCRYPT] = NULL;
			headers[SADB_X_EXT_LOCAL_AUTH] = NULL;
			headers[SADB_X_EXT_REMOTE_AUTH] = NULL;

			newsa->tdb_seq = smsg->sadb_msg_seq;

			rval = tdb_init(newsa, alg, &ii);
			if (rval) {
				rval = EINVAL;
				tdb_free(freeme);
				freeme = NULL;
				goto splxret;
			}

			newsa->tdb_cur_allocations = sa2->tdb_cur_allocations;

			/* Delete old version of the SA, insert new one */
			tdb_delete(sa2);
			puttdb((struct tdb *) freeme);
			sa2 = freeme = NULL;
		} else {
			/*
			 * The SA is already initialized, so we're only allowed to
			 * change lifetimes and some other information; we're
			 * not allowed to change keys, addresses or identities.
			 */
			if (headers[SADB_EXT_KEY_AUTH] ||
			    headers[SADB_EXT_KEY_ENCRYPT] ||
			    headers[SADB_EXT_IDENTITY_SRC] ||
			    headers[SADB_EXT_IDENTITY_DST] ||
			    headers[SADB_EXT_SENSITIVITY]) {
				rval = EINVAL;
				goto splxret;
			}

			import_sa(sa2, headers[SADB_EXT_SA], NULL);
			import_lifetime(sa2,
			    headers[SADB_EXT_LIFETIME_CURRENT],
			    PFKEYV2_LIFETIME_CURRENT);
			import_lifetime(sa2, headers[SADB_EXT_LIFETIME_SOFT],
			    PFKEYV2_LIFETIME_SOFT);
			import_lifetime(sa2, headers[SADB_EXT_LIFETIME_HARD],
			    PFKEYV2_LIFETIME_HARD);
			import_udpencap(sa2, headers[SADB_X_EXT_UDPENCAP]);
#if NPF > 0
			import_tag(sa2, headers[SADB_X_EXT_TAG]);
			import_tap(sa2, headers[SADB_X_EXT_TAP]);
#endif
		}

		splx(s);
		break;
	case SADB_ADD:
		ssa = (struct sadb_sa *) headers[SADB_EXT_SA];
		sunionp = (union sockaddr_union *) (headers[SADB_EXT_ADDRESS_DST] +
		    sizeof(struct sadb_address));

		/* Either all or none of the flow must be included */
		if ((headers[SADB_X_EXT_SRC_FLOW] ||
		    headers[SADB_X_EXT_PROTOCOL] ||
		    headers[SADB_X_EXT_FLOW_TYPE] ||
		    headers[SADB_X_EXT_DST_FLOW] ||
		    headers[SADB_X_EXT_SRC_MASK] ||
		    headers[SADB_X_EXT_DST_MASK]) &&
		    !(headers[SADB_X_EXT_SRC_FLOW] &&
		    headers[SADB_X_EXT_PROTOCOL] &&
		    headers[SADB_X_EXT_FLOW_TYPE] &&
		    headers[SADB_X_EXT_DST_FLOW] &&
		    headers[SADB_X_EXT_SRC_MASK] &&
		    headers[SADB_X_EXT_DST_MASK])) {
			rval = EINVAL;
			goto ret;
		}
#ifdef IPSEC
		/* UDP encap has to be enabled and is only supported for ESP */
		if (headers[SADB_X_EXT_UDPENCAP] &&
		    (!udpencap_enable ||
		    smsg->sadb_msg_satype != SADB_SATYPE_ESP)) {
			rval = EINVAL;
			goto ret;
		}
#endif /* IPSEC */

		s = splsoftnet();

		sa2 = gettdb(rdomain, ssa->sadb_sa_spi, sunionp,
		    SADB_X_GETSPROTO(smsg->sadb_msg_satype));

		/* We can't add an existing SA! */
		if (sa2 != NULL) {
			rval = EEXIST;
			goto splxret;
		}

		/* We can only add "mature" SAs */
		if (ssa->sadb_sa_state != SADB_SASTATE_MATURE) {
			rval = EINVAL;
			goto splxret;
		}

		/* Allocate and initialize new TDB */
		freeme = tdb_alloc(rdomain);

		{
			struct tdb *newsa = (struct tdb *) freeme;
			struct ipsecinit ii;
			int alg;

			bzero(&ii, sizeof(struct ipsecinit));

			newsa->tdb_satype = smsg->sadb_msg_satype;
			if ((rval = pfkeyv2_get_proto_alg(newsa->tdb_satype,
			    &newsa->tdb_sproto, &alg))) {
				tdb_free(freeme);
				freeme = NULL;
				goto splxret;
			}

			import_sa(newsa, headers[SADB_EXT_SA], &ii);
			import_address((struct sockaddr *) &newsa->tdb_src,
			    headers[SADB_EXT_ADDRESS_SRC]);
			import_address((struct sockaddr *) &newsa->tdb_dst,
			    headers[SADB_EXT_ADDRESS_DST]);

			import_lifetime(newsa,
			    headers[SADB_EXT_LIFETIME_CURRENT],
			    PFKEYV2_LIFETIME_CURRENT);
			import_lifetime(newsa, headers[SADB_EXT_LIFETIME_SOFT],
			    PFKEYV2_LIFETIME_SOFT);
			import_lifetime(newsa, headers[SADB_EXT_LIFETIME_HARD],
			    PFKEYV2_LIFETIME_HARD);

			import_key(&ii, headers[SADB_EXT_KEY_AUTH],
			    PFKEYV2_AUTHENTICATION_KEY);
			import_key(&ii, headers[SADB_EXT_KEY_ENCRYPT],
			    PFKEYV2_ENCRYPTION_KEY);

			import_identity(newsa, headers[SADB_EXT_IDENTITY_SRC],
			    PFKEYV2_IDENTITY_SRC);
			import_identity(newsa, headers[SADB_EXT_IDENTITY_DST],
			    PFKEYV2_IDENTITY_DST);

			import_credentials(newsa,
			    headers[SADB_X_EXT_LOCAL_CREDENTIALS],
			    PFKEYV2_CRED_LOCAL);
			import_credentials(newsa,
			    headers[SADB_X_EXT_REMOTE_CREDENTIALS],
			    PFKEYV2_CRED_REMOTE);
			import_auth(newsa, headers[SADB_X_EXT_LOCAL_AUTH],
			    PFKEYV2_AUTH_LOCAL);
			import_auth(newsa, headers[SADB_X_EXT_REMOTE_AUTH],
			    PFKEYV2_AUTH_REMOTE);
			import_flow(&newsa->tdb_filter, &newsa->tdb_filtermask,
			    headers[SADB_X_EXT_SRC_FLOW],
			    headers[SADB_X_EXT_SRC_MASK],
			    headers[SADB_X_EXT_DST_FLOW],
			    headers[SADB_X_EXT_DST_MASK],
			    headers[SADB_X_EXT_PROTOCOL],
			    headers[SADB_X_EXT_FLOW_TYPE]);
			import_udpencap(newsa, headers[SADB_X_EXT_UDPENCAP]);
#if NPF > 0
			import_tag(newsa, headers[SADB_X_EXT_TAG]);
			import_tap(newsa, headers[SADB_X_EXT_TAP]);
#endif

			/* Exclude sensitive data from reply message. */
			headers[SADB_EXT_KEY_AUTH] = NULL;
			headers[SADB_EXT_KEY_ENCRYPT] = NULL;
			headers[SADB_X_EXT_LOCAL_AUTH] = NULL;
			headers[SADB_X_EXT_REMOTE_AUTH] = NULL;

			newsa->tdb_seq = smsg->sadb_msg_seq;

			rval = tdb_init(newsa, alg, &ii);
			if (rval) {
				rval = EINVAL;
				tdb_free(freeme);
				freeme = NULL;
				goto splxret;
			}
		}

		/* Add TDB in table */
		puttdb((struct tdb *) freeme);

		splx(s);

		freeme = NULL;
		break;

	case SADB_DELETE:
		ssa = (struct sadb_sa *) headers[SADB_EXT_SA];
		sunionp =
		    (union sockaddr_union *)(headers[SADB_EXT_ADDRESS_DST] +
			sizeof(struct sadb_address));
		s = splsoftnet();

		sa2 = gettdb(rdomain, ssa->sadb_sa_spi, sunionp,
		    SADB_X_GETSPROTO(smsg->sadb_msg_satype));
		if (sa2 == NULL) {
			rval = ESRCH;
			goto splxret;
		}

		tdb_delete(sa2);

		splx(s);

		sa2 = NULL;
		break;

	case SADB_X_ASKPOLICY:
		/* Get the relevant policy */
		ipa = ipsec_get_acquire(((struct sadb_x_policy *) headers[SADB_X_EXT_POLICY])->sadb_x_policy_seq);
		if (ipa == NULL) {
			rval = ESRCH;
			goto ret;
		}

		rval = pfkeyv2_policy(ipa, headers, &freeme);
		if (rval)
			mode = PFKEYV2_SENDMESSAGE_UNICAST;

		break;

	case SADB_GET:
		ssa = (struct sadb_sa *) headers[SADB_EXT_SA];
		sunionp =
		    (union sockaddr_union *)(headers[SADB_EXT_ADDRESS_DST] +
			sizeof(struct sadb_address));

		s = splsoftnet();

		sa2 = gettdb(rdomain, ssa->sadb_sa_spi, sunionp,
		    SADB_X_GETSPROTO(smsg->sadb_msg_satype));
		if (sa2 == NULL) {
			rval = ESRCH;
			goto splxret;
		}

		rval = pfkeyv2_get(sa2, headers, &freeme, NULL);
		if (rval)
			mode = PFKEYV2_SENDMESSAGE_UNICAST;

		splx(s);

		break;

	case SADB_REGISTER:
		if (!(pfkeyv2_socket->flags & PFKEYV2_SOCKETFLAGS_REGISTERED)) {
			pfkeyv2_socket->flags |= PFKEYV2_SOCKETFLAGS_REGISTERED;
			nregistered++;
		}

		i = sizeof(struct sadb_supported) + sizeof(ealgs);

		if (!(freeme = malloc(i, M_PFKEY, M_NOWAIT | M_ZERO))) {
			rval = ENOMEM;
			goto ret;
		}

		ssup = (struct sadb_supported *) freeme;
		ssup->sadb_supported_len = i / sizeof(uint64_t);

		{
			void *p = freeme + sizeof(struct sadb_supported);

			bcopy(&ealgs[0], p, sizeof(ealgs));
		}

		headers[SADB_EXT_SUPPORTED_ENCRYPT] = freeme;

		i = sizeof(struct sadb_supported) + sizeof(aalgs);

		if (!(freeme = malloc(i, M_PFKEY, M_NOWAIT | M_ZERO))) {
			rval = ENOMEM;
			goto ret;
		}

		/* Keep track what this socket has registered for */
		pfkeyv2_socket->registration |= (1 << ((struct sadb_msg *)message)->sadb_msg_satype);

		ssup = (struct sadb_supported *) freeme;
		ssup->sadb_supported_len = i / sizeof(uint64_t);

		{
			void *p = freeme + sizeof(struct sadb_supported);

			bcopy(&aalgs[0], p, sizeof(aalgs));
		}

		headers[SADB_EXT_SUPPORTED_AUTH] = freeme;

		i = sizeof(struct sadb_supported) + sizeof(calgs);

		if (!(freeme = malloc(i, M_PFKEY, M_NOWAIT | M_ZERO))) {
			rval = ENOMEM;
			goto ret;
		}

		ssup = (struct sadb_supported *) freeme;
		ssup->sadb_supported_len = i / sizeof(uint64_t);

		{
			void *p = freeme + sizeof(struct sadb_supported);

			bcopy(&calgs[0], p, sizeof(calgs));
		}

		headers[SADB_X_EXT_SUPPORTED_COMP] = freeme;

		break;

	case SADB_ACQUIRE:
	case SADB_EXPIRE:
		/* Nothing to handle */
		rval = 0;
		break;

	case SADB_FLUSH:
		rval = 0;

		switch (smsg->sadb_msg_satype) {
		case SADB_SATYPE_UNSPEC:
			s = splsoftnet();

			/*
			 * Go through the list of policies, delete those that
			 * are not socket-attached.
			 */
			for (ipo = TAILQ_FIRST(&ipsec_policy_head);
			    ipo != NULL; ipo = tmpipo) {
				tmpipo = TAILQ_NEXT(ipo, ipo_list);
				if (!(ipo->ipo_flags & IPSP_POLICY_SOCKET) &&
				    ipo->ipo_rdomain == rdomain)
					ipsec_delete_policy(ipo);
			}
			splx(s);
			/* FALLTHROUGH */
		case SADB_SATYPE_AH:
		case SADB_SATYPE_ESP:
		case SADB_X_SATYPE_IPIP:
		case SADB_X_SATYPE_IPCOMP:
#ifdef TCP_SIGNATURE
		case SADB_X_SATYPE_TCPSIGNATURE:
#endif /* TCP_SIGNATURE */
			s = splsoftnet();

			tdb_walk(rdomain, pfkeyv2_flush_walker,
			    (u_int8_t *) &(smsg->sadb_msg_satype));

			splx(s);
			break;

		default:
			rval = EINVAL; /* Unknown/unsupported type */
		}

		break;

	case SADB_DUMP:
	{
		struct dump_state dump_state;
		dump_state.sadb_msg = (struct sadb_msg *) headers[0];
		dump_state.socket = socket;

		s = splsoftnet();
		rval = tdb_walk(rdomain, pfkeyv2_dump_walker, &dump_state);
		splx(s);

		if (!rval)
			goto realret;

		if ((rval == ENOMEM) || (rval == ENOBUFS))
			rval = 0;
	}
	break;

	case SADB_X_GRPSPIS:
	{
		struct tdb *tdb1, *tdb2, *tdb3;
		struct sadb_protocol *sa_proto;

		ssa = (struct sadb_sa *) headers[SADB_EXT_SA];
		sunionp = (union sockaddr_union *) (headers[SADB_EXT_ADDRESS_DST] +
		    sizeof(struct sadb_address));

		s = splsoftnet();

		tdb1 = gettdb(rdomain, ssa->sadb_sa_spi, sunionp,
		    SADB_X_GETSPROTO(smsg->sadb_msg_satype));
		if (tdb1 == NULL) {
			rval = ESRCH;
			goto splxret;
		}

		ssa = (struct sadb_sa *) headers[SADB_X_EXT_SA2];
		sunionp = (union sockaddr_union *) (headers[SADB_X_EXT_DST2] +
		    sizeof(struct sadb_address));
		sa_proto = ((struct sadb_protocol *) headers[SADB_X_EXT_PROTOCOL]);

		tdb2 = gettdb(rdomain, ssa->sadb_sa_spi, sunionp,
		    SADB_X_GETSPROTO(sa_proto->sadb_protocol_proto));
		if (tdb2 == NULL) {
			rval = ESRCH;
			goto splxret;
		}

		/* Detect cycles */
		for (tdb3 = tdb2; tdb3; tdb3 = tdb3->tdb_onext)
			if (tdb3 == tdb1) {
				rval = ESRCH;
				goto splxret;
			}

		/* Maintenance */
		if ((tdb1->tdb_onext) &&
		    (tdb1->tdb_onext->tdb_inext == tdb1))
			tdb1->tdb_onext->tdb_inext = NULL;

		if ((tdb2->tdb_inext) &&
		    (tdb2->tdb_inext->tdb_onext == tdb2))
			tdb2->tdb_inext->tdb_onext = NULL;

		/* Link them */
		tdb1->tdb_onext = tdb2;
		tdb2->tdb_inext = tdb1;

		splx(s);
	}
	break;

	case SADB_X_DELFLOW:
		delflag = 1;
		/*FALLTHROUGH*/
	case SADB_X_ADDFLOW:
	{
		struct sadb_protocol *sab;
		union sockaddr_union *ssrc;
		struct rtentry *rt;
		int exists = 0;

		sab = (struct sadb_protocol *) headers[SADB_X_EXT_FLOW_TYPE];

		if ((sab->sadb_protocol_direction != IPSP_DIRECTION_IN) &&
		    (sab->sadb_protocol_direction != IPSP_DIRECTION_OUT)) {
			rval = EINVAL;
			goto ret;
		}

		/* If the security protocol wasn't specified, pretend it was ESP */
		if (smsg->sadb_msg_satype == 0)
			smsg->sadb_msg_satype = SADB_SATYPE_ESP;

		if (headers[SADB_EXT_ADDRESS_DST])
			sunionp = (union sockaddr_union *)
			    (headers[SADB_EXT_ADDRESS_DST] +
				sizeof(struct sadb_address));
		else
			sunionp = NULL;

		if (headers[SADB_EXT_ADDRESS_SRC])
			ssrc = (union sockaddr_union *)
			    (headers[SADB_EXT_ADDRESS_SRC] +
				sizeof(struct sadb_address));
		else
			ssrc = NULL;

		import_flow(&encapdst, &encapnetmask,
		    headers[SADB_X_EXT_SRC_FLOW], headers[SADB_X_EXT_SRC_MASK],
		    headers[SADB_X_EXT_DST_FLOW], headers[SADB_X_EXT_DST_MASK],
		    headers[SADB_X_EXT_PROTOCOL], headers[SADB_X_EXT_FLOW_TYPE]);

		/* Determine whether the exact same SPD entry already exists. */
		bzero(&encapgw, sizeof(struct sockaddr_encap));

		s = splsoftnet();
		rt = rtalloc((struct sockaddr *)&encapdst, RT_REPORT|RT_RESOLVE,
		    rdomain);
		if (rt != NULL) {
			ipo = ((struct sockaddr_encap *)rt->rt_gateway)->sen_ipsp;
			rtfree(rt);

			/* Verify that the entry is identical */
			if (bcmp(&ipo->ipo_addr, &encapdst,
				sizeof(struct sockaddr_encap)) ||
			    bcmp(&ipo->ipo_mask, &encapnetmask,
				sizeof(struct sockaddr_encap)))
				ipo = NULL; /* Fall through */
			else
				exists = 1;
		} else
			ipo = NULL;

		/*
		 * If the existing policy is static, only delete or update
		 * it if the new one is also static.
		 */
		if (exists && (ipo->ipo_flags & IPSP_POLICY_STATIC)) {
			if (!(sab->sadb_protocol_flags &
				SADB_X_POLICYFLAGS_POLICY)) {
				splx(s);
				goto ret;
			}
		}

		/* Delete ? */
		if (delflag) {
			if (exists) {
				rval = ipsec_delete_policy(ipo);
				splx(s);
				goto ret;
			}

			/* If we were asked to delete something non-existent, error. */
			splx(s);
			rval = ESRCH;
			break;
		}

		if (!exists) {
			if (ipsec_policy_pool_initialized == 0) {
				ipsec_policy_pool_initialized = 1;
				pool_init(&ipsec_policy_pool,
				    sizeof(struct ipsec_policy), 0, 0, 0,
				    "ipsec policy", NULL);
			}

			/* Allocate policy entry */
			ipo = pool_get(&ipsec_policy_pool, PR_NOWAIT);
			if (ipo == NULL) {
				splx(s);
				rval = ENOMEM;
				goto ret;
			}

			bzero(ipo, sizeof(struct ipsec_policy));
			ipo->ipo_ref_count = 1;
			TAILQ_INIT(&ipo->ipo_acquires);

			/* Finish initialization of SPD entry */
			encapgw.sen_len = SENT_LEN;
			encapgw.sen_family = PF_KEY;
			encapgw.sen_type = SENT_IPSP;
			encapgw.sen_ipsp = ipo;

			/* Initialize policy entry */
			bcopy(&encapdst, &ipo->ipo_addr,
			    sizeof(struct sockaddr_encap));
			bcopy(&encapnetmask, &ipo->ipo_mask,
			    sizeof(struct sockaddr_encap));

			ipo->ipo_rdomain = rdomain;
		}

		switch (((struct sadb_protocol *) headers[SADB_X_EXT_FLOW_TYPE])->sadb_protocol_proto) {
		case SADB_X_FLOW_TYPE_USE:
			ipo->ipo_type = IPSP_IPSEC_USE;
			break;

		case SADB_X_FLOW_TYPE_ACQUIRE:
			ipo->ipo_type = IPSP_IPSEC_ACQUIRE;
			break;

		case SADB_X_FLOW_TYPE_REQUIRE:
			ipo->ipo_type = IPSP_IPSEC_REQUIRE;
			break;

		case SADB_X_FLOW_TYPE_DENY:
			ipo->ipo_type = IPSP_DENY;
			break;

		case SADB_X_FLOW_TYPE_BYPASS:
			ipo->ipo_type = IPSP_PERMIT;
			break;

		case SADB_X_FLOW_TYPE_DONTACQ:
			ipo->ipo_type = IPSP_IPSEC_DONTACQ;
			break;

		default:
			if (!exists)
				pool_put(&ipsec_policy_pool, ipo);
			else
				ipsec_delete_policy(ipo);

			splx(s);
			rval = EINVAL;
			goto ret;
		}

		if (sab->sadb_protocol_flags & SADB_X_POLICYFLAGS_POLICY)
			ipo->ipo_flags |= IPSP_POLICY_STATIC;

		if (sunionp)
			bcopy(sunionp, &ipo->ipo_dst,
			    sizeof(union sockaddr_union));
		else
			bzero(&ipo->ipo_dst, sizeof(union sockaddr_union));

		if (ssrc)
			bcopy(ssrc, &ipo->ipo_src,
			    sizeof(union sockaddr_union));
		else
			bzero(&ipo->ipo_src, sizeof(union sockaddr_union));

		ipo->ipo_sproto = SADB_X_GETSPROTO(smsg->sadb_msg_satype);

		if (ipo->ipo_srcid) {
			ipsp_reffree(ipo->ipo_srcid);
			ipo->ipo_srcid = NULL;
		}

		if (ipo->ipo_dstid) {
			ipsp_reffree(ipo->ipo_dstid);
			ipo->ipo_dstid = NULL;
		}

		if ((sid = headers[SADB_EXT_IDENTITY_SRC]) != NULL) {
			int clen =  (sid->sadb_ident_len * sizeof(u_int64_t)) -
			    sizeof(struct sadb_ident);

			ipo->ipo_srcid = malloc(clen + sizeof(struct ipsec_ref),
			    M_CREDENTIALS, M_NOWAIT);
			if (ipo->ipo_srcid == NULL) {
				if (exists)
					ipsec_delete_policy(ipo);
				else
					pool_put(&ipsec_policy_pool, ipo);
				splx(s);
				rval = ENOBUFS;
				goto ret;
			}
			ipo->ipo_srcid->ref_type = sid->sadb_ident_type;
			ipo->ipo_srcid->ref_len = clen;
			ipo->ipo_srcid->ref_count = 1;
			ipo->ipo_srcid->ref_malloctype = M_CREDENTIALS;
			bcopy(sid + 1, ipo->ipo_srcid + 1, ipo->ipo_srcid->ref_len);
		}

		if ((sid = headers[SADB_EXT_IDENTITY_DST]) != NULL) {
			int clen =  (sid->sadb_ident_len * sizeof(u_int64_t)) -
			    sizeof(struct sadb_ident);

			ipo->ipo_dstid = malloc(clen + sizeof(struct ipsec_ref),
			    M_CREDENTIALS, M_NOWAIT);
			if (ipo->ipo_dstid == NULL) {
				if (exists)
					ipsec_delete_policy(ipo);
				else {
					if (ipo->ipo_dstid)
						ipsp_reffree(ipo->ipo_dstid);
					pool_put(&ipsec_policy_pool, ipo);
				}

				splx(s);
				rval = ENOBUFS;
				goto ret;
			}
			ipo->ipo_dstid->ref_type = sid->sadb_ident_type;
			ipo->ipo_dstid->ref_len = clen;
			ipo->ipo_dstid->ref_count = 1;
			ipo->ipo_dstid->ref_malloctype = M_CREDENTIALS;
			bcopy(sid + 1, ipo->ipo_dstid + 1,
			    ipo->ipo_dstid->ref_len);
		}

		/* Flow type */
		if (!exists) {
			/* Add SPD entry */
			struct rt_addrinfo info;

			bzero(&info, sizeof(info));
			info.rti_info[RTAX_DST] = (struct sockaddr *)&encapdst;
			info.rti_info[RTAX_GATEWAY] =
			    (struct sockaddr *)&encapgw;
			info.rti_info[RTAX_NETMASK] =
			    (struct sockaddr *)&encapnetmask;
			info.rti_flags = RTF_UP | RTF_GATEWAY | RTF_STATIC;
			if ((rval = rtrequest1(RTM_ADD, &info, RTP_DEFAULT,
			    NULL, rdomain)) != 0) {
				/* Remove from linked list of policies on TDB */
				if (ipo->ipo_tdb)
					TAILQ_REMOVE(&ipo->ipo_tdb->tdb_policy_head,
					    ipo, ipo_tdb_next);

				if (ipo->ipo_srcid)
					ipsp_reffree(ipo->ipo_srcid);
				if (ipo->ipo_dstid)
					ipsp_reffree(ipo->ipo_dstid);
				pool_put(&ipsec_policy_pool, ipo);

				splx(s);
				goto ret;
			}

			TAILQ_INSERT_HEAD(&ipsec_policy_head, ipo, ipo_list);
			ipsec_in_use++;
		} else {
			ipo->ipo_last_searched = ipo->ipo_flags = 0;
		}

		splx(s);
	}
	break;

	case SADB_X_PROMISC:
		if (len >= 2 * sizeof(struct sadb_msg)) {
			struct mbuf *packet;

			if ((rval = pfdatatopacket(message, len, &packet)) != 0)
				goto ret;

			for (so = pfkeyv2_sockets; so; so = so->next)
				if ((so != pfkeyv2_socket) &&
				    (so->rdomain == rdomain) &&
				    (!smsg->sadb_msg_seq ||
				    (smsg->sadb_msg_seq == pfkeyv2_socket->pid)))
					pfkey_sendup(so->socket, packet, 1);

			m_freem(packet);
		} else {
			if (len != sizeof(struct sadb_msg)) {
				rval = EINVAL;
				goto ret;
			}

			i = (pfkeyv2_socket->flags &
			    PFKEYV2_SOCKETFLAGS_PROMISC) ? 1 : 0;
			j = smsg->sadb_msg_satype ? 1 : 0;

			if (i ^ j) {
				if (j) {
					pfkeyv2_socket->flags |=
					    PFKEYV2_SOCKETFLAGS_PROMISC;
					npromisc++;
				} else {
					pfkeyv2_socket->flags &=
					    ~PFKEYV2_SOCKETFLAGS_PROMISC;
					npromisc--;
				}
			}
		}
	

		break;

	default:
		rval = EINVAL;
		goto ret;
	}

ret:
	if (rval) {
		if ((rval == EINVAL) || (rval == ENOMEM) || (rval == ENOBUFS))
			goto realret;

		for (i = 1; i <= SADB_EXT_MAX; i++)
			headers[i] = NULL;

		smsg->sadb_msg_errno = abs(rval);
	} else {
		uint64_t seen = 0LL;

		for (i = 1; i <= SADB_EXT_MAX; i++)
			if (headers[i])
				seen |= (1LL << i);

		if ((seen & sadb_exts_allowed_out[smsg->sadb_msg_type])
		    != seen)
			goto realret;

		if ((seen & sadb_exts_required_out[smsg->sadb_msg_type]) !=
		    sadb_exts_required_out[smsg->sadb_msg_type])
			goto realret;
	}

	rval = pfkeyv2_sendmessage(headers, mode, socket, 0, 0, rdomain);

realret:
	if (freeme)
		free(freeme, M_PFKEY, 0);

	explicit_bzero(message, len);
	free(message, M_PFKEY, 0);

	if (sa1)
		free(sa1, M_PFKEY, 0);

	return (rval);

splxret:
	splx(s);
	goto ret;
}

/*
 * Send an ACQUIRE message to key management, to get a new SA.
 */
int
pfkeyv2_acquire(struct ipsec_policy *ipo, union sockaddr_union *gw,
    union sockaddr_union *laddr, u_int32_t *seq, struct sockaddr_encap *ddst)
{
	void *p, *headers[SADB_EXT_MAX + 1], *buffer = NULL;
	struct sadb_ident *srcid, *dstid;
	struct sadb_x_cred *lcred, *lauth;
	struct sadb_comb *sadb_comb;
	struct sadb_address *sadd;
	struct sadb_prop *sa_prop;
	struct sadb_msg *smsg;
	int rval = 0;
	int i, j;

	*seq = pfkeyv2_seq++;

	if (!nregistered) {
		rval = ESRCH;
		goto ret;
	}

	/* How large a buffer do we need... XXX we only do one proposal for now */
	i = sizeof(struct sadb_msg) +
	    (laddr == NULL ? 0 : sizeof(struct sadb_address) +
		PADUP(SA_LEN(&ipo->ipo_src.sa))) +
	    sizeof(struct sadb_address) + PADUP(SA_LEN(&gw->sa)) +
	    sizeof(struct sadb_prop) + 1 * sizeof(struct sadb_comb);

	if (ipo->ipo_srcid)
		i += sizeof(struct sadb_ident) + PADUP(ipo->ipo_srcid->ref_len);

	if (ipo->ipo_dstid)
		i += sizeof(struct sadb_ident) + PADUP(ipo->ipo_dstid->ref_len);

	if (ipo->ipo_local_cred)
		i += sizeof(struct sadb_x_cred) + PADUP(ipo->ipo_local_cred->ref_len);

	if (ipo->ipo_local_auth)
		i += sizeof(struct sadb_x_cred) + PADUP(ipo->ipo_local_auth->ref_len);

	/* Allocate */
	if (!(p = malloc(i, M_PFKEY, M_NOWAIT | M_ZERO))) {
		rval = ENOMEM;
		goto ret;
	}

	bzero(headers, sizeof(headers));

	buffer = p;

	headers[0] = p;
	p += sizeof(struct sadb_msg);

	smsg = (struct sadb_msg *) headers[0];
	smsg->sadb_msg_version = PF_KEY_V2;
	smsg->sadb_msg_type = SADB_ACQUIRE;
	smsg->sadb_msg_len = i / sizeof(uint64_t);
	smsg->sadb_msg_seq = *seq;

	if (ipo->ipo_sproto == IPPROTO_ESP)
		smsg->sadb_msg_satype = SADB_SATYPE_ESP;
	else if (ipo->ipo_sproto == IPPROTO_AH)
		smsg->sadb_msg_satype = SADB_SATYPE_AH;
	else if (ipo->ipo_sproto == IPPROTO_IPCOMP)
		smsg->sadb_msg_satype = SADB_X_SATYPE_IPCOMP;

	if (laddr) {
		headers[SADB_EXT_ADDRESS_SRC] = p;
		p += sizeof(struct sadb_address) + PADUP(SA_LEN(&laddr->sa));
		sadd = (struct sadb_address *) headers[SADB_EXT_ADDRESS_SRC];
		sadd->sadb_address_len = (sizeof(struct sadb_address) +
		    SA_LEN(&laddr->sa) + sizeof(uint64_t) - 1) /
		    sizeof(uint64_t);
		bcopy(laddr, headers[SADB_EXT_ADDRESS_SRC] +
		    sizeof(struct sadb_address), SA_LEN(&laddr->sa));
	}

	headers[SADB_EXT_ADDRESS_DST] = p;
	p += sizeof(struct sadb_address) + PADUP(SA_LEN(&gw->sa));
	sadd = (struct sadb_address *) headers[SADB_EXT_ADDRESS_DST];
	sadd->sadb_address_len = (sizeof(struct sadb_address) +
	    SA_LEN(&gw->sa) + sizeof(uint64_t) - 1) / sizeof(uint64_t);
	bcopy(gw, headers[SADB_EXT_ADDRESS_DST] + sizeof(struct sadb_address),
	    SA_LEN(&gw->sa));

	if (ipo->ipo_srcid) {
		headers[SADB_EXT_IDENTITY_SRC] = p;
		p += sizeof(struct sadb_ident) + PADUP(ipo->ipo_srcid->ref_len);
		srcid = (struct sadb_ident *) headers[SADB_EXT_IDENTITY_SRC];
		srcid->sadb_ident_len = (sizeof(struct sadb_ident) +
		    PADUP(ipo->ipo_srcid->ref_len)) / sizeof(u_int64_t);
		srcid->sadb_ident_type = ipo->ipo_srcid->ref_type;
		bcopy(ipo->ipo_srcid + 1, headers[SADB_EXT_IDENTITY_SRC] +
		    sizeof(struct sadb_ident), ipo->ipo_srcid->ref_len);
	}

	if (ipo->ipo_dstid) {
		headers[SADB_EXT_IDENTITY_DST] = p;
		p += sizeof(struct sadb_ident) + PADUP(ipo->ipo_dstid->ref_len);
		dstid = (struct sadb_ident *) headers[SADB_EXT_IDENTITY_DST];
		dstid->sadb_ident_len = (sizeof(struct sadb_ident) +
		    PADUP(ipo->ipo_dstid->ref_len)) / sizeof(u_int64_t);
		dstid->sadb_ident_type = ipo->ipo_dstid->ref_type;
		bcopy(ipo->ipo_dstid + 1, headers[SADB_EXT_IDENTITY_DST] +
		    sizeof(struct sadb_ident), ipo->ipo_dstid->ref_len);
	}

	if (ipo->ipo_local_cred) {
		headers[SADB_X_EXT_LOCAL_CREDENTIALS] = p;
		p += sizeof(struct sadb_x_cred) + PADUP(ipo->ipo_local_cred->ref_len);
		lcred = (struct sadb_x_cred *) headers[SADB_X_EXT_LOCAL_CREDENTIALS];
		lcred->sadb_x_cred_len = (sizeof(struct sadb_x_cred) +
		    PADUP(ipo->ipo_local_cred->ref_len)) / sizeof(u_int64_t);
		switch (ipo->ipo_local_cred->ref_type) {
		case IPSP_CRED_KEYNOTE:
			lcred->sadb_x_cred_type = SADB_X_CREDTYPE_KEYNOTE;
			break;
		case IPSP_CRED_X509:
			lcred->sadb_x_cred_type = SADB_X_CREDTYPE_X509;
			break;
		}
		bcopy(ipo->ipo_local_cred + 1, headers[SADB_X_EXT_LOCAL_CREDENTIALS] +
		    sizeof(struct sadb_x_cred), ipo->ipo_local_cred->ref_len);
	}

	if (ipo->ipo_local_auth) {
		headers[SADB_X_EXT_LOCAL_AUTH] = p;
		p += sizeof(struct sadb_x_cred) + PADUP(ipo->ipo_local_auth->ref_len);
		lauth = (struct sadb_x_cred *) headers[SADB_X_EXT_LOCAL_AUTH];
		lauth->sadb_x_cred_len = (sizeof(struct sadb_x_cred) +
		    PADUP(ipo->ipo_local_auth->ref_len)) / sizeof(u_int64_t);
		switch (ipo->ipo_local_auth->ref_type) {
		case IPSP_AUTH_PASSPHRASE:
			lauth->sadb_x_cred_type = SADB_X_AUTHTYPE_PASSPHRASE;
			break;
		case IPSP_AUTH_RSA:
			lauth->sadb_x_cred_type = SADB_X_AUTHTYPE_RSA;
			break;
		}

		bcopy(ipo->ipo_local_auth + 1, headers[SADB_X_EXT_LOCAL_AUTH] +
		    sizeof(struct sadb_x_cred), ipo->ipo_local_auth->ref_len);
	}

	headers[SADB_EXT_PROPOSAL] = p;
	p += sizeof(struct sadb_prop);
	sa_prop = (struct sadb_prop *) headers[SADB_EXT_PROPOSAL];
	sa_prop->sadb_prop_num = 1; /* XXX One proposal only */
	sa_prop->sadb_prop_len = (sizeof(struct sadb_prop) +
	    (sizeof(struct sadb_comb) * sa_prop->sadb_prop_num)) /
	    sizeof(uint64_t);

	sadb_comb = p;

	/* XXX Should actually ask the crypto layer what's supported */
	for (j = 0; j < sa_prop->sadb_prop_num; j++) {
		sadb_comb->sadb_comb_flags = 0;

		if (ipsec_require_pfs)
			sadb_comb->sadb_comb_flags |= SADB_SAFLAGS_PFS;

		/* Set the encryption algorithm */
		if (ipo->ipo_sproto == IPPROTO_ESP) {
			if (!strncasecmp(ipsec_def_enc, "aes",
			    sizeof("aes"))) {
				sadb_comb->sadb_comb_encrypt = SADB_X_EALG_AES;
				sadb_comb->sadb_comb_encrypt_minbits = 128;
				sadb_comb->sadb_comb_encrypt_maxbits = 256;
			} else if (!strncasecmp(ipsec_def_enc, "aesctr",
			    sizeof("aesctr"))) {
				sadb_comb->sadb_comb_encrypt = SADB_X_EALG_AESCTR;
				sadb_comb->sadb_comb_encrypt_minbits = 128+32;
				sadb_comb->sadb_comb_encrypt_maxbits = 256+32;
			} else if (!strncasecmp(ipsec_def_enc, "3des",
			    sizeof("3des"))) {
				sadb_comb->sadb_comb_encrypt = SADB_EALG_3DESCBC;
				sadb_comb->sadb_comb_encrypt_minbits = 192;
				sadb_comb->sadb_comb_encrypt_maxbits = 192;
			} else if (!strncasecmp(ipsec_def_enc, "des",
			    sizeof("des"))) {
				sadb_comb->sadb_comb_encrypt = SADB_EALG_DESCBC;
				sadb_comb->sadb_comb_encrypt_minbits = 64;
				sadb_comb->sadb_comb_encrypt_maxbits = 64;
			} else if (!strncasecmp(ipsec_def_enc, "blowfish",
			    sizeof("blowfish"))) {
				sadb_comb->sadb_comb_encrypt = SADB_X_EALG_BLF;
				sadb_comb->sadb_comb_encrypt_minbits = 40;
				sadb_comb->sadb_comb_encrypt_maxbits = BLF_MAXKEYLEN * 8;
			} else if (!strncasecmp(ipsec_def_enc, "cast128",
			    sizeof("cast128"))) {
				sadb_comb->sadb_comb_encrypt = SADB_X_EALG_CAST;
				sadb_comb->sadb_comb_encrypt_minbits = 40;
				sadb_comb->sadb_comb_encrypt_maxbits = 128;
			}
		} else if (ipo->ipo_sproto == IPPROTO_IPCOMP) {
			/* Set the compression algorithm */
			if (!strncasecmp(ipsec_def_comp, "deflate",
			    sizeof("deflate"))) {
				sadb_comb->sadb_comb_encrypt = SADB_X_CALG_DEFLATE;
				sadb_comb->sadb_comb_encrypt_minbits = 0;
				sadb_comb->sadb_comb_encrypt_maxbits = 0;
			} else if (!strncasecmp(ipsec_def_comp, "lzs",
			    sizeof("lzs"))) {
				sadb_comb->sadb_comb_encrypt = SADB_X_CALG_LZS;
				sadb_comb->sadb_comb_encrypt_minbits = 0;
				sadb_comb->sadb_comb_encrypt_maxbits = 0;
			}
		}

		/* Set the authentication algorithm */
		if (!strncasecmp(ipsec_def_auth, "hmac-sha1",
		    sizeof("hmac-sha1"))) {
			sadb_comb->sadb_comb_auth = SADB_AALG_SHA1HMAC;
			sadb_comb->sadb_comb_auth_minbits = 160;
			sadb_comb->sadb_comb_auth_maxbits = 160;
		} else if (!strncasecmp(ipsec_def_auth, "hmac-ripemd160",
		    sizeof("hmac_ripemd160"))) {
			sadb_comb->sadb_comb_auth = SADB_X_AALG_RIPEMD160HMAC;
			sadb_comb->sadb_comb_auth_minbits = 160;
			sadb_comb->sadb_comb_auth_maxbits = 160;
		} else if (!strncasecmp(ipsec_def_auth, "hmac-md5",
		    sizeof("hmac-md5"))) {
			sadb_comb->sadb_comb_auth = SADB_AALG_MD5HMAC;
			sadb_comb->sadb_comb_auth_minbits = 128;
			sadb_comb->sadb_comb_auth_maxbits = 128;
		} else if (!strncasecmp(ipsec_def_auth, "hmac-sha2-256",
		    sizeof("hmac-sha2-256"))) {
			sadb_comb->sadb_comb_auth = SADB_X_AALG_SHA2_256;
			sadb_comb->sadb_comb_auth_minbits = 256;
			sadb_comb->sadb_comb_auth_maxbits = 256;
		} else if (!strncasecmp(ipsec_def_auth, "hmac-sha2-384",
		    sizeof("hmac-sha2-384"))) {
			sadb_comb->sadb_comb_auth = SADB_X_AALG_SHA2_384;
			sadb_comb->sadb_comb_auth_minbits = 384;
			sadb_comb->sadb_comb_auth_maxbits = 384;
		} else if (!strncasecmp(ipsec_def_auth, "hmac-sha2-512",
		    sizeof("hmac-sha2-512"))) {
			sadb_comb->sadb_comb_auth = SADB_X_AALG_SHA2_512;
			sadb_comb->sadb_comb_auth_minbits = 512;
			sadb_comb->sadb_comb_auth_maxbits = 512;
		}

		sadb_comb->sadb_comb_soft_allocations = ipsec_soft_allocations;
		sadb_comb->sadb_comb_hard_allocations = ipsec_exp_allocations;

		sadb_comb->sadb_comb_soft_bytes = ipsec_soft_bytes;
		sadb_comb->sadb_comb_hard_bytes = ipsec_exp_bytes;

		sadb_comb->sadb_comb_soft_addtime = ipsec_soft_timeout;
		sadb_comb->sadb_comb_hard_addtime = ipsec_exp_timeout;

		sadb_comb->sadb_comb_soft_usetime = ipsec_soft_first_use;
		sadb_comb->sadb_comb_hard_usetime = ipsec_exp_first_use;
		sadb_comb++;
	}

	/* Send the ACQUIRE message to all compliant registered listeners. */
	if ((rval = pfkeyv2_sendmessage(headers,
	    PFKEYV2_SENDMESSAGE_REGISTERED, NULL, smsg->sadb_msg_satype, 0,
	    ipo->ipo_rdomain)) != 0)
		goto ret;

	rval = 0;
ret:
	if (buffer != NULL) {
		bzero(buffer, i);
		free(buffer, M_PFKEY, 0);
	}

	return (rval);
}

/*
 * Notify key management that an expiration went off. The second argument
 * specifies the type of expiration (soft or hard).
 */
int
pfkeyv2_expire(struct tdb *sa, u_int16_t type)
{
	void *p, *headers[SADB_EXT_MAX+1], *buffer = NULL;
	struct sadb_msg *smsg;
	int rval = 0;
	int i;

	switch (sa->tdb_sproto) {
	case IPPROTO_AH:
	case IPPROTO_ESP:
	case IPPROTO_IPIP:
	case IPPROTO_IPCOMP:
#ifdef TCP_SIGNATURE
	case IPPROTO_TCP:
#endif /* TCP_SIGNATURE */
		break;

	default:
		rval = EOPNOTSUPP;
		goto ret;
	}

	i = sizeof(struct sadb_msg) + sizeof(struct sadb_sa) +
	    2 * sizeof(struct sadb_lifetime) +
	    sizeof(struct sadb_address) + PADUP(SA_LEN(&sa->tdb_src.sa)) +
	    sizeof(struct sadb_address) + PADUP(SA_LEN(&sa->tdb_dst.sa));

	if (!(p = malloc(i, M_PFKEY, M_NOWAIT | M_ZERO))) {
		rval = ENOMEM;
		goto ret;
	}

	bzero(headers, sizeof(headers));

	buffer = p;

	headers[0] = p;
	p += sizeof(struct sadb_msg);

	smsg = (struct sadb_msg *) headers[0];
	smsg->sadb_msg_version = PF_KEY_V2;
	smsg->sadb_msg_type = SADB_EXPIRE;
	smsg->sadb_msg_satype = sa->tdb_satype;
	smsg->sadb_msg_len = i / sizeof(uint64_t);
	smsg->sadb_msg_seq = pfkeyv2_seq++;

	headers[SADB_EXT_SA] = p;
	export_sa(&p, sa);

	headers[SADB_EXT_LIFETIME_CURRENT] = p;
	export_lifetime(&p, sa, 2);

	headers[type] = p;
	export_lifetime(&p, sa, type == SADB_EXT_LIFETIME_SOFT ?
	    PFKEYV2_LIFETIME_SOFT : PFKEYV2_LIFETIME_HARD);

	headers[SADB_EXT_ADDRESS_SRC] = p;
	export_address(&p, (struct sockaddr *) &sa->tdb_src);

	headers[SADB_EXT_ADDRESS_DST] = p;
	export_address(&p, (struct sockaddr *) &sa->tdb_dst);

	if ((rval = pfkeyv2_sendmessage(headers, PFKEYV2_SENDMESSAGE_BROADCAST,
	    NULL, 0, 0, sa->tdb_rdomain)) != 0)
		goto ret;

	rval = 0;

 ret:
	if (buffer != NULL) {
		bzero(buffer, i);
		free(buffer, M_PFKEY, 0);
	}

	return (rval);
}

struct pfkeyv2_sysctl_walk {
	void		*w_where;
	size_t		 w_len;
	int		 w_op;
	u_int8_t	 w_satype;
};

int
pfkeyv2_sysctl_walker(struct tdb *sa, void *arg, int last)
{
	struct pfkeyv2_sysctl_walk *w = (struct pfkeyv2_sysctl_walk *)arg;
	void *buffer = NULL;
	int error = 0;
	int buflen, i;

	if (w->w_satype != SADB_SATYPE_UNSPEC &&
	    w->w_satype != sa->tdb_satype)
		return (0);

	if (w->w_where) {
		void *headers[SADB_EXT_MAX+1];
		struct sadb_msg msg;

		bzero(headers, sizeof(headers));
		if ((error = pfkeyv2_get(sa, headers, &buffer, &buflen)) != 0)
			goto done;
		if (w->w_len < sizeof(msg) + buflen) {
			error = ENOMEM;
			goto done;
		}
		/* prepend header */
		bzero(&msg, sizeof(msg));
		msg.sadb_msg_version = PF_KEY_V2;
		msg.sadb_msg_satype = sa->tdb_satype;
		msg.sadb_msg_type = SADB_DUMP;
		msg.sadb_msg_len = (sizeof(msg) + buflen) / sizeof(uint64_t);
		if ((error = copyout(&msg, w->w_where, sizeof(msg))) != 0)
			goto done;
		w->w_where += sizeof(msg);
		w->w_len -= sizeof(msg);
		/* set extension type */
		for (i = 1; i <= SADB_EXT_MAX; i++)
			if (headers[i])
				((struct sadb_ext *)
				    headers[i])->sadb_ext_type = i;
		if ((error = copyout(buffer, w->w_where, buflen)) != 0)
			goto done;
		w->w_where += buflen;
		w->w_len -= buflen;
	} else {
		if ((error = pfkeyv2_get(sa, NULL, NULL, &buflen)) != 0)
			return (error);
		w->w_len += buflen;
		w->w_len += sizeof(struct sadb_msg);
	}

done:
	if (buffer)
		free(buffer, M_PFKEY, 0);
	return (error);
}

int
pfkeyv2_dump_policy(struct ipsec_policy *ipo, void **headers, void **buffer,
    int *lenp)
{
	struct sadb_ident *ident;
	int i, rval, perm;
	void *p;

	/* Find how much space we need. */
	i = 2 * sizeof(struct sadb_protocol);

	/* We'll need four of them: src, src mask, dst, dst mask. */
	switch (ipo->ipo_addr.sen_type) {
	case SENT_IP4:
		i += 4 * PADUP(sizeof(struct sockaddr_in));
		i += 4 * sizeof(struct sadb_address);
		break;
#ifdef INET6
	case SENT_IP6:
		i += 4 * PADUP(sizeof(struct sockaddr_in6));
		i += 4 * sizeof(struct sadb_address);
		break;
#endif /* INET6 */
	default:
		return (EINVAL);
	}

	/* Local address, might be zeroed. */
	switch (ipo->ipo_src.sa.sa_family) {
	case 0:
		break;
	case AF_INET:
		i += PADUP(sizeof(struct sockaddr_in));
		i += sizeof(struct sadb_address);
		break;
#ifdef INET6
	case AF_INET6:
		i += PADUP(sizeof(struct sockaddr_in6));
		i += sizeof(struct sadb_address);
		break;
#endif /* INET6 */
	default:
		return (EINVAL);
	}

	/* Remote address, might be zeroed. XXX ??? */
	switch (ipo->ipo_dst.sa.sa_family) {
	case 0:
		break;
	case AF_INET:
		i += PADUP(sizeof(struct sockaddr_in));
		i += sizeof(struct sadb_address);
		break;
#ifdef INET6
	case AF_INET6:
		i += PADUP(sizeof(struct sockaddr_in6));
		i += sizeof(struct sadb_address);
		break;
#endif /* INET6 */
	default:
		return (EINVAL);
	}

	if (ipo->ipo_srcid)
		i += sizeof(struct sadb_ident) + PADUP(ipo->ipo_srcid->ref_len);
	if (ipo->ipo_dstid)
		i += sizeof(struct sadb_ident) + PADUP(ipo->ipo_dstid->ref_len);

	if (lenp)
		*lenp = i;

	if (buffer == NULL) {
		rval = 0;
		goto ret;
	}

	if (!(p = malloc(i, M_PFKEY, M_NOWAIT | M_ZERO))) {
		rval = ENOMEM;
		goto ret;
	} else
		*buffer = p;

	/* Local address. */
	if (ipo->ipo_src.sa.sa_family) {
		headers[SADB_EXT_ADDRESS_SRC] = p;
		export_address(&p, (struct sockaddr *)&ipo->ipo_src);
	}
	
	/* Remote address. */
	if (ipo->ipo_dst.sa.sa_family) {
		headers[SADB_EXT_ADDRESS_DST] = p;
		export_address(&p, (struct sockaddr *)&ipo->ipo_dst);
	}

	/* Get actual flow. */
	export_flow(&p, ipo->ipo_type, &ipo->ipo_addr, &ipo->ipo_mask,
	    headers);

	/* Add ids only when we are root. */
	perm = suser(curproc, 0);
	if (perm == 0 && ipo->ipo_srcid) {
		headers[SADB_EXT_IDENTITY_SRC] = p;
		p += sizeof(struct sadb_ident) + PADUP(ipo->ipo_srcid->ref_len);
		ident = (struct sadb_ident *)headers[SADB_EXT_IDENTITY_SRC];
		ident->sadb_ident_len = (sizeof(struct sadb_ident) +
		    PADUP(ipo->ipo_srcid->ref_len)) / sizeof(uint64_t);
		ident->sadb_ident_type = ipo->ipo_srcid->ref_type;
		bcopy(ipo->ipo_srcid + 1, headers[SADB_EXT_IDENTITY_SRC] +
		    sizeof(struct sadb_ident), ipo->ipo_srcid->ref_len);
	}
	if (perm == 0 && ipo->ipo_dstid) {
		headers[SADB_EXT_IDENTITY_DST] = p;
		p += sizeof(struct sadb_ident) + PADUP(ipo->ipo_dstid->ref_len);
		ident = (struct sadb_ident *)headers[SADB_EXT_IDENTITY_DST];
		ident->sadb_ident_len = (sizeof(struct sadb_ident) +
		    PADUP(ipo->ipo_dstid->ref_len)) / sizeof(uint64_t);
		ident->sadb_ident_type = ipo->ipo_dstid->ref_type;
		bcopy(ipo->ipo_dstid + 1, headers[SADB_EXT_IDENTITY_DST] +
		    sizeof(struct sadb_ident), ipo->ipo_dstid->ref_len);
	}

	rval = 0;
ret:
	return (rval);
}

/*
 * Caller is responsible for setting at least splsoftnet().
 */
int
pfkeyv2_ipo_walk(u_int rdomain, int (*walker)(struct ipsec_policy *, void *),
    void *arg)
{
	int rval = 0;
	struct ipsec_policy *ipo;

	TAILQ_FOREACH(ipo, &ipsec_policy_head, ipo_list) {
		if (ipo->ipo_rdomain != rdomain)
			continue;
		rval = walker(ipo, (void *)arg);
	}
	return (rval);
}

int
pfkeyv2_sysctl_policydumper(struct ipsec_policy *ipo, void *arg)
{
	struct pfkeyv2_sysctl_walk *w = (struct pfkeyv2_sysctl_walk *)arg;
	void *buffer = 0;
	int i, buflen, error = 0;

	/* Do not dump policies attached to a socket. */
	if (ipo->ipo_flags & IPSP_POLICY_SOCKET)
		return (0);

	if (w->w_where) {
		void *headers[SADB_EXT_MAX + 1];
		struct sadb_msg msg;

		bzero(headers, sizeof(headers));
		if ((error = pfkeyv2_dump_policy(ipo, headers, &buffer,
		    &buflen)) != 0)
			goto done;
		if (w->w_len < buflen) {
			error = ENOMEM;
			goto done;
		}
		/* prepend header */
		bzero(&msg, sizeof(msg));
		msg.sadb_msg_version = PF_KEY_V2;
		if (ipo->ipo_sproto == IPPROTO_ESP)
			msg.sadb_msg_satype = SADB_SATYPE_ESP;
		else if (ipo->ipo_sproto == IPPROTO_AH)
			msg.sadb_msg_satype = SADB_SATYPE_AH;
		else if (ipo->ipo_sproto == IPPROTO_IPCOMP)
			msg.sadb_msg_satype = SADB_X_SATYPE_IPCOMP;
		else if (ipo->ipo_sproto == IPPROTO_IPIP)
			msg.sadb_msg_satype = SADB_X_SATYPE_IPIP;
		msg.sadb_msg_type = SADB_X_SPDDUMP;
		msg.sadb_msg_len = (sizeof(msg) + buflen) / sizeof(uint64_t);
		if ((error = copyout(&msg, w->w_where, sizeof(msg))) != 0)
			goto done;
		w->w_where += sizeof(msg);
		w->w_len -= sizeof(msg);
		/* set extension type */
		for (i = 1; i < SADB_EXT_MAX; i++)
			if (headers[i])
				((struct sadb_ext *)
				    headers[i])->sadb_ext_type = i;
		if ((error = copyout(buffer, w->w_where, buflen)) != 0)
			goto done;
		w->w_where += buflen;
		w->w_len -= buflen;
	} else {
		if ((error = pfkeyv2_dump_policy(ipo, NULL, NULL,
		    &buflen)) != 0)
			goto done;
		w->w_len += buflen;
		w->w_len += sizeof(struct sadb_msg);
	}

done:
	if (buffer)
		free(buffer, M_PFKEY, 0);
	return (error);
}

int
pfkeyv2_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *new, size_t newlen)
{
	struct pfkeyv2_sysctl_walk w;
	int s, error = EINVAL;
	u_int rdomain;

	if (new)
		return (EPERM);
	if (namelen < 1)
		return (EINVAL);
	w.w_op = name[0];
	w.w_satype = name[1];
	w.w_where = oldp;
	w.w_len = oldp ? *oldlenp : 0;

	rdomain = rtable_l2(curproc->p_p->ps_rtableid);

	switch(w.w_op) {
	case NET_KEY_SADB_DUMP:
		if ((error = suser(curproc, 0)) != 0)
			return (error);
		s = splsoftnet();
		error = tdb_walk(rdomain, pfkeyv2_sysctl_walker, &w);
		splx(s);
		if (oldp)
			*oldlenp = w.w_where - oldp;
		else
			*oldlenp = w.w_len;
		break;

	case NET_KEY_SPD_DUMP:
		s = splsoftnet();
		error = pfkeyv2_ipo_walk(rdomain,
		    pfkeyv2_sysctl_policydumper, &w);
		splx(s);
		if (oldp)
			*oldlenp = w.w_where - oldp;
		else
			*oldlenp = w.w_len;
		break;
	}

	return (error);
}

int
pfkeyv2_init(void)
{
	int rval;

	bzero(&pfkeyv2_version, sizeof(struct pfkey_version));
	pfkeyv2_version.protocol = PFKEYV2_PROTOCOL;
	pfkeyv2_version.create = &pfkeyv2_create;
	pfkeyv2_version.release = &pfkeyv2_release;
	pfkeyv2_version.send = &pfkeyv2_send;
	pfkeyv2_version.sysctl = &pfkeyv2_sysctl;

	rval = pfkey_register(&pfkeyv2_version);
	return (rval);
}

int
pfkeyv2_cleanup(void)
{
	pfkey_unregister(&pfkeyv2_version);
	return (0);
}
