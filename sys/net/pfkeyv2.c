/* $OpenBSD: pfkeyv2.c,v 1.176 2018/02/19 08:59:52 mpi Exp $ */

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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 *	This product includes software developed at the Information
 *	Technology Division, US Naval Research Laboratory.
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

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/pool.h>
#include <sys/mutex.h>

#include <net/route.h>
#include <netinet/ip_ipsp.h>
#include <net/pfkeyv2.h>
#include <net/radix.h>
#include <net/raw_cb.h>
#include <netinet/ip_ah.h>
#include <netinet/ip_esp.h>
#include <netinet/ip_ipcomp.h>
#include <crypto/blf.h>

#if NPF > 0
#include <net/if.h>
#include <net/pfvar.h>
#endif


static const struct sadb_alg ealgs[] = {
	{ SADB_EALG_NULL, 0, 0, 0 },
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

extern struct radix_node_head **spd_tables;

#define PFKEY_MSG_MAXSZ 4096
struct sockaddr pfkey_addr = { 2, PF_KEY, };
struct domain pfkeydomain;

struct keycb {
	struct rawcb			rcb;
	LIST_ENTRY(keycb)	kcb_list;
	int flags;
	uint32_t pid;
	uint32_t registration;    /* Increase size if SATYPE_MAX > 31 */
	uint rdomain;
};
#define sotokeycb(so) ((struct keycb *)(so)->so_pcb)


struct dump_state {
	struct sadb_msg *sadb_msg;
	struct socket *socket;
};

/* Static globals */
static LIST_HEAD(, keycb) pfkeyv2_sockets = LIST_HEAD_INITIALIZER(keycb);

struct mutex pfkeyv2_mtx = MUTEX_INITIALIZER(IPL_NONE);
static uint32_t pfkeyv2_seq = 1;
static int nregistered = 0;
static int npromisc = 0;

void pfkey_init(void);

int pfkeyv2_attach(struct socket *, int);
int pfkeyv2_detach(struct socket *);
int pfkeyv2_usrreq(struct socket *, int, struct mbuf *, struct mbuf *,
    struct mbuf *, struct proc *);
int pfkeyv2_output(struct mbuf *, struct socket *, struct sockaddr *,
    struct mbuf *);
int pfkey_sendup(struct keycb *, struct mbuf *, int);
int pfkeyv2_sa_flush(struct tdb *, void *, int);
int pfkeyv2_policy_flush(struct ipsec_policy *, void *, unsigned int);
int pfkeyv2_sysctl_policydumper(struct ipsec_policy *, void *, unsigned int);

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

static struct protosw pfkeysw[] = {
{
  .pr_type      = SOCK_RAW,
  .pr_domain    = &pfkeydomain,
  .pr_protocol  = PF_KEY_V2,
  .pr_flags     = PR_ATOMIC | PR_ADDR,
  .pr_output    = pfkeyv2_output,
  .pr_usrreq    = pfkeyv2_usrreq,
  .pr_attach    = pfkeyv2_attach,
  .pr_detach    = pfkeyv2_detach,
  .pr_sysctl    = pfkeyv2_sysctl,
}
};

struct domain pfkeydomain = {
  .dom_family = PF_KEY,
  .dom_name = "PF_KEY",
  .dom_init = pfkey_init,
  .dom_protosw = pfkeysw,
  .dom_protoswNPROTOSW = &pfkeysw[nitems(pfkeysw)],
};

void
pfkey_init(void)
{
	rn_init(sizeof(struct sockaddr_encap));
}


/*
 * Attach a new PF_KEYv2 socket.
 */
int
pfkeyv2_attach(struct socket *so, int proto)
{
	struct rawcb *rp;
	struct keycb *kp;
	int error;

	if ((so->so_state & SS_PRIV) == 0)
		return EACCES;

	kp = malloc(sizeof(struct keycb), M_PCB, M_WAITOK | M_ZERO);
	rp = &kp->rcb;
	so->so_pcb = rp;

	error = soreserve(so, RAWSNDQ, RAWRCVQ);

	if (error) {
		free(kp, M_PCB, sizeof(struct keycb));
		return (error);
	}

	rp->rcb_socket = so;
	rp->rcb_proto.sp_family = so->so_proto->pr_domain->dom_family;
	rp->rcb_proto.sp_protocol = proto;

	so->so_options |= SO_USELOOPBACK;
	soisconnected(so);

	rp->rcb_faddr = &pfkey_addr;
	kp->pid = curproc->p_p->ps_pid;

	/*
	 * XXX we should get this from the socket instead but
	 * XXX rawcb doesn't store the rdomain like inpcb does.
	 */
	kp->rdomain = rtable_l2(curproc->p_p->ps_rtableid);

	LIST_INSERT_HEAD(&pfkeyv2_sockets, kp, kcb_list);

	return (0);
}

/*
 * Close a PF_KEYv2 socket.
 */
int
pfkeyv2_detach(struct socket *so)
{
	struct keycb *kp;

	kp = sotokeycb(so);
	if (kp == NULL)
		return ENOTCONN;

	LIST_REMOVE(kp, kcb_list);

	if (kp->flags &
	    (PFKEYV2_SOCKETFLAGS_REGISTERED|PFKEYV2_SOCKETFLAGS_PROMISC)) {
		mtx_enter(&pfkeyv2_mtx);
		if (kp->flags & PFKEYV2_SOCKETFLAGS_REGISTERED)
			nregistered--;

		if (kp->flags & PFKEYV2_SOCKETFLAGS_PROMISC)
			npromisc--;
		mtx_leave(&pfkeyv2_mtx);
	}

	so->so_pcb = NULL;
	sofree(so);
	free(kp, M_PCB, sizeof(struct keycb));
	return (0);
}

int
pfkeyv2_usrreq(struct socket *so, int req, struct mbuf *mbuf,
    struct mbuf *nam, struct mbuf *control, struct proc *p)
{
	return (raw_usrreq(so, req, mbuf, nam, control, p));
}

int
pfkeyv2_output(struct mbuf *mbuf, struct socket *so,
    struct sockaddr *dstaddr, struct mbuf *control)
{
	void *message;
	int error = 0;

#ifdef DIAGNOSTIC
	if (!mbuf || !(mbuf->m_flags & M_PKTHDR)) {
		error = EINVAL;
		goto ret;
	}
#endif /* DIAGNOSTIC */

	if (mbuf->m_pkthdr.len > PFKEY_MSG_MAXSZ) {
		error = EMSGSIZE;
		goto ret;
	}

	if (!(message = malloc((unsigned long) mbuf->m_pkthdr.len,
	    M_PFKEY, M_DONTWAIT))) {
		error = ENOMEM;
		goto ret;
	}

	m_copydata(mbuf, 0, mbuf->m_pkthdr.len, message);

	error = pfkeyv2_send(so, message, mbuf->m_pkthdr.len);

ret:
	m_freem(mbuf);
	return (error);
}

int
pfkey_sendup(struct keycb *kp, struct mbuf *packet, int more)
{
	struct socket *so = kp->rcb.rcb_socket;
	struct mbuf *packet2;

	NET_ASSERT_LOCKED();

	if (more) {
		if (!(packet2 = m_dup_pkt(packet, 0, M_DONTWAIT)))
			return (ENOMEM);
	} else
		packet2 = packet;

	if (!sbappendaddr(so, &so->so_rcv, &pfkey_addr, packet2, NULL)) {
		m_freem(packet2);
		return (ENOBUFS);
	}

	sorwakeup(so);
	return (0);
}

/*
 * Send a PFKEYv2 message, possibly to many receivers, based on the
 * satype of the socket (which is set by the REGISTER message), and the
 * third argument.
 */
int
pfkeyv2_sendmessage(void **headers, int mode, struct socket *so,
    u_int8_t satype, int count, u_int rdomain)
{
	int i, j, rval;
	void *p, *buffer = NULL;
	struct mbuf *packet;
	struct keycb *s;
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
		pfkey_sendup(sotokeycb(so), packet, 0);

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
		KERNEL_LOCK();
		LIST_FOREACH(s, &pfkeyv2_sockets, kcb_list) {
			if ((s->flags & PFKEYV2_SOCKETFLAGS_PROMISC) &&
			    (s->rcb.rcb_socket != so) &&
			    (s->rdomain == rdomain))
				pfkey_sendup(s, packet, 1);
		}
		KERNEL_UNLOCK();
		m_freem(packet);
		break;

	case PFKEYV2_SENDMESSAGE_REGISTERED:
		/*
		 * Send the message to all registered sockets that match
		 * the specified satype (e.g., all IPSEC-ESP negotiators)
		 */
		KERNEL_LOCK();
		LIST_FOREACH(s, &pfkeyv2_sockets, kcb_list) {
			if ((s->flags & PFKEYV2_SOCKETFLAGS_REGISTERED) &&
			    (s->rdomain == rdomain)) {
				if (!satype)    /* Just send to everyone registered */
					pfkey_sendup(s, packet, 1);
				else {
					/* Check for specified satype */
					if ((1 << satype) & s->registration)
						pfkey_sendup(s, packet, 1);
				}
			}
		}
		KERNEL_UNLOCK();
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
		KERNEL_LOCK();
		LIST_FOREACH(s, &pfkeyv2_sockets, kcb_list) {
			if ((s->flags & PFKEYV2_SOCKETFLAGS_PROMISC) &&
			    !(s->flags & PFKEYV2_SOCKETFLAGS_REGISTERED) &&
			    (s->rdomain == rdomain))
				pfkey_sendup(s, packet, 1);
		}
		KERNEL_UNLOCK();
		m_freem(packet);
		break;

	case PFKEYV2_SENDMESSAGE_BROADCAST:
		/* Send message to all sockets */
		KERNEL_LOCK();
		LIST_FOREACH(s, &pfkeyv2_sockets, kcb_list) {
			if (s->rdomain == rdomain)
				pfkey_sendup(s, packet, 1);
		}
		KERNEL_UNLOCK();
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
	export_address(&p, &sunion.sa);

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
	export_address(&p, &sunion.sa);

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
	export_address(&p, &sunion.sa);

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
	export_address(&p, &sunion.sa);

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

	i += sizeof(struct sadb_address) + PADUP(sa->tdb_src.sa.sa_len);
	i += sizeof(struct sadb_address) + PADUP(sa->tdb_dst.sa.sa_len);

	if (sa->tdb_ids) {
		i += sizeof(struct sadb_ident) + PADUP(sa->tdb_ids->id_local->len);
		i += sizeof(struct sadb_ident) + PADUP(sa->tdb_ids->id_remote->len);
	}

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

	if (sa->tdb_onext) {
		i += sizeof(struct sadb_sa);
		i += sizeof(struct sadb_address) +
		    PADUP(sa->tdb_onext->tdb_dst.sa.sa_len);
		i += sizeof(struct sadb_protocol);
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
	export_address(&p, &sa->tdb_src.sa);

	/* Export TDB destination address */
	headers[SADB_EXT_ADDRESS_DST] = p;
	export_address(&p, &sa->tdb_dst.sa);

	/* Export source/destination identities, if present */
	if (sa->tdb_ids)
		export_identities(&p, sa->tdb_ids, sa->tdb_ids_swapped, headers);

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

	if (sa->tdb_onext) {
		headers[SADB_X_EXT_SA2] = p;
		export_sa(&p, sa->tdb_onext);
		headers[SADB_X_EXT_DST2] = p;
		export_address(&p, &sa->tdb_onext->tdb_dst.sa);
		headers[SADB_X_EXT_SATYPE2] = p;
		export_satype(&p, sa->tdb_onext);
	}

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
pfkeyv2_sa_flush(struct tdb *sa, void *satype_vp, int last)
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
pfkeyv2_send(struct socket *so, void *message, int len)
{
	int i, j, rval = 0, mode = PFKEYV2_SENDMESSAGE_BROADCAST;
	int delflag = 0;
	struct sockaddr_encap encapdst, encapnetmask;
	struct ipsec_policy *ipo;
	struct ipsec_acquire *ipa;
	struct radix_node_head *rnh;
	struct radix_node *rn = NULL;
	struct keycb *kp, *bkp;
	void *freeme = NULL, *bckptr = NULL;
	void *headers[SADB_EXT_MAX + 1];
	union sockaddr_union *sunionp;
	struct tdb *sa1 = NULL, *sa2 = NULL;
	struct sadb_msg *smsg;
	struct sadb_spirange *sprng;
	struct sadb_sa *ssa;
	struct sadb_supported *ssup;
	struct sadb_ident *sid, *did;
	u_int rdomain;
	int promisc;

	mtx_enter(&pfkeyv2_mtx);
	promisc = npromisc;
	mtx_leave(&pfkeyv2_mtx);

	NET_LOCK();

	/* Verify that we received this over a legitimate pfkeyv2 socket */
	bzero(headers, sizeof(headers));

	kp = sotokeycb(so);
	if (!kp) {
		rval = EINVAL;
		goto ret;
	}

	rdomain = kp->rdomain;

	/* If we have any promiscuous listeners, send them a copy of the message */
	if (promisc) {
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
		KERNEL_LOCK();
		LIST_FOREACH(bkp, &pfkeyv2_sockets, kcb_list) {
			if ((bkp->flags & PFKEYV2_SOCKETFLAGS_PROMISC) &&
			    (bkp->rdomain == rdomain))
				pfkey_sendup(bkp, packet, 1);
		}
		KERNEL_UNLOCK();

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

		import_address(&sa1->tdb_src.sa, headers[SADB_EXT_ADDRESS_SRC]);
		import_address(&sa1->tdb_dst.sa, headers[SADB_EXT_ADDRESS_DST]);

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

		/* Find TDB */
		sa2 = gettdb(rdomain, ssa->sadb_sa_spi, sunionp,
		    SADB_X_GETSPROTO(smsg->sadb_msg_satype));

		/* If there's no such SA, we're done */
		if (sa2 == NULL) {
			rval = ESRCH;
			goto ret;
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
				goto ret;
			}

			/* Initialize SA */
			import_sa(newsa, headers[SADB_EXT_SA], &ii);
			import_address(&newsa->tdb_src.sa,
			    headers[SADB_EXT_ADDRESS_SRC]);
			import_address(&newsa->tdb_dst.sa,
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
			newsa->tdb_ids_swapped = 1; /* only on TDB_UPDATE */
			import_identities(&newsa->tdb_ids,
			    newsa->tdb_ids_swapped,
			    headers[SADB_EXT_IDENTITY_SRC],
			    headers[SADB_EXT_IDENTITY_DST]);
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
				goto ret;
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
				goto ret;
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
			if (headers[SADB_EXT_ADDRESS_SRC] ||
			    headers[SADB_EXT_ADDRESS_PROXY]) {
				tdb_unlink(sa2);
				import_address((struct sockaddr *)&sa2->tdb_src,
				    headers[SADB_EXT_ADDRESS_SRC]);
				import_address((struct sockaddr *)&sa2->tdb_dst,
				    headers[SADB_EXT_ADDRESS_PROXY]);
				puttdb(sa2);
			}
		}

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

		sa2 = gettdb(rdomain, ssa->sadb_sa_spi, sunionp,
		    SADB_X_GETSPROTO(smsg->sadb_msg_satype));

		/* We can't add an existing SA! */
		if (sa2 != NULL) {
			rval = EEXIST;
			goto ret;
		}

		/* We can only add "mature" SAs */
		if (ssa->sadb_sa_state != SADB_SASTATE_MATURE) {
			rval = EINVAL;
			goto ret;
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
				goto ret;
			}

			import_sa(newsa, headers[SADB_EXT_SA], &ii);
			import_address(&newsa->tdb_src.sa,
			    headers[SADB_EXT_ADDRESS_SRC]);
			import_address(&newsa->tdb_dst.sa,
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

			import_identities(&newsa->tdb_ids,
			    newsa->tdb_ids_swapped,
			    headers[SADB_EXT_IDENTITY_SRC],
			    headers[SADB_EXT_IDENTITY_DST]);

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
				goto ret;
			}
		}

		/* Add TDB in table */
		puttdb((struct tdb *) freeme);

		freeme = NULL;
		break;

	case SADB_DELETE:
		ssa = (struct sadb_sa *) headers[SADB_EXT_SA];
		sunionp =
		    (union sockaddr_union *)(headers[SADB_EXT_ADDRESS_DST] +
			sizeof(struct sadb_address));

		sa2 = gettdb(rdomain, ssa->sadb_sa_spi, sunionp,
		    SADB_X_GETSPROTO(smsg->sadb_msg_satype));
		if (sa2 == NULL) {
			rval = ESRCH;
			goto ret;
		}

		tdb_delete(sa2);

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

		sa2 = gettdb(rdomain, ssa->sadb_sa_spi, sunionp,
		    SADB_X_GETSPROTO(smsg->sadb_msg_satype));
		if (sa2 == NULL) {
			rval = ESRCH;
			goto ret;
		}

		rval = pfkeyv2_get(sa2, headers, &freeme, NULL);
		if (rval)
			mode = PFKEYV2_SENDMESSAGE_UNICAST;

		break;

	case SADB_REGISTER:
		if (!(kp->flags & PFKEYV2_SOCKETFLAGS_REGISTERED)) {
			kp->flags |= PFKEYV2_SOCKETFLAGS_REGISTERED;
			mtx_enter(&pfkeyv2_mtx);
			nregistered++;
			mtx_leave(&pfkeyv2_mtx);
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
		kp->registration |= (1 << ((struct sadb_msg *)message)->sadb_msg_satype);

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
			spd_table_walk(rdomain, pfkeyv2_policy_flush, NULL);
			/* FALLTHROUGH */
		case SADB_SATYPE_AH:
		case SADB_SATYPE_ESP:
		case SADB_X_SATYPE_IPIP:
		case SADB_X_SATYPE_IPCOMP:
#ifdef TCP_SIGNATURE
		case SADB_X_SATYPE_TCPSIGNATURE:
#endif /* TCP_SIGNATURE */
			tdb_walk(rdomain, pfkeyv2_sa_flush,
			    (u_int8_t *) &(smsg->sadb_msg_satype));

			break;

		default:
			rval = EINVAL; /* Unknown/unsupported type */
		}

		break;

	case SADB_DUMP:
	{
		struct dump_state dump_state;
		dump_state.sadb_msg = (struct sadb_msg *) headers[0];
		dump_state.socket = so;

		rval = tdb_walk(rdomain, pfkeyv2_dump_walker, &dump_state);
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

		tdb1 = gettdb(rdomain, ssa->sadb_sa_spi, sunionp,
		    SADB_X_GETSPROTO(smsg->sadb_msg_satype));
		if (tdb1 == NULL) {
			rval = ESRCH;
			goto ret;
		}

		ssa = (struct sadb_sa *) headers[SADB_X_EXT_SA2];
		sunionp = (union sockaddr_union *) (headers[SADB_X_EXT_DST2] +
		    sizeof(struct sadb_address));
		sa_proto = (struct sadb_protocol *) headers[SADB_X_EXT_SATYPE2];

		tdb2 = gettdb(rdomain, ssa->sadb_sa_spi, sunionp,
		    SADB_X_GETSPROTO(sa_proto->sadb_protocol_proto));
		if (tdb2 == NULL) {
			rval = ESRCH;
			goto ret;
		}

		/* Detect cycles */
		for (tdb3 = tdb2; tdb3; tdb3 = tdb3->tdb_onext)
			if (tdb3 == tdb1) {
				rval = ESRCH;
				goto ret;
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
	}
	break;

	case SADB_X_DELFLOW:
		delflag = 1;
		/*FALLTHROUGH*/
	case SADB_X_ADDFLOW:
	{
		struct sadb_protocol *sab;
		union sockaddr_union *ssrc;
		int exists = 0;

		if ((rnh = spd_table_add(rdomain)) == NULL) {
			rval = ENOMEM;
			goto ret;
		}

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
		if ((rn = rn_match(&encapdst, rnh)) != NULL) {
			ipo = (struct ipsec_policy *)rn;

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
				goto ret;
			}
		}

		/* Delete ? */
		if (delflag) {
			if (exists) {
				rval = ipsec_delete_policy(ipo);
				goto ret;
			}

			/* If we were asked to delete something non-existent, error. */
			rval = ESRCH;
			break;
		}

		if (!exists) {
			if (ipsec_policy_pool_initialized == 0) {
				ipsec_policy_pool_initialized = 1;
				pool_init(&ipsec_policy_pool,
				    sizeof(struct ipsec_policy), 0,
				    IPL_NONE, 0, "ipsec policy", NULL);
			}

			/* Allocate policy entry */
			ipo = pool_get(&ipsec_policy_pool, PR_NOWAIT|PR_ZERO);
			if (ipo == NULL) {
				rval = ENOMEM;
				goto ret;
			}
		}

		switch (sab->sadb_protocol_proto) {
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

		if (ipo->ipo_ids) {
			ipsp_ids_free(ipo->ipo_ids);
			ipo->ipo_ids = NULL;
		}

		if ((sid = headers[SADB_EXT_IDENTITY_SRC]) != NULL &&
		    (did = headers[SADB_EXT_IDENTITY_DST]) != NULL) {
			import_identities(&ipo->ipo_ids, 0, sid, did);
			if (ipo->ipo_ids == NULL) {
				if (exists)
					ipsec_delete_policy(ipo);
				else
					pool_put(&ipsec_policy_pool, ipo);
				rval = ENOBUFS;
				goto ret;
			}
		}

		/* Flow type */
		if (!exists) {
			/* Initialize policy entry */
			bcopy(&encapdst, &ipo->ipo_addr,
			    sizeof(struct sockaddr_encap));
			bcopy(&encapnetmask, &ipo->ipo_mask,
			    sizeof(struct sockaddr_encap));

			TAILQ_INIT(&ipo->ipo_acquires);
			ipo->ipo_rdomain = rdomain;
			ipo->ipo_ref_count = 1;

			/* Add SPD entry */
			if ((rnh = spd_table_get(rdomain)) == NULL ||
			    (rn = rn_addroute((caddr_t)&ipo->ipo_addr,
				(caddr_t)&ipo->ipo_mask, rnh,
				ipo->ipo_nodes, 0)) == NULL) {
				/* Remove from linked list of policies on TDB */
				if (ipo->ipo_tdb)
					TAILQ_REMOVE(&ipo->ipo_tdb->tdb_policy_head,
					    ipo, ipo_tdb_next);

				if (ipo->ipo_ids)
					ipsp_ids_free(ipo->ipo_ids);
				pool_put(&ipsec_policy_pool, ipo);

				goto ret;
			}
			TAILQ_INSERT_HEAD(&ipsec_policy_head, ipo, ipo_list);
			ipsec_in_use++;
			/*
			 * XXXSMP IPsec data structures are not ready to be
			 * accessed by multiple Network threads in parallel,
			 * so force all packets to be processed by the first
			 * one.
			 */
			extern int nettaskqs;
			nettaskqs = 1;
		} else {
			ipo->ipo_last_searched = ipo->ipo_flags = 0;
		}
	}
	break;

	case SADB_X_PROMISC:
		if (len >= 2 * sizeof(struct sadb_msg)) {
			struct mbuf *packet;

			if ((rval = pfdatatopacket(message, len, &packet)) != 0)
				goto ret;

			KERNEL_LOCK();
			LIST_FOREACH(bkp, &pfkeyv2_sockets, kcb_list) {
				if ((bkp != kp) &&
				    (bkp->rdomain == rdomain) &&
				    (!smsg->sadb_msg_seq ||
				    (smsg->sadb_msg_seq == kp->pid)))
					pfkey_sendup(bkp, packet, 1);
			}
			KERNEL_UNLOCK();

			m_freem(packet);
		} else {
			if (len != sizeof(struct sadb_msg)) {
				rval = EINVAL;
				goto ret;
			}

			i = (kp->flags &
			    PFKEYV2_SOCKETFLAGS_PROMISC) ? 1 : 0;
			j = smsg->sadb_msg_satype ? 1 : 0;

			if (i ^ j) {
				if (j) {
					kp->flags |=
					    PFKEYV2_SOCKETFLAGS_PROMISC;
					mtx_enter(&pfkeyv2_mtx);
					npromisc++;
					mtx_leave(&pfkeyv2_mtx);
				} else {
					kp->flags &=
					    ~PFKEYV2_SOCKETFLAGS_PROMISC;
					mtx_enter(&pfkeyv2_mtx);
					npromisc--;
					mtx_leave(&pfkeyv2_mtx);
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

	rval = pfkeyv2_sendmessage(headers, mode, so, 0, 0, rdomain);

realret:
	NET_UNLOCK();

	if (freeme)
		free(freeme, M_PFKEY, 0);

	explicit_bzero(message, len);
	free(message, M_PFKEY, 0);

	if (sa1)
		free(sa1, M_PFKEY, 0);

	return (rval);
}

/*
 * Send an ACQUIRE message to key management, to get a new SA.
 */
int
pfkeyv2_acquire(struct ipsec_policy *ipo, union sockaddr_union *gw,
    union sockaddr_union *laddr, u_int32_t *seq, struct sockaddr_encap *ddst)
{
	void *p, *headers[SADB_EXT_MAX + 1], *buffer = NULL;
	struct sadb_comb *sadb_comb;
	struct sadb_address *sadd;
	struct sadb_prop *sa_prop;
	struct sadb_msg *smsg;
	int rval = 0;
	int i, j, registered;

	mtx_enter(&pfkeyv2_mtx);
	*seq = pfkeyv2_seq++;

	registered = nregistered;
	mtx_leave(&pfkeyv2_mtx);

	if (!registered) {
		rval = ESRCH;
		goto ret;
	}

	/* How large a buffer do we need... XXX we only do one proposal for now */
	i = sizeof(struct sadb_msg) +
	    (laddr == NULL ? 0 : sizeof(struct sadb_address) +
		PADUP(ipo->ipo_src.sa.sa_len)) +
	    sizeof(struct sadb_address) + PADUP(gw->sa.sa_len) +
	    sizeof(struct sadb_prop) + 1 * sizeof(struct sadb_comb);

	if (ipo->ipo_ids) {
		i += sizeof(struct sadb_ident) + PADUP(ipo->ipo_ids->id_local->len);
		i += sizeof(struct sadb_ident) + PADUP(ipo->ipo_ids->id_remote->len);
	}

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
		p += sizeof(struct sadb_address) + PADUP(laddr->sa.sa_len);
		sadd = (struct sadb_address *) headers[SADB_EXT_ADDRESS_SRC];
		sadd->sadb_address_len = (sizeof(struct sadb_address) +
		    laddr->sa.sa_len + sizeof(uint64_t) - 1) /
		    sizeof(uint64_t);
		bcopy(laddr, headers[SADB_EXT_ADDRESS_SRC] +
		    sizeof(struct sadb_address), laddr->sa.sa_len);
	}

	headers[SADB_EXT_ADDRESS_DST] = p;
	p += sizeof(struct sadb_address) + PADUP(gw->sa.sa_len);
	sadd = (struct sadb_address *) headers[SADB_EXT_ADDRESS_DST];
	sadd->sadb_address_len = (sizeof(struct sadb_address) +
	    gw->sa.sa_len + sizeof(uint64_t) - 1) / sizeof(uint64_t);
	bcopy(gw, headers[SADB_EXT_ADDRESS_DST] + sizeof(struct sadb_address),
	    gw->sa.sa_len);

	if (ipo->ipo_ids)
		export_identities(&p, ipo->ipo_ids, 0, headers);

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
	    sizeof(struct sadb_address) + PADUP(sa->tdb_src.sa.sa_len) +
	    sizeof(struct sadb_address) + PADUP(sa->tdb_dst.sa.sa_len);

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

	mtx_enter(&pfkeyv2_mtx);
	smsg->sadb_msg_seq = pfkeyv2_seq++;
	mtx_leave(&pfkeyv2_mtx);

	headers[SADB_EXT_SA] = p;
	export_sa(&p, sa);

	headers[SADB_EXT_LIFETIME_CURRENT] = p;
	export_lifetime(&p, sa, 2);

	headers[type] = p;
	export_lifetime(&p, sa, type == SADB_EXT_LIFETIME_SOFT ?
	    PFKEYV2_LIFETIME_SOFT : PFKEYV2_LIFETIME_HARD);

	headers[SADB_EXT_ADDRESS_SRC] = p;
	export_address(&p, &sa->tdb_src.sa);

	headers[SADB_EXT_ADDRESS_DST] = p;
	export_address(&p, &sa->tdb_dst.sa);

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

	if (ipo->ipo_ids) {
		i += sizeof(struct sadb_ident) + PADUP(ipo->ipo_ids->id_local->len);
		i += sizeof(struct sadb_ident) + PADUP(ipo->ipo_ids->id_remote->len);
	}

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
		export_address(&p, &ipo->ipo_src.sa);
	}

	/* Remote address. */
	if (ipo->ipo_dst.sa.sa_family) {
		headers[SADB_EXT_ADDRESS_DST] = p;
		export_address(&p, &ipo->ipo_dst.sa);
	}

	/* Get actual flow. */
	export_flow(&p, ipo->ipo_type, &ipo->ipo_addr, &ipo->ipo_mask,
	    headers);

	/* Add ids only when we are root. */
	perm = suser(curproc);
	if (perm == 0 && ipo->ipo_ids)
		export_identities(&p, ipo->ipo_ids, 0, headers);

	rval = 0;
ret:
	return (rval);
}

int
pfkeyv2_sysctl_policydumper(struct ipsec_policy *ipo, void *arg,
    unsigned int tableid)
{
	struct pfkeyv2_sysctl_walk *w = (struct pfkeyv2_sysctl_walk *)arg;
	void *buffer = 0;
	int i, buflen, error = 0;

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
pfkeyv2_policy_flush(struct ipsec_policy *ipo, void *arg, unsigned int tableid)
{
	int error;

	error = ipsec_delete_policy(ipo);
	if (error == 0)
		error = EAGAIN;

	return (error);
}

int
pfkeyv2_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *new, size_t newlen)
{
	struct pfkeyv2_sysctl_walk w;
	int error = EINVAL;
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
		if ((error = suser(curproc)) != 0)
			return (error);
		NET_LOCK();
		error = tdb_walk(rdomain, pfkeyv2_sysctl_walker, &w);
		NET_UNLOCK();
		if (oldp)
			*oldlenp = w.w_where - oldp;
		else
			*oldlenp = w.w_len;
		break;

	case NET_KEY_SPD_DUMP:
		NET_LOCK();
		error = spd_table_walk(rdomain,
		    pfkeyv2_sysctl_policydumper, &w);
		NET_UNLOCK();
		if (oldp)
			*oldlenp = w.w_where - oldp;
		else
			*oldlenp = w.w_len;
		break;
	}

	return (error);
}
