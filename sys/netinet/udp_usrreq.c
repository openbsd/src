/*	$OpenBSD: udp_usrreq.c,v 1.36 2000/01/04 10:38:36 itojun Exp $	*/
/*	$NetBSD: udp_usrreq.c,v 1.28 1996/03/16 23:54:03 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
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
 *	@(#)udp_usrreq.c	8.4 (Berkeley) 1/21/94
 */

/*
%%% portions-copyright-nrl-95
Portions of this software are Copyright 1995-1998 by Randall Atkinson,
Ronald Lee, Daniel McDonald, Bao Phan, and Chris Winters. All Rights
Reserved. All rights under this copyright have been assigned to the US
Naval Research Laboratory (NRL). The NRL Copyright Notice and License
Agreement Version 1.1 (January 17, 1995) applies to these portions of the
software.
You should have received a copy of the license with this software. If you
didn't get a copy, you may request one from <license@ipv6.nrl.navy.mil>.
*/

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#ifdef IPSEC
#include <netinet/ip_ipsp.h>

extern int     	check_ipsec_policy  __P((struct inpcb *, u_int32_t));
#endif

#include <machine/stdarg.h>

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/ip6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#include <netinet6/icmp6.h>
#include <netinet6/ip6protosw.h>

extern int ip6_defhlim;
#endif /* INET6 */

/*
 * UDP protocol implementation.
 * Per RFC 768, August, 1980.
 */
int	udpcksum = 1;


static	void udp_detach __P((struct inpcb *));
static	void udp_notify __P((struct inpcb *, int));
static	struct mbuf *udp_saveopt __P((caddr_t, int, int));

#ifndef UDBHASHSIZE
#define	UDBHASHSIZE	128
#endif
int	udbhashsize = UDBHASHSIZE;

/* from in_pcb.c */
extern	struct baddynamicports baddynamicports;

void
udp_init()
{

	in_pcbinit(&udbtable, udbhashsize);
}

#if defined(INET6) && !defined(TCP6)
int
udp6_input(mp, offp, proto)
	struct mbuf **mp;
	int *offp, proto;
{
	struct mbuf *m = *mp;

#if defined(NFAITH) && 0 < NFAITH
	if (m->m_pkthdr.rcvif) {
		if (m->m_pkthdr.rcvif->if_type == IFT_FAITH) {
			/* XXX send icmp6 host/port unreach? */
			m_freem(m);
			return IPPROTO_DONE;
		}
	}
#endif

	udp_input(m, *offp, proto);
	return IPPROTO_DONE;
}
#endif

void
#if __STDC__
udp_input(struct mbuf *m, ...)
#else
udp_input(m, va_alist)
	struct mbuf *m;
	va_dcl
#endif
{
	register struct ip *ip;
	register struct udphdr *uh;
	register struct inpcb *inp;
	struct mbuf *opts = 0;
	int len;
	struct ip save_ip;
	int iphlen;
	va_list ap;
	u_int16_t savesum;
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
#ifdef INET6
		struct sockaddr_in6 sin6;
#endif /* INET6 */
	} srcsa, dstsa;
#ifdef INET6
	struct ip6_hdr *ipv6;
#endif /* INET6 */
#ifdef IPSEC
	struct tdb  *tdb = NULL;
#endif /* IPSEC */

	va_start(ap, m);
	iphlen = va_arg(ap, int);
	va_end(ap);

	udpstat.udps_ipackets++;

#ifdef IPSEC
	/* Save the last SA which was used to process the mbuf */
	if ((m->m_flags & (M_CONF|M_AUTH)) && m->m_pkthdr.tdbi) {
		struct tdb_ident *tdbi = m->m_pkthdr.tdbi;
		/* XXX gettdb() should really be called at spltdb().      */
		/* XXX this is splsoftnet(), currently they are the same. */
		tdb = gettdb(tdbi->spi, &tdbi->dst, tdbi->proto);
		free(m->m_pkthdr.tdbi, M_TEMP);
		m->m_pkthdr.tdbi = NULL;
	}
#endif /* IPSEC */

	switch (mtod(m, struct ip *)->ip_v) {
	case 4:
		ip = mtod(m, struct ip *);
#ifdef INET6
		ipv6 = NULL;
#endif /* INET6 */
		srcsa.sa.sa_family = AF_INET;
		break;
#ifdef INET6
	case 6:
		ip = NULL;
		ipv6 = mtod(m, struct ip6_hdr *);
		srcsa.sa.sa_family = AF_INET6;
		break;
#endif /* INET6 */
	default:
		printf("udp_input: received unknown IP version %d",
		    mtod(m, struct ip *)->ip_v);
		goto bad;
	}

	/*
	 * Strip IP options, if any; should skip this,
	 * make available to user, and use on returned packets,
	 * but we don't yet have a way to check the checksum
	 * with options still present.
	 */
	/*
	 * (contd. from above...)  Furthermore, we may want to strip options
	 * for such things as ICMP errors, where options just get in the way.
	 */
	if (ip && iphlen > sizeof (struct ip)) {
		ip_stripoptions(m, (struct mbuf *)0);
		iphlen = sizeof(struct ip);
	}

	/*
	 * Get IP and UDP header together in first mbuf.
	 */
	if (m->m_len < iphlen + sizeof(struct udphdr)) {
		if ((m = m_pullup2(m, iphlen + sizeof(struct udphdr))) == 0) {
			udpstat.udps_hdrops++;
			return;
		}
#ifdef INET6
		if (ipv6)
			ipv6 = mtod(m, struct ip6_hdr *);
		else
#endif /* INET6 */
			ip = mtod(m, struct ip *);
	}
	uh = (struct udphdr *)(mtod(m, caddr_t) + iphlen);

	/*
	 * Make mbuf data length reflect UDP length.
	 * If not enough data to reflect UDP length, drop.
	 */
	len = ntohs((u_int16_t)uh->uh_ulen);
	if (m->m_pkthdr.len - iphlen != len) {
		if (len > (m->m_pkthdr.len - iphlen) ||
			len < sizeof(struct udphdr)) {
			udpstat.udps_badlen++;
			goto bad;
		}
		m_adj(m, len - (m->m_pkthdr.len - iphlen));
	}
	/*
	 * Save a copy of the IP header in case we want restore it
	 * for sending an ICMP error message in response.
	 */
	if (ip)
		save_ip = *ip;

	/*
	 * Checksum extended UDP header and data.
	 * from W.R.Stevens: check incoming udp cksums even if
	 *	udpcksum is not set.
	 */
	savesum = uh->uh_sum;
#ifdef INET6
	if (ipv6) {
		/* Be proactive about malicious use of IPv4 mapped address */
		if (IN6_IS_ADDR_V4MAPPED(&ipv6->ip6_src) ||
		    IN6_IS_ADDR_V4MAPPED(&ipv6->ip6_dst)) {
			/* XXX stat */
			goto bad;
		}

		/*
		 * In IPv6, the UDP checksum is ALWAYS used.
		 */
		if ((uh->uh_sum = in6_cksum(m, IPPROTO_UDP, iphlen, len))) {
			udpstat.udps_badsum++;
			goto bad;
		}
	} else
#endif /* INET6 */
	if (uh->uh_sum) {
		bzero(((struct ipovly *)ip)->ih_x1,
		    sizeof ((struct ipovly *)ip)->ih_x1);
		((struct ipovly *)ip)->ih_len = uh->uh_ulen;
		if ((uh->uh_sum = in_cksum(m, len + sizeof (struct ip))) != 0) {
			udpstat.udps_badsum++;
			m_freem(m);
			return;
		}
	} else
		udpstat.udps_nosum++;

	switch (srcsa.sa.sa_family) {
	case AF_INET:
		bzero(&srcsa, sizeof(struct sockaddr_in));
		srcsa.sin.sin_len = sizeof(struct sockaddr_in);
		srcsa.sin.sin_family = AF_INET;
		srcsa.sin.sin_port = uh->uh_sport;
		srcsa.sin.sin_addr = ip->ip_src;

		bzero(&dstsa, sizeof(struct sockaddr_in));
		dstsa.sin.sin_len = sizeof(struct sockaddr_in);
		dstsa.sin.sin_family = AF_INET;
		dstsa.sin.sin_port = uh->uh_dport;
		dstsa.sin.sin_addr = ip->ip_dst;
		break;
#ifdef INET6
	case AF_INET6:
		bzero(&srcsa, sizeof(struct sockaddr_in6));
		srcsa.sin6.sin6_len = sizeof(struct sockaddr_in6);
		srcsa.sin6.sin6_family = AF_INET6;
		srcsa.sin6.sin6_port = uh->uh_sport;
		srcsa.sin6.sin6_flowinfo = htonl(0x0fffffff) & ipv6->ip6_flow;
		srcsa.sin6.sin6_addr = ipv6->ip6_src;
		if (IN6_IS_SCOPE_LINKLOCAL(&srcsa.sin6.sin6_addr))
			srcsa.sin6.sin6_addr.s6_addr16[1] = 0;
		if (m->m_pkthdr.rcvif) {
			if (IN6_IS_SCOPE_LINKLOCAL(&srcsa.sin6.sin6_addr)) {
				srcsa.sin6.sin6_scope_id =
					m->m_pkthdr.rcvif->if_index;
			} else
				srcsa.sin6.sin6_scope_id = 0;
		} else
			srcsa.sin6.sin6_scope_id = 0;

		bzero(&dstsa, sizeof(struct sockaddr_in6));
		dstsa.sin6.sin6_len = sizeof(struct sockaddr_in6);
		dstsa.sin6.sin6_family = AF_INET6;
		dstsa.sin6.sin6_port = uh->uh_dport;
		dstsa.sin6.sin6_addr = ipv6->ip6_dst;
		break;
#endif /* INET6 */
	}

#ifdef INET6
	if ((ipv6 && IN6_IS_ADDR_MULTICAST(&ipv6->ip6_dst)) ||
	    (ip && IN_MULTICAST(ip->ip_dst.s_addr)) ||
	    (ip && in_broadcast(ip->ip_dst, m->m_pkthdr.rcvif)))
#else /* INET6 */
	if (IN_MULTICAST(ip->ip_dst.s_addr) ||
	    in_broadcast(ip->ip_dst, m->m_pkthdr.rcvif))
#endif /* INET6 */
	{
		struct socket *last;
		/*
		 * Deliver a multicast or broadcast datagram to *all* sockets
		 * for which the local and remote addresses and ports match
		 * those of the incoming datagram.  This allows more than
		 * one process to receive multi/broadcasts on the same port.
		 * (This really ought to be done for unicast datagrams as
		 * well, but that would cause problems with existing
		 * applications that open both address-specific sockets and
		 * a wildcard socket listening to the same port -- they would
		 * end up receiving duplicates of every unicast datagram.
		 * Those applications open the multiple sockets to overcome an
		 * inadequacy of the UDP socket interface, but for backwards
		 * compatibility we avoid the problem here rather than
		 * fixing the interface.  Maybe 4.5BSD will remedy this?)
		 */

		iphlen += sizeof(struct udphdr);

		/*
		 * Locate pcb(s) for datagram.
		 * (Algorithm copied from raw_intr().)
		 */
		last = NULL;
		for (inp = udbtable.inpt_queue.cqh_first;
		    inp != (struct inpcb *)&udbtable.inpt_queue;
		    inp = inp->inp_queue.cqe_next) {
#ifdef INET6
			/* don't accept it if AF does not match */
			if (ipv6 && !(inp->inp_flags & INP_IPV6))
				continue;
			if (!ipv6 && (inp->inp_flags & INP_IPV6))
				continue;
#endif
			if (inp->inp_lport != uh->uh_dport)
				continue;
#ifdef INET6
			if (ipv6) {
				if (!IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6))
					if (!IN6_ARE_ADDR_EQUAL(&inp->inp_laddr6,
					    &ipv6->ip6_dst))
						continue;
			} else
#endif /* INET6 */
			if (inp->inp_laddr.s_addr != INADDR_ANY) {
				if (inp->inp_laddr.s_addr !=
				    ip->ip_dst.s_addr)
					continue;
			}
#ifdef INET6
			if (ipv6) {
				if (!IN6_IS_ADDR_UNSPECIFIED(&inp->inp_faddr6))
					if (!IN6_ARE_ADDR_EQUAL(&inp->inp_faddr6,
					    &ipv6->ip6_src) ||
					    inp->inp_fport != uh->uh_sport)
			        continue;
			} else
#endif /* INET6 */
			if (inp->inp_faddr.s_addr != INADDR_ANY) {
				if (inp->inp_faddr.s_addr !=
				    ip->ip_src.s_addr ||
				    inp->inp_fport != uh->uh_sport)
					continue;
			}

			if (last != NULL) {
				struct mbuf *n;

				if ((n = m_copy(m, 0, M_COPYALL)) != NULL) {
					opts = NULL;
#ifdef INET6
					if (ipv6 && (inp->inp_flags & IN6P_CONTROLOPTS))
						ip6_savecontrol(inp, &opts, ipv6, n);
#endif /* INET6 */
					m_adj(n, iphlen);
					if (sbappendaddr(&last->so_rcv,
					    &srcsa.sa, n, opts) == 0) {
						m_freem(n);
						if (opts)
							m_freem(opts);
						udpstat.udps_fullsock++;
					} else
						sorwakeup(last);
					opts = NULL;
				}
			}
			last = inp->inp_socket;
			/*
			 * Don't look for additional matches if this one does
			 * not have either the SO_REUSEPORT or SO_REUSEADDR
			 * socket options set.  This heuristic avoids searching
			 * through all pcbs in the common case of a non-shared
			 * port.  It * assumes that an application will never
			 * clear these options after setting them.
			 */
			if ((last->so_options&(SO_REUSEPORT|SO_REUSEADDR)) == 0)
				break;
		}

		if (last == NULL) {
			/*
			 * No matching pcb found; discard datagram.
			 * (No need to send an ICMP Port Unreachable
			 * for a broadcast or multicast datgram.)
			 */
			udpstat.udps_noportbcast++;
			goto bad;
		}

		opts = NULL;
#ifdef INET6
		if (ipv6 && (inp->inp_flags & IN6P_CONTROLOPTS))
			ip6_savecontrol(inp, &opts, ipv6, m);
#endif /* INET6 */
		m_adj(m, iphlen);
		if (sbappendaddr(&last->so_rcv, 
		    &srcsa.sa, m, opts) == 0) {
			udpstat.udps_fullsock++;
			goto bad;
		}
		sorwakeup(last);
		return;
	}
	/*
	 * Locate pcb for datagram.
	 */
#ifdef INET6
	if (ipv6)
		inp = in6_pcbhashlookup(&udbtable, &ipv6->ip6_src, uh->uh_sport,
		    &ipv6->ip6_dst, uh->uh_dport);
	else
#endif /* INET6 */
	inp = in_pcbhashlookup(&udbtable, ip->ip_src, uh->uh_sport,
	    ip->ip_dst, uh->uh_dport);
	if (inp == 0) {
		++udpstat.udps_pcbhashmiss;
#ifdef INET6
		if (ipv6) {
			inp = in_pcblookup(&udbtable,
			    (struct in_addr *)&(ipv6->ip6_src),
			    uh->uh_sport, (struct in_addr *)&(ipv6->ip6_dst),
			    uh->uh_dport, INPLOOKUP_WILDCARD | INPLOOKUP_IPV6);
		} else
#endif /* INET6 */
		inp = in_pcblookup(&udbtable, &ip->ip_src, uh->uh_sport,
		    &ip->ip_dst, uh->uh_dport, INPLOOKUP_WILDCARD);
		if (inp == 0) {
			udpstat.udps_noport++;
			if (m->m_flags & (M_BCAST | M_MCAST)) {
				udpstat.udps_noportbcast++;
				goto bad;
			}
#ifdef INET6
			if (ipv6) {
				icmp6_error(m, ICMP6_DST_UNREACH,
				    ICMP6_DST_UNREACH_NOPORT,0);
			} else
#endif /* INET6 */
			{
				*ip = save_ip;
				HTONS(ip->ip_len);
				HTONS(ip->ip_id);
				HTONS(ip->ip_off);
				uh->uh_sum = savesum;
				icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_PORT,
					0, 0);
			}
			return;
		}
	}

#ifdef IPSEC
	/* Check if this socket requires security for incoming packets */
	if ((inp->inp_seclevel[SL_AUTH] >= IPSEC_LEVEL_REQUIRE &&
	     !(m->m_flags & M_AUTH)) ||
	    (inp->inp_seclevel[SL_ESP_TRANS] >= IPSEC_LEVEL_REQUIRE &&
	     !(m->m_flags & M_CONF))) {
#ifdef notyet
#ifdef INET6
		if (ipv6)
			ipv6_icmp_error(m, ICMPV6_BLAH, ICMPV6_BLAH, 0);
		else
#endif /* INET6 */
		icmp_error(m, ICMP_BLAH, ICMP_BLAH, 0, 0);
		m = NULL;
#endif /* notyet */
		udpstat.udps_nosec++;
		goto bad;
	}
	/* Use tdb_bind_out for this inp's outbound communication */
	if (tdb)
		tdb_add_inp(tdb, inp);
#endif /*IPSEC */

	opts = NULL;
#ifdef INET6
	if (ipv6 && (inp->inp_flags & IN6P_CONTROLOPTS))
		ip6_savecontrol(inp, &opts, ipv6, m);
#endif /* INET6 */
	if (ip && (inp->inp_flags & INP_CONTROLOPTS)) {
		struct mbuf **mp = &opts;

		if (inp->inp_flags & INP_RECVDSTADDR) {
			*mp = udp_saveopt((caddr_t) &ip->ip_dst,
			    sizeof(struct in_addr), IP_RECVDSTADDR);
			if (*mp)
				mp = &(*mp)->m_next;
		}
#ifdef notyet
		/* options were tossed above */
		if (inp->inp_flags & INP_RECVOPTS) {
			*mp = udp_saveopt((caddr_t) opts_deleted_above,
			    sizeof(struct in_addr), IP_RECVOPTS);
			if (*mp)
				mp = &(*mp)->m_next;
		}
		/* ip_srcroute doesn't do what we want here, need to fix */
		if (inp->inp_flags & INP_RECVRETOPTS) {
			*mp = udp_saveopt((caddr_t) ip_srcroute(),
			    sizeof(struct in_addr), IP_RECVRETOPTS);
			if (*mp)
				mp = &(*mp)->m_next;
		}
#endif
	}
	iphlen += sizeof(struct udphdr);
	m_adj(m, iphlen);
	if (sbappendaddr(&inp->inp_socket->so_rcv,
		&srcsa.sa, m, opts) == 0) {
		udpstat.udps_fullsock++;
		goto bad;
	}
	sorwakeup(inp->inp_socket);
	return;
bad:
	m_freem(m);
	if (opts)
		m_freem(opts);
}

/*
 * Create a "control" mbuf containing the specified data
 * with the specified type for presentation with a datagram.
 */
struct mbuf *
udp_saveopt(p, size, type)
	caddr_t p;
	register int size;
	int type;
{
	register struct cmsghdr *cp;
	struct mbuf *m;

	if ((m = m_get(M_DONTWAIT, MT_CONTROL)) == NULL)
		return ((struct mbuf *) NULL);
	cp = (struct cmsghdr *) mtod(m, struct cmsghdr *);
	bcopy(p, CMSG_DATA(cp), size);
	size += sizeof(*cp);
	m->m_len = size;
	cp->cmsg_len = size;
	cp->cmsg_level = IPPROTO_IP;
	cp->cmsg_type = type;
	return (m);
}

/*
 * Notify a udp user of an asynchronous error;
 * just wake up so that he can collect error status.
 */
static void
udp_notify(inp, errno)
	register struct inpcb *inp;
	int errno;
{
	inp->inp_socket->so_error = errno;
	sorwakeup(inp->inp_socket);
	sowwakeup(inp->inp_socket);
}

#if defined(INET6) && !defined(TCP6)
void
udp6_ctlinput(cmd, sa, d)
	int cmd;
	struct sockaddr *sa;
	void *d;
{
	struct sockaddr_in6 sa6;
	struct ip6_hdr *ip6;
	struct mbuf *m;
	int off;

	if (sa == NULL)
		return;
	if (sa->sa_family != AF_INET6)
		return;

	/* decode parameter from icmp6. */
	if (d != NULL) {
		struct ip6ctlparam *ip6cp = (struct ip6ctlparam *)d;
		ip6 = ip6cp->ip6c_ip6;
		m = ip6cp->ip6c_m;
		off = ip6cp->ip6c_off;
	} else
		return;

	/* translate addresses into internal form */
	sa6 = *(struct sockaddr_in6 *)sa;
	if (IN6_IS_ADDR_LINKLOCAL(&sa6.sin6_addr) && m && m->m_pkthdr.rcvif)
		sa6.sin6_addr.s6_addr16[1] = htons(m->m_pkthdr.rcvif->if_index);
	sa = (struct sockaddr *)&sa6;

	(void)udp_ctlinput(cmd, sa, (void *)ip6);
}
#endif

void *
udp_ctlinput(cmd, sa, v)
	int cmd;
	struct sockaddr *sa;
	void *v;
{
	register struct ip *ip = v;
	register struct udphdr *uh;
	extern int inetctlerrmap[];
	void (*notify) __P((struct inpcb *, int)) = udp_notify;
	int errno;

	if ((unsigned)cmd >= PRC_NCMDS)
		return NULL;
	errno = inetctlerrmap[cmd];
	if (PRC_IS_REDIRECT(cmd))
		notify = in_rtchange, ip = 0;
	else if (cmd == PRC_HOSTDEAD)
		ip = 0;
	else if (errno == 0)
		return NULL;
	if (sa == NULL)
		return NULL;
#ifdef INET6
	if (sa->sa_family == AF_INET6) {
		if (ip) {
			struct ip6_hdr *ipv6 = (struct ip6_hdr *)ip;
		
			uh = (struct udphdr *)((caddr_t)ipv6 + sizeof(struct ip6_hdr));
#if 0 /*XXX*/
			in6_pcbnotify(&udbtable, sa, uh->uh_dport,
			    &(ipv6->ip6_src), uh->uh_sport, cmd, udp_notify);
#endif
		} else {
#if 0 /*XXX*/
			in6_pcbnotify(&udbtable, sa, 0,
			    (struct in6_addr *)&in6addr_any, 0, cmd, udp_notify);
#endif
		}
	} else
#endif /* INET6 */
	if (ip) {
		uh = (struct udphdr *)((caddr_t)ip + (ip->ip_hl << 2));
		in_pcbnotify(&udbtable, sa, uh->uh_dport, ip->ip_src,
		    uh->uh_sport, errno, notify);
	} else
		in_pcbnotifyall(&udbtable, sa, errno, notify);
	return NULL;
}

int
#if __STDC__
udp_output(struct mbuf *m, ...)
#else
udp_output(m, va_alist)
	struct mbuf *m;
	va_dcl
#endif
{
	register struct inpcb *inp;
	struct mbuf *addr, *control;
	register struct udpiphdr *ui;
	register int len = m->m_pkthdr.len;
	struct in_addr laddr;
	int s = 0, error = 0;
	va_list ap;
#ifdef INET6
	register struct in6_addr laddr6;
	int v6packet = 0;
	struct sockaddr_in6 *sin6 = NULL;
	struct ip6_pktopts opt, *stickyopt = NULL;
#endif /* INET6 */
	int pcbflags = 0;

	va_start(ap, m);
	inp = va_arg(ap, struct inpcb *);
	addr = va_arg(ap, struct mbuf *);
	control = va_arg(ap, struct mbuf *);
	va_end(ap);

#ifdef INET6
	v6packet = ((inp->inp_flags & INP_IPV6) &&
		    !(inp->inp_flags & INP_IPV6_MAPPED));
#endif

#ifdef INET6
	stickyopt = inp->inp_outputopts6;
	if (control && v6packet) {
		error = ip6_setpktoptions(control, &opt,
		    ((inp->inp_socket->so_state & SS_PRIV) != 0));
		if (error != 0)
			goto release;
		inp->inp_outputopts6 = &opt;
	}
#endif

	if (addr) {
#ifdef INET6
		sin6 = mtod(addr, struct sockaddr_in6 *);
#endif

	        /*
		 * Save current PCB flags because they may change during
		 * temporary connection, particularly the INP_IPV6_UNDEC
		 * flag.
		 */
                pcbflags = inp->inp_flags;

#ifdef INET6
	        if (inp->inp_flags & INP_IPV6)
			laddr6 = inp->inp_laddr6;
		else
#endif /* INET6 */
			laddr = inp->inp_laddr;
#ifdef INET6
		if (((inp->inp_flags & INP_IPV6) &&
		    !IN6_IS_ADDR_UNSPECIFIED(&inp->inp_faddr6)) ||
		    (inp->inp_faddr.s_addr != INADDR_ANY))
#else /* INET6 */
		if (inp->inp_faddr.s_addr != INADDR_ANY)
#endif /* INET6 */
		{
			error = EISCONN;
			goto release;
		}
		/*
		 * Must block input while temporarily connected.
		 */
		s = splsoftnet();
		error = in_pcbconnect(inp, addr);
		if (error) {
			splx(s);
			goto release;
		}
	} else {
#ifdef INET6
	        if (((inp->inp_flags & INP_IPV6) && 
		    IN6_IS_ADDR_UNSPECIFIED(&inp->inp_faddr6)) ||
		    (inp->inp_faddr.s_addr == INADDR_ANY))
#else /* INET6 */
		if (inp->inp_faddr.s_addr == INADDR_ANY)
#endif /* INET6 */
		{
			error = ENOTCONN;
			goto release;
		}
	}
	/*
	 * Calculate data length and get a mbuf
	 * for UDP and IP headers.
	 */
#ifdef INET6
	/*
	 * Handles IPv4-mapped IPv6 address because temporary connect sets
	 * the right flag.
	 */
	M_PREPEND(m, v6packet ? (sizeof(struct udphdr) +
	    sizeof(struct ip6_hdr)) : sizeof(struct udpiphdr), M_DONTWAIT);
#else /* INET6 */
	M_PREPEND(m, sizeof(struct udpiphdr), M_DONTWAIT);
#endif /* INET6 */
	if (m == 0) {
		error = ENOBUFS;
		goto bail;
	}

	/*
	 * Compute the packet length of the IP header, and
	 * punt if the length looks bogus.
	 */
	if ((len + sizeof(struct udpiphdr)) > IP_MAXPACKET) {
		error = EMSGSIZE;
		goto release;
	}

	/*
	 * Fill in mbuf with extended UDP header
	 * and addresses and length put into network format.
	 */
#ifdef INET6
	if (v6packet) {
		struct ip6_hdr *ipv6 = mtod(m, struct ip6_hdr *);
		struct udphdr *uh = (struct udphdr *)(mtod(m, caddr_t) +
		    sizeof(struct ip6_hdr));
		int payload = sizeof(struct ip6_hdr);
		struct in6_addr *faddr;
		struct in6_addr *laddr;
		struct ifnet *oifp = NULL;

		ipv6->ip6_flow = htonl(0x60000000) |
		    (inp->inp_ipv6.ip6_flow & htonl(0x0fffffff)); 
	  
		ipv6->ip6_nxt = IPPROTO_UDP;
		ipv6->ip6_dst = inp->inp_faddr6;
		/*
		 * If the scope of the destination is link-local,
		 * embed the interface
		 * index in the address.
		 *
		 * XXX advanced-api value overrides sin6_scope_id 
		 */
		faddr = &ipv6->ip6_dst;
		if (IN6_IS_ADDR_LINKLOCAL(faddr) ||
		    IN6_IS_ADDR_MC_LINKLOCAL(faddr)) {
			struct ip6_pktopts *optp = inp->inp_outputopts6;
			struct in6_pktinfo *pi = NULL;
			struct ip6_moptions *mopt = NULL;

			/*
			 * XXX Boundary check is assumed to be already done in
			 * ip6_setpktoptions().
			 */
			if (optp && (pi = optp->ip6po_pktinfo) &&
			    pi->ipi6_ifindex) {
				faddr->s6_addr16[1] = htons(pi->ipi6_ifindex);
				oifp = ifindex2ifnet[pi->ipi6_ifindex];
			}
			else if (IN6_IS_ADDR_MULTICAST(faddr) &&
				 (mopt = inp->inp_moptions6) &&
				 mopt->im6o_multicast_ifp) {
				oifp = mopt->im6o_multicast_ifp;
				faddr->s6_addr16[1] = oifp->if_index;
			} else if (sin6 && sin6->sin6_scope_id) {
				/* boundary check */
				if (sin6->sin6_scope_id < 0 
				    || if_index < sin6->sin6_scope_id) {
					error = ENXIO;  /* XXX EINVAL? */
					goto release;
				}
				/* XXX */
				faddr->s6_addr16[1] =
					htons(sin6->sin6_scope_id & 0xffff);
			}
		}
		ipv6->ip6_hlim = in6_selecthlim(inp, oifp);
		if (sin6) {	/*XXX*/
			laddr = in6_selectsrc(sin6, inp->inp_outputopts6,
					      inp->inp_moptions6,
					      &inp->inp_route6,
					      &inp->inp_laddr6, &error);
			if (laddr == NULL) {
				if (error == 0)
					error = EADDRNOTAVAIL;
				goto release;
			}
		} else
			laddr = &inp->inp_laddr6;

		ipv6->ip6_src = *laddr;

		ipv6->ip6_plen = (u_short)len + sizeof(struct udphdr);

		uh->uh_sport = inp->inp_lport;
		uh->uh_dport = inp->inp_fport;
		uh->uh_ulen = htons(ipv6->ip6_plen);
		uh->uh_sum = 0;

		/* 
		 * Always calculate udp checksum for IPv6 datagrams
		 */
		if (!(uh->uh_sum = in6_cksum(m, IPPROTO_UDP,
		    payload, len + sizeof(struct udphdr))))
			uh->uh_sum = 0xffff;

		error = ip6_output(m, inp->inp_outputopts6, &inp->inp_route6, 
		    inp->inp_socket->so_options & SO_DONTROUTE,
		    (inp->inp_flags & INP_IPV6_MCAST)?inp->inp_moptions6:NULL,
		    NULL);
	} else
#endif /* INET6 */
	{
		ui = mtod(m, struct udpiphdr *);
		bzero(ui->ui_x1, sizeof ui->ui_x1);
		ui->ui_pr = IPPROTO_UDP;
		ui->ui_len = htons((u_int16_t)len + sizeof (struct udphdr));
		ui->ui_src = inp->inp_laddr;
		ui->ui_dst = inp->inp_faddr;
		ui->ui_sport = inp->inp_lport;
		ui->ui_dport = inp->inp_fport;
		ui->ui_ulen = ui->ui_len;

		/*
		 * Stuff checksum and output datagram.
		 */

		ui->ui_sum = 0;
		if (udpcksum) {
			if ((ui->ui_sum = in_cksum(m, sizeof (struct udpiphdr) +
			    len)) == 0)
				ui->ui_sum = 0xffff;
		}
		((struct ip *)ui)->ip_len = sizeof (struct udpiphdr) + len;
#ifdef INET6
		/*
		 *  For now, we use the default values for ttl and tos for 
		 *  v4 packets sent using a v6 pcb.  We probably want to
		 *  later allow v4 setsockopt operations on a v6 socket to 
		 *  modify the ttl and tos for v4 packets sent using
		 *  the mapped address format.  We really ought to
		 *  save the v4 ttl and v6 hoplimit in separate places 
		 *  instead of craming both in the inp_hu union.
		 */
		if (inp->inp_flags & INP_IPV6) {
			((struct ip *)ui)->ip_ttl = ip_defttl;
			((struct ip *)ui)->ip_tos = 0;	  
		} else
#endif /* INET6 */
		{
			((struct ip *)ui)->ip_ttl = inp->inp_ip.ip_ttl;	
			((struct ip *)ui)->ip_tos = inp->inp_ip.ip_tos;
		}

		udpstat.udps_opackets++;
#ifdef INET6
		if (inp->inp_flags & INP_IPV6_MCAST) {
			error = ip_output(m, inp->inp_options, &inp->inp_route,
				inp->inp_socket->so_options &
				(SO_DONTROUTE | SO_BROADCAST),
				NULL, NULL, inp->inp_socket);
		} else
#endif /* INET6 */
		{
			error = ip_output(m, inp->inp_options, &inp->inp_route,
				inp->inp_socket->so_options &
				(SO_DONTROUTE | SO_BROADCAST),
		    		inp->inp_moptions, inp, NULL);
		}
	}

bail:
	if (addr) {
		in_pcbdisconnect(inp);
                inp->inp_flags = pcbflags;
#ifdef INET6
		if (inp->inp_flags & INP_IPV6)
			inp->inp_laddr6 = laddr6;
	        else
#endif
			inp->inp_laddr = laddr;
		splx(s);
	}
	if (control) {
#ifdef INET6
		if (v6packet)
			inp->inp_outputopts6 = stickyopt;
#endif
		m_freem(control);
	}
	return (error);

release:
	m_freem(m);
	if (control) {
#ifdef INET6
		if (v6packet)
			inp->inp_outputopts6 = stickyopt;
#endif
		m_freem(control);
	}
	return (error);
}

u_int	udp_sendspace = 9216;		/* really max datagram size */
u_int	udp_recvspace = 40 * (1024 + sizeof(struct sockaddr_in));
					/* 40 1K datagrams */

#if defined(INET6) && !defined(TCP6)
/*ARGSUSED*/
int
udp6_usrreq(so, req, m, addr, control, p)
	struct socket *so;
	int req;
	struct mbuf *m, *addr, *control;
	struct proc *p;
{
	return udp_usrreq(so, req, m, addr, control);
}
#endif

/*ARGSUSED*/
int
udp_usrreq(so, req, m, addr, control)
	struct socket *so;
	int req;
	struct mbuf *m, *addr, *control;
{
	struct inpcb *inp = sotoinpcb(so);
	int error = 0;
	int s;

	if (req == PRU_CONTROL) {
#ifdef INET6
		if (inp->inp_flags & INP_IPV6)
			return (in6_control(so, (u_long)m, (caddr_t)addr,
			    (struct ifnet *)control, 0));
		else
#endif /* INET6 */
			return (in_control(so, (u_long)m, (caddr_t)addr,
			    (struct ifnet *)control));
	}
	if (inp == NULL && req != PRU_ATTACH) {
		error = EINVAL;
		goto release;
	}
	/*
	 * Note: need to block udp_input while changing
	 * the udp pcb queue and/or pcb addresses.
	 */
	switch (req) {

	case PRU_ATTACH:
		if (inp != NULL) {
			error = EINVAL;
			break;
		}
		s = splsoftnet();
		error = in_pcballoc(so, &udbtable);
		splx(s);
		if (error)
			break;
		error = soreserve(so, udp_sendspace, udp_recvspace);
		if (error)
			break;
#ifdef INET6
		if (((struct inpcb *)so->so_pcb)->inp_flags & INP_IPV6)
			((struct inpcb *) so->so_pcb)->inp_ipv6.ip6_hlim =
			    ip6_defhlim;
		else
#endif /* INET6 */
			((struct inpcb *) so->so_pcb)->inp_ip.ip_ttl = ip_defttl;
		break;

	case PRU_DETACH:
		udp_detach(inp);
		break;

	case PRU_BIND:
		s = splsoftnet();
#ifdef INET6
		if (inp->inp_flags & INP_IPV6)
			error = in6_pcbbind(inp, addr);
		else
#endif
			error = in_pcbbind(inp, addr);
		splx(s);
		break;

	case PRU_LISTEN:
		error = EOPNOTSUPP;
		break;

	case PRU_CONNECT:
#ifdef INET6 
		if (inp->inp_flags & INP_IPV6) {
			if (!IN6_IS_ADDR_UNSPECIFIED(&inp->inp_faddr6)) {
				error = EISCONN;
				break;
			}
			s = splsoftnet();
			error = in6_pcbconnect(inp, addr);
			splx(s);
		} else
#endif /* INET6 */
		{
			if (inp->inp_faddr.s_addr != INADDR_ANY) {
				error = EISCONN;
				break;
			}
			s = splsoftnet();
			error = in_pcbconnect(inp, addr);
			splx(s);
		}

		if (error == 0)
			soisconnected(so);
		break;

	case PRU_CONNECT2:
		error = EOPNOTSUPP;
		break;

	case PRU_ACCEPT:
		error = EOPNOTSUPP;
		break;

	case PRU_DISCONNECT:
#ifdef INET6 
		if (inp->inp_flags & INP_IPV6) {
			if (IN6_IS_ADDR_UNSPECIFIED(&inp->inp_faddr6)) {
				error = ENOTCONN;
				break;
			}
		} else
#endif /* INET6 */
			if (inp->inp_faddr.s_addr == INADDR_ANY) {
				error = ENOTCONN;
				break;
			}

		s = splsoftnet();
		in_pcbdisconnect(inp);
#ifdef INET6
		if (inp->inp_flags & INP_IPV6)
			inp->inp_laddr6 = in6addr_any;
		else
#endif /* INET6 */
			inp->inp_laddr.s_addr = INADDR_ANY;

		splx(s);
		so->so_state &= ~SS_ISCONNECTED;		/* XXX */
		break;

	case PRU_SHUTDOWN:
		socantsendmore(so);
		break;

	case PRU_SEND:
#ifdef IPSEC
		error = check_ipsec_policy(inp,0);
		if (error)
			return (error);
#endif
		return (udp_output(m, inp, addr, control));

	case PRU_ABORT:
		soisdisconnected(so);
		udp_detach(inp);
		break;

	case PRU_SOCKADDR:
#ifdef INET6
		if (inp->inp_flags & INP_IPV6)
			in6_setsockaddr(inp, addr);
		else
#endif /* INET6 */
			in_setsockaddr(inp, addr);
		break;

	case PRU_PEERADDR:
#ifdef INET6
		if (inp->inp_flags & INP_IPV6)
			in6_setpeeraddr(inp, addr);
		else
#endif /* INET6 */
			in_setpeeraddr(inp, addr);
		break;

	case PRU_SENSE:
		/*
		 * stat: don't bother with a blocksize.
		 */
		/*
		 * Perhaps Path MTU might be returned for a connected
		 * UDP socket in this case.
		 */
		return (0);

	case PRU_SENDOOB:
	case PRU_FASTTIMO:
	case PRU_SLOWTIMO:
	case PRU_PROTORCV:
	case PRU_PROTOSEND:
		error =  EOPNOTSUPP;
		break;

	case PRU_RCVD:
	case PRU_RCVOOB:
		return (EOPNOTSUPP);	/* do not free mbuf's */

	default:
		panic("udp_usrreq");
	}

release:
	if (control) {
		printf("udp control data unexpectedly retained\n");
		m_freem(control);
	}
	if (m)
		m_freem(m);
	return (error);
}

static void
udp_detach(inp)
	struct inpcb *inp;
{
	int s = splsoftnet();

	in_pcbdetach(inp);
	splx(s);
}

/*
 * Sysctl for udp variables.
 */
int
udp_sysctl(name, namelen, oldp, oldlenp, newp, newlen)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case UDPCTL_CHECKSUM:
		return (sysctl_int(oldp, oldlenp, newp, newlen, &udpcksum));
	case UDPCTL_BADDYNAMIC:
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    baddynamicports.udp, sizeof(baddynamicports.udp)));
	case UDPCTL_RECVSPACE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,&udp_recvspace));
	case UDPCTL_SENDSPACE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,&udp_sendspace));
	default:
		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}
