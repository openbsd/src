/*	$OpenBSD: ipx_input.c,v 1.7 2000/01/11 20:30:13 fgsch Exp $	*/

/*-
 *
 * Copyright (c) 1996 Michael Shalayeff
 * Copyright (c) 1995, Mike Mitchell
 * Copyright (c) 1984, 1985, 1986, 1987, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)ipx_input.c
 *
 * from FreeBSD Id: ipx_input.c,v 1.8 1996/03/11 15:13:48 davidg Exp
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <vm/vm.h>

#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>
#include <net/raw_cb.h>

#include <netipx/ipx.h>
#include <netipx/spx.h>
#include <netipx/ipx_if.h>
#include <netipx/ipx_pcb.h>
#include <netipx/ipx_var.h>

#ifndef IPXPRINTFS
#define IPXPRINTFS	1	/* printing forwarding information */
#endif

int ipxprintfs = IPXPRINTFS;
int ipxcksum = 0;
int ipxdonosocks = 0;
int ipxforwarding = 0;

union ipx_net	ipx_zeronet;
union ipx_host	ipx_zerohost;

union ipx_net	ipx_broadnet;
union ipx_host	ipx_broadhost;

struct ipxstat ipxstat;
struct sockaddr_ipx ipx_netmask, ipx_hostmask;

static u_int16_t	allones[] = {0xffff, 0xffff, 0xffff};

#ifndef	IPXCBHASHSIZE
#define	IPXCBHASHSIZE	32
#endif
struct ipxpcbtable	ipxrawcbtable;
int			ipxrawcbhashsize = IPXCBHASHSIZE;

struct ipx_ifaddrhead	ipx_ifaddr;
struct ifqueue	ipxintrq;
int	ipxqmaxlen = IFQ_MAXLEN;

u_long	ipx_pexseq;

/*
 * IPX initialization.
 */

void
ipx_init()
{
	ipx_broadnet = * (union ipx_net *) allones;
	ipx_broadhost = * (union ipx_host *) allones;

	ipx_pexseq = time.tv_usec;
	ipxintrq.ifq_maxlen = ipxqmaxlen;

	ipx_netmask.sipx_len = 6;
	ipx_netmask.sipx_addr.ipx_net = ipx_broadnet;

	ipx_hostmask.sipx_len = 12;
	ipx_hostmask.sipx_addr.ipx_net = ipx_broadnet;
	ipx_hostmask.sipx_addr.ipx_host = ipx_broadhost;

	ipx_pcbinit(&ipxrawcbtable, ipxrawcbhashsize);

	TAILQ_INIT(&ipx_ifaddr);
}

/*
 * IPX input routine.  Pass to next level.
 */
void
ipxintr()
{
	register struct ipx *ipx;
	register struct mbuf *m;
	register struct ipxpcb *ipxp;
	struct ipx_ifaddr *ia;
	int len, s;

next:
	/*
	 * Get next datagram off input queue and get IPX header
	 * in first mbuf.
	 */
	s = splimp();
	IF_DEQUEUE(&ipxintrq, m);
	splx(s);
	if (m == NULL) {
		return;
	}

	ipxstat.ipxs_total++;

	if ((m->m_flags & M_EXT || m->m_len < sizeof (struct ipx)) &&
	    (m = m_pullup(m, sizeof (struct ipx))) == 0) {
		ipxstat.ipxs_toosmall++;
		goto next;
	}

	/*
	 * Give any raw listeners a crack at the packet
	 */
	for (ipxp = ipxrawcbtable.ipxpt_queue.cqh_first;
	     ipxp != (struct ipxpcb *)&ipxrawcbtable.ipxpt_queue;
	     ipxp = ipxp->ipxp_queue.cqe_next) {
		struct mbuf *m1 = m_copy(m, 0, (int)M_COPYALL);
		if (m1)
			ipx_input(m1, ipxp);
	}

	ipx = mtod(m, struct ipx *);
	len = ntohs(ipx->ipx_len);
	/*
	 * Check that the amount of data in the buffers
	 * is as at least much as the IPX header would have us expect.
	 * Trim mbufs if longer than we expect.
	 * Drop packet if shorter than we expect.
	 */
	if (m->m_pkthdr.len < len) {
		ipxstat.ipxs_tooshort++;
		goto bad;
	}
	if (m->m_pkthdr.len > len) {
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = len;
			m->m_pkthdr.len = len;
		} else
			m_adj(m, len - m->m_pkthdr.len);
	}
	if (ipxcksum && ipx->ipx_sum != 0xffff) {
		if (ipx->ipx_sum != ipx_cksum(m, len)) {
			ipxstat.ipxs_badsum++;
			goto bad;
		}
	}

	/*
	 * Is this a directed broadcast?
	 */
	if (ipx_hosteqnh(ipx_broadhost,ipx->ipx_dna.ipx_host)) {
		if ((!ipx_neteq(ipx->ipx_dna, ipx->ipx_sna)) &&
		    (!ipx_neteqnn(ipx->ipx_dna.ipx_net, ipx_broadnet)) &&
		    (!ipx_neteqnn(ipx->ipx_sna.ipx_net, ipx_zeronet)) &&
		    (!ipx_neteqnn(ipx->ipx_dna.ipx_net, ipx_zeronet)) ) {
			/*
			 * If it is a broadcast to the net where it was
			 * received from, treat it as ours.
			 */
			for (ia = ipx_ifaddr.tqh_first; ia;
			    ia = ia->ia_list.tqe_next)
				if((ia->ia_ifa.ifa_ifp == m->m_pkthdr.rcvif) &&
				    ipx_neteq(ia->ia_addr.sipx_addr, 
				    ipx->ipx_dna))
                                       goto ours;

			/*
			 * Look to see if I need to eat this packet.
			 * Algorithm is to forward all young packets
			 * and prematurely age any packets which will
			 * by physically broadcasted.
			 * Any very old packets eaten without forwarding
			 * would die anyway.
			 *
			 * Suggestion of Bill Nesheim, Cornell U.
			 */
			if (ipx->ipx_tc < IPX_MAXHOPS) {
				ipx_forward(m);
				goto next;
			}
		}
	/*
	 * Is this our packet? If not, forward.
	 */
	} else {
		for (ia = ipx_ifaddr.tqh_first; ia; ia = ia->ia_list.tqe_next)
			if (ipx_hosteq(ipx->ipx_dna, ia->ia_addr.sipx_addr) &&
			    (ipx_neteq(ipx->ipx_dna, ia->ia_addr.sipx_addr) ||
			    ipx_neteqnn(ipx->ipx_dna.ipx_net, ipx_zeronet)))
				break;

		if (ia == NULL) {
			ipx_forward(m);
			goto next;
		}
	}
ours:
	/*
	 * Locate pcb for datagram.
	 */
	ipxp = ipx_pcblookup(&ipx->ipx_sna, ipx->ipx_dna.ipx_port,
	    IPX_WILDCARD);
	/*
	 * Switch out to protocol's input routine.
	 */
	if (ipxp) {
		ipxstat.ipxs_delivered++;
		if ((ipxp->ipxp_flags & IPXP_ALL_PACKETS) == 0)
			switch (ipx->ipx_pt) {

			    case IPXPROTO_SPX:
				    spx_input(m, ipxp);
				    goto next;
			}
		ipx_input(m, ipxp);
	} else
		goto bad;

	goto next;

bad:
	m_freem(m);
	goto next;
}

void *
ipx_ctlinput(cmd, arg_as_sa, dummy)
	int cmd;
	struct sockaddr *arg_as_sa;	/* XXX should be swapped with dummy */
	void *dummy;
{
	caddr_t arg = (/* XXX */ caddr_t)arg_as_sa;
	struct ipx_addr *ipx;

	if (cmd < 0 || cmd > PRC_NCMDS)
		return NULL;
	switch (cmd) {
		struct sockaddr_ipx *sipx;

	case PRC_IFDOWN:
	case PRC_HOSTDEAD:
	case PRC_HOSTUNREACH:
		sipx = (struct sockaddr_ipx *)arg;
		if (sipx == NULL || sipx->sipx_family != AF_IPX)
			return NULL;
		ipx = &sipx->sipx_addr;
		break;

	default:
		break;
	}
	return NULL;
}

/*
 * Forward a packet.  If some error occurs return the sender
 * an error packet.  Note we can't always generate a meaningful
 * error message because the IPX errors don't have a large enough repetoire
 * of codes and types.
 */
struct route ipx_droute;
struct route ipx_sroute;

void
ipx_forward(m)
struct mbuf *m;
{
	register struct ipx *ipx = mtod(m, struct ipx *);
	register int error;
	struct mbuf *mcopy = NULL;
	int agedelta = 1;
	int flags = IPX_FORWARDING;
	int ok_there = 0;
	int ok_back = 0;

	if (ipxforwarding == 0) {
		m_freem(m);
		goto cleanup;
	}
	ipx->ipx_tc++;
	if (ipx->ipx_tc > IPX_MAXHOPS) {
		m_freem(m);
		goto cleanup;
	}

	if ((ok_there = ipx_do_route(&ipx->ipx_dna,&ipx_droute))==0) {
		m_freem(m);
		goto cleanup;
	}
	/*
	 * Here we think about  forwarding  broadcast packets,
	 * so we try to insure that it doesn't go back out
	 * on the interface it came in on.  Also, if we
	 * are going to physically broadcast this, let us
	 * age the packet so we can eat it safely the second time around.
	 */
	if (ipx->ipx_dna.ipx_host.c_host[0] & 0x1) {
		struct ipx_ifaddr *ia = ipx_iaonnetof(&ipx->ipx_dna);
		struct ifnet *ifp;
		if (ia) {
			/* I'm gonna hafta eat this packet */
			agedelta += IPX_MAXHOPS - ipx->ipx_tc;
			ipx->ipx_tc = IPX_MAXHOPS;
		}
		if ((ok_back = ipx_do_route(&ipx->ipx_sna,&ipx_sroute))==0) {
			/* error = ENETUNREACH; He'll never get it! */
			m_freem(m);
			goto cleanup;
		}
		if (ipx_droute.ro_rt &&
		    (ifp=ipx_droute.ro_rt->rt_ifp) &&
		    ipx_sroute.ro_rt &&
		    (ifp!=ipx_sroute.ro_rt->rt_ifp)) {
			flags |= IPX_ALLOWBROADCAST;
		} else {
			m_freem(m);
			goto cleanup;
		}
	}
	/* need to adjust checksum */
	if (ipxcksum && ipx->ipx_sum != 0xffff) {
		union bytes {
			u_char c[4];
			u_short s[2];
			long l;
		} x;
		register int shift;
		x.l = 0; x.c[0] = agedelta;
		shift = (((((int)ntohs(ipx->ipx_len))+1)>>1)-2) & 0xf;
		x.l = ipx->ipx_sum + (x.s[0] << shift);
		x.l = x.s[0] + x.s[1];
		x.l = x.s[0] + x.s[1];
		if (x.l==0xffff) ipx->ipx_sum = 0; else ipx->ipx_sum = x.l;
	} else 
		ipx->ipx_sum = 0xffff;

	error = ipx_outputfl(m, &ipx_droute, flags);
	if (error == 0) {
		if (ipxprintfs) {
			printf("forward: ");
			ipx_printhost(&ipx->ipx_sna);
			printf(" to ");
			ipx_printhost(&ipx->ipx_dna);
			printf(" hops %d\n", ipx->ipx_tc);
		}
	} else if (mcopy != NULL) {
		ipx = mtod(mcopy, struct ipx *);
		switch (error) {

		case ENETUNREACH:
		case EHOSTDOWN:
		case EHOSTUNREACH:
		case ENETDOWN:
		case EPERM:
			break;

		case EMSGSIZE:
			break;

		case ENOBUFS:
			break;
		}
		mcopy = NULL;
		m_freem(m);
	}
cleanup:
	if (ok_there)
		ipx_undo_route(&ipx_droute);
	if (ok_back)
		ipx_undo_route(&ipx_sroute);
	if (mcopy != NULL)
		m_freem(mcopy);
}

int
ipx_do_route(src, ro)
struct ipx_addr *src;
struct route *ro;
{
	struct sockaddr_ipx *dst;

	bzero((caddr_t)ro, sizeof (*ro));
	dst = (struct sockaddr_ipx *)&ro->ro_dst;

	dst->sipx_len = sizeof(*dst);
	dst->sipx_family = AF_IPX;
	dst->sipx_addr = *src;
	dst->sipx_addr.ipx_port = 0;
	rtalloc(ro);
	if (ro->ro_rt == 0 || ro->ro_rt->rt_ifp == 0) {
		return (0);
	}
	ro->ro_rt->rt_use++;
	return (1);
}

void
ipx_undo_route(ro)
register struct route *ro;
{
	if (ro->ro_rt) {RTFREE(ro->ro_rt);}
}

void
ipx_watch_output(m, ifp)
struct mbuf *m;
struct ifnet *ifp;
{
	register struct ipxpcb *ipxp;
	register struct ifaddr *ifa;
	register struct ipx_ifaddr *ia;
	/*
	 * Give any raw listeners a crack at the packet
	 */
	for (ipxp = ipxrawcbtable.ipxpt_queue.cqh_first;
	    ipxp != (struct ipxpcb *)&ipxrawcbtable.ipxpt_queue;
	    ipxp = ipxp->ipxp_queue.cqe_next) {
		struct mbuf *m0 = m_copy(m, 0, (int)M_COPYALL);
		if (m0) {
			register struct ipx *ipx;

			M_PREPEND(m0, sizeof (*ipx), M_DONTWAIT);
			if (m0 == NULL)
				continue;
			ipx = mtod(m0, struct ipx *);
			ipx->ipx_sna.ipx_net = ipx_zeronet;
			for (ia = ipx_ifaddr.tqh_first; ia;
			    ia = ia->ia_list.tqe_next)
				if (ifp == ia->ia_ifp)
					break;
			if (ia == NULL)
				ipx->ipx_sna.ipx_host = ipx_zerohost;
			else
				ipx->ipx_sna.ipx_host =
				    ia->ia_addr.sipx_addr.ipx_host;

			if (ifp && (ifp->if_flags & IFF_POINTOPOINT))
			    for(ifa = ifp->if_addrlist.tqh_first; ifa;
					ifa = ifa->ifa_list.tqe_next) {
				if (ifa->ifa_addr->sa_family == AF_IPX) {
				    ipx->ipx_sna = IA_SIPX(ifa)->sipx_addr;
				    break;
				}
			    }
			ipx->ipx_len = ntohl(m0->m_pkthdr.len);
			ipx_input(m0, ipxp);
		}
	}
}
