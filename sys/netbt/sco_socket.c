/*	$OpenBSD: sco_socket.c,v 1.1 2007/06/01 02:46:12 uwe Exp $	*/
/*	$NetBSD: sco_socket.c,v 1.9 2007/04/21 06:15:23 plunky Exp $	*/

/*-
 * Copyright (c) 2006 Itronix Inc.
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
 * 3. The name of Itronix Inc. may not be used to endorse
 *    or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ITRONIX INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ITRONIX INC. BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>

/* load symbolic names */
#ifdef BLUETOOTH_DEBUG
#define PRUREQUESTS
#define PRCOREQUESTS
#endif

#include <sys/param.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/systm.h>

#include <netbt/bluetooth.h>
#include <netbt/hci.h>
#include <netbt/sco.h>

/*******************************************************************************
 *
 * SCO SOCK_SEQPACKET sockets - low latency audio data
 */

static void sco_connecting(void *);
static void sco_connected(void *);
static void sco_disconnected(void *, int);
static void *sco_newconn(void *, struct sockaddr_bt *, struct sockaddr_bt *);
static void sco_complete(void *, int);
static void sco_linkmode(void *, int);
static void sco_input(void *, struct mbuf *);

static const struct btproto sco_proto = {
	sco_connecting,
	sco_connected,
	sco_disconnected,
	sco_newconn,
	sco_complete,
	sco_linkmode,
	sco_input,
};

int sco_sendspace = 4096;
int sco_recvspace = 4096;

/*
 * User Request.
 * up is socket
 * m is either
 *	optional mbuf chain containing message
 *	ioctl command (PRU_CONTROL)
 * nam is either
 *	optional mbuf chain containing an address
 *	ioctl data (PRU_CONTROL)
 *      optionally, protocol number (PRU_ATTACH)
 * ctl is optional mbuf chain containing socket options
 * l is pointer to process requesting action (if any)
 *
 * we are responsible for disposing of m and ctl if
 * they are mbuf chains
 */
int
sco_usrreq(struct socket *up, int req, struct mbuf *m,
    struct mbuf *nam, struct mbuf *ctl)
{
	struct sco_pcb *pcb = (struct sco_pcb *)up->so_pcb;
	struct sockaddr_bt *sa;
	struct mbuf *m0;
	int err = 0;

#ifdef notyet			/* XXX */
	DPRINTFN(2, "%s\n", prurequests[req]);
#endif

	switch(req) {
	case PRU_CONTROL:
		return EOPNOTSUPP;

#ifdef notyet			/* XXX */
	case PRU_PURGEIF:
		return EOPNOTSUPP;
#endif

	case PRU_ATTACH:
		if (pcb)
			return EINVAL;

		err = soreserve(up, sco_sendspace, sco_recvspace);
		if (err)
			return err;

		return sco_attach((struct sco_pcb **)&up->so_pcb,
					&sco_proto, up);
	}

	/* anything after here *requires* a pcb */
	if (pcb == NULL) {
		err = EINVAL;
		goto release;
	}

	switch(req) {
	case PRU_DISCONNECT:
		soisdisconnecting(up);
		return sco_disconnect(pcb, up->so_linger);

	case PRU_ABORT:
		sco_disconnect(pcb, 0);
		soisdisconnected(up);
		/* fall through to */
	case PRU_DETACH:
		return sco_detach((struct sco_pcb **)&up->so_pcb);

	case PRU_BIND:
		KASSERT(nam != NULL);
		sa = mtod(nam, struct sockaddr_bt *);

		if (sa->bt_len != sizeof(struct sockaddr_bt))
			return EINVAL;

		if (sa->bt_family != AF_BLUETOOTH)
			return EAFNOSUPPORT;

		return sco_bind(pcb, sa);

	case PRU_CONNECT:
		KASSERT(nam != NULL);
		sa = mtod(nam, struct sockaddr_bt *);

		if (sa->bt_len != sizeof(struct sockaddr_bt))
			return EINVAL;

		if (sa->bt_family != AF_BLUETOOTH)
			return EAFNOSUPPORT;

		soisconnecting(up);
		return sco_connect(pcb, sa);

	case PRU_PEERADDR:
		KASSERT(nam != NULL);
		sa = mtod(nam, struct sockaddr_bt *);
		nam->m_len = sizeof(struct sockaddr_bt);
		return sco_peeraddr(pcb, sa);

	case PRU_SOCKADDR:
		KASSERT(nam != NULL);
		sa = mtod(nam, struct sockaddr_bt *);
		nam->m_len = sizeof(struct sockaddr_bt);
		return sco_sockaddr(pcb, sa);

	case PRU_SHUTDOWN:
		socantsendmore(up);
		break;

	case PRU_SEND:
		KASSERT(m != NULL);
		if (m->m_pkthdr.len == 0)
			break;

		if (m->m_pkthdr.len > pcb->sp_mtu) {
			err = EMSGSIZE;
			break;
		}

		m0 = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
		if (m0 == NULL) {
			err = ENOMEM;
			break;
		}

		if (ctl) /* no use for that */
			m_freem(ctl);

		sbappendrecord(&up->so_snd, m);
		return sco_send(pcb, m0);

	case PRU_SENSE:
		return 0;		/* (no sense - Doh!) */

	case PRU_RCVD:
	case PRU_RCVOOB:
		return EOPNOTSUPP;	/* (no release) */

	case PRU_LISTEN:
		return sco_listen(pcb);

	case PRU_ACCEPT:
		KASSERT(nam != NULL);
		sa = mtod(nam, struct sockaddr_bt *);
		nam->m_len = sizeof(struct sockaddr_bt);
		return sco_peeraddr(pcb, sa);

	case PRU_CONNECT2:
	case PRU_SENDOOB:
	case PRU_FASTTIMO:
	case PRU_SLOWTIMO:
	case PRU_PROTORCV:
	case PRU_PROTOSEND:
		err = EOPNOTSUPP;
		break;

	default:
		UNKNOWN(req);
		err = EOPNOTSUPP;
		break;
	}

release:
	if (m) m_freem(m);
	if (ctl) m_freem(ctl);
	return err;
}

/*
 * get/set socket options
 */
int
sco_ctloutput(int req, struct socket *so, int level,
		int optname, struct mbuf **opt)
{
	struct sco_pcb *pcb = (struct sco_pcb *)so->so_pcb;
	struct mbuf *m;
	int err = 0;

#ifdef notyet			/* XXX */
	DPRINTFN(2, "req %s\n", prcorequests[req]);
#endif

	if (pcb == NULL)
		return EINVAL;

	if (level != BTPROTO_SCO)
		return ENOPROTOOPT;

	switch(req) {
	case PRCO_GETOPT:
		m = m_get(M_WAIT, MT_SOOPTS);
		m->m_len = sco_getopt(pcb, optname, mtod(m, uint8_t *));
		if (m->m_len == 0) {
			m_freem(m);
			m = NULL;
			err = ENOPROTOOPT;
		}
		*opt = m;
		break;

	case PRCO_SETOPT:
		m = *opt;
		KASSERT(m != NULL);
		err = sco_setopt(pcb, optname, mtod(m, uint8_t *));
		m_freem(m);
		break;

	default:
		err = ENOPROTOOPT;
		break;
	}

	return err;
}

/*****************************************************************************
 *
 *	SCO Protocol socket callbacks
 *
 */
static void
sco_connecting(void *arg)
{
	struct socket *so = arg;

	DPRINTF("Connecting\n");
	soisconnecting(so);
}

static void
sco_connected(void *arg)
{
	struct socket *so = arg;

	DPRINTF("Connected\n");
	soisconnected(so);
}

static void
sco_disconnected(void *arg, int err)
{
	struct socket *so = arg;

	DPRINTF("Disconnected (%d)\n", err);

	so->so_error = err;
	soisdisconnected(so);
}

static void *
sco_newconn(void *arg, struct sockaddr_bt *laddr,
    struct sockaddr_bt *raddr)
{
	struct socket *so = arg;

	DPRINTF("New Connection\n");
	so = sonewconn(so, 0);
	if (so == NULL)
		return NULL;

	soisconnecting(so);
	return so->so_pcb;
}

static void
sco_complete(void *arg, int num)
{
	struct socket *so = arg;

	while (num-- > 0)
		sbdroprecord(&so->so_snd);

	sowwakeup(so);
}

static void
sco_linkmode(void *arg, int mode)
{
}

static void
sco_input(void *arg, struct mbuf *m)
{
	struct socket *so = arg;

	/*
	 * since this data is time sensitive, if the buffer
	 * is full we just dump data until the latest one
	 * will fit.
	 */

	while (m->m_pkthdr.len > sbspace(&so->so_rcv))
		sbdroprecord(&so->so_rcv);

	DPRINTFN(10, "received %d bytes\n", m->m_pkthdr.len);

	sbappendrecord(&so->so_rcv, m);
	sorwakeup(so);
}
