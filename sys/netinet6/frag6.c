/*	$OpenBSD: frag6.c,v 1.58 2014/12/05 15:50:04 mpi Exp $	*/
/*	$KAME: frag6.c,v 1.40 2002/05/27 21:40:31 itojun Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
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
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#include <netinet/ip.h>		/* for ECN definitions */

/*
 * Define it to get a correct behavior on per-interface statistics.
 * You will need to perform an extra routing table lookup, per fragment,
 * to do it.  This may, or may not be, a performance hit.
 */
#define IN6_IFSTAT_STRICT

void frag6_freef(struct ip6q *);

static int ip6q_locked;
u_int frag6_nfragpackets;
u_int frag6_nfrags;
TAILQ_HEAD(ip6q_head, ip6q) frag6_queue;	/* ip6 reassemble queue */

static __inline int ip6q_lock_try(void);
static __inline void ip6q_unlock(void);

static __inline int
ip6q_lock_try()
{
	int s;

	/* Use splvm() due to mbuf allocation. */
	s = splvm();
	if (ip6q_locked) {
		splx(s);
		return (0);
	}
	ip6q_locked = 1;
	splx(s);
	return (1);
}

static __inline void
ip6q_unlock()
{
	int s;

	s = splvm();
	ip6q_locked = 0;
	splx(s);
}

#ifdef DIAGNOSTIC
#define	IP6Q_LOCK()							\
do {									\
	if (ip6q_lock_try() == 0) {					\
		printf("%s:%d: ip6q already locked\n", __FILE__, __LINE__); \
		panic("ip6q_lock");					\
	}								\
} while (0)
#define	IP6Q_LOCK_CHECK()						\
do {									\
	if (ip6q_locked == 0) {						\
		printf("%s:%d: ip6q lock not held\n", __FILE__, __LINE__); \
		panic("ip6q lock check");				\
	}								\
} while (0)
#else
#define	IP6Q_LOCK()		(void) ip6q_lock_try()
#define	IP6Q_LOCK_CHECK()	/* nothing */
#endif

#define	IP6Q_UNLOCK()		ip6q_unlock()

/*
 * Initialise reassembly queue and fragment identifier.
 */
void
frag6_init(void)
{

	TAILQ_INIT(&frag6_queue);
}

/*
 * In RFC2460, fragment and reassembly rule do not agree with each other,
 * in terms of next header field handling in fragment header.
 * While the sender will use the same value for all of the fragmented packets,
 * receiver is suggested not to check the consistency.
 *
 * fragment rule (p20):
 *	(2) A Fragment header containing:
 *	The Next Header value that identifies the first header of
 *	the Fragmentable Part of the original packet.
 *		-> next header field is same for all fragments
 *
 * reassembly rule (p21):
 *	The Next Header field of the last header of the Unfragmentable
 *	Part is obtained from the Next Header field of the first
 *	fragment's Fragment header.
 *		-> should grab it from the first fragment only
 *
 * The following note also contradicts with fragment rule - noone is going to
 * send different fragment with different next header field.
 *
 * additional note (p22):
 *	The Next Header values in the Fragment headers of different
 *	fragments of the same original packet may differ.  Only the value
 *	from the Offset zero fragment packet is used for reassembly.
 *		-> should grab it from the first fragment only
 *
 * There is no explicit reason given in the RFC.  Historical reason maybe?
 */
/*
 * Fragment input
 */
int
frag6_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m = *mp, *t;
	struct ip6_hdr *ip6;
	struct ip6_frag *ip6f;
	struct ip6q *q6;
	struct ip6asfrag *af6, *ip6af, *naf6, *paf6;
	int offset = *offp, nxt, i, next;
	int first_frag = 0;
	int fragoff, frgpartlen;	/* must be larger than u_int16_t */
	struct ifnet *dstifp;
#ifdef IN6_IFSTAT_STRICT
	struct route_in6 ro;
	struct sockaddr_in6 *dst;
#endif
	u_int8_t ecn, ecn0;

	ip6 = mtod(m, struct ip6_hdr *);
	IP6_EXTHDR_GET(ip6f, struct ip6_frag *, m, offset, sizeof(*ip6f));
	if (ip6f == NULL)
		return IPPROTO_DONE;

	dstifp = NULL;
#ifdef IN6_IFSTAT_STRICT
	/* find the destination interface of the packet. */
	bzero(&ro, sizeof(ro));
	ro.ro_tableid = m->m_pkthdr.ph_rtableid;
	dst = &ro.ro_dst;
	dst->sin6_family = AF_INET6;
	dst->sin6_len = sizeof(struct sockaddr_in6);
	dst->sin6_addr = ip6->ip6_dst;

	ro.ro_rt = rtalloc_mpath(sin6tosa(&ro.ro_dst),
	    &ip6->ip6_src.s6_addr32[0], ro.ro_tableid);

	if (ro.ro_rt != NULL && ro.ro_rt->rt_ifa != NULL)
		dstifp = ifatoia6(ro.ro_rt->rt_ifa)->ia_ifp;
	if (ro.ro_rt != NULL) {
		rtfree(ro.ro_rt);
		ro.ro_rt = NULL;
	}
#else
	/* we are violating the spec, this is not the destination interface */
	if ((m->m_flags & M_PKTHDR) != 0)
		dstifp = m->m_pkthdr.rcvif;
#endif

	/* jumbo payload can't contain a fragment header */
	if (ip6->ip6_plen == 0) {
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER, offset);
		in6_ifstat_inc(dstifp, ifs6_reass_fail);
		return IPPROTO_DONE;
	}

	/*
	 * check whether fragment packet's fragment length is
	 * multiple of 8 octets.
	 * sizeof(struct ip6_frag) == 8
	 * sizeof(struct ip6_hdr) = 40
	 */
	if ((ip6f->ip6f_offlg & IP6F_MORE_FRAG) &&
	    (((ntohs(ip6->ip6_plen) - offset) & 0x7) != 0)) {
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
		    offsetof(struct ip6_hdr, ip6_plen));
		in6_ifstat_inc(dstifp, ifs6_reass_fail);
		return IPPROTO_DONE;
	}

	ip6stat.ip6s_fragments++;
	in6_ifstat_inc(dstifp, ifs6_reass_reqd);

	/* offset now points to data portion */
	offset += sizeof(struct ip6_frag);

	/*
	 * RFC6946:  A host that receives an IPv6 packet which includes
	 * a Fragment Header with the "Fragment Offset" equal to 0 and
	 * the "M" bit equal to 0 MUST process such packet in isolation
	 * from any other packets/fragments.
	 */
	fragoff = ntohs(ip6f->ip6f_offlg & IP6F_OFF_MASK);
	if (fragoff == 0 && !(ip6f->ip6f_offlg & IP6F_MORE_FRAG)) {
		ip6stat.ip6s_reassembled++;
		in6_ifstat_inc(dstifp, ifs6_reass_ok);
		*offp = offset;
		return ip6f->ip6f_nxt;
	}

	IP6Q_LOCK();

	/*
	 * Enforce upper bound on number of fragments.
	 * If maxfrag is 0, never accept fragments.
	 * If maxfrag is -1, accept all fragments without limitation.
	 */
	if (ip6_maxfrags < 0)
		;
	else if (frag6_nfrags >= (u_int)ip6_maxfrags)
		goto dropfrag;

	TAILQ_FOREACH(q6, &frag6_queue, ip6q_queue)
		if (ip6f->ip6f_ident == q6->ip6q_ident &&
		    IN6_ARE_ADDR_EQUAL(&ip6->ip6_src, &q6->ip6q_src) &&
		    IN6_ARE_ADDR_EQUAL(&ip6->ip6_dst, &q6->ip6q_dst))
			break;

	if (q6 == NULL) {
		/*
		 * the first fragment to arrive, create a reassembly queue.
		 */
		first_frag = 1;

		/*
		 * Enforce upper bound on number of fragmented packets
		 * for which we attempt reassembly;
		 * If maxfragpackets is 0, never accept fragments.
		 * If maxfragpackets is -1, accept all fragments without
		 * limitation.
		 */
		if (ip6_maxfragpackets < 0)
			;
		else if (frag6_nfragpackets >= (u_int)ip6_maxfragpackets)
			goto dropfrag;
		frag6_nfragpackets++;
		q6 = malloc(sizeof(*q6), M_FTABLE, M_NOWAIT | M_ZERO);
		if (q6 == NULL)
			goto dropfrag;

		TAILQ_INSERT_HEAD(&frag6_queue, q6, ip6q_queue);

		/* ip6q_nxt will be filled afterwards, from 1st fragment */
		LIST_INIT(&q6->ip6q_asfrag);
		q6->ip6q_ident	= ip6f->ip6f_ident;
		q6->ip6q_ttl	= IPV6_FRAGTTL;
		q6->ip6q_src	= ip6->ip6_src;
		q6->ip6q_dst	= ip6->ip6_dst;
		q6->ip6q_unfrglen = -1;	/* The 1st fragment has not arrived. */
		q6->ip6q_nfrag = 0;
	}

	/*
	 * If it's the 1st fragment, record the length of the
	 * unfragmentable part and the next header of the fragment header.
	 */
	if (fragoff == 0) {
		q6->ip6q_unfrglen = offset - sizeof(struct ip6_hdr) -
		    sizeof(struct ip6_frag);
		q6->ip6q_nxt = ip6f->ip6f_nxt;
	}

	/*
	 * Check that the reassembled packet would not exceed 65535 bytes
	 * in size.
	 * If it would exceed, discard the fragment and return an ICMP error.
	 */
	frgpartlen = sizeof(struct ip6_hdr) + ntohs(ip6->ip6_plen) - offset;
	if (q6->ip6q_unfrglen >= 0) {
		/* The 1st fragment has already arrived. */
		if (q6->ip6q_unfrglen + fragoff + frgpartlen > IPV6_MAXPACKET) {
			icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
			    offset - sizeof(struct ip6_frag) +
			    offsetof(struct ip6_frag, ip6f_offlg));
			IP6Q_UNLOCK();
			return (IPPROTO_DONE);
		}
	} else if (fragoff + frgpartlen > IPV6_MAXPACKET) {
		icmp6_error(m, ICMP6_PARAM_PROB, ICMP6_PARAMPROB_HEADER,
			    offset - sizeof(struct ip6_frag) +
				offsetof(struct ip6_frag, ip6f_offlg));
		IP6Q_UNLOCK();
		return (IPPROTO_DONE);
	}
	/*
	 * If it's the first fragment, do the above check for each
	 * fragment already stored in the reassembly queue.
	 */
	if (fragoff == 0) {
		LIST_FOREACH_SAFE(af6, &q6->ip6q_asfrag, ip6af_list, naf6) {
			if (q6->ip6q_unfrglen + af6->ip6af_off +
			    af6->ip6af_frglen > IPV6_MAXPACKET) {
				struct mbuf *merr = IP6_REASS_MBUF(af6);
				struct ip6_hdr *ip6err;
				int erroff = af6->ip6af_offset;

				/* dequeue the fragment. */
				LIST_REMOVE(af6, ip6af_list);
				free(af6, M_FTABLE, 0);

				/* adjust pointer. */
				ip6err = mtod(merr, struct ip6_hdr *);

				/*
				 * Restore source and destination addresses
				 * in the erroneous IPv6 header.
				 */
				ip6err->ip6_src = q6->ip6q_src;
				ip6err->ip6_dst = q6->ip6q_dst;

				icmp6_error(merr, ICMP6_PARAM_PROB,
				    ICMP6_PARAMPROB_HEADER,
				    erroff - sizeof(struct ip6_frag) +
				    offsetof(struct ip6_frag, ip6f_offlg));
			}
		}
	}

	ip6af = malloc(sizeof(*ip6af), M_FTABLE, M_NOWAIT | M_ZERO);
	if (ip6af == NULL)
		goto dropfrag;
	ip6af->ip6af_flow = ip6->ip6_flow;
	ip6af->ip6af_mff = ip6f->ip6f_offlg & IP6F_MORE_FRAG;
	ip6af->ip6af_off = fragoff;
	ip6af->ip6af_frglen = frgpartlen;
	ip6af->ip6af_offset = offset;
	IP6_REASS_MBUF(ip6af) = m;

	if (first_frag) {
		paf6 = NULL;
		goto insert;
	}

	/*
	 * Handle ECN by comparing this segment with the first one;
	 * if CE is set, do not lose CE.
	 * drop if CE and not-ECT are mixed for the same packet.
	 */
	af6 = LIST_FIRST(&q6->ip6q_asfrag);
	ecn = (ntohl(ip6->ip6_flow) >> 20) & IPTOS_ECN_MASK;
	ecn0 = (ntohl(af6->ip6af_flow) >> 20) & IPTOS_ECN_MASK;
	if (ecn == IPTOS_ECN_CE) {
		if (ecn0 == IPTOS_ECN_NOTECT) {
			free(ip6af, M_FTABLE, 0);
			goto dropfrag;
		}
		if (ecn0 != IPTOS_ECN_CE)
			af6->ip6af_flow |= htonl(IPTOS_ECN_CE << 20);
	}
	if (ecn == IPTOS_ECN_NOTECT && ecn0 != IPTOS_ECN_NOTECT) {
		free(ip6af, M_FTABLE, 0);
		goto dropfrag;
	}

	/*
	 * Find a segment which begins after this one does.
	 */
	for (paf6 = NULL, af6 = LIST_FIRST(&q6->ip6q_asfrag);
	    af6 != NULL;
	    paf6 = af6, af6 = LIST_NEXT(af6, ip6af_list))
		if (af6->ip6af_off > ip6af->ip6af_off)
			break;

	/*
	 * RFC 5722, Errata 3089:  When reassembling an IPv6 datagram, if one
	 * or more its constituent fragments is determined to be an overlapping
	 * fragment, the entire datagram (and any constituent fragments) MUST
	 * be silently discarded.
	 */
	if (paf6 != NULL) {
		i = (paf6->ip6af_off + paf6->ip6af_frglen) - ip6af->ip6af_off;
		if (i > 0) {
#if 0				/* suppress the noisy log */
			char ip[INET6_ADDRSTRLEN];
			log(LOG_ERR, "%d bytes of a fragment from %s "
			    "overlaps the previous fragment\n",
			    i,
			    inet_ntop(AF_INET6, &q6->ip6q_src, ip, sizeof(ip)));
#endif
			free(ip6af, M_FTABLE, 0);
			goto flushfrags;
		}
	}
	if (af6 != NULL) {
		i = (ip6af->ip6af_off + ip6af->ip6af_frglen) - af6->ip6af_off;
		if (i > 0) {
#if 0				/* suppress the noisy log */
			char ip[INET6_ADDRSTRLEN];
			log(LOG_ERR, "%d bytes of a fragment from %s "
			    "overlaps the succeeding fragment",
			    i,
			    inet_ntop(AF_INET6, &q6->ip6q_src, ip, sizeof(ip)));
#endif
			free(ip6af, M_FTABLE, 0);
			goto flushfrags;
		}
	}

 insert:
	/*
	 * Stick new segment in its place;
	 * check for complete reassembly.
	 * Move to front of packet queue, as we are
	 * the most recently active fragmented packet.
	 */
	if (paf6 != NULL)
		LIST_INSERT_AFTER(paf6, ip6af, ip6af_list);
	else
		LIST_INSERT_HEAD(&q6->ip6q_asfrag, ip6af, ip6af_list);
	frag6_nfrags++;
	q6->ip6q_nfrag++;
#if 0 /* xxx */
	if (q6 != TAILQ_FIRST(&frag6_queue)) {
		TAILQ_REMOVE(&frag6_queue, q6, ip6q_queue);
		TAILQ_INSERT_HEAD(&frag6_queue, q6, ip6q_queue);
	}
#endif
	next = 0;
	for (paf6 = NULL, af6 = LIST_FIRST(&q6->ip6q_asfrag);
	    af6 != NULL;
	    paf6 = af6, af6 = LIST_NEXT(af6, ip6af_list)) {
		if (af6->ip6af_off != next) {
			IP6Q_UNLOCK();
			return IPPROTO_DONE;
		}
		next += af6->ip6af_frglen;
	}
	if (paf6->ip6af_mff) {
		IP6Q_UNLOCK();
		return IPPROTO_DONE;
	}

	/*
	 * Reassembly is complete; concatenate fragments.
	 */
	ip6af = LIST_FIRST(&q6->ip6q_asfrag);
	LIST_REMOVE(ip6af, ip6af_list);
	t = m = IP6_REASS_MBUF(ip6af);
	while ((af6 = LIST_FIRST(&q6->ip6q_asfrag)) != NULL) {
		LIST_REMOVE(af6, ip6af_list);
		while (t->m_next)
			t = t->m_next;
		t->m_next = IP6_REASS_MBUF(af6);
		m_adj(t->m_next, af6->ip6af_offset);
		free(af6, M_FTABLE, 0);
	}

	/* adjust offset to point where the original next header starts */
	offset = ip6af->ip6af_offset - sizeof(struct ip6_frag);
	free(ip6af, M_FTABLE, 0);
	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_plen = htons((u_short)next + offset - sizeof(struct ip6_hdr));
	ip6->ip6_src = q6->ip6q_src;
	ip6->ip6_dst = q6->ip6q_dst;
	nxt = q6->ip6q_nxt;

	/* Delete frag6 header */
	if (frag6_deletefraghdr(m, offset) != 0) {
		TAILQ_REMOVE(&frag6_queue, q6, ip6q_queue);
		frag6_nfrags -= q6->ip6q_nfrag;
		free(q6, M_FTABLE, 0);
		frag6_nfragpackets--;
		goto dropfrag;
	}

	/*
	 * Store NXT to the original.
	 */
	{
		u_int8_t *prvnxtp = ip6_get_prevhdr(m, offset); /* XXX */
		*prvnxtp = nxt;
	}

	TAILQ_REMOVE(&frag6_queue, q6, ip6q_queue);
	frag6_nfrags -= q6->ip6q_nfrag;
	free(q6, M_FTABLE, 0);
	frag6_nfragpackets--;

	if (m->m_flags & M_PKTHDR) { /* Isn't it always true? */
		int plen = 0;
		for (t = m; t; t = t->m_next)
			plen += t->m_len;
		m->m_pkthdr.len = plen;
	}

	ip6stat.ip6s_reassembled++;
	in6_ifstat_inc(dstifp, ifs6_reass_ok);

	/*
	 * Tell launch routine the next header
	 */

	*mp = m;
	*offp = offset;

	IP6Q_UNLOCK();
	return nxt;

 flushfrags:
	while ((af6 = LIST_FIRST(&q6->ip6q_asfrag)) != NULL) {
		LIST_REMOVE(af6, ip6af_list);
		m_freem(IP6_REASS_MBUF(af6));
		free(af6, M_FTABLE, 0);
	}
	ip6stat.ip6s_fragdropped += q6->ip6q_nfrag;
	TAILQ_REMOVE(&frag6_queue, q6, ip6q_queue);
	frag6_nfrags -= q6->ip6q_nfrag;
	free(q6, M_FTABLE, 0);
	frag6_nfragpackets--;

 dropfrag:
	in6_ifstat_inc(dstifp, ifs6_reass_fail);
	ip6stat.ip6s_fragdropped++;
	m_freem(m);
	IP6Q_UNLOCK();
	return IPPROTO_DONE;
}

/*
 * Delete fragment header after the unfragmentable header portions.
 */
int
frag6_deletefraghdr(struct mbuf *m, int offset)
{
	struct mbuf *t;

	if (m->m_len >= offset + sizeof(struct ip6_frag)) {
		memmove(mtod(m, caddr_t) + sizeof(struct ip6_frag),
		    mtod(m, caddr_t), offset);
		m->m_data += sizeof(struct ip6_frag);
		m->m_len -= sizeof(struct ip6_frag);
	} else {
		/* this comes with no copy if the boundary is on cluster */
		if ((t = m_split(m, offset, M_DONTWAIT)) == NULL)
			return (ENOBUFS);
		m_adj(t, sizeof(struct ip6_frag));
		m_cat(m, t);
	}

	return (0);
}

/*
 * Free a fragment reassembly header and all
 * associated datagrams.
 */
void
frag6_freef(struct ip6q *q6)
{
	struct ip6asfrag *af6;

	IP6Q_LOCK_CHECK();

	while ((af6 = LIST_FIRST(&q6->ip6q_asfrag)) != NULL) {
		struct mbuf *m = IP6_REASS_MBUF(af6);

		LIST_REMOVE(af6, ip6af_list);

		/*
		 * Return ICMP time exceeded error for the 1st fragment.
		 * Just free other fragments.
		 */
		if (af6->ip6af_off == 0) {
			struct ip6_hdr *ip6;

			/* adjust pointer */
			ip6 = mtod(m, struct ip6_hdr *);

			/* restore source and destination addresses */
			ip6->ip6_src = q6->ip6q_src;
			ip6->ip6_dst = q6->ip6q_dst;

			icmp6_error(m, ICMP6_TIME_EXCEEDED,
				    ICMP6_TIME_EXCEED_REASSEMBLY, 0);
		} else
			m_freem(m);
		free(af6, M_FTABLE, 0);
	}
	TAILQ_REMOVE(&frag6_queue, q6, ip6q_queue);
	frag6_nfrags -= q6->ip6q_nfrag;
	free(q6, M_FTABLE, 0);
	frag6_nfragpackets--;
}

/*
 * IPv6 reassembling timer processing;
 * if a timer expires on a reassembly
 * queue, discard it.
 */
void
frag6_slowtimo(void)
{
	struct ip6q *q6, *nq6;
	int s = splsoftnet();
	extern struct route_in6 ip6_forward_rt;

	IP6Q_LOCK();
	TAILQ_FOREACH_SAFE(q6, &frag6_queue, ip6q_queue, nq6)
		if (--q6->ip6q_ttl == 0) {
			ip6stat.ip6s_fragtimeout++;
			/* XXX in6_ifstat_inc(ifp, ifs6_reass_fail) */
			frag6_freef(q6);
		}

	/*
	 * If we are over the maximum number of fragments
	 * (due to the limit being lowered), drain off
	 * enough to get down to the new limit.
	 */
	while (frag6_nfragpackets > (u_int)ip6_maxfragpackets &&
	    !TAILQ_EMPTY(&frag6_queue)) {
		ip6stat.ip6s_fragoverflow++;
		/* XXX in6_ifstat_inc(ifp, ifs6_reass_fail) */
		frag6_freef(TAILQ_LAST(&frag6_queue, ip6q_head));
	}
	IP6Q_UNLOCK();

	/*
	 * Routing changes might produce a better route than we last used;
	 * make sure we notice eventually, even if forwarding only for one
	 * destination and the cache is never replaced.
	 */
	if (ip6_forward_rt.ro_rt) {
		rtfree(ip6_forward_rt.ro_rt);
		ip6_forward_rt.ro_rt = NULL;
	}

	splx(s);
}

/*
 * Drain off all datagram fragments.
 */
void
frag6_drain(void)
{
	struct ip6q *q6;

	if (ip6q_lock_try() == 0)
		return;
	while ((q6 = TAILQ_FIRST(&frag6_queue)) != NULL) {
		ip6stat.ip6s_fragdropped++;
		/* XXX in6_ifstat_inc(ifp, ifs6_reass_fail) */
		frag6_freef(q6);
	}
	IP6Q_UNLOCK();
}
