/*	$OpenBSD: tp_input.c,v 1.8 2004/01/03 14:08:54 espie Exp $	*/
/*	$NetBSD: tp_input.c,v 1.9 1996/03/16 23:13:51 christos Exp $	*/

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
 *	@(#)tp_input.c	8.1 (Berkeley) 6/10/93
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
 * tp_input() gets an mbuf chain from ip.  Actually, not directly from ip,
 * because ip calls a net-level routine that strips off the net header and
 * then calls tp_input(), passing the proper type of addresses for the
 * address family in use (how it figures out which AF is not yet determined.)
 *
 * Decomposing the tpdu is some of the most laughable code.  The variable-length
 * parameters and the problem of non-aligned memory references necessitates
 * such abominations as the macros WHILE_OPTIONS (q.v. below) to loop through
 * the header and decompose it.
 *
 * The routine tp_newsocket() is called when a CR comes in for a listening
 * socket.  tp_input calls sonewconn() and tp_newsocket() to set up the
 * "child" socket.  Most tpcb values are copied from the parent tpcb into the
 * child.
 *
 * Also in here is tp_headersize() (grot) which tells the expected size of a tp
 * header, to be used by other layers.  It's in here because it uses the
 * static structure tpdu_info.
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
#include <sys/kernel.h>

#include <net/if.h>

#include <netiso/iso.h>
#include <netiso/iso_errno.h>
#include <netiso/iso_pcb.h>
#include <netiso/tp_param.h>
#include <netiso/tp_timer.h>
#include <netiso/tp_stat.h>
#include <netiso/tp_pcb.h>
#include <netiso/argo_debug.h>
#include <netiso/tp_trace.h>
#include <netiso/tp_tpdu.h>
#include <netiso/tp_var.h>
#include <netiso/iso_var.h>

#ifdef TRUE
#undef FALSE
#undef TRUE
#endif
#include <netccitt/x25.h>
#include <netccitt/pk.h>
#include <netccitt/pk_var.h>

#include <sys/stdarg.h>

static struct socket *tp_newsocket(struct socket *, struct sockaddr *,
					caddr_t, u_int, u_int);

struct mbuf    *
tp_inputprep(m)
	struct mbuf *m;
{
	int             hdrlen;

#ifdef ARGO_DEBUG
	if (argo_debug[D_TPINPUT]) {
		printf("tp_inputprep: m %p\n", m);
	}
#endif

	while (m->m_len < 1) {
		/*
		 * The "m_free" logic if( (m = m_free(m)) == MNULL ) return
		 * (struct mbuf *)0; would cause a system crash if ever
		 * executed. This logic will be executed if the first mbuf in
		 * the chain only contains a CLNP header. The m_free routine
		 * will release the mbuf containing the CLNP header from the
		 * chain and the new head of the chain will not have the
		 * M_PKTHDR bit set. This routine, tp_inputprep, will
		 * eventually call the "sbappendaddr" routine. "sbappendaddr"
		 * calls "panic" if M_PKTHDR is not set. m_pullup is a cheap
		 * way of keeping the head of the chain from being freed.
		 */
		if ((m = m_pullup(m, 1)) == MNULL)
			return (MNULL);
	}
	if (((long) m->m_data) & 0x3) {
		/*
		 * If we are not 4-byte aligned, we have to be above the
		 * beginning of the mbuf, and it is ok just to slide it back.
		 */
		caddr_t         ocp = m->m_data;

		m->m_data = (caddr_t) (((long) m->m_data) & ~0x3);
		bcopy(ocp, m->m_data, (unsigned) m->m_len);
	}
	CHANGE_MTYPE(m, TPMT_DATA);

	/*
	 * we KNOW that there is at least 1 byte in this mbuf and that it is
	 * hdr->tpdu_li XXXXXXX!
	 */

	hdrlen = 1 + *mtod(m, u_char *);

	/*
	 * now pull up the whole tp header
	 */
	if (m->m_len < hdrlen) {
		if ((m = m_pullup(m, hdrlen)) == MNULL) {
			IncStat(ts_recv_drop);
			return (struct mbuf *) 0;
		}
	}
#ifdef ARGO_DEBUG
	if (argo_debug[D_INPUT]) {
		printf(
		       " at end: m %p hdr->tpdu_li 0x%x m_len 0x%x\n", m,
		       hdrlen, m->m_len);
	}
#endif
	return m;
}

/*
 * begin groan -- this array and the following macros allow you to step
 * through the parameters of the variable part of a header note that if for
 * any reason the values of the **_TPDU macros (in tp_events.h) should
 * change, this array has to be rearranged
 */

#define TP_LEN_CLASS_0_INDEX	2
#define TP_MAX_DATA_INDEX 3

static u_char   tpdu_info[][4] =
{
	/* length						 max data len */
	/* reg fmt 	xtd fmt  class 0  		 	  */
	/* UNUSED		0x0 */ { 0x0, 0x0, 0x0, 0x0		},
	/* XPD_TPDU_type	0x1 */ { 0x5, 0x8, 0x0, TP_MAX_XPD_DATA	},
	/* XAK_TPDU_type	0x2 */ { 0x5, 0x8, 0x0, 0x0		},
	/* GR_TPDU_type		0x3 */ { 0x0, 0x0, 0x0, 0x0		},
	/* UNUSED		0x4 */ { 0x0, 0x0, 0x0, 0x0		},
	/* UNUSED		0x5 */ { 0x0, 0x0, 0x0, 0x0		},
	/* AK_TPDU_type		0x6 */ { 0x5, 0xa, 0x0, 0x0		},
	/* ER_TPDU_type		0x7 */ { 0x5, 0x5, 0x0, 0x0		},
	/* DR_TPDU_type		0x8 */ { 0x7, 0x7, 0x7, TP_MAX_DR_DATA	},
	/* UNUSED		0x9 */ { 0x0, 0x0, 0x0, 0x0		},
	/* UNUSED		0xa */ { 0x0, 0x0, 0x0, 0x0		},
	/* UNUSED		0xb */ { 0x0, 0x0, 0x0, 0x0		},
	/* DC_TPDU_type		0xc */ { 0x6, 0x6, 0x0, 0x0		},
	/* CC_TPDU_type		0xd */ { 0x7, 0x7, 0x7, TP_MAX_CC_DATA	},
	/* CR_TPDU_type		0xe */ { 0x7, 0x7, 0x7, TP_MAX_CR_DATA	},
	/* DT_TPDU_type		0xf */ { 0x5, 0x8, 0x3, 0x0		},
};

#define CHECK(Phrase, Erval, Stat, Whattodo, Loc)\
	if (Phrase) {error = (Erval); errlen = (int)(Loc); IncStat(Stat);\
	goto Whattodo; }

/*
 * WHENEVER YOU USE THE FOLLOWING MACRO, BE SURE THE TPDUTYPE IS A LEGIT
 * VALUE FIRST!
 */

#define WHILE_OPTIONS(P, hdr, format)\
{	caddr_t P = tpdu_info[(hdr)->tpdu_type][(format)] + (caddr_t)hdr;\
	caddr_t PLIM = 1 + hdr->tpdu_li + (caddr_t)hdr;\
	for (;; P += 2 + ((struct tp_vbp *)P)->tpv_len) {\
		CHECK((P > PLIM), E_TP_LENGTH_INVAL, ts_inv_length,\
				respond, P - (caddr_t)hdr);\
		if (P == PLIM) break;

#define END_WHILE_OPTIONS(P) } }

/* end groan */

/*
 * NAME:  tp_newsocket()
 *
 * CALLED FROM:
 *  tp_input() on incoming CR, when a socket w/ the called suffix
 * is awaiting a  connection request
 *
 * FUNCTION and ARGUMENTS:
 *  Create a new socket structure, attach to it a new transport pcb,
 *  using a copy of the net level pcb for the parent socket.
 *  (so) is the parent socket.
 *  (fname) is the foreign address (all that's used is the nsap portion)
 *
 * RETURN VALUE:
 *  a new socket structure, being this end of the newly formed connection.
 *
 * SIDE EFFECTS:
 *  Sets a few things in the tpcb and net level pcb
 *
 * NOTES:
 */
static struct socket *
tp_newsocket(so, fname, cons_channel, class_to_use, netservice)
	struct socket  *so;
	struct sockaddr *fname;
	caddr_t         cons_channel;
	u_int          class_to_use;
	u_int           netservice;
{
	struct tp_pcb *tpcb = sototpcb(so);	/* old tpcb, needed
							 * below */
	struct tp_pcb *newtpcb;

	/*
	 * sonewconn() gets a new socket structure, a new lower layer pcb and
	 * a new tpcb, but the pcbs are unnamed (not bound)
	 */
#ifdef TPPT
	if (tp_traceflags[D_NEWSOCK]) {
		tptraceTPCB(TPPTmisc, "newsock: listg_so, _tpcb, so_head",
			    so, tpcb, so->so_head, 0);
	}
#endif

	if ((so = sonewconn(so, SS_ISCONFIRMING)) == (struct socket *) 0)
		return so;
#ifdef TPPT
	if (tp_traceflags[D_NEWSOCK]) {
		tptraceTPCB(TPPTmisc, "newsock: after newconn so, so_head",
			    so, so->so_head, 0, 0);
	}
#endif

#ifdef ARGO_DEBUG
	if (argo_debug[D_NEWSOCK]) {
		printf("tp_newsocket(channel %p)  after sonewconn so %p \n",
		       cons_channel, so);
		dump_addr(fname);
		{
			struct socket  *t, *head;

			head = so->so_head;
			t = so;
			printf("so %p so_head %p so_q0 %p, q0len %d\n",
			       t, t->so_head, t->so_q0, t->so_q0len);
			while ((t = t->so_q0) && t != so && t != head)
				printf("so %p so_head %p so_q0 %p, q0len %d\n",
				       t, t->so_head, t->so_q0, t->so_q0len);
		}
	}
#endif

	/*
	 * before we clobber the old tpcb ptr, get these items from the
	 * parent pcb
	 */
	newtpcb = sototpcb(so);
	newtpcb->_tp_param = tpcb->_tp_param;
	newtpcb->tp_flags = tpcb->tp_flags;
	newtpcb->tp_lcredit = tpcb->tp_lcredit;
	newtpcb->tp_l_tpdusize = tpcb->tp_l_tpdusize;
	newtpcb->tp_lsuffixlen = tpcb->tp_lsuffixlen;
	bcopy(tpcb->tp_lsuffix, newtpcb->tp_lsuffix, newtpcb->tp_lsuffixlen);

	if ( /* old */ tpcb->tp_ucddata) {
		/*
		 * These data are the connect- , confirm- or disconnect-
		 * data.
		 */
		struct mbuf    *conndata;

		conndata = m_copy(tpcb->tp_ucddata, 0, (int) M_COPYALL);
#ifdef ARGO_DEBUG
		if (argo_debug[D_CONN]) {
			dump_mbuf(conndata, "conndata after mcopy");
		}
#endif
		newtpcb->tp_ucddata = conndata;
	}
	tpcb = newtpcb;
	tpcb->tp_state = TP_LISTENING;
	tpcb->tp_class = class_to_use;
	tpcb->tp_netservice = netservice;


	ASSERT(fname != 0);	/* just checking */
	if (fname) {
		/*
		 *	tp_route_to takes its address argument in the form of an mbuf.
		 */
		struct mbuf    *m;
		int             err;

		MGET(m, M_DONTWAIT, MT_SONAME);	/* mbuf type used is
						 * confusing */
		if (m) {
			/*
			 * this seems a bit grotesque, but tp_route_to expects
			 * an mbuf * instead of simply a sockaddr; it calls the ll
			 * pcb_connect, which expects the name/addr in an mbuf as well.
			 * sigh.
			 */
			bcopy((caddr_t) fname, mtod(m, caddr_t), fname->sa_len);
			m->m_len = fname->sa_len;

			/*
			 * grot  : have to say the kernel can override params
			 * in the passive open case
			 */
			tpcb->tp_dont_change_params = 0;
			err = tp_route_to(m, tpcb, cons_channel);
			m_free(m);

			if (!err)
				goto ok;
		}
#ifdef ARGO_DEBUG
		if (argo_debug[D_CONN]) {
			printf("tp_route_to FAILED! detaching tpcb %p, so %p\n",
			       tpcb, so);
		}
#endif
		(void) tp_detach(tpcb);
		return 0;
	}
ok:
#ifdef ARGO_DEBUG
	if (argo_debug[D_TPINPUT]) {
		printf("tp_newsocket returning so %p, sototpcb(so) %p\n",
		       so, sototpcb(so));
	}
#endif
	return so;
}

/*
 * NAME: 	tp_input()
 *
 * CALLED FROM: net layer input routine
 *
 * FUNCTION and ARGUMENTS: Process an incoming TPDU (m), finding the associated
 * tpcb if there is one. Create the appropriate type of event and call the
 * driver. (faddr) and (laddr) are the foreign and local addresses.
 *
 * When tp_input() is called we KNOW that the ENTIRE TP HEADER has been
 * m_pullup-ed.
 *
 * RETURN VALUE: Nada
 *
 * SIDE EFFECTS: When using COSNS it may affect the state of the net-level pcb
 *
 * NOTE: The initial value of acktime is 2 so that we will never have a 0 value
 * for tp_peer_acktime.  It gets used in the computation of the
 * retransmission timer value, and so it mustn't be zero. 2 seems like a
 * reasonable minimum.
 */
void
tp_input(struct mbuf *m, ...)
{
	struct sockaddr *faddr, *laddr;	/* NSAP addresses */
	caddr_t         cons_channel;
	int             (*dgout_routine)(struct mbuf *, ...);
	int             ce_bit;
	struct tp_pcb *tpcb;
	struct tpdu *hdr;
	struct socket  *so;
	struct tp_event e;
	int             error;
	unsigned        dutype;
	u_short         dref, sref, acktime, subseq;
	u_char          preferred_class, class_to_use, pdusize;
	u_char          opt, dusize, addlopt, version = 0;
#ifdef TP_PERF_MEAS
	u_char          perf_meas;
#endif				/* TP_PERF_MEAS */
	u_char          fsufxlen, lsufxlen;
	caddr_t         fsufxloc, lsufxloc;
	int             tpdu_len;
	u_int           takes_data;
	u_int           fcc_present;
	int             errlen;
	struct tp_conn_param tpp;
	va_list		ap;

	va_start(ap, m);
	faddr = va_arg(ap, struct sockaddr *);
	laddr = va_arg(ap, struct sockaddr *);
	cons_channel = va_arg(ap, caddr_t);
	/* XXX: Does va_arg does not work for function ptrs */
	dgout_routine = (int (*)(struct mbuf *, ...)) va_arg(ap, void *);
	ce_bit = va_arg(ap, int);
	va_end(ap);

again:
	hdr = mtod(m, struct tpdu *);
	tpcb = 0;
	error = errlen = tpdu_len = 0;
	takes_data = fcc_present = FALSE;
	acktime = 2;
	sref = subseq = 0;
	fsufxloc = lsufxloc = NULL;
	fsufxlen = lsufxlen =
		preferred_class = class_to_use = pdusize = addlopt = 0;
	dusize = TP_DFL_TPDUSIZE;
#ifdef TP_PERF_MEAS
	GET_CUR_TIME(&e.e_time);
	perf_meas = 0;
#endif				/* TP_PERF_MEAS */

#ifdef ARGO_DEBUG
	if (argo_debug[D_TPINPUT]) {
		printf("tp_input(%p, ... %p)\n", m, cons_channel);
	}
#endif


	/*
	 * get the actual tpdu length - necessary for monitoring and for
	 * checksumming
	 *
	 * Also, maybe measure the mbuf chain lengths and sizes.
	 */

	{
		struct mbuf *n = m;
#ifdef ARGO_DEBUG
		int             chain_length = 0;
#endif				/* ARGO_DEBUG */

		for (;;) {
			tpdu_len += n->m_len;
#ifdef ARGO_DEBUG
			if (argo_debug[D_MBUF_MEAS]) {
				if (n->m_flags & M_EXT) {
					IncStat(ts_mb_cluster);
				} else {
					IncStat(ts_mb_small);
				}
				chain_length++;
			}
#endif
			if (n->m_next == MNULL) {
				break;
			}
			n = n->m_next;
		}
#ifdef ARGO_DEBUG
		if (argo_debug[D_MBUF_MEAS]) {
			if (chain_length > 16)
				chain_length = 0;	/* zero used for
							 * anything > 16 */
			tp_stat.ts_mb_len_distr[chain_length]++;
		}
#endif
	}
#ifdef TPPT
	if (tp_traceflags[D_TPINPUT]) {
		tptraceTPCB(TPPTtpduin, hdr->tpdu_type, hdr, hdr->tpdu_li + 1,
			    tpdu_len, 0);
	}
#endif

		dref = ntohs((short) hdr->tpdu_dref);
	sref = ntohs((short) hdr->tpdu_sref);
	dutype = (int) hdr->tpdu_type;

#ifdef ARGO_DEBUG
	if (argo_debug[D_TPINPUT]) {
		printf("input: dutype 0x%x cons_channel %p dref 0x%x\n",
		    dutype, cons_channel, dref);
		printf("input: dref 0x%x sref 0x%x\n", dref, sref);
	}
#endif
#ifdef TPPT
	if (tp_traceflags[D_TPINPUT]) {
		tptrace(TPPTmisc, "channel dutype dref ",
			cons_channel, dutype, dref, 0);
	}
#endif


#ifdef ARGO_DEBUG
	if ((dutype < TP_MIN_TPDUTYPE) || (dutype > TP_MAX_TPDUTYPE)) {
		printf("BAD dutype! 0x%x, channel %p dref 0x%x\n",
		       dutype, cons_channel, dref);
		dump_buf(m, sizeof(struct mbuf));

		IncStat(ts_inv_dutype);
		goto discard;
	}
#endif				/* ARGO_DEBUG */

	CHECK((dutype < TP_MIN_TPDUTYPE || dutype > TP_MAX_TPDUTYPE),
	      E_TP_INV_TPDU, ts_inv_dutype, respond,
	      2);
	/*
	 * unfortunately we can't take the address of the tpdu_type field,
	 * since it's a bit field - so we just use the constant offset 2
	 */

	/*
	 * Now this isn't very neat but since you locate a pcb one way at the
	 * beginning of connection establishment, and by the dref for each
	 * tpdu after that, we have to treat CRs differently
	 */
	if (dutype == CR_TPDU_type) {
		u_char          alt_classes = 0;

		preferred_class = 1 << hdr->tpdu_CRclass;
		opt = hdr->tpdu_CRoptions;

		WHILE_OPTIONS(P, hdr, 1)	/* { */
			switch (vbptr(P)->tpv_code) {

		case TPP_tpdu_size:
			vb_getval(P, u_char, dusize);
#ifdef ARGO_DEBUG
			if (argo_debug[D_TPINPUT]) {
				printf("CR dusize 0x%x\n", dusize);
			}
#endif
			/* COS tests: NBS IA (Dec. 1987) Sec. 4.5.2.1 */
			if (dusize < TP_MIN_TPDUSIZE || dusize > TP_MAX_TPDUSIZE)
				dusize = TP_DFL_TPDUSIZE;
			break;
		case TPP_ptpdu_size:
			switch (vbptr(P)->tpv_len) {
			case 1:
				pdusize = vbval(P, u_char);
				break;
			case 2:
				pdusize = ntohs(vbval(P, u_short));
				break;
			default:;
#ifdef ARGO_DEBUG
				if (argo_debug[D_TPINPUT]) {
					printf("malformed prefered TPDU option\n");
				}
#endif
			}
			break;
		case TPP_addl_opt:
			vb_getval(P, u_char, addlopt);
			break;
		case TPP_calling_sufx:
			/*
			 * could use vb_getval, but we want to save the loc &
			 * len for later use
			 */
			fsufxloc = (caddr_t) & vbptr(P)->tpv_val;
			fsufxlen = vbptr(P)->tpv_len;
#ifdef ARGO_DEBUG
			if (argo_debug[D_TPINPUT]) {
				printf("CR fsufx:");
				{
					int    j;
					for (j = 0; j < fsufxlen; j++) {
						printf(" 0x%x. ", *((caddr_t) (fsufxloc + j)));
					}
					printf("\n");
				}
			}
#endif
			break;
		case TPP_called_sufx:
			/*
			 * could use vb_getval, but we want to save the loc &
			 * len for later use
			 */
			lsufxloc = (caddr_t) & vbptr(P)->tpv_val;
			lsufxlen = vbptr(P)->tpv_len;
#ifdef ARGO_DEBUG
			if (argo_debug[D_TPINPUT]) {
				printf("CR lsufx:");
				{
					int    j;
					for (j = 0; j < lsufxlen; j++) {
						printf(" 0x%x. ", *((u_char *) (lsufxloc + j)));
					}
					printf("\n");
				}
			}
#endif
			break;

#ifdef TP_PERF_MEAS
		case TPP_perf_meas:
			vb_getval(P, u_char, perf_meas);
			break;
#endif				/* TP_PERF_MEAS */

		case TPP_vers:
			/* not in class 0; 1 octet; in CR_TPDU only */
			/*
			 * COS tests says if version wrong, use default
			 * version!?XXX
			 */
			CHECK((vbval(P, u_char) != TP_VERSION),
			      E_TP_INV_PVAL, ts_inv_pval, setversion,
			(1 + (caddr_t) & vbptr(P)->tpv_val - (caddr_t) hdr));
	setversion:
			version = vbval(P, u_char);
			break;
		case TPP_acktime:
			vb_getval(P, u_short, acktime);
			acktime = ntohs(acktime);
			acktime = acktime / 500;	/* convert to slowtimo
							 * ticks */
			if ((short) acktime <= 0)
				acktime = 2;	/* don't allow a bad peer to
						 * screw us up */
#ifdef ARGO_DEBUG
			if (argo_debug[D_TPINPUT]) {
				printf("CR acktime 0x%x\n", acktime);
			}
#endif
			break;

		case TPP_alt_class:
			{
				u_char         *aclass = 0;
				int    i;
				static u_char   bad_alt_classes[5] =
				{~0, ~3, ~5, ~0xf, ~0x1f};

				aclass =
					(u_char *) & (((struct tp_vbp *) P)->tpv_val);
				for (i = ((struct tp_vbp *) P)->tpv_len; i > 0; i--) {
					alt_classes |= (1 << ((*aclass++) >> 4));
				}
				CHECK((bad_alt_classes[hdr->tpdu_CRclass] & alt_classes),
				      E_TP_INV_PVAL, ts_inv_aclass, respond,
				      ((caddr_t) aclass) - (caddr_t) hdr);
#ifdef ARGO_DEBUG
				if (argo_debug[D_TPINPUT]) {
					printf("alt_classes 0x%x\n", alt_classes);
				}
#endif
			}
			break;

		case TPP_security:
		case TPP_residER:
		case TPP_priority:
		case TPP_transdelay:
		case TPP_throughput:
		case TPP_addl_info:
		case TPP_subseq:
		default:
#ifdef ARGO_DEBUG
			if (argo_debug[D_TPINPUT]) {
				printf("param ignored CR_TPDU code= 0x%x\n",
				       vbptr(P)->tpv_code);
			}
#endif
			IncStat(ts_param_ignored);
			break;

		case TPP_checksum:
#ifdef ARGO_DEBUG
			if (argo_debug[D_TPINPUT]) {
				printf("CR before cksum\n");
			}
#endif

			CHECK(iso_check_csum(m, tpdu_len),
			      E_TP_INV_PVAL, ts_bad_csum, discard, 0)
#ifdef ARGO_DEBUG
				if (argo_debug[D_TPINPUT]) {
				printf("CR before cksum\n");
			}
#endif
			break;
		}

		 /* } */ END_WHILE_OPTIONS(P)
			if (lsufxlen == 0) {
			/* can't look for a tpcb w/o any called sufx */
			error = E_TP_LENGTH_INVAL;
			IncStat(ts_inv_sufx);
			goto respond;
		} else {
			struct tp_pcb *t;
			/*
			 * The intention here is to trap all CR requests
			 * to a given nsap, for constructing transport
			 * service bridges at user level; so these
			 * intercepts should precede the normal listens.
			 * Phrasing the logic in this way also allows for
			 * mop-up listeners, which we don't currently implement.
			 * We also wish to have a single socket be able to
			 * listen over any network service provider,
			 * (cons or clns or ip).
			 */
			for (t = tp_listeners; t; t = t->tp_nextlisten)
				if ((t->tp_lsuffixlen == 0 ||
				     (lsufxlen == t->tp_lsuffixlen &&
				      bcmp(lsufxloc, t->tp_lsuffix, lsufxlen) == 0)) &&
				    ((t->tp_flags & TPF_GENERAL_ADDR) ||
				     (laddr->sa_family == t->tp_domain &&
				      (*t->tp_nlproto->nlp_cmpnetaddr)
				      (t->tp_npcb, laddr, TP_LOCAL))))
					break;

			CHECK(t == 0, E_TP_NO_SESSION, ts_inv_sufx, respond,
			  (1 + 2 + (caddr_t) & hdr->_tpduf - (caddr_t) hdr))
			/*
			 * _tpduf is the fixed part; add 2 to get the dref
			 * bits of the fixed part (can't take the address of
			 * a bit field)
			 */
#ifdef ARGO_DEBUG
				if (argo_debug[D_TPINPUT]) {
				printf("checking if dup CR\n");
			}
#endif
			tpcb = t;
			for (t = tpcb->tp_next; t != tpcb; t = t->tp_next) {
				if (sref != t->tp_fref)
					continue;
				if ((*tpcb->tp_nlproto->nlp_cmpnetaddr) (
					   t->tp_npcb, faddr, TP_FOREIGN)) {
#ifdef ARGO_DEBUG
					if (argo_debug[D_TPINPUT]) {
						printf("duplicate CR discarded\n");
					}
#endif
					goto discard;
				}
			}
#ifdef TPPT
		if (tp_traceflags[D_TPINPUT]) {
				tptrace(TPPTmisc, "tp_input: tpcb *lsufxloc tpstate",
					tpcb, *lsufxloc, tpcb->tp_state, 0);
			}
#endif
		}

		/*
		 * WE HAVE A TPCB already know that the classes in the CR
		 * match at least one class implemented, but we don't know
		 * yet if they include any classes permitted by this server.
		 */

#ifdef ARGO_DEBUG
		if (argo_debug[D_TPINPUT]) {
			printf("HAVE A TPCB 1: %p\n", tpcb);
		}
#endif
#ifdef ARGO_DEBUG
		if (argo_debug[D_CONN]) {
			printf(
			       "CR: bef CHKS: flags 0x%x class_to_use 0x%x alt 0x%x opt 0x%x tp_class 0x%x\n",
			       tpcb->tp_flags, class_to_use, alt_classes, opt, tpcb->tp_class);
		}
#endif
		/* tpcb->tp_class doesn't include any classes not implemented  */
		class_to_use = (preferred_class & tpcb->tp_class);
		if ((class_to_use = preferred_class & tpcb->tp_class) == 0)
			class_to_use = alt_classes & tpcb->tp_class;

		class_to_use = 1 << tp_mask_to_num(class_to_use);

		{
			tpp = tpcb->_tp_param;
			tpp.p_class = class_to_use;
			tpp.p_tpdusize = dusize;
			tpp.p_ptpdusize = pdusize;
			tpp.p_xtd_format = (opt & TPO_XTD_FMT) == TPO_XTD_FMT;
			tpp.p_xpd_service = (addlopt & TPAO_USE_TXPD) == TPAO_USE_TXPD;
			tpp.p_use_checksum = (tpp.p_class == TP_CLASS_0) ? 0 :
				(addlopt & TPAO_NO_CSUM) == 0;
			tpp.p_version = version;
#ifdef notdef
			tpp.p_use_efc = (opt & TPO_USE_EFC) == TPO_USE_EFC;
			tpp.p_use_nxpd = (addlopt & TPAO_USE_NXPD) == TPAO_USE_NXPD;
			tpp.p_use_rcc = (addlopt & TPAO_USE_RCC) == TPAO_USE_RCC;
#endif				/* notdef */

			CHECK(
			      tp_consistency(tpcb, 0 /* not force or strict */ , &tpp) != 0,
			E_TP_NEGOT_FAILED, ts_negotfailed, clear_parent_tcb,
			      (1 + 2 + (caddr_t) & hdr->_tpdufr.CRCC - (caddr_t) hdr)
			/* ^ more or less the location of class */
				)
		}
#ifdef TPPT
		if (tp_traceflags[D_CONN]) {
			tptrace(TPPTmisc,
		       "after 1 consist class_to_use class, out, tpconsout",
				class_to_use,
				tpcb->tp_class, dgout_routine, tpcons_output
			);
		}
#endif
		CHECK(((class_to_use == TP_CLASS_0) && 
		      (dgout_routine != tpcons_output)),
			E_TP_NEGOT_FAILED, ts_negotfailed, clear_parent_tcb,
		     (1 + 2 + (caddr_t) & hdr->_tpdufr.CRCC - (caddr_t) hdr)
		/* ^ more or less the location of class */
			)
#ifdef ARGO_DEBUG
			if (argo_debug[D_CONN]) {
			printf("CR: after CRCCCHECKS: tpcb %p, flags 0x%x\n",
			       tpcb, tpcb->tp_flags);
		}
#endif
		takes_data = TRUE;
		e.TPDU_ATTR(CR).e_cdt = hdr->tpdu_CRcdt;
		e.ev_number = CR_TPDU;

		so = tpcb->tp_sock;
		if (so->so_options & SO_ACCEPTCONN) {
			struct tp_pcb  *parent_tpcb = tpcb;
			/*
			 * Create a socket, tpcb, ll pcb, etc. for this
			 * newborn connection, and fill in all the values.
			 */
#ifdef ARGO_DEBUG
			if (argo_debug[D_CONN]) {
				printf("abt to call tp_newsocket(%p, %p, %p, %p)\n",
				       so, laddr, faddr, cons_channel);
			}
#endif
			if ((so =
			     tp_newsocket(so, faddr, cons_channel,
					  class_to_use,
			       ((tpcb->tp_netservice == IN_CLNS) ? IN_CLNS :
				(dgout_routine == tpcons_output) ? ISO_CONS : ISO_CLNS))
			     ) == (struct socket *) 0) {
				/*
				 * note - even if netservice is IN_CLNS, as
				 * far as the tp entity is concerned, the
				 * only differences are CO vs CL
				 */
#ifdef ARGO_DEBUG
				if (argo_debug[D_CONN]) {
					printf("tp_newsocket returns 0\n");
				}
#endif
				goto discard;
		clear_parent_tcb:
				tpcb = 0;
				goto respond;
			}
			tpcb = sototpcb(so);
			insque(tpcb, parent_tpcb);

			/*
			 * Stash the addresses in the net level pcb
			 * kind of like a pcbconnect() but don't need
			 * or want all those checks.
			 */
			(tpcb->tp_nlproto->nlp_putnetaddr) (tpcb->tp_npcb, faddr, TP_FOREIGN);
			(tpcb->tp_nlproto->nlp_putnetaddr) (tpcb->tp_npcb, laddr, TP_LOCAL);

			/* stash the f suffix in the new tpcb */
			if ((tpcb->tp_fsuffixlen = fsufxlen) != 0) {
				bcopy(fsufxloc, tpcb->tp_fsuffix, fsufxlen);
				(tpcb->tp_nlproto->nlp_putsufx)
					(tpcb->tp_npcb, fsufxloc, fsufxlen, TP_FOREIGN);
			}
			/* stash the l suffix in the new tpcb */
			tpcb->tp_lsuffixlen = lsufxlen;
			bcopy(lsufxloc, tpcb->tp_lsuffix, lsufxlen);
			(tpcb->tp_nlproto->nlp_putsufx)
				(tpcb->tp_npcb, lsufxloc, lsufxlen, TP_LOCAL);
#ifdef TP_PERF_MEAS
			if (tpcb->tp_perf_on = perf_meas) {	/* assignment */
				/*
				 * ok, let's create an mbuf for stashing the
				 * statistics if one doesn't already exist
				 */
				(void) tp_setup_perf(tpcb);
			}
#endif				/* TP_PERF_MEAS */
			tpcb->tp_fref = sref;

			/*
			 * We've already checked for consistency with the
			 * options set in tpp,  but we couldn't set them
			 * earlier because we didn't want to change options
			 * in the LISTENING tpcb. Now we set the options in
			 * the new socket's tpcb.
			 */
			(void) tp_consistency(tpcb, TP_FORCE, &tpp);

			if (!tpcb->tp_use_checksum)
				IncStat(ts_csum_off);
			if (tpcb->tp_xpd_service)
				IncStat(ts_use_txpd);
			if (tpcb->tp_xtd_format)
				IncStat(ts_xtd_fmt);

			tpcb->tp_peer_acktime = acktime;

			/*
			 * The following kludge is used to test
			 * retransmissions and timeout during connection
			 * establishment.
			 */
#ifdef ARGO_DEBUG
			if (argo_debug[D_ZDREF]) {
				IncStat(ts_zdebug);
				/* tpcb->tp_fref = 0; */
			}
#endif
		}
		LOCAL_CREDIT(tpcb);
		IncStat(ts_CR_rcvd);
		if (!tpcb->tp_cebit_off) {
			tpcb->tp_win_recv = tp_start_win << 8;
			tpcb->tp_cong_sample.cs_size = 0;
			CONG_INIT_SAMPLE(tpcb);
			CONG_UPDATE_SAMPLE(tpcb, ce_bit);
		}
	} else if (dutype == ER_TPDU_type) {
		/*
		 * ER TPDUs have to be recognized separately because they
		 * don't necessarily have a tpcb with them and we don't want
		 * err out looking for such a beast. We could put a bunch of
		 * little kludges in the next section of code so it would
		 * avoid references to tpcb if dutype == ER_TPDU_type but we
		 * don't want code for ERs to mess up code for data transfer.
		 */
		IncStat(ts_ER_rcvd);
		e.ev_number = ER_TPDU;
		e.TPDU_ATTR(ER).e_reason = (u_char) hdr->tpdu_ERreason;
		CHECK(((int) dref <= 0 || dref >= tp_refinfo.tpr_size ||
		     (tpcb = tp_ref[dref].tpr_pcb) == (struct tp_pcb *) 0 ||
		       tpcb->tp_refstate == REF_FREE ||
		       tpcb->tp_refstate == REF_FROZEN),
		      E_TP_MISM_REFS, ts_inv_dref, discard, 0)
	} else {
		/* tpdu type is CC, XPD, XAK, GR, AK, DR, DC, or DT */

		/*
		 * In the next 4 checks, _tpduf is the fixed part; add 2 to
		 * get the dref bits of the fixed part (can't take the
		 * address of a bit field)
		 */
#ifdef TPCONS
		if (cons_channel && dutype == DT_TPDU_type) {
			struct isopcb  *isop = ((struct isopcb *)
			       ((struct pklcd *) cons_channel)->lcd_upnext);
			if (isop && isop->isop_refcnt == 1 && isop->isop_socket &&
			    (tpcb = sototpcb(isop->isop_socket)) &&
			    (tpcb->tp_class == TP_CLASS_0 /* || == CLASS_1 */ )) {
#ifdef ARGO_DEBUG
				if (argo_debug[D_TPINPUT]) {
					printf("tpinput_dt: class 0 short circuit\n");
				}
#endif
				dref = tpcb->tp_lref;
				sref = tpcb->tp_fref;
				CHECK((tpcb->tp_refstate == REF_FREE),
				      E_TP_MISM_REFS, ts_inv_dref, nonx_dref,
				      (1 + 2 + (caddr_t) & hdr->_tpduf - (caddr_t) hdr))
					goto tp0_data;
			}
		}
#endif
		{

			CHECK(((int) dref <= 0 || dref >= tp_refinfo.tpr_size),
			      E_TP_MISM_REFS, ts_inv_dref, nonx_dref,
			  (1 + 2 + (caddr_t) & hdr->_tpduf - (caddr_t) hdr))
				CHECK(((tpcb = tp_ref[dref].tpr_pcb) == (struct tp_pcb *) 0),
				      E_TP_MISM_REFS, ts_inv_dref, nonx_dref,
			  (1 + 2 + (caddr_t) & hdr->_tpduf - (caddr_t) hdr))
				CHECK((tpcb->tp_refstate == REF_FREE),
				      E_TP_MISM_REFS, ts_inv_dref, nonx_dref,
			  (1 + 2 + (caddr_t) & hdr->_tpduf - (caddr_t) hdr))
		}

#ifdef ARGO_DEBUG
		if (argo_debug[D_TPINPUT]) {
			printf("HAVE A TPCB 2: %p\n", tpcb);
		}
#endif

		/* causes a DR to be sent for CC; ER for all else */
		CHECK((tpcb->tp_refstate == REF_FROZEN),
		(dutype == CC_TPDU_type ? E_TP_NO_SESSION : E_TP_MISM_REFS),
		      ts_inv_dref, respond,
		      (1 + 2 + (caddr_t) & hdr->_tpduf - (caddr_t) hdr))
#ifdef ARGO_DEBUG
		if (argo_debug[D_TPINPUT]) {
			printf("state of dref %d ok, tpcb %p\n", dref, tpcb);
		}
#endif
		/*
		 * At this point the state of the dref could be FROZEN:
		 * tpr_pcb == NULL,  has ( reference only) timers for
		 * example, DC may arrive after the close() has detached the
		 * tpcb (e.g., if user turned off SO_LISTEN option) OPENING :
		 * a tpcb exists but no timers yet OPEN  : tpcb exists &
		 * timers are outstanding
		 */

		if (!tpcb->tp_cebit_off)
			CONG_UPDATE_SAMPLE(tpcb, ce_bit);

		dusize = tpcb->tp_tpdusize;
		pdusize = tpcb->tp_ptpdusize;

		dutype = hdr->tpdu_type << 8;	/* for the switch below */

		WHILE_OPTIONS(P, hdr, tpcb->tp_xtd_format)	/* { */
#define caseof(x,y) case (((x)<<8)+(y))
			switch (dutype | vbptr(P)->tpv_code) {

	caseof(CC_TPDU_type, TPP_addl_opt):
			/* not in class 0; 1 octet */
			vb_getval(P, u_char, addlopt);
			break;
	caseof(CC_TPDU_type, TPP_tpdu_size):
			{
				u_char          odusize = dusize;
				vb_getval(P, u_char, dusize);
				CHECK((dusize < TP_MIN_TPDUSIZE ||
				       dusize > TP_MAX_TPDUSIZE || dusize > odusize),
				      E_TP_INV_PVAL, ts_inv_pval, respond,
				      (1 + (caddr_t) & vbptr(P)->tpv_val - (caddr_t) hdr))
#ifdef ARGO_DEBUG
					if (argo_debug[D_TPINPUT]) {
					printf("CC dusize 0x%x\n", dusize);
				}
#endif
			}
			break;
	caseof(CC_TPDU_type, TPP_ptpdu_size):
			{
				u_short         opdusize = pdusize;
				switch (vbptr(P)->tpv_len) {
				case 1:
					pdusize = vbval(P, u_char);
					break;
				case 2:
					pdusize = ntohs(vbval(P, u_short));
					break;
				default:;
#ifdef ARGO_DEBUG
					if (argo_debug[D_TPINPUT]) {
						printf("malformed prefered TPDU option\n");
					}
#endif
				}
				CHECK((pdusize == 0 ||
				       (opdusize && (pdusize > opdusize))),
				      E_TP_INV_PVAL, ts_inv_pval, respond,
				      (1 + (caddr_t) & vbptr(P)->tpv_val - (caddr_t) hdr))
			}
			break;
	caseof(CC_TPDU_type, TPP_calling_sufx):
#ifdef ARGO_DEBUG
			if (argo_debug[D_TPINPUT]) {
				printf("CC calling (local) sufxlen 0x%x\n", lsufxlen);
			}
#endif
			lsufxloc = (caddr_t) & vbptr(P)->tpv_val;
			lsufxlen = vbptr(P)->tpv_len;
			break;
	caseof(CC_TPDU_type, TPP_acktime):
			/* class 4 only, 2 octets */
			vb_getval(P, u_short, acktime);
			acktime = ntohs(acktime);
			acktime = acktime / 500;	/* convert to slowtimo
							 * ticks */
			if ((short) acktime <= 0)
				acktime = 2;
			break;
	caseof(CC_TPDU_type, TPP_called_sufx):
			fsufxloc = (caddr_t) & vbptr(P)->tpv_val;
			fsufxlen = vbptr(P)->tpv_len;
#ifdef ARGO_DEBUG
			if (argo_debug[D_TPINPUT]) {
				printf("CC called (foreign) sufx len %d\n", fsufxlen);
			}
#endif
			break;

	caseof(CC_TPDU_type, TPP_checksum):
	caseof(DR_TPDU_type, TPP_checksum):
	caseof(DT_TPDU_type, TPP_checksum):
	caseof(XPD_TPDU_type, TPP_checksum):
			if (tpcb->tp_use_checksum) {
				CHECK(iso_check_csum(m, tpdu_len),
				      E_TP_INV_PVAL, ts_bad_csum, discard, 0)
			}
			break;

			/*
			 * this is different from the above because in the
			 * context of concat/ sep tpdu_len might not be the
			 * same as hdr len
			 */
	caseof(AK_TPDU_type, TPP_checksum):
	caseof(XAK_TPDU_type, TPP_checksum):
	caseof(DC_TPDU_type, TPP_checksum):
			if (tpcb->tp_use_checksum) {
				CHECK(iso_check_csum(m, (int) hdr->tpdu_li + 1),
				      E_TP_INV_PVAL, ts_bad_csum, discard, 0)
			}
			break;
#ifdef notdef
	caseof(DR_TPDU_type, TPP_addl_info):
			/*
			 * ignore - its length and meaning are user defined
			 * and there's no way to pass this info to the user
			 * anyway
			 */
			break;
#endif				/* notdef */

	caseof(AK_TPDU_type, TPP_subseq):
			/* used after reduction of window */
			vb_getval(P, u_short, subseq);
			subseq = ntohs(subseq);
#ifdef ARGO_DEBUG
			if (argo_debug[D_ACKRECV]) {
				printf("AK dref 0x%x Subseq 0x%x\n", dref, subseq);
			}
#endif
			break;

	caseof(AK_TPDU_type, TPP_flow_cntl_conf):
			{
				u_int           ylwe;
				u_short         ysubseq, ycredit;

				fcc_present = TRUE;
				vb_getval(P, u_int, ylwe);
				vb_getval(P, u_short, ysubseq);
				vb_getval(P, u_short, ycredit);
				ylwe = ntohl(ylwe);
				ysubseq = ntohs(ysubseq);
				ycredit = ntohs(ycredit);
#ifdef ARGO_DEBUG
				if (argo_debug[D_ACKRECV]) {
					printf("%s%x, subseq 0x%x, cdt 0x%x dref 0x%x\n",
					       "AK FCC lwe 0x", ylwe, ysubseq, ycredit, dref);
				}
#endif
			}
			break;

		default:
#ifdef ARGO_DEBUG
			if (argo_debug[D_TPINPUT]) {
				printf("param ignored dutype 0x%x, code  0x%x\n",
				       dutype, vbptr(P)->tpv_code);
			}
#endif
#ifdef TPPT
			if (tp_traceflags[D_TPINPUT]) {
				tptrace(TPPTmisc, "param ignored dutype code ",
					dutype, vbptr(P)->tpv_code, 0, 0);
			}
#endif
			IncStat(ts_param_ignored);
			break;
#undef caseof
		}
		 /* } */ END_WHILE_OPTIONS(P)
		/* NOTE: the variable dutype has been shifted left! */

			switch (hdr->tpdu_type) {
		case CC_TPDU_type:
			/*
			 * If CC comes back with an unacceptable class
			 * respond with a DR or ER
			 */

			opt = hdr->tpdu_CCoptions;	/* 1 byte */

			{
				tpp = tpcb->_tp_param;
				tpp.p_class = (1 << hdr->tpdu_CCclass);
				tpp.p_tpdusize = dusize;
				tpp.p_ptpdusize = pdusize;
				tpp.p_dont_change_params = 0;
				tpp.p_xtd_format = (opt & TPO_XTD_FMT) == TPO_XTD_FMT;
				tpp.p_xpd_service = (addlopt & TPAO_USE_TXPD) == TPAO_USE_TXPD;
				tpp.p_use_checksum = (addlopt & TPAO_NO_CSUM) == 0;
#ifdef notdef
				tpp.p_use_efc = (opt & TPO_USE_EFC) == TPO_USE_EFC;
				tpp.p_use_nxpd = (addlopt & TPAO_USE_NXPD) == TPAO_USE_NXPD;
				tpp.p_use_rcc = (addlopt & TPAO_USE_RCC) == TPAO_USE_RCC;
#endif				/* notdef */

				CHECK(
				  tp_consistency(tpcb, TP_FORCE, &tpp) != 0,
				 E_TP_NEGOT_FAILED, ts_negotfailed, respond,
				      (1 + 2 + (caddr_t) & hdr->_tpdufr.CRCC - (caddr_t) hdr)
				/* ^ more or less the location of class */
					)
#ifdef TPPT
					if (tp_traceflags[D_CONN]) {
					tptrace(TPPTmisc,
				    "after 1 consist class, out, tpconsout",
						tpcb->tp_class, dgout_routine, tpcons_output, 0
					);
				}
#endif
					CHECK(
					    ((class_to_use == TP_CLASS_0) &&
					  (dgout_routine != tpcons_output)),
				 E_TP_NEGOT_FAILED, ts_negotfailed, respond,
					      (1 + 2 + (caddr_t) & hdr->_tpdufr.CRCC - (caddr_t) hdr)
				/* ^ more or less the location of class */
					)
#ifdef TPCONS
					if (tpcb->tp_netservice == ISO_CONS &&
					    class_to_use == TP_CLASS_0) {
					struct isopcb  *isop = (struct isopcb *) tpcb->tp_npcb;
					struct pklcd   *lcp = (struct pklcd *) isop->isop_chan;
					lcp->lcd_flags &= ~X25_DG_CIRCUIT;
				}
#endif
			}
			if (!tpcb->tp_use_checksum)
				IncStat(ts_csum_off);
			if (tpcb->tp_xpd_service)
				IncStat(ts_use_txpd);
			if (tpcb->tp_xtd_format)
				IncStat(ts_xtd_fmt);

#ifdef TPPT
			if (tp_traceflags[D_CONN]) {
				tptrace(TPPTmisc, "after CC class flags dusize CCclass",
			  tpcb->tp_class, tpcb->tp_flags, tpcb->tp_tpdusize,
					hdr->tpdu_CCclass);
			}
#endif

			/*
			 * if called or calling suffices appeared on the CC,
			 * they'd better jive with what's in the pcb
			 */
				if (fsufxlen) {
				CHECK(((tpcb->tp_fsuffixlen != fsufxlen) ||
				bcmp(fsufxloc, tpcb->tp_fsuffix, fsufxlen)),
				      E_TP_INV_PVAL, ts_inv_sufx, respond,
				      (1 + fsufxloc - (caddr_t) hdr))
			}
			if (lsufxlen) {
				CHECK(((tpcb->tp_lsuffixlen != lsufxlen) ||
				bcmp(lsufxloc, tpcb->tp_lsuffix, lsufxlen)),
				      E_TP_INV_PVAL, ts_inv_sufx, respond,
				      (1 + lsufxloc - (caddr_t) hdr))
			}
			e.TPDU_ATTR(CC).e_sref = sref;
			e.TPDU_ATTR(CC).e_cdt = hdr->tpdu_CCcdt;
			takes_data = TRUE;
			e.ev_number = CC_TPDU;
			IncStat(ts_CC_rcvd);
			break;

		case DC_TPDU_type:
			if (sref != tpcb->tp_fref)
				printf("INPUT: inv sufx DCsref 0x%x, tp_fref 0x%x\n",
				       sref, tpcb->tp_fref);

			CHECK((sref != tpcb->tp_fref),
			      E_TP_MISM_REFS, ts_inv_sufx, discard,
			 (1 + (caddr_t) & hdr->tpdu_DCsref - (caddr_t) hdr))
				e.ev_number = DC_TPDU;
			IncStat(ts_DC_rcvd);
			break;

		case DR_TPDU_type:
#ifdef TPPT
			if (tp_traceflags[D_TPINPUT]) {
				tptrace(TPPTmisc, "DR recvd", hdr->tpdu_DRreason, 0, 0, 0);
			}
#endif
				if (sref != tpcb->tp_fref) {
				printf("INPUT: inv sufx DRsref 0x%x tp_fref 0x%x\n",
				       sref, tpcb->tp_fref);
			}
			CHECK((sref != 0 && sref != tpcb->tp_fref &&
			       tpcb->tp_state != TP_CRSENT),
			      (TP_ERROR_SNDC | E_TP_MISM_REFS), ts_inv_sufx, respond,
			 (1 + (caddr_t) & hdr->tpdu_DRsref - (caddr_t) hdr))
				e.TPDU_ATTR(DR).e_reason = hdr->tpdu_DRreason;
			e.TPDU_ATTR(DR).e_sref = (u_short) sref;
			takes_data = TRUE;
			e.ev_number = DR_TPDU;
			IncStat(ts_DR_rcvd);
			break;

		case ER_TPDU_type:
#ifdef TPPT
			if (tp_traceflags[D_TPINPUT]) {
				tptrace(TPPTmisc, "ER recvd", hdr->tpdu_ERreason, 0, 0, 0);
			}
#endif
				e.ev_number = ER_TPDU;
			e.TPDU_ATTR(ER).e_reason = hdr->tpdu_ERreason;
			IncStat(ts_ER_rcvd);
			break;

		case AK_TPDU_type:

			e.TPDU_ATTR(AK).e_subseq = subseq;
			e.TPDU_ATTR(AK).e_fcc_present = fcc_present;

			if (tpcb->tp_xtd_format) {
#ifdef BYTE_ORDER
				union seq_type  seqeotX;

				seqeotX.s_seqeot = ntohl(hdr->tpdu_seqeotX);
				e.TPDU_ATTR(AK).e_seq = seqeotX.s_seq;
				e.TPDU_ATTR(AK).e_cdt = ntohs(hdr->tpdu_AKcdtX);
#else
				e.TPDU_ATTR(AK).e_cdt = hdr->tpdu_AKcdtX;
				e.TPDU_ATTR(AK).e_seq = hdr->tpdu_AKseqX;
#endif				/* BYTE_ORDER */
			} else {
				e.TPDU_ATTR(AK).e_cdt = hdr->tpdu_AKcdt;
				e.TPDU_ATTR(AK).e_seq = hdr->tpdu_AKseq;
			}
#ifdef TPPT
			if (tp_traceflags[D_TPINPUT]) {
				tptrace(TPPTmisc, "AK recvd seq cdt subseq fcc_pres",
			       e.TPDU_ATTR(AK).e_seq, e.TPDU_ATTR(AK).e_cdt,
					subseq, fcc_present);
			}
#endif

				e.ev_number = AK_TPDU;
			IncStat(ts_AK_rcvd);
			IncPStat(tpcb, tps_AK_rcvd);
			break;

		case XAK_TPDU_type:
			if (tpcb->tp_xtd_format) {
#ifdef BYTE_ORDER
				union seq_type  seqeotX;

				seqeotX.s_seqeot = ntohl(hdr->tpdu_seqeotX);
				e.TPDU_ATTR(XAK).e_seq = seqeotX.s_seq;
#else
				e.TPDU_ATTR(XAK).e_seq = hdr->tpdu_XAKseqX;
#endif				/* BYTE_ORDER */
			} else {
				e.TPDU_ATTR(XAK).e_seq = hdr->tpdu_XAKseq;
			}
			e.ev_number = XAK_TPDU;
			IncStat(ts_XAK_rcvd);
			IncPStat(tpcb, tps_XAK_rcvd);
			break;

		case XPD_TPDU_type:
			if (tpcb->tp_xtd_format) {
#ifdef BYTE_ORDER
				union seq_type  seqeotX;

				seqeotX.s_seqeot = ntohl(hdr->tpdu_seqeotX);
				e.TPDU_ATTR(XPD).e_seq = seqeotX.s_seq;
#else
				e.TPDU_ATTR(XPD).e_seq = hdr->tpdu_XPDseqX;
#endif				/* BYTE_ORDER */
			} else {
				e.TPDU_ATTR(XPD).e_seq = hdr->tpdu_XPDseq;
			}
			takes_data = TRUE;
			e.ev_number = XPD_TPDU;
			IncStat(ts_XPD_rcvd);
			IncPStat(tpcb, tps_XPD_rcvd);
			break;

		case DT_TPDU_type:
			/*
			 * the y option will cause occasional packets
			 * to be dropped. A little crude but it
			 * works.
			 */

#ifdef ARGO_DEBUG
			if (argo_debug[D_DROP]) {
				if (time.tv_usec & 0x4 &&
				    hdr->tpdu_DTseq & 0x1) {
					IncStat(ts_ydebug);
					goto discard;
				}
			}
#endif
			if (tpcb->tp_class == TP_CLASS_0) {
#ifdef TPCONS
		tp0_data:
#endif
				e.TPDU_ATTR(DT).e_seq = 0;	/* actually don't care */
				e.TPDU_ATTR(DT).e_eot = (((struct tp0du *) hdr)->tp0du_eot);
			} else if (tpcb->tp_xtd_format) {
#ifdef BYTE_ORDER
				union seq_type  seqeotX;

				seqeotX.s_seqeot = ntohl(hdr->tpdu_seqeotX);
				e.TPDU_ATTR(DT).e_seq = seqeotX.s_seq;
				e.TPDU_ATTR(DT).e_eot = seqeotX.s_eot;
#else
				e.TPDU_ATTR(DT).e_seq = hdr->tpdu_DTseqX;
				e.TPDU_ATTR(DT).e_eot = hdr->tpdu_DTeotX;
#endif				/* BYTE_ORDER */
			} else {
				e.TPDU_ATTR(DT).e_seq = hdr->tpdu_DTseq;
				e.TPDU_ATTR(DT).e_eot = hdr->tpdu_DTeot;
			}
			if (e.TPDU_ATTR(DT).e_eot)
				IncStat(ts_eot_input);
			takes_data = TRUE;
			e.ev_number = DT_TPDU;
			IncStat(ts_DT_rcvd);
			IncPStat(tpcb, tps_DT_rcvd);
			break;

		case GR_TPDU_type:
			tp_indicate(T_DISCONNECT, tpcb, ECONNABORTED);
			/* drop through */
		default:
			/*
			 * this should NEVER happen because there is a check
			 * for dutype well above here
			 */
			error = E_TP_INV_TPDU;	/* causes an ER  */
#ifdef ARGO_DEBUG
			if (argo_debug[D_TPINPUT]) {
				printf("INVALID dutype 0x%x\n", hdr->tpdu_type);
			}
#endif
			IncStat(ts_inv_dutype);
			goto respond;
		}
	}
	/*
	 * peel off the tp header; remember that the du_li doesn't count
	 * itself. This may leave us w/ an empty mbuf at the front of a
	 * chain. We can't just throw away the empty mbuf because hdr still
	 * points into the mbuf's data area and we're still using hdr (the
	 * tpdu header)
	 */
	m->m_len -= ((int) hdr->tpdu_li + 1);
	m->m_data += ((int) hdr->tpdu_li + 1);

	if (takes_data) {
		int             max = tpdu_info[hdr->tpdu_type][TP_MAX_DATA_INDEX];
		int             datalen = tpdu_len - hdr->tpdu_li - 1, mbtype = MT_DATA;
		struct {
			struct tp_disc_reason dr;
			struct cmsghdr  x_hdr;
		}               x;
#define c_hdr x.x_hdr
		struct mbuf *n;

		CHECK((max && datalen > max), E_TP_LENGTH_INVAL,
		      ts_inv_length, respond, (max + hdr->tpdu_li + 1));
		switch (hdr->tpdu_type) {

		case CR_TPDU_type:
			c_hdr.cmsg_type = TPOPT_CONN_DATA;
			goto make_control_msg;

		case CC_TPDU_type:
			c_hdr.cmsg_type = TPOPT_CFRM_DATA;
			goto make_control_msg;

		case DR_TPDU_type:
			x.dr.dr_hdr.cmsg_len = sizeof(x) - sizeof(c_hdr);
			x.dr.dr_hdr.cmsg_type = TPOPT_DISC_REASON;
			x.dr.dr_hdr.cmsg_level = SOL_TRANSPORT;
			x.dr.dr_reason = hdr->tpdu_DRreason;
			c_hdr.cmsg_type = TPOPT_DISC_DATA;
	make_control_msg:
			datalen += sizeof(c_hdr);
			c_hdr.cmsg_len = datalen;
			c_hdr.cmsg_level = SOL_TRANSPORT;
			mbtype = MT_CONTROL;
			MGET(n, M_DONTWAIT, MT_DATA);
			if (n == 0) {
				m_freem(m);
				m = 0;
				datalen = 0;
				goto invoke;
			}
			if (hdr->tpdu_type == DR_TPDU_type) {
				datalen += sizeof(x) - sizeof(c_hdr);
				bcopy((caddr_t) & x, mtod(n, caddr_t), n->m_len = sizeof(x));
			} else
				bcopy((caddr_t) & c_hdr, mtod(n, caddr_t),
				      n->m_len = sizeof(c_hdr));
			n->m_next = m;
			m = n;
			/* FALLTHROUGH */

		case XPD_TPDU_type:
			if (mbtype != MT_CONTROL)
				mbtype = MT_OOBDATA;
			m->m_flags |= M_EOR;
			/* FALLTHROUGH */

		case DT_TPDU_type:
			for (n = m; n; n = n->m_next) {
				MCHTYPE(n, mbtype);
			}
	invoke:
			e.TPDU_ATTR(DT).e_datalen = datalen;
			e.TPDU_ATTR(DT).e_data = m;
			break;

		default:
			printf(
			       "ERROR in tp_input! hdr->tpdu_type 0x%x takes_data 0x%x m %p\n",
			       hdr->tpdu_type, takes_data, m);
			break;
		}
		/*
		 * prevent m_freem() after tp_driver() from throwing it all
		 * away
		 */
		m = MNULL;
	}
	IncStat(ts_tpdu_rcvd);

#ifdef ARGO_DEBUG
	if (argo_debug[D_TPINPUT]) {
		printf("tp_input: before driver, state 0x%x event 0x%x m %p",
		       tpcb->tp_state, e.ev_number, m);
		printf(" e.e_data %p\n", e.TPDU_ATTR(DT).e_data);
		printf("takes_data 0x%x m_len 0x%x, tpdu_len 0x%x\n",
		       takes_data, (m == MNULL) ? 0 : m->m_len, tpdu_len);
	}
#endif

	error = tp_driver(tpcb, &e);

	ASSERT(tpcb != (struct tp_pcb *) 0);
	ASSERT(tpcb->tp_sock != (struct socket *) 0);
	if (tpcb->tp_sock->so_error == 0)
		tpcb->tp_sock->so_error = error;

	/*
	 * Kludge to keep the state tables under control (adding data on
	 * connect & disconnect & freeing the mbuf containing the data would
	 * have exploded the tables and made a big mess ).
	 */
	switch (e.ev_number) {
	case CC_TPDU:
	case DR_TPDU:
	case CR_TPDU:
		m = e.TPDU_ATTR(CC).e_data;	/* same field for all three
						 * dutypes */
#ifdef ARGO_DEBUG
		if (argo_debug[D_TPINPUT]) {
			printf("after driver, restoring m to %p, takes_data 0x%x\n",
			       m, takes_data);
		}
#endif
		break;
	default:
		break;
	}
	/*
	 * Concatenated sequences are terminated by any tpdu that carries
	 * data: CR, CC, DT, XPD, DR. All other tpdu types may be
	 * concatenated: AK, XAK, DC, ER.
	 */

	if (takes_data == 0) {
		ASSERT(m != MNULL);
		/*
		 * we already peeled off the prev. tp header so we can just
		 * pull up some more and repeat
		 */

		if ((m = tp_inputprep(m)) != NULL) {
#ifdef ARGO_DEBUG
			if (argo_debug[D_TPINPUT]) {
				hdr = mtod(m, struct tpdu *);
				printf("tp_input @ separate: hdr %p size %d m %p\n",
				       hdr, (int) hdr->tpdu_li + 1, m);
				dump_mbuf(m, "tp_input after driver, at separate");
			}
#endif

			IncStat(ts_concat_rcvd);
			goto again;
		}
	}
	if (m != MNULL) {
#ifdef ARGO_DEBUG
		if (argo_debug[D_TPINPUT]) {
			printf("tp_input : m_freem(%p)\n", m);
		}
#endif
		m_freem(m);
#ifdef ARGO_DEBUG
		if (argo_debug[D_TPINPUT]) {
			printf("tp_input : after m_freem %p\n", m);
		}
#endif
	}
	return;

discard:
	/* class 4: drop the tpdu */
	/*
	 * class 2,0: Should drop the net connection, if you can figure out
	 * to which connection it applies
	 */
#ifdef ARGO_DEBUG
	if (argo_debug[D_TPINPUT]) {
		printf("tp_input DISCARD\n");
	}
#endif
#ifdef TPPT
	if (tp_traceflags[D_TPINPUT]) {
		tptrace(TPPTmisc, "tp_input DISCARD m", m, 0, 0, 0);
	}
#endif
		m_freem(m);
	IncStat(ts_recv_drop);
	return;

nonx_dref:
	switch (dutype) {
	default:
		goto discard;
	case CC_TPDU_type:
		/* error = E_TP_MISM_REFS; */
		break;
	case DR_TPDU_type:
		error |= TP_ERROR_SNDC;
	}
respond:
#ifdef ARGO_DEBUG
	if (argo_debug[D_TPINPUT]) {
		printf("RESPOND: error 0x%x, errlen 0x%x\n", error, errlen);
	}
#endif
#ifdef TPPT
	if (tp_traceflags[D_TPINPUT]) {
		tptrace(TPPTmisc, "tp_input RESPOND m error sref", m, error, sref, 0);
	}
#endif
		if (sref == 0)
		goto discard;
	(void) tp_error_emit(error, (u_long) sref, satosiso(faddr),
			     satosiso(laddr), m, errlen, tpcb,
			     cons_channel, dgout_routine);
#ifdef ARGO_DEBUG
	if (argo_debug[D_ERROR_EMIT]) {
		printf("tp_input after error_emit\n");
	}
#endif

#ifdef lint
	printf("", sref, opt);
#endif				/* lint */
	IncStat(ts_recv_drop);
}


/*
 * NAME: tp_headersize()
 *
 * CALLED FROM:
 *  tp_emit() and tp_sbsend()
 *  TP needs to know the header size so it can figure out how
 *  much data to put in each tpdu.
 *
 * FUNCTION, ARGUMENTS, and RETURN VALUE:
 *  For a given connection, represented by (tpcb), and
 *  tpdu type (dutype), return the size of a tp header.
 *
 * RETURNS:	  the expected size of the heade in bytesr
 *
 * SIDE EFFECTS:
 *
 * NOTES:	 It would be nice if it got the network header size as well.
 */
int
tp_headersize(dutype, tpcb)
	int             dutype;
	struct tp_pcb  *tpcb;
{
	int    size = 0;

#ifdef TPPT
	if (tp_traceflags[D_CONN]) {
		tptrace(TPPTmisc, "tp_headersize dutype class xtd_format",
			dutype, tpcb->tp_class, tpcb->tp_xtd_format, 0);
	}
#endif
	if (!((tpcb->tp_class == TP_CLASS_0) ||
		      (tpcb->tp_class == TP_CLASS_4) ||
		      (dutype == DR_TPDU_type) ||
		      (dutype == CR_TPDU_type))) {
		printf("tp_headersize:dutype 0x%x, class 0x%x",
		       dutype, tpcb->tp_class);
		/* TODO: identify this and GET RID OF IT */
	}
	ASSERT((tpcb->tp_class == TP_CLASS_0) ||
	       (tpcb->tp_class == TP_CLASS_4) ||
	       (dutype == DR_TPDU_type) ||
	       (dutype == CR_TPDU_type));

	if (tpcb->tp_class == TP_CLASS_0) {
		size = tpdu_info[dutype][TP_LEN_CLASS_0_INDEX];
	} else {
		size = tpdu_info[dutype][tpcb->tp_xtd_format];
	}
	return size;
	/* caller must get network level header size separately */
}
