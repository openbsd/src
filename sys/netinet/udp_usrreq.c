/*	$OpenBSD: udp_usrreq.c,v 1.101 2004/06/14 05:24:04 mcbride Exp $	*/
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
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
#include <netinet/ip_esp.h>
#endif

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/ip6protosw.h>

extern int ip6_defhlim;
#endif /* INET6 */

/*
 * UDP protocol implementation.
 * Per RFC 768, August, 1980.
 */
int	udpcksum = 1;

u_int	udp_sendspace = 9216;		/* really max datagram size */
u_int	udp_recvspace = 40 * (1024 + sizeof(struct sockaddr_in));
					/* 40 1K datagrams */

int *udpctl_vars[UDPCTL_MAXID] = UDPCTL_VARS;

struct	inpcbtable udbtable;
struct	udpstat udpstat;

static	void udp_detach(struct inpcb *);
static	void udp_notify(struct inpcb *, int);
static	struct mbuf *udp_saveopt(caddr_t, int, int);

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

#ifdef INET6
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
udp_input(struct mbuf *m, ...)
{
	struct ip *ip;
	struct udphdr *uh;
	struct inpcb *inp;
	struct mbuf *opts = 0;
	struct ip save_ip;
	int iphlen, len;
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
	struct ip6_hdr *ip6;
#endif /* INET6 */
#ifdef IPSEC
	struct m_tag *mtag;
	struct tdb_ident *tdbi;
	struct tdb *tdb;
	int error, s;
#endif /* IPSEC */

	va_start(ap, m);
	iphlen = va_arg(ap, int);
	va_end(ap);

	udpstat.udps_ipackets++;

	switch (mtod(m, struct ip *)->ip_v) {
	case 4:
		ip = mtod(m, struct ip *);
#ifdef INET6
		ip6 = NULL;
#endif /* INET6 */
		srcsa.sa.sa_family = AF_INET;
		break;
#ifdef INET6
	case 6:
		ip = NULL;
		ip6 = mtod(m, struct ip6_hdr *);
		srcsa.sa.sa_family = AF_INET6;
		break;
#endif /* INET6 */
	default:
		goto bad;
	}

	IP6_EXTHDR_GET(uh, struct udphdr *, m, iphlen, sizeof(struct udphdr));
	if (!uh) {
		udpstat.udps_hdrops++;
		return;
	}

	/* Check for illegal destination port 0 */
	if (uh->uh_dport == 0) {
		udpstat.udps_noport++;
		goto bad;
	}

	/*
	 * Make mbuf data length reflect UDP length.
	 * If not enough data to reflect UDP length, drop.
	 */
	len = ntohs((u_int16_t)uh->uh_ulen);
	if (ip) {
		if (m->m_pkthdr.len - iphlen != len) {
			if (len > (m->m_pkthdr.len - iphlen) ||
			    len < sizeof(struct udphdr)) {
				udpstat.udps_badlen++;
				goto bad;
			}
			m_adj(m, len - (m->m_pkthdr.len - iphlen));
		}
	}
#ifdef INET6
	else if (ip6) {
		/* jumbograms */
		if (len == 0 && m->m_pkthdr.len - iphlen > 0xffff)
			len = m->m_pkthdr.len - iphlen;
		if (len != m->m_pkthdr.len - iphlen) {
			udpstat.udps_badlen++;
			goto bad;
		}
	}
#endif
	else /* shouldn't happen */
		goto bad;

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
	if (ip6) {
		/* Be proactive about malicious use of IPv4 mapped address */
		if (IN6_IS_ADDR_V4MAPPED(&ip6->ip6_src) ||
		    IN6_IS_ADDR_V4MAPPED(&ip6->ip6_dst)) {
			/* XXX stat */
			goto bad;
		}

		/*
		 * In IPv6, the UDP checksum is ALWAYS used.
		 */
		if (uh->uh_sum == 0) {
			udpstat.udps_nosum++;
			goto bad;
		}
		if ((uh->uh_sum = in6_cksum(m, IPPROTO_UDP, iphlen, len))) {
			udpstat.udps_badsum++;
			goto bad;
		}
	} else
#endif /* INET6 */
	if (uh->uh_sum) {
		if ((m->m_pkthdr.csum & M_UDP_CSUM_IN_OK) == 0) {
			if (m->m_pkthdr.csum & M_UDP_CSUM_IN_BAD) {
				udpstat.udps_badsum++;
				udpstat.udps_inhwcsum++;
				m_freem(m);
				return;
			}

			if ((uh->uh_sum = in4_cksum(m, IPPROTO_UDP,
			    iphlen, len))) {
				udpstat.udps_badsum++;
				m_freem(m);
				return;
			}
		} else {
			m->m_pkthdr.csum &= ~M_UDP_CSUM_IN_OK;
			udpstat.udps_inhwcsum++;
		}
	} else
		udpstat.udps_nosum++;

#ifdef IPSEC
	if (udpencap_enable && udpencap_port &&
	    uh->uh_dport == htons(udpencap_port)) {
		u_int32_t spi;
		int skip = iphlen + sizeof(struct udphdr);

		if (m->m_pkthdr.len - skip < sizeof(u_int32_t)) {
			/* packet too short */
			m_freem(m);
			return;
		}
		m_copydata(m, skip, sizeof(u_int32_t), (caddr_t) &spi);
		/*
		 * decapsulate if the SPI is not zero, otherwise pass
		 * to userland
		 */
		if (spi != 0) {
			if ((m = m_pullup2(m, skip)) == NULL) {
				udpstat.udps_hdrops++;
				return;
			}

			/* remove the UDP header */
			bcopy(mtod(m, u_char *),
			    mtod(m, u_char *) + sizeof(struct udphdr), iphlen);
			m_adj(m, sizeof(struct udphdr));
			skip -= sizeof(struct udphdr);

			espstat.esps_udpencin++;
			ipsec_common_input(m, skip, offsetof(struct ip, ip_p),
			    srcsa.sa.sa_family, IPPROTO_ESP, 1);
			return;
		}
	}
#endif

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
#if 0 /*XXX inbound flowinfo */
		srcsa.sin6.sin6_flowinfo = htonl(0x0fffffff) & ip6->ip6_flow;
#endif
		/* KAME hack: recover scopeid */
		(void)in6_recoverscope(&srcsa.sin6, &ip6->ip6_src,
		    m->m_pkthdr.rcvif);

		bzero(&dstsa, sizeof(struct sockaddr_in6));
		dstsa.sin6.sin6_len = sizeof(struct sockaddr_in6);
		dstsa.sin6.sin6_family = AF_INET6;
		dstsa.sin6.sin6_port = uh->uh_dport;
		/* KAME hack: recover scopeid */
		(void)in6_recoverscope(&dstsa.sin6, &ip6->ip6_dst,
		    m->m_pkthdr.rcvif);
		break;
#endif /* INET6 */
	}

#ifdef INET6
	if ((ip6 && IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst)) ||
	    (ip && IN_MULTICAST(ip->ip_dst.s_addr)) ||
	    (ip && in_broadcast(ip->ip_dst, m->m_pkthdr.rcvif))) {
#else /* INET6 */
	if (IN_MULTICAST(ip->ip_dst.s_addr) ||
	    in_broadcast(ip->ip_dst, m->m_pkthdr.rcvif)) {
#endif /* INET6 */
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
		CIRCLEQ_FOREACH(inp, &udbtable.inpt_queue, inp_queue) {
#ifdef INET6
			/* don't accept it if AF does not match */
			if (ip6 && !(inp->inp_flags & INP_IPV6))
				continue;
			if (!ip6 && (inp->inp_flags & INP_IPV6))
				continue;
#endif
			if (inp->inp_lport != uh->uh_dport)
				continue;
#ifdef INET6
			if (ip6) {
				if (!IN6_IS_ADDR_UNSPECIFIED(&inp->inp_laddr6))
					if (!IN6_ARE_ADDR_EQUAL(&inp->inp_laddr6,
					    &ip6->ip6_dst))
						continue;
			} else
#endif /* INET6 */
			if (inp->inp_laddr.s_addr != INADDR_ANY) {
				if (inp->inp_laddr.s_addr !=
				    ip->ip_dst.s_addr)
					continue;
			}
#ifdef INET6
			if (ip6) {
				if (!IN6_IS_ADDR_UNSPECIFIED(&inp->inp_faddr6))
					if (!IN6_ARE_ADDR_EQUAL(&inp->inp_faddr6,
					    &ip6->ip6_src) ||
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
					if (ip6 && (inp->inp_flags & IN6P_CONTROLOPTS))
						ip6_savecontrol(inp, &opts, ip6, n);
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
		if (ip6 && (inp->inp_flags & IN6P_CONTROLOPTS))
			ip6_savecontrol(inp, &opts, ip6, m);
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
	if (ip6)
		inp = in6_pcbhashlookup(&udbtable, &ip6->ip6_src, uh->uh_sport,
		    &ip6->ip6_dst, uh->uh_dport);
	else
#endif /* INET6 */
	inp = in_pcbhashlookup(&udbtable, ip->ip_src, uh->uh_sport,
	    ip->ip_dst, uh->uh_dport);
	if (inp == 0) {
		++udpstat.udps_pcbhashmiss;
#ifdef INET6
		if (ip6) {
			inp = in6_pcblookup_listen(&udbtable,
			    &ip6->ip6_dst, uh->uh_dport, m_tag_find(m,
			    PACKET_TAG_PF_TRANSLATE_LOCALHOST, NULL) != NULL);
		} else
#endif /* INET6 */
		inp = in_pcblookup_listen(&udbtable,
		    ip->ip_dst, uh->uh_dport, m_tag_find(m,
		    PACKET_TAG_PF_TRANSLATE_LOCALHOST, NULL) != NULL);
		if (inp == 0) {
			udpstat.udps_noport++;
			if (m->m_flags & (M_BCAST | M_MCAST)) {
				udpstat.udps_noportbcast++;
				goto bad;
			}
#ifdef INET6
			if (ip6) {
				uh->uh_sum = savesum;
				icmp6_error(m, ICMP6_DST_UNREACH,
				    ICMP6_DST_UNREACH_NOPORT,0);
			} else
#endif /* INET6 */
			{
				*ip = save_ip;
				uh->uh_sum = savesum;
				icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_PORT,
				    0, 0);
			}
			return;
		}
	}

#ifdef IPSEC
	mtag = m_tag_find(m, PACKET_TAG_IPSEC_IN_DONE, NULL);
	s = splnet();
	if (mtag != NULL) {
		tdbi = (struct tdb_ident *)(mtag + 1);
		tdb = gettdb(tdbi->spi, &tdbi->dst, tdbi->proto);
	} else
		tdb = NULL;
	ipsp_spd_lookup(m, srcsa.sa.sa_family, iphlen, &error,
	    IPSP_DIRECTION_IN, tdb, inp);
	if (error) {
		splx(s);
		goto bad;
	}

	/* Latch SA only if the socket is connected */
	if (inp->inp_tdb_in != tdb &&
	    (inp->inp_socket->so_state & SS_ISCONNECTED)) {
		if (tdb) {
			tdb_add_inp(tdb, inp, 1);
			if (inp->inp_ipo == NULL) {
				inp->inp_ipo = ipsec_add_policy(inp,
				    srcsa.sa.sa_family, IPSP_DIRECTION_OUT);
				if (inp->inp_ipo == NULL) {
					splx(s);
					goto bad;
				}
			}
			if (inp->inp_ipo->ipo_dstid == NULL &&
			    tdb->tdb_srcid != NULL) {
				inp->inp_ipo->ipo_dstid = tdb->tdb_srcid;
				tdb->tdb_srcid->ref_count++;
			}
			if (inp->inp_ipsec_remotecred == NULL &&
			    tdb->tdb_remote_cred != NULL) {
				inp->inp_ipsec_remotecred =
				    tdb->tdb_remote_cred;
				tdb->tdb_remote_cred->ref_count++;
			}
			if (inp->inp_ipsec_remoteauth == NULL &&
			    tdb->tdb_remote_auth != NULL) {
				inp->inp_ipsec_remoteauth =
				    tdb->tdb_remote_auth;
				tdb->tdb_remote_auth->ref_count++;
			}
		} else { /* Just reset */
			TAILQ_REMOVE(&inp->inp_tdb_in->tdb_inp_in, inp,
			    inp_tdb_in_next);
			inp->inp_tdb_in = NULL;
		}
	}
	splx(s);
#endif /*IPSEC */

	opts = NULL;
#ifdef INET6
	if (ip6 && (inp->inp_flags & IN6P_CONTROLOPTS))
		ip6_savecontrol(inp, &opts, ip6, m);
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
	if (sbappendaddr(&inp->inp_socket->so_rcv, &srcsa.sa, m, opts) == 0) {
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
	int size;
	int type;
{
	struct cmsghdr *cp;
	struct mbuf *m;

	if ((m = m_get(M_DONTWAIT, MT_CONTROL)) == NULL)
		return ((struct mbuf *) NULL);
	cp = (struct cmsghdr *) mtod(m, struct cmsghdr *);
	bcopy(p, CMSG_DATA(cp), size);
	size = CMSG_LEN(size);
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
	struct inpcb *inp;
	int errno;
{
	inp->inp_socket->so_error = errno;
	sorwakeup(inp->inp_socket);
	sowwakeup(inp->inp_socket);
}

#ifdef INET6
void
udp6_ctlinput(cmd, sa, d)
	int cmd;
	struct sockaddr *sa;
	void *d;
{
	struct udphdr uh;
	struct sockaddr_in6 sa6;
	struct ip6_hdr *ip6;
	struct mbuf *m;
	int off;
	void *cmdarg;
	struct ip6ctlparam *ip6cp = NULL;
	struct udp_portonly {
		u_int16_t uh_sport;
		u_int16_t uh_dport;
	} *uhp;
	void (*notify)(struct inpcb *, int) = udp_notify;

	if (sa == NULL)
		return;
	if (sa->sa_family != AF_INET6 ||
	    sa->sa_len != sizeof(struct sockaddr_in6))
		return;

	if ((unsigned)cmd >= PRC_NCMDS)
		return;
	if (PRC_IS_REDIRECT(cmd))
		notify = in_rtchange, d = NULL;
	else if (cmd == PRC_HOSTDEAD)
		d = NULL;
	else if (cmd == PRC_MSGSIZE)
		; /* special code is present, see below */
	else if (inet6ctlerrmap[cmd] == 0)
		return;

	/* if the parameter is from icmp6, decode it. */
	if (d != NULL) {
		ip6cp = (struct ip6ctlparam *)d;
		m = ip6cp->ip6c_m;
		ip6 = ip6cp->ip6c_ip6;
		off = ip6cp->ip6c_off;
		cmdarg = ip6cp->ip6c_cmdarg;
	} else {
		m = NULL;
		ip6 = NULL;
		cmdarg = NULL;
		/* XXX: translate addresses into internal form */
		sa6 = *(struct sockaddr_in6 *)sa;
#ifndef SCOPEDROUTING
		if (in6_embedscope(&sa6.sin6_addr, &sa6, NULL, NULL)) {
			/* should be impossible */
			return;
		}
#endif
	}

	if (ip6cp && ip6cp->ip6c_finaldst) {
		bzero(&sa6, sizeof(sa6));
		sa6.sin6_family = AF_INET6;
		sa6.sin6_len = sizeof(sa6);
		sa6.sin6_addr = *ip6cp->ip6c_finaldst;
		/* XXX: assuming M is valid in this case */
		sa6.sin6_scope_id = in6_addr2scopeid(m->m_pkthdr.rcvif,
		    ip6cp->ip6c_finaldst);
#ifndef SCOPEDROUTING
		if (in6_embedscope(ip6cp->ip6c_finaldst, &sa6, NULL, NULL)) {
			/* should be impossible */
			return;
		}
#endif
	} else {
		/* XXX: translate addresses into internal form */
		sa6 = *(struct sockaddr_in6 *)sa;
#ifndef SCOPEDROUTING
		if (in6_embedscope(&sa6.sin6_addr, &sa6, NULL, NULL)) {
			/* should be impossible */
			return;
		}
#endif
	}

	if (ip6) {
		/*
		 * XXX: We assume that when IPV6 is non NULL,
		 * M and OFF are valid.
		 */
		struct sockaddr_in6 sa6_src;

		/* check if we can safely examine src and dst ports */
		if (m->m_pkthdr.len < off + sizeof(*uhp))
			return;

		bzero(&uh, sizeof(uh));
		m_copydata(m, off, sizeof(*uhp), (caddr_t)&uh);

		bzero(&sa6_src, sizeof(sa6_src));
		sa6_src.sin6_family = AF_INET6;
		sa6_src.sin6_len = sizeof(sa6_src);
		sa6_src.sin6_addr = ip6->ip6_src;
		sa6_src.sin6_scope_id = in6_addr2scopeid(m->m_pkthdr.rcvif,
		    &ip6->ip6_src);
#ifndef SCOPEDROUTING
		if (in6_embedscope(&sa6_src.sin6_addr, &sa6_src, NULL, NULL)) {
			/* should be impossible */
			return;
		}
#endif

		if (cmd == PRC_MSGSIZE) {
			int valid = 0;

			/*
			 * Check to see if we have a valid UDP socket
			 * corresponding to the address in the ICMPv6 message
			 * payload.
			 */
			if (in6_pcbhashlookup(&udbtable, &sa6.sin6_addr,
			    uh.uh_dport, &sa6_src.sin6_addr, uh.uh_sport))
				valid = 1;
#if 0
			/*
			 * As the use of sendto(2) is fairly popular,
			 * we may want to allow non-connected pcb too.
			 * But it could be too weak against attacks...
			 * We should at least check if the local address (= s)
			 * is really ours.
			 */
			else if (in6_pcblookup_listen(&udbtable,
			    &sa6_src.sin6_addr, uh.uh_sport, 0);
				valid = 1;
#endif

			/*
			 * Depending on the value of "valid" and routing table
			 * size (mtudisc_{hi,lo}wat), we will:
			 * - recalculate the new MTU and create the
			 *   corresponding routing entry, or
			 * - ignore the MTU change notification.
			 */
			icmp6_mtudisc_update((struct ip6ctlparam *)d, valid);

			/*
			 * regardless of if we called icmp6_mtudisc_update(),
			 * we need to call in6_pcbnotify(), to notify path
			 * MTU change to the userland (2292bis-02), because
			 * some unconnected sockets may share the same
			 * destination and want to know the path MTU.
			 */
		}

		(void) in6_pcbnotify(&udbtable, (struct sockaddr *)&sa6,
		    uh.uh_dport, (struct sockaddr *)&sa6_src,
		    uh.uh_sport, cmd, cmdarg, notify);
	} else {
		(void) in6_pcbnotify(&udbtable, (struct sockaddr *)&sa6, 0,
		    (struct sockaddr *)&sa6_any, 0, cmd, cmdarg, notify);
	}
}
#endif

void *
udp_ctlinput(cmd, sa, v)
	int cmd;
	struct sockaddr *sa;
	void *v;
{
	struct ip *ip = v;
	struct udphdr *uhp;
	extern int inetctlerrmap[];
	void (*notify)(struct inpcb *, int) = udp_notify;
	int errno;

	if (sa == NULL)
		return NULL;
	if (sa->sa_family != AF_INET ||
	    sa->sa_len != sizeof(struct sockaddr_in))
		return NULL;

	if ((unsigned)cmd >= PRC_NCMDS)
		return NULL;
	errno = inetctlerrmap[cmd];
	if (PRC_IS_REDIRECT(cmd))
		notify = in_rtchange, ip = 0;
	else if (cmd == PRC_HOSTDEAD)
		ip = 0;
	else if (errno == 0)
		return NULL;
	if (ip) {
		uhp = (struct udphdr *)((caddr_t)ip + (ip->ip_hl << 2));
		(void) in_pcbnotify(&udbtable, sa, uhp->uh_dport, ip->ip_src,
		    uhp->uh_sport, errno, notify);
	} else
		in_pcbnotifyall(&udbtable, sa, errno, notify);
	return NULL;
}

int
udp_output(struct mbuf *m, ...)
{
	struct inpcb *inp;
	struct mbuf *addr, *control;
	struct udpiphdr *ui;
	int len = m->m_pkthdr.len;
	struct in_addr laddr;
	int s = 0, error = 0;
	va_list ap;
	int pcbflags = 0;

	va_start(ap, m);
	inp = va_arg(ap, struct inpcb *);
	addr = va_arg(ap, struct mbuf *);
	control = va_arg(ap, struct mbuf *);
	va_end(ap);

#ifdef DIAGNOSTIC
	if ((inp->inp_flags & INP_IPV6) != 0)
		panic("IPv6 inpcb to udp_output");
#endif

	/*
	 * Compute the packet length of the IP header, and
	 * punt if the length looks bogus.
	 */
	if ((len + sizeof(struct udpiphdr)) > IP_MAXPACKET) {
		error = EMSGSIZE;
		goto release;
	}

	if (addr) {
		/*
		 * Save current PCB flags because they may change during
		 * temporary connection.
		 */
		pcbflags = inp->inp_flags;

		laddr = inp->inp_laddr;
		if (inp->inp_faddr.s_addr != INADDR_ANY) {
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
		if (inp->inp_faddr.s_addr == INADDR_ANY) {
			error = ENOTCONN;
			goto release;
		}
	}
	/*
	 * Calculate data length and get a mbuf
	 * for UDP and IP headers.
	 */
	M_PREPEND(m, sizeof(struct udpiphdr), M_DONTWAIT);
	if (m == 0) {
		error = ENOBUFS;
		goto bail;
	}

	/*
	 * Fill in mbuf with extended UDP header
	 * and addresses and length put into network format.
	 */
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
	 * Compute the pseudo-header checksum; defer further checksumming
	 * until ip_output() or hardware (if it exists).
	 */
	if (udpcksum) {
		m->m_pkthdr.csum |= M_UDPV4_CSUM_OUT;
		ui->ui_sum = in_cksum_phdr(ui->ui_src.s_addr,
		    ui->ui_dst.s_addr, htons((u_int16_t)len +
		    sizeof (struct udphdr) + IPPROTO_UDP));
	} else
		ui->ui_sum = 0;
	((struct ip *)ui)->ip_len = htons(sizeof (struct udpiphdr) + len);
	((struct ip *)ui)->ip_ttl = inp->inp_ip.ip_ttl;
	((struct ip *)ui)->ip_tos = inp->inp_ip.ip_tos;

	udpstat.udps_opackets++;
	error = ip_output(m, inp->inp_options, &inp->inp_route,
	    inp->inp_socket->so_options & (SO_DONTROUTE | SO_BROADCAST),
	    inp->inp_moptions, inp, (void *)NULL);

bail:
	if (addr) {
		in_pcbdisconnect(inp);
		inp->inp_flags = pcbflags;
		inp->inp_laddr = laddr;
		splx(s);
	}
	if (control)
		m_freem(control);
	return (error);

release:
	m_freem(m);
	if (control)
		m_freem(control);
	return (error);
}

#ifdef INET6
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
		{
			if (inp->inp_faddr.s_addr == INADDR_ANY) {
				error = ENOTCONN;
				break;
			}
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
#ifdef INET6
		if (inp->inp_flags & INP_IPV6)
			return (udp6_output(inp, m, addr, control));
		else
			return (udp_output(m, inp, addr, control));
#else
		return (udp_output(m, inp, addr, control));
#endif

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
	case UDPCTL_BADDYNAMIC:
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    baddynamicports.udp, sizeof(baddynamicports.udp)));
	default:
		if (name[0] < UDPCTL_MAXID)
			return (sysctl_int_arr(udpctl_vars, name, namelen,
			    oldp, oldlenp, newp, newlen));
		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}
