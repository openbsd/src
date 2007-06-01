/*	$OpenBSD: rfcomm_socket.c,v 1.1 2007/06/01 02:46:12 uwe Exp $	*/
/*	$NetBSD: rfcomm_socket.c,v 1.7 2007/04/21 06:15:23 plunky Exp $	*/

/*-
 * Copyright (c) 2006 Itronix Inc.
 * All rights reserved.
 *
 * Written by Iain Hibbert for Itronix Inc.
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
#include <netbt/hci.h>		/* XXX for EPASSTHROUGH */
#include <netbt/rfcomm.h>

/****************************************************************************
 *
 *	RFCOMM SOCK_STREAM Sockets - serial line emulation
 *
 */

static void rfcomm_connecting(void *);
static void rfcomm_connected(void *);
static void rfcomm_disconnected(void *, int);
static void *rfcomm_newconn(void *, struct sockaddr_bt *, struct sockaddr_bt *);
static void rfcomm_complete(void *, int);
static void rfcomm_linkmode(void *, int);
static void rfcomm_input(void *, struct mbuf *);

static const struct btproto rfcomm_proto = {
	rfcomm_connecting,
	rfcomm_connected,
	rfcomm_disconnected,
	rfcomm_newconn,
	rfcomm_complete,
	rfcomm_linkmode,
	rfcomm_input,
};

/* sysctl variables */
int rfcomm_sendspace = 4096;
int rfcomm_recvspace = 4096;

/*
 * User Request.
 * up is socket
 * m is either
 *	optional mbuf chain containing message
 *	ioctl command (PRU_CONTROL)
 * nam is either
 *	optional mbuf chain containing an address
 *	ioctl data (PRU_CONTROL)
 *	optionally protocol number (PRU_ATTACH)
 *	message flags (PRU_RCVD)
 * ctl is either
 *	optional mbuf chain containing socket options
 *	optional interface pointer (PRU_CONTROL, PRU_PURGEIF)
 * l is pointer to process requesting action (if any)
 *
 * we are responsible for disposing of m and ctl if
 * they are mbuf chains
 */
int
rfcomm_usrreq(struct socket *up, int req, struct mbuf *m,
		struct mbuf *nam, struct mbuf *ctl)
{
	struct rfcomm_dlc *pcb = up->so_pcb;
	struct sockaddr_bt *sa;
	struct mbuf *m0;
	int err = 0;

#ifdef notyet			/* XXX */
	DPRINTFN(2, "%s\n", prurequests[req]);
#endif

	switch (req) {
	case PRU_CONTROL:
		return EPASSTHROUGH;

#ifdef notyet			/* XXX */
	case PRU_PURGEIF:
		return EOPNOTSUPP;
#endif

	case PRU_ATTACH:
		if (pcb != NULL)
			return EINVAL;

		/*
		 * Since we have nothing to add, we attach the DLC
		 * structure directly to our PCB pointer.
		 */
		err = rfcomm_attach((struct rfcomm_dlc **)&up->so_pcb,
					&rfcomm_proto, up);
		if (err)
			return err;

		err = soreserve(up, rfcomm_sendspace, rfcomm_recvspace);
		if (err)
			return err;

		err = rfcomm_rcvd(up->so_pcb, sbspace(&up->so_rcv));
		if (err)
			return err;

		return 0;
	}

	if (pcb == NULL) {
		err = EINVAL;
		goto release;
	}

	switch(req) {
	case PRU_DISCONNECT:
		soisdisconnecting(up);
		return rfcomm_disconnect(pcb, up->so_linger);

	case PRU_ABORT:
		rfcomm_disconnect(pcb, 0);
		soisdisconnected(up);
		/* fall through to */
	case PRU_DETACH:
		return rfcomm_detach((struct rfcomm_dlc **)&up->so_pcb);

	case PRU_BIND:
		KASSERT(nam != NULL);
		sa = mtod(nam, struct sockaddr_bt *);

		if (sa->bt_len != sizeof(struct sockaddr_bt))
			return EINVAL;

		if (sa->bt_family != AF_BLUETOOTH)
			return EAFNOSUPPORT;

		return rfcomm_bind(pcb, sa);

	case PRU_CONNECT:
		KASSERT(nam != NULL);
		sa = mtod(nam, struct sockaddr_bt *);

		if (sa->bt_len != sizeof(struct sockaddr_bt))
			return EINVAL;

		if (sa->bt_family != AF_BLUETOOTH)
			return EAFNOSUPPORT;

		soisconnecting(up);
		return rfcomm_connect(pcb, sa);

	case PRU_PEERADDR:
		KASSERT(nam != NULL);
		sa = mtod(nam, struct sockaddr_bt *);
		nam->m_len = sizeof(struct sockaddr_bt);
		return rfcomm_peeraddr(pcb, sa);

	case PRU_SOCKADDR:
		KASSERT(nam != NULL);
		sa = mtod(nam, struct sockaddr_bt *);
		nam->m_len = sizeof(struct sockaddr_bt);
		return rfcomm_sockaddr(pcb, sa);

	case PRU_SHUTDOWN:
		socantsendmore(up);
		break;

	case PRU_SEND:
		KASSERT(m != NULL);

		if (ctl)	/* no use for that */
			m_freem(ctl);

		m0 = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
		if (m0 == NULL)
			return ENOMEM;

		sbappendstream(&up->so_snd, m);

		return rfcomm_send(pcb, m0);

	case PRU_SENSE:
		return 0;		/* (no release) */

	case PRU_RCVD:
		return rfcomm_rcvd(pcb, sbspace(&up->so_rcv));

	case PRU_RCVOOB:
		return EOPNOTSUPP;	/* (no release) */

	case PRU_LISTEN:
		return rfcomm_listen(pcb);

	case PRU_ACCEPT:
		KASSERT(nam != NULL);
		sa = mtod(nam, struct sockaddr_bt *);
		nam->m_len = sizeof(struct sockaddr_bt);
		return rfcomm_peeraddr(pcb, sa);

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
 * rfcomm_ctloutput(request, socket, level, optname, opt)
 *
 */
int
rfcomm_ctloutput(int req, struct socket *so, int level,
		int optname, struct mbuf **opt)
{
	struct rfcomm_dlc *pcb = so->so_pcb;
	struct mbuf *m;
	int err = 0;

#ifdef notyet			/* XXX */
	DPRINTFN(2, "%s\n", prcorequests[req]);
#endif

	if (pcb == NULL)
		return EINVAL;

	if (level != BTPROTO_RFCOMM)
		return ENOPROTOOPT;

	switch(req) {
	case PRCO_GETOPT:
		m = m_get(M_WAIT, MT_SOOPTS);
		m->m_len = rfcomm_getopt(pcb, optname, mtod(m, void *));
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
		err = rfcomm_setopt(pcb, optname, mtod(m, void *));
		m_freem(m);
		break;

	default:
		err = ENOPROTOOPT;
		break;
	}

	return err;
}

/**********************************************************************
 *
 * RFCOMM callbacks
 */

static void
rfcomm_connecting(void *arg)
{
	/* struct socket *so = arg; */

	KASSERT(arg != NULL);
	DPRINTF("Connecting\n");
}

static void
rfcomm_connected(void *arg)
{
	struct socket *so = arg;

	KASSERT(so != NULL);
	DPRINTF("Connected\n");
	soisconnected(so);
}

static void
rfcomm_disconnected(void *arg, int err)
{
	struct socket *so = arg;

	KASSERT(so != NULL);
	DPRINTF("Disconnected\n");

	so->so_error = err;
	soisdisconnected(so);
}

static void *
rfcomm_newconn(void *arg, struct sockaddr_bt *laddr,
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

/*
 * rfcomm_complete(rfcomm_dlc, length)
 *
 * length bytes are sent and may be removed from socket buffer
 */
static void
rfcomm_complete(void *arg, int length)
{
	struct socket *so = arg;

	sbdrop(&so->so_snd, length);
	sowwakeup(so);
}

/*
 * rfcomm_linkmode(rfcomm_dlc, new)
 *
 * link mode change notification.
 */
static void
rfcomm_linkmode(void *arg, int new)
{
	struct socket *so = arg;
	int mode;

	DPRINTF("auth %s, encrypt %s, secure %s\n",
		(new & RFCOMM_LM_AUTH ? "on" : "off"),
		(new & RFCOMM_LM_ENCRYPT ? "on" : "off"),
		(new & RFCOMM_LM_SECURE ? "on" : "off"));

	(void)rfcomm_getopt(so->so_pcb, SO_RFCOMM_LM, &mode);
	if (((mode & RFCOMM_LM_AUTH) && !(new & RFCOMM_LM_AUTH))
	    || ((mode & RFCOMM_LM_ENCRYPT) && !(new & RFCOMM_LM_ENCRYPT))
	    || ((mode & RFCOMM_LM_SECURE) && !(new & RFCOMM_LM_SECURE)))
		rfcomm_disconnect(so->so_pcb, 0);
}

/*
 * rfcomm_input(rfcomm_dlc, mbuf)
 */
static void
rfcomm_input(void *arg, struct mbuf *m)
{
	struct socket *so = arg;

	KASSERT(so != NULL);

	if (m->m_pkthdr.len > sbspace(&so->so_rcv)) {
		printf("%s: %d bytes dropped (socket buffer full)\n",
			__func__, m->m_pkthdr.len);
		m_freem(m);
		return;
	}

	DPRINTFN(10, "received %d bytes\n", m->m_pkthdr.len);

	sbappendstream(&so->so_rcv, m);
	sorwakeup(so);
}
