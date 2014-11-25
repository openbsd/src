/*	$OpenBSD: udp_usrreq.c,v 1.194 2014/11/25 12:13:59 mpi Exp $	*/
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
#include <net/if_media.h>
#include <net/route.h>

#include <netinet/in.h>
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
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#include <netinet6/ip6protosw.h>
#endif /* INET6 */

#include "pf.h"
#if NPF > 0
#include <net/pfvar.h>
#endif

#ifdef PIPEX 
#include <netinet/if_ether.h>
#include <net/pipex.h>
#endif

#include "vxlan.h"
#if NVXLAN > 0
#include <net/if_vxlan.h>
#endif

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

int	udp_output(struct inpcb *, struct mbuf *, struct mbuf *, struct mbuf *);
void	udp_detach(struct inpcb *);
void	udp_notify(struct inpcb *, int);

#ifndef	UDB_INITIAL_HASH_SIZE
#define	UDB_INITIAL_HASH_SIZE	128
#endif

void
udp_init()
{
	in_pcbinit(&udbtable, UDB_INITIAL_HASH_SIZE);
}

#ifdef INET6
int
udp6_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m = *mp;

	udp_input(m, *offp, proto);
	return IPPROTO_DONE;
}
#endif

void
udp_input(struct mbuf *m, ...)
{
	struct ip *ip;
	struct udphdr *uh;
	struct inpcb *inp = NULL;
	struct mbuf *opts = NULL;
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
	int error;
	u_int32_t ipsecflowinfo = 0;
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

#ifdef INET6
	if (ip6) {
		/* Be proactive about malicious use of IPv4 mapped address */
		if (IN6_IS_ADDR_V4MAPPED(&ip6->ip6_src) ||
		    IN6_IS_ADDR_V4MAPPED(&ip6->ip6_dst)) {
			/* XXX stat */
			goto bad;
		}
	}
#endif /* INET6 */

	/*
	 * Checksum extended UDP header and data.
	 * from W.R.Stevens: check incoming udp cksums even if
	 *	udpcksum is not set.
	 */
	savesum = uh->uh_sum;
	if (uh->uh_sum == 0) {
		udpstat.udps_nosum++;
#ifdef INET6
		/*
		 * In IPv6, the UDP checksum is ALWAYS used.
		 */
		if (ip6)
			goto bad;
#endif /* INET6 */
	} else {
		if ((m->m_pkthdr.csum_flags & M_UDP_CSUM_IN_OK) == 0) {
			if (m->m_pkthdr.csum_flags & M_UDP_CSUM_IN_BAD) {
				udpstat.udps_badsum++;
				goto bad;
			}
			udpstat.udps_inswcsum++;

			if (ip)
				uh->uh_sum = in4_cksum(m, IPPROTO_UDP,
				    iphlen, len);
#ifdef INET6
			else if (ip6)
				uh->uh_sum = in6_cksum(m, IPPROTO_UDP,
				    iphlen, len);
#endif /* INET6 */
			if (uh->uh_sum != 0) {
				udpstat.udps_badsum++;
				goto bad;
			}
		}
	}

#ifdef IPSEC
	if (udpencap_enable && udpencap_port &&
#if NPF > 0
	    !(m->m_pkthdr.pf.flags & PF_TAG_DIVERTED) &&
#endif
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
			if ((m = m_pullup(m, skip)) == NULL) {
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

#if NVXLAN > 0
	if (vxlan_enable > 0 &&
#if NPF > 0
	    !(m->m_pkthdr.pf.flags & PF_TAG_DIVERTED) &&
#endif
	    (error = vxlan_lookup(m, uh, iphlen, &srcsa.sa)) != 0) {
		if (error == -1) {
			udpstat.udps_hdrops++;
			m_freem(m);
		}
		return;
	}
#endif

	if (m->m_flags & (M_BCAST|M_MCAST)) {
		struct inpcb *last;
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
		TAILQ_FOREACH(inp, &udbtable.inpt_queue, inp_queue) {
			if (inp->inp_socket->so_state & SS_CANTRCVMORE)
				continue;
#ifdef INET6
			/* don't accept it if AF does not match */
			if (ip6 && !(inp->inp_flags & INP_IPV6))
				continue;
			if (!ip6 && (inp->inp_flags & INP_IPV6))
				continue;
#endif
			if (rtable_l2(inp->inp_rtableid) !=
			    rtable_l2(m->m_pkthdr.ph_rtableid))
				continue;
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
				if (inp->inp_laddr.s_addr != ip->ip_dst.s_addr)
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
#ifdef INET6
					if (ip6 && (last->inp_flags &
					    IN6P_CONTROLOPTS ||
					    last->inp_socket->so_options &
					    SO_TIMESTAMP))
						ip6_savecontrol(last, n, &opts);
#endif /* INET6 */
					if (ip && (last->inp_flags &
					    INP_CONTROLOPTS ||
					    last->inp_socket->so_options &
					    SO_TIMESTAMP))
						ip_savecontrol(last, &opts,
						    ip, n);

					m_adj(n, iphlen);
					if (sbappendaddr(
					    &last->inp_socket->so_rcv,
					    &srcsa.sa, n, opts) == 0) {
						m_freem(n);
						if (opts)
							m_freem(opts);
						udpstat.udps_fullsock++;
					} else
						sorwakeup(last->inp_socket);
					opts = NULL;
				}
			}
			last = inp;
			/*
			 * Don't look for additional matches if this one does
			 * not have either the SO_REUSEPORT or SO_REUSEADDR
			 * socket options set.  This heuristic avoids searching
			 * through all pcbs in the common case of a non-shared
			 * port.  It assumes that an application will never
			 * clear these options after setting them.
			 */
			if ((last->inp_socket->so_options & (SO_REUSEPORT |
			    SO_REUSEADDR)) == 0)
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

#ifdef INET6
		if (ip6 && (last->inp_flags & IN6P_CONTROLOPTS ||
		    last->inp_socket->so_options & SO_TIMESTAMP))
			ip6_savecontrol(last, m, &opts);
#endif /* INET6 */
		if (ip && (last->inp_flags & INP_CONTROLOPTS ||
		    last->inp_socket->so_options & SO_TIMESTAMP))
			ip_savecontrol(last, &opts, ip, m);

		m_adj(m, iphlen);
		if (sbappendaddr(&last->inp_socket->so_rcv,
		    &srcsa.sa, m, opts) == 0) {
			udpstat.udps_fullsock++;
			goto bad;
		}
		sorwakeup(last->inp_socket);
		return;
	}
	/*
	 * Locate pcb for datagram.
	 */
#if 0
	if (m->m_pkthdr.pf.statekey) {
		inp = m->m_pkthdr.pf.statekey->inp;
		if (inp && inp->inp_pf_sk)
			KASSERT(m->m_pkthdr.pf.statekey == inp->inp_pf_sk);
	}
#endif
	if (inp == NULL) {
#ifdef INET6
		if (ip6)
			inp = in6_pcbhashlookup(&udbtable, &ip6->ip6_src,
			    uh->uh_sport, &ip6->ip6_dst, uh->uh_dport,
			    m->m_pkthdr.ph_rtableid);
		else
#endif /* INET6 */
		inp = in_pcbhashlookup(&udbtable, ip->ip_src, uh->uh_sport,
		    ip->ip_dst, uh->uh_dport, m->m_pkthdr.ph_rtableid);
#if NPF > 0
		if (m->m_pkthdr.pf.statekey && inp) {
			m->m_pkthdr.pf.statekey->inp = inp;
			inp->inp_pf_sk = m->m_pkthdr.pf.statekey;
		}
#endif
	}
	if (inp == 0) {
		int	inpl_reverse = 0;
		if (m->m_pkthdr.pf.flags & PF_TAG_TRANSLATE_LOCALHOST)
			inpl_reverse = 1;
		++udpstat.udps_pcbhashmiss;
#ifdef INET6
		if (ip6) {
			inp = in6_pcblookup_listen(&udbtable,
			    &ip6->ip6_dst, uh->uh_dport, inpl_reverse, m,
			    m->m_pkthdr.ph_rtableid);
		} else
#endif /* INET6 */
		inp = in_pcblookup_listen(&udbtable,
		    ip->ip_dst, uh->uh_dport, inpl_reverse, m,
		    m->m_pkthdr.ph_rtableid);
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
	KASSERT(sotoinpcb(inp->inp_socket) == inp);

#if NPF > 0
	if (m->m_pkthdr.pf.statekey && !m->m_pkthdr.pf.statekey->inp &&
	    !inp->inp_pf_sk && (inp->inp_socket->so_state & SS_ISCONNECTED)) {
		m->m_pkthdr.pf.statekey->inp = inp;
		inp->inp_pf_sk = m->m_pkthdr.pf.statekey;
	}
	/* The statekey has finished finding the inp, it is no longer needed. */
	m->m_pkthdr.pf.statekey = NULL;
#endif

#ifdef IPSEC
	mtag = m_tag_find(m, PACKET_TAG_IPSEC_IN_DONE, NULL);
	if (mtag != NULL) {
		tdbi = (struct tdb_ident *)(mtag + 1);
		tdb = gettdb(tdbi->rdomain, tdbi->spi,
		    &tdbi->dst, tdbi->proto);
	} else
		tdb = NULL;
	ipsp_spd_lookup(m, srcsa.sa.sa_family, iphlen, &error,
	    IPSP_DIRECTION_IN, tdb, inp, 0);
	if (error) {
		udpstat.udps_nosec++;
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
	/* create ipsec options while we know that tdb cannot be modified */
	if (tdb)
		ipsecflowinfo = tdb->tdb_spi;

#endif /*IPSEC */

	opts = NULL;
#ifdef INET6
	if (ip6 && (inp->inp_flags & IN6P_CONTROLOPTS ||
	    inp->inp_socket->so_options & SO_TIMESTAMP))
		ip6_savecontrol(inp, m, &opts);
#endif /* INET6 */
	if (ip && (inp->inp_flags & INP_CONTROLOPTS ||
	    inp->inp_socket->so_options & SO_TIMESTAMP))
		ip_savecontrol(inp, &opts, ip, m);
#ifdef INET6
	if (ip6 && (inp->inp_flags & IN6P_RECVDSTPORT)) {
		struct mbuf **mp = &opts;

		while (*mp)
			mp = &(*mp)->m_next;
		*mp = sbcreatecontrol((caddr_t)&uh->uh_dport, sizeof(u_int16_t),
		    IPV6_RECVDSTPORT, IPPROTO_IPV6);
	}
#endif /* INET6 */
	if (ip && (inp->inp_flags & INP_RECVDSTPORT)) {
		struct mbuf **mp = &opts;

		while (*mp)
			mp = &(*mp)->m_next;
		*mp = sbcreatecontrol((caddr_t)&uh->uh_dport, sizeof(u_int16_t),
		    IP_RECVDSTPORT, IPPROTO_IP);
	}
#ifdef IPSEC
	if (ipsecflowinfo && (inp->inp_flags & INP_IPSECFLOWINFO)) {
		struct mbuf **mp = &opts;

		while (*mp)
			mp = &(*mp)->m_next;
		*mp = sbcreatecontrol((caddr_t)&ipsecflowinfo,
		    sizeof(u_int32_t), IP_IPSECFLOWINFO, IPPROTO_IP);
	}
#endif
#ifdef PIPEX
	if (pipex_enable && inp->inp_pipex) {
		struct pipex_session *session;
		int off = iphlen + sizeof(struct udphdr);
		if ((session = pipex_l2tp_lookup_session(m, off)) != NULL) {
			if ((m = pipex_l2tp_input(m, off, session,
			    ipsecflowinfo)) == NULL) {
				if (opts)
					m_freem(opts);
				return; /* the packet is handled by PIPEX */
			}
		}
	}
#endif

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
 * Notify a udp user of an asynchronous error;
 * just wake up so that he can collect error status.
 */
void
udp_notify(struct inpcb *inp, int errno)
{
	inp->inp_socket->so_error = errno;
	sorwakeup(inp->inp_socket);
	sowwakeup(inp->inp_socket);
}

#ifdef INET6
void
udp6_ctlinput(int cmd, struct sockaddr *sa, u_int rdomain, void *d)
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
		if (in6_embedscope(&sa6.sin6_addr, &sa6, NULL, NULL)) {
			/* should be impossible */
			return;
		}
	}

	if (ip6cp && ip6cp->ip6c_finaldst) {
		bzero(&sa6, sizeof(sa6));
		sa6.sin6_family = AF_INET6;
		sa6.sin6_len = sizeof(sa6);
		sa6.sin6_addr = *ip6cp->ip6c_finaldst;
		/* XXX: assuming M is valid in this case */
		sa6.sin6_scope_id = in6_addr2scopeid(m->m_pkthdr.rcvif,
		    ip6cp->ip6c_finaldst);
		if (in6_embedscope(ip6cp->ip6c_finaldst, &sa6, NULL, NULL)) {
			/* should be impossible */
			return;
		}
	} else {
		/* XXX: translate addresses into internal form */
		sa6 = *(struct sockaddr_in6 *)sa;
		if (in6_embedscope(&sa6.sin6_addr, &sa6, NULL, NULL)) {
			/* should be impossible */
			return;
		}
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
		if (in6_embedscope(&sa6_src.sin6_addr, &sa6_src, NULL, NULL)) {
			/* should be impossible */
			return;
		}

		if (cmd == PRC_MSGSIZE) {
			int valid = 0;

			/*
			 * Check to see if we have a valid UDP socket
			 * corresponding to the address in the ICMPv6 message
			 * payload.
			 */
			if (in6_pcbhashlookup(&udbtable, &sa6.sin6_addr,
			    uh.uh_dport, &sa6_src.sin6_addr, uh.uh_sport,
			    rdomain))
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
			    &sa6_src.sin6_addr, uh.uh_sport, 0,
			    rdomain))
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

		(void) in6_pcbnotify(&udbtable, &sa6, uh.uh_dport,
		    &sa6_src, uh.uh_sport, rdomain, cmd, cmdarg, notify);
	} else {
		(void) in6_pcbnotify(&udbtable, &sa6, 0,
		    &sa6_any, 0, rdomain, cmd, cmdarg, notify);
	}
}
#endif

void *
udp_ctlinput(int cmd, struct sockaddr *sa, u_int rdomain, void *v)
{
	struct ip *ip = v;
	struct udphdr *uhp;
	struct in_addr faddr;
	struct inpcb *inp;
	void (*notify)(struct inpcb *, int) = udp_notify;
	int errno;

	if (sa == NULL)
		return NULL;
	if (sa->sa_family != AF_INET ||
	    sa->sa_len != sizeof(struct sockaddr_in))
		return NULL;
	faddr = satosin(sa)->sin_addr;
	if (faddr.s_addr == INADDR_ANY)
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

#ifdef IPSEC
		/* PMTU discovery for udpencap */
		if (cmd == PRC_MSGSIZE && ip_mtudisc && udpencap_enable &&
		    udpencap_port && uhp->uh_sport == htons(udpencap_port)) {
			udpencap_ctlinput(cmd, sa, rdomain, v);
			return (NULL);
		}
#endif
		inp = in_pcbhashlookup(&udbtable,
		    ip->ip_dst, uhp->uh_dport, ip->ip_src, uhp->uh_sport,
		    rdomain);
		if (inp && inp->inp_socket != NULL)
			notify(inp, errno);
	} else
		in_pcbnotifyall(&udbtable, sa, rdomain, errno, notify);
	return (NULL);
}

int
udp_output(struct inpcb *inp, struct mbuf *m, struct mbuf *addr,
    struct mbuf *control)
{
	struct sockaddr_in *sin = NULL;
	struct udpiphdr *ui;
	u_int32_t ipsecflowinfo = 0;
	int len = m->m_pkthdr.len;
	struct in_addr *laddr;
	int error = 0;

#ifdef DIAGNOSTIC
	if ((inp->inp_flags & INP_IPV6) != 0)
		panic("IPv6 inpcb to %s", __func__);
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
		sin = mtod(addr, struct sockaddr_in *);

		if (addr->m_len != sizeof(*sin)) {
			error = EINVAL;
			goto release;
		}
		if (sin->sin_family != AF_INET) {
			error = EAFNOSUPPORT;
			goto release;
		}
		if (sin->sin_port == 0) {
			error = EADDRNOTAVAIL;
			goto release;
		}

		if (inp->inp_faddr.s_addr != INADDR_ANY) {
			error = EISCONN;
			goto release;
		}

		error = in_selectsrc(&laddr, sin, inp->inp_moptions,
		    &inp->inp_route, &inp->inp_laddr, inp->inp_rtableid);
		if (error)
			goto release;

		if (inp->inp_lport == 0) {
			int s = splsoftnet();
			error = in_pcbbind(inp, NULL, curproc);
			splx(s);
			if (error)
				goto release;
		}
	} else {
		if (inp->inp_faddr.s_addr == INADDR_ANY) {
			error = ENOTCONN;
			goto release;
		}
		laddr = &inp->inp_laddr;
	}

#ifdef IPSEC
	if (control && (inp->inp_flags & INP_IPSECFLOWINFO) != 0) {
		u_int clen;
		struct cmsghdr *cm;
		caddr_t cmsgs;

		/*
		 * XXX: Currently, we assume all the optional information is stored
		 * in a single mbuf.
		 */
		if (control->m_next) {
			error = EINVAL;
			goto release;
		}

		clen = control->m_len;
		cmsgs = mtod(control, caddr_t);
		do {
			if (clen < CMSG_LEN(0)) {
				error = EINVAL;
				goto release;
			}
			cm = (struct cmsghdr *)cmsgs;
			if (cm->cmsg_len < CMSG_LEN(0) ||
			    CMSG_ALIGN(cm->cmsg_len) > clen) {
				error = EINVAL;
				goto release;
			}
			if (cm->cmsg_len == CMSG_LEN(sizeof(ipsecflowinfo)) &&
			    cm->cmsg_level == IPPROTO_IP &&
			    cm->cmsg_type == IP_IPSECFLOWINFO) {
				ipsecflowinfo = *(u_int32_t *)CMSG_DATA(cm);
				break;
			}
			clen -= CMSG_ALIGN(cm->cmsg_len);
			cmsgs += CMSG_ALIGN(cm->cmsg_len);
		} while (clen);
	}
#endif
	/*
	 * Calculate data length and get a mbuf
	 * for UDP and IP headers.
	 */
	M_PREPEND(m, sizeof(struct udpiphdr), M_DONTWAIT);
	if (m == NULL) {
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
	ui->ui_src = *laddr;
	ui->ui_dst = sin ? sin->sin_addr : inp->inp_faddr;
	ui->ui_sport = inp->inp_lport;
	ui->ui_dport = sin ? sin->sin_port : inp->inp_fport;
	ui->ui_ulen = ui->ui_len;
	((struct ip *)ui)->ip_len = htons(sizeof (struct udpiphdr) + len);
	((struct ip *)ui)->ip_ttl = inp->inp_ip.ip_ttl;
	((struct ip *)ui)->ip_tos = inp->inp_ip.ip_tos;
	if (udpcksum)
		m->m_pkthdr.csum_flags |= M_UDP_CSUM_OUT;

	udpstat.udps_opackets++;

	/* force routing table */
	m->m_pkthdr.ph_rtableid = inp->inp_rtableid;

#if NPF > 0
	if (inp->inp_socket->so_state & SS_ISCONNECTED)
		m->m_pkthdr.pf.inp = inp;
#endif

	error = ip_output(m, inp->inp_options, &inp->inp_route,
	    (inp->inp_socket->so_options & SO_BROADCAST), inp->inp_moptions,
	    inp, ipsecflowinfo);
	if (error == EACCES)	/* translate pf(4) error for userland */
		error = EHOSTUNREACH;

bail:
	if (control)
		m_freem(control);
	return (error);

release:
	m_freem(m);
	goto bail;
}

/*ARGSUSED*/
int
udp_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *addr,
    struct mbuf *control, struct proc *p)
{
	struct inpcb *inp = sotoinpcb(so);
	int error = 0;
	int s;

	if (req == PRU_CONTROL) {
#ifdef INET6
		if (inp->inp_flags & INP_IPV6)
			return (in6_control(so, (u_long)m, (caddr_t)addr,
			    (struct ifnet *)control));
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
		if ((error = soreserve(so, udp_sendspace, udp_recvspace)) ||
		    (error = in_pcballoc(so, &udbtable))) {
			splx(s);
			break;
		}
		splx(s);
#ifdef INET6
		if (sotoinpcb(so)->inp_flags & INP_IPV6)
			sotoinpcb(so)->inp_ipv6.ip6_hlim = ip6_defhlim;
		else
#endif /* INET6 */
			sotoinpcb(so)->inp_ip.ip_ttl = ip_defttl;
		break;

	case PRU_DETACH:
		udp_detach(inp);
		break;

	case PRU_BIND:
		s = splsoftnet();
#ifdef INET6
		if (inp->inp_flags & INP_IPV6)
			error = in6_pcbbind(inp, addr, p);
		else
#endif
			error = in_pcbbind(inp, addr, p);
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
#ifdef INET6
		if (inp->inp_flags & INP_IPV6)
			inp->inp_laddr6 = in6addr_any;
		else
#endif /* INET6 */
			inp->inp_laddr.s_addr = INADDR_ANY;
		in_pcbdisconnect(inp);

		splx(s);
		so->so_state &= ~SS_ISCONNECTED;		/* XXX */
		break;

	case PRU_SHUTDOWN:
		socantsendmore(so);
		break;

	case PRU_SEND:
#ifdef PIPEX
		if (inp->inp_pipex) {
			struct pipex_session *session;

			if (addr != NULL) 
				session =
				    pipex_l2tp_userland_lookup_session(m,
					mtod(addr, struct sockaddr *));
			else
#ifdef INET6
			if (inp->inp_flags & INP_IPV6)
				session =
				    pipex_l2tp_userland_lookup_session_ipv6(
					m, inp->inp_faddr6);
			else
#endif
				session =
				    pipex_l2tp_userland_lookup_session_ipv4(
					m, inp->inp_faddr);
			if (session != NULL)
				if ((m = pipex_l2tp_userland_output(
				    m, session)) == NULL) {
					error = ENOMEM;
					goto release;
				}
		}
#endif

#ifdef INET6
		if (inp->inp_flags & INP_IPV6)
			return (udp6_output(inp, m, addr, control));
		else
			return (udp_output(inp, m, addr, control));
#else
		return (udp_output(inp, m, addr, control));
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

void
udp_detach(struct inpcb *inp)
{
	int s = splsoftnet();

	in_pcbdetach(inp);
	splx(s);
}

/*
 * Sysctl for udp variables.
 */
int
udp_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case UDPCTL_BADDYNAMIC:
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    baddynamicports.udp, sizeof(baddynamicports.udp)));

	case UDPCTL_STATS:
		if (newp != NULL)
			return (EPERM);
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    &udpstat, sizeof(udpstat)));

	default:
		if (name[0] < UDPCTL_MAXID)
			return (sysctl_int_arr(udpctl_vars, name, namelen,
			    oldp, oldlenp, newp, newlen));
		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}
