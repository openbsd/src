/*	$OpenBSD: iso_pcb.c,v 1.7 2003/12/10 07:22:44 itojun Exp $	*/
/*	$NetBSD: iso_pcb.c,v 1.10 1996/04/13 01:34:56 cgd Exp $	*/

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
 *	@(#)iso_pcb.c	8.1 (Berkeley) 6/10/93
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
 * Iso address family net-layer(s) pcb stuff. NEH 1/29/87
 */

#ifdef ISO

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>

#include <netiso/argo_debug.h>
#include <netiso/iso.h>
#include <netiso/clnp.h>
#include <netinet/in_systm.h>
#include <net/if.h>
#include <net/route.h>
#include <netiso/iso_pcb.h>
#include <netiso/iso_var.h>
#include <sys/protosw.h>

#ifdef TPCONS
#include <netccitt/x25.h>
#include <netccitt/pk.h>
#include <netccitt/pk_var.h>
#include <netccitt/pk_extern.h>
#endif

#define PCBNULL (struct isopcb *)0
struct iso_addr zeroiso_addr = {
	0
};


/*
 * FUNCTION:		iso_pcballoc
 *
 * PURPOSE:		creates an isopcb structure in an mbuf,
 *			with socket (so), and
 *			puts it in the queue with head (head)
 *
 * RETURNS:		0 if OK, ENOBUFS if can't alloc the necessary mbuf
 */
int
iso_pcballoc(so, v)
	struct socket  *so;
	void *v;
{
	struct isopcb  *head = v;
	struct isopcb *isop;

#ifdef ARGO_DEBUG
	if (argo_debug[D_ISO]) {
		printf("iso_pcballoc(so %p)\n", so);
	}
#endif
	MALLOC(isop, struct isopcb *, sizeof(*isop), M_PCB, M_NOWAIT);
	if (isop == NULL)
		return ENOBUFS;
	bzero(isop, sizeof(*isop));
	isop->isop_head = head;
	isop->isop_socket = so;
	insque(isop, head);
	if (so)
		so->so_pcb = isop;
	return 0;
}

/*
 * FUNCTION:		iso_pcbbind
 *
 * PURPOSE:		binds the address given in *(nam) to the socket
 *			specified by the isopcb in *(isop)
 *			If the given address is zero, it makes sure the
 *			address isn't already in use and if it's got a network
 *			portion, we look for an interface with that network
 *			address.  If the address given is zero, we allocate
 *			a port and stuff it in the (nam) structure.
 *
 * RETURNS:		errno E* or 0 if ok.
 *
 * SIDE EFFECTS:	increments head->isop_lport if it allocates a port #
 *
 * NOTES:
 */
int
iso_pcbbind(v, nam)
	void *v;
	struct mbuf    *nam;
{
	struct isopcb *isop = v;
	struct isopcb *head = isop->isop_head;
	struct sockaddr_iso *siso;
	struct iso_ifaddr *ia;
	union {
		char            data[2];
		u_short         s;
	} suf;

#ifdef ARGO_DEBUG
	if (argo_debug[D_ISO]) {
		printf("iso_pcbbind(isop %p, nam %p)\n", isop, nam);
	}
#endif
	suf.s = 0;
	if (iso_ifaddr.tqh_first == 0)	/* any interfaces attached? */
		return EADDRNOTAVAIL;
	if (isop->isop_laddr)	/* already bound */
		return EADDRINUSE;
	if (nam == (struct mbuf *) 0) {
		isop->isop_laddr = &isop->isop_sladdr;
		isop->isop_sladdr.siso_len = sizeof(struct sockaddr_iso);
		isop->isop_sladdr.siso_family = AF_ISO;
		isop->isop_sladdr.siso_tlen = 2;
		isop->isop_sladdr.siso_nlen = 0;
		isop->isop_sladdr.siso_slen = 0;
		isop->isop_sladdr.siso_plen = 0;
		goto noname;
	}
	siso = mtod(nam, struct sockaddr_iso *);
#ifdef ARGO_DEBUG
	if (argo_debug[D_ISO]) {
		printf("iso_pcbbind(name len 0x%x)\n", nam->m_len);
		printf("The address is %s\n", clnp_iso_addrp(&siso->siso_addr));
	}
#endif
	/*
	 * We would like sort of length check but since some OSI addrs
	 * do not have fixed length, we can't really do much.
	 * The ONLY thing we can say is that an osi addr has to have
	 * at LEAST an afi and one more byte and had better fit into
	 * a struct iso_addr.
	 * However, in fact the size of the whole thing is a struct
	 * sockaddr_iso, so probably this is what we should check for.
	 */
	if ((nam->m_len < 2) || (nam->m_len < siso->siso_len)) {
		return ENAMETOOLONG;
	}
	if (siso->siso_nlen) {
		/* non-zero net addr- better match one of our interfaces */
#ifdef ARGO_DEBUG
		if (argo_debug[D_ISO]) {
			printf("iso_pcbbind: bind to NOT zeroisoaddr\n");
		}
#endif
		for (ia = iso_ifaddr.tqh_first; ia != 0; ia = ia->ia_list.tqe_next)
			if (SAME_ISOADDR(siso, &ia->ia_addr))
				break;
		if (ia == 0)
			return EADDRNOTAVAIL;
	}
	if (siso->siso_len <= sizeof(isop->isop_sladdr)) {
		isop->isop_laddr = &isop->isop_sladdr;
	} else {
		if ((nam = m_copy(nam, 0, (int) M_COPYALL)) == 0)
			return ENOBUFS;
		isop->isop_laddr = mtod(nam, struct sockaddr_iso *);
	}
	bcopy((caddr_t) siso, (caddr_t) isop->isop_laddr, siso->siso_len);
	if (siso->siso_tlen == 0)
		goto noname;
	if ((isop->isop_socket->so_options & SO_REUSEADDR) == 0 &&
	    iso_pcblookup(head, 0, (caddr_t) 0, isop->isop_laddr))
		return EADDRINUSE;
	if (siso->siso_tlen <= 2) {
		bcopy(TSEL(siso), suf.data, sizeof(suf.data));
		suf.s = ntohs(suf.s);
		if ((suf.s < ISO_PORT_RESERVED) &&
		    (isop->isop_socket->so_state & SS_PRIV) == 0)
			return EACCES;
	} else {
		char  *cp;
noname:
		cp = TSEL(isop->isop_laddr);
#ifdef ARGO_DEBUG
		if (argo_debug[D_ISO]) {
			printf("iso_pcbbind noname\n");
		}
#endif
		do {
			if (head->isop_lport++ < ISO_PORT_RESERVED ||
			    head->isop_lport > ISO_PORT_USERRESERVED)
				head->isop_lport = ISO_PORT_RESERVED;
			suf.s = htons(head->isop_lport);
			cp[0] = suf.data[0];
			cp[1] = suf.data[1];
		} while (iso_pcblookup(head, 0, (caddr_t) 0, isop->isop_laddr));
	}
#ifdef ARGO_DEBUG
	if (argo_debug[D_ISO]) {
		printf("iso_pcbbind returns 0, suf 0x%x\n", suf.s);
	}
#endif
	return 0;
}
/*
 * FUNCTION:		iso_pcbconnect
 *
 * PURPOSE:		Make the isopcb (isop) look like it's connected.
 *			In other words, give it the peer address given in
 *			the mbuf * (nam).   Make sure such a combination
 *			of local, peer addresses doesn't already exist
 *			for this protocol.  Internet mentality prevails here,
 *			wherein a src,dst pair uniquely identifies a connection.
 * 			Both net address and port must be specified in argument
 *			(nam).
 * 			If we don't have a local address for this socket yet,
 *			we pick one by calling iso_pcbbind().
 *
 * RETURNS:		errno E* or 0 if ok.
 *
 * SIDE EFFECTS:	Looks up a route, which may cause one to be left
 *			in the isopcb.
 *
 * NOTES:
 */
int
iso_pcbconnect(v, nam)
	void *v;
	struct mbuf    *nam;
{
	struct isopcb *isop = v;
	struct sockaddr_iso *siso = mtod(nam, struct sockaddr_iso *);
	int             local_zero, error = 0;
	struct iso_ifaddr *ia;

#ifdef ARGO_DEBUG
	if (argo_debug[D_ISO]) {
		printf("iso_pcbconnect(isop %p sock %p nam %p",
		       isop, isop->isop_socket, nam);
		printf("nam->m_len 0x%x), addr:\n", nam->m_len);
		dump_isoaddr(siso);
	}
#endif
	if (nam->m_len < siso->siso_len)
		return EINVAL;
	if (siso->siso_family != AF_ISO)
		return EAFNOSUPPORT;
	if (siso->siso_nlen == 0) {
		if ((ia = iso_ifaddr.tqh_first) != NULL) {
			int             nlen = ia->ia_addr.siso_nlen;
			ovbcopy(TSEL(siso), nlen + TSEL(siso),
			siso->siso_plen + siso->siso_tlen + siso->siso_slen);
			bcopy((caddr_t) & ia->ia_addr.siso_addr,
			      (caddr_t) & siso->siso_addr, nlen + 1);
			/* includes siso->siso_nlen = nlen; */
		} else
			return EADDRNOTAVAIL;
	}
	/*
	 * Local zero means either not bound, or bound to a TSEL, but no
	 * particular local interface.  So, if we want to send somebody
	 * we need to choose a return address.
	 */
	local_zero =
		((isop->isop_laddr == 0) || (isop->isop_laddr->siso_nlen == 0));
	if (local_zero) {
		int             flags;

#ifdef ARGO_DEBUG
		if (argo_debug[D_ISO]) {
			printf("iso_pcbconnect localzero 1\n");
		}
#endif
		/*
		 * If route is known or can be allocated now, our src addr is
		 * taken from the i/f, else punt.
		 */
		flags = isop->isop_socket->so_options & SO_DONTROUTE;
		error = clnp_route(&siso->siso_addr, &isop->isop_route, flags,
				   NULL, &ia);
		if (error)
			return error;
#ifdef ARGO_DEBUG
		if (argo_debug[D_ISO]) {
			printf("iso_pcbconnect localzero 2, ro->ro_rt %p",
			       isop->isop_route.ro_rt);
			printf(" ia %p\n", ia);
		}
#endif
	}
#ifdef ARGO_DEBUG
	if (argo_debug[D_ISO]) {
		printf("in iso_pcbconnect before lookup isop %p isop->sock %p\n",
		       isop, isop->isop_socket);
	}
#endif
	if (local_zero) {
		int             nlen, tlen, totlen;
		caddr_t         oldtsel, newtsel;
		siso = isop->isop_laddr;
		if (siso == 0 || siso->siso_tlen == 0)
			(void) iso_pcbbind(isop, NULL);
		/*
		 * Here we have problem of squezeing in a definite network address
		 * into an existing sockaddr_iso, which in fact may not have room
		 * for it.  This gets messy.
		 */
		siso = isop->isop_laddr;
		oldtsel = TSEL(siso);
		tlen = siso->siso_tlen;
		nlen = ia->ia_addr.siso_nlen;
		totlen = tlen + nlen + _offsetof(struct sockaddr_iso, siso_data[0]);
		if ((siso == &isop->isop_sladdr) &&
		    (totlen > sizeof(isop->isop_sladdr))) {
			struct mbuf    *m = m_get(M_DONTWAIT, MT_SONAME);
			if (m == 0)
				return ENOBUFS;
			m->m_len = totlen;
			isop->isop_laddr = siso = mtod(m, struct sockaddr_iso *);
		}
		siso->siso_nlen = ia->ia_addr.siso_nlen;
		newtsel = TSEL(siso);
		ovbcopy(oldtsel, newtsel, tlen);
		bcopy(ia->ia_addr.siso_data, siso->siso_data, nlen);
		siso->siso_tlen = tlen;
		siso->siso_family = AF_ISO;
		siso->siso_len = totlen;
		siso = mtod(nam, struct sockaddr_iso *);
	}
#ifdef ARGO_DEBUG
	if (argo_debug[D_ISO]) {
		printf("in iso_pcbconnect before bcopy isop %p isop->sock %p\n",
		       isop, isop->isop_socket);
	}
#endif
	/*
	 * If we had to allocate space to a previous big foreign address,
	 * and for some reason we didn't free it, we reuse it knowing
	 * that is going to be big enough, as sockaddrs are delivered in
	 * 128 byte mbufs.
	 * If the foreign address is small enough, we use default space;
	 * otherwise, we grab an mbuf to copy into.
	 */
	if (isop->isop_faddr == 0 || isop->isop_faddr == &isop->isop_sfaddr) {
		if (siso->siso_len <= sizeof(isop->isop_sfaddr))
			isop->isop_faddr = &isop->isop_sfaddr;
		else {
			struct mbuf    *m = m_get(M_DONTWAIT, MT_SONAME);
			if (m == 0)
				return ENOBUFS;
			isop->isop_faddr = mtod(m, struct sockaddr_iso *);
		}
	}
	bcopy((caddr_t) siso, (caddr_t) isop->isop_faddr, siso->siso_len);
#ifdef ARGO_DEBUG
	if (argo_debug[D_ISO]) {
		printf("in iso_pcbconnect after bcopy isop %p isop->sock %p\n",
		       isop, isop->isop_socket);
		printf("iso_pcbconnect connected to addr:\n");
		dump_isoaddr(isop->isop_faddr);
		printf("iso_pcbconnect end: src addr:\n");
		dump_isoaddr(isop->isop_laddr);
	}
#endif
	return 0;
}

/*
 * FUNCTION:		iso_pcbdisconnect()
 *
 * PURPOSE:		washes away the peer address info so the socket
 *			appears to be disconnected.
 *			If there's no file descriptor associated with the socket
 *			it detaches the pcb.
 *
 * RETURNS:		Nada.
 *
 * SIDE EFFECTS:	May detach the pcb.
 *
 * NOTES:
 */
void
iso_pcbdisconnect(v)
	void *v;
{
	struct isopcb  *isop = v;
	struct sockaddr_iso *siso;

#ifdef ARGO_DEBUG
	if (argo_debug[D_ISO]) {
		printf("iso_pcbdisconnect(isop %p)\n", isop);
	}
#endif
	/*
	 * Preserver binding infnormation if already bound.
	 */
	if ((siso = isop->isop_laddr) && siso->siso_nlen && siso->siso_tlen) {
		caddr_t         otsel = TSEL(siso);
		siso->siso_nlen = 0;
		ovbcopy(otsel, TSEL(siso), siso->siso_tlen);
	}
	if (isop->isop_faddr && isop->isop_faddr != &isop->isop_sfaddr)
		m_freem(dtom(isop->isop_faddr));
	isop->isop_faddr = 0;
	if (isop->isop_socket->so_state & SS_NOFDREF)
		iso_pcbdetach(isop);
}

/*
 * FUNCTION:		iso_pcbdetach
 *
 * PURPOSE:		detach the pcb at *(isop) from it's socket and free
 *			the mbufs associated with the pcb..
 *			Dequeues (isop) from its head.
 *
 * RETURNS:		Nada.
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
void
iso_pcbdetach(v)
	void *v;
{
	struct isopcb  *isop = v;
	struct socket  *so = isop->isop_socket;

#ifdef ARGO_DEBUG
	if (argo_debug[D_ISO]) {
		printf("iso_pcbdetach(isop %p socket %p so %p)\n",
		       isop, isop->isop_socket, so);
	}
#endif
#ifdef TPCONS
	if (isop->isop_chan) {
		struct pklcd *lcp = (struct pklcd *) isop->isop_chan;
		if (--isop->isop_refcnt > 0)
			return;
		if (lcp && lcp->lcd_state == DATA_TRANSFER) {
			lcp->lcd_upper = 0;
			lcp->lcd_upnext = 0;
			pk_disconnect(lcp);
		}
		isop->isop_chan = 0;
	}
#endif
	if (so) {		/* in the x.25 domain, we sometimes have no
				 * socket */
		so->so_pcb = 0;
		sofree(so);
	}
#ifdef ARGO_DEBUG
	if (argo_debug[D_ISO]) {
		printf("iso_pcbdetach 2 \n");
	}
#endif
	if (isop->isop_options)
		(void) m_free(isop->isop_options);
#ifdef ARGO_DEBUG
	if (argo_debug[D_ISO]) {
		printf("iso_pcbdetach 3 \n");
	}
#endif
	if (isop->isop_route.ro_rt)
		rtfree(isop->isop_route.ro_rt);
#ifdef ARGO_DEBUG
	if (argo_debug[D_ISO]) {
		printf("iso_pcbdetach 3.1\n");
	}
#endif
	if (isop->isop_clnpcache != NULL) {
		struct clnp_cache *clcp =
		mtod(isop->isop_clnpcache, struct clnp_cache *);
#ifdef ARGO_DEBUG
		if (argo_debug[D_ISO]) {
			printf("iso_pcbdetach 3.2: clcp %p freeing clc_hdr %p\n",
			       clcp, clcp->clc_hdr);
		}
#endif
		if (clcp->clc_hdr != NULL)
			m_free(clcp->clc_hdr);
#ifdef ARGO_DEBUG
		if (argo_debug[D_ISO]) {
			printf("iso_pcbdetach 3.3: freeing cache %p\n",
			       isop->isop_clnpcache);
		}
#endif
		m_free(isop->isop_clnpcache);
	}
#ifdef ARGO_DEBUG
	if (argo_debug[D_ISO]) {
		printf("iso_pcbdetach 4 \n");
	}
#endif
	remque(isop);
#ifdef ARGO_DEBUG
	if (argo_debug[D_ISO]) {
		printf("iso_pcbdetach 5 \n");
	}
#endif
	if (isop->isop_laddr && (isop->isop_laddr != &isop->isop_sladdr))
		m_freem(dtom(isop->isop_laddr));
	free((caddr_t) isop, M_PCB);
}


/*
 * FUNCTION:		iso_pcbnotify
 *
 * PURPOSE:		notify all connections in this protocol's queue (head)
 *			that have peer address (dst) of the problem (errno)
 *			by calling (notify) on the connections' isopcbs.
 *
 * RETURNS:		Rien.
 *
 * SIDE EFFECTS:
 *
 * NOTES:		(notify) is called at splimp!
 */
void
iso_pcbnotify(head, siso, errno, notify)
	struct isopcb  *head;
	struct sockaddr_iso *siso;
	int             errno;
	void (*notify)(struct isopcb *);
{
	struct isopcb *isop;
	int             s = splimp();

#ifdef ARGO_DEBUG
	if (argo_debug[D_ISO]) {
		printf("iso_pcbnotify(head %p, notify %p) dst:\n",
			head, notify);
	}
#endif
	for (isop = head->isop_next; isop != head; isop = isop->isop_next) {
		if (isop->isop_socket == 0 || isop->isop_faddr == 0 ||
		    !SAME_ISOADDR(siso, isop->isop_faddr)) {
#ifdef ARGO_DEBUG
			if (argo_debug[D_ISO]) {
				printf("iso_pcbnotify: CONTINUE isop %p, sock %p\n",
				       isop, isop->isop_socket);
				printf("addrmatch cmp'd with (%p):\n",
					isop->isop_faddr);
				dump_isoaddr(isop->isop_faddr);
			}
#endif
			continue;
		}
		if (errno)
			isop->isop_socket->so_error = errno;
		if (notify)
			(*notify) (isop);
	}
	splx(s);
#ifdef ARGO_DEBUG
	if (argo_debug[D_ISO]) {
		printf("END OF iso_pcbnotify\n");
	}
#endif
}


/*
 * FUNCTION:		iso_pcblookup
 *
 * PURPOSE:		looks for a given combination of (faddr), (fport),
 *			(lport), (laddr) in the queue named by (head).
 *			Argument (flags) is ignored.
 *
 * RETURNS:		ptr to the isopcb if it finds a connection matching
 *			these arguments, o.w. returns zero.
 *
 * SIDE EFFECTS:
 *
 * NOTES:
 */
struct isopcb  *
iso_pcblookup(head, fportlen, fport, laddr)
	struct isopcb  *head;
	struct sockaddr_iso *laddr;
	caddr_t         fport;
	int             fportlen;
{
	struct isopcb *isop;
	caddr_t lp = TSEL(laddr);
	unsigned int    llen = laddr->siso_tlen;

#ifdef ARGO_DEBUG
	if (argo_debug[D_ISO]) {
		printf("iso_pcblookup(head %p laddr %p fport %p)\n",
		       head, laddr, fport);
	}
#endif
	for (isop = head->isop_next; isop != head; isop = isop->isop_next) {
		if (isop->isop_laddr == 0 || isop->isop_laddr == laddr)
			continue;
		if (isop->isop_laddr->siso_tlen != llen)
			continue;
		if (bcmp(lp, TSEL(isop->isop_laddr), llen))
			continue;
		if (fportlen && isop->isop_faddr &&
		    bcmp(fport, TSEL(isop->isop_faddr), (unsigned) fportlen))
			continue;
		/*
		 * PHASE2 addrmatch1 should be iso_addrmatch(a, b, mask)
		 * where mask is taken from isop->isop_laddrmask (new field)
		 * isop_lnetmask will also be available in isop if (laddr !=
		 * &zeroiso_addr && !iso_addrmatch1(laddr,
		 * &(isop->isop_laddr.siso_addr))) continue;
		 */
		if (laddr->siso_nlen && (!SAME_ISOADDR(laddr, isop->isop_laddr)))
			continue;
		return (isop);
	}
	return (struct isopcb *) 0;
}
#endif /* ISO */
