/*	$OpenBSD: asc_vsbus.c,v 1.14 2011/09/11 19:29:01 miod Exp $	*/
/*	$NetBSD: asc_vsbus.c,v 1.22 2001/02/04 20:36:32 ragge Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/queue.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_message.h>

#include <machine/bus.h>
#include <machine/vmparam.h>

#include <dev/ic/ncr53c9xreg.h>
#include <dev/ic/ncr53c9xvar.h>

#include <machine/cpu.h>
#include <machine/sid.h>
#include <machine/scb.h>
#include <machine/vsbus.h>
#include <machine/clock.h>	/* for SCSI ctlr ID# XXX */

struct asc_vsbus_softc {
	struct ncr53c9x_softc sc_ncr53c9x;	/* Must be first */
	struct evcount sc_intrcnt;		/* count interrupts */
	int	sc_cvec;			/* vector */
	bus_space_tag_t sc_bst;			/* bus space tag */
	bus_space_handle_t sc_bsh;		/* bus space handle */
	bus_space_handle_t sc_dirh;		/* scsi direction handle */
	bus_space_handle_t sc_adrh;		/* scsi address handle */
	bus_space_handle_t sc_ncrh;		/* ncr bus space handle */
	bus_dma_tag_t sc_dmat;			/* bus dma tag */
	bus_dmamap_t sc_dmamap;
	caddr_t *sc_dmaaddr;
	size_t *sc_dmalen;
	size_t sc_dmasize;
	unsigned int sc_flags;
#define	ASC_FROMMEMORY		0x0001		/* Must be 1 */
#define	ASC_DMAACTIVE		0x0002
#define	ASC_MAPLOADED		0x0004
	unsigned long sc_xfers;
};

#define	ASC_REG_KA46_ADR	0x0000
#define	ASC_REG_KA46_DIR	0x000C
#define	ASC_REG_KA49_ADR	0x0000
#define	ASC_REG_KA49_DIR	0x0004
#define	ASC_REG_NCR		0x0080
#define	ASC_REG_END		0x00B0

#define	ASC_MAXXFERSIZE		65536
#define	ASC_FREQUENCY		25000000

extern struct cfdriver sd_cd;

int asc_vsbus_match(struct device *, void *, void *);
void asc_vsbus_attach(struct device *, struct device *, void *);

struct cfattach asc_vsbus_ca = {
	sizeof(struct asc_vsbus_softc), asc_vsbus_match, asc_vsbus_attach
};

struct cfdriver asc_cd = {
	NULL, "asc", DV_DULL
};

struct scsi_adapter	asc_vsbus_ops = {
	ncr53c9x_scsi_cmd,	
	scsi_minphys,
	NULL,
	NULL
};

/*
 * Functions and the switch for the MI code
 */
u_char	asc_vsbus_read_reg(struct ncr53c9x_softc *, int);
void	asc_vsbus_write_reg(struct ncr53c9x_softc *, int, u_char);
int	asc_vsbus_dma_isintr(struct ncr53c9x_softc *);
void	asc_vsbus_dma_reset(struct ncr53c9x_softc *);
int	asc_vsbus_dma_intr(struct ncr53c9x_softc *);
int	asc_vsbus_dma_setup(struct ncr53c9x_softc *, caddr_t *,
	    size_t *, int, size_t *);
void	asc_vsbus_dma_go(struct ncr53c9x_softc *);
void	asc_vsbus_dma_stop(struct ncr53c9x_softc *);
int	asc_vsbus_dma_isactive(struct ncr53c9x_softc *);
int	asc_vsbus_controller_id(void);

static struct ncr53c9x_glue asc_vsbus_glue = {
	asc_vsbus_read_reg,
	asc_vsbus_write_reg,
	asc_vsbus_dma_isintr,
	asc_vsbus_dma_reset,
	asc_vsbus_dma_intr,
	asc_vsbus_dma_setup,
	asc_vsbus_dma_go,
	asc_vsbus_dma_stop,
	asc_vsbus_dma_isactive,
	NULL,
};

static u_int8_t asc_attached;		/* can't have more than one asc */

int
asc_vsbus_match(struct device *parent, void *conf, void *aux)
{
	struct vsbus_attach_args *va = aux;
	struct cfdata *cf = conf;
	volatile u_int8_t *ncr_regs;
	volatile int dummy;

	if (asc_attached)
		return 0;

	switch (vax_boardtype) {
	case VAX_BTYP_46:
	case VAX_BTYP_48:
		if (cf->cf_loc[0] != (SCA_REGS | 0x80))
			return 0;
		break;
	case VAX_BTYP_49:
	case VAX_BTYP_1303:
		if (cf->cf_loc[0] != (SCA_REGS_KA49 | 0x80))
			return 0;
		break;
	default:
		return 0;
	}

	ncr_regs = (volatile u_int8_t *) va->va_addr;

	/*  *** need to generate an interrupt here
	 * From trial and error, I've determined that an INT is generated
	 * only when the following sequence of events occurs:
	 *   1) The interrupt status register (0x05) must be read.
	 *   2) SCSI bus reset interrupt must be enabled
	 *   3) SCSI bus reset command must be sent
	 *   4) NOP command must be sent
	 */

	dummy = ncr_regs[NCR_INTR << 2] & 0xFF;
        ncr_regs[NCR_CFG1 << 2] = asc_vsbus_controller_id(); /* turn on INT
							   for SCSI reset */
        ncr_regs[NCR_CMD << 2] = NCRCMD_RSTSCSI;	/* send the reset */
        ncr_regs[NCR_CMD << 2] = NCRCMD_NOP;		/* send a NOP */
	DELAY(10000);

	dummy = ncr_regs[NCR_INTR << 2] & 0xFF;
	return (dummy & NCRINTR_SBR) != 0;
}


/*
 * Attach this instance, and then all the sub-devices
 */
void
asc_vsbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct vsbus_attach_args *va = aux;
	struct asc_vsbus_softc *asc = (void *)self;
	struct ncr53c9x_softc *sc = &asc->sc_ncr53c9x;
	int error;

	asc_attached = 1;
	/*
	 * Set up glue for MI code early; we use some of it here.
	 */
	sc->sc_glue = &asc_vsbus_glue;

	asc->sc_bst = va->va_iot;
	asc->sc_dmat = va->va_dmat;

	error = bus_space_map(asc->sc_bst, va->va_paddr - ASC_REG_NCR,
	    ASC_REG_END, 0, &asc->sc_bsh);
	if (error) {
		printf(": failed to map registers: error=%d\n", error);
		return;
	}
	error = bus_space_subregion(asc->sc_bst, asc->sc_bsh, ASC_REG_NCR,
	    ASC_REG_END - ASC_REG_NCR, &asc->sc_ncrh);
	if (error) {
		printf(": failed to map ncr registers: error=%d\n", error);
		return;
	}
	if (vax_boardtype == VAX_BTYP_46 || vax_boardtype == VAX_BTYP_48) {
		error = bus_space_subregion(asc->sc_bst, asc->sc_bsh,
		    ASC_REG_KA46_ADR, sizeof(u_int32_t), &asc->sc_adrh);
		if (error) {
			printf(": failed to map adr register: error=%d\n",
			     error);
			return;
		}
		error = bus_space_subregion(asc->sc_bst, asc->sc_bsh,
		    ASC_REG_KA46_DIR, sizeof(u_int32_t), &asc->sc_dirh);
		if (error) {
			printf(": failed to map dir register: error=%d\n",
			     error);
			return;
		}
	} else {
		/* This is a gross and disgusting kludge but it'll
		 * save a bunch of ugly code.  Unlike the VS4000/60,
		 * the SCSI Address and direction registers are not
		 * near the SCSI NCR registers and are inside the 
		 * block of general VAXstation registers.  So we grab
		 * them from there and knowing the internals of the 
		 * bus_space implementation, we cast to bus_space_handles.
		 */
		struct vsbus_softc *vsc = (struct vsbus_softc *) parent;
		asc->sc_adrh = (bus_space_handle_t) (vsc->sc_vsregs + ASC_REG_KA49_ADR);
		asc->sc_dirh = (bus_space_handle_t) (vsc->sc_vsregs + ASC_REG_KA49_DIR);
#if 0
		printf("\n%s: adrh=0x%08lx dirh=0x%08lx", self->dv_xname,
		       asc->sc_adrh, asc->sc_dirh);
		ncr53c9x_debug = NCR_SHOWDMA|NCR_SHOWINTS|NCR_SHOWCMDS|NCR_SHOWPHASE|NCR_SHOWSTART|NCR_SHOWMSGS;
#endif
	}
	error = bus_dmamap_create(asc->sc_dmat, ASC_MAXXFERSIZE, 1, 
	    ASC_MAXXFERSIZE, 0, BUS_DMA_NOWAIT, &asc->sc_dmamap);

	sc->sc_id = asc_vsbus_controller_id();
	sc->sc_freq = ASC_FREQUENCY;

	/* gimme MHz */
	sc->sc_freq /= 1000000;

	scb_vecalloc(va->va_cvec, (void (*)(void *)) ncr53c9x_intr,
	    &asc->sc_ncr53c9x, SCB_ISTACK, &asc->sc_intrcnt);
	asc->sc_cvec = va->va_cvec;
	evcount_attach(&asc->sc_intrcnt, self->dv_xname, &asc->sc_cvec);

	/*
	 * XXX More of this should be in ncr53c9x_attach(), but
	 * XXX should we really poke around the chip that much in
	 * XXX the MI code?  Think about this more...
	 */

	/*
	 * Set up static configuration info.
	 */
	sc->sc_cfg1 = sc->sc_id | NCRCFG1_PARENB;
	sc->sc_cfg2 = NCRCFG2_SCSI2;
	sc->sc_cfg3 = 0;
	sc->sc_rev = NCR_VARIANT_NCR53C94;

	/*
	 * XXX minsync and maxxfer _should_ be set up in MI code,
	 * XXX but it appears to have some dependency on what sort
	 * XXX of DMA we're hooked up to, etc.
	 */

	/*
	 * This is the value used to start sync negotiations
	 * Note that the NCR register "SYNCTP" is programmed
	 * in "clocks per byte", and has a minimum value of 4.
	 * The SCSI period used in negotiation is one-fourth
	 * of the time (in nanoseconds) needed to transfer one byte.
	 * Since the chip's clock is given in MHz, we have the following
	 * formula: 4 * period = (1000 / freq) * 4
	 */
	sc->sc_minsync = (1000 / sc->sc_freq);
	sc->sc_maxxfer = 64 * 1024;

	/* Do the common parts of attachment. */
	ncr53c9x_attach(sc, &asc_vsbus_ops);
}

/*
 * Return the host controllers SCSI ID.
 * The factory default is 6 (unlike most vendors who use 7), but this can
 * be changed in the prom.
 */
int
asc_vsbus_controller_id()
{
	switch (vax_boardtype) {
#if defined(VAX46) || defined(VAX48) || defined(VAX49)
	case VAX_BTYP_46:
	case VAX_BTYP_48:
	case VAX_BTYP_49:
		return (clk_page[0xbc / 2] >> clk_tweak) & 7;
#endif
	default:
		return 6;	/* XXX need to get this from VMB */
	}
}

/*
 * Glue functions.
 */

u_char
asc_vsbus_read_reg(struct ncr53c9x_softc *sc, int reg)
{
	struct asc_vsbus_softc *asc = (struct asc_vsbus_softc *)sc;

	return bus_space_read_1(asc->sc_bst, asc->sc_ncrh,
	    reg * sizeof(u_int32_t));
}

void
asc_vsbus_write_reg(struct ncr53c9x_softc *sc, int reg, u_char val)
{
	struct asc_vsbus_softc *asc = (struct asc_vsbus_softc *)sc;

	bus_space_write_1(asc->sc_bst, asc->sc_ncrh,
	    reg * sizeof(u_int32_t), val);
}

int
asc_vsbus_dma_isintr(struct ncr53c9x_softc *sc)
{
	struct asc_vsbus_softc *asc = (struct asc_vsbus_softc *)sc;
	return bus_space_read_1(asc->sc_bst, asc->sc_ncrh,
	    NCR_STAT * sizeof(u_int32_t)) & NCRSTAT_INT;
}

void
asc_vsbus_dma_reset(struct ncr53c9x_softc *sc)
{
	struct asc_vsbus_softc *asc = (struct asc_vsbus_softc *)sc;

	if (asc->sc_flags & ASC_MAPLOADED)
		bus_dmamap_unload(asc->sc_dmat, asc->sc_dmamap);
	asc->sc_flags &= ~(ASC_DMAACTIVE|ASC_MAPLOADED);
}

int
asc_vsbus_dma_intr(struct ncr53c9x_softc *sc)
{
	struct asc_vsbus_softc *asc = (struct asc_vsbus_softc *)sc;
	u_int tcl, tcm;
	int trans, resid;
	
	if ((asc->sc_flags & ASC_DMAACTIVE) == 0)
		panic("asc_vsbus_dma_intr: DMA wasn't active");

	asc->sc_flags &= ~ASC_DMAACTIVE;

	if (asc->sc_dmasize == 0) {
		/* A "Transfer Pad" operation completed */
		tcl = NCR_READ_REG(sc, NCR_TCL); 
		tcm = NCR_READ_REG(sc, NCR_TCM);
		NCR_DMA(("asc_vsbus_intr: discarded %d bytes (tcl=%d, tcm=%d)\n",
		    tcl | (tcm << 8), tcl, tcm));
		return 0;
	}

	resid = 0;
	if ((resid = (NCR_READ_REG(sc, NCR_FFLAG) & NCRFIFO_FF)) != 0) {
		NCR_DMA(("asc_vsbus_intr: empty FIFO of %d ", resid));
		DELAY(1);
	}
	if (asc->sc_flags & ASC_MAPLOADED) {
		bus_dmamap_sync(asc->sc_dmat, asc->sc_dmamap,
				0, asc->sc_dmasize,
				asc->sc_flags & ASC_FROMMEMORY
					? BUS_DMASYNC_POSTWRITE
					: BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(asc->sc_dmat, asc->sc_dmamap);
	}
	asc->sc_flags &= ~ASC_MAPLOADED;

	resid += (tcl = NCR_READ_REG(sc, NCR_TCL));
	resid += (tcm = NCR_READ_REG(sc, NCR_TCM)) << 8;

	trans = asc->sc_dmasize - resid;
	if (trans < 0) {			/* transferred < 0 ? */
		printf("%s: xfer (%d) > req (%lu)\n",
		    __func__, trans, (u_long) asc->sc_dmasize);
		trans = asc->sc_dmasize;
	}
	NCR_DMA(("asc_vsbus_intr: tcl=%d, tcm=%d; trans=%d, resid=%d\n",
	    tcl, tcm, trans, resid));

	*asc->sc_dmalen -= trans;
	*asc->sc_dmaaddr += trans;
	
	asc->sc_xfers++;
	return 0;
}

int
asc_vsbus_dma_setup(struct ncr53c9x_softc *sc, caddr_t *addr, size_t *len,
		    int datain, size_t *dmasize)
{
	struct asc_vsbus_softc *asc = (struct asc_vsbus_softc *)sc;

	asc->sc_dmaaddr = addr;
	asc->sc_dmalen = len;
	if (datain) {
		asc->sc_flags &= ~ASC_FROMMEMORY;
	} else {
		asc->sc_flags |= ASC_FROMMEMORY;
	}
	if ((vaddr_t) *asc->sc_dmaaddr < VM_MIN_KERNEL_ADDRESS)
		panic("asc_vsbus_dma_setup: dma address (%p) outside of kernel",
		    *asc->sc_dmaaddr);

        NCR_DMA(("%s: start %d@%p,%d\n", sc->sc_dev.dv_xname,
                (int)*asc->sc_dmalen, *asc->sc_dmaaddr, (asc->sc_flags & ASC_FROMMEMORY)));
	*dmasize = asc->sc_dmasize = min(*dmasize, ASC_MAXXFERSIZE);

	if (asc->sc_dmasize) {
		if (bus_dmamap_load(asc->sc_dmat, asc->sc_dmamap,
				*asc->sc_dmaaddr, asc->sc_dmasize,
				NULL /* kernel address */,   
				BUS_DMA_NOWAIT|VAX_BUS_DMA_SPILLPAGE))
			panic("%s: cannot load dma map", sc->sc_dev.dv_xname);
		bus_dmamap_sync(asc->sc_dmat, asc->sc_dmamap,
				0, asc->sc_dmasize,
				asc->sc_flags & ASC_FROMMEMORY
					? BUS_DMASYNC_PREWRITE
					: BUS_DMASYNC_PREREAD);
		bus_space_write_4(asc->sc_bst, asc->sc_adrh, 0,
				  asc->sc_dmamap->dm_segs[0].ds_addr);
		bus_space_write_4(asc->sc_bst, asc->sc_dirh, 0,
				  asc->sc_flags & ASC_FROMMEMORY);
		NCR_DMA(("%s: dma-load %lu@0x%08lx\n", sc->sc_dev.dv_xname,
			asc->sc_dmamap->dm_segs[0].ds_len,
			asc->sc_dmamap->dm_segs[0].ds_addr));
		asc->sc_flags |= ASC_MAPLOADED;
	}

	return 0;
}

void
asc_vsbus_dma_go(struct ncr53c9x_softc *sc)
{
	struct asc_vsbus_softc *asc = (struct asc_vsbus_softc *)sc;

	asc->sc_flags |= ASC_DMAACTIVE;
}

void
asc_vsbus_dma_stop(struct ncr53c9x_softc *sc)
{
	struct asc_vsbus_softc *asc = (struct asc_vsbus_softc *)sc;

	if (asc->sc_flags & ASC_MAPLOADED) {
		bus_dmamap_sync(asc->sc_dmat, asc->sc_dmamap,
				0, asc->sc_dmasize,
				asc->sc_flags & ASC_FROMMEMORY
					? BUS_DMASYNC_POSTWRITE
					: BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(asc->sc_dmat, asc->sc_dmamap);
	}

	asc->sc_flags &= ~(ASC_DMAACTIVE|ASC_MAPLOADED);
}

int
asc_vsbus_dma_isactive(struct ncr53c9x_softc *sc)
{
	struct asc_vsbus_softc *asc = (struct asc_vsbus_softc *)sc;

	return (asc->sc_flags & ASC_DMAACTIVE) != 0;
}
