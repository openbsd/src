/*	$OpenBSD: tp_usrreq.c,v 1.7 2003/12/10 07:22:44 itojun Exp $	*/
/*	$NetBSD: tp_usrreq.c,v 1.9 1996/03/16 23:14:06 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993
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
 *	@(#)tp_usrreq.c	8.1 (Berkeley) 6/10/93
 */

/***********************************************************
				Copyright IBM Corporation 1987

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of IBM not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.

IBM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
IBM BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/*
 * ARGO Project, Computer Sciences Dept., University of Wisconsin - Madison
 */
/*
 * tp_usrreq(), the fellow that gets called from most of the socket code.
 * Pretty straighforward. THe only really awful stuff here is the OOB
 * processing, which is done wholly here. tp_rcvoob() and tp_sendoob() are
 * contained here and called by tp_usrreq().
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/proc.h>

#include <netiso/tp_param.h>
#include <netiso/tp_timer.h>
#include <netiso/tp_stat.h>
#include <netiso/tp_seq.h>
#include <netiso/tp_ip.h>
#include <netiso/tp_pcb.h>
#include <netiso/tp_var.h>
#include <netiso/argo_debug.h>
#include <netiso/tp_trace.h>
#include <netiso/tp_meas.h>
#include <netiso/iso.h>
#include <netiso/iso_errno.h>

int             TNew;
int             TPNagle1, TPNagle2;
struct tp_pcb  *tp_listeners, *tp_intercepts;

#ifdef ARGO_DEBUG
/*
 * CALLED FROM:
 *  anywhere you want to debug...
 * FUNCTION and ARGUMENTS:
 *  print (str) followed by the control info in the mbufs of an mbuf chain (n)
 */
void
dump_mbuf(n, str)
	struct mbuf    *n;
	char           *str;
{
	struct mbuf    *nextrecord;

	printf("dump %s\n", str);

	if (n == MNULL) {
		printf("EMPTY:\n");
		return;
	}
	while (n) {
		nextrecord = n->m_act;
		printf("RECORD:\n");
		while (n) {
			printf("%p : Len %x Data %p A %p Nx %p Tp %x\n",
			       n, n->m_len, n->m_data, n->m_act, n->m_next, n->m_type);
#ifdef notdef
			{
				char  *p = mtod(n, char *);
				int    i;

				printf("data: ");
				for (i = 0; i < n->m_len; i++) {
					if (i % 8 == 0)
						printf("\n");
					printf("0x%x ", *(p + i));
				}
				printf("\n");
			}
#endif				/* notdef */
			if (n->m_next == n) {
				printf("LOOP!\n");
				return;
			}
			n = n->m_next;
		}
		n = nextrecord;
	}
	printf("\n");
}

#endif				/* ARGO_DEBUG */

/*
 * CALLED FROM:
 *  tp_usrreq(), PRU_RCVOOB
 * FUNCTION and ARGUMENTS:
 * 	Copy data from the expedited data socket buffer into
 * 	the pre-allocated mbuf m.
 * 	There is an isomorphism between XPD TPDUs and expedited data TSDUs.
 * 	XPD tpdus are limited to 16 bytes of data so they fit in one mbuf.
 * RETURN VALUE:
 *  EINVAL if debugging is on and a disaster has occurred
 *  ENOTCONN if the socket isn't connected
 *  EWOULDBLOCK if the socket is in non-blocking mode and there's no
 *		xpd data in the buffer
 *  E* whatever is returned from the fsm.
 */
int
tp_rcvoob(tpcb, so, m, outflags, inflags)
	struct tp_pcb  *tpcb;
	struct socket *so;
	struct mbuf *m;
	int            *outflags;
	int             inflags;
{
	struct mbuf *n;
	struct sockbuf *sb = &so->so_rcv;
	struct tp_event E;
	int             error = 0;
	struct mbuf **nn;

#ifdef ARGO_DEBUG
	if (argo_debug[D_XPD]) {
		printf("PRU_RCVOOB, sostate 0x%x\n", so->so_state);
	}
#endif

	/* if you use soreceive */
	if (m == MNULL)
		return ENOBUFS;

restart:
	if ((((so->so_state & SS_ISCONNECTED) == 0)
	     || (so->so_state & SS_ISDISCONNECTING) != 0) &&
	    (so->so_proto->pr_flags & PR_CONNREQUIRED)) {
		return ENOTCONN;
	}
	/*
	 * Take the first mbuf off the chain. Each XPD TPDU gives you a
	 * complete TSDU so the chains don't get coalesced, but one TSDU may
	 * span several mbufs. Nevertheless, since n should have a most 16
	 * bytes, it will fit into m.  (size was checked in tp_input() )
	 */

	/*
	 * Code for excision of OOB data should be added to
	 * uipc_socket2.c (like sbappend).
	 */

	sblock(sb, M_WAITOK);
	for (nn = &sb->sb_mb; (n = *nn) != NULL; nn = &n->m_act)
		if (n->m_type == MT_OOBDATA)
			break;

	if (n == 0) {
#ifdef ARGO_DEBUG
		if (argo_debug[D_XPD]) {
			printf("RCVOOB: empty queue!\n");
		}
#endif
		sbunlock(sb);
		if (so->so_state & SS_NBIO) {
			return EWOULDBLOCK;
		}
		sbwait(sb);
		goto restart;
	}
	m->m_len = 0;

	/* Assuming at most one xpd tpdu is in the buffer at once */
	while (n != MNULL) {
		m->m_len += n->m_len;
		bcopy(mtod(n, caddr_t), mtod(m, caddr_t), (unsigned) n->m_len);
		m->m_data += n->m_len;	/* so mtod() in bcopy() above gives
					 * right addr */
		n = n->m_next;
	}
	m->m_data = m->m_dat;
	m->m_flags |= M_EOR;

#ifdef ARGO_DEBUG
	if (argo_debug[D_XPD]) {
		printf("tp_rcvoob: xpdlen 0x%x\n", m->m_len);
		dump_mbuf(so->so_rcv.sb_mb, "RCVOOB: Rcv socketbuf");
		dump_mbuf(sb->sb_mb, "RCVOOB: Xrcv socketbuf");
	}
#endif

	if ((inflags & MSG_PEEK) == 0) {
		n = *nn;
		*nn = n->m_act;
		for (; n; n = m_free(n))
			sbfree(sb, n);
	}
	sbunlock(sb);

#ifdef TPPT
	if (tp_traceflags[D_XPD]) {
		tptraceTPCB(TPPTmisc, "PRU_RCVOOB @ release sb_cc m_len",
			    tpcb->tp_Xrcv.sb_cc, m->m_len, 0, 0);
	}
#endif
	if (error == 0)
		error = DoEvent(T_USR_Xrcvd);
	return error;
}

/*
 * CALLED FROM:
 *  tp_usrreq(), PRU_SENDOOB
 * FUNCTION and ARGUMENTS:
 * 	Send what's in the mbuf chain (m) as an XPD TPDU.
 * 	The mbuf may not contain more than 16 bytes of data.
 * 	XPD TSDUs aren't segmented, so they translate into
 * 	exactly one XPD TPDU, with EOT bit set.
 * RETURN VALUE:
 *  EWOULDBLOCK if socket is in non-blocking mode and the previous
 *   xpd data haven't been acked yet.
 *  EMSGSIZE if trying to send > max-xpd bytes (16)
 *  ENOBUFS if ran out of mbufs
 */
int
tp_sendoob(tpcb, so, xdata, outflags)
	struct tp_pcb  *tpcb;
	struct socket *so;
	struct mbuf *xdata;
	int            *outflags;	/* not used */
{
	/*
	 * Each mbuf chain represents a sequence # in the XPD seq space.
	 * The first one in the queue has sequence # tp_Xuna.
	 * When we add to the XPD queue, we stuff a zero-length
	 * mbuf (mark) into the DATA queue, with its sequence number in m_next
	 * to be assigned to this XPD tpdu, so data xfer can stop
	 * when it reaches the zero-length mbuf if this XPD TPDU hasn't
	 * yet been acknowledged.
	 */
	struct sockbuf *sb = &(tpcb->tp_Xsnd);
	struct mbuf *xmark;
	int    len = 0;
	struct tp_event E;

#ifdef ARGO_DEBUG
	if (argo_debug[D_XPD]) {
		printf("tp_sendoob:");
		if (xdata)
			printf("xdata len 0x%x\n", xdata->m_len);
	}
#endif
	/*
	 * DO NOT LOCK the Xsnd buffer!!!! You can have at MOST one socket
	 * buf locked at any time!!! (otherwise you might sleep() in sblock()
	 * w/ a signal pending and cause the system call to be aborted w/ a
	 * locked socketbuf, which is a problem.  So the so_snd buffer lock
	 * (done in sosend()) serves as the lock for Xpd.
	 */
	if (sb->sb_mb) {	/* Anything already in eXpedited data
				 * sockbuf? */
		if (so->so_state & SS_NBIO) {
			return EWOULDBLOCK;
		}
		while (sb->sb_mb) {
			sbunlock(&so->so_snd);	/* already locked by sosend */
			sbwait(&so->so_snd);
			sblock(&so->so_snd, M_WAITOK);	/* sosend will unlock on
							 * return */
		}
	}
	if (xdata == (struct mbuf *) 0) {
		/* empty xpd packet */
		MGETHDR(xdata, M_WAIT, MT_OOBDATA);
		xdata->m_len = 0;
		xdata->m_pkthdr.len = 0;
	}
#ifdef ARGO_DEBUG
	if (argo_debug[D_XPD]) {
		printf("tp_sendoob 1:");
		if (xdata)
			printf("xdata len 0x%x\n", xdata->m_len);
	}
#endif
	xmark = xdata;		/* temporary use of variable xmark */
	while (xmark) {
		len += xmark->m_len;
		xmark = xmark->m_next;
	}
	if (len > TP_MAX_XPD_DATA) {
		return EMSGSIZE;
	}
#ifdef ARGO_DEBUG
	if (argo_debug[D_XPD]) {
		printf("tp_sendoob 2:");
		if (xdata)
			printf("xdata len 0x%x\n", len);
	}
#endif


#ifdef TPPT
	if (tp_traceflags[D_XPD]) {
		tptraceTPCB(TPPTmisc, "XPD mark m_next ", xdata->m_next, 0, 0, 0);
	}
#endif

	sbappendrecord(sb, xdata);

#ifdef ARGO_DEBUG
	if (argo_debug[D_XPD]) {
		printf("tp_sendoob len 0x%x\n", len);
		dump_mbuf(so->so_snd.sb_mb, "XPD request Regular sndbuf:");
		dump_mbuf(tpcb->tp_Xsnd.sb_mb, "XPD request Xsndbuf:");
	}
#endif
	return DoEvent(T_XPD_req);
}

/*
 * CALLED FROM:
 *  the socket routines
 * FUNCTION and ARGUMENTS:
 * 	Handles all "user requests" except the [gs]ockopts() requests.
 * 	The argument (req) is the request type (PRU*),
 * 	(m) is an mbuf chain, generally used for send and
 * 	receive type requests only.
 * 	(nam) is used for addresses usually, in particular for the bind request.
 *
 */
/* ARGSUSED */
int
tp_usrreq(so, req, m, nam, controlp)
	struct socket  *so;
	int             req;
	struct mbuf    *m, *nam, *controlp;
{
	struct tp_pcb *tpcb = sototpcb(so);
	int             s = splsoftnet();
	int             error = 0;
	int             flags, *outflags = &flags;
	u_long          eotsdu = 0;
	struct tp_event E;

#ifdef ARGO_DEBUG
	if (argo_debug[D_REQUEST]) {
		printf("usrreq(%p,%d,%p,%p,%p)\n", so, req, m, nam, outflags);
		if (so->so_error)
			printf("WARNING!!! so->so_error is 0x%x\n", so->so_error);
	}
#endif
#ifdef TPPT
	if (tp_traceflags[D_REQUEST]) {
		tptraceTPCB(TPPTusrreq, "req so m state [", req, so, m,
			    tpcb ? tpcb->tp_state : 0);
	}
#endif

	if ((u_long) tpcb == 0 && req != PRU_ATTACH) {
#ifdef TPPT
		if (tp_traceflags[D_REQUEST]) {
			tptraceTPCB(TPPTusrreq, "req failed NO TPCB[", 0, 0, 0, 0);
		}
#endif
		splx(s);
		return ENOTCONN;
	}
	switch (req) {

	case PRU_ATTACH:
		if (tpcb) {
			error = EISCONN;
		} else if ((error = tp_attach(so, (long) nam)) == 0)
			tpcb = sototpcb(so);
		break;

	case PRU_ABORT:	/* called from close() */
		/*
		 * called for each incoming connect queued on the parent
		 * (accepting) socket
		 */
		if (tpcb->tp_state == TP_OPEN || tpcb->tp_state == TP_CONFIRMING) {
			E.TPDU_ATTR(REQ).e_reason = E_TP_NO_SESSION ^ TP_ERROR_MASK;
			error = DoEvent(T_DISC_req);	/* pretend it was a
							 * close() */
			break;
		}		/* else DROP THROUGH */
	case PRU_DETACH:	/* called from close() */
		/* called only after disconnect was called */
		error = DoEvent(T_DETACH);
		if (tpcb->tp_state == TP_CLOSED) {
			if (tpcb->tp_notdetached) {
#ifdef ARGO_DEBUG
				if (argo_debug[D_CONN]) {
					printf("PRU_DETACH: not detached\n");
				}
#endif
				tp_detach(tpcb);
			}
			free((caddr_t) tpcb, M_PCB);
			tpcb = 0;
		}
		break;

	case PRU_SHUTDOWN:
		/*
		 * recv end may have been released; local credit might be
		 * zero
		 */
	case PRU_DISCONNECT:
		E.TPDU_ATTR(REQ).e_reason = E_TP_NORMAL_DISC ^ TP_ERROR_MASK;
		error = DoEvent(T_DISC_req);
		break;

	case PRU_BIND:
		error = tp_pcbbind(tpcb, nam);
		break;

	case PRU_LISTEN:
		if (tpcb->tp_state != TP_CLOSED || tpcb->tp_lsuffixlen == 0 ||
		    tpcb->tp_next == 0)
			error = EINVAL;
		else {
			struct tp_pcb **tt;
			remque(tpcb);
			tpcb->tp_next = tpcb->tp_prev = tpcb;
			for (tt = &tp_listeners; *tt; tt = &((*tt)->tp_nextlisten))
				if ((*tt)->tp_lsuffixlen)
					break;
			tpcb->tp_nextlisten = *tt;
			*tt = tpcb;
			error = DoEvent(T_LISTEN_req);
		}
		break;

	case PRU_CONNECT2:
		error = EOPNOTSUPP;	/* for unix domain sockets */
		break;

	case PRU_CONNECT:
#ifdef TPPT
		if (tp_traceflags[D_CONN]) {
			tptraceTPCB(TPPTmisc,
				    "PRU_CONNECT: so 0x%x *SHORT_LSUFXP(tpcb) 0x%x lsuflen 0x%x, class 0x%x",
		    tpcb->tp_sock, *SHORT_LSUFXP(tpcb), tpcb->tp_lsuffixlen,
				    tpcb->tp_class);
		}
#endif
#ifdef ARGO_DEBUG
		if (argo_debug[D_CONN]) {
			printf("PRU_CONNECT: so %p *SHORT_LSUFXP(tpcb) 0x%x lsuflen 0x%x, class 0x%x",
			       tpcb->tp_sock, *SHORT_LSUFXP(tpcb), tpcb->tp_lsuffixlen,
			       tpcb->tp_class);
		}
#endif
		if (tpcb->tp_lsuffixlen == 0) {
			if ((error = tp_pcbbind(tpcb, MNULL)) != 0) {
#ifdef ARGO_DEBUG
				if (argo_debug[D_CONN]) {
					printf("pcbbind returns error 0x%x\n", error);
				}
#endif
				break;
			}
		}
#ifdef ARGO_DEBUG
		if (argo_debug[D_CONN]) {
			printf("isop %p isop->isop_socket offset 12 :\n", tpcb->tp_npcb);
			dump_buf(tpcb->tp_npcb, 16);
		}
#endif
		if ((error = tp_route_to(nam, tpcb, /* channel */ 0)) != 0)
			break;
#ifdef ARGO_DEBUG
		if (argo_debug[D_CONN]) {
			printf(
			       "PRU_CONNECT after tpcb %p so %p npcb %p flags 0x%x\n",
			       tpcb, so, tpcb->tp_npcb, tpcb->tp_flags);
			printf("isop %p isop->isop_socket offset 12 :\n", tpcb->tp_npcb);
			dump_buf(tpcb->tp_npcb, 16);
		}
#endif
		if (tpcb->tp_fsuffixlen == 0) {
			/* didn't set peer extended suffix */
			(tpcb->tp_nlproto->nlp_getsufx) (tpcb->tp_npcb,
					      &tpcb->tp_fsuffixlen,
					      tpcb->tp_fsuffix, TP_FOREIGN);
		}
		if (tpcb->tp_state == TP_CLOSED) {
			soisconnecting(so);
			error = DoEvent(T_CONN_req);
		} else {
			(tpcb->tp_nlproto->nlp_pcbdisc) (tpcb->tp_npcb);
			error = EISCONN;
		}
#ifdef TP_PERF_MEAS
		if (DOPERF(tpcb)) {
			u_int           lsufx, fsufx;
			lsufx = *(u_short *) (tpcb->tp_lsuffix);
			fsufx = *(u_short *) (tpcb->tp_fsuffix);

			tpmeas(tpcb->tp_lref,
			       TPtime_open | (tpcb->tp_xtd_format << 4),
			       &time, lsufx, fsufx, tpcb->tp_fref);
		}
#endif
		break;

	case PRU_ACCEPT:
		(tpcb->tp_nlproto->nlp_getnetaddr) (tpcb->tp_npcb, nam, TP_FOREIGN);
#ifdef ARGO_DEBUG
		if (argo_debug[D_REQUEST]) {
			printf("ACCEPT PEERADDDR:");
			dump_buf(mtod(nam, char *), nam->m_len);
		}
#endif
#ifdef TP_PERF_MEAS
		if (DOPERF(tpcb)) {
			u_int           lsufx, fsufx;
			lsufx = *(u_short *) (tpcb->tp_lsuffix);
			fsufx = *(u_short *) (tpcb->tp_fsuffix);

			tpmeas(tpcb->tp_lref, TPtime_open,
			       &time, lsufx, fsufx, tpcb->tp_fref);
		}
#endif
		break;

	case PRU_RCVD:
		if (so->so_state & SS_ISCONFIRMING) {
			if (tpcb->tp_state == TP_CONFIRMING)
				error = tp_confirm(tpcb);
			break;
		}
#ifdef TPPT
		if (tp_traceflags[D_DATA]) {
			tptraceTPCB(TPPTmisc,
				    "RCVD BF: lcredit sent_lcdt cc hiwat \n",
				    tpcb->tp_lcredit, tpcb->tp_sent_lcdt,
				    so->so_rcv.sb_cc, so->so_rcv.sb_hiwat);
			LOCAL_CREDIT(tpcb);
			tptraceTPCB(TPPTmisc,
				    "PRU_RCVD AF sbspace lcredit hiwat cc",
				    sbspace(&so->so_rcv), tpcb->tp_lcredit,
				    so->so_rcv.sb_cc, so->so_rcv.sb_hiwat);
		}
#endif
#ifdef ARGO_DEBUG
		if (argo_debug[D_REQUEST]) {
			printf("RCVD: cc %ld space %ld hiwat %ld\n",
			       so->so_rcv.sb_cc, sbspace(&so->so_rcv),
			       so->so_rcv.sb_hiwat);
		}
#endif
		if (((long) nam) & MSG_OOB)
			error = DoEvent(T_USR_Xrcvd);
		else
			error = DoEvent(T_USR_rcvd);
		break;

	case PRU_RCVOOB:
		if ((so->so_state & SS_ISCONNECTED) == 0) {
			error = ENOTCONN;
			break;
		}
		if (!tpcb->tp_xpd_service) {
			error = EOPNOTSUPP;
			break;
		}
		/* kludge - nam is really flags here */
		error = tp_rcvoob(tpcb, so, m, outflags, (long) nam);
		break;

	case PRU_SEND:
	case PRU_SENDOOB:
		if (controlp) {
			error = tp_snd_control(controlp, so, &m);
			controlp = NULL;
			if (error)
				break;
		}
		if ((so->so_state & SS_ISCONFIRMING) &&
		    (tpcb->tp_state == TP_CONFIRMING) &&
		    (error = tp_confirm(tpcb)))
			break;
		if (req == PRU_SENDOOB) {
			error = (tpcb->tp_xpd_service == 0) ?
				EOPNOTSUPP : tp_sendoob(tpcb, so, m, outflags);
			break;
		}
		if (m == 0)
			break;
		if (m->m_flags & M_EOR) {
			eotsdu = 1;
			m->m_flags &= ~M_EOR;
		}
		if (eotsdu == 0 && m->m_pkthdr.len == 0)
			break;
		if (tpcb->tp_state != TP_AKWAIT && tpcb->tp_state != TP_OPEN) {
			error = ENOTCONN;
			break;
		}
		/*
		 * The protocol machine copies mbuf chains,
		 * prepends headers, assigns seq numbers, and
		 * puts the packets on the device.
		 * When they are acked they are removed from the socket buf.
		 *
		 * sosend calls this up until sbspace goes negative.
		 * Sbspace may be made negative by appending this mbuf chain,
		 * possibly by a whole cluster.
		 */
		{
			/*
			 * Could have eotsdu and no data.(presently MUST have
			 * an mbuf though, even if its length == 0)
			 */
			int             totlen = m->m_pkthdr.len;
			struct sockbuf *sb = &so->so_snd;
#ifdef TP_PERF_MEAS
			if (DOPERF(tpcb)) {
				PStat(tpcb, Nb_from_sess) += totlen;
				tpmeas(tpcb->tp_lref, TPtime_from_session, 0, 0,
				       PStat(tpcb, Nb_from_sess), totlen);
			}
#endif
#ifdef ARGO_DEBUG
			if (argo_debug[D_SYSCALL]) {
				printf(
				       "PRU_SEND: eot %ld before sbappend %p len 0x%x to sb @ %p\n",
				       eotsdu, m, totlen, sb);
				dump_mbuf(sb->sb_mb, "so_snd.sb_mb");
				dump_mbuf(m, "m : to be added");
			}
#endif
			tp_packetize(tpcb, m, eotsdu);
#ifdef ARGO_DEBUG
			if (argo_debug[D_SYSCALL]) {
				printf("PRU_SEND: eot %ld after sbappend %p\n", eotsdu, m);
				dump_mbuf(sb->sb_mb, "so_snd.sb_mb");
			}
#endif
			if (tpcb->tp_state == TP_OPEN)
				error = DoEvent(T_DATA_req);
#ifdef ARGO_DEBUG
			if (argo_debug[D_SYSCALL]) {
				printf("PRU_SEND: after driver error 0x%x \n", error);
				printf("so_snd %p cc 0t%ld mbcnt 0t%ld\n",
				       sb, sb->sb_cc, sb->sb_mbcnt);
				dump_mbuf(sb->sb_mb, "so_snd.sb_mb after driver");
			}
#endif
		}
		break;

	case PRU_SOCKADDR:
		(tpcb->tp_nlproto->nlp_getnetaddr) (tpcb->tp_npcb, nam, TP_LOCAL);
		break;

	case PRU_PEERADDR:
		(tpcb->tp_nlproto->nlp_getnetaddr) (tpcb->tp_npcb, nam, TP_FOREIGN);
		break;

	case PRU_CONTROL:
		error = EOPNOTSUPP;
		break;

	case PRU_PROTOSEND:
	case PRU_PROTORCV:
	case PRU_SENSE:
	case PRU_SLOWTIMO:
	case PRU_FASTTIMO:
		error = EOPNOTSUPP;
		break;

	default:
#ifdef ARGO_DEBUG
		printf("tp_usrreq UNKNOWN PRU %d\n", req);
#endif				/* ARGO_DEBUG */
		error = EOPNOTSUPP;
	}

#ifdef ARGO_DEBUG
	if (argo_debug[D_REQUEST]) {
		printf("%s, so %p, tpcb %p, error %d, state %d\n",
		       "returning from tp_usrreq", so, tpcb, error,
		       tpcb ? tpcb->tp_state : 0);
	}
#endif
#ifdef TPPT
	if (tp_traceflags[D_REQUEST]) {
		tptraceTPCB(TPPTusrreq, "END req so m state [", req, so, m,
			    tpcb ? tpcb->tp_state : 0);
	}
#endif
	if (controlp) {
		m_freem(controlp);
		printf("control data unexpectedly retained in tp_usrreq()");
	}
	splx(s);
	return error;
}

void
tp_ltrace(so, uio)
	struct socket  *so;
	struct uio     *uio;
{
#ifdef TPPT
	if (tp_traceflags[D_DATA]) {
		struct tp_pcb *tpcb = sototpcb(so);
		if (tpcb) {
			tptraceTPCB(TPPTmisc, "sosend so resid iovcnt", so,
				    uio->uio_resid, uio->uio_iovcnt, 0);
		}
	}
#endif
}

int
tp_confirm(tpcb)
	struct tp_pcb *tpcb;
{
	struct tp_event E;
	if (tpcb->tp_state == TP_CONFIRMING)
		return DoEvent(T_ACPT_req);
	printf("Tp confirm called when not confirming; tpcb %p, state 0x%x\n",
	       tpcb, tpcb->tp_state);
	return 0;
}

/*
 * Process control data sent with sendmsg()
 */
int
tp_snd_control(m, so, data)
	struct mbuf    *m;
	struct socket  *so;
	struct mbuf **data;
{
	struct cmsghdr *ch;
	int             error = 0;

	if (m && m->m_len) {
		ch = mtod(m, struct cmsghdr *);
		m->m_len -= sizeof(*ch);
		m->m_data += sizeof(*ch);
		error = tp_ctloutput(PRCO_SETOPT,
				     so, ch->cmsg_level, ch->cmsg_type, &m);
		if (ch->cmsg_type == TPOPT_DISC_DATA) {
			if (data && *data) {
				m_freem(*data);
				*data = 0;
			}
			error = tp_usrreq(so, PRU_DISCONNECT, NULL,
					  NULL, NULL);
		}
	}
	if (m)
		m_freem(m);
	return error;
}
