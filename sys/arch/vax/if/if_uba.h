/*	$OpenBSD: if_uba.h,v 1.7 2003/11/10 21:05:04 miod Exp $	*/
/*	$NetBSD: if_uba.h,v 1.6 1996/08/20 14:07:50 ragge Exp $	*/

/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
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
 *	@(#)if_uba.h	7.4 (Berkeley) 6/28/90
 */

/*
 * Structure and routine definitions
 * for UNIBUS network interfaces.
 */

#define	IF_MAXNUBAMR	10
/*
 * Each interface has structures giving information
 * about UNIBUS resources held by the interface
 * for each send and receive buffer.
 *
 * We hold IF_NUBAMR map registers for datagram data, starting
 * at ifr_mr.  Map register ifr_mr[-1] maps the local network header
 * ending on the page boundary.  Bdp's are reserved for read and for
 * write, given by ifr_bdp.  The prototype of the map register for
 * read and for write is saved in ifr_proto.
 *
 * When write transfers are not full pages on page boundaries we just
 * copy the data into the pages mapped on the UNIBUS and start the
 * transfer.  If a write transfer is of a (1024 byte) page on a page
 * boundary, we swap in UNIBUS pte's to reference the pages, and then
 * remap the initial pages (from ifu_wmap) when the transfer completes.
 *
 * When read transfers give whole pages of data to be input, we
 * allocate page frames from a network page list and trade them
 * with the pages already containing the data, mapping the allocated
 * pages to replace the input pages for the next UNIBUS data input.
 */

/*
 * Information per interface.
 */
struct	ifubinfo {
	short	iff_flags;			/* used during uballoc's */
	short	iff_hlen;			/* local net header length */
	struct	uba_regs *iff_uba;		/* uba adaptor regs, in vm */
	pt_entry_t *iff_ubamr;			/* uba map regs, in vm */
	struct	uba_softc *iff_softc;		/* uba */
};

/*
 * Information per buffer.
 */
struct ifrw {
	caddr_t	ifrw_addr;			/* virt addr of header */
	short	ifrw_bdp;			/* unibus bdp */
	short	ifrw_flags;			/* type, etc. */
#define	IFRW_W	0x01				/* is a transmit buffer */
	int	ifrw_info;			/* value from ubaalloc */
	int	ifrw_proto;			/* map register prototype */
	pt_entry_t *ifrw_mr;			/* base of map registers */
};

/*
 * Information per transmit buffer, including the above.
 */
struct ifxmt {
	struct	ifrw ifrw;
	caddr_t	ifw_base;			/* virt addr of buffer */
	pt_entry_t ifw_wmap[IF_MAXNUBAMR];	/* base pages for output */
	struct	mbuf *ifw_xtofree;		/* pages being dma'd out */
	short	ifw_xswapd;			/* mask of clusters swapped */
	short	ifw_nmr;			/* number of entries in wmap */
};
#define	ifw_addr	ifrw.ifrw_addr
#define	ifw_bdp		ifrw.ifrw_bdp
#define	ifw_flags	ifrw.ifrw_flags
#define	ifw_info	ifrw.ifrw_info
#define	ifw_proto	ifrw.ifrw_proto
#define	ifw_mr		ifrw.ifrw_mr

/*
 * Most interfaces have a single receive and a single transmit buffer,
 * and use struct ifuba to store all of the unibus information.
 */
struct ifuba {
	struct	ifubinfo ifu_info;
	struct	ifrw ifu_r;
	struct	ifxmt ifu_xmt;
};

#define	ifu_softc	ifu_info.iff_softc
#define	ifu_hlen	ifu_info.iff_hlen
#define	ifu_uba		ifu_info.iff_uba
#define	ifu_ubamr	ifu_info.iff_ubamr
#define	ifu_flags	ifu_info.iff_flags
#define	ifu_w		ifu_xmt.ifrw
#define	ifu_xtofree	ifu_xmt.ifw_xtofree

#ifdef 	_KERNEL
#define	if_ubainit(ifuba, uban, hlen, nmr) \
		if_ubaminit(&(ifuba)->ifu_info, uban, hlen, nmr, \
			&(ifuba)->ifu_r, 1, &(ifuba)->ifu_xmt, 1)
#define	if_rubaget(ifu, totlen, off0, ifp) \
		if_ubaget(&(ifu)->ifu_info, &(ifu)->ifu_r, totlen, off0, ifp)
#define	if_wubaput(ifu, m) \
		if_ubaput(&(ifu)->ifu_info, &(ifu)->ifu_xmt, m)

/* Prototypes */
int	if_ubaminit(struct ifubinfo *, struct uba_softc *, int, int,
	    struct ifrw *, int, struct ifxmt *, int);
int	if_ubaput(struct ifubinfo *, struct ifxmt *, struct mbuf *);
struct mbuf *if_ubaget(struct ifubinfo *, struct ifrw *, int,
	struct ifnet *);

#endif
