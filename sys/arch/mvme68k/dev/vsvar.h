/*	$OpenBSD: vsvar.h,v 1.8 2009/02/17 22:28:41 miod Exp $ */
/*
 * Copyright (c) 2004, 2009, Miodrag Vallat.
 * Copyright (c) 1999 Steve Murphree, Jr.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory.
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
 */
#ifndef	_VSVAR_H_
#define	_VSVAR_H_

/*
 * Scatter/gather structures
 *
 * Each s/g element is 8 bytes long; a s/g list is limited to 64
 * entries. To afford larger lists, it is possible to use ``link''
 * entries, which can add up to 64 new entries (and contain link
 * entries themselves, too), by setting the M_ADR_SG_LINK bit in
 * the address modifier field.
 *
 * To keep things simple, this driver will use only use a flat list
 * of up to 64 entries, thereby limiting the maximum transfer to
 * 64 pages (worst case situation).
 */

#define	MAX_SG_ELEMENTS	64

struct vs_sg_entry {
	union {
		uint16_t	bytes;	/* entry byte count */
		struct {
			uint8_t gather;	/* count of linked entries */
			uint8_t reserved;
		} link;
	} count;
	uint16_t	pa_high;	/* 32-bit address field split... */
	uint16_t	pa_low;		/* ... due to alignment */
	uint16_t	addr;		/* address type and modifier */
};

/* largest power of two for the bytes field */
#define	MAX_SG_ELEMENT_SIZE	(1U << 15)

/*
 * Command control block
 */
struct vs_cb {
	struct scsi_xfer	*cb_xs;	/* current command */
	u_int			 cb_q;	/* controller queue it's in */

	bus_dmamap_t		 cb_dmamap;
	bus_size_t		 cb_dmalen;
};

/*
 * Per-channel information
 */
struct vs_channel {
	int			vc_id;		/* host id */
	int			vc_width;	/* number of targets */
	int			vc_type;	/* channel type */
#define	VCT_UNKNOWN			0
#define	VCT_SE				1
#define	VCT_DIFFERENTIAL		2
	struct scsi_link	vc_link;
};

struct vs_softc {
	struct device		 sc_dev;

	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_ioh;
	bus_dma_tag_t		 sc_dmat;

	int			 sc_bid;	/* board id */
	struct vs_channel	 sc_channel[2];

	struct intrhand		 sc_ih_n;	/* normal interrupt handler */
	int			 sc_nvec;
	struct intrhand		 sc_ih_e;	/* error interrupt handler */
	int			 sc_evec;
	char			 sc_intrname_e[16 + 4];
	int			 sc_ipl;

	u_int			 sc_nwq;	/* number of work queues */
	struct vs_cb		*sc_cb;

	bus_dmamap_t		 sc_sgmap;
	bus_dma_segment_t	 sc_sgseg;
	struct vs_sg_entry	*sc_sgva;
};

/* Access macros */

#define	vs_read(w,o) \
	bus_space_read_##w (sc->sc_iot, sc->sc_ioh, (o))
#define	vs_write(w,o,v) \
	bus_space_write_##w (sc->sc_iot, sc->sc_ioh, (o), (v))
#define	vs_bzero(o,s) \
	bus_space_set_region_2(sc->sc_iot, sc->sc_ioh, (o), 0, (s) / 2)

#define	cib_read(w,o)		vs_read(w, sh_CIB + (o))
#define	cib_write(w,o,v)	vs_write(w, sh_CIB + (o), (v))
#define	crb_read(w,o)		vs_read(w, sh_CRB + (o))
#define	crb_write(w,o,v)	vs_write(w, sh_CRB + (o), (v))
#define	csb_read(w,o)		vs_read(w, sh_CSS + (o))
#define	mce_read(w,o)		vs_read(w, sh_MCE + (o))
#define	mce_write(w,o,v)	vs_write(w, sh_MCE + (o), (v))
#define	mce_iopb_read(w,o)	vs_read(w, sh_MCE_IOPB + (o))
#define	mce_iopb_write(w,o,v)	vs_write(w, sh_MCE_IOPB + (o), (v))
#define	mcsb_read(w,o)		vs_read(w, sh_MCSB + (o))
#define	mcsb_write(w,o,v)	vs_write(w, sh_MCSB + (o), (v))

#define	CRSW		crb_read(2, CRB_CRSW)
#define	CRB_CLR_DONE	crb_write(2, CRB_CRSW, 0)
#define	CRB_CLR_ER	crb_write(2, CRB_CRSW, CRSW & ~M_CRSW_ER)

#define	THAW_REG	mcsb_read(2, MCSB_THAW)
#define	THAW(x) \
	do { \
		mcsb_write(1, MCSB_THAW, (x)); \
		mcsb_write(1, MCSB_THAW + 1, M_THAW_TWQE); \
	} while (0)

#endif /* _M328VAR_H */
