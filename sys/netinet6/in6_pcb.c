/*	$OpenBSD: in6_pcb.c,v 1.108 2018/10/04 17:33:41 bluhm Exp $	*/

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
 * Copyright (c) 1982, 1986, 1990, 1993, 1995
 *	Regents of the University of California.  All rights reserved.
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
 */

#include "pf.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/pfvar.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>

#include <netinet6/in6_var.h>

const struct in6_addr zeroin6_addr;

struct inpcbhead *
in6_pcbhash(struct inpcbtable *table, int rdom,
    const struct in6_addr *faddr, u_short fport,
    const struct in6_addr *laddr, u_short lport)
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

int
in6_pcbaddrisavail(struct inpcb *inp, struct sockaddr_in6 *sin6, int wild,
    struct proc *p)
{
	struct socket *so = inp->inp_socket;
	struct inpcbtable *table = inp->inp_table;
	u_short lport = sin6->sin6_port;
	int reuseport = (so->so_options & SO_REUSEPORT);

	wild |= INPLOOKUP_IPV6;
	/* KAME hack: embed scopeid */
	if (in6_embedscope(&sin6->sin6_addr, sin6, inp) != 0)
		return (EINVAL);
	/* this must be cleared for ifa_ifwithaddr() */
	sin6->sin6_scope_id = 0;
	/* reject IPv4 mapped address, we have no support for it */
	if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
		return (EADDRNOTAVAIL);

	if (IN6_IS_ADDR_MULTICAST(&sin6->sin6_addr)) {
		/*
		 * Treat SO_REUSEADDR as SO_REUSEPORT for multicast;
		 * allow complete duplication of binding if
		 * SO_REUSEPORT is set, or if SO_REUSEADDR is set
		 * and a multicast address is bound on both
		 * new and duplicated sockets.
		 */
		if (so->so_options & (SO_REUSEADDR|SO_REUSEPORT))
			reuseport = SO_REUSEADDR | SO_REUSEPORT;
	} else if (!IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
		struct ifaddr *ifa = NULL;

		sin6->sin6_port = 0;  /*
				       * Yechhhh, because of upcoming
				       * call to ifa_ifwithaddr(), which
				       * does bcmp's over the PORTS as
				       * well.  (What about flow?)
				       */
		sin6->sin6_flowinfo = 0;
		if (!(so->so_options & SO_BINDANY) &&
		    (ifa = ifa_ifwithaddr(sin6tosa(sin6),
		    inp->inp_rtableid)) == NULL)
			return (EADDRNOTAVAIL);
		sin6->sin6_port = lport;

		/*
		 * bind to an anycast address might accidentally
		 * cause sending a packet with an anycast source
		 * address, so we forbid it.
		 *
		 * We should allow to bind to a deprecated address,
		 * since the application dare to use it.
		 * But, can we assume that they are careful enough
		 * to check if the address is deprecated or not?
		 * Maybe, as a safeguard, we should have a setsockopt
		 * flag to control the bind(2) behavior against
		 * deprecated addresses (default: forbid bind(2)).
		 */
		if (ifa && ifatoia6(ifa)->ia6_flags & (IN6_IFF_ANYCAST|
		    IN6_IFF_TENTATIVE|IN6_IFF_DUPLICATED|IN6_IFF_DETACHED))
			return (EADDRNOTAVAIL);
	}
	if (lport) {
		struct inpcb *t;

		if (so->so_euid) {
			t = in_pcblookup_local(table,
			    (struct in_addr *)&sin6->sin6_addr, lport,
			    INPLOOKUP_WILDCARD | INPLOOKUP_IPV6,
			    inp->inp_rtableid);
			if (t && (so->so_euid != t->inp_socket->so_euid))
				return (EADDRINUSE);
		}
		t = in_pcblookup_local(table,
		    (struct in_addr *)&sin6->sin6_addr, lport,
		    wild, inp->inp_rtableid);
		if (t && (reuseport & t->inp_socket->so_options) == 0)
			return (EADDRINUSE);
	}
	return (0);
}

/*
 * Connect from a socket to a specified address.
 * Both address and port must be specified in argument sin6.
 * Eventually, flow labels will have to be dealt with here, as well.
 *
 * If don't have a local address for this socket yet,
 * then pick one.
 */
int
in6_pcbconnect(struct inpcb *inp, struct mbuf *nam)
{
	struct in6_addr *in6a = NULL;
	struct sockaddr_in6 *sin6;
	int error;
	struct sockaddr_in6 tmp;

	KASSERT(inp->inp_flags & INP_IPV6);

	if ((error = in6_nam2sin6(nam, &sin6)))
		return (error);
	if (sin6->sin6_port == 0)
		return (EADDRNOTAVAIL);
	/* reject IPv4 mapped address, we have no support for it */
	if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr))
		return (EADDRNOTAVAIL);

	/* protect *sin6 from overwrites */
	tmp = *sin6;
	sin6 = &tmp;

	/* KAME hack: embed scopeid */
	if (in6_embedscope(&sin6->sin6_addr, sin6, inp) != 0)
		return EINVAL;
	/* this must be cleared for ifa_ifwithaddr() */
	sin6->sin6_scope_id = 0;

	/* Source address selection. */
	/*
	 * XXX: in6_selectsrc might replace the bound local address
	 * with the address specified by setsockopt(IPV6_PKTINFO).
	 * Is it the intended behavior?
	 */
	error = in6_pcbselsrc(&in6a, sin6, inp, inp->inp_outputopts6);
	if (error)
		return (error);

	inp->inp_ipv6.ip6_hlim = (u_int8_t)in6_selecthlim(inp);

	if (in6_pcbhashlookup(inp->inp_table, &sin6->sin6_addr, sin6->sin6_port,
	    IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6) ? in6a : &inp->inp_laddr6,
	    inp->inp_lport, inp->inp_rtableid) != NULL) {
		return (EADDRINUSE);
	}

	KASSERT(IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6) || inp->inp_lport);

	if (IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6)) {
		if (inp->inp_lport == 0) {
			error = in_pcbbind(inp, NULL, curproc);
			if (error)
				return (error);
			if (in6_pcbhashlookup(inp->inp_table, &sin6->sin6_addr,
			    sin6->sin6_port, in6a, inp->inp_lport,
			    inp->inp_rtableid) != NULL) {
				inp->inp_lport = 0;
				return (EADDRINUSE);
			}
		}
		inp->inp_laddr6 = *in6a;
	}
	inp->inp_faddr6 = sin6->sin6_addr;
	inp->inp_fport = sin6->sin6_port;
	inp->inp_flowinfo &= ~IPV6_FLOWLABEL_MASK;
	if (ip6_auto_flowlabel)
		inp->inp_flowinfo |=
		    (htonl(ip6_randomflowlabel()) & IPV6_FLOWLABEL_MASK);
	in_pcbrehash(inp);
	return (0);
}

/*
 * Get the local address/port, and put it in a sockaddr_in6.
 * This services the getsockname(2) call.
 */
int
in6_setsockaddr(struct inpcb *inp, struct mbuf *nam)
{
	struct sockaddr_in6 *sin6;

	nam->m_len = sizeof(struct sockaddr_in6);
	sin6 = mtod(nam,struct sockaddr_in6 *);

	bzero ((caddr_t)sin6,sizeof(struct sockaddr_in6));
	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	sin6->sin6_port = inp->inp_lport;
	sin6->sin6_addr = inp->inp_laddr6;
	/* KAME hack: recover scopeid */
	in6_recoverscope(sin6, &inp->inp_laddr6);

	return 0;
}

/*
 * Get the foreign address/port, and put it in a sockaddr_in6.
 * This services the getpeername(2) call.
 */
int
in6_setpeeraddr(struct inpcb *inp, struct mbuf *nam)
{
	struct sockaddr_in6 *sin6;

	nam->m_len = sizeof(struct sockaddr_in6);
	sin6 = mtod(nam,struct sockaddr_in6 *);

	bzero ((caddr_t)sin6,sizeof(struct sockaddr_in6));
	sin6->sin6_family = AF_INET6;
	sin6->sin6_len = sizeof(struct sockaddr_in6);
	sin6->sin6_port = inp->inp_fport;
	sin6->sin6_addr = inp->inp_faddr6;
	/* KAME hack: recover scopeid */
	in6_recoverscope(sin6, &inp->inp_faddr6);

	return 0;
}

/*
 * Pass some notification to all connections of a protocol
 * associated with address dst.  The local address and/or port numbers
 * may be specified to limit the search.  The "usual action" will be
 * taken, depending on the ctlinput cmd.  The caller must filter any
 * cmds that are uninteresting (e.g., no error in the map).
 * Call the protocol specific routine (if any) to report
 * any errors for each matching socket.
 *
 * Also perform input-side security policy check
 *    once PCB to be notified has been located.
 */
int
in6_pcbnotify(struct inpcbtable *table, struct sockaddr_in6 *dst,
    uint fport_arg, const struct sockaddr_in6 *src, uint lport_arg,
    u_int rtable, int cmd, void *cmdarg, void (*notify)(struct inpcb *, int))
{
	struct inpcb *inp, *ninp;
	u_short fport = fport_arg, lport = lport_arg;
	struct sockaddr_in6 sa6_src;
	int errno, nmatch = 0;
	u_int32_t flowinfo;
	u_int rdomain;

	NET_ASSERT_LOCKED();

	if ((unsigned)cmd >= PRC_NCMDS)
		return (0);

	if (IN6_IS_ADDR_UNSPECIFIED(&dst->sin6_addr))
		return (0);
	if (IN6_IS_ADDR_V4MAPPED(&dst->sin6_addr)) {
#ifdef DIAGNOSTIC
		printf("Huh?  Thought in6_pcbnotify() never got "
		       "called with mapped!\n");
#endif
		return (0);
	}

	/*
	 * note that src can be NULL when we get notify by local fragmentation.
	 */
	sa6_src = (src == NULL) ? sa6_any : *src;
	flowinfo = sa6_src.sin6_flowinfo;

	/*
	 * Redirects go to all references to the destination,
	 * and use in_rtchange to invalidate the route cache.
	 * Dead host indications: also use in_rtchange to invalidate
	 * the cache, and deliver the error to all the sockets.
	 * Otherwise, if we have knowledge of the local port and address,
	 * deliver only to that socket.
	 */
	if (PRC_IS_REDIRECT(cmd) || cmd == PRC_HOSTDEAD) {
		fport = 0;
		lport = 0;
		sa6_src.sin6_addr = in6addr_any;

		if (cmd != PRC_HOSTDEAD)
			notify = in_rtchange;
	}
	errno = inet6ctlerrmap[cmd];

	rdomain = rtable_l2(rtable);
	TAILQ_FOREACH_SAFE(inp, &table->inpt_queue, inp_queue, ninp) {
		if ((inp->inp_flags & INP_IPV6) == 0)
			continue;

		/*
		 * Under the following condition, notify of redirects
		 * to the pcb, without making address matches against inpcb.
		 * - redirect notification is arrived.
		 * - the inpcb is unconnected.
		 * - the inpcb is caching !RTF_HOST routing entry.
		 * - the ICMPv6 notification is from the gateway cached in the
		 *   inpcb.  i.e. ICMPv6 notification is from nexthop gateway
		 *   the inpcb used very recently.
		 *
		 * This is to improve interaction between netbsd/openbsd
		 * redirect handling code, and inpcb route cache code.
		 * without the clause, !RTF_HOST routing entry (which carries
		 * gateway used by inpcb right before the ICMPv6 redirect)
		 * will be cached forever in unconnected inpcb.
		 *
		 * There still is a question regarding to what is TRT:
		 * - On bsdi/freebsd, RTF_HOST (cloned) routing entry will be
		 *   generated on packet output.  inpcb will always cache
		 *   RTF_HOST routing entry so there's no need for the clause
		 *   (ICMPv6 redirect will update RTF_HOST routing entry,
		 *   and inpcb is caching it already).
		 *   However, bsdi/freebsd are vulnerable to local DoS attacks
		 *   due to the cloned routing entries.
		 * - Specwise, "destination cache" is mentioned in RFC2461.
		 *   Jinmei says that it implies bsdi/freebsd behavior, itojun
		 *   is not really convinced.
		 * - Having hiwat/lowat on # of cloned host route (redirect/
		 *   pmtud) may be a good idea.  netbsd/openbsd has it.  see
		 *   icmp6_mtudisc_update().
		 */
		if ((PRC_IS_REDIRECT(cmd) || cmd == PRC_HOSTDEAD) &&
		    IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6) &&
		    inp->inp_route.ro_rt &&
		    !(inp->inp_route.ro_rt->rt_flags & RTF_HOST)) {
			struct sockaddr_in6 *dst6;

			dst6 = satosin6(&inp->inp_route.ro_dst);
			if (IN6_ARE_ADDR_EQUAL(&dst6->sin6_addr,
			    &dst->sin6_addr))
				goto do_notify;
		}

		/*
		 * Detect if we should notify the error. If no source and
		 * destination ports are specified, but non-zero flowinfo and
		 * local address match, notify the error. This is the case
		 * when the error is delivered with an encrypted buffer
		 * by ESP. Otherwise, just compare addresses and ports
		 * as usual.
		 */
		if (lport == 0 && fport == 0 && flowinfo &&
		    inp->inp_socket != NULL &&
		    flowinfo == (inp->inp_flowinfo & IPV6_FLOWLABEL_MASK) &&
		    IN6_ARE_ADDR_EQUAL(&inp->inp_laddr6, &sa6_src.sin6_addr))
			goto do_notify;
		else if (!IN6_ARE_ADDR_EQUAL(&inp->inp_faddr6,
					     &dst->sin6_addr) ||
			 rtable_l2(inp->inp_rtableid) != rdomain ||
			 inp->inp_socket == NULL ||
			 (lport && inp->inp_lport != lport) ||
			 (!IN6_IS_ADDR_UNSPECIFIED(&sa6_src.sin6_addr) &&
			  !IN6_ARE_ADDR_EQUAL(&inp->inp_laddr6,
					      &sa6_src.sin6_addr)) ||
			 (fport && inp->inp_fport != fport)) {
			continue;
		}
	  do_notify:
		nmatch++;
		if (notify)
			(*notify)(inp, errno);
	}
	return (nmatch);
}

struct inpcb *
in6_pcbhashlookup(struct inpcbtable *table, const struct in6_addr *faddr,
    u_int fport_arg, const struct in6_addr *laddr, u_int lport_arg,
    u_int rtable)
{
	struct inpcbhead *head;
	struct inpcb *inp;
	u_int16_t fport = fport_arg, lport = lport_arg;
	u_int rdomain;

	rdomain = rtable_l2(rtable);
	head = in6_pcbhash(table, rdomain, faddr, fport, laddr, lport);
	LIST_FOREACH(inp, head, inp_hash) {
		if (!(inp->inp_flags & INP_IPV6))
			continue;
		if (IN6_ARE_ADDR_EQUAL(&inp->inp_faddr6, faddr) &&
		    inp->inp_fport == fport && inp->inp_lport == lport &&
		    IN6_ARE_ADDR_EQUAL(&inp->inp_laddr6, laddr) &&
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
		printf("%s: faddr= fport=%d laddr= lport=%d rdom=%u\n",
		    __func__, ntohs(fport), ntohs(lport), rdomain);
	}
#endif
	return (inp);
}

struct inpcb *
in6_pcblookup_listen(struct inpcbtable *table, struct in6_addr *laddr,
    u_int lport_arg, struct mbuf *m, u_int rtable)
{
	struct inpcbhead *head;
	const struct in6_addr *key1, *key2;
	struct inpcb *inp;
	u_int16_t lport = lport_arg;
	u_int rdomain;

	key1 = laddr;
	key2 = &zeroin6_addr;
#if NPF > 0
	if (m && m->m_pkthdr.pf.flags & PF_TAG_DIVERTED) {
		struct pf_divert *divert;

		divert = pf_find_divert(m);
		KASSERT(divert != NULL);
		switch (divert->type) {
		case PF_DIVERT_TO:
			key1 = key2 = &divert->addr.v6;
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
		 * as connections directed to ::1 since localhost
		 * can only be accessed from the host itself.
		 */
		key1 = &zeroin6_addr;
		key2 = laddr;
	}
#endif

	rdomain = rtable_l2(rtable);
	head = in6_pcbhash(table, rdomain, &zeroin6_addr, 0, key1, lport);
	LIST_FOREACH(inp, head, inp_hash) {
		if (!(inp->inp_flags & INP_IPV6))
			continue;
		if (inp->inp_lport == lport && inp->inp_fport == 0 &&
		    IN6_ARE_ADDR_EQUAL(&inp->inp_laddr6, key1) &&
		    IN6_IS_ADDR_UNSPECIFIED(&inp->inp_faddr6) &&
		    rtable_l2(inp->inp_rtableid) == rdomain)
			break;
	}
	if (inp == NULL && ! IN6_ARE_ADDR_EQUAL(key1, key2)) {
		head = in6_pcbhash(table, rdomain,
		    &zeroin6_addr, 0, key2, lport);
		LIST_FOREACH(inp, head, inp_hash) {
			if (!(inp->inp_flags & INP_IPV6))
				continue;
			if (inp->inp_lport == lport && inp->inp_fport == 0 &&
			    IN6_ARE_ADDR_EQUAL(&inp->inp_laddr6, key2) &&
			    IN6_IS_ADDR_UNSPECIFIED(&inp->inp_faddr6) &&
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
		printf("%s: laddr= lport=%d rdom=%u\n",
		    __func__, ntohs(lport), rdomain);
	}
#endif
	return (inp);
}
