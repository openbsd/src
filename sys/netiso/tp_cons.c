/*	$OpenBSD: tp_cons.c,v 1.11 2004/01/03 14:08:54 espie Exp $	*/
/*	$NetBSD: tp_cons.c,v 1.8 1996/02/14 21:32:37 christos Exp $	*/

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
 *	@(#)tp_cons.c	8.1 (Berkeley) 6/10/93
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
 * Here is where you find the iso- and cons-dependent code.  We've tried keep
 * all net-level and (primarily) address-family-dependent stuff out of the tp
 * source, and everthing here is reached indirectly through a switch table
 * (struct nl_protosw *) tpcb->tp_nlproto (see tp_pcb.c). The routines here
 * are: tpcons_input: pullup and call tp_input w/ correct arguments
 * tpcons_output: package a pkt for cons given an isopcb & some data
 * cons_chan_to_tpcb: find a tpcb based on the channel #
 */

#ifdef ISO
#ifdef TPCONS

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/route.h>

#include <netiso/tp_param.h>
#include <netiso/argo_debug.h>
#include <netiso/tp_stat.h>
#include <netiso/tp_pcb.h>
#include <netiso/tp_trace.h>
#include <netiso/tp_stat.h>
#include <netiso/tp_tpdu.h>
#include <netiso/iso.h>
#include <netiso/iso_errno.h>
#include <netiso/iso_pcb.h>
#include <netiso/iso_var.h>
#include <netiso/cons.h>
#include <netiso/tp_seq.h>
#include <netiso/tp_var.h>
#include <netiso/clnp.h>

#undef FALSE
#undef TRUE
#include <netccitt/x25.h>
#include <netccitt/pk.h>
#include <netccitt/pk_var.h>
#include <netccitt/pk_extern.h>

#include <sys/stdarg.h>

#include <netiso/if_cons.c>


/*
 * CALLED FROM:
 *  tp_route_to() for PRU_CONNECT
 * FUNCTION, ARGUMENTS, SIDE EFFECTS and RETURN VALUE:
 *  version of the previous procedure for X.25
 */

int
tpcons_pcbconnect(v, nam)
	void *v;
	struct mbuf *nam;
{
	struct isopcb  *isop = v;
	int             error;
	if ((error = iso_pcbconnect(isop, nam)) != 0)
		return error;
	if ((isop->isop_chan = (caddr_t) pk_attach((struct socket *) 0)) == 0) {
#ifdef ARGO_DEBUG
		if (argo_debug[D_CCONS]) {
			printf("tpcons_pcbconnect: no pklcd; returns 0x%x\n", error);
		}
#endif
		return ENOBUFS;
	}
	if ((error = cons_connect(isop)) != 0) {	/* if it doesn't work */
		/* oh, dear, throw packet away */
		pk_disconnect((struct pklcd *) isop->isop_chan);
		isop->isop_chan = 0;
	} else
		isop->isop_refcnt = 1;
	return error;
}


/*
 * CALLED FROM:
 * 	cons
 * FUNCTION and ARGUMENTS:
 * THIS MAYBE BELONGS IN SOME OTHER PLACE??? but i think not -
 */
void *
tpcons_ctlinput(cmd, siso, v)
	int             cmd;
	struct sockaddr *siso;
	void *v;
{
	struct isopcb  *isop = v;
	struct tp_pcb *tpcb = 0;

	if (isop->isop_socket)
		tpcb = (struct tp_pcb *) isop->isop_socket->so_pcb;
	switch (cmd) {

	case PRC_CONS_SEND_DONE:
		if (tpcb) {
			struct tp_event E;
			int             error = 0;

			if (tpcb->tp_class == TP_CLASS_0) {
				/*
				 * only if class is exactly class zero, not
				 * still in class negotiation
				 */
				/* fake an ack */
				SeqNum seq = SEQ_ADD(tpcb, tpcb->tp_snduna, 1);

#ifdef TPPT
				if(tp_traceflags[D_DATA])
					tptrace(TPPTmisc, "FAKE ACK seq cdt 1",
						seq, 0, 0, 0);
#endif
#ifdef ARGO_DEBUG
					if (argo_debug[D_DATA]) {
					printf("FAKE ACK seq 0x%x cdt 1\n", seq);
				}
#endif
				E.TP_ATTR(AK_TPDU).e_cdt = 1;
				E.TP_ATTR(AK_TPDU).e_seq = seq;
				E.TP_ATTR(AK_TPDU).e_subseq = 0;
				E.TP_ATTR(AK_TPDU).e_fcc_present = 0;
				error = DoEvent(AK_TPDU);
				if (error) {
					tpcb->tp_sock->so_error = error;
				}
			}	/* else ignore it */
		}
		break;
	case PRC_ROUTEDEAD:
		if (tpcb && tpcb->tp_class == TP_CLASS_0) {
			tpiso_reset(isop);
			break;
		}		/* else drop through */
	default:
		tpclnp_ctlinput(cmd, siso, NULL);
		break;
	}
	return NULL;
}

/*
 * CALLED FROM:
 * 	cons's intr routine
 * FUNCTION and ARGUMENTS:
 * Take a packet (m) from cons, pullup m as required by tp,
 *  ignore the socket argument, and call tp_input.
 * No return value.
 */
void
tpcons_input(struct mbuf *m, ...)
{
	struct sockaddr *faddr, *laddr;
	caddr_t         channel;
	va_list ap;
	if (m == NULL)
		return;
	va_start(ap, m);
	faddr = va_arg(ap, struct sockaddr *);
	laddr = va_arg(ap, struct sockaddr *);
	channel = va_arg(ap, caddr_t);
	va_end(ap);

	m = (struct mbuf *) tp_inputprep(m);

#ifdef ARGO_DEBUG
	if (argo_debug[D_TPINPUT]) {
		printf("tpcons_input before tp_input(m 0x%x)\n", m);
		dump_buf(m, 12 + m->m_len);
	}
#endif
	tp_input(m, faddr, laddr, channel, tpcons_output, 0);
}


/*
 * CALLED FROM:
 *  tp_emit()
 * FUNCTION and ARGUMENTS:
 *  Take a packet(m0) from tp and package it so that cons will accept it.
 *  This means filling in a few of the fields.
 *  inp is the isopcb structure; datalen is the length of the data in the
 *  mbuf string m0.
 * RETURN VALUE:
 *  whatever (E*) is returned form the net layer output routine.
 */

int
tpcons_output(struct mbuf *m0, ...)
{
	struct isopcb  *isop;
	int             datalen;
	int             nochksum;
	struct mbuf *m = m0;
	int             error;
	va_list		ap;

	va_start(ap, m0);
	datalen = va_arg(ap, int);
	isop = va_arg(ap, struct isopcb *);
	nochksum = va_arg(ap, int);
	va_end(ap);

#ifdef ARGO_DEBUG
	if (argo_debug[D_EMIT]) {
		printf(
		  "tpcons_output(isop 0x%x, m 0x%x, len 0x%x socket 0x%x\n",
		       isop, m0, datalen, isop->isop_socket);
	}
#endif
	if (m == NULL)
		return 0;
	if ((m->m_flags & M_PKTHDR) == 0) {
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == 0)
			return ENOBUFS;
		M_MOVE_HDR(m, m0);
		m->m_next = m0;
	}
	m->m_pkthdr.len = datalen;
	if (isop->isop_chan == 0) {
		/* got a restart maybe? */
		if ((isop->isop_chan = (caddr_t) pk_attach((struct socket *) 0)) == 0) {
#ifdef ARGO_DEBUG
			if (argo_debug[D_CCONS]) {
				printf("tpcons_output: no pklcd\n");
			}
#endif
			error = ENOBUFS;
		}
		if ((error = cons_connect(isop)) != 0) {
			pk_disconnect((struct pklcd *) isop->isop_chan);
			isop->isop_chan = 0;
#ifdef ARGO_DEBUG
			if (argo_debug[D_CCONS]) {
				printf("tpcons_output: can't reconnect\n");
			}
#endif
		}
	} else {
		error = pk_send(m, isop->isop_chan);
		IncStat(ts_tpdu_sent);
	}
	return error;
}
/*
 * CALLED FROM:
 *  tp_error_emit()
 * FUNCTION and ARGUMENTS:
 *  Take a packet(m0) from tp and package it so that cons will accept it.
 *  chan is the cons channel to use; datalen is the length of the data in the
 *  mbuf string m0.
 * RETURN VALUE:
 *  whatever (E*) is returned form the net layer output routine.
 */

int
tpcons_output_dg(struct mbuf *m0, ...)
{
	int             datalen;
	caddr_t         chan;
	va_list		ap;

	va_start(ap, m0);
	datalen = va_arg(ap, int);
	chan = va_arg(ap, caddr_t);
	va_end(ap);

	return tpcons_output(m0, datalen,
			     ((struct pklcd *) chan)->lcd_upnext, 
			     0);
}
#else

#include <sys/param.h>

struct mbuf;

int tpcons_output(struct mbuf *m0, ...);

int
tpcons_output(struct mbuf *m0, ...)
{
	return 0;
}
#endif /* TPCONS */
#endif /* ISO */
