/*	$OpenBSD: pipex.c,v 1.106 2017/11/20 10:35:24 mpi Exp $	*/

/*-
 * Copyright (c) 2009 Internet Initiative Japan Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/conf.h>
#include <sys/time.h>
#include <sys/timeout.h>
#include <sys/kernel.h>
#include <sys/pool.h>

#include <net/if.h>
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/if_dl.h>

#include <net/radix.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/ppp_defs.h>
#include <net/ppp-comp.h>

#include "pf.h"
#if NPF > 0
#include <net/pfvar.h>
#endif

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/ip.h>
#include <netinet/ip_var.h>
#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <crypto/arc4.h>

/* drop static for ddb debuggability */
#define	Static

#include <net/pipex.h>
#include "pipex_local.h"

struct pool pipex_session_pool;
struct pool mppe_key_pool;

/*
 * static/global variables
 */
int	pipex_enable = 0;
struct pipex_hash_head
    pipex_session_list,				/* master session list */
    pipex_close_wait_list,			/* expired session list */
    pipex_peer_addr_hashtable[PIPEX_HASH_SIZE],	/* peer's address hash */
    pipex_id_hashtable[PIPEX_HASH_SIZE];	/* peer id hash */

struct radix_node_head	*pipex_rd_head4 = NULL;
struct radix_node_head	*pipex_rd_head6 = NULL;
struct timeout pipex_timer_ch;		/* callout timer context */
int pipex_prune = 1;			/* walk list every seconds */

/* pipex traffic queue */
struct mbuf_queue pipexinq = MBUF_QUEUE_INITIALIZER(IFQ_MAXLEN, IPL_NET);
struct mbuf_queue pipexoutq = MBUF_QUEUE_INITIALIZER(IFQ_MAXLEN, IPL_NET);

/* borrow an mbuf pkthdr field */
#define ph_ppp_proto ether_vtag

/* from udp_usrreq.c */
extern int udpcksum;

#ifdef PIPEX_DEBUG
int pipex_debug = 0;		/* systcl net.inet.ip.pipex_debug */
#endif

/* PPP compression == MPPE is assumed, so don't answer CCP Reset-Request. */
#define PIPEX_NO_CCP_RESETACK	1

/************************************************************************
 * Core functions
 ************************************************************************/
void
pipex_init(void)
{
	int		 i;
	static int	 pipex_init_done = 0;

	if (pipex_init_done++)
		return;

	rn_init(sizeof(struct sockaddr_in6));

	pool_init(&pipex_session_pool, sizeof(struct pipex_session), 0,
	    IPL_SOFTNET, PR_WAITOK, "ppxss", NULL);
	pool_init(&mppe_key_pool, PIPEX_MPPE_KEYLEN * PIPEX_MPPE_NOLDKEY, 0,
	    IPL_SOFTNET, PR_WAITOK, "mppekey", NULL);

	LIST_INIT(&pipex_session_list);
	LIST_INIT(&pipex_close_wait_list);

	for (i = 0; i < nitems(pipex_id_hashtable); i++)
		LIST_INIT(&pipex_id_hashtable[i]);
	for (i = 0; i < nitems(pipex_peer_addr_hashtable); i++)
		LIST_INIT(&pipex_peer_addr_hashtable[i]);
}

void
pipex_iface_init(struct pipex_iface_context *pipex_iface, struct ifnet *ifp)
{
	struct pipex_session *session;

	pipex_iface->pipexmode = 0;
	pipex_iface->ifnet_this = ifp;

	if (pipex_rd_head4 == NULL) {
		if (!rn_inithead((void **)&pipex_rd_head4,
		    offsetof(struct sockaddr_in, sin_addr)))
			panic("rn_inithead() failed on pipex_init()");
	}
	if (pipex_rd_head6 == NULL) {
		if (!rn_inithead((void **)&pipex_rd_head6,
		    offsetof(struct sockaddr_in6, sin6_addr)))
			panic("rn_inithead() failed on pipex_init()");
	}

	/* virtual pipex_session entry for multicast */
	session = pool_get(&pipex_session_pool, PR_WAITOK | PR_ZERO);
	session->is_multicast = 1;
	session->pipex_iface = pipex_iface;
	pipex_iface->multicast_session = session;
}

Static void
pipex_iface_start(struct pipex_iface_context *pipex_iface)
{
	pipex_iface->pipexmode = 1;
}

Static void
pipex_iface_stop(struct pipex_iface_context *pipex_iface)
{
	struct pipex_session *session;
	struct pipex_session *session_next;

	pipex_iface->pipexmode = 0;
	/*
	 * traversal all pipex sessions.
	 * it will become heavy if the number of pppac devices bocomes large.
	 */
	for (session = LIST_FIRST(&pipex_session_list);
	    session; session = session_next) {
		session_next = LIST_NEXT(session, session_list);
		if (session->pipex_iface == pipex_iface)
			pipex_destroy_session(session);
	}
}

void
pipex_iface_fini(struct pipex_iface_context *pipex_iface)
{
	pool_put(&pipex_session_pool, pipex_iface->multicast_session);
	pipex_iface_stop(pipex_iface);
}

int
pipex_ioctl(struct pipex_iface_context *pipex_iface, u_long cmd, caddr_t data)
{
	int pipexmode, ret = 0;

	NET_LOCK();
	switch (cmd) {
	case PIPEXSMODE:
		pipexmode = *(int *)data;
		if (pipex_iface->pipexmode != pipexmode) {
			if (pipexmode)
				pipex_iface_start(pipex_iface);
			else
				pipex_iface_stop(pipex_iface);
		}
		break;

	case PIPEXGMODE:
		*(int *)data = pipex_iface->pipexmode;
		break;

	case PIPEXASESSION:
		ret = pipex_add_session((struct pipex_session_req *)data,
		    pipex_iface);
		break;

	case PIPEXDSESSION:
		ret = pipex_close_session(
		    (struct pipex_session_close_req *)data);
		break;

	case PIPEXCSESSION:
		ret = pipex_config_session(
		    (struct pipex_session_config_req *)data);
		break;

	case PIPEXGSTAT:
		ret = pipex_get_stat((struct pipex_session_stat_req *)data);
		break;

	case PIPEXGCLOSED:
		ret = pipex_get_closed((struct pipex_session_list_req *)data);
		break;

	default:
		ret = ENOTTY;
		break;
	}
	NET_UNLOCK();

	return (ret);
}

/************************************************************************
 * Session management functions
 ************************************************************************/
Static int
pipex_add_session(struct pipex_session_req *req,
    struct pipex_iface_context *iface)
{
	struct pipex_session *session;
	struct pipex_hash_head *chain;
	struct radix_node *rn;
#ifdef PIPEX_PPPOE
	struct ifnet *over_ifp = NULL;
#endif

	/* Checks requeted parameters.  */
	if (!iface->pipexmode)
		return (ENXIO);
	switch (req->pr_protocol) {
#ifdef PIPEX_PPPOE
	case PIPEX_PROTO_PPPOE:
		over_ifp = ifunit(req->pr_proto.pppoe.over_ifname);
		if (over_ifp == NULL)
			return (EINVAL);
		if (req->pr_peer_address.ss_family != AF_UNSPEC)
			return (EINVAL);
		break;
#endif
#if defined(PIPEX_L2TP) || defined(PIPEX_PPTP)
#ifdef PIPEX_PPTP
	case PIPEX_PROTO_PPTP:
#endif
#ifdef PIPEX_L2TP
	case PIPEX_PROTO_L2TP:
#endif
		switch (req->pr_peer_address.ss_family) {
		case AF_INET:
			if (req->pr_peer_address.ss_len !=
			    sizeof(struct sockaddr_in))
				return (EINVAL);
			break;
#ifdef INET6
		case AF_INET6:
			if (req->pr_peer_address.ss_len !=
			    sizeof(struct sockaddr_in6))
				return (EINVAL);
			break;
#endif
		default:
			return (EPROTONOSUPPORT);
		}
		if (req->pr_peer_address.ss_family !=
		    req->pr_local_address.ss_family ||
		    req->pr_peer_address.ss_len !=
		    req->pr_local_address.ss_len)
			return (EINVAL);
		break;
#endif
	default:
		return (EPROTONOSUPPORT);
	}

	/* prepare a new session */
	session = pool_get(&pipex_session_pool, PR_WAITOK | PR_ZERO);
	session->state = PIPEX_STATE_OPENED;
	session->protocol = req->pr_protocol;
	session->session_id = req->pr_session_id;
	session->peer_session_id = req->pr_peer_session_id;
	session->peer_mru = req->pr_peer_mru;
	session->timeout_sec = req->pr_timeout_sec;
	session->pipex_iface = iface;
	session->ppp_flags = req->pr_ppp_flags;
	session->ppp_id = req->pr_ppp_id;

	session->ip_forward = 1;

	session->ip_address.sin_family = AF_INET;
	session->ip_address.sin_len = sizeof(struct sockaddr_in);
	session->ip_address.sin_addr = req->pr_ip_address;

	session->ip_netmask.sin_family = AF_INET;
	session->ip_netmask.sin_len = sizeof(struct sockaddr_in);
	session->ip_netmask.sin_addr = req->pr_ip_netmask;

	if (session->ip_netmask.sin_addr.s_addr == 0L)
		session->ip_netmask.sin_addr.s_addr = 0xffffffffL;
	session->ip_address.sin_addr.s_addr &=
	    session->ip_netmask.sin_addr.s_addr;

	if (req->pr_peer_address.ss_len > 0)
		memcpy(&session->peer, &req->pr_peer_address,
		    MIN(req->pr_peer_address.ss_len, sizeof(session->peer)));
	if (req->pr_local_address.ss_len > 0)
		memcpy(&session->local, &req->pr_local_address,
		    MIN(req->pr_local_address.ss_len, sizeof(session->local)));
#ifdef PIPEX_PPPOE
	if (req->pr_protocol == PIPEX_PROTO_PPPOE)
		session->proto.pppoe.over_ifidx = over_ifp->if_index;
#endif
#ifdef PIPEX_PPTP
	if (req->pr_protocol == PIPEX_PROTO_PPTP) {
		struct pipex_pptp_session *sess_pptp = &session->proto.pptp;

		sess_pptp->snd_gap = 0;
		sess_pptp->rcv_gap = 0;
		sess_pptp->snd_una = req->pr_proto.pptp.snd_una;
		sess_pptp->snd_nxt = req->pr_proto.pptp.snd_nxt;
		sess_pptp->rcv_nxt = req->pr_proto.pptp.rcv_nxt;
		sess_pptp->rcv_acked = req->pr_proto.pptp.rcv_acked;

		sess_pptp->winsz = req->pr_proto.pptp.winsz;
		sess_pptp->maxwinsz = req->pr_proto.pptp.maxwinsz;
		sess_pptp->peer_maxwinsz = req->pr_proto.pptp.peer_maxwinsz;
		/* last ack number */
		sess_pptp->ul_snd_una = sess_pptp->snd_una - 1;
	}
#endif
#ifdef PIPEX_L2TP
	if (req->pr_protocol == PIPEX_PROTO_L2TP) {
		struct pipex_l2tp_session *sess_l2tp = &session->proto.l2tp;

		/* session keys */
		sess_l2tp->tunnel_id = req->pr_proto.l2tp.tunnel_id;
		sess_l2tp->peer_tunnel_id = req->pr_proto.l2tp.peer_tunnel_id;

		/* protocol options */
		sess_l2tp->option_flags = req->pr_proto.l2tp.option_flags;

		/* initial state of dynamic context */
		sess_l2tp->ns_gap = sess_l2tp->nr_gap = 0;
		sess_l2tp->ns_nxt = req->pr_proto.l2tp.ns_nxt;
		sess_l2tp->nr_nxt = req->pr_proto.l2tp.nr_nxt;
		sess_l2tp->ns_una = req->pr_proto.l2tp.ns_una;
		sess_l2tp->nr_acked = req->pr_proto.l2tp.nr_acked;
		/* last ack number */
		sess_l2tp->ul_ns_una = sess_l2tp->ns_una - 1;
		sess_l2tp->ipsecflowinfo = req->pr_proto.l2tp.ipsecflowinfo;
	}
#endif
#ifdef PIPEX_MPPE
	if ((req->pr_ppp_flags & PIPEX_PPP_MPPE_ACCEPTED) != 0) {
		if (req->pr_mppe_recv.keylenbits <= 0) {
			pool_put(&pipex_session_pool, session);
			return (EINVAL);
		}
		pipex_session_init_mppe_recv(session,
		    req->pr_mppe_recv.stateless, req->pr_mppe_recv.keylenbits,
		    req->pr_mppe_recv.master_key);
	}
	if ((req->pr_ppp_flags & PIPEX_PPP_MPPE_ENABLED) != 0) {
		if (req->pr_mppe_send.keylenbits <= 0) {
			pool_put(&pipex_session_pool, session);
			return (EINVAL);
		}
		pipex_session_init_mppe_send(session,
		    req->pr_mppe_send.stateless, req->pr_mppe_send.keylenbits,
		    req->pr_mppe_send.master_key);
	}

	if (pipex_session_is_mppe_required(session)) {
		if (!pipex_session_is_mppe_enabled(session) ||
		    !pipex_session_is_mppe_accepted(session)) {
			pool_put(&pipex_session_pool, session);
			return (EINVAL);
		}
	}
#endif

	NET_ASSERT_LOCKED();
	/* commit the session */
	if (!in_nullhost(session->ip_address.sin_addr)) {
		if (pipex_lookup_by_ip_address(session->ip_address.sin_addr)
		    != NULL) {
			pool_put(&pipex_session_pool, session);
			return (EADDRINUSE);
		}

		rn = rn_addroute(&session->ip_address, &session->ip_netmask,
		    pipex_rd_head4, session->ps4_rn, RTP_STATIC);
		if (rn == NULL) {
			pool_put(&pipex_session_pool, session);
			return (ENOMEM);
		}
	}
	if (0) { /* NOT YET */
		rn = rn_addroute(&session->ip6_address, &session->ip6_prefixlen,
		    pipex_rd_head6, session->ps6_rn, RTP_STATIC);
		if (rn == NULL) {
			pool_put(&pipex_session_pool, session);
			return (ENOMEM);
		}
	}

	chain = PIPEX_ID_HASHTABLE(session->session_id);
	LIST_INSERT_HEAD(chain, session, id_chain);
	LIST_INSERT_HEAD(&pipex_session_list, session, session_list);
	switch (req->pr_protocol) {
	case PIPEX_PROTO_PPTP:
	case PIPEX_PROTO_L2TP:
		chain = PIPEX_PEER_ADDR_HASHTABLE(
		    pipex_sockaddr_hash_key(&session->peer.sa));
		LIST_INSERT_HEAD(chain, session, peer_addr_chain);
	}

	/* if first session is added, start timer */
	if (LIST_NEXT(session, session_list) == NULL)
		pipex_timer_start();

	pipex_session_log(session, LOG_INFO, "PIPEX is ready.");

	return (0);
}

int
pipex_notify_close_session(struct pipex_session *session)
{
	NET_ASSERT_LOCKED();
	session->state = PIPEX_STATE_CLOSE_WAIT;
	session->stat.idle_time = 0;
	LIST_INSERT_HEAD(&pipex_close_wait_list, session, state_list);

	return (0);
}

int
pipex_notify_close_session_all(void)
{
	struct pipex_session *session;

	NET_ASSERT_LOCKED();
	LIST_FOREACH(session, &pipex_session_list, session_list)
		if (session->state == PIPEX_STATE_OPENED)
			pipex_notify_close_session(session);
	return (0);
}

Static int
pipex_close_session(struct pipex_session_close_req *req)
{
	struct pipex_session *session;

	NET_ASSERT_LOCKED();
	session = pipex_lookup_by_session_id(req->pcr_protocol,
	    req->pcr_session_id);
	if (session == NULL)
		return (EINVAL);

	/* remove from close_wait list */
	if (session->state == PIPEX_STATE_CLOSE_WAIT)
		LIST_REMOVE(session, state_list);

	/* get statistics before destroy the session */
	req->pcr_stat = session->stat;
	session->state = PIPEX_STATE_CLOSED;

	return (0);
}

Static int
pipex_config_session(struct pipex_session_config_req *req)
{
	struct pipex_session *session;

	NET_ASSERT_LOCKED();
	session = pipex_lookup_by_session_id(req->pcr_protocol,
	    req->pcr_session_id);
	if (session == NULL)
		return (EINVAL);
	session->ip_forward = req->pcr_ip_forward;

	return (0);
}

Static int
pipex_get_stat(struct pipex_session_stat_req *req)
{
	struct pipex_session *session;

	NET_ASSERT_LOCKED();
	session = pipex_lookup_by_session_id(req->psr_protocol,
	    req->psr_session_id);
	if (session == NULL) {
		return (EINVAL);
	}
	req->psr_stat = session->stat;

	return (0);
}

Static int
pipex_get_closed(struct pipex_session_list_req *req)
{
	struct pipex_session *session;

	NET_ASSERT_LOCKED();
	bzero(req, sizeof(*req));
	while (!LIST_EMPTY(&pipex_close_wait_list)) {
		session = LIST_FIRST(&pipex_close_wait_list);
		req->plr_ppp_id[req->plr_ppp_id_count++] = session->ppp_id;
		LIST_REMOVE(session, state_list);
		session->state = PIPEX_STATE_CLOSE_WAIT2;
		if (req->plr_ppp_id_count >= PIPEX_MAX_LISTREQ) {
			if (!LIST_EMPTY(&pipex_close_wait_list))
				req->plr_flags |= PIPEX_LISTREQ_MORE;
			break;
		}
	}

	return (0);
}

Static int
pipex_destroy_session(struct pipex_session *session)
{
	struct radix_node *rn;

	/* remove from radix tree and hash chain */
	NET_ASSERT_LOCKED();

	if (!in_nullhost(session->ip_address.sin_addr)) {
		rn = rn_delete(&session->ip_address, &session->ip_netmask,
		    pipex_rd_head4, (struct radix_node *)session);
		KASSERT(rn != NULL);
	}

	LIST_REMOVE(session, id_chain);
	LIST_REMOVE(session, session_list);
#ifdef PIPEX_PPTP
	if (session->protocol == PIPEX_PROTO_PPTP) {
		LIST_REMOVE(session, peer_addr_chain);
	}
#endif
#ifdef PIPEX_L2TP
	if (session->protocol == PIPEX_PROTO_L2TP) {
		LIST_REMOVE(session, peer_addr_chain);
	}
#endif
	/* if final session is destroyed, stop timer */
	if (LIST_EMPTY(&pipex_session_list))
		pipex_timer_stop();

	if (session->mppe_recv.old_session_keys)
		pool_put(&mppe_key_pool, session->mppe_recv.old_session_keys);
	pool_put(&pipex_session_pool, session);

	return (0);
}

Static struct pipex_session *
pipex_lookup_by_ip_address(struct in_addr addr)
{
	struct pipex_session *session;
	struct sockaddr_in pipex_in4, pipex_in4mask;

	bzero(&pipex_in4, sizeof(pipex_in4));
	pipex_in4.sin_addr = addr;
	pipex_in4.sin_family = AF_INET;
	pipex_in4.sin_len = sizeof(pipex_in4);

	bzero(&pipex_in4mask, sizeof(pipex_in4mask));
	pipex_in4mask.sin_addr.s_addr = htonl(0xFFFFFFFFL);
	pipex_in4mask.sin_family = AF_INET;
	pipex_in4mask.sin_len = sizeof(pipex_in4mask);

	session = (struct pipex_session *)rn_lookup(&pipex_in4, &pipex_in4mask,
	    pipex_rd_head4);

#ifdef PIPEX_DEBUG
	if (session == NULL) {
		char buf[INET_ADDRSTRLEN];

		PIPEX_DBG((NULL, LOG_DEBUG, "<%s> session not found (addr=%s)",
		    __func__, inet_ntop(AF_INET, &addr, buf, sizeof(buf))));
	}
#endif

	return (session);
}

Static struct pipex_session *
pipex_lookup_by_session_id(int protocol, int session_id)
{
	struct pipex_hash_head *list;
	struct pipex_session *session;

	NET_ASSERT_LOCKED();
	list = PIPEX_ID_HASHTABLE(session_id);
	LIST_FOREACH(session, list, id_chain) {
		if (session->protocol == protocol &&
		    session->session_id == session_id)
			break;
	}

#ifdef PIPEX_DEBUG
	if (session == NULL)
		PIPEX_DBG((NULL, LOG_DEBUG,
		    "<%s> session not found (session_id=%d)", __func__,
		    session_id));
#endif

	return (session);
}

/***********************************************************************
 * Queue and Software Interrupt Handler
 ***********************************************************************/
void
pipexintr(void)
{
	struct pipex_session *pkt_session;
	u_int16_t proto;
	struct mbuf *m;
	struct mbuf_list ml;

	/* ppp output */
	mq_delist(&pipexoutq, &ml);
	while ((m = ml_dequeue(&ml)) != NULL) {
		pkt_session = m->m_pkthdr.ph_cookie;
		if (pkt_session == NULL) {
			m_freem(m);
			continue;
		}
		proto = m->m_pkthdr.ph_ppp_proto;

		m->m_pkthdr.ph_cookie = NULL;
		m->m_pkthdr.ph_ppp_proto = 0;

		if (pkt_session->is_multicast != 0) {
			struct pipex_session *session;
			struct mbuf *m0;

			LIST_FOREACH(session, &pipex_session_list,
			    session_list) {
				if (session->pipex_iface !=
				    pkt_session->pipex_iface)
					continue;
				if (session->ip_forward == 0 &&
				    session->ip6_forward == 0)
					continue;
				m0 = m_copym(m, 0, M_COPYALL, M_NOWAIT);
				if (m0 == NULL) {
					session->stat.oerrors++;
					continue;
				}
				pipex_ppp_output(m0, session, proto);
			}
			m_freem(m);
		} else
			pipex_ppp_output(m, pkt_session, proto);
	}

	/* ppp input */
	mq_delist(&pipexinq, &ml);
	while ((m = ml_dequeue(&ml)) != NULL) {
		pkt_session = m->m_pkthdr.ph_cookie;
		if (pkt_session == NULL) {
			m_freem(m);
			continue;
		}
		pipex_ppp_input(m, pkt_session, 0);
	}
}

Static int
pipex_ppp_enqueue(struct mbuf *m0, struct pipex_session *session,
    struct mbuf_queue *mq)
{
	m0->m_pkthdr.ph_cookie = session;
	/* XXX need to support other protocols */
	m0->m_pkthdr.ph_ppp_proto = PPP_IP;

	if (mq_enqueue(mq, m0) != 0)
		return (1);

	schednetisr(NETISR_PIPEX);

	return (0);
}

/***********************************************************************
 * Timer functions
 ***********************************************************************/
Static void
pipex_timer_start(void)
{
	timeout_set_proc(&pipex_timer_ch, pipex_timer, NULL);
	timeout_add_sec(&pipex_timer_ch, pipex_prune);
}

Static void
pipex_timer_stop(void)
{
	timeout_del(&pipex_timer_ch);
}

Static void
pipex_timer(void *ignored_arg)
{
	struct pipex_session *session;
	struct pipex_session *session_next;

	timeout_add_sec(&pipex_timer_ch, pipex_prune);

	NET_LOCK();
	/* walk through */
	for (session = LIST_FIRST(&pipex_session_list); session;
	    session = session_next) {
		session_next = LIST_NEXT(session, session_list);
		switch (session->state) {
		case PIPEX_STATE_OPENED:
			if (session->timeout_sec == 0)
				continue;

			session->stat.idle_time++;
			if (session->stat.idle_time < session->timeout_sec)
				continue;

			pipex_notify_close_session(session);
			break;

		case PIPEX_STATE_CLOSE_WAIT:
		case PIPEX_STATE_CLOSE_WAIT2:
			/* Wait PIPEXDSESSION from userland */
			session->stat.idle_time++;
			if (session->stat.idle_time < PIPEX_CLOSE_TIMEOUT)
				continue;

			if (session->state == PIPEX_STATE_CLOSE_WAIT)
				LIST_REMOVE(session, state_list);
			session->state = PIPEX_STATE_CLOSED;
			/* FALLTHROUGH */

		case PIPEX_STATE_CLOSED:
			/*
			 * mbuf queued in pipexinq or pipexoutq may have a
			 * refererce to this session.
			 */
			if (!mq_empty(&pipexinq) || !mq_empty(&pipexoutq))
				continue;

			pipex_destroy_session(session);
			break;

		default:
			break;
		}
	}

	NET_UNLOCK();
}

/***********************************************************************
 * Common network I/O functions.  (tunnel protocol independent)
 ***********************************************************************/
struct mbuf *
pipex_output(struct mbuf *m0, int af, int off,
    struct pipex_iface_context *pipex_iface)
{
	struct pipex_session *session;
	struct ip ip;
	struct mbuf *mret;

	session = NULL;
	mret = NULL;
	switch (af) {
	case AF_INET:
		if (m0->m_pkthdr.len >= sizeof(struct ip) + off) {
			m_copydata(m0, off, sizeof(struct ip), (caddr_t)&ip);
			if (IN_MULTICAST(ip.ip_dst.s_addr))
				session = pipex_iface->multicast_session;
			else
				session = pipex_lookup_by_ip_address(ip.ip_dst);
		}
		if (session != NULL) {
			if (session == pipex_iface->multicast_session) {
				mret = m0;
				m0 = m_copym(m0, 0, M_COPYALL, M_NOWAIT);
				if (m0 == NULL) {
					m0 = mret;
					mret = NULL;
					goto drop;
				}
			}

			if (off > 0)
				m_adj(m0, off);

			pipex_ip_output(m0, session);
			return (mret);
		}
		break;
	}

	return (m0);

drop:
	m_freem(m0);
	if (session != NULL)
		session->stat.oerrors++;
	return(NULL);
}

Static void
pipex_ip_output(struct mbuf *m0, struct pipex_session *session)
{
	int is_idle;
	struct ifnet *ifp;

	/* output succeed here as a interface */
	ifp = session->pipex_iface->ifnet_this;
	ifp->if_opackets++;
	ifp->if_obytes+=m0->m_pkthdr.len;

	if (session->is_multicast == 0) {
		/*
		 * Multicast packet is a idle packet and it's not TCP.
		 */
		if (session->ip_forward == 0 && session->ip6_forward == 0)
			goto drop;
		/* reset idle timer */
		if (session->timeout_sec != 0) {
			is_idle = 0;
			m0 = ip_is_idle_packet(m0, &is_idle);
			if (m0 == NULL)
				goto dropped;
			if (is_idle == 0)
				/* update expire time */
				session->stat.idle_time = 0;
		}

		/* adjust tcpmss */
		if ((session->ppp_flags & PIPEX_PPP_ADJUST_TCPMSS) != 0) {
			m0 = adjust_tcp_mss(m0, session->peer_mru);
			if (m0 == NULL)
				goto dropped;
		}
	} else
		m0->m_flags &= ~(M_BCAST|M_MCAST);

	/* output ip packets to the session tunnel */
	if (pipex_ppp_enqueue(m0, session, &pipexoutq))
		goto dropped;

	return;
drop:
	m_freem(m0);
dropped:
	session->stat.oerrors++;
}

Static void
pipex_ppp_output(struct mbuf *m0, struct pipex_session *session, int proto)
{
	u_char *cp, hdr[16];

#ifdef PIPEX_MPPE
	if (pipex_session_is_mppe_enabled(session)) {
		if (proto == PPP_IP) {
			pipex_mppe_output(m0, session, PPP_IP);
			return;
		}
	}
#endif /* PIPEX_MPPE */
	cp = hdr;
	if (session->protocol != PIPEX_PROTO_PPPOE) {
		/* PPPoE has not address and control field */
		PUTCHAR(PPP_ALLSTATIONS, cp);
		PUTCHAR(PPP_UI, cp);
	}
	PUTSHORT(proto, cp);

	M_PREPEND(m0, cp - hdr, M_NOWAIT);
	if (m0 == NULL)
		goto drop;
	memcpy(mtod(m0, u_char *), hdr, cp - hdr);

	switch (session->protocol) {
#ifdef	PIPEX_PPPOE
	case PIPEX_PROTO_PPPOE:
		pipex_pppoe_output(m0, session);
		break;
#endif
#ifdef PIPEX_PPTP
	case PIPEX_PROTO_PPTP:
		pipex_pptp_output(m0, session, 1, 1);
		break;
#endif
#ifdef	PIPEX_L2TP
	case PIPEX_PROTO_L2TP:
		pipex_l2tp_output(m0, session);
		break;
#endif
	default:
		goto drop;
	}

	return;
drop:
	m_freem(m0);
	session->stat.oerrors++;
}

Static void
pipex_ppp_input(struct mbuf *m0, struct pipex_session *session, int decrypted)
{
	int proto, hlen = 0;
	struct mbuf *n;

	KASSERT(m0->m_pkthdr.len >= PIPEX_PPPMINLEN);
	proto = pipex_ppp_proto(m0, session, 0, &hlen);
#ifdef PIPEX_MPPE
	if (proto == PPP_COMP) {
		if (decrypted)
			goto drop;

		/* checked this on ppp_common_input() already. */
		KASSERT(pipex_session_is_mppe_accepted(session));

		m_adj(m0, hlen);
		pipex_mppe_input(m0, session);
		return;
	}
	if (proto == PPP_CCP) {
		if (decrypted)
			goto drop;

#if NBPFILTER > 0
	    {
		struct ifnet *ifp = session->pipex_iface->ifnet_this;
		if (ifp->if_bpf && ifp->if_type == IFT_PPP)
			bpf_mtap(ifp->if_bpf, m0, BPF_DIRECTION_IN);
	    }
#endif
		m_adj(m0, hlen);
		pipex_ccp_input(m0, session);
		return;
	}
#endif
	m_adj(m0, hlen);
	if (!ALIGNED_POINTER(mtod(m0, caddr_t), uint32_t)) {
		n = m_dup_pkt(m0, 0, M_NOWAIT);
		if (n == NULL)
			goto drop;
		m_freem(m0);
		m0 = n;
	}

	switch (proto) {
	case PPP_IP:
		if (session->ip_forward == 0)
			goto drop;
		if (!decrypted && pipex_session_is_mppe_required(session))
			/*
			 * if ip packet received when mppe
			 * is required, discard it.
			 */
			goto drop;
		pipex_ip_input(m0, session);
		return;
#ifdef INET6
	case PPP_IPV6:
		if (session->ip6_forward == 0)
			goto drop;
		if (!decrypted && pipex_session_is_mppe_required(session))
			/*
			 * if ip packet received when mppe
			 * is required, discard it.
			 */
			goto drop;
		pipex_ip6_input(m0, session);
		return;
#endif
	default:
		if (decrypted)
			goto drop;
		/* protocol must be checked on pipex_common_input() already */
		KASSERT(0);
		goto drop;
	}

	return;
drop:
	m_freem(m0);
	session->stat.ierrors++;
}

Static void
pipex_ip_input(struct mbuf *m0, struct pipex_session *session)
{
	struct ifnet *ifp;
	struct ip *ip;
	int len;
	int is_idle;

	/* change recvif */
	ifp = session->pipex_iface->ifnet_this;
	m0->m_pkthdr.ph_ifidx = ifp->if_index;

	if (ISSET(session->ppp_flags, PIPEX_PPP_INGRESS_FILTER)) {
		PIPEX_PULLUP(m0, sizeof(struct ip));
		if (m0 == NULL)
			goto drop;
		/* ingress filter */
		ip = mtod(m0, struct ip *);
		if ((ip->ip_src.s_addr & session->ip_netmask.sin_addr.s_addr) !=
		    session->ip_address.sin_addr.s_addr) {
			char src[INET_ADDRSTRLEN];

			pipex_session_log(session, LOG_DEBUG,
			    "ip packet discarded by ingress filter (src %s)",
			    inet_ntop(AF_INET, &ip->ip_src, src, sizeof(src)));
			goto drop;
		}
	}

	/* idle timer */
	if (session->timeout_sec != 0) {
		is_idle = 0;
		m0 = ip_is_idle_packet(m0, &is_idle);
		if (m0 == NULL)
			goto drop;
		if (is_idle == 0)
			/* update expire time */
			session->stat.idle_time = 0;
	}

	/* adjust tcpmss */
	if (session->ppp_flags & PIPEX_PPP_ADJUST_TCPMSS) {
		m0 = adjust_tcp_mss(m0, session->peer_mru);
		if (m0 == NULL)
			goto drop;
	}

#if NPF > 0
	pf_pkt_addr_changed(m0);
#endif

	len = m0->m_pkthdr.len;

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap_af(ifp->if_bpf, AF_INET, m0, BPF_DIRECTION_IN);
#endif

	ifp->if_ipackets++;
	ifp->if_ibytes += len;
	session->stat.ipackets++;
	session->stat.ibytes += len;
	ipv4_input(ifp, m0);

	return;
drop:
	m_freem(m0);
	session->stat.ierrors++;
}

#ifdef INET6
Static void
pipex_ip6_input(struct mbuf *m0, struct pipex_session *session)
{
	struct ifnet *ifp;
	int len;

	/* change recvif */
	ifp = session->pipex_iface->ifnet_this;
	m0->m_pkthdr.ph_ifidx = ifp->if_index;

	/*
	 * XXX: what is reasonable ingress filter ???
	 *      only one address is enough ??
	 */

	/* XXX: we must define idle packet for IPv6(ICMPv6). */

	/*
	 * XXX: tcpmss adjustment for IPv6 is required???
	 *      We may use PMTUD in IPv6....
	 */

#if NPF > 0
	pf_pkt_addr_changed(m0);
#endif

	len = m0->m_pkthdr.len;

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap_af(ifp->if_bpf, AF_INET6, m0, BPF_DIRECTION_IN);
#endif

	ifp->if_ipackets++;
	ifp->if_ibytes += len;
	session->stat.ipackets++;
	session->stat.ibytes += len;
	ipv6_input(ifp, m0);
}
#endif

Static struct mbuf *
pipex_common_input(struct pipex_session *session, struct mbuf *m0, int hlen,
    int plen, int useq)
{
	int proto, ppphlen;
	u_char code;

	if ((m0->m_pkthdr.len < hlen + PIPEX_PPPMINLEN) ||
	    (plen < PIPEX_PPPMINLEN))
		goto drop;

	proto = pipex_ppp_proto(m0, session, hlen, &ppphlen);
	switch (proto) {
#ifdef PIPEX_MPPE
	case PPP_CCP:
		code = 0;
		KASSERT(m0->m_pkthdr.len >= hlen + ppphlen + 1);
		m_copydata(m0, hlen + ppphlen, 1, (caddr_t)&code);
		if (code != CCP_RESETREQ && code != CCP_RESETACK)
			goto not_ours;
		break;

	case PPP_COMP:
		if (pipex_session_is_mppe_accepted(session))
			break;
		goto not_ours;
#endif
	case PPP_IP:
#ifdef INET6
	case PPP_IPV6:
#endif
		break;
	default:
		goto not_ours;
	}

	/* ok,  The packet is for PIPEX */
	m_adj(m0, hlen);/* cut off the tunnle protocol header */

	/* ensure the mbuf length equals the PPP frame length */
	if (m0->m_pkthdr.len < plen)
		goto drop;
	if (m0->m_pkthdr.len > plen) {
		if (m0->m_len == m0->m_pkthdr.len) {
			m0->m_len = plen;
			m0->m_pkthdr.len = plen;
		} else
			m_adj(m0, plen - m0->m_pkthdr.len);
	}

	if (!useq) {
		pipex_ppp_input(m0, session, 0);
		return (NULL);
	}

	/* input ppp packets to kernel session */
	if (pipex_ppp_enqueue(m0, session, &pipexinq) != 0)
		goto dropped;
	else
		return (NULL);
drop:
	m_freem(m0);
dropped:
	session->stat.ierrors++;
	return (NULL);

not_ours:
	return (m0);	/* Not to be handled by PIPEX */
}

/*
 * pipex_ppp_proto
 */
Static int
pipex_ppp_proto(struct mbuf *m0, struct pipex_session *session, int off,
    int *hlenp)
{
	int proto;
	u_char *cp, pktbuf[4];

	KASSERT(m0->m_pkthdr.len > sizeof(pktbuf));
	m_copydata(m0, off, sizeof(pktbuf), pktbuf);
	cp = pktbuf;

	if (pipex_session_has_acf(session)) {
		if (cp[0] == PPP_ALLSTATIONS && cp[1] == PPP_UI)
			cp += 2;
#ifdef PIPEX_DEBUG
		else if (!pipex_session_is_acfc_accepted(session))
			PIPEX_DBG((session, LOG_DEBUG,
			    "no acf but acfc is not accepted by the peer."));
#endif
	}
	if ((*cp & 0x01) != 0) {
		if (!pipex_session_is_pfc_accepted(session)) {
			PIPEX_DBG((session, LOG_DEBUG, "Received a broken ppp "
			    "frame.  No protocol field. %02x-%02x",
			    cp[0], cp[1]));
			return (-1);
		}
		GETCHAR(proto, cp);
	} else
		GETSHORT(proto, cp);

	if (hlenp != NULL)
		*hlenp = cp - pktbuf;

	return (proto);
}

#ifdef PIPEX_PPPOE
/***********************************************************************
 * PPPoE
 ***********************************************************************/
Static u_char	pipex_pppoe_padding[ETHERMIN];
/*
 * pipex_pppoe_lookup_session
 */
struct pipex_session *
pipex_pppoe_lookup_session(struct mbuf *m0)
{
	struct pipex_session *session;
	struct pipex_pppoe_header pppoe;

	/* short packet */
	if (m0->m_pkthdr.len < (sizeof(struct ether_header) + sizeof(pppoe)))
		return (NULL);

	m_copydata(m0, sizeof(struct ether_header),
	    sizeof(struct pipex_pppoe_header), (caddr_t)&pppoe);
	pppoe.session_id = ntohs(pppoe.session_id);
	session = pipex_lookup_by_session_id(PIPEX_PROTO_PPPOE,
	    pppoe.session_id);
#ifdef PIPEX_DEBUG
	if (session == NULL)
		PIPEX_DBG((NULL, LOG_DEBUG, "<%s> session not found (id=%d)",
		    __func__, pppoe.session_id));
#endif
	if (session && session->proto.pppoe.over_ifidx != m0->m_pkthdr.ph_ifidx)
		session = NULL;

	return (session);
}

struct mbuf *
pipex_pppoe_input(struct mbuf *m0, struct pipex_session *session)
{
	int hlen;
	struct pipex_pppoe_header pppoe;

	/* already checked at pipex_pppoe_lookup_session */
	KASSERT(m0->m_pkthdr.len >= (sizeof(struct ether_header) +
	    sizeof(pppoe)));

	m_copydata(m0, sizeof(struct ether_header),
	    sizeof(struct pipex_pppoe_header), (caddr_t)&pppoe);

	hlen = sizeof(struct ether_header) + sizeof(struct pipex_pppoe_header);
	if ((m0 = pipex_common_input(session, m0, hlen, ntohs(pppoe.length), 0))
	    == NULL)
		return (NULL);
	m_freem(m0);
	session->stat.ierrors++;
	return (NULL);
}

/*
 * pipex_ppope_output
 */
Static void
pipex_pppoe_output(struct mbuf *m0, struct pipex_session *session)
{
	struct pipex_pppoe_header *pppoe;
	struct ifnet *ifp;
	int len, padlen;

	/* save length for pppoe header */
	len = m0->m_pkthdr.len;

	/* prepend protocol header */
	M_PREPEND(m0, sizeof(struct pipex_pppoe_header), M_NOWAIT);
	if (m0 == NULL) {
		PIPEX_DBG((NULL, LOG_ERR,
		    "<%s> cannot prepend header.", __func__));
		session->stat.oerrors++;
		return;
	}
	padlen = ETHERMIN - m0->m_pkthdr.len;
	if (padlen > 0)
		m_copyback(m0, m0->m_pkthdr.len, padlen, pipex_pppoe_padding,
		    M_NOWAIT);

	/* setup pppoe header information */
	pppoe = mtod(m0, struct pipex_pppoe_header *);
	pppoe->vertype = PIPEX_PPPOE_VERTYPE;
	pppoe->code = PIPEX_PPPOE_CODE_SESSION;
	pppoe->session_id = htons(session->session_id);
	pppoe->length = htons(len);

	m0->m_pkthdr.ph_ifidx = session->proto.pppoe.over_ifidx;
	m0->m_flags &= ~(M_BCAST|M_MCAST);

	ifp = if_get(session->proto.pppoe.over_ifidx);
	if (ifp != NULL) {
		ifp->if_output(ifp, m0, &session->peer.sa, NULL);
		session->stat.opackets++;
		session->stat.obytes += len;
	} else {
		m_freem(m0);
		session->stat.oerrors++;
	}
	if_put(ifp);
}
#endif /* PIPEX_PPPOE */

#ifdef PIPEX_PPTP
/***********************************************************************
 * PPTP
 ***********************************************************************/
Static void
pipex_pptp_output(struct mbuf *m0, struct pipex_session *session,
    int has_seq, int has_ack)
{
	int len, reqlen;
	struct pipex_gre_header *gre = NULL;
	struct ip *ip;
	u_char *cp;

	reqlen = PIPEX_IPGRE_HDRLEN + (has_seq + has_ack) * 4;

	len = 0;
	if (m0 != NULL) {
		/* save length for gre header */
		len = m0->m_pkthdr.len;
		/* prepend protocol header */
		M_PREPEND(m0, reqlen, M_NOWAIT);
		if (m0 == NULL)
			goto drop;
	} else {
		MGETHDR(m0, M_DONTWAIT, MT_DATA);
		if (m0 && reqlen > MHLEN) {
			MCLGET(m0, M_DONTWAIT);
			if ((m0->m_flags & M_EXT) == 0) {
				m_freem(m0);
				m0 = NULL;
			}
		}
		if (m0 == NULL)
			goto drop;
		m0->m_pkthdr.len = m0->m_len = reqlen;
	}

	/* setup ip header information */
	ip = mtod(m0, struct ip *);

	ip->ip_len = htons(m0->m_pkthdr.len);
	ip->ip_off = 0;
	ip->ip_ttl = MAXTTL;
	ip->ip_p = IPPROTO_GRE;
	ip->ip_tos = 0;

	ip->ip_src = session->local.sin4.sin_addr;
	ip->ip_dst = session->peer.sin4.sin_addr;
#if NPF > 0
	pf_pkt_addr_changed(m0);
#endif

	/* setup gre(ver1) header information */
	gre = PIPEX_SEEK_NEXTHDR(ip, sizeof(struct ip),
	    struct pipex_gre_header *);
	gre->type = htons(PIPEX_GRE_PROTO_PPP);
	gre->call_id = htons(session->peer_session_id);
	gre->flags = PIPEX_GRE_KFLAG | PIPEX_GRE_VER;	/* do htons later */
	gre->len = htons(len);

	cp = PIPEX_SEEK_NEXTHDR(gre, sizeof(struct pipex_gre_header),u_char *);
	if (has_seq) {
		gre->flags |= PIPEX_GRE_SFLAG;
		PUTLONG(session->proto.pptp.snd_nxt, cp);
		session->proto.pptp.snd_nxt++;
		session->proto.pptp.snd_gap++;
	}
	if (has_ack) {
		gre->flags |= PIPEX_GRE_AFLAG;
		session->proto.pptp.rcv_acked = session->proto.pptp.rcv_nxt - 1;
		PUTLONG(session->proto.pptp.rcv_acked, cp);
	}
	gre->flags = htons(gre->flags);

	m0->m_pkthdr.ph_ifidx = session->pipex_iface->ifnet_this->if_index;
	if (ip_output(m0, NULL, NULL, 0, NULL, NULL, 0) != 0) {
		PIPEX_DBG((session, LOG_DEBUG, "ip_output failed."));
		goto drop;
	}
	if (len > 0) {	/* network layer only */
		/* countup statistics */
		session->stat.opackets++;
		session->stat.obytes += len;
	}

	return;
drop:
	session->stat.oerrors++;
}

struct pipex_session *
pipex_pptp_lookup_session(struct mbuf *m0)
{
	struct pipex_session *session;
	struct pipex_gre_header gre;
	struct ip ip;
	uint16_t flags;
	uint16_t id;
	int hlen;

	if (m0->m_pkthdr.len < PIPEX_IPGRE_HDRLEN) {
		PIPEX_DBG((NULL, LOG_DEBUG,
		    "<%s> packet length is too short", __func__));
		goto not_ours;
	}

	/* get ip header info */
	m_copydata(m0, 0, sizeof(struct ip), (caddr_t)&ip);
	hlen = ip.ip_hl << 2;

	/*
	 * m0 has already passed ip_input(), so there is
	 * no necessity for ip packet inspection.
	 */

	/* get gre flags */
	m_copydata(m0, hlen, sizeof(gre), (caddr_t)&gre);
	flags = ntohs(gre.flags);

	/* gre version must be '1' */
	if ((flags & PIPEX_GRE_VERMASK) != PIPEX_GRE_VER) {
		PIPEX_DBG((NULL, LOG_DEBUG,
		    "<%s> gre header wrong version.", __func__));
		goto not_ours;
	}

	/* gre keys must be present */
	if ((flags & PIPEX_GRE_KFLAG) == 0) {
		PIPEX_DBG((NULL, LOG_DEBUG,
		    "<%s> gre header has no keys.", __func__));
		goto not_ours;
	}

	/* flag check */
	if ((flags & PIPEX_GRE_UNUSEDFLAGS) != 0) {
		PIPEX_DBG((NULL, LOG_DEBUG,
		    "<%s> gre header has unused flags at pptp.", __func__));
		goto not_ours;
	}

	/* lookup pipex session table */
	id = ntohs(gre.call_id);
	session = pipex_lookup_by_session_id(PIPEX_PROTO_PPTP, id);
#ifdef PIPEX_DEBUG
	if (session == NULL) {
		PIPEX_DBG((NULL, LOG_DEBUG,
		    "<%s> session not found (id=%d)", __func__, id));
		goto not_ours;
	}
#endif

	return (session);

not_ours:
	return (NULL);
}

struct mbuf *
pipex_pptp_input(struct mbuf *m0, struct pipex_session *session)
{
	int hlen, has_seq, has_ack, nseq;
	const char *reason = "";
	u_char *cp, *seqp = NULL, *ackp = NULL;
	uint32_t flags, seq = 0, ack = 0;
	struct ip *ip;
	struct pipex_gre_header *gre;
	struct pipex_pptp_session *pptp_session;
	int rewind = 0;

	KASSERT(m0->m_pkthdr.len >= PIPEX_IPGRE_HDRLEN);
	pptp_session = &session->proto.pptp;

	/* get ip header */
	ip = mtod(m0, struct ip *);
	hlen = ip->ip_hl << 2;

	/* seek gre header */
	gre = PIPEX_SEEK_NEXTHDR(ip, hlen, struct pipex_gre_header *);
	flags = ntohs(gre->flags);

	/* pullup for seek sequences in header */
	has_seq = (flags & PIPEX_GRE_SFLAG) ? 1 : 0;
	has_ack = (flags & PIPEX_GRE_AFLAG) ? 1 : 0;
	hlen = PIPEX_IPGRE_HDRLEN + 4 * (has_seq + has_ack);
	if (m0->m_len < hlen) {
		m0 = m_pullup(m0, hlen);
		if (m0 == NULL) {
			PIPEX_DBG((session, LOG_DEBUG, "pullup failed."));
			goto drop;
		}
	}

	/* check sequence */
	cp = PIPEX_SEEK_NEXTHDR(gre, sizeof(struct pipex_gre_header),u_char *);
	if (has_seq) {
		seqp = cp;
		GETLONG(seq, cp);
	}
	if (has_ack) {
		ackp = cp;
		GETLONG(ack, cp);
		if (ack + 1 == pptp_session->snd_una) {
			/* ack has not changed before */
		} else if (SEQ32_LT(ack, pptp_session->snd_una)) {
			/* OoO ack packets should not be dropped. */
			rewind = 1;
		} else if (SEQ32_GT(ack, pptp_session->snd_nxt)) {
			reason = "ack for unknown sequence";
			goto out_seq;
		} else
			pptp_session->snd_una = ack + 1;
	}
	if (!has_seq) {
		/* ack only packet */
		goto not_ours;
	}
	if (SEQ32_LT(seq, pptp_session->rcv_nxt)) {
		rewind = 1;
		if (SEQ32_LT(seq,
		    pptp_session->rcv_nxt - PIPEX_REWIND_LIMIT)) {
			reason = "out of sequence";
			goto out_seq;
		}
	} else if (SEQ32_GE(seq, pptp_session->rcv_nxt +
	    pptp_session->maxwinsz)) {
		pipex_session_log(session, LOG_DEBUG,
		    "received packet caused window overflow. seq=%u(%u-%u)"
		    "may lost %d packets.", seq, pptp_session->rcv_nxt,
		    pptp_session->rcv_nxt + pptp_session->maxwinsz,
		    (int)SEQ32_SUB(seq, pptp_session->rcv_nxt));
	}

	seq++;
	nseq = SEQ32_SUB(seq, pptp_session->rcv_nxt);
	if (!rewind) {
		pptp_session->rcv_nxt = seq;
		if (SEQ32_SUB(seq, pptp_session->rcv_acked) >
		    roundup(pptp_session->winsz, 2) / 2) /* Send ack only packet. */
			pipex_pptp_output(NULL, session, 0, 1);
	}

	if ((m0 = pipex_common_input(session, m0, hlen, ntohs(gre->len), 1))
	    == NULL) {
		/* ok,  The packet is for PIPEX */
		if (!rewind)
			session->proto.pptp.rcv_gap += nseq;
		return (NULL);
	}

	if (rewind)
		goto out_seq;

not_ours:
	seq--;	/* revert original seq value */

	/*
	 * overwrite sequence numbers to adjust a gap between pipex and
	 * userland.
	 */
	if (seqp != NULL) {
		seq -= pptp_session->rcv_gap;
		PUTLONG(seq, seqp);
	}
	if (ackp != NULL) {
		if (pptp_session->snd_nxt == pptp_session->snd_una) {
			ack -= session->proto.pptp.snd_gap;
			pptp_session->ul_snd_una = ack;
		} else {
			/*
			 * There are sending packets they are not acked.
			 * In this situation, (ack - snd_gap) may points
			 * before sending window of userland.  So we don't
			 * update the ack number.
			 */
			ack = pptp_session->ul_snd_una;
		}
		PUTLONG(ack, ackp);
	}

	return (m0);
out_seq:
	pipex_session_log(session, LOG_DEBUG,
	    "Received bad data packet: %s: seq=%u(%u-%u) ack=%u(%u-%u)",
	    reason, seq, pptp_session->rcv_nxt,
	    pptp_session->rcv_nxt + pptp_session->maxwinsz,
	    ack, pptp_session->snd_una,
	    pptp_session->snd_nxt);

	/* FALLTHROUGH */
drop:
	m_freem(m0);
	session->stat.ierrors++;

	return (NULL);
}

struct pipex_session *
pipex_pptp_userland_lookup_session_ipv4(struct mbuf *m0, struct in_addr dst)
{
	struct sockaddr_in sin;

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr = dst;

	return pipex_pptp_userland_lookup_session(m0, sintosa(&sin));
}

#ifdef INET6
struct pipex_session *
pipex_pptp_userland_lookup_session_ipv6(struct mbuf *m0, struct in6_addr dst)
{
	struct sockaddr_in6 sin6;

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_len = sizeof(sin6);
	sin6.sin6_family = AF_INET6;
	in6_recoverscope(&sin6, &dst);

	return pipex_pptp_userland_lookup_session(m0, sin6tosa(&sin6));
}
#endif

Static struct pipex_session *
pipex_pptp_userland_lookup_session(struct mbuf *m0, struct sockaddr *sa)
{
	struct pipex_gre_header gre;
	struct pipex_hash_head *list;
	struct pipex_session *session;
	uint16_t id, flags;

	/* pullup */
	if (m0->m_pkthdr.len < sizeof(gre)) {
		PIPEX_DBG((NULL, LOG_DEBUG,
		    "<%s> packet length is too short", __func__));
		return (NULL);
	}

	/* get flags */
	m_copydata(m0, 0, sizeof(struct pipex_gre_header), (caddr_t)&gre);
	flags = ntohs(gre.flags);

	/* gre version must be '1' */
	if ((flags & PIPEX_GRE_VERMASK) != PIPEX_GRE_VER) {
		PIPEX_DBG((NULL, LOG_DEBUG,
		    "<%s> gre header wrong version.", __func__));
		return (NULL);
	}

	/* gre keys must be present */
	if ((flags & PIPEX_GRE_KFLAG) == 0) {
		PIPEX_DBG((NULL, LOG_DEBUG,
		    "<%s> gre header has no keys.", __func__));
		return (NULL);
	}

	/* lookup pipex session table */
	id = ntohs(gre.call_id);

	list = PIPEX_PEER_ADDR_HASHTABLE(pipex_sockaddr_hash_key(sa));
	LIST_FOREACH(session, list, peer_addr_chain) {
		if (pipex_sockaddr_compar_addr(&session->peer.sa, sa) != 0)
			continue;
		if (session->peer_session_id == id)
			break;
	}
#ifdef PIPEX_DEBUG
	if (session == NULL) {
		PIPEX_DBG((NULL, LOG_DEBUG,
		    "<%s> session not found (,call_id=%d)",
		    __func__, (int)gre.call_id));
	}
#endif
	return (session);
}

/*
 * pipex_pptp_userland_output
 */
struct mbuf *
pipex_pptp_userland_output(struct mbuf *m0, struct pipex_session *session)
{
	int len;
	struct pipex_gre_header *gre, gre0;
	uint16_t flags;
	u_char *cp, *cp0;
	uint32_t val32;

	len = sizeof(struct pipex_gre_header);
	m_copydata(m0, 0, len, (caddr_t)&gre0);
	gre = &gre0;
	flags = ntohs(gre->flags);
	if ((flags & PIPEX_GRE_SFLAG) != 0)
		len += 4;
	if ((flags & PIPEX_GRE_AFLAG) != 0)
		len += 4;

	/* check length */
	PIPEX_PULLUP(m0, len);
	if (m0 == NULL) {
		PIPEX_DBG((session, LOG_DEBUG, "gre header is too short."));
		return (NULL);
	}

	gre = mtod(m0, struct pipex_gre_header *);
	cp = PIPEX_SEEK_NEXTHDR(gre, sizeof(struct pipex_gre_header), u_char *);

	/*
	 * overwrite sequence numbers to adjust a gap between pipex and
	 * userland.
	 */
	if ((flags & PIPEX_GRE_SFLAG) != 0) {
		cp0 = cp;
		GETLONG(val32, cp);
		val32 += session->proto.pptp.snd_gap;
		PUTLONG(val32, cp0);
		session->proto.pptp.snd_nxt++;
	}
	if ((flags & PIPEX_GRE_AFLAG) != 0) {
		cp0 = cp;
		GETLONG(val32, cp);
		val32 += session->proto.pptp.rcv_gap;
		PUTLONG(val32, cp0);
		if (SEQ32_GT(val32, session->proto.pptp.rcv_acked))
			session->proto.pptp.rcv_acked = val32;
	}

	return (m0);
}
#endif /* PIPEX_PPTP */

#ifdef PIPEX_L2TP
/***********************************************************************
 * L2TP support
 ***********************************************************************/
Static void
pipex_l2tp_output(struct mbuf *m0, struct pipex_session *session)
{
	int hlen, plen, datalen;
	struct pipex_l2tp_header *l2tp = NULL;
	struct pipex_l2tp_seq_header *seq = NULL;
	struct udphdr *udp;
	struct ip *ip;
#ifdef INET6
	struct ip6_hdr *ip6;
#endif

	hlen = sizeof(struct pipex_l2tp_header) +
	    ((pipex_session_is_l2tp_data_sequencing_on(session))
		    ? sizeof(struct pipex_l2tp_seq_header) : 0) +
	    sizeof(struct udphdr) +
#ifdef INET6
	    ((session->peer.sin6.sin6_family == AF_INET6)
		    ? sizeof(struct ip6_hdr) : sizeof(struct ip));
#else
	    sizeof(struct ip);
#endif

	datalen = 0;
	if (m0 != NULL) {
		datalen = m0->m_pkthdr.len;
		M_PREPEND(m0, hlen, M_NOWAIT);
		if (m0 == NULL)
			goto drop;
	} else {
		MGETHDR(m0, M_DONTWAIT, MT_DATA);
		if (m0 == NULL)
			goto drop;
		KASSERT(hlen <= MHLEN);
		m0->m_pkthdr.len = m0->m_len = hlen;
	}

#ifdef INET6
	hlen = (session->peer.sin6.sin6_family == AF_INET6)
	    ? sizeof(struct ip6_hdr) : sizeof(struct ip);
#else
	hlen = sizeof(struct ip);
#endif
	plen = datalen + sizeof(struct pipex_l2tp_header) +
	    ((pipex_session_is_l2tp_data_sequencing_on(session))
		    ? sizeof(struct pipex_l2tp_seq_header) : 0);

	l2tp = (struct pipex_l2tp_header *)
	    (mtod(m0, caddr_t) + hlen + sizeof(struct udphdr));
	l2tp->flagsver = PIPEX_L2TP_VER | PIPEX_L2TP_FLAG_LENGTH;
	l2tp->length = htons(plen);
	l2tp->tunnel_id = htons(session->proto.l2tp.peer_tunnel_id);
	l2tp->session_id = htons(session->peer_session_id);
	if (pipex_session_is_l2tp_data_sequencing_on(session)) {
		seq = (struct pipex_l2tp_seq_header *)(l2tp + 1);
		l2tp->flagsver |= PIPEX_L2TP_FLAG_SEQUENCE;
		seq->ns = htons(session->proto.l2tp.ns_nxt);
		session->proto.l2tp.ns_nxt++;
		session->proto.l2tp.ns_gap++;
		session->proto.l2tp.nr_acked = session->proto.l2tp.nr_nxt - 1;
		seq->nr = htons(session->proto.l2tp.nr_acked);
	}
	l2tp->flagsver = htons(l2tp->flagsver);

	plen += sizeof(struct udphdr);
	udp = (struct udphdr *)(mtod(m0, caddr_t) + hlen);
	udp->uh_sport = session->local.sin6.sin6_port;
	udp->uh_dport = session->peer.sin6.sin6_port;
	udp->uh_ulen = htons(plen);
	udp->uh_sum = 0;

	m0->m_pkthdr.csum_flags |= M_UDP_CSUM_OUT;
	m0->m_pkthdr.ph_ifidx = session->pipex_iface->ifnet_this->if_index;
#if NPF > 0
	pf_pkt_addr_changed(m0);
#endif
	switch (session->peer.sin6.sin6_family) {
	case AF_INET:
		ip = mtod(m0, struct ip *);
		ip->ip_p = IPPROTO_UDP;
		ip->ip_src = session->local.sin4.sin_addr;
		ip->ip_dst = session->peer.sin4.sin_addr;
		ip->ip_len = htons(hlen + plen);
		ip->ip_ttl = MAXTTL;
		ip->ip_tos = 0;
		ip->ip_off = 0;

		if (ip_output(m0, NULL, NULL, 0, NULL, NULL,
		    session->proto.l2tp.ipsecflowinfo) != 0) {
			PIPEX_DBG((session, LOG_DEBUG, "ip_output failed."));
			goto drop;
		}
		break;
#ifdef INET6
	case AF_INET6:
		ip6 = mtod(m0, struct ip6_hdr *);

		ip6->ip6_flow = 0;
		ip6->ip6_vfc &= ~IPV6_VERSION_MASK;
		ip6->ip6_vfc |= IPV6_VERSION;
		ip6->ip6_nxt = IPPROTO_UDP;
		ip6->ip6_src = session->local.sin6.sin6_addr;
		(void)in6_embedscope(&ip6->ip6_dst,
		    &session->peer.sin6, NULL);
		/* ip6->ip6_plen will be filled in ip6_output. */

		if (ip6_output(m0, NULL, NULL, 0, NULL, NULL) != 0) {
			PIPEX_DBG((session, LOG_DEBUG, "ip6_output failed."));
			goto drop;
		}
		break;
#endif
	}
	udpstat_inc(udps_opackets);

	if (datalen > 0) {	/* network layer only */
		/* countup statistics */
		session->stat.opackets++;
		session->stat.obytes += datalen;
	}

	return;
drop:
	session->stat.oerrors++;
}

struct pipex_session *
pipex_l2tp_lookup_session(struct mbuf *m0, int off)
{
	struct pipex_session *session;
	uint16_t flags, session_id, ver;
	u_char *cp, buf[PIPEX_L2TP_MINLEN];

	if (m0->m_pkthdr.len < off + PIPEX_L2TP_MINLEN) {
		PIPEX_DBG((NULL, LOG_DEBUG,
		    "<%s> packet length is too short", __func__));
		goto not_ours;
	}

	/* get first 16bits of L2TP */
	m_copydata(m0, off, sizeof(buf), buf);
	cp = buf;
	GETSHORT(flags, cp);
	ver = flags & PIPEX_L2TP_VER_MASK;

	/* l2tp version must be '2' */
	if (ver != PIPEX_L2TP_VER) {
		PIPEX_DBG((NULL, LOG_DEBUG,
		    "<%s> l2tp header wrong version %u.", __func__, ver));
		goto not_ours;
	}
	if ((flags & PIPEX_L2TP_FLAG_TYPE) != 0)
		goto not_ours;

	if (flags & PIPEX_L2TP_FLAG_LENGTH)
		cp += 2;			/* skip length field */
	cp += 2;				/* skip tunnel-id field */
	GETSHORT(session_id, cp);		/* get session-id field */

	/* lookup pipex session table */
	session = pipex_lookup_by_session_id(PIPEX_PROTO_L2TP, session_id);
#ifdef PIPEX_DEBUG
	if (session == NULL) {
		PIPEX_DBG((NULL, LOG_DEBUG,
		    "<%s> session not found (id=%d)", __func__, session_id));
		goto not_ours;
	}
#endif

	return (session);

not_ours:
	return (NULL);
}

struct mbuf *
pipex_l2tp_input(struct mbuf *m0, int off0, struct pipex_session *session,
    uint32_t ipsecflowinfo)
{
	struct pipex_l2tp_session *l2tp_session;
	int length, offset, hlen, nseq;
	u_char *cp, *nsp, *nrp;
	uint16_t flags, ns = 0, nr = 0;
	int rewind = 0;

	length = offset = ns = nr = 0;
	l2tp_session = &session->proto.l2tp;
	l2tp_session->ipsecflowinfo = ipsecflowinfo;
	nsp = nrp = NULL;

	m_copydata(m0, off0, sizeof(flags), (caddr_t)&flags);

	flags = ntohs(flags) & PIPEX_L2TP_FLAG_MASK;
	KASSERT((flags & PIPEX_L2TP_FLAG_TYPE) == 0);

	hlen = 2;				/* flags and version fields */
	if (flags & PIPEX_L2TP_FLAG_LENGTH)	/* length */
		hlen += 2;
	hlen += 4;				/* tunnel-id and session-id */
	if (flags & PIPEX_L2TP_FLAG_SEQUENCE)	/* ns and nr */
		hlen += 4;
	if (flags & PIPEX_L2TP_FLAG_OFFSET)	/* offset */
		hlen += 2;

	PIPEX_PULLUP(m0, off0 + hlen);
	if (m0  == NULL)
		goto drop;

	cp = mtod(m0, u_char *) + off0;
	cp += 2;	/* flags and version */
	if (flags & PIPEX_L2TP_FLAG_LENGTH)
		GETSHORT(length, cp);
	else
		length = m0->m_pkthdr.len - off0;
	cp += 4;	/* skip tunnel-id and session-id field */

	/* pullup for seek sequences in header */
	nseq = 0;
	if (flags & PIPEX_L2TP_FLAG_SEQUENCE) {
		nsp = cp;
		GETSHORT(ns, cp);
		nrp = cp;
		GETSHORT(nr, cp);

		nr++;
		if (SEQ16_GT(nr, l2tp_session->ns_una) &&
		    SEQ16_LE(nr, l2tp_session->ns_nxt))
			/* update 'ns_una' only if the ns is in valid range */
			l2tp_session->ns_una = nr;
		if (SEQ16_LT(ns, l2tp_session->nr_nxt)) {
			rewind = 1;
			if (SEQ16_LT(ns,
			    l2tp_session->nr_nxt - PIPEX_REWIND_LIMIT))
				goto out_seq;
		}

		ns++;
		nseq = SEQ16_SUB(ns, l2tp_session->nr_nxt);
		if (!rewind)
			l2tp_session->nr_nxt = ns;
	}
	if (flags & PIPEX_L2TP_FLAG_OFFSET)
		GETSHORT(offset, cp);

	length -= hlen + offset;
	hlen += off0 + offset;
	if ((m0 = pipex_common_input(session, m0, hlen, length, 1)) == NULL) {
		/* ok,  The packet is for PIPEX */
		if (!rewind)
			session->proto.l2tp.nr_gap += nseq;
		return (NULL);
	}

	if (rewind)
		goto out_seq;

	/*
	 * overwrite sequence numbers to adjust a gap between pipex and
	 * userland.
	 */
	if (flags & PIPEX_L2TP_FLAG_SEQUENCE) {
		--ns; --nr;	/* revert original values */
		ns -= l2tp_session->nr_gap;
		PUTSHORT(ns, nsp);

		if (l2tp_session->ns_nxt == l2tp_session->ns_una) {
			nr -= l2tp_session->ns_gap;
			l2tp_session->ul_ns_una = nr;
		} else {
			/*
			 * There are sending packets they are not acked.
			 * In this situation, (ack - snd_gap) may points
			 * before sending window of userland.  So we don't
			 * update the ack number.
			 */
			nr = l2tp_session->ul_ns_una;
		}
		PUTSHORT(nr, nrp);
	}

	return (m0);
out_seq:
	pipex_session_log(session, LOG_DEBUG,
	    "Received bad data packet: out of sequence: seq=%u(%u-) "
	    "ack=%u(%u-%u)", ns, l2tp_session->nr_nxt, nr, l2tp_session->ns_una,
	    l2tp_session->ns_nxt);

	/* FALLTHROUGH */
drop:
	m_freem(m0);
	session->stat.ierrors++;

	return (NULL);
}

struct pipex_session *
pipex_l2tp_userland_lookup_session_ipv4(struct mbuf *m0, struct in_addr dst)
{
	struct sockaddr_in sin;

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;
	sin.sin_addr = dst;

	return pipex_l2tp_userland_lookup_session(m0, sintosa(&sin));
}

#ifdef INET6
struct pipex_session *
pipex_l2tp_userland_lookup_session_ipv6(struct mbuf *m0, struct in6_addr dst)
{
	struct sockaddr_in6 sin6;

	memset(&sin6, 0, sizeof(sin6));
	sin6.sin6_len = sizeof(sin6);
	sin6.sin6_family = AF_INET6;
	in6_recoverscope(&sin6, &dst);

	return pipex_l2tp_userland_lookup_session(m0, sin6tosa(&sin6));
}
#endif

struct pipex_session *
pipex_l2tp_userland_lookup_session(struct mbuf *m0, struct sockaddr *sa)
{
	struct pipex_l2tp_header l2tp;
	struct pipex_hash_head *list;
	struct pipex_session *session;
	uint16_t session_id, tunnel_id, flags;

	if (sa->sa_family != AF_INET && sa->sa_family != AF_INET6)
		return (NULL);

	/* pullup */
	if (m0->m_pkthdr.len < sizeof(l2tp)) {
		PIPEX_DBG((NULL, LOG_DEBUG,
		    "<%s> packet length is too short", __func__));
		return (NULL);
	}

	/* get flags */
	m_copydata(m0, 0, sizeof(l2tp), (caddr_t)&l2tp);
	flags = ntohs(l2tp.flagsver);

	/* l2tp version must be '2' */
	if ((flags & PIPEX_L2TP_VER_MASK) != PIPEX_L2TP_VER) {
		PIPEX_DBG((NULL, LOG_DEBUG,
		    "<%s> l2tp header wrong version.", __func__));
		return (NULL);
	}
	/* We need L2TP data messages only */
	if ((flags & PIPEX_L2TP_FLAG_TYPE) != 0)
		return (NULL);
	/* No need to hook packets that don't have the sequence field */
	if ((flags & PIPEX_L2TP_FLAG_SEQUENCE) == 0)
		return (NULL);

	session_id = ntohs(l2tp.session_id);
	tunnel_id = ntohs(l2tp.tunnel_id);

	list = PIPEX_PEER_ADDR_HASHTABLE(pipex_sockaddr_hash_key(sa));
	LIST_FOREACH(session, list, peer_addr_chain) {
		if (pipex_sockaddr_compar_addr(&session->peer.sa, sa) != 0)
			continue;
		if (session->proto.l2tp.peer_tunnel_id != tunnel_id)
			continue;
		if (session->peer_session_id == session_id)
			break;
	}
#ifdef PIPEX_DEBUG
	if (session == NULL) {
		PIPEX_DBG((NULL, LOG_DEBUG, "<%s> session not found "
		    "(tunnel_id=%d, session_id=%d)", __func__,
		    tunnel_id, session_id));
	}
#endif

	return (session);
}

struct mbuf *
pipex_l2tp_userland_output(struct mbuf *m0, struct pipex_session *session)
{
	struct pipex_l2tp_header *l2tp;
	struct pipex_l2tp_seq_header *seq;
	uint16_t ns, nr;

	/* check length */
	PIPEX_PULLUP(m0, sizeof(struct pipex_l2tp_header) +
	    sizeof(struct pipex_l2tp_seq_header));
	if (m0 == NULL)
		return (NULL);

	l2tp = mtod(m0, struct pipex_l2tp_header *);
	KASSERT(ntohs(l2tp->flagsver) & PIPEX_L2TP_FLAG_SEQUENCE);

	/*
	 * overwrite sequence numbers to adjust a gap between pipex and
	 * userland.
	 */
		seq = (struct pipex_l2tp_seq_header *)(l2tp + 1);
		ns = ntohs(seq->ns);
		nr = ntohs(seq->nr);

		ns += session->proto.l2tp.ns_gap;
		seq->ns = htons(ns);
		session->proto.l2tp.ns_nxt++;

		nr += session->proto.l2tp.nr_gap;
		seq->nr = htons(nr);
		if (SEQ16_GT(nr, session->proto.l2tp.nr_acked))
			session->proto.l2tp.nr_acked = nr;

	return (m0);
}
#endif /* PIPEX_L2TP */

#ifdef PIPEX_MPPE
/**********************************************************************
 * MPPE
 ***********************************************************************/
#define	PIPEX_COHERENCY_CNT_MASK		0x0fff
Static void
pipex_mppe_init(struct pipex_mppe *mppe, int stateless, int keylenbits,
    u_char *master_key, int has_oldkey)
{
	memset(mppe, 0, sizeof(struct pipex_mppe));
	if (stateless)
		mppe->stateless = 1;
	if (has_oldkey)
		mppe->old_session_keys =
		    pool_get(&mppe_key_pool, PR_WAITOK);
	else
		mppe->old_session_keys = NULL;
	memcpy(mppe->master_key, master_key, sizeof(mppe->master_key));

	mppe->keylenbits = keylenbits;
	switch (keylenbits) {
	case 40:
	case 56:
		mppe->keylen = 8;
		break;
	case 128:
		mppe->keylen = 16;
		break;
	}

	GetNewKeyFromSHA(mppe->master_key, mppe->master_key, mppe->keylen,
	    mppe->session_key);
	pipex_mppe_reduce_key(mppe);
	pipex_mppe_setkey(mppe);
}

void
pipex_session_init_mppe_recv(struct pipex_session *session, int stateless,
    int keylenbits, u_char *master_key)
{
	pipex_mppe_init(&session->mppe_recv, stateless, keylenbits,
	    master_key, stateless);
	session->ppp_flags |= PIPEX_PPP_MPPE_ACCEPTED;
}

void
pipex_session_init_mppe_send(struct pipex_session *session, int stateless,
    int keylenbits, u_char *master_key)
{
	pipex_mppe_init(&session->mppe_send, stateless, keylenbits,
	    master_key, 0);
	session->ppp_flags |= PIPEX_PPP_MPPE_ENABLED;
}

#include <crypto/sha1.h>

static u_char SHAPad1[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
}, SHAPad2[] = {
	0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
	0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
	0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
	0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
	0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
};

Static void
GetNewKeyFromSHA(u_char *StartKey, u_char *SessionKey, int SessionKeyLength,
    u_char *InterimKey)
{
	u_char Digest[20];
	SHA1_CTX Context;

	SHA1Init(&Context);
	SHA1Update(&Context, StartKey, SessionKeyLength);
	SHA1Update(&Context, SHAPad1, 40);
	SHA1Update(&Context, SessionKey, SessionKeyLength);
	SHA1Update(&Context, SHAPad2, 40);
	SHA1Final(Digest, &Context);

	memcpy(InterimKey, Digest, SessionKeyLength);
}

Static void
pipex_mppe_reduce_key(struct pipex_mppe *mppe)
{
	switch (mppe->keylenbits) {
	case 40:
		mppe->session_key[0] = 0xd1;
		mppe->session_key[1] = 0x26;
		mppe->session_key[2] = 0x9e;
		break;
	case 56:
		mppe->session_key[0] = 0xd1;
		break;
	}
}

Static void
mppe_key_change(struct pipex_mppe *mppe)
{
	u_char interim[16];
	struct rc4_ctx keychg;

	memset(&keychg, 0, sizeof(keychg));

	GetNewKeyFromSHA(mppe->master_key, mppe->session_key, mppe->keylen,
	    interim);

	rc4_keysetup(&keychg, interim, mppe->keylen);
	rc4_crypt(&keychg, interim, mppe->session_key, mppe->keylen);

	pipex_mppe_reduce_key(mppe);

	if (mppe->old_session_keys) {
		int idx = mppe->coher_cnt & PIPEX_MPPE_OLDKEYMASK;
		memcpy(mppe->old_session_keys[idx],
		    mppe->session_key, PIPEX_MPPE_KEYLEN);
	}
}

Static void
pipex_mppe_input(struct mbuf *m0, struct pipex_session *session)
{
	int pktloss, encrypt, flushed, m, n, len;
	struct pipex_mppe *mppe;
	uint16_t coher_cnt;
	struct mbuf *m1;
	u_char *cp;
	int rewind = 0;

	/* pullup */
	PIPEX_PULLUP(m0, sizeof(coher_cnt));
	if (m0 == NULL)
		goto drop;

	mppe = &session->mppe_recv;
	/* get header information */
	cp = mtod(m0, u_char *);
	GETSHORT(coher_cnt, cp);
	flushed = ((coher_cnt & 0x8000) != 0) ? 1 : 0;
	encrypt = ((coher_cnt & 0x1000) != 0) ? 1 : 0;
	coher_cnt &= PIPEX_COHERENCY_CNT_MASK;
	pktloss = 0;

	PIPEX_MPPE_DBG((session, LOG_DEBUG, "in coher_cnt=%03x %s%s",
	    mppe->coher_cnt, (flushed) ? "[flushed]" : "",
	    (encrypt) ? "[encrypt]" : ""));

	if (encrypt == 0) {
		pipex_session_log(session, LOG_DEBUG,
		    "Received unexpected MPPE packet.(no ecrypt)");
		goto drop;
	}

	/* adjust mbuf */
	m_adj(m0, sizeof(coher_cnt));

	/*
	 * L2TP data session may be used without sequencing, PPP frames may
	 * arrive in disorder.  The 'coherency counter' of MPPE detects such
	 * situations, but we cannot distinguish between 'disorder' and
	 * 'packet loss' exactly.
	 *
	 * When 'coherency counter' detects lost packets greater than
	 * (4096 - 256), we treat as 'disorder' otherwise treat as
	 * 'packet loss'.
	 */
    {
	int coher_cnt0;

	coher_cnt0 = coher_cnt;
	if (coher_cnt < mppe->coher_cnt)
		coher_cnt0 += 0x1000;
	if (coher_cnt0 - mppe->coher_cnt > 0x0f00) {
		if (!mppe->stateless ||
		    coher_cnt0 - mppe->coher_cnt
		    <= 0x1000 - PIPEX_MPPE_NOLDKEY) {
			pipex_session_log(session, LOG_DEBUG,
			    "Workaround the out-of-sequence PPP framing problem: "
			    "%d => %d", mppe->coher_cnt, coher_cnt);
			goto drop;
		}
		rewind = 1;
	}
    }

	if (mppe->stateless != 0) {
		if (!rewind) {
			mppe_key_change(mppe);
			while (mppe->coher_cnt != coher_cnt) {
				mppe->coher_cnt++;
				mppe->coher_cnt &= PIPEX_COHERENCY_CNT_MASK;
				mppe_key_change(mppe);
				pktloss++;
			}
		}
		pipex_mppe_setoldkey(mppe, coher_cnt);
	} else {
		if (flushed) {
			if (coher_cnt < mppe->coher_cnt) {
				coher_cnt += 0x1000;
			}
			pktloss += coher_cnt - mppe->coher_cnt;
			m = mppe->coher_cnt / 256;
			n = coher_cnt / 256;
			while (m++ < n)
				mppe_key_change(mppe);

			coher_cnt &= PIPEX_COHERENCY_CNT_MASK;
			mppe->coher_cnt = coher_cnt;
		} else if (mppe->coher_cnt != coher_cnt) {
			/* Send CCP ResetReq */
			PIPEX_DBG((session, LOG_DEBUG, "CCP SendResetReq"));
			pipex_ccp_output(session, CCP_RESETREQ,
			    session->ccp_id++);
			goto drop;
		}
		if ((coher_cnt & 0xff) == 0xff) {
			mppe_key_change(mppe);
			flushed = 1;
		}
		if (flushed)
			pipex_mppe_setkey(mppe);
	}

	if (pktloss > 1000) {
		pipex_session_log(session, LOG_DEBUG,
		    "%d packets loss.", pktloss);
	}

	/* decrypt ppp payload */
	for (m1 = m0; m1; m1 = m1->m_next) {
		cp = mtod(m1, u_char *);
		len = m1->m_len;
		pipex_mppe_crypt(mppe, len, cp, cp);
	}

	if (!rewind) {
		/* update coher_cnt */
		mppe->coher_cnt++;
		mppe->coher_cnt &= PIPEX_COHERENCY_CNT_MASK;
	}
	if (m0->m_pkthdr.len < PIPEX_PPPMINLEN)
		goto drop;

	pipex_ppp_input(m0, session, 1);

	return;
drop:
	m_freem(m0);
	session->stat.ierrors++;
}

Static void
pipex_mppe_output(struct mbuf *m0, struct pipex_session *session,
    uint16_t protocol)
{
	int encrypt, flushed, len;
	struct mppe_header {
		uint16_t coher_cnt;
		uint16_t protocol;
	} __packed *hdr;
	u_char *cp;
	struct pipex_mppe *mppe;
	struct mbuf *m;

	mppe = &session->mppe_send;

	/*
	 * create a deep-copy if the mbuf has a shared mbuf cluster.
	 * this is required to handle cases of tcp retransmition.
	 */
	for (m = m0; m != NULL; m = m->m_next) {
		if (M_READONLY(m)) {
			m = m_dup_pkt(m0, max_linkhdr, M_NOWAIT);
			m_freem(m0);
			if (m == NULL)
				goto drop;
			m0 = m;
			break;
		}
	}
	/* prepend mppe header */
	M_PREPEND(m0, sizeof(struct mppe_header), M_NOWAIT);
	if (m0 == NULL)
		goto drop;
	hdr = mtod(m0, struct mppe_header *);
	hdr->protocol = protocol;

	/* check coherency counter */
	flushed = 0;
	encrypt = 1;

	if (mppe->stateless != 0) {
		flushed = 1;
		mppe_key_change(mppe);
	} else {
		if ((mppe->coher_cnt % 0x100) == 0xff) {
			flushed = 1;
			mppe_key_change(mppe);
		} else if (mppe->resetreq != 0) {
			flushed = 1;
			mppe->resetreq = 0;
		}
	}

	if (flushed)
		pipex_mppe_setkey(mppe);

	PIPEX_MPPE_DBG((session, LOG_DEBUG, "out coher_cnt=%03x %s%s",
	    mppe->coher_cnt, (flushed) ? "[flushed]" : "",
	    (encrypt) ? "[encrypt]" : ""));

	/* setup header information */
	hdr->coher_cnt = (mppe->coher_cnt++) & PIPEX_COHERENCY_CNT_MASK;
	hdr->coher_cnt &= PIPEX_COHERENCY_CNT_MASK;
	if (flushed)
		hdr->coher_cnt |= 0x8000;
	if (encrypt)
		hdr->coher_cnt |= 0x1000;

	hdr->protocol = htons(hdr->protocol);
	hdr->coher_cnt = htons(hdr->coher_cnt);

	/* encrypt chain */
	for (m = m0; m; m = m->m_next) {
		cp = mtod(m, u_char *);
		len = m->m_len;
		if (m == m0 && len > offsetof(struct mppe_header, protocol)) {
			len -= offsetof(struct mppe_header, protocol);
			cp += offsetof(struct mppe_header, protocol);
		}
		pipex_mppe_crypt(mppe, len, cp, cp);
	}

	pipex_ppp_output(m0, session, PPP_COMP);

	return;
drop:
	session->stat.oerrors++;
}

Static void
pipex_ccp_input(struct mbuf *m0, struct pipex_session *session)
{
	u_char *cp;
	int code, id, len;

	if (m0->m_pkthdr.len < PPP_HDRLEN)
		goto drop;
	if ((m0 = m_pullup(m0, PPP_HDRLEN)) == NULL)
		goto drop;

	cp = mtod(m0, u_char *);
	GETCHAR(code, cp);
	GETCHAR(id, cp);
	GETSHORT(len, cp);

	switch (code) {
	case CCP_RESETREQ:
		PIPEX_DBG((session, LOG_DEBUG, "CCP RecvResetReq"));
		session->mppe_send.resetreq = 1;
#ifndef PIPEX_NO_CCP_RESETACK
		PIPEX_DBG((session, LOG_DEBUG, "CCP SendResetAck"));
		pipex_ccp_output(session, CCP_RESETACK, id);
#endif
		/* ignore error */
		break;
	case CCP_RESETACK:
		PIPEX_DBG((session, LOG_DEBUG, "CCP RecvResetAck"));
		break;
	default:
		PIPEX_DBG((session, LOG_DEBUG, "CCP Recv code=%d", code));
		goto drop;
	}
	m_freem(m0);

	return;
drop:
	m_freem(m0);
	session->stat.ierrors++;
}

Static int
pipex_ccp_output(struct pipex_session *session, int code, int id)
{
	u_char *cp;
	struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		session->stat.oerrors++;
		return (1);
	}
	m->m_pkthdr.len = m->m_len = 4;
	cp = mtod(m, u_char *);
	PUTCHAR(code, cp);
	PUTCHAR(id, cp);
	PUTSHORT(4, cp);

	pipex_ppp_output(m, session, PPP_CCP);

	return (0);
}
#endif
/***********************************************************************
 * Miscellaneous fuctions
 ***********************************************************************/
/* adapted from FreeBSD:src/usr.sbin/ppp/tcpmss.c */
/*
 * Copyright (c) 2000 Ruslan Ermilov and Brian Somers <brian@Awfulhak.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/usr.sbin/ppp/tcpmss.c,v 1.1.4.3 2001/07/19 11:39:54 brian Exp $
 */
#define TCP_OPTLEN_IN_SEGMENT	12	/* timestamp option and padding */
#define MAXMSS(mtu) (mtu - sizeof(struct ip) - sizeof(struct tcphdr) - \
    TCP_OPTLEN_IN_SEGMENT)
/*
 * The following macro is used to update an internet checksum.  "acc" is a
 * 32-bit accumulation of all the changes to the checksum (adding in old
 * 16-bit words and subtracting out new words), and "cksum" is the checksum
 * value to be updated.
 */
#define ADJUST_CHECKSUM(acc, cksum) {			\
	acc += cksum;					\
	if (acc < 0) {					\
		acc = -acc;				\
		acc = (acc >> 16) + (acc & 0xffff);	\
		acc += acc >> 16;			\
		cksum = (u_short) ~acc;			\
	} else {					\
		acc = (acc >> 16) + (acc & 0xffff);	\
		acc += acc >> 16;			\
		cksum = (u_short) acc;			\
	}						\
}

/*
 * Rewrite max-segment-size TCP option to avoid PMTU blackhole issues.
 * The mtu parameter should be the MTU bottleneck (as far as we know)
 * on the link between the source and the destination.
 */
Static struct mbuf *
adjust_tcp_mss(struct mbuf *m0, int mtu)
{
	int opt, optlen, acc, mss, maxmss, lpktp;
	struct ip *pip;
	struct tcphdr *th;
	u_char *pktp, *mssp;
	u_int16_t ip_off;

	lpktp = sizeof(struct ip) + sizeof(struct tcphdr) + PIPEX_TCP_OPTLEN;
	lpktp = MIN(lpktp, m0->m_pkthdr.len);

	PIPEX_PULLUP(m0, lpktp);
	if (m0 == NULL)
		goto drop;

	pktp = mtod(m0, char *);
	pip = (struct ip *)pktp;
	ip_off = ntohs(pip->ip_off);

	/* Non TCP or fragmented packet must not have a MSS option */
	if (pip->ip_p != IPPROTO_TCP ||
	    (ip_off & IP_MF) != 0 || (ip_off & IP_OFFMASK) != 0)
		goto handled;

	pktp += pip->ip_hl << 2;
	lpktp -= pip->ip_hl << 2;

	/* packet is broken */
	if (sizeof(struct tcphdr) > lpktp)
		goto drop;
	th = (struct tcphdr *)pktp;

	/*
	 * As RFC 973, a MSS field must only be sent in the initial
	 * connection request(it must be with SYN).
	 */
	if ((th->th_flags & TH_SYN) == 0)
		goto handled;

	lpktp = MIN(th->th_off << 4, lpktp);

	pktp += sizeof(struct tcphdr);
	lpktp -= sizeof(struct tcphdr);
	while (lpktp >= TCPOLEN_MAXSEG) {
		GETCHAR(opt, pktp);
		switch (opt) {
		case TCPOPT_MAXSEG:
			GETCHAR(optlen, pktp);
			mssp = pktp;		/* mss place holder */
			GETSHORT(mss, pktp);
			maxmss = MAXMSS(mtu);
			if (mss > maxmss) {
				PIPEX_DBG((NULL, LOG_DEBUG,
				    "change tcp-mss %d => %d", mss, maxmss));
				PUTSHORT(maxmss, mssp);
				acc = htons(mss);
				acc -= htons(maxmss);
				ADJUST_CHECKSUM(acc, th->th_sum);
			}
			goto handled;
			/* NOTREACHED */
		case TCPOPT_EOL:
			goto handled;
			/* NOTREACHED */
		case TCPOPT_NOP:
			lpktp--;
			break;
		default:
			GETCHAR(optlen, pktp);
			if (optlen < 2)	/* packet is broken */
				goto drop;
			pktp += optlen - 2;
			lpktp -= optlen;
			break;
		}
	}

handled:
	return (m0);

drop:
	m_freem(m0);
	return (NULL);
}

/*
 *  Check whether a packet should reset idle timer
 *  Returns 1 to don't reset timer (i.e. the packet is "idle" packet)
 */
Static struct mbuf *
ip_is_idle_packet(struct mbuf *m0, int *ris_idle)
{
	u_int16_t ip_off;
	const struct udphdr *uh;
	struct ip *pip;
	int len;

	/* pullup ip header */
	len = sizeof(struct ip);
	PIPEX_PULLUP(m0, len);
	if (m0 == NULL)
		goto error;
	pip = mtod(m0, struct ip *);

	/*
	 * the packet which fragmentations was not the idle packet.
	 */
	ip_off = ntohs(pip->ip_off);
	if ((ip_off & IP_MF) || ((ip_off & IP_OFFMASK) != 0))
		goto is_active;

	switch (pip->ip_p) {
	case IPPROTO_IGMP:
		goto is_active;
	case IPPROTO_ICMP:
		len = pip->ip_hl * 4 + 8;
		PIPEX_PULLUP(m0, len);
		if (m0 == NULL)
			goto error;
		pip = mtod(m0, struct ip *);

		switch (((unsigned char *) pip)[pip->ip_hl * 4]) {
		case 0:	/* Echo Reply */
		case 8:	/* Echo Request */
			goto is_active;
		default:
			goto is_idle;
		}

	case IPPROTO_UDP:
	case IPPROTO_TCP:
		len = pip->ip_hl * 4 + sizeof(struct udphdr);
		PIPEX_PULLUP(m0, len);
		if (m0 == NULL)
			goto error;
		pip = mtod(m0, struct ip *);
		uh = (struct udphdr *)(mtod(m0, caddr_t) + pip->ip_hl * 4);

		switch (ntohs(uh->uh_sport)) {
		case 53:	/* DOMAIN */
		case 67:	/* BOOTPS */
		case 68:	/* BOOTPC */
		case 123:	/* NTP */
		case 137:	/* NETBIOS-NS */
		case 520:	/* RIP */
			goto is_idle;
		}
		switch (ntohs(uh->uh_dport)) {
		case 53:	/* DOMAIN */
		case 67:	/* BOOTPS */
		case 68:	/* BOOTPC */
		case 123:	/* NTP */
		case 137:	/* NETBIOS-NS */
		case 520:	/* RIP */
			goto is_idle;
		}
		goto is_active;
	default:
		goto is_active;
	}

is_active:
	*ris_idle = 0;
	return (m0);

is_idle:
	*ris_idle = 1;
	return (m0);

error:
	return (NULL);
}

Static void
pipex_session_log(struct pipex_session *session, int prio, const char *fmt, ...)
{
	char logbuf[1024];
	va_list ap;

	logpri(prio);
	if (session != NULL) {
		addlog("pipex: ppp=%d iface=%s protocol=%s id=%d ",
		    session->ppp_id, session->pipex_iface->ifnet_this->if_xname,
		    (session->protocol == PIPEX_PROTO_PPPOE)? "PPPoE" :
		    (session->protocol == PIPEX_PROTO_PPTP)? "PPTP" :
		    (session->protocol == PIPEX_PROTO_L2TP) ? "L2TP" :
		    "Unknown", session->session_id);
	} else
		addlog("pipex: ");

	va_start(ap, fmt);
	vsnprintf(logbuf, sizeof(logbuf), fmt, ap);
	va_end(ap);
	addlog("%s\n", logbuf);
}

Static uint32_t
pipex_sockaddr_hash_key(struct sockaddr *sa)
{
	switch (sa->sa_family) {
	case AF_INET:
		return ntohl(satosin(sa)->sin_addr.s_addr);
	case AF_INET6:
		return ntohl(satosin6(sa)->sin6_addr.s6_addr32[3]);
	}
	panic("pipex_sockaddr_hash_key: unknown address family");
	return (0);
}

/*
 * Compare struct sockaddr_in{,6} with the address only.
 * The port number is not covered.
 */
Static int
pipex_sockaddr_compar_addr(struct sockaddr *a, struct sockaddr *b)
{
	int cmp;

	cmp = b->sa_family - a->sa_family;
	if (cmp != 0)
		return cmp;
	switch (a->sa_family) {
	case AF_INET:
		return (satosin(b)->sin_addr.s_addr -
		    satosin(a)->sin_addr.s_addr);
	case AF_INET6:
		cmp = (satosin6(b)->sin6_scope_id - satosin6(a)->sin6_scope_id);
		if (cmp != 0)
			return cmp;
		return (memcmp(&satosin6(a)->sin6_addr,
		    &satosin6(b)->sin6_addr,
		    sizeof(struct in6_addr)));
	}
	panic("pipex_sockaddr_compar_addr: unknown address family");

	return (-1);
}

Static inline int
pipex_mppe_setkey(struct pipex_mppe *mppe)
{
	rc4_keysetup(&mppe->rc4ctx, mppe->session_key, mppe->keylen);

	return (0);
}

Static inline int
pipex_mppe_setoldkey(struct pipex_mppe *mppe, uint16_t coher_cnt)
{
	KASSERT(mppe->old_session_keys != NULL);

	rc4_keysetup(&mppe->rc4ctx,
	    mppe->old_session_keys[coher_cnt & PIPEX_MPPE_OLDKEYMASK],
	    mppe->keylen);

	return (0);
}

Static inline void
pipex_mppe_crypt(struct pipex_mppe *mppe, int len, u_char *indata,
    u_char *outdata)
{
	rc4_crypt(&mppe->rc4ctx, indata, outdata, len);
}

int
pipex_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	switch (name[0]) {
	case PIPEXCTL_ENABLE:
		if (namelen != 1)
			return (ENOTDIR);
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &pipex_enable));
	case PIPEXCTL_INQ:
	        return (sysctl_mq(name + 1, namelen - 1,
		    oldp, oldlenp, newp, newlen, &pipexinq));
	case PIPEXCTL_OUTQ:
	        return (sysctl_mq(name + 1, namelen - 1,
		    oldp, oldlenp, newp, newlen, &pipexoutq));
	default:
		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}
