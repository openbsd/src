/*	$OpenBSD: fwohcivar.h,v 1.4 2003/01/12 12:03:48 tdeval Exp $	*/
/*	$NetBSD: fwohcivar.h,v 1.17 2002/01/16 01:47:37 eeh Exp $	*/

/*
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Matt Thomas of the 3am Software Foundry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_DEV_IEEE1394_FWOHCIVAR_H_
#define	_DEV_IEEE1394_FWOHCIVAR_H_

#ifdef	__NetBSD__
#include <sys/callout.h>
#else
#include <sys/timeout.h>
#endif
#include <sys/queue.h>

#include <machine/bus.h>

#define	OHCI_PAGE_SIZE				0x0800
#define	OHCI_BUF_ARRQ_CNT			16
#define	OHCI_BUF_ARRS_CNT			8
#define	OHCI_BUF_ATRQ_CNT			(8*8)
#define	OHCI_BUF_ATRS_CNT			(8*8)
#define	OHCI_BUF_IR_CNT				8
#define	OHCI_BUF_CNT							\
	(OHCI_BUF_ARRQ_CNT + OHCI_BUF_ARRS_CNT + OHCI_BUF_ATRQ_CNT +	\
	    OHCI_BUF_ATRS_CNT + OHCI_BUF_IR_CNT + 1 + 1)

#define	OHCI_LOOP				1000
#define	OHCI_SELFID_TIMEOUT			(3 * hz)
#define	OHCI_ASYNC_STREAM			0x40

struct fwohci_softc;
struct fwohci_pkt;
struct mbuf;

typedef struct fwohci_buf {
	TAILQ_ENTRY(fwohci_buf)	  fb_list;
	bus_dma_segment_t	  fb_seg;
	int			  fb_nseg;
	bus_dmamap_t		  fb_dmamap;	/* DMA map of the buffer      */
	caddr_t			  fb_buf;	/* kernel vaddr of the buffer */
	struct fwohci_desc	 *fb_desc;	/* kernel vaddr of descriptor */
	bus_addr_t		  fb_daddr;	/* paddr of the descriptor    */
	int			  fb_off;
	struct mbuf		 *fb_m;
	void			 *fb_statusarg;
	void			(*fb_callback)(struct device *, struct mbuf *);
	int			(*fb_statuscb)(struct fwohci_softc *, void *,
					       struct fwohci_pkt *);
} fwohci_buf;

typedef struct fwohci_pkt {
	int			  fp_tcode;
	int			  fp_hlen;
	int			  fp_dlen;
	u_int32_t		  fp_hdr[4];
	struct uio		  fp_uio;
	struct iovec		  fp_iov[6];
	u_int32_t		 *fp_trail;
	struct mbuf		 *fp_m;
	u_int16_t		  fp_status;
	void			 *fp_statusarg;
	void			(*fp_callback)(struct device *, struct mbuf *);
	int			(*fp_statuscb)(struct fwohci_softc *, void *,
					       struct fwohci_pkt *);
} fwohci_pkt;

typedef struct fwohci_handler {
	LIST_ENTRY(fwohci_handler) fh_list;
	u_int32_t		   fh_tcode;	/* ARRQ   / ARRS   / IR   */
	u_int32_t		   fh_key1;	/* addrhi / srcid  / chan */
	u_int32_t		   fh_key2;	/* addrlo / tlabel / tag  */
	u_int32_t		   fh_key3;	/*    node_id  :  count   */
	int			 (*fh_handler)(struct fwohci_softc *, void *,
					       struct fwohci_pkt *);
	void			  *fh_handarg;
} fwohci_handler;

typedef struct fwohci_ctx {
	int			 fc_ctx;
	int			 fc_type;	/*
						 * FWOHCI_CTX_(ASYNC|
						 *	       ISO_SINGLE|
						 *	       ISO_MULTI)
						 */
	int			 fc_bufcnt;
	u_int32_t		*fc_branch;
	TAILQ_HEAD(fwohci_buf_s, fwohci_buf) fc_buf;
	struct fwohci_buf_s	 fc_buf2;	/* for iso */
	LIST_HEAD(, fwohci_handler) fc_handler;
	struct fwohci_buf	*fc_buffers;
} fwohci_ctx;

typedef struct fwohci_uidtbl {
	int			fu_valid;
	int			fu_link_speed;
	u_int8_t		fu_uid[8];
} fwohci_uidtbl;

/*
 * Needed to keep track of outstanding packets during a read op. Since the
 * packet stream is asynch it's possible to parse a response packet before the
 * ack bits are processed. In this case something needs to track whether the
 * abuf is still valid before possibly attempting to use items from within it.
 */

typedef struct fwohci_cb {
	struct ieee1394_abuf	*ab;
	int			 count;
	int			 abuf_valid;
} fwohci_cb;

typedef struct fwohci_softc {
	struct ieee1394_softc	  sc_sc1394;
	struct evcnt		  sc_intrcnt;
	struct evcnt		  sc_isocnt;
	struct evcnt		  sc_isopktcnt;

	bus_space_tag_t		  sc_memt;
	bus_space_handle_t	  sc_memh;
	bus_dma_tag_t		  sc_dmat;
	bus_size_t		  sc_memsize;
#if 0

/*
 * Mandatory structures to get the link enabled.
 */
	bus_dmamap_t		  sc_configrom_map;
	bus_dmamap_t		  sc_selfid_map;
	u_int32_t		 *sc_selfid_buf;
	u_int32_t		 *sc_configrom;
#endif

	bus_dma_segment_t	  sc_dseg;
	int			  sc_dnseg;
	bus_dmamap_t		  sc_ddmamap;
	struct fwohci_desc	 *sc_desc;
	u_int8_t		 *sc_descmap;
	int			  sc_descsize;
	int			  sc_isoctx;

	void			 *sc_shutdownhook;
	void			 *sc_powerhook;
#ifdef	__NetBSD__
	struct callout		  sc_selfid_callout;
#else
	struct timeout		  sc_selfid_callout;
#endif
	int			  sc_selfid_fail;

	struct fwohci_ctx	 *sc_ctx_arrq;
	struct fwohci_ctx	 *sc_ctx_arrs;
	struct fwohci_ctx	 *sc_ctx_atrq;
	struct fwohci_ctx	 *sc_ctx_atrs;
	struct fwohci_ctx	**sc_ctx_ir;
	struct fwohci_buf	  sc_buf_cnfrom;
	struct fwohci_buf	  sc_buf_selfid;

	struct proc		 *sc_event_thread;

	int			 *sc_dying;
	u_int32_t		  sc_intmask;
	u_int32_t		  sc_iso;
	u_int32_t		  sc_isorxrst;
	u_int32_t		  sc_isotxrst;

	u_int8_t		  sc_csr[CSR_SB_END];

	struct fwohci_uidtbl	 *sc_uidtbl;
	u_int16_t		  sc_nodeid;	/* Full Node ID of this node */
	u_int8_t		  sc_rootid;	/* Phy ID of Root */
	u_int8_t		  sc_irmid;	/* Phy ID of IRM */
	u_int8_t		  sc_tlabel;	/* Transaction Label */

	LIST_HEAD(, ieee1394_softc) sc_nodelist;
} fwohci_softc;

int fwohci_init (struct fwohci_softc *, const struct evcnt *);
int fwohci_intr (void *);
int fwohci_print (void *, const char *);
int fwohci_detach(struct fwohci_softc *, int);
int fwohci_activate(struct device *, enum devact);

/* Macros to read and write the OHCI registers
 */
#define	OHCI_CSR_WRITE(sc, reg, val) \
	bus_space_write_4((sc)->sc_memt, (sc)->sc_memh, reg, htole32(val))
#ifdef	__NetBSD__
#define	OHCI_CSR_READ(sc, reg) \
	le32toh(bus_space_read_4((sc)->sc_memt, (sc)->sc_memh, reg))
#else
#define	OHCI_CSR_READ(sc, reg) \
	letoh32(bus_space_read_4((sc)->sc_memt, (sc)->sc_memh, reg))
#endif

#define	FWOHCI_CTX_ASYNC			0
#define	FWOHCI_CTX_ISO_SINGLE			1	/* for async stream */
#define	FWOHCI_CTX_ISO_MULTI			2	/* for isochronous */

/* Locators. */

#ifdef	__NetBSD__
#include "locators.h"
#else
/* dup from sys/conf/files */
#define	FWBUSCF_IDHI_DEFAULT			(-1)
#define	FWBUSCF_IDLO_DEFAULT			(-1)
#define	FWBUSCF_IDHI				1
#define	FWBUSCF_IDLO				0
#endif

#define	fwbuscf_idhi	cf_loc[FWBUSCF_IDHI]
#define	FWBUS_UNK_IDHI	FWBUSCF_IDHI_DEFAULT

#define	fwbuscf_idlo	cf_loc[FWBUSCF_IDLO]
#define	FWBUS_UNK_IDLO	FWBUSCF_IDLO_DEFAULT

#endif	/* _DEV_IEEE1394_FWOHCIVAR_H_ */
