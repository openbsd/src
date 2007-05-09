/*	$OpenBSD: ip_icmp.c,v 1.74 2007/05/09 14:35:25 deraadt Exp $	*/
/*	$NetBSD: ip_icmp.c,v 1.19 1996/02/13 23:42:22 christos Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1988, 1993
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

#include "carp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netinet/icmp_var.h>

#if NCARP > 0
#include <net/if_types.h>
#include <netinet/ip_carp.h>
#endif

/*
 * ICMP routines: error generation, receive packet processing, and
 * routines to turnaround packets back to the originator, and
 * host table maintenance routines.
 */

int	icmpmaskrepl = 0;
int	icmpbmcastecho = 0;
int	icmptstamprepl = 1;
#ifdef ICMPPRINTFS
int	icmpprintfs = 0;
#endif
int	icmperrppslim = 100;
int	icmperrpps_count = 0;
struct timeval icmperrppslim_last;
int	icmp_rediraccept = 1;
int	icmp_redirtimeout = 10 * 60;
static struct rttimer_queue *icmp_redirect_timeout_q = NULL;
struct	icmpstat icmpstat;

int *icmpctl_vars[ICMPCTL_MAXID] = ICMPCTL_VARS;

void icmp_mtudisc_timeout(struct rtentry *, struct rttimer *);
int icmp_ratelimit(const struct in_addr *, const int, const int);
static void icmp_redirect_timeout(struct rtentry *, struct rttimer *);

extern	struct protosw inetsw[];

void
icmp_init(void)
{
	/*
	 * This is only useful if the user initializes redirtimeout to
	 * something other than zero.
	 */
	if (icmp_redirtimeout != 0) {
		icmp_redirect_timeout_q =
		    rt_timer_queue_create(icmp_redirtimeout);
	}
}

struct mbuf *
icmp_do_error(struct mbuf *n, int type, int code, n_long dest, int destmtu)
{
	struct ip *oip = mtod(n, struct ip *), *nip;
	unsigned oiplen = oip->ip_hl << 2;
	struct icmp *icp;
	struct mbuf *m;
	unsigned icmplen, mblen;
#if NPF > 0
	struct pf_mtag	*mtag;
#endif

#ifdef ICMPPRINTFS
	if (icmpprintfs)
		printf("icmp_error(%x, %d, %d)\n", oip, type, code);
#endif
	if (type != ICMP_REDIRECT)
		icmpstat.icps_error++;
	/*
	 * Don't send error if not the first fragment of message.
	 * Don't error if the old packet protocol was ICMP
	 * error message, only known informational types.
	 */
	if (oip->ip_off & htons(IP_OFFMASK))
		goto freeit;
	if (oip->ip_p == IPPROTO_ICMP && type != ICMP_REDIRECT &&
	    n->m_len >= oiplen + ICMP_MINLEN &&
	    !ICMP_INFOTYPE(((struct icmp *)
	    ((caddr_t)oip + oiplen))->icmp_type)) {
		icmpstat.icps_oldicmp++;
		goto freeit;
	}
	/* Don't send error in response to a multicast or broadcast packet */
	if (n->m_flags & (M_BCAST|M_MCAST))
		goto freeit;

	/*
	 * First, do a rate limitation check.
	 */
	if (icmp_ratelimit(&oip->ip_src, type, code))
		goto freeit;	/* XXX stat */

	/*
	 * Now, formulate icmp message
	 */
	icmplen = oiplen + min(8, ntohs(oip->ip_len));
	/*
	 * Defend against mbuf chains shorter than oip->ip_len:
	 */
	mblen = 0;
	for (m = n; m && (mblen < icmplen); m = m->m_next)
		mblen += m->m_len;
	icmplen = min(mblen, icmplen);

	/*
	 * As we are not required to return everything we have,
	 * we return whatever we can return at ease.
	 *
	 * Note that ICMP datagrams longer than 576 octets are out of spec
	 * according to RFC1812;
	 */

	KASSERT(ICMP_MINLEN <= MCLBYTES);

	if (icmplen + ICMP_MINLEN > MCLBYTES)
		icmplen = MCLBYTES - ICMP_MINLEN - sizeof (struct ip);

	m = m_gethdr(M_DONTWAIT, MT_HEADER);
	if (m && (sizeof (struct ip) + icmplen + ICMP_MINLEN > MHLEN)) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			m = NULL;
		}
	}
	if (m == NULL)
		goto freeit;
	m->m_len = icmplen + ICMP_MINLEN;
	if ((m->m_flags & M_EXT) == 0)
		MH_ALIGN(m, m->m_len);
	icp = mtod(m, struct icmp *);
	if ((u_int)type > ICMP_MAXTYPE)
		panic("icmp_error");
	icmpstat.icps_outhist[type]++;
	icp->icmp_type = type;
	if (type == ICMP_REDIRECT)
		icp->icmp_gwaddr.s_addr = dest;
	else {
		icp->icmp_void = 0;
		/*
		 * The following assignments assume an overlay with the
		 * zeroed icmp_void field.
		 */
		if (type == ICMP_PARAMPROB) {
			icp->icmp_pptr = code;
			code = 0;
		} else if (type == ICMP_UNREACH &&
		    code == ICMP_UNREACH_NEEDFRAG && destmtu)
			icp->icmp_nextmtu = htons(destmtu);
	}

	icp->icmp_code = code;
	m_copydata(n, 0, icmplen, (caddr_t)&icp->icmp_ip);

	/*
	 * Now, copy old ip header (without options)
	 * in front of icmp message.
	 */
	if ((m->m_flags & M_EXT) == 0 &&
	    m->m_data - sizeof(struct ip) < m->m_pktdat)
		panic("icmp len");
	m->m_data -= sizeof(struct ip);
	m->m_len += sizeof(struct ip);
	m->m_pkthdr.len = m->m_len;
	m->m_pkthdr.rcvif = n->m_pkthdr.rcvif;
	nip = mtod(m, struct ip *);
	/* ip_v set in ip_output */
	nip->ip_hl = sizeof(struct ip) >> 2;
	nip->ip_tos = 0;
	nip->ip_len = htons(m->m_len);
	/* ip_id set in ip_output */
	nip->ip_off = 0;
	/* ip_ttl set in icmp_reflect */
	nip->ip_p = IPPROTO_ICMP;
	nip->ip_src = oip->ip_src;
	nip->ip_dst = oip->ip_dst;
#if NPF > 0
	/* move PF_GENERATED to new packet, if existant XXX preserve more? */
	if ((mtag = pf_find_mtag(n)) != NULL &&
	    mtag->flags & PF_TAG_GENERATED) {
		mtag = pf_get_tag(m);
		mtag->flags |= PF_TAG_GENERATED;
	}
#endif
	m_freem(n);
	return (m);

freeit:
	m_freem(n);
	return (NULL);
}

/*
 * Generate an error packet of type error
 * in response to bad packet ip.
 *
 * The ip packet inside has ip_off and ip_len in host byte order.
 */
void
icmp_error(struct mbuf *n, int type, int code, n_long dest, int destmtu)
{
	struct mbuf *m;

	m = icmp_do_error(n, type, code, dest, destmtu);
	if (m != NULL)
		icmp_reflect(m);
}

struct sockaddr_in icmpsrc = { sizeof (struct sockaddr_in), AF_INET };
static struct sockaddr_in icmpdst = { sizeof (struct sockaddr_in), AF_INET };
static struct sockaddr_in icmpgw = { sizeof (struct sockaddr_in), AF_INET };

/*
 * Process a received ICMP message.
 */
void
icmp_input(struct mbuf *m, ...)
{
	struct icmp *icp;
	struct ip *ip = mtod(m, struct ip *);
	int icmplen;
	int i;
	struct in_ifaddr *ia;
	void *(*ctlfunc)(int, struct sockaddr *, void *);
	int code;
	extern u_char ip_protox[];
	int hlen;
	va_list ap;
	struct rtentry *rt;

	va_start(ap, m);
	hlen = va_arg(ap, int);
	va_end(ap);

	/*
	 * Locate icmp structure in mbuf, and check
	 * that not corrupted and of at least minimum length.
	 */
	icmplen = ntohs(ip->ip_len) - hlen;
#ifdef ICMPPRINTFS
	if (icmpprintfs) {
		char buf[4 * sizeof("123")];

		strlcpy(buf, inet_ntoa(ip->ip_dst), sizeof buf);
		printf("icmp_input from %s to %s, len %d\n",
		    inet_ntoa(ip->ip_src), buf, icmplen);
	}
#endif
	if (icmplen < ICMP_MINLEN) {
		icmpstat.icps_tooshort++;
		goto freeit;
	}
	i = hlen + min(icmplen, ICMP_ADVLENMIN);
	if (m->m_len < i && (m = m_pullup(m, i)) == NULL) {
		icmpstat.icps_tooshort++;
		return;
	}
	ip = mtod(m, struct ip *);
	m->m_len -= hlen;
	m->m_data += hlen;
	icp = mtod(m, struct icmp *);
	if (in_cksum(m, icmplen)) {
		icmpstat.icps_checksum++;
		goto freeit;
	}
	m->m_len += hlen;
	m->m_data -= hlen;

#ifdef ICMPPRINTFS
	/*
	 * Message type specific processing.
	 */
	if (icmpprintfs)
		printf("icmp_input, type %d code %d\n", icp->icmp_type,
		    icp->icmp_code);
#endif
	if (icp->icmp_type > ICMP_MAXTYPE)
		goto raw;
	icmpstat.icps_inhist[icp->icmp_type]++;
	code = icp->icmp_code;
	switch (icp->icmp_type) {

	case ICMP_UNREACH:
		switch (code) {
		case ICMP_UNREACH_NET:
		case ICMP_UNREACH_HOST:
		case ICMP_UNREACH_PROTOCOL:
		case ICMP_UNREACH_PORT:
		case ICMP_UNREACH_SRCFAIL:
			code += PRC_UNREACH_NET;
			break;

		case ICMP_UNREACH_NEEDFRAG:
			code = PRC_MSGSIZE;
			break;

		case ICMP_UNREACH_NET_UNKNOWN:
		case ICMP_UNREACH_NET_PROHIB:
		case ICMP_UNREACH_TOSNET:
			code = PRC_UNREACH_NET;
			break;

		case ICMP_UNREACH_HOST_UNKNOWN:
		case ICMP_UNREACH_ISOLATED:
		case ICMP_UNREACH_HOST_PROHIB:
		case ICMP_UNREACH_TOSHOST:
		case ICMP_UNREACH_FILTER_PROHIB:
		case ICMP_UNREACH_HOST_PRECEDENCE:
		case ICMP_UNREACH_PRECEDENCE_CUTOFF:
			code = PRC_UNREACH_HOST;
			break;

		default:
			goto badcode;
		}
		goto deliver;

	case ICMP_TIMXCEED:
		if (code > 1)
			goto badcode;
		code += PRC_TIMXCEED_INTRANS;
		goto deliver;

	case ICMP_PARAMPROB:
		if (code > 1)
			goto badcode;
		code = PRC_PARAMPROB;
		goto deliver;

	case ICMP_SOURCEQUENCH:
		if (code)
			goto badcode;
		code = PRC_QUENCH;
	deliver:
		/* Free packet atttributes */
		if (m->m_flags & M_PKTHDR)
			m_tag_delete_chain(m);

		/*
		 * Problem with datagram; advise higher level routines.
		 */
		if (icmplen < ICMP_ADVLENMIN || icmplen < ICMP_ADVLEN(icp) ||
		    icp->icmp_ip.ip_hl < (sizeof(struct ip) >> 2)) {
			icmpstat.icps_badlen++;
			goto freeit;
		}
		if (IN_MULTICAST(icp->icmp_ip.ip_dst.s_addr))
			goto badcode;
#ifdef INET6
		/* Get more contiguous data for a v6 in v4 ICMP message. */
		if (icp->icmp_ip.ip_p == IPPROTO_IPV6) {
			if (icmplen < ICMP_V6ADVLENMIN ||
			    icmplen < ICMP_V6ADVLEN(icp)) {
				icmpstat.icps_badlen++;
				goto freeit;
			} else {
				if ((m = m_pullup(m, (ip->ip_hl << 2) +
				    ICMP_V6ADVLEN(icp))) == NULL) {
					icmpstat.icps_tooshort++;
					return;
				}
				ip = mtod(m, struct ip *);
				icp = (struct icmp *)
				    (m->m_data + (ip->ip_hl << 2));
			}
		}
#endif /* INET6 */
#ifdef ICMPPRINTFS
		if (icmpprintfs)
			printf("deliver to protocol %d\n", icp->icmp_ip.ip_p);
#endif
		icmpsrc.sin_addr = icp->icmp_ip.ip_dst;
#if NCARP > 0
		if (m->m_pkthdr.rcvif->if_type == IFT_CARP &&
		    m->m_pkthdr.rcvif->if_flags & IFF_LINK0 &&
		    carp_lsdrop(m, AF_INET, &icmpsrc.sin_addr.s_addr,
		    &ip->ip_dst.s_addr))
			goto freeit;
#endif
		/*
		 * XXX if the packet contains [IPv4 AH TCP], we can't make a
		 * notification to TCP layer.
		 */
		ctlfunc = inetsw[ip_protox[icp->icmp_ip.ip_p]].pr_ctlinput;
		if (ctlfunc)
			(*ctlfunc)(code, sintosa(&icmpsrc), &icp->icmp_ip);
		break;

	badcode:
		icmpstat.icps_badcode++;
		break;

	case ICMP_ECHO:
		if (!icmpbmcastecho &&
		    (m->m_flags & (M_MCAST | M_BCAST)) != 0) {
			icmpstat.icps_bmcastecho++;
			break;
		}
		icp->icmp_type = ICMP_ECHOREPLY;
		goto reflect;

	case ICMP_TSTAMP:
		if (icmptstamprepl == 0)
			break;

		if (!icmpbmcastecho &&
		    (m->m_flags & (M_MCAST | M_BCAST)) != 0) {
			icmpstat.icps_bmcastecho++;
			break;
		}
		if (icmplen < ICMP_TSLEN) {
			icmpstat.icps_badlen++;
			break;
		}
		icp->icmp_type = ICMP_TSTAMPREPLY;
		icp->icmp_rtime = iptime();
		icp->icmp_ttime = icp->icmp_rtime;	/* bogus, do later! */
		goto reflect;

	case ICMP_MASKREQ:
		if (icmpmaskrepl == 0)
			break;
		/*
		 * We are not able to respond with all ones broadcast
		 * unless we receive it over a point-to-point interface.
		 */
		if (icmplen < ICMP_MASKLEN) {
			icmpstat.icps_badlen++;
			break;
		}
		if (ip->ip_dst.s_addr == INADDR_BROADCAST ||
		    ip->ip_dst.s_addr == INADDR_ANY)
			icmpdst.sin_addr = ip->ip_src;
		else
			icmpdst.sin_addr = ip->ip_dst;
		if (m->m_pkthdr.rcvif == NULL)
			break;
		ia = ifatoia(ifaof_ifpforaddr(sintosa(&icmpdst),
		    m->m_pkthdr.rcvif));
		if (ia == 0)
			break;
		icp->icmp_type = ICMP_MASKREPLY;
		icp->icmp_mask = ia->ia_sockmask.sin_addr.s_addr;
		if (ip->ip_src.s_addr == 0) {
			if (ia->ia_ifp->if_flags & IFF_BROADCAST)
				ip->ip_src = ia->ia_broadaddr.sin_addr;
			else if (ia->ia_ifp->if_flags & IFF_POINTOPOINT)
				ip->ip_src = ia->ia_dstaddr.sin_addr;
		}
reflect:
#if NCARP > 0
		if (m->m_pkthdr.rcvif->if_type == IFT_CARP &&
		    m->m_pkthdr.rcvif->if_flags & IFF_LINK0 &&
		    carp_lsdrop(m, AF_INET, &ip->ip_src.s_addr,
		    &ip->ip_dst.s_addr))
			goto freeit;
#endif
		/* Free packet atttributes */
		if (m->m_flags & M_PKTHDR)
			m_tag_delete_chain(m);

		icmpstat.icps_reflect++;
		icmpstat.icps_outhist[icp->icmp_type]++;
		icmp_reflect(m);
		return;

	case ICMP_REDIRECT:
		/* Free packet atttributes */
		if (m->m_flags & M_PKTHDR)
			m_tag_delete_chain(m);
		if (icmp_rediraccept == 0)
			goto freeit;
		if (code > 3)
			goto badcode;
		if (icmplen < ICMP_ADVLENMIN || icmplen < ICMP_ADVLEN(icp) ||
		    icp->icmp_ip.ip_hl < (sizeof(struct ip) >> 2)) {
			icmpstat.icps_badlen++;
			break;
		}
		/*
		 * Short circuit routing redirects to force
		 * immediate change in the kernel's routing
		 * tables.  The message is also handed to anyone
		 * listening on a raw socket (e.g. the routing
		 * daemon for use in updating its tables).
		 */
		icmpgw.sin_addr = ip->ip_src;
		icmpdst.sin_addr = icp->icmp_gwaddr;
#ifdef	ICMPPRINTFS
		if (icmpprintfs) {
			char buf[4 * sizeof("123")];
			strlcpy(buf, inet_ntoa(icp->icmp_ip.ip_dst),
			    sizeof buf);

			printf("redirect dst %s to %s\n",
			    buf, inet_ntoa(icp->icmp_gwaddr));
		}
#endif
		icmpsrc.sin_addr = icp->icmp_ip.ip_dst;
#if NCARP > 0
		if (m->m_pkthdr.rcvif->if_type == IFT_CARP &&
		    m->m_pkthdr.rcvif->if_flags & IFF_LINK0 &&
		    carp_lsdrop(m, AF_INET, &icmpsrc.sin_addr.s_addr,
		    &ip->ip_dst.s_addr))
			goto freeit;
#endif
		rt = NULL;
		rtredirect(sintosa(&icmpsrc), sintosa(&icmpdst),
		    (struct sockaddr *)0, RTF_GATEWAY | RTF_HOST,
		    sintosa(&icmpgw), (struct rtentry **)&rt);
		if (rt != NULL && icmp_redirtimeout != 0) {
			(void)rt_timer_add(rt, icmp_redirect_timeout,
			    icmp_redirect_timeout_q);
		}
		if (rt != NULL)
			rtfree(rt);
		pfctlinput(PRC_REDIRECT_HOST, sintosa(&icmpsrc));
		break;

	/*
	 * No kernel processing for the following;
	 * just fall through to send to raw listener.
	 */
	case ICMP_ECHOREPLY:
	case ICMP_ROUTERADVERT:
	case ICMP_ROUTERSOLICIT:
	case ICMP_TSTAMPREPLY:
	case ICMP_IREQREPLY:
	case ICMP_MASKREPLY:
	case ICMP_TRACEROUTE:
	case ICMP_DATACONVERR:
	case ICMP_MOBILE_REDIRECT:
	case ICMP_IPV6_WHEREAREYOU:
	case ICMP_IPV6_IAMHERE:
	case ICMP_MOBILE_REGREQUEST:
	case ICMP_MOBILE_REGREPLY:
	case ICMP_PHOTURIS:
	default:
		break;
	}

raw:
	rip_input(m);
	return;

freeit:
	m_freem(m);
}

/*
 * Reflect the ip packet back to the source
 */
void
icmp_reflect(struct mbuf *m)
{
	struct ip *ip = mtod(m, struct ip *);
	struct in_ifaddr *ia;
	struct in_addr t;
	struct mbuf *opts = 0;
	int optlen = (ip->ip_hl << 2) - sizeof(struct ip);

	if (!in_canforward(ip->ip_src) &&
	    ((ip->ip_src.s_addr & IN_CLASSA_NET) !=
	    htonl(IN_LOOPBACKNET << IN_CLASSA_NSHIFT))) {
		m_freem(m);	/* Bad return address */
		goto done;	/* ip_output() will check for broadcast */
	}
	t = ip->ip_dst;
	ip->ip_dst = ip->ip_src;
	/*
	 * If the incoming packet was addressed directly to us,
	 * use dst as the src for the reply.  For broadcast, use
	 * the address which corresponds to the incoming interface.
	 */
	TAILQ_FOREACH(ia, &in_ifaddr, ia_list) {
		if (t.s_addr == ia->ia_addr.sin_addr.s_addr)
			break;
		if ((ia->ia_ifp->if_flags & IFF_BROADCAST) &&
		    t.s_addr == ia->ia_broadaddr.sin_addr.s_addr)
			break;
	}
	/*
	 * The following happens if the packet was not addressed to us.
	 * Use the new source address and do a route lookup. If it fails
	 * drop the packet as there is no path to the host.
	 */
	if (ia == (struct in_ifaddr *)0) {
		struct sockaddr_in *dst;
		struct route ro;

		bzero((caddr_t) &ro, sizeof(ro));
		dst = satosin(&ro.ro_dst);
		dst->sin_family = AF_INET;
		dst->sin_len = sizeof(*dst);
		dst->sin_addr = ip->ip_src;

		rtalloc(&ro);
		if (ro.ro_rt == 0) {
			ipstat.ips_noroute++;
			m_freem(m);
			goto done;
		}

		ia = ifatoia(ro.ro_rt->rt_ifa);
		ro.ro_rt->rt_use++;
		RTFREE(ro.ro_rt);
	}

	t = ia->ia_addr.sin_addr;
	ip->ip_src = t;
	ip->ip_ttl = MAXTTL;

	if (optlen > 0) {
		u_char *cp;
		int opt, cnt;
		u_int len;

		/*
		 * Retrieve any source routing from the incoming packet;
		 * add on any record-route or timestamp options.
		 */
		cp = (u_char *) (ip + 1);
		if ((opts = ip_srcroute()) == 0 &&
		    (opts = m_gethdr(M_DONTWAIT, MT_HEADER))) {
			opts->m_len = sizeof(struct in_addr);
			mtod(opts, struct in_addr *)->s_addr = 0;
		}
		if (opts) {
#ifdef ICMPPRINTFS
			if (icmpprintfs)
				printf("icmp_reflect optlen %d rt %d => ",
				    optlen, opts->m_len);
#endif
			for (cnt = optlen; cnt > 0; cnt -= len, cp += len) {
				opt = cp[IPOPT_OPTVAL];
				if (opt == IPOPT_EOL)
					break;
				if (opt == IPOPT_NOP)
					len = 1;
				else {
					if (cnt < IPOPT_OLEN + sizeof(*cp))
						break;
					len = cp[IPOPT_OLEN];
					if (len < IPOPT_OLEN + sizeof(*cp) ||
					    len > cnt)
						break;
				}
				/*
				 * Should check for overflow, but it
				 * "can't happen"
				 */
				if (opt == IPOPT_RR || opt == IPOPT_TS ||
				    opt == IPOPT_SECURITY) {
					bcopy((caddr_t)cp,
					    mtod(opts, caddr_t) + opts->m_len,
					    len);
					opts->m_len += len;
				}
			}
			/* Terminate & pad, if necessary */
			if ((cnt = opts->m_len % 4) != 0)
				for (; cnt < 4; cnt++) {
					*(mtod(opts, caddr_t) + opts->m_len) =
					    IPOPT_EOL;
					opts->m_len++;
				}
#ifdef ICMPPRINTFS
			if (icmpprintfs)
				printf("%d\n", opts->m_len);
#endif
		}
		/*
		 * Now strip out original options by copying rest of first
		 * mbuf's data back, and adjust the IP length.
		 */
		ip->ip_len = htons(ntohs(ip->ip_len) - optlen);
		ip->ip_hl = sizeof(struct ip) >> 2;
		m->m_len -= optlen;
		if (m->m_flags & M_PKTHDR)
			m->m_pkthdr.len -= optlen;
		optlen += sizeof(struct ip);
		bcopy((caddr_t)ip + optlen, (caddr_t)(ip + 1),
		    (unsigned)(m->m_len - sizeof(struct ip)));
	}
	m->m_flags &= ~(M_BCAST|M_MCAST);
	icmp_send(m, opts);
done:
	if (opts)
		(void)m_free(opts);
}

/*
 * Send an icmp packet back to the ip level,
 * after supplying a checksum.
 */
void
icmp_send(struct mbuf *m, struct mbuf *opts)
{
	struct ip *ip = mtod(m, struct ip *);
	int hlen;
	struct icmp *icp;

	hlen = ip->ip_hl << 2;
	m->m_data += hlen;
	m->m_len -= hlen;
	icp = mtod(m, struct icmp *);
	icp->icmp_cksum = 0;
	icp->icmp_cksum = in_cksum(m, ntohs(ip->ip_len) - hlen);
	m->m_data -= hlen;
	m->m_len += hlen;
#ifdef ICMPPRINTFS
	if (icmpprintfs) {
		char buf[4 * sizeof("123")];

		strlcpy(buf, inet_ntoa(ip->ip_dst), sizeof buf);
		printf("icmp_send dst %s src %s\n",
		    buf, inet_ntoa(ip->ip_src));
	}
#endif
	(void)ip_output(m, opts, (void *)NULL, 0, (void *)NULL, (void *)NULL);
}

n_time
iptime(void)
{
	struct timeval atv;
	u_long t;

	microtime(&atv);
	t = (atv.tv_sec % (24*60*60)) * 1000 + atv.tv_usec / 1000;
	return (htonl(t));
}

int
icmp_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{

	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case ICMPCTL_REDIRTIMEOUT: {
		int error;

		error = sysctl_int(oldp, oldlenp, newp, newlen,
		    &icmp_redirtimeout);
		if (icmp_redirect_timeout_q != NULL) {
			if (icmp_redirtimeout == 0) {
				rt_timer_queue_destroy(icmp_redirect_timeout_q,
				    TRUE);
				icmp_redirect_timeout_q = NULL;
			} else
				rt_timer_queue_change(icmp_redirect_timeout_q,
				    icmp_redirtimeout);
		} else if (icmp_redirtimeout > 0) {
			icmp_redirect_timeout_q =
			    rt_timer_queue_create(icmp_redirtimeout);
		}
		return (error);

		break;
	}
	default:
		if (name[0] < ICMPCTL_MAXID)
			return (sysctl_int_arr(icmpctl_vars, name, namelen,
			    oldp, oldlenp, newp, newlen));
		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}


/* XXX only handles table 0 right now */
struct rtentry *
icmp_mtudisc_clone(struct sockaddr *dst)
{
	struct rtentry *rt;
	int error;

	rt = rtalloc1(dst, 1, 0);
	if (rt == 0)
		return (NULL);

	/* If we didn't get a host route, allocate one */

	if ((rt->rt_flags & RTF_HOST) == 0) {
		struct rtentry *nrt;

		error = rtrequest((int) RTM_ADD, dst,
		    (struct sockaddr *) rt->rt_gateway,
		    (struct sockaddr *) 0,
		    RTF_GATEWAY | RTF_HOST | RTF_DYNAMIC, &nrt, 0);
		if (error) {
			rtfree(rt);
			return (NULL);
		}
		nrt->rt_rmx = rt->rt_rmx;
		rtfree(rt);
		rt = nrt;
	}
	error = rt_timer_add(rt, icmp_mtudisc_timeout, ip_mtudisc_timeout_q);
	if (error) {
		rtfree(rt);
		return (NULL);
	}

	return (rt);
}

void
icmp_mtudisc(struct icmp *icp)
{
	struct rtentry *rt;
	struct sockaddr *dst = sintosa(&icmpsrc);
	u_long mtu = ntohs(icp->icmp_nextmtu);  /* Why a long?  IPv6 */

	/* Table of common MTUs: */

	static u_short mtu_table[] = {
		65535, 65280, 32000, 17914, 9180, 8166,
		4352, 2002, 1492, 1006, 508, 296, 68, 0
	};

	rt = icmp_mtudisc_clone(dst);
	if (rt == 0)
		return;

	if (mtu == 0) {
		int i = 0;

		mtu = ntohs(icp->icmp_ip.ip_len);
		/* Some 4.2BSD-based routers incorrectly adjust the ip_len */
		if (mtu > rt->rt_rmx.rmx_mtu && rt->rt_rmx.rmx_mtu != 0)
			mtu -= (icp->icmp_ip.ip_hl << 2);

		/* If we still can't guess a value, try the route */

		if (mtu == 0) {
			mtu = rt->rt_rmx.rmx_mtu;

			/* If no route mtu, default to the interface mtu */

			if (mtu == 0)
				mtu = rt->rt_ifp->if_mtu;
		}

		for (i = 0; i < sizeof(mtu_table) / sizeof(mtu_table[0]); i++)
			if (mtu > mtu_table[i]) {
				mtu = mtu_table[i];
				break;
			}
	}

	/*
	 * XXX:   RTV_MTU is overloaded, since the admin can set it
	 *	  to turn off PMTU for a route, and the kernel can
	 *	  set it to indicate a serious problem with PMTU
	 *	  on a route.  We should be using a separate flag
	 *	  for the kernel to indicate this.
	 */

	if ((rt->rt_rmx.rmx_locks & RTV_MTU) == 0) {
		if (mtu < 296 || mtu > rt->rt_ifp->if_mtu)
			rt->rt_rmx.rmx_locks |= RTV_MTU;
		else if (rt->rt_rmx.rmx_mtu > mtu ||
		    rt->rt_rmx.rmx_mtu == 0)
			rt->rt_rmx.rmx_mtu = mtu;
	}

	rtfree(rt);
}

/* XXX only handles table 0 right now */
void
icmp_mtudisc_timeout(struct rtentry *rt, struct rttimer *r)
{
	if (rt == NULL)
		panic("icmp_mtudisc_timeout:  bad route to timeout");
	if ((rt->rt_flags & (RTF_DYNAMIC | RTF_HOST)) ==
	    (RTF_DYNAMIC | RTF_HOST)) {
		void *(*ctlfunc)(int, struct sockaddr *, void *);
		extern u_char ip_protox[];
		struct sockaddr_in sa;

		sa = *(struct sockaddr_in *)rt_key(rt);
		rtrequest((int) RTM_DELETE, (struct sockaddr *)rt_key(rt),
		    rt->rt_gateway, rt_mask(rt), rt->rt_flags, 0, 0);

		/* Notify TCP layer of increased Path MTU estimate */
		ctlfunc = inetsw[ip_protox[IPPROTO_TCP]].pr_ctlinput;
		if (ctlfunc)
			(*ctlfunc)(PRC_MTUINC,(struct sockaddr *)&sa, NULL);
	} else
		if ((rt->rt_rmx.rmx_locks & RTV_MTU) == 0)
			rt->rt_rmx.rmx_mtu = 0;
}

/*
 * Perform rate limit check.
 * Returns 0 if it is okay to send the icmp packet.
 * Returns 1 if the router SHOULD NOT send this icmp packet due to rate
 * limitation.
 *
 * XXX per-destination/type check necessary?
 */
int
icmp_ratelimit(const struct in_addr *dst, const int type, const int code)
{

	/* PPS limit */
	if (!ppsratecheck(&icmperrppslim_last, &icmperrpps_count,
	    icmperrppslim))
		return 1;

	/*okay to send*/
	return 0;
}

/* XXX only handles table 0 right now */
static void
icmp_redirect_timeout(struct rtentry *rt, struct rttimer *r)
{
	if (rt == NULL)
		panic("icmp_redirect_timeout:  bad route to timeout");
	if ((rt->rt_flags & (RTF_DYNAMIC | RTF_HOST)) ==
	    (RTF_DYNAMIC | RTF_HOST)) {
		rtrequest((int) RTM_DELETE, (struct sockaddr *)rt_key(rt),
		    rt->rt_gateway, rt_mask(rt), rt->rt_flags, 0, 0);
	}
}
