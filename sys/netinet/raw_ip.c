/*	$OpenBSD: raw_ip.c,v 1.77 2014/11/09 22:05:08 bluhm Exp $	*/
/*	$NetBSD: raw_ip.c,v 1.25 1996/02/18 18:58:33 christos Exp $	*/

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
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/socketvar.h>

#include <net/if.h>
#include <net/route.h>
#include <net/pfvar.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_mroute.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_icmp.h>

#include "pf.h"

struct inpcbtable rawcbtable;

/*
 * Nominal space allocated to a raw ip socket.
 */
#define	RIPSNDQ		8192
#define	RIPRCVQ		8192

/*
 * Raw interface to IP protocol.
 */

/*
 * Initialize raw connection block q.
 */
void
rip_init()
{

	in_pcbinit(&rawcbtable, 1);
}

struct sockaddr_in ripsrc = { sizeof(ripsrc), AF_INET };

/*
 * Setup generic address and protocol structures
 * for raw_input routine, then pass them along with
 * mbuf chain.
 */
void
rip_input(struct mbuf *m, ...)
{
	struct ip *ip = mtod(m, struct ip *);
	struct inpcb *inp, *last = NULL;
	struct mbuf *opts = NULL;

	ripsrc.sin_addr = ip->ip_src;
	TAILQ_FOREACH(inp, &rawcbtable.inpt_queue, inp_queue) {
		if (inp->inp_socket->so_state & SS_CANTRCVMORE)
			continue;
#ifdef INET6
		if (inp->inp_flags & INP_IPV6)
			continue;
#endif
		if (rtable_l2(inp->inp_rtableid) !=
		    rtable_l2(m->m_pkthdr.ph_rtableid))
			continue;

		if (inp->inp_ip.ip_p && inp->inp_ip.ip_p != ip->ip_p)
			continue;
#if NPF > 0
		if (m->m_pkthdr.pf.flags & PF_TAG_DIVERTED) {
			struct pf_divert *divert;

			/* XXX rdomain support */
			if ((divert = pf_find_divert(m)) == NULL)
				continue;
			if (inp->inp_laddr.s_addr != divert->addr.v4.s_addr)
				continue;
		} else
#endif
		if (inp->inp_laddr.s_addr &&
		    inp->inp_laddr.s_addr != ip->ip_dst.s_addr)
			continue;
		if (inp->inp_faddr.s_addr &&
		    inp->inp_faddr.s_addr != ip->ip_src.s_addr)
			continue;
		if (last) {
			struct mbuf *n;

			if ((n = m_copy(m, 0, (int)M_COPYALL)) != NULL) {
				if (last->inp_flags & INP_CONTROLOPTS ||
				    last->inp_socket->so_options & SO_TIMESTAMP)
					ip_savecontrol(last, &opts, ip, n);
				if (sbappendaddr(&last->inp_socket->so_rcv,
				    sintosa(&ripsrc), n, opts) == 0) {
					/* should notify about lost packet */
					m_freem(n);
					if (opts)
						m_freem(opts);
				} else
					sorwakeup(last->inp_socket);
				opts = NULL;
			}
		}
		last = inp;
	}
	if (last) {
		if (last->inp_flags & INP_CONTROLOPTS ||
		    last->inp_socket->so_options & SO_TIMESTAMP)
			ip_savecontrol(last, &opts, ip, m);
		if (sbappendaddr(&last->inp_socket->so_rcv, sintosa(&ripsrc), m,
		    opts) == 0) {
			m_freem(m);
			if (opts)
				m_freem(opts);
		} else
			sorwakeup(last->inp_socket);
	} else {
		if (ip->ip_p != IPPROTO_ICMP)
			icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_PROTOCOL, 0, 0);
		else
			m_freem(m);
		ipstat.ips_noproto++;
		ipstat.ips_delivered--;
	}
}

/*
 * Generate IP header and pass packet to ip_output.
 * Tack on options user may have setup with control call.
 */
int
rip_output(struct mbuf *m, ...)
{
	struct socket *so;
	u_long dst;
	struct ip *ip;
	struct inpcb *inp;
	int flags, error;
	va_list ap;

	va_start(ap, m);
	so = va_arg(ap, struct socket *);
	dst = va_arg(ap, u_long);
	va_end(ap);

	inp = sotoinpcb(so);
	flags = IP_ALLOWBROADCAST;

	/*
	 * If the user handed us a complete IP packet, use it.
	 * Otherwise, allocate an mbuf for a header and fill it in.
	 */
	if ((inp->inp_flags & INP_HDRINCL) == 0) {
		if ((m->m_pkthdr.len + sizeof(struct ip)) > IP_MAXPACKET) {
			m_freem(m);
			return (EMSGSIZE);
		}
		M_PREPEND(m, sizeof(struct ip), M_DONTWAIT);
		if (!m)
			return (ENOBUFS);
		ip = mtod(m, struct ip *);
		ip->ip_tos = inp->inp_ip.ip_tos;
		ip->ip_off = htons(0);
		ip->ip_p = inp->inp_ip.ip_p;
		ip->ip_len = htons(m->m_pkthdr.len);
		ip->ip_src = inp->inp_laddr;
		ip->ip_dst.s_addr = dst;
		ip->ip_ttl = inp->inp_ip.ip_ttl ? inp->inp_ip.ip_ttl : MAXTTL;
	} else {
		if (m->m_pkthdr.len > IP_MAXPACKET) {
			m_freem(m);
			return (EMSGSIZE);
		}
		if (m->m_pkthdr.len < sizeof(struct ip)) {
			m_freem(m);
			return (EINVAL);
		}
		ip = mtod(m, struct ip *);
		/*
		 * don't allow both user specified and setsockopt options,
		 * and don't allow packet length sizes that will crash
		 */
		if ((ip->ip_hl != (sizeof (*ip) >> 2) && inp->inp_options) ||
		    ntohs(ip->ip_len) > m->m_pkthdr.len ||
		    ntohs(ip->ip_len) < ip->ip_hl << 2) {
			m_freem(m);
			return (EINVAL);
		}
		if (ip->ip_id == 0) {
			ip->ip_id = htons(ip_randomid());
		}
		/* XXX prevent ip_output from overwriting header fields */
		flags |= IP_RAWOUTPUT;
		ipstat.ips_rawout++;
	}
#ifdef INET6
	/*
	 * A thought:  Even though raw IP shouldn't be able to set IPv6
	 *             multicast options, if it does, the last parameter to
	 *             ip_output should be guarded against v6/v4 problems.
	 */
#endif
	/* force routing table */
	m->m_pkthdr.ph_rtableid = inp->inp_rtableid;

#if NPF > 0
	if (inp->inp_socket->so_state & SS_ISCONNECTED &&
	    ip->ip_p != IPPROTO_ICMP)
		m->m_pkthdr.pf.inp = inp;
#endif

	error = ip_output(m, inp->inp_options, &inp->inp_route, flags,
	    inp->inp_moptions, inp, 0);
	if (error == EACCES)	/* translate pf(4) error for userland */
		error = EHOSTUNREACH;
	return (error);
}

/*
 * Raw IP socket option processing.
 */
int
rip_ctloutput(int op, struct socket *so, int level, int optname,
    struct mbuf **m)
{
	struct inpcb *inp = sotoinpcb(so);
	int error = 0;
	int dir;

	if (level != IPPROTO_IP) {
		if (op == PRCO_SETOPT && *m)
			(void) m_free(*m);
		return (EINVAL);
	}

	switch (optname) {

	case IP_HDRINCL:
		error = 0;
		if (op == PRCO_SETOPT) {
			if (*m == 0 || (*m)->m_len < sizeof (int))
				error = EINVAL;
			else if (*mtod(*m, int *))
				inp->inp_flags |= INP_HDRINCL;
			else
				inp->inp_flags &= ~INP_HDRINCL;
			if (*m)
				(void)m_free(*m);
		} else {
			*m = m_get(M_WAIT, M_SOOPTS);
			(*m)->m_len = sizeof(int);
			*mtod(*m, int *) = inp->inp_flags & INP_HDRINCL;
		}
		return (error);

	case IP_DIVERTFL:
		switch (op) {
		case PRCO_SETOPT:
			if (*m == 0 || (*m)->m_len < sizeof (int)) {
				error = EINVAL;
				break;
			}
			dir = *mtod(*m, int *);
			if (inp->inp_divertfl > 0)
				error = ENOTSUP;
			else if ((dir & IPPROTO_DIVERT_RESP) ||
				   (dir & IPPROTO_DIVERT_INIT))
				inp->inp_divertfl = dir;
			else 
				error = EINVAL;

			break;

		case PRCO_GETOPT:
			*m = m_get(M_WAIT, M_SOOPTS);
			(*m)->m_len = sizeof(int);
			*mtod(*m, int *) = inp->inp_divertfl;
			break;

		default:
			error = EINVAL;
			break;
		}

		if (op == PRCO_SETOPT && *m)
			(void)m_free(*m);
		return (error);

	case MRT_INIT:
	case MRT_DONE:
	case MRT_ADD_VIF:
	case MRT_DEL_VIF:
	case MRT_ADD_MFC:
	case MRT_DEL_MFC:
	case MRT_VERSION:
	case MRT_ASSERT:
	case MRT_API_SUPPORT:
	case MRT_API_CONFIG:
#ifdef MROUTING
		switch (op) {
		case PRCO_SETOPT:
			error = ip_mrouter_set(so, optname, m);
			break;
		case PRCO_GETOPT:
			error = ip_mrouter_get(so, optname, m);
			break;
		default:
			error = EINVAL;
			break;
		}
		return (error);
#else
		if (op == PRCO_SETOPT && *m)
			m_free(*m);
		return (EOPNOTSUPP);
#endif
	}
	return (ip_ctloutput(op, so, level, optname, m));
}

u_long	rip_sendspace = RIPSNDQ;
u_long	rip_recvspace = RIPRCVQ;

/*ARGSUSED*/
int
rip_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *nam,
    struct mbuf *control, struct proc *p)
{
	struct inpcb *inp = sotoinpcb(so);
	int error = 0;
	int s;

	if (req == PRU_CONTROL)
		return (in_control(so, (u_long)m, (caddr_t)nam,
		    (struct ifnet *)control));

	if (inp == NULL && req != PRU_ATTACH) {
		error = EINVAL;
		goto release;
	}

	switch (req) {

	case PRU_ATTACH:
		if (inp)
			panic("rip_attach");
		if ((so->so_state & SS_PRIV) == 0) {
			error = EACCES;
			break;
		}
		if ((long)nam < 0 || (long)nam >= IPPROTO_MAX) {
			error = EPROTONOSUPPORT;
			break;
		}
		s = splsoftnet();
		if ((error = soreserve(so, rip_sendspace, rip_recvspace)) ||
		    (error = in_pcballoc(so, &rawcbtable))) {
			splx(s);
			break;
		}
		splx(s);
		inp = sotoinpcb(so);
		inp->inp_ip.ip_p = (long)nam;
		break;

	case PRU_DISCONNECT:
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			error = ENOTCONN;
			break;
		}
		/* FALLTHROUGH */
	case PRU_ABORT:
		soisdisconnected(so);
		/* FALLTHROUGH */
	case PRU_DETACH:
		if (inp == 0)
			panic("rip_detach");
#ifdef MROUTING
		if (so == ip_mrouter)
			ip_mrouter_done();
#endif
		in_pcbdetach(inp);
		break;

	case PRU_BIND:
	    {
		struct sockaddr_in *addr = mtod(nam, struct sockaddr_in *);

		if (nam->m_len != sizeof(*addr)) {
			error = EINVAL;
			break;
		}
		if (addr->sin_family != AF_INET) {
			error = EADDRNOTAVAIL;
			break;
		}
		if (!((so->so_options & SO_BINDANY) ||
		    addr->sin_addr.s_addr == INADDR_ANY ||
		    addr->sin_addr.s_addr == INADDR_BROADCAST ||
		    ifa_ifwithaddr(sintosa(addr), inp->inp_rtableid))) {
			error = EADDRNOTAVAIL;
			break;
		}
		inp->inp_laddr = addr->sin_addr;
		break;
	    }
	case PRU_CONNECT:
	    {
		struct sockaddr_in *addr = mtod(nam, struct sockaddr_in *);

		if (nam->m_len != sizeof(*addr)) {
			error = EINVAL;
			break;
		}
		if (addr->sin_family != AF_INET) {
			error = EAFNOSUPPORT;
			break;
		}
		inp->inp_faddr = addr->sin_addr;
		soisconnected(so);
		break;
	    }

	case PRU_CONNECT2:
		error = EOPNOTSUPP;
		break;

	/*
	 * Mark the connection as being incapable of further input.
	 */
	case PRU_SHUTDOWN:
		socantsendmore(so);
		break;

	/*
	 * Ship a packet out.  The appropriate raw output
	 * routine handles any massaging necessary.
	 */
	case PRU_SEND:
	    {
		u_int32_t dst;

		if (so->so_state & SS_ISCONNECTED) {
			if (nam) {
				error = EISCONN;
				break;
			}
			dst = inp->inp_faddr.s_addr;
		} else {
			if (nam == NULL) {
				error = ENOTCONN;
				break;
			}
			dst = mtod(nam, struct sockaddr_in *)->sin_addr.s_addr;
		}
#ifdef IPSEC
		/* XXX Find an IPsec TDB */
#endif
		error = rip_output(m, so, dst);
		m = NULL;
		break;
	    }

	case PRU_SENSE:
		/*
		 * stat: don't bother with a blocksize.
		 */
		return (0);

	/*
	 * Not supported.
	 */
	case PRU_RCVOOB:
	case PRU_RCVD:
	case PRU_LISTEN:
	case PRU_ACCEPT:
	case PRU_SENDOOB:
		error = EOPNOTSUPP;
		break;

	case PRU_SOCKADDR:
		in_setsockaddr(inp, nam);
		break;

	case PRU_PEERADDR:
		in_setpeeraddr(inp, nam);
		break;

	default:
		panic("rip_usrreq");
	}
release:
	if (m != NULL)
		m_freem(m);
	return (error);
}
