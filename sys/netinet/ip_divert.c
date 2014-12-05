/*      $OpenBSD: ip_divert.c,v 1.31 2014/12/05 15:50:04 mpi Exp $ */

/*
 * Copyright (c) 2009 Michele Marchetto <michele@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/netisr.h>
#include <net/pfvar.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_divert.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>

struct	inpcbtable	divbtable;
struct	divstat		divstat;

#ifndef DIVERT_SENDSPACE
#define DIVERT_SENDSPACE	(65536 + 100)
#endif
u_int   divert_sendspace = DIVERT_SENDSPACE;
#ifndef DIVERT_RECVSPACE
#define DIVERT_RECVSPACE	(65536 + 100)
#endif
u_int   divert_recvspace = DIVERT_RECVSPACE;

#ifndef DIVERTHASHSIZE
#define DIVERTHASHSIZE	128
#endif

int *divertctl_vars[DIVERTCTL_MAXID] = DIVERTCTL_VARS;

int divbhashsize = DIVERTHASHSIZE;

static struct sockaddr_in ipaddr = { sizeof(ipaddr), AF_INET };

void	divert_detach(struct inpcb *);
int	divert_output(struct inpcb *, struct mbuf *, struct mbuf *,
	    struct mbuf *);
void
divert_init()
{
	in_pcbinit(&divbtable, divbhashsize);
}

void
divert_input(struct mbuf *m, ...)
{
	m_freem(m);
}

int
divert_output(struct inpcb *inp, struct mbuf *m, struct mbuf *nam,
    struct mbuf *control)
{
	struct ifqueue *inq;
	struct sockaddr_in *sin;
	struct socket *so;
	struct ifaddr *ifa;
	int s, error = 0, min_hdrlen = 0, dir;
	struct ip *ip;
	u_int16_t off;

	m->m_pkthdr.rcvif = NULL;
	m->m_nextpkt = NULL;
	m->m_pkthdr.ph_rtableid = inp->inp_rtableid;

	if (control)
		m_freem(control);

	sin = mtod(nam, struct sockaddr_in *);
	so = inp->inp_socket;

	/* Do basic sanity checks. */
	if (m->m_pkthdr.len < sizeof(struct ip))
		goto fail;
	if ((m = m_pullup(m, sizeof(struct ip))) == NULL) {
		/* m_pullup() has freed the mbuf, so just return. */
		divstat.divs_errors++;
		return (ENOBUFS);
	}
	ip = mtod(m, struct ip *);
	if (ip->ip_v != IPVERSION)
		goto fail;
	off = ip->ip_hl << 2;
	if (off < sizeof(struct ip) || ntohs(ip->ip_len) < off ||
	    m->m_pkthdr.len < ntohs(ip->ip_len))
		goto fail;

	dir = (sin->sin_addr.s_addr == INADDR_ANY ? PF_OUT : PF_IN);

	switch (ip->ip_p) {
	case IPPROTO_TCP:
		min_hdrlen = sizeof(struct tcphdr);
		m->m_pkthdr.csum_flags |= M_TCP_CSUM_OUT;
		break;
	case IPPROTO_UDP:
		min_hdrlen = sizeof(struct udphdr);
		m->m_pkthdr.csum_flags |= M_UDP_CSUM_OUT;
		break;
	case IPPROTO_ICMP:
		min_hdrlen = ICMP_MINLEN;
		m->m_pkthdr.csum_flags |= M_ICMP_CSUM_OUT;
		break;
	default:
		/* nothing */
		break;
	}
	if (min_hdrlen && m->m_pkthdr.len < off + min_hdrlen)
		goto fail;

	m->m_pkthdr.pf.flags |= PF_TAG_DIVERTED_PACKET;

	if (dir == PF_IN) {
		ipaddr.sin_addr = sin->sin_addr;
		ifa = ifa_ifwithaddr(sintosa(&ipaddr), m->m_pkthdr.ph_rtableid);
		if (ifa == NULL) {
			error = EADDRNOTAVAIL;
			goto fail;
		}
		m->m_pkthdr.rcvif = ifa->ifa_ifp;

		inq = &ipintrq;

		/*
		 * Recalculate IP and protocol checksums for the inbound packet
		 * since the userspace application may have modified the packet
		 * prior to reinjection.
		 */
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(m, off);
		in_proto_cksum_out(m, NULL);

		s = splnet();
		IF_INPUT_ENQUEUE(inq, m);
		schednetisr(NETISR_IP);
		splx(s);
	} else {
		error = ip_output(m, NULL, &inp->inp_route,
		    IP_ALLOWBROADCAST | IP_RAWOUTPUT, NULL, NULL, 0);
		if (error == EACCES)	/* translate pf(4) error for userland */
			error = EHOSTUNREACH;
	}

	divstat.divs_opackets++;
	return (error);

fail:
	m_freem(m);
	divstat.divs_errors++;
	return (error ? error : EINVAL);
}

int
divert_packet(struct mbuf *m, int dir, u_int16_t divert_port)
{
	struct inpcb *inp;
	struct socket *sa = NULL;
	struct sockaddr_in addr;

	inp = NULL;
	divstat.divs_ipackets++;

	if (m->m_len < sizeof(struct ip) &&
	    (m = m_pullup(m, sizeof(struct ip))) == NULL) {
		divstat.divs_errors++;
		return (0);
	}

	TAILQ_FOREACH(inp, &divbtable.inpt_queue, inp_queue) {
		if (inp->inp_lport != divert_port)
			continue;
		if (inp->inp_divertfl == 0)
			break;
		if (dir == PF_IN && !(inp->inp_divertfl & IPPROTO_DIVERT_RESP))
			return (-1);
		if (dir == PF_OUT && !(inp->inp_divertfl & IPPROTO_DIVERT_INIT))
			return (-1);
		break;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_len = sizeof(addr);

	if (dir == PF_IN) {
		struct ifaddr *ifa;
		struct ifnet *ifp;

		ifp = m->m_pkthdr.rcvif;
		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;
			addr.sin_addr.s_addr = ((struct sockaddr_in *)
			    ifa->ifa_addr)->sin_addr.s_addr;
			break;
		}
	}

	if (inp) {
		sa = inp->inp_socket;
		if (sbappendaddr(&sa->so_rcv, (struct sockaddr *)&addr, 
		    m, NULL) == 0) {
			divstat.divs_fullsock++;
			m_freem(m);
			return (0);
		} else
			sorwakeup(inp->inp_socket);
	}

	if (sa == NULL) {
		divstat.divs_noport++;
		m_freem(m);
	}
	return (0);
}

/*ARGSUSED*/
int
divert_usrreq(struct socket *so, int req, struct mbuf *m, struct mbuf *addr,
    struct mbuf *control, struct proc *p)
{
	struct inpcb *inp = sotoinpcb(so);
	int error = 0;
	int s;

	if (req == PRU_CONTROL) {
		return (in_control(so, (u_long)m, (caddr_t)addr,
		    (struct ifnet *)control));
	}
	if (inp == NULL && req != PRU_ATTACH) {
		error = EINVAL;
		goto release;
	}
	switch (req) {

	case PRU_ATTACH:
		if (inp != NULL) {
			error = EINVAL;
			break;
		}
		if ((so->so_state & SS_PRIV) == 0) {
			error = EACCES;
			break;
		}
		s = splsoftnet();
		error = in_pcballoc(so, &divbtable);
		splx(s);
		if (error)
			break;

		error = soreserve(so, divert_sendspace, divert_recvspace);
		if (error)
			break;
		sotoinpcb(so)->inp_flags |= INP_HDRINCL;
		break;

	case PRU_DETACH:
		divert_detach(inp);
		break;

	case PRU_BIND:
		s = splsoftnet();
		error = in_pcbbind(inp, addr, p);
		splx(s);
		break;

	case PRU_SHUTDOWN:
		socantsendmore(so);
		break;

	case PRU_SEND:
		return (divert_output(inp, m, addr, control));

	case PRU_ABORT:
		soisdisconnected(so);
		divert_detach(inp);
		break;

	case PRU_SOCKADDR:
		in_setsockaddr(inp, addr);
		break;

	case PRU_PEERADDR:
		in_setpeeraddr(inp, addr);
		break;

	case PRU_SENSE:
		return (0);

	case PRU_LISTEN:
	case PRU_CONNECT:
	case PRU_CONNECT2:
	case PRU_ACCEPT:
	case PRU_DISCONNECT:
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
		panic("divert_usrreq");
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
divert_detach(struct inpcb *inp)
{
	int s = splsoftnet();

	in_pcbdetach(inp);
	splx(s);
}

/*
 * Sysctl for divert variables.
 */
int
divert_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case DIVERTCTL_SENDSPACE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &divert_sendspace));
	case DIVERTCTL_RECVSPACE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &divert_recvspace));
	case DIVERTCTL_STATS:
		if (newp != NULL)
			return (EPERM);
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    &divstat, sizeof(divstat)));
	default:
		if (name[0] < DIVERTCTL_MAXID)
			return sysctl_int_arr(divertctl_vars, name, namelen,
			    oldp, oldlenp, newp, newlen);

		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}
