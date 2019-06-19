/*	$OpenBSD: in_pcb.c,v 1.247 2018/10/04 17:33:41 bluhm Exp $	*/
/*	$NetBSD: in_pcb.c,v 1.25 1996/02/13 23:41:53 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1991, 1993
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
 * 3. Neither the name of the University nor the names of its contributors
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

#include "pf.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/mount.h>
#include <sys/pool.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/pfvar.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#ifdef IPSEC
#include <netinet/ip_esp.h>
#endif /* IPSEC */

const struct in_addr zeroin_addr;

union {
	struct in_addr	za_in;
	struct in6_addr	za_in6;
} zeroin46_addr;

/*
 * These configure the range of local port addresses assigned to
 * "unspecified" outgoing connections/packets/whatever.
 */
int ipport_firstauto = IPPORT_RESERVED;
int ipport_lastauto = IPPORT_USERRESERVED;
int ipport_hifirstauto = IPPORT_HIFIRSTAUTO;
int ipport_hilastauto = IPPORT_HILASTAUTO;

struct baddynamicports baddynamicports;
struct baddynamicports rootonlyports;
struct pool inpcb_pool;
int inpcb_pool_initialized = 0;

int in_pcbresize (struct inpcbtable *, int);

#define	INPCBHASH_LOADFACTOR(_x)	(((_x) * 3) / 4)

struct inpcbhead *in_pcbhash(struct inpcbtable *, int,
    const struct in_addr *, u_short, const struct in_addr *, u_short);
struct inpcbhead *in_pcblhash(struct inpcbtable *, int, u_short);

struct inpcbhead *
in_pcbhash(struct inpcbtable *table, int rdom,
    const struct in_addr *faddr, u_short fport,
    const struct in_addr *laddr, u_short lport)
{
	SIPHASH_CTX ctx;
	u_int32_t nrdom = htonl(rdom);

	SipHash24_Init(&ctx, &table->inpt_key);
	SipHash24_Update(&ctx, &nrdom, sizeof(nrdom));
	SipHash24_Update(&ctx, faddr, sizeof(*faddr));
	SipHash24_Update(&ctx, &fport, sizeof(fport));
	SipHash24_Update(&ctx, laddr, sizeof(*laddr));
	SipHash24_Update(&ctx, &lport, sizeof(lport));

	return (&table->inpt_hashtbl[SipHash24_End(&ctx) & table->inpt_mask]);
}

struct inpcbhead *
in_pcblhash(struct inpcbtable *table, int rdom, u_short lport)
{
	SIPHASH_CTX ctx;
	u_int32_t nrdom = htonl(rdom);

	SipHash24_Init(&ctx, &table->inpt_lkey);
	SipHash24_Update(&ctx, &nrdom, sizeof(nrdom));
	SipHash24_Update(&ctx, &lport, sizeof(lport));

	return (&table->inpt_lhashtbl[SipHash24_End(&ctx) & table->inpt_lmask]);
}

void
in_pcbinit(struct inpcbtable *table, int hashsize)
{

	TAILQ_INIT(&table->inpt_queue);
	table->inpt_hashtbl = hashinit(hashsize, M_PCB, M_NOWAIT,
	    &table->inpt_mask);
	if (table->inpt_hashtbl == NULL)
		panic("in_pcbinit: hashinit failed");
	table->inpt_lhashtbl = hashinit(hashsize, M_PCB, M_NOWAIT,
	    &table->inpt_lmask);
	if (table->inpt_lhashtbl == NULL)
		panic("in_pcbinit: hashinit failed for lport");
	table->inpt_count = 0;
	table->inpt_size = hashsize;
	arc4random_buf(&table->inpt_key, sizeof(table->inpt_key));
	arc4random_buf(&table->inpt_lkey, sizeof(table->inpt_lkey));
}

/*
 * Check if the specified port is invalid for dynamic allocation.
 */
int
in_baddynamic(u_int16_t port, u_int16_t proto)
{
	switch (proto) {
	case IPPROTO_TCP:
		return (DP_ISSET(baddynamicports.tcp, port));
	case IPPROTO_UDP:
#ifdef IPSEC
		/* Cannot preset this as it is a sysctl */
		if (port == udpencap_port)
			return (1);
#endif
		return (DP_ISSET(baddynamicports.udp, port));
	default:
		return (0);
	}
}

int
in_rootonly(u_int16_t port, u_int16_t proto)
{
	switch (proto) {
	case IPPROTO_TCP:
		return (port < IPPORT_RESERVED ||
		    DP_ISSET(rootonlyports.tcp, port));
	case IPPROTO_UDP:
		return (port < IPPORT_RESERVED ||
		    DP_ISSET(rootonlyports.udp, port));
	default:
		return (0);
	}
}

int
in_pcballoc(struct socket *so, struct inpcbtable *table)
{
	struct inpcb *inp;
	struct inpcbhead *head;

	NET_ASSERT_LOCKED();

	if (inpcb_pool_initialized == 0) {
		pool_init(&inpcb_pool, sizeof(struct inpcb), 0,
		    IPL_SOFTNET, 0, "inpcbpl", NULL);
		inpcb_pool_initialized = 1;
	}
	inp = pool_get(&inpcb_pool, PR_NOWAIT|PR_ZERO);
	if (inp == NULL)
		return (ENOBUFS);
	inp->inp_table = table;
	inp->inp_socket = so;
	refcnt_init(&inp->inp_refcnt);
	inp->inp_seclevel[SL_AUTH] = IPSEC_AUTH_LEVEL_DEFAULT;
	inp->inp_seclevel[SL_ESP_TRANS] = IPSEC_ESP_TRANS_LEVEL_DEFAULT;
	inp->inp_seclevel[SL_ESP_NETWORK] = IPSEC_ESP_NETWORK_LEVEL_DEFAULT;
	inp->inp_seclevel[SL_IPCOMP] = IPSEC_IPCOMP_LEVEL_DEFAULT;
	inp->inp_rtableid = curproc->p_p->ps_rtableid;
	inp->inp_hops = -1;
#ifdef INET6
	/*
	 * Small change in this function to set the INP_IPV6 flag so routines
	 * outside pcb-specific routines don't need to use sotopf(), and all
	 * of its pointer chasing, later.
	 */
	if (sotopf(so) == PF_INET6)
		inp->inp_flags = INP_IPV6;
	inp->inp_cksum6 = -1;
#endif /* INET6 */

	if (table->inpt_count++ > INPCBHASH_LOADFACTOR(table->inpt_size))
		(void)in_pcbresize(table, table->inpt_size * 2);
	TAILQ_INSERT_HEAD(&table->inpt_queue, inp, inp_queue);
	head = in_pcblhash(table, inp->inp_rtableid, inp->inp_lport);
	LIST_INSERT_HEAD(head, inp, inp_lhash);
#ifdef INET6
	if (sotopf(so) == PF_INET6)
		head = in6_pcbhash(table, rtable_l2(inp->inp_rtableid),
		    &inp->inp_faddr6, inp->inp_fport,
		    &inp->inp_laddr6, inp->inp_lport);
	else
#endif /* INET6 */
		head = in_pcbhash(table, rtable_l2(inp->inp_rtableid),
		    &inp->inp_faddr, inp->inp_fport,
		    &inp->inp_laddr, inp->inp_lport);
	LIST_INSERT_HEAD(head, inp, inp_hash);
	so->so_pcb = inp;

	return (0);
}

int
in_pcbbind(struct inpcb *inp, struct mbuf *nam, struct proc *p)
{
	struct socket *so = inp->inp_socket;
	u_int16_t lport = 0;
	int wild = 0;
	void *laddr = &zeroin46_addr;
	int error;

	if (inp->inp_lport)
		return (EINVAL);

	if ((so->so_options & (SO_REUSEADDR|SO_REUSEPORT)) == 0 &&
	    ((so->so_proto->pr_flags & PR_CONNREQUIRED) == 0 ||
	     (so->so_options & SO_ACCEPTCONN) == 0))
		wild = INPLOOKUP_WILDCARD;

	switch (sotopf(so)) {
#ifdef INET6
	case PF_INET6:
		if (!IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6))
			return (EINVAL);
		wild |= INPLOOKUP_IPV6;

		if (nam) {
			struct sockaddr_in6 *sin6;

			if ((error = in6_nam2sin6(nam, &sin6)))
				return (error);
			if ((error = in6_pcbaddrisavail(inp, sin6, wild, p)))
				return (error);
			laddr = &sin6->sin6_addr;
			lport = sin6->sin6_port;
		}
		break;
#endif
	case PF_INET:
		if (inp->inp_laddr.s_addr != INADDR_ANY)
			return (EINVAL);

		if (nam) {
			struct sockaddr_in *sin;

			if ((error = in_nam2sin(nam, &sin)))
				return (error);
			if ((error = in_pcbaddrisavail(inp, sin, wild, p)))
				return (error);
			laddr = &sin->sin_addr;
			lport = sin->sin_port;
		}
		break;
	default:
		return (EINVAL);
	}

	if (lport == 0) {
		if ((error = in_pcbpickport(&lport, laddr, wild, inp, p)))
			return (error);
	} else {
		if (in_rootonly(ntohs(lport), so->so_proto->pr_protocol) &&
		    suser(p) != 0)
			return (EACCES);
	}
	if (nam) {
		switch (sotopf(so)) {
#ifdef INET6
		case PF_INET6:
			inp->inp_laddr6 = *(struct in6_addr *)laddr;
			break;
#endif
		case PF_INET:
			inp->inp_laddr = *(struct in_addr *)laddr;
			break;
		}
	}
	inp->inp_lport = lport;
	in_pcbrehash(inp);
	return (0);
}

int
in_pcbaddrisavail(struct inpcb *inp, struct sockaddr_in *sin, int wild,
    struct proc *p)
{
	struct socket *so = inp->inp_socket;
	struct inpcbtable *table = inp->inp_table;
	u_int16_t lport = sin->sin_port;
	int reuseport = (so->so_options & SO_REUSEPORT);

	if (IN_MULTICAST(sin->sin_addr.s_addr)) {
		/*
		 * Treat SO_REUSEADDR as SO_REUSEPORT for multicast;
		 * allow complete duplication of binding if
		 * SO_REUSEPORT is set, or if SO_REUSEADDR is set
		 * and a multicast address is bound on both
		 * new and duplicated sockets.
		 */
		if (so->so_options & (SO_REUSEADDR|SO_REUSEPORT))
			reuseport = SO_REUSEADDR|SO_REUSEPORT;
	} else if (sin->sin_addr.s_addr != INADDR_ANY) {
		/*
		 * we must check that we are binding to an address we
		 * own except when:
		 * - SO_BINDANY is set or
		 * - we are binding a UDP socket to 255.255.255.255 or
		 * - we are binding a UDP socket to one of our broadcast
		 *   addresses
		 */
		if (!ISSET(so->so_options, SO_BINDANY) &&
		    !(so->so_type == SOCK_DGRAM &&
		    sin->sin_addr.s_addr == INADDR_BROADCAST) &&
		    !(so->so_type == SOCK_DGRAM &&
		    in_broadcast(sin->sin_addr, inp->inp_rtableid))) {
			struct ifaddr *ia;

			sin->sin_port = 0;
			memset(sin->sin_zero, 0, sizeof(sin->sin_zero));
			ia = ifa_ifwithaddr(sintosa(sin), inp->inp_rtableid);
			sin->sin_port = lport;

			if (ia == NULL)
				return (EADDRNOTAVAIL);
		}
	}
	if (lport) {
		struct inpcb *t;

		if (so->so_euid) {
			t = in_pcblookup_local(table, &sin->sin_addr, lport,
			    INPLOOKUP_WILDCARD, inp->inp_rtableid);
			if (t && (so->so_euid != t->inp_socket->so_euid))
				return (EADDRINUSE);
		}
		t = in_pcblookup_local(table, &sin->sin_addr, lport,
		    wild, inp->inp_rtableid);
		if (t && (reuseport & t->inp_socket->so_options) == 0)
			return (EADDRINUSE);
	}

	return (0);
}

int
in_pcbpickport(u_int16_t *lport, void *laddr, int wild, struct inpcb *inp,
    struct proc *p)
{
	struct socket *so = inp->inp_socket;
	struct inpcbtable *table = inp->inp_table;
	u_int16_t first, last, lower, higher, candidate, localport;
	int count;

	if (inp->inp_flags & INP_HIGHPORT) {
		first = ipport_hifirstauto;	/* sysctl */
		last = ipport_hilastauto;
	} else if (inp->inp_flags & INP_LOWPORT) {
		if (suser(p))
			return (EACCES);
		first = IPPORT_RESERVED-1; /* 1023 */
		last = 600;		   /* not IPPORT_RESERVED/2 */
	} else {
		first = ipport_firstauto;	/* sysctl */
		last = ipport_lastauto;
	}
	if (first < last) {
		lower = first;
		higher = last;
	} else {
		lower = last;
		higher = first;
	}

	/*
	 * Simple check to ensure all ports are not used up causing
	 * a deadlock here.
	 */

	count = higher - lower;
	candidate = lower + arc4random_uniform(count);

	do {
		if (count-- < 0)	/* completely used? */
			return (EADDRNOTAVAIL);
		++candidate;
		if (candidate < lower || candidate > higher)
			candidate = lower;
		localport = htons(candidate);
	} while (in_baddynamic(candidate, so->so_proto->pr_protocol) ||
	    in_pcblookup_local(table, laddr, localport, wild,
	    inp->inp_rtableid));
	*lport = localport;

	return (0);
}

/*
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sin.
 * If don't have a local address for this socket yet,
 * then pick one.
 */
int
in_pcbconnect(struct inpcb *inp, struct mbuf *nam)
{
	struct in_addr *ina = NULL;
	struct sockaddr_in *sin;
	int error;

#ifdef INET6
	if (sotopf(inp->inp_socket) == PF_INET6)
		return (in6_pcbconnect(inp, nam));
	KASSERT((inp->inp_flags & INP_IPV6) == 0);
#endif /* INET6 */

	if ((error = in_nam2sin(nam, &sin)))
		return (error);
	if (sin->sin_port == 0)
		return (EADDRNOTAVAIL);
	error = in_pcbselsrc(&ina, sin, inp);
	if (error)
		return (error);

	if (in_pcbhashlookup(inp->inp_table, sin->sin_addr, sin->sin_port,
	    *ina, inp->inp_lport, inp->inp_rtableid) != NULL)
		return (EADDRINUSE);

	KASSERT(inp->inp_laddr.s_addr == INADDR_ANY || inp->inp_lport);

	if (inp->inp_laddr.s_addr == INADDR_ANY) {
		if (inp->inp_lport == 0) {
			error = in_pcbbind(inp, NULL, curproc);
			if (error)
				return (error);
			if (in_pcbhashlookup(inp->inp_table, sin->sin_addr,
			    sin->sin_port, *ina, inp->inp_lport,
			    inp->inp_rtableid) != NULL) {
				inp->inp_lport = 0;
				return (EADDRINUSE);
			}
		}
		inp->inp_laddr = *ina;
	}
	inp->inp_faddr = sin->sin_addr;
	inp->inp_fport = sin->sin_port;
	in_pcbrehash(inp);
#ifdef IPSEC
	{
		/* Cause an IPsec SA to be established. */
		/* error is just ignored */
		ipsp_spd_inp(NULL, AF_INET, 0, &error, IPSP_DIRECTION_OUT,
		    NULL, inp, NULL);
	}
#endif
	return (0);
}

void
in_pcbdisconnect(struct inpcb *inp)
{
	switch (sotopf(inp->inp_socket)) {
#ifdef INET6
	case PF_INET6:
		inp->inp_faddr6 = in6addr_any;
		break;
#endif
	case PF_INET:
		inp->inp_faddr.s_addr = INADDR_ANY;
		break;
	}

	inp->inp_fport = 0;
	in_pcbrehash(inp);
	if (inp->inp_socket->so_state & SS_NOFDREF)
		in_pcbdetach(inp);
}

void
in_pcbdetach(struct inpcb *inp)
{
	struct socket *so = inp->inp_socket;

	NET_ASSERT_LOCKED();

	so->so_pcb = NULL;
	/*
	 * As long as the NET_LOCK() is the default lock for Internet
	 * sockets, do not release it to not introduce new sleeping
	 * points.
	 */
	sofree(so, SL_NOUNLOCK);
	m_freem(inp->inp_options);
	if (inp->inp_route.ro_rt) {
		rtfree(inp->inp_route.ro_rt);
		inp->inp_route.ro_rt = NULL;
	}
#ifdef INET6
	if (inp->inp_flags & INP_IPV6) {
		ip6_freepcbopts(inp->inp_outputopts6);
		ip6_freemoptions(inp->inp_moptions6);
	} else
#endif
		ip_freemoptions(inp->inp_moptions);
#if NPF > 0
	if (inp->inp_pf_sk) {
		pf_remove_divert_state(inp->inp_pf_sk);
		/* pf_remove_divert_state() may have detached the state */
		pf_inp_unlink(inp);
	}
#endif
	LIST_REMOVE(inp, inp_lhash);
	LIST_REMOVE(inp, inp_hash);
	TAILQ_REMOVE(&inp->inp_table->inpt_queue, inp, inp_queue);
	inp->inp_table->inpt_count--;
	in_pcbunref(inp);
}

struct inpcb *
in_pcbref(struct inpcb *inp)
{
	if (inp != NULL)
		refcnt_take(&inp->inp_refcnt);
	return inp;
}

void
in_pcbunref(struct inpcb *inp)
{
	if (refcnt_rele(&inp->inp_refcnt)) {
		KASSERT((LIST_NEXT(inp, inp_hash) == NULL) ||
		    (LIST_NEXT(inp, inp_hash) == _Q_INVALID));
		KASSERT((LIST_NEXT(inp, inp_lhash) == NULL) ||
		    (LIST_NEXT(inp, inp_lhash) == _Q_INVALID));
		KASSERT((TAILQ_NEXT(inp, inp_queue) == NULL) ||
		    (TAILQ_NEXT(inp, inp_queue) == _Q_INVALID));
		pool_put(&inpcb_pool, inp);
	}
}

void
in_setsockaddr(struct inpcb *inp, struct mbuf *nam)
{
	struct sockaddr_in *sin;

	nam->m_len = sizeof(*sin);
	sin = mtod(nam, struct sockaddr_in *);
	memset(sin, 0, sizeof(*sin));
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	sin->sin_port = inp->inp_lport;
	sin->sin_addr = inp->inp_laddr;
}

void
in_setpeeraddr(struct inpcb *inp, struct mbuf *nam)
{
	struct sockaddr_in *sin;

#ifdef INET6
	if (sotopf(inp->inp_socket) == PF_INET6) {
		in6_setpeeraddr(inp, nam);
		return;
	}
#endif /* INET6 */

	nam->m_len = sizeof(*sin);
	sin = mtod(nam, struct sockaddr_in *);
	memset(sin, 0, sizeof(*sin));
	sin->sin_family = AF_INET;
	sin->sin_len = sizeof(*sin);
	sin->sin_port = inp->inp_fport;
	sin->sin_addr = inp->inp_faddr;
}

/*
 * Pass some notification to all connections of a protocol
 * associated with address dst.  The "usual action" will be
 * taken, depending on the ctlinput cmd.  The caller must filter any
 * cmds that are uninteresting (e.g., no error in the map).
 * Call the protocol specific routine (if any) to report
 * any errors for each matching socket.
 */
void
in_pcbnotifyall(struct inpcbtable *table, struct sockaddr *dst, u_int rtable,
    int errno, void (*notify)(struct inpcb *, int))
{
	struct inpcb *inp, *ninp;
	struct in_addr faddr;
	u_int rdomain;

	NET_ASSERT_LOCKED();

#ifdef INET6
	/*
	 * See in6_pcbnotify() for IPv6 codepath.  By the time this
	 * gets called, the addresses passed are either definitely IPv4 or
	 * IPv6; *_pcbnotify() never gets called with v4-mapped v6 addresses.
	 */
#endif /* INET6 */

	if (dst->sa_family != AF_INET)
		return;
	faddr = satosin(dst)->sin_addr;
	if (faddr.s_addr == INADDR_ANY)
		return;

	rdomain = rtable_l2(rtable);
	TAILQ_FOREACH_SAFE(inp, &table->inpt_queue, inp_queue, ninp) {
#ifdef INET6
		if (inp->inp_flags & INP_IPV6)
			continue;
#endif
		if (inp->inp_faddr.s_addr != faddr.s_addr ||
		    rtable_l2(inp->inp_rtableid) != rdomain ||
		    inp->inp_socket == NULL) {
			continue;
		}
		if (notify)
			(*notify)(inp, errno);
	}
}

/*
 * Check for alternatives when higher level complains
 * about service problems.  For now, invalidate cached
 * routing information.  If the route was created dynamically
 * (by a redirect), time to try a default gateway again.
 */
void
in_losing(struct inpcb *inp)
{
	struct rtentry *rt = inp->inp_route.ro_rt;

	if (rt) {
		inp->inp_route.ro_rt = NULL;

		if (rt->rt_flags & RTF_DYNAMIC) {
			struct ifnet *ifp;

			ifp = if_get(rt->rt_ifidx);
			/*
			 * If the interface is gone, all its attached
			 * route entries have been removed from the table,
			 * so we're dealing with a stale cache and have
			 * nothing to do.
			 */
			if (ifp != NULL)
				rtdeletemsg(rt, ifp, inp->inp_rtableid);
			if_put(ifp);
		}
		/*
		 * A new route can be allocated
		 * the next time output is attempted.
		 * rtfree() needs to be called in anycase because the inp
		 * is still holding a reference to rt.
		 */
		rtfree(rt);
	}
}

/*
 * After a routing change, flush old routing
 * and allocate a (hopefully) better one.
 */
void
in_rtchange(struct inpcb *inp, int errno)
{
	if (inp->inp_route.ro_rt) {
		rtfree(inp->inp_route.ro_rt);
		inp->inp_route.ro_rt = 0;
		/*
		 * A new route can be allocated the next time
		 * output is attempted.
		 */
	}
}

struct inpcb *
in_pcblookup_local(struct inpcbtable *table, void *laddrp, u_int lport_arg,
    int flags, u_int rtable)
{
	struct inpcb *inp, *match = NULL;
	int matchwild = 3, wildcard;
	u_int16_t lport = lport_arg;
	struct in_addr laddr = *(struct in_addr *)laddrp;
#ifdef INET6
	struct in6_addr *laddr6 = (struct in6_addr *)laddrp;
#endif
	struct inpcbhead *head;
	u_int rdomain;

	rdomain = rtable_l2(rtable);
	head = in_pcblhash(table, rdomain, lport);
	LIST_FOREACH(inp, head, inp_lhash) {
		if (rtable_l2(inp->inp_rtableid) != rdomain)
			continue;
		if (inp->inp_lport != lport)
			continue;
		wildcard = 0;
#ifdef INET6
		if (ISSET(flags, INPLOOKUP_IPV6)) {
			if (!ISSET(inp->inp_flags, INP_IPV6))
				continue;

			if (!IN6_IS_ADDR_UNSPECIFIED(&inp->inp_faddr6))
				wildcard++;

			if (!IN6_ARE_ADDR_EQUAL(&inp->inp_laddr6, laddr6)) {
				if (IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6) ||
				    IN6_IS_ADDR_UNSPECIFIED(laddr6))
					wildcard++;
				else
					continue;
			}

		} else
#endif /* INET6 */
		{
#ifdef INET6
			if (ISSET(inp->inp_flags, INP_IPV6))
				continue;
#endif /* INET6 */

			if (inp->inp_faddr.s_addr != INADDR_ANY)
				wildcard++;

			if (inp->inp_laddr.s_addr != laddr.s_addr) {
				if (inp->inp_laddr.s_addr == INADDR_ANY ||
				    laddr.s_addr == INADDR_ANY)
					wildcard++;
				else
					continue;
			}

		}
		if ((!wildcard || (flags & INPLOOKUP_WILDCARD)) &&
		    wildcard < matchwild) {
			match = inp;
			if ((matchwild = wildcard) == 0)
				break;
		}
	}
	return (match);
}

struct rtentry *
in_pcbrtentry(struct inpcb *inp)
{
	struct route *ro;

	ro = &inp->inp_route;

	/* check if route is still valid */
	if (!rtisvalid(ro->ro_rt)) {
		rtfree(ro->ro_rt);
		ro->ro_rt = NULL;
	}

	/*
	 * No route yet, so try to acquire one.
	 */
	if (ro->ro_rt == NULL) {
#ifdef INET6
		memset(ro, 0, sizeof(struct route_in6));
#else
		memset(ro, 0, sizeof(struct route));
#endif

		switch(sotopf(inp->inp_socket)) {
#ifdef INET6
		case PF_INET6:
			if (IN6_IS_ADDR_UNSPECIFIED(&inp->inp_faddr6))
				break;
			ro->ro_dst.sa_family = AF_INET6;
			ro->ro_dst.sa_len = sizeof(struct sockaddr_in6);
			satosin6(&ro->ro_dst)->sin6_addr = inp->inp_faddr6;
			ro->ro_tableid = inp->inp_rtableid;
			ro->ro_rt = rtalloc_mpath(&ro->ro_dst,
			    &inp->inp_laddr6.s6_addr32[0], ro->ro_tableid);
			break;
#endif /* INET6 */
		case PF_INET:
			if (inp->inp_faddr.s_addr == INADDR_ANY)
				break;
			ro->ro_dst.sa_family = AF_INET;
			ro->ro_dst.sa_len = sizeof(struct sockaddr_in);
			satosin(&ro->ro_dst)->sin_addr = inp->inp_faddr;
			ro->ro_tableid = inp->inp_rtableid;
			ro->ro_rt = rtalloc_mpath(&ro->ro_dst,
			    &inp->inp_laddr.s_addr, ro->ro_tableid);
			break;
		}
	}
	return (ro->ro_rt);
}

/*
 * Return an IPv4 address, which is the most appropriate for a given
 * destination.
 * If necessary, this function lookups the routing table and returns
 * an entry to the caller for later use.
 */
int
in_pcbselsrc(struct in_addr **insrc, struct sockaddr_in *sin,
    struct inpcb *inp)
{
	struct ip_moptions *mopts = inp->inp_moptions;
	struct route *ro = &inp->inp_route;
	struct in_addr *laddr = &inp->inp_laddr;
	u_int rtableid = inp->inp_rtableid;

	struct sockaddr_in *sin2;
	struct in_ifaddr *ia = NULL;

	/*
	 * If the socket(if any) is already bound, use that bound address
	 * unless it is INADDR_ANY or INADDR_BROADCAST.
	 */
	if (laddr && laddr->s_addr != INADDR_ANY &&
	    laddr->s_addr != INADDR_BROADCAST) {
		*insrc = laddr;
		return (0);
	}

	/*
	 * If the destination address is multicast and an outgoing
	 * interface has been set as a multicast option, use the
	 * address of that interface as our source address.
	 */
	if (IN_MULTICAST(sin->sin_addr.s_addr) && mopts != NULL) {
		struct ifnet *ifp;

		ifp = if_get(mopts->imo_ifidx);
		if (ifp != NULL) {
			if (ifp->if_rdomain == rtable_l2(rtableid))
				IFP_TO_IA(ifp, ia);
			if (ia == NULL) {
				if_put(ifp);
				return (EADDRNOTAVAIL);
			}

			*insrc = &ia->ia_addr.sin_addr;
			if_put(ifp);
			return (0);
		}
	}
	/*
	 * If route is known or can be allocated now,
	 * our src addr is taken from the i/f, else punt.
	 */
	if (!rtisvalid(ro->ro_rt) || (ro->ro_tableid != rtableid) ||
	    (satosin(&ro->ro_dst)->sin_addr.s_addr != sin->sin_addr.s_addr)) {
		rtfree(ro->ro_rt);
		ro->ro_rt = NULL;
	}
	if (ro->ro_rt == NULL) {
		/* No route yet, so try to acquire one */
		ro->ro_dst.sa_family = AF_INET;
		ro->ro_dst.sa_len = sizeof(struct sockaddr_in);
		satosin(&ro->ro_dst)->sin_addr = sin->sin_addr;
		ro->ro_tableid = rtableid;
		ro->ro_rt = rtalloc_mpath(&ro->ro_dst, NULL, ro->ro_tableid);

		/*
		 * It is important to zero out the rest of the
		 * struct sockaddr_in when mixing v6 & v4!
		 */
		sin2 = satosin(&ro->ro_dst);
		memset(sin2->sin_zero, 0, sizeof(sin2->sin_zero));
	}
	/*
	 * If we found a route, use the address
	 * corresponding to the outgoing interface.
	 */
	if (ro->ro_rt != NULL)
		ia = ifatoia(ro->ro_rt->rt_ifa);

	if (ia == NULL)
		return (EADDRNOTAVAIL);

	*insrc = &ia->ia_addr.sin_addr;
	return (0);
}

void
in_pcbrehash(struct inpcb *inp)
{
	struct inpcbtable *table = inp->inp_table;
	struct inpcbhead *head;

	NET_ASSERT_LOCKED();

	LIST_REMOVE(inp, inp_lhash);
	head = in_pcblhash(table, inp->inp_rtableid, inp->inp_lport);
	LIST_INSERT_HEAD(head, inp, inp_lhash);
	LIST_REMOVE(inp, inp_hash);
#ifdef INET6
	if (inp->inp_flags & INP_IPV6)
		head = in6_pcbhash(table, rtable_l2(inp->inp_rtableid),
		    &inp->inp_faddr6, inp->inp_fport,
		    &inp->inp_laddr6, inp->inp_lport);
	else
#endif /* INET6 */
		head = in_pcbhash(table, rtable_l2(inp->inp_rtableid),
		    &inp->inp_faddr, inp->inp_fport,
		    &inp->inp_laddr, inp->inp_lport);
	LIST_INSERT_HEAD(head, inp, inp_hash);
}

int
in_pcbresize(struct inpcbtable *table, int hashsize)
{
	u_long nmask, nlmask;
	int osize;
	void *nhashtbl, *nlhashtbl, *ohashtbl, *olhashtbl;
	struct inpcb *inp;

	ohashtbl = table->inpt_hashtbl;
	olhashtbl = table->inpt_lhashtbl;
	osize = table->inpt_size;

	nhashtbl = hashinit(hashsize, M_PCB, M_NOWAIT, &nmask);
	if (nhashtbl == NULL)
		return ENOBUFS;
	nlhashtbl = hashinit(hashsize, M_PCB, M_NOWAIT, &nlmask);
	if (nlhashtbl == NULL) {
		hashfree(nhashtbl, hashsize, M_PCB);
		return ENOBUFS;
	}
	table->inpt_hashtbl = nhashtbl;
	table->inpt_lhashtbl = nlhashtbl;
	table->inpt_mask = nmask;
	table->inpt_lmask = nlmask;
	table->inpt_size = hashsize;
	arc4random_buf(&table->inpt_key, sizeof(table->inpt_key));
	arc4random_buf(&table->inpt_lkey, sizeof(table->inpt_lkey));

	TAILQ_FOREACH(inp, &table->inpt_queue, inp_queue) {
		in_pcbrehash(inp);
	}
	hashfree(ohashtbl, osize, M_PCB);
	hashfree(olhashtbl, osize, M_PCB);

	return (0);
}

#ifdef DIAGNOSTIC
int	in_pcbnotifymiss = 0;
#endif

/*
 * The in(6)_pcbhashlookup functions are used to locate connected sockets
 * quickly:
 *     faddr.fport <-> laddr.lport
 * No wildcard matching is done so that listening sockets are not found.
 * If the functions return NULL in(6)_pcblookup_listen can be used to
 * find a listening/bound socket that may accept the connection.
 * After those two lookups no other are necessary.
 */
struct inpcb *
in_pcbhashlookup(struct inpcbtable *table, struct in_addr faddr,
    u_int fport_arg, struct in_addr laddr, u_int lport_arg, u_int rtable)
{
	struct inpcbhead *head;
	struct inpcb *inp;
	u_int16_t fport = fport_arg, lport = lport_arg;
	u_int rdomain;

	rdomain = rtable_l2(rtable);
	head = in_pcbhash(table, rdomain, &faddr, fport, &laddr, lport);
	LIST_FOREACH(inp, head, inp_hash) {
#ifdef INET6
		if (inp->inp_flags & INP_IPV6)
			continue;	/*XXX*/
#endif
		if (inp->inp_faddr.s_addr == faddr.s_addr &&
		    inp->inp_fport == fport && inp->inp_lport == lport &&
		    inp->inp_laddr.s_addr == laddr.s_addr &&
		    rtable_l2(inp->inp_rtableid) == rdomain) {
			/*
			 * Move this PCB to the head of hash chain so that
			 * repeated accesses are quicker.  This is analogous to
			 * the historic single-entry PCB cache.
			 */
			if (inp != LIST_FIRST(head)) {
				LIST_REMOVE(inp, inp_hash);
				LIST_INSERT_HEAD(head, inp, inp_hash);
			}
			break;
		}
	}
#ifdef DIAGNOSTIC
	if (inp == NULL && in_pcbnotifymiss) {
		printf("%s: faddr=%08x fport=%d laddr=%08x lport=%d rdom=%u\n",
		    __func__, ntohl(faddr.s_addr), ntohs(fport),
		    ntohl(laddr.s_addr), ntohs(lport), rdomain);
	}
#endif
	return (inp);
}

/*
 * The in(6)_pcblookup_listen functions are used to locate listening
 * sockets quickly.  This are sockets with unspecified foreign address
 * and port:
 *		*.*     <-> laddr.lport
 *		*.*     <->     *.lport
 */
struct inpcb *
in_pcblookup_listen(struct inpcbtable *table, struct in_addr laddr,
    u_int lport_arg, struct mbuf *m, u_int rtable)
{
	struct inpcbhead *head;
	const struct in_addr *key1, *key2;
	struct inpcb *inp;
	u_int16_t lport = lport_arg;
	u_int rdomain;

	key1 = &laddr;
	key2 = &zeroin_addr;
#if NPF > 0
	if (m && m->m_pkthdr.pf.flags & PF_TAG_DIVERTED) {
		struct pf_divert *divert;

		divert = pf_find_divert(m);
		KASSERT(divert != NULL);
		switch (divert->type) {
		case PF_DIVERT_TO:
			key1 = key2 = &divert->addr.v4;
			lport = divert->port;
			break;
		case PF_DIVERT_REPLY:
			return (NULL);
		default:
			panic("%s: unknown divert type %d, mbuf %p, divert %p",
			    __func__, divert->type, m, divert);
		}
	} else if (m && m->m_pkthdr.pf.flags & PF_TAG_TRANSLATE_LOCALHOST) {
		/*
		 * Redirected connections should not be treated the same
		 * as connections directed to 127.0.0.0/8 since localhost
		 * can only be accessed from the host itself.
		 * For example portmap(8) grants more permissions for
		 * connections to the socket bound to 127.0.0.1 than
		 * to the * socket.
		 */
		key1 = &zeroin_addr;
		key2 = &laddr;
	}
#endif

	rdomain = rtable_l2(rtable);
	head = in_pcbhash(table, rdomain, &zeroin_addr, 0, key1, lport);
	LIST_FOREACH(inp, head, inp_hash) {
#ifdef INET6
		if (inp->inp_flags & INP_IPV6)
			continue;	/*XXX*/
#endif
		if (inp->inp_lport == lport && inp->inp_fport == 0 &&
		    inp->inp_laddr.s_addr == key1->s_addr &&
		    inp->inp_faddr.s_addr == INADDR_ANY &&
		    rtable_l2(inp->inp_rtableid) == rdomain)
			break;
	}
	if (inp == NULL && key1->s_addr != key2->s_addr) {
		head = in_pcbhash(table, rdomain,
		    &zeroin_addr, 0, key2, lport);
		LIST_FOREACH(inp, head, inp_hash) {
#ifdef INET6
			if (inp->inp_flags & INP_IPV6)
				continue;	/*XXX*/
#endif
			if (inp->inp_lport == lport && inp->inp_fport == 0 &&
			    inp->inp_laddr.s_addr == key2->s_addr &&
			    inp->inp_faddr.s_addr == INADDR_ANY &&
			    rtable_l2(inp->inp_rtableid) == rdomain)
				break;
		}
	}
	/*
	 * Move this PCB to the head of hash chain so that
	 * repeated accesses are quicker.  This is analogous to
	 * the historic single-entry PCB cache.
	 */
	if (inp != NULL && inp != LIST_FIRST(head)) {
		LIST_REMOVE(inp, inp_hash);
		LIST_INSERT_HEAD(head, inp, inp_hash);
	}
#ifdef DIAGNOSTIC
	if (inp == NULL && in_pcbnotifymiss) {
		printf("%s: laddr=%08x lport=%d rdom=%u\n",
		    __func__, ntohl(laddr.s_addr), ntohs(lport), rdomain);
	}
#endif
	return (inp);
}
