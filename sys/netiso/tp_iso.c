/*	$OpenBSD: tp_iso.c,v 1.9 2004/01/03 14:08:54 espie Exp $	*/
/*	$NetBSD: tp_iso.c,v 1.8 1996/03/16 23:13:54 christos Exp $	*/

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
 *	@(#)tp_iso.c	8.1 (Berkeley) 6/10/93
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
 * Here is where you find the iso-dependent code.  We've tried keep all
 * net-level and (primarily) address-family-dependent stuff out of the tp
 * source, and everthing here is reached indirectly through a switch table
 * (struct nl_protosw *) tpcb->tp_nlproto (see tp_pcb.c). The routines here
 * are: iso_getsufx: gets transport suffix out of an isopcb structure.
 * iso_putsufx: put transport suffix into an isopcb structure.
 * iso_putnetaddr: put a whole net addr into an isopcb. iso_getnetaddr: get a
 * whole net addr from an isopcb. iso_cmpnetaddr: compare a whole net addr
 * from an isopcb. iso_recycle_suffix: clear suffix for reuse in isopcb
 * tpclnp_ctlinput: handle ER CNLPdu : icmp-like stuff tpclnp_mtu: figure out
 * what size tpdu to use tpclnp_input: take a pkt from clnp, strip off its
 * clnp header, give to tp tpclnp_output_dg: package a pkt for clnp given 2
 * addresses & some data tpclnp_output: package a pkt for clnp given an
 * isopcb & some data
 */

#ifdef ISO

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/protosw.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/route.h>

#include <netiso/argo_debug.h>
#include <netiso/tp_param.h>
#include <netiso/tp_stat.h>
#include <netiso/tp_pcb.h>
#include <netiso/tp_trace.h>
#include <netiso/tp_stat.h>
#include <netiso/tp_tpdu.h>
#include <netiso/tp_clnp.h>
#include <netiso/tp_var.h>
#include <netiso/cltp_var.h>
#include <netiso/idrp_var.h>

#ifdef TUBA
#include <netiso/tuba_table.h>
#endif

#include <sys/stdarg.h>

/*
 * CALLED FROM:
 * 	pr_usrreq() on PRU_BIND, PRU_CONNECT, PRU_ACCEPT, and PRU_PEERADDR
 * FUNCTION, ARGUMENTS:
 * 	The argument (which) takes the value TP_LOCAL or TP_FOREIGN.
 */

void
iso_getsufx(v, lenp, data_out, which)
	void	       *v;
	u_short        *lenp;
	caddr_t         data_out;
	int             which;
{
	struct isopcb  *isop = v;
	struct sockaddr_iso *addr = 0;

	switch (which) {
	case TP_LOCAL:
		addr = isop->isop_laddr;
		break;

	case TP_FOREIGN:
		addr = isop->isop_faddr;
	}
	if (addr)
		bcopy(TSEL(addr), data_out, (*lenp = addr->siso_tlen));
}

/*
 * CALLED FROM: tp_newsocket(); i.e., when a connection is being established
 * by an incoming CR_TPDU.
 *
 * FUNCTION, ARGUMENTS: Put a transport suffix (found in name) into an isopcb
 * structure (isop). The argument (which) takes the value TP_LOCAL or
 * TP_FOREIGN.
 */
void
iso_putsufx(v, sufxloc, sufxlen, which)
	void	       *v;
	caddr_t         sufxloc;
	int             sufxlen, which;
{
	struct isopcb  *isop = v;
	struct sockaddr_iso **dst, *backup;
	struct sockaddr_iso *addr;
	struct mbuf    *m;
	int             len;

	switch (which) {
	default:
		return;

	case TP_LOCAL:
		dst = &isop->isop_laddr;
		backup = &isop->isop_sladdr;
		break;

	case TP_FOREIGN:
		dst = &isop->isop_faddr;
		backup = &isop->isop_sfaddr;
	}
	if ((addr = *dst) == 0) {
		addr = *dst = backup;
		addr->siso_nlen = 0;
		addr->siso_slen = 0;
		addr->siso_plen = 0;
		printf("iso_putsufx on un-initialized isopcb\n");
	}
	len = sufxlen + addr->siso_nlen +
		(sizeof(*addr) - sizeof(addr->siso_data));
	if (addr == backup) {
		if (len > sizeof(*addr)) {
			m = m_getclr(M_DONTWAIT, MT_SONAME);
			if (m == 0)
				return;
			addr = *dst = mtod(m, struct sockaddr_iso *);
			*addr = *backup;
			m->m_len = len;
		}
	}
	bcopy(sufxloc, TSEL(addr), sufxlen);
	addr->siso_tlen = sufxlen;
	addr->siso_len = len;
}

/*
 * CALLED FROM:
 * 	tp.trans whenever we go into REFWAIT state.
 * FUNCTION and ARGUMENT:
 *	 Called when a ref is frozen, to allow the suffix to be reused.
 * 	(isop) is the net level pcb.  This really shouldn't have to be
 * 	done in a NET level pcb but... for the internet world that just
 * 	the way it is done in BSD...
 * 	The alternative is to have the port unusable until the reference
 * 	timer goes off.
 */
void
iso_recycle_tsuffix(v)
	void *v;
{
	struct isopcb *isop = v;
	isop->isop_laddr->siso_tlen = isop->isop_faddr->siso_tlen = 0;
}

/*
 * CALLED FROM:
 * 	tp_newsocket(); i.e., when a connection is being established by an
 * 	incoming CR_TPDU.
 *
 * FUNCTION and ARGUMENTS:
 * 	Copy a whole net addr from a struct sockaddr (name).
 * 	into an isopcb (isop).
 * 	The argument (which) takes values TP_LOCAL or TP_FOREIGN
 */
void
iso_putnetaddr(v, nm, which)
	void *v;
	struct sockaddr *nm;
	int             which;
{
	struct isopcb *isop = v;
	struct sockaddr_iso *name = (struct sockaddr_iso *) nm;
	struct sockaddr_iso **sisop, *backup;
	struct sockaddr_iso *siso;

	switch (which) {
	default:
		printf("iso_putnetaddr: should panic\n");
		return;
	case TP_LOCAL:
		sisop = &isop->isop_laddr;
		backup = &isop->isop_sladdr;
		break;
	case TP_FOREIGN:
		sisop = &isop->isop_faddr;
		backup = &isop->isop_sfaddr;
	}
	siso = ((*sisop == 0) ? (*sisop = backup) : *sisop);
#ifdef ARGO_DEBUG
	if (argo_debug[D_TPISO]) {
		printf("ISO_PUTNETADDR\n");
		dump_isoaddr(isop->isop_faddr);
	}
#endif
	siso->siso_addr = name->siso_addr;
}

/*
 * CALLED FROM:
 * 	tp_input() when a connection is being established by an
 * 	incoming CR_TPDU, and considered for interception.
 *
 * FUNCTION and ARGUMENTS:
 * 	compare a whole net addr from a struct sockaddr (name),
 * 	with that implicitly stored in an isopcb (isop).
 * 	The argument (which) takes values TP_LOCAL or TP_FOREIGN.
 */
int
iso_cmpnetaddr(v, nm, which)
	void *v;
	struct sockaddr *nm;
	int             which;
{
	struct isopcb *isop = v;
	struct sockaddr_iso *name = (struct sockaddr_iso *) nm;
	struct sockaddr_iso **sisop, *backup;
	struct sockaddr_iso *siso;

	switch (which) {
	default:
		printf("iso_cmpnetaddr: should panic\n");
		return 0;
	case TP_LOCAL:
		sisop = &isop->isop_laddr;
		backup = &isop->isop_sladdr;
		break;
	case TP_FOREIGN:
		sisop = &isop->isop_faddr;
		backup = &isop->isop_sfaddr;
	}
	siso = ((*sisop == 0) ? (*sisop = backup) : *sisop);
#ifdef ARGO_DEBUG
	if (argo_debug[D_TPISO]) {
		printf("ISO_CMPNETADDR\n");
		dump_isoaddr(siso);
	}
#endif
	if (name->siso_tlen && bcmp(TSEL(name), TSEL(siso), name->siso_tlen))
		return (0);
	return (bcmp((caddr_t) name->siso_data,
		     (caddr_t) siso->siso_data, name->siso_nlen) == 0);
}

/*
 * CALLED FROM:
 *  pr_usrreq() PRU_SOCKADDR, PRU_ACCEPT, PRU_PEERADDR
 * FUNCTION and ARGUMENTS:
 * 	Copy a whole net addr from an isopcb (isop) into
 * 	a struct sockaddr (name).
 * 	The argument (which) takes values TP_LOCAL or TP_FOREIGN.
 */

void
iso_getnetaddr(v, name, which)
	void *v;
	struct mbuf    *name;
	int             which;
{
	struct inpcb *inp = v;
	struct isopcb *isop = (struct isopcb *) inp;
	struct sockaddr_iso *siso =
	(which == TP_LOCAL ? isop->isop_laddr : isop->isop_faddr);
	if (siso)
		bcopy((caddr_t) siso, mtod(name, caddr_t),
		      (unsigned) (name->m_len = siso->siso_len));
	else
		name->m_len = 0;
}
/*
 * NAME: 	tpclnp_mtu()
 *
 * CALLED FROM:
 *  tp_route_to() on incoming CR, CC, and pr_usrreq() for PRU_CONNECT
 *
 * FUNCTION, ARGUMENTS, and RETURN VALUE:
 *
 * Perform subnetwork dependent part of determining MTU information.
 * It appears that setting a double pointer to the rtentry associated with
 * the destination, and returning the header size for the network protocol
 * suffices.
 *
 * SIDE EFFECTS:
 * Sets tp_routep pointer in pcb.
 *
 * NOTES:
 */
int
tpclnp_mtu(v)
	void *v;
{
	struct tp_pcb *tpcb = v;
	struct isopcb  *isop = (struct isopcb *) tpcb->tp_npcb;

#ifdef ARGO_DEBUG
	if (argo_debug[D_CONN]) {
		printf("tpclnp_mtu(tpcb %p)\n", tpcb);
	}
#endif
	tpcb->tp_routep = &(isop->isop_route.ro_rt);
	if (tpcb->tp_netservice == ISO_CONS)
		return 0;
	else
		return (sizeof(struct clnp_fixed) + sizeof(struct clnp_segment) +
			2 * sizeof(struct iso_addr));

}

/*
 * CALLED FROM:
 *  tp_emit()
 * FUNCTION and ARGUMENTS:
 *  Take a packet(m0) from tp and package it so that clnp will accept it.
 *  This means prepending space for the clnp header and filling in a few
 *  of the fields.
 *  isop is the isopcb structure; datalen is the length of the data in the
 *  mbuf string m0.
 * RETURN VALUE:
 *  whatever (E*) is returned form the net layer output routine.
 */

int
tpclnp_output(struct mbuf *m0, ...)
{
	int             datalen;
	struct isopcb  *isop;
	int             nochksum;
	va_list ap;

	va_start(ap, m0);
	datalen = va_arg(ap, int);
	isop = va_arg(ap, struct isopcb *);
	nochksum = va_arg(ap, int);
	va_end(ap);

	IncStat(ts_tpdu_sent);

#ifdef ARGO_DEBUG
	if (argo_debug[D_TPISO]) {
		struct tpdu    *hdr = mtod(m0, struct tpdu *);

		printf(
		       "abt to call clnp_output: datalen 0x%x, hdr.li 0x%x, hdr.dutype 0x%x nocsum x%x dst addr:\n",
		       datalen,
		       (int) hdr->tpdu_li, (int) hdr->tpdu_type, nochksum);
		dump_isoaddr(isop->isop_faddr);
		printf("\nsrc addr:\n");
		dump_isoaddr(isop->isop_laddr);
		dump_mbuf(m0, "at tpclnp_output");
	}
#endif

	return
		clnp_output(m0, isop, datalen, /* flags */ nochksum ? CLNP_NO_CKSUM : 0);
}

/*
 * CALLED FROM:
 *  tp_error_emit()
 * FUNCTION and ARGUMENTS:
 *  This is a copy of tpclnp_output that takes the addresses
 *  instead of a pcb.  It's used by the tp_error_emit, when we
 *  don't have an iso_pcb with which to call the normal output rtn.
 * RETURN VALUE:
 *  ENOBUFS or
 *  whatever (E*) is returned form the net layer output routine.
 */

int
tpclnp_output_dg(struct mbuf *m0, ...)
{
	struct isopcb   tmppcb;
	int             err;
	int             flags;
	int             datalen;
	struct iso_addr *laddr, *faddr;
	struct route   *ro;
	int             nochksum;
	va_list		ap;

	va_start(ap, m0);
	datalen = va_arg(ap, int);
	laddr = va_arg(ap, struct iso_addr *);
	faddr = va_arg(ap, struct iso_addr *);
	ro = va_arg(ap, struct route *);
	nochksum = va_arg(ap, int);
	va_end(ap);

#ifdef ARGO_DEBUG
	if (argo_debug[D_TPISO]) {
		printf("tpclnp_output_dg  datalen 0x%x m0 %p\n", datalen, m0);
	}
#endif

	/*
	 *	Fill in minimal portion of isopcb so that clnp can send the
	 *	packet.
	 */
	bzero((caddr_t) & tmppcb, sizeof(tmppcb));
	tmppcb.isop_laddr = &tmppcb.isop_sladdr;
	tmppcb.isop_laddr->siso_addr = *laddr;
	tmppcb.isop_faddr = &tmppcb.isop_sfaddr;
	tmppcb.isop_faddr->siso_addr = *faddr;

#ifdef ARGO_DEBUG
	if (argo_debug[D_TPISO]) {
		printf("tpclnp_output_dg  faddr: \n");
		dump_isoaddr(&tmppcb.isop_sfaddr);
		printf("\ntpclnp_output_dg  laddr: \n");
		dump_isoaddr(&tmppcb.isop_sladdr);
		printf("\n");
	}
#endif

	/*
	 *	Do not use packet cache since this is a one shot error packet
	 */
	flags = (CLNP_NOCACHE | (nochksum ? CLNP_NO_CKSUM : 0));

	IncStat(ts_tpdu_sent);

	err = clnp_output(m0, &tmppcb, datalen, flags);

	/*
	 *	Free route allocated by clnp (if the route was indeed allocated)
	 */
	if (tmppcb.isop_route.ro_rt)
		RTFREE(tmppcb.isop_route.ro_rt);

	return (err);
}
/*
 * CALLED FROM:
 * 	clnp's input routine, indirectly through the protosw.
 * FUNCTION and ARGUMENTS:
 * Take a packet (m) from clnp, strip off the clnp header and give it to tp
 * No return value.
 */
void
tpclnp_input(struct mbuf *m, ...)
{
	struct sockaddr_iso *src, *dst;
	int             clnp_len, ce_bit;
	void            (*input)(struct mbuf *, ...) = tp_input;
	va_list		ap;

	va_start(ap, m);
	src = va_arg(ap, struct sockaddr_iso *);
	dst = va_arg(ap, struct sockaddr_iso *);
	clnp_len = va_arg(ap, int);
	ce_bit = va_arg(ap, int);
	va_end(ap);

	IncStat(ts_pkt_rcvd);

#ifdef ARGO_DEBUG
	if (argo_debug[D_TPINPUT]) {
		printf("tpclnp_input: m %p clnp_len 0x%x\n", m, clnp_len);
		dump_mbuf(m, "at tpclnp_input");
	}
#endif
	/*
	 * CLNP gives us an mbuf chain WITH the clnp header pulled up,
	 * and the length of the clnp header.
	 * First, strip off the Clnp header. leave the mbuf there for the
	 * pullup that follows.
	 */
	m->m_len -= clnp_len;
	m->m_data += clnp_len;
	m->m_pkthdr.len -= clnp_len;
	/* XXXX: should probably be in clnp_input */
	switch (dst->siso_data[dst->siso_nlen - 1]) {
#ifdef TUBA
	case ISOPROTO_TCP:
		tuba_tcpinput(m, src, dst);
		return;
#endif
	case 0:
		if (m->m_len == 0 && (m = m_pullup(m, 1)) == 0)
			return;
		if (*(mtod(m, u_char *)) == ISO10747_IDRP) {
			idrp_input(m, src, dst);
			return;
		}
	}
	m = tp_inputprep(m);
	if (m == 0)
		return;
	if (mtod(m, u_char *)[1] == UD_TPDU_type)
		input = cltp_input;

#ifdef ARGO_DEBUG
	if (argo_debug[D_TPINPUT]) {
		dump_mbuf(m, "after tpclnp_input both pullups");
	}
#endif

#ifdef ARGO_DEBUG
	if (argo_debug[D_TPISO]) {
		printf("calling %sinput : src %p, dst %p, src addr:\n",
		       (input == tp_input ? "tp_" : "clts_"), src, dst);
		dump_isoaddr(src);
		printf(" dst addr:\n");
		dump_isoaddr(dst);
	}
#endif

	(*input) (m, (struct sockaddr *) src, (struct sockaddr *) dst, 0,
		  tpclnp_output_dg, ce_bit);

#ifdef ARGO_DEBUG
	if (argo_debug[D_QUENCH]) {{
			if (time.tv_usec & 0x4 && time.tv_usec & 0x40) {
				printf("tpclnp_input: FAKING %s\n",
				       tp_stat.ts_pkt_rcvd & 0x1 ? "QUENCH" : "QUENCH2");
				if (tp_stat.ts_pkt_rcvd & 0x1)
					tpclnp_ctlinput(PRC_QUENCH, 
							(struct sockaddr *)
							&src, NULL);
				else
					tpclnp_ctlinput(PRC_QUENCH2,
							(struct sockaddr *)
							&src, NULL);
			}
	}
	}
#endif
}

/*ARGSUSED*/
void
iso_rtchange(pcb)
	struct isopcb *pcb;
{

}

/*
 * CALLED FROM:
 *  tpclnp_ctlinput()
 * FUNCTION and ARGUMENTS:
 *  find the tpcb pointer and pass it to tp_quench
 */
void
tpiso_decbit(isop)
	struct isopcb  *isop;
{
	tp_quench((struct inpcb *) isop->isop_socket->so_pcb, PRC_QUENCH2);
}
/*
 * CALLED FROM:
 *  tpclnp_ctlinput()
 * FUNCTION and ARGUMENTS:
 *  find the tpcb pointer and pass it to tp_quench
 */
void
tpiso_quench(isop)
	struct isopcb  *isop;
{
	tp_quench((struct inpcb *) isop->isop_socket->so_pcb, PRC_QUENCH);
}

/*
 * CALLED FROM:
 *  The network layer through the protosw table.
 * FUNCTION and ARGUMENTS:
 *	When clnp an ICMP-like msg this gets called.
 *	It either returns an error status to the user or
 *	it causes all connections on this address to be aborted
 *	by calling the appropriate xx_notify() routine.
 *	(cmd) is the type of ICMP error.
 * 	(siso) is the address of the guy who sent the ER CLNPDU
 */
void *
tpclnp_ctlinput(cmd, saddr, dummy)
	int             cmd;
	struct sockaddr *saddr;
	void *dummy;
{
	struct sockaddr_iso *siso = (struct sockaddr_iso *) saddr;
	extern u_char   inetctlerrmap[];

#ifdef ARGO_DEBUG
	if (argo_debug[D_TPINPUT]) {
		printf("tpclnp_ctlinput1: cmd 0x%x addr: \n", cmd);
		dump_isoaddr(siso);
	}
#endif

	if (cmd < 0 || cmd >= PRC_NCMDS)
		return NULL;
	if (siso->siso_family != AF_ISO)
		return NULL;
	switch (cmd) {

	case PRC_QUENCH2:
		iso_pcbnotify(&tp_isopcb, siso, 0, tpiso_decbit);
		break;

	case PRC_QUENCH:
		iso_pcbnotify(&tp_isopcb, siso, 0, tpiso_quench);
		break;

	case PRC_TIMXCEED_REASS:
	case PRC_ROUTEDEAD:
		iso_pcbnotify(&tp_isopcb, siso, 0, tpiso_reset);
		break;

	case PRC_HOSTUNREACH:
	case PRC_UNREACH_NET:
	case PRC_IFDOWN:
	case PRC_HOSTDEAD:
		iso_pcbnotify(&tp_isopcb, siso,
			      (int) inetctlerrmap[cmd], iso_rtchange);
		break;

	default:
		/*
		case	PRC_MSGSIZE:
		case	PRC_UNREACH_HOST:
		case	PRC_UNREACH_PROTOCOL:
		case	PRC_UNREACH_PORT:
		case	PRC_UNREACH_NEEDFRAG:
		case	PRC_UNREACH_SRCFAIL:
		case	PRC_REDIRECT_NET:
		case	PRC_REDIRECT_HOST:
		case	PRC_REDIRECT_TOSNET:
		case	PRC_REDIRECT_TOSHOST:
		case	PRC_TIMXCEED_INTRANS:
		case	PRC_PARAMPROB:
		*/
		iso_pcbnotify(&tp_isopcb, siso, (int) inetctlerrmap[cmd], tpiso_abort);
		break;
	}
	return NULL;
}
/*
 * XXX - Variant which is called by clnp_er.c with an isoaddr rather
 * than a sockaddr_iso.
 */

static struct sockaddr_iso siso = {sizeof(siso), AF_ISO};
void
tpclnp_ctlinput1(cmd, isoa)
	int             cmd;
	struct iso_addr *isoa;
{
	bzero((caddr_t) & siso.siso_addr, sizeof(siso.siso_addr));
	bcopy((caddr_t) isoa, (caddr_t) & siso.siso_addr, isoa->isoa_len);
	tpclnp_ctlinput(cmd, (struct sockaddr *) &siso, NULL);
}

/*
 * These next 2 routines are
 * CALLED FROM:
 *	xxx_notify() from tp_ctlinput() when
 *  net level gets some ICMP-equiv. type event.
 * FUNCTION and ARGUMENTS:
 *  Cause the connection to be aborted with some sort of error
 *  reason indicating that the network layer caused the abort.
 *  Fakes an ER TPDU so we can go through the driver.
 *  abort always aborts the TP connection.
 *  reset may or may not, depending on the TP class that's in use.
 */
void
tpiso_abort(isop)
	struct isopcb  *isop;
{
	struct tp_event e;

#ifdef ARGO_DEBUG
	if (argo_debug[D_CONN]) {
		printf("tpiso_abort %p\n", isop);
	}
#endif
	e.ev_number = ER_TPDU;
	e.TPDU_ATTR(ER).e_reason = ECONNABORTED;
	tp_driver((struct tp_pcb *) isop->isop_socket->so_pcb, &e);
}

void
tpiso_reset(isop)
	struct isopcb  *isop;
{
	struct tp_event e;

	e.ev_number = T_NETRESET;
	tp_driver((struct tp_pcb *) isop->isop_socket->so_pcb, &e);

}

#endif				/* ISO */
