/*      $OpenBSD: ip_divert.c,v 1.1 2009/09/08 17:00:41 michele Exp $ */

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
#include <net/route.h>
#include <net/netisr.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_divert.h>

#include "pf.h"
#if NPF > 0
#include <net/pfvar.h>
#endif

struct	inpcbtable divbtable;

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

int	divbhashsize = DIVERTHASHSIZE;

void	divert_detach(struct inpcb *);

void
divert_init()
{
	in_pcbinit(&divbtable, divbhashsize);
}

/* Dummy function, so drop */
void
divert_input(struct mbuf *m, ...)
{
	m_freem(m);
}

int
divert_output(struct mbuf *m, ...)
{
	struct inpcb *inp;
	struct ifqueue *inq;
	struct mbuf *nam, *control;
	struct sockaddr_in *sin;
	struct socket *so;
	struct ifaddr *ifa;
	int s, error = 0;
	va_list ap;

	va_start(ap, m);
	inp = va_arg(ap, struct inpcb *);
	nam = va_arg(ap, struct mbuf *);
	control = va_arg(ap, struct mbuf *);
	va_end(ap);

	m->m_pkthdr.rcvif = NULL;
	m->m_nextpkt = NULL;

	if (control)
		m_freem(control);

	sin = mtod(nam, struct sockaddr_in *);
	so = inp->inp_socket;

	m->m_pkthdr.pf.flags |= PF_TAG_DIVERTED_PACKET;

	if (sin->sin_addr.s_addr != INADDR_ANY) {
		ifa = ifa_ifwithaddr((struct sockaddr *)sin, 0);
		if (ifa == NULL) {
			m_freem(m);
			return (EADDRNOTAVAIL);
		}
		m->m_pkthdr.rcvif = ifa->ifa_ifp;

		inq = &ipintrq;

		s = splnet();
		IF_INPUT_ENQUEUE(inq, m);
		schednetisr(NETISR_IP);
		splx(s);
	} else {
		error = ip_output(m, (void *)NULL, &inp->inp_route,
		    ((so->so_options & SO_DONTROUTE) ? IP_ROUTETOIF : 0)
		    | IP_ALLOWBROADCAST | IP_RAWOUTPUT, (void *)NULL,
		    (void *)NULL);
	}

	return (error);
}

void
divert_packet(struct mbuf *m, int dir)
{
	struct inpcb *inp;
	struct socket *sa = NULL;
	struct sockaddr_in addr;
	struct pf_divert *pd;

	if (m->m_len < sizeof(struct ip) &&
	    (m = m_pullup(m, sizeof(struct ip))) == NULL)
		return;

	pd = pf_find_divert(m);
	if (pd == NULL) {
		m_freem(m);
		return;
	}

	bzero(&addr, sizeof(addr));
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

	CIRCLEQ_FOREACH(inp, &divbtable.inpt_queue, inp_queue) {
		if (inp->inp_lport != pd->port)
			continue;

		sa = inp->inp_socket;
		if (sbappendaddr(&sa->so_rcv, (struct sockaddr *)&addr, 
		    m, NULL) == 0) {
			m_freem(m);
			return;
		} else
			sorwakeup(inp->inp_socket);
		break;
	}

	if (sa == NULL)
		m_freem(m);
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
		((struct inpcb *) so->so_pcb)->inp_flags |= INP_HDRINCL;
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
		return (divert_output(m, inp, addr, control));

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
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case DIVERTCTL_SENDSPACE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &divert_sendspace));
	case DIVERTCTL_RECVSPACE:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &divert_recvspace));
	default:
		if (name[0] < DIVERTCTL_MAXID)
		return sysctl_int_arr(divertctl_vars, name, namelen,
		    oldp, oldlenp, newp, newlen);

		return (ENOPROTOOPT);
	}
}
