/*	$OpenBSD: pcscp.c,v 1.5 2001/08/25 14:52:57 jason Exp $	*/
/*	$NetBSD: pcscp.c,v 1.11 2000/11/14 18:42:58 thorpej Exp $	*/

/*-
 * Copyright (c) 1997, 1998, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center; Izumi Tsutsui.
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

/*
 * pcscp.c: device dependent code for AMD Am53c974 (PCscsi-PCI)
 * written by Izumi Tsutsui <tsutsui@ceres.dti.ne.jp>
 *
 * Technical manual available at
 * http://www.amd.com/products/npd/techdocs/techdocs.html
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/endian.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_message.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/ncr53c9xreg.h>
#include <dev/ic/ncr53c9xvar.h>

#include <dev/pci/pcscpreg.h>

#define IO_MAP_REG	0x10
#define MEM_MAP_REG	0x14

struct pcscp_softc {
	struct ncr53c9x_softc sc_ncr53c9x;	/* glue to MI code */

	bus_space_tag_t sc_st;		/* bus space tag */
	bus_space_handle_t sc_sh;	/* bus space handle */
	void *sc_ih;			/* interrupt cookie */

	bus_dma_tag_t sc_dmat;		/* DMA tag */

	bus_dmamap_t sc_xfermap;	/* DMA map for transfers */

	u_int32_t *sc_mdladdr;		/* MDL array */
	bus_dmamap_t sc_mdldmap;	/* MDL DMA map */

	int	sc_active;		/* DMA state */
	int	sc_datain;		/* DMA Data Direction */
	size_t	sc_dmasize;		/* DMA size */
	char	**sc_dmaaddr;		/* DMA address */
	size_t	*sc_dmalen;		/* DMA length */
};

#define	READ_DMAREG(sc, reg) \
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (reg))
#define	WRITE_DMAREG(sc, reg, var) \
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, (reg), (var))

/* don't have to use MI defines in MD code... */
#undef	NCR_READ_REG
#define	NCR_READ_REG(sc, reg)		pcscp_read_reg((sc), (reg))
#undef	NCR_WRITE_REG
#define	NCR_WRITE_REG(sc, reg, val)	pcscp_write_reg((sc), (reg), (val))

int	pcscp_match __P((struct device *, void *, void *)); 
void	pcscp_attach __P((struct device *, struct device *, void *));  

struct cfattach pcscp_ca = {
	sizeof(struct pcscp_softc), pcscp_match, pcscp_attach
};

struct cfdriver pcscp_cd = {
	NULL, "pcscp", DV_DULL
};

/*
 * Functions and the switch for the MI code.
 */

u_char	pcscp_read_reg __P((struct ncr53c9x_softc *, int));
void	pcscp_write_reg __P((struct ncr53c9x_softc *, int, u_char));
int	pcscp_dma_isintr __P((struct ncr53c9x_softc *));
void	pcscp_dma_reset __P((struct ncr53c9x_softc *));
int	pcscp_dma_intr __P((struct ncr53c9x_softc *));
int	pcscp_dma_setup __P((struct ncr53c9x_softc *, caddr_t *,
			       size_t *, int, size_t *));
void	pcscp_dma_go __P((struct ncr53c9x_softc *));
void	pcscp_dma_stop __P((struct ncr53c9x_softc *));
int	pcscp_dma_isactive __P((struct ncr53c9x_softc *));

struct scsi_adapter pcscp_adapter = {
	ncr53c9x_scsi_cmd,	/* cmd */
	minphys,		/* minphys */
	0,			/* open */
	0,			/* close */
};

struct ncr53c9x_glue pcscp_glue = {
	pcscp_read_reg,
	pcscp_write_reg,
	pcscp_dma_isintr,
	pcscp_dma_reset,
	pcscp_dma_intr,
	pcscp_dma_setup,
	pcscp_dma_go,
	pcscp_dma_stop,
	pcscp_dma_isactive,
	NULL,			/* gl_clear_latched_intr */
};

#ifdef __HAS_NEW_BUS_DMAMAP_SYNC
#define pcscp_bus_dmamap_sync(t, m, o, l, f) \
    bus_dmamap_sync((t), (m), (o), (l), (f))
#else
#define pcscp_bus_dmamap_sync(t, m, o, l, f) \
    bus_dmamap_sync((t), (m), (f))
#endif

int
pcscp_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct pci_attach_args *pa = aux;
	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_AMD)
		return 0;

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_AMD_PCSCSI_PCI:
#if 0
	case PCI_PRODUCT_AMD_PCNETS_PCI:
#endif
		return 1;
	}
	return 0;
}

/*
 * Attach this instance, and then all the sub-devices
 */
void
pcscp_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	struct pcscp_softc *esc = (void *)self;
	struct ncr53c9x_softc *sc = &esc->sc_ncr53c9x;
	bus_space_tag_t st, iot, memt;
	bus_space_handle_t sh, ioh, memh;
	int ioh_valid, memh_valid;
	pci_intr_handle_t ih;
	const char *intrstr;
	pcireg_t csr;
	bus_dma_segment_t seg;
	int error, rseg;

	ioh_valid = (pci_mapreg_map(pa, IO_MAP_REG, PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, NULL, NULL, 0) == 0);
#if 0	/* XXX cannot use memory map? */
	memh_valid = (pci_mapreg_map(pa, MEM_MAP_REG,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &memt, &memh, NULL, NULL, 0) == 0);
#else
	memh_valid = 0;
#endif

	if (memh_valid) {
		st = memt;
		sh = memh;
	} else if (ioh_valid) {
		st = iot;
		sh = ioh;
	} else {
		printf(": unable to map registers\n");
		return;
	}

	sc->sc_glue = &pcscp_glue;

	esc->sc_st = st;
	esc->sc_sh = sh;
	esc->sc_dmat = pa->pa_dmat;

	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
	    csr | PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_IO_ENABLE);
				     
	/*
	 * XXX More of this should be in ncr53c9x_attach(), but
	 * XXX should we really poke around the chip that much in
	 * XXX the MI code?  Think about this more...
	 */

	/*
	 * Set up static configuration info.
	 */

	/*
	 * XXX should read configuration from EEPROM?
	 *
	 * MI ncr53c9x driver does not support configuration
	 * per each target device, though...
	 */
	sc->sc_id = 7;
	sc->sc_cfg1 = sc->sc_id | NCRCFG1_PARENB;
	sc->sc_cfg2 = NCRCFG2_SCSI2 | NCRCFG2_FE;
	sc->sc_cfg3 = NCRAMDCFG3_IDM | NCRAMDCFG3_FCLK;
	sc->sc_cfg4 = NCRAMDCFG4_GE12NS | NCRAMDCFG4_RADE;
	sc->sc_rev = NCR_VARIANT_AM53C974;
	sc->sc_features = NCR_F_FASTSCSI;
	sc->sc_cfg3_fscsi = NCRAMDCFG3_FSCSI;
	sc->sc_freq = 40; /* MHz */

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

	sc->sc_minsync = 1000 / sc->sc_freq; 

	/* Really no limit, but since we want to fit into the TCR... */
	sc->sc_maxxfer = 16 * 1024 * 1024;

	/* map and establish interrupt */
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		return;
	}

	intrstr = pci_intr_string(pa->pa_pc, ih);
	esc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, 
	    ncr53c9x_intr, esc, sc->sc_dev.dv_xname);
	if (esc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	if (intrstr != NULL)
		printf(": %s\n", intrstr);

	/*
	 * Create the DMA maps for the data transfers.
         */

#define MDL_SEG_SIZE	0x1000 /* 4kbyte per segment */
#define MDL_SEG_OFFSET	0x0FFF
#define MDL_SIZE	(MAXPHYS / MDL_SEG_SIZE + 1) /* no hardware limit? */

	if (bus_dmamap_create(esc->sc_dmat, MAXPHYS, MDL_SIZE, MAXPHYS, 0,
	    BUS_DMA_NOWAIT, &esc->sc_xfermap)) {
		printf("%s: can't create dma maps\n", sc->sc_dev.dv_xname);
		return;
	}

	/*
	 * Allocate and map memory for the MDL.
	 */

	if ((error = bus_dmamem_alloc(esc->sc_dmat,
	    sizeof(u_int32_t) * MDL_SIZE, PAGE_SIZE, 0, &seg, 1, &rseg,
	    BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to allocate memory for the MDL, "
		    "error = %d\n", sc->sc_dev.dv_xname, error);
		return;
	}
	if ((error = bus_dmamem_map(esc->sc_dmat, &seg, rseg,
	    sizeof(u_int32_t) * MDL_SIZE , (caddr_t *)&esc->sc_mdladdr,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		printf("%s: unable to map the MDL memory, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		return;
	}
	if ((error = bus_dmamap_create(esc->sc_dmat, 
	    sizeof(u_int32_t) * MDL_SIZE, 1, sizeof(u_int32_t) * MDL_SIZE,
	    0, BUS_DMA_NOWAIT, &esc->sc_mdldmap)) != 0) {
		printf("%s: unable to map_create for the MDL, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		return;
	}
	if ((error = bus_dmamap_load(esc->sc_dmat, esc->sc_mdldmap,
	     esc->sc_mdladdr, sizeof(u_int32_t) * MDL_SIZE,
	     NULL, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: unable to load for the MDL, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		return;
	}

	/* Do the common parts of attachment. */
	printf("%s", sc->sc_dev.dv_xname);

	ncr53c9x_attach(sc, &pcscp_adapter, NULL);

	/* Turn on target selection using the `dma' method */
	ncr53c9x_dmaselect = 1;
}

/*
 * Glue functions.
 */

u_char
pcscp_read_reg(sc, reg)
	struct ncr53c9x_softc *sc;
	int reg;
{
	struct pcscp_softc *esc = (struct pcscp_softc *)sc;

	return bus_space_read_1(esc->sc_st, esc->sc_sh, reg << 2);
}

void
pcscp_write_reg(sc, reg, v)
	struct ncr53c9x_softc *sc;
	int reg;
	u_char v;
{
	struct pcscp_softc *esc = (struct pcscp_softc *)sc;

	bus_space_write_1(esc->sc_st, esc->sc_sh, reg << 2, v);
}

int
pcscp_dma_isintr(sc)
	struct ncr53c9x_softc *sc;
{

	return NCR_READ_REG(sc, NCR_STAT) & NCRSTAT_INT;
}

void
pcscp_dma_reset(sc)
	struct ncr53c9x_softc *sc;
{
	struct pcscp_softc *esc = (struct pcscp_softc *)sc;

	WRITE_DMAREG(esc, DMA_CMD, DMACMD_IDLE);

	esc->sc_active = 0;
}

int
pcscp_dma_intr(sc)
	struct ncr53c9x_softc *sc;
{
	struct pcscp_softc *esc = (struct pcscp_softc *)sc;
	int trans, resid, i;
	bus_dmamap_t dmap = esc->sc_xfermap;
	int datain = esc->sc_datain;
	u_int32_t dmastat;
	char *p = NULL;

	dmastat = READ_DMAREG(esc, DMA_STAT);

	if (dmastat & DMASTAT_ERR) {
		/* XXX not tested... */
		WRITE_DMAREG(esc, DMA_CMD,
		    DMACMD_ABORT | (datain ? DMACMD_DIR : 0));

		printf("%s: error: DMA error detected; Aborting.\n",
		    sc->sc_dev.dv_xname);
		bus_dmamap_unload(esc->sc_dmat, dmap);
		return -1;
	}

	if (dmastat & DMASTAT_ABT) {
		/* XXX What should be done? */
		printf("%s: dma_intr: DMA aborted.\n", sc->sc_dev.dv_xname);
		WRITE_DMAREG(esc, DMA_CMD,
		    DMACMD_IDLE | (datain ? DMACMD_DIR : 0));
		esc->sc_active = 0;
		return 0;
	}

	/* This is an "assertion" :) */
	if (esc->sc_active == 0)
		panic("pcscp dmaintr: DMA wasn't active");

	/* DMA has stopped */

	esc->sc_active = 0;

	if (esc->sc_dmasize == 0) {
		/* A "Transfer Pad" operation completed */
		NCR_DMA(("dmaintr: discarded %d bytes (tcl=%d, tcm=%d)\n",
		    NCR_READ_REG(sc, NCR_TCL) |
		    (NCR_READ_REG(sc, NCR_TCM) << 8),
		    NCR_READ_REG(sc, NCR_TCL),
		    NCR_READ_REG(sc, NCR_TCM)));
		return 0;
	}

	resid = 0;
	/*
	 * If a transfer onto the SCSI bus gets interrupted by the device
	 * (e.g. for a SAVEPOINTER message), the data in the FIFO counts
	 * as residual since the ESP counter registers get decremented as
	 * bytes are clocked into the FIFO.
	 */
	if (!datain &&
	    (resid = (NCR_READ_REG(sc, NCR_FFLAG) & NCRFIFO_FF)) != 0) {
		NCR_DMA(("pcscp_dma_intr: empty esp FIFO of %d ", resid));
	}

	if ((sc->sc_espstat & NCRSTAT_TC) == 0) {
		/*
		 * `Terminal count' is off, so read the residue
		 * out of the ESP counter registers.
		 */
		if (datain) {
			resid = NCR_READ_REG(sc, NCR_FFLAG) & NCRFIFO_FF;
			while (resid > 1)
				resid =
				    NCR_READ_REG(sc, NCR_FFLAG) & NCRFIFO_FF;
			WRITE_DMAREG(esc, DMA_CMD, DMACMD_BLAST | DMACMD_MDL |
			    (datain ? DMACMD_DIR : 0));

			for (i = 0; i < 0x8000; i++) /* XXX 0x8000 ? */
				if (READ_DMAREG(esc, DMA_STAT) & DMASTAT_BCMP)
					break;

			/* See the below comments... */
			if (resid)
				p = *esc->sc_dmaaddr;
		}
		
		resid += (NCR_READ_REG(sc, NCR_TCL) |
		    (NCR_READ_REG(sc, NCR_TCM) << 8) |
		    ((sc->sc_cfg2 & NCRCFG2_FE)
		    ? (NCR_READ_REG(sc, NCR_TCH) << 16) : 0));

		if (resid == 0 && esc->sc_dmasize == 65536 &&
		    (sc->sc_cfg2 & NCRCFG2_FE) == 0)
			/* A transfer of 64K is encoded as `TCL=TCM=0' */
			resid = 65536;
	} else {
		while((dmastat & DMASTAT_DONE) == 0)
			dmastat = READ_DMAREG(esc, DMA_STAT);
	}

	WRITE_DMAREG(esc, DMA_CMD, DMACMD_IDLE | (datain ? DMACMD_DIR : 0));

	pcscp_bus_dmamap_sync(esc->sc_dmat, dmap, 0, dmap_>dm_mapsize,
	    datain ? BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(esc->sc_dmat, dmap);

	trans = esc->sc_dmasize - resid;

	/*
	 * From the technical manual notes:
	 *
	 * `In some odd byte conditions, one residual byte will be left
	 *  in the SCSI FIFO, and the FIFO flags will never count to 0.
	 *  When this happens, the residual byte should be retrieved
	 *  via PIO following completion of the BLAST operation.'
	 */
	
	if (p) {
		p += trans;
		*p = NCR_READ_REG(sc, NCR_FIFO);
		trans++;
	}

	if (trans < 0) {			/* transferred < 0 ? */
#if 0
		/*
		 * This situation can happen in perfectly normal operation
		 * if the ESP is reselected while using DMA to select
		 * another target.  As such, don't print the warning.
		 */
		printf("%s: xfer (%d) > req (%d)\n",
		    sc->sc_dev.dv_xname, trans, esc->sc_dmasize);
#endif
		trans = esc->sc_dmasize;
	}

	NCR_DMA(("dmaintr: tcl=%d, tcm=%d, tch=%d; trans=%d, resid=%d\n",
	    NCR_READ_REG(sc, NCR_TCL),
	    NCR_READ_REG(sc, NCR_TCM),
	    (sc->sc_cfg2 & NCRCFG2_FE) ? NCR_READ_REG(sc, NCR_TCH) : 0,
	    trans, resid));

	*esc->sc_dmalen -= trans;
	*esc->sc_dmaaddr += trans;

	return 0;
}

int
pcscp_dma_setup(sc, addr, len, datain, dmasize)
	struct ncr53c9x_softc *sc;
	caddr_t *addr;
	size_t *len;
	int datain;
	size_t *dmasize;
{
	struct pcscp_softc *esc = (struct pcscp_softc *)sc;
	bus_dmamap_t dmap = esc->sc_xfermap;
	u_int32_t *mdl;
	int error, nseg, seg;
	bus_addr_t s_offset, s_addr;
	long rest, count;

	WRITE_DMAREG(esc, DMA_CMD, DMACMD_IDLE | (datain ? DMACMD_DIR : 0));

	esc->sc_dmaaddr = addr;
	esc->sc_dmalen = len;
	esc->sc_dmasize = *dmasize;
	esc->sc_datain = datain;

#ifdef DIAGNOSTIC
	if ((*dmasize / MDL_SEG_SIZE) > MDL_SIZE)
		panic("pcscp: transfer size too large");
#endif

	/*
	 * No need to set up DMA in `Transfer Pad' operation.
	 * (case of *dmasize == 0)
	 */
	if (*dmasize == 0)
		return 0;

	error = bus_dmamap_load(esc->sc_dmat, dmap, *esc->sc_dmaaddr,
	    *esc->sc_dmalen, NULL,
	    sc->sc_nexus->xs->flags & SCSI_NOSLEEP ?
	    BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
	if (error) {
		printf("%s: unable to load dmamap, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		return error;
	}

	/* set transfer length */
	WRITE_DMAREG(esc, DMA_STC, *dmasize); 

	/* set up MDL */
	mdl = esc->sc_mdladdr;
	nseg = dmap->dm_nsegs;

	/* the first segment is possibly not aligned with 4k MDL boundary */
	count = dmap->dm_segs[0].ds_len;
	s_addr = dmap->dm_segs[0].ds_addr;
	s_offset = s_addr & MDL_SEG_OFFSET;
	s_addr -= s_offset;
	rest = MDL_SEG_SIZE - s_offset;

	/* set the first MDL and offset */
	WRITE_DMAREG(esc, DMA_SPA, s_offset); 
	*mdl++ = htole32(s_addr);
	count -= rest;
	
	/* rests of the first dmamap segment */
	while (count > 0) {
		s_addr += MDL_SEG_SIZE;
		*mdl++ = htole32(s_addr);
		count -= MDL_SEG_SIZE;
	}

	/* the rest dmamap segments are aligned with 4k boundary */
	for (seg = 1; seg < nseg; seg++) {
		count = dmap->dm_segs[seg].ds_len;
		s_addr = dmap->dm_segs[seg].ds_addr;

		/* first 4kbyte of each dmamap segment */
		*mdl++ = htole32(s_addr);
		count -= MDL_SEG_SIZE;

		/* trailing contiguous 4k frames of each dmamap segments */
		while (count > 0) {
			s_addr += MDL_SEG_SIZE;
			*mdl++ = htole32(s_addr);
			count -= MDL_SEG_SIZE;
		}
	}

	return 0;
}

void
pcscp_dma_go(sc)
	struct ncr53c9x_softc *sc;
{
	struct pcscp_softc *esc = (struct pcscp_softc *)sc;
	bus_dmamap_t dmap = esc->sc_xfermap, mdldmap = esc->sc_mdldmap;
	int datain = esc->sc_datain;

	/* No DMA transfer in Transfer Pad operation */
	if (esc->sc_dmasize == 0)
		return;

	/* sync transfer buffer */
	pcscp_bus_dmamap_sync(esc->sc_dmat, dmap, 0, dmap->dm_mapsize,
	    datain ? BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

	/* sync MDL */
	pcscp_bus_dmamap_sync(esc->sc_dmat, mdldmap, 0, mdldmap->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	/* set Starting MDL Address */
	WRITE_DMAREG(esc, DMA_SMDLA, mdldmap->dm_segs[0].ds_addr);

	/* set DMA command register bits */
	/* XXX DMA Transfer Interrupt Enable bit is broken? */
	WRITE_DMAREG(esc, DMA_CMD, DMACMD_IDLE | DMACMD_MDL |
	    /* DMACMD_INTE | */
	    (datain ? DMACMD_DIR : 0));

	/* issue DMA start command */
	WRITE_DMAREG(esc, DMA_CMD, DMACMD_START | DMACMD_MDL |
	    /* DMACMD_INTE | */
	    (datain ? DMACMD_DIR : 0));

	esc->sc_active = 1;
}

void
pcscp_dma_stop(sc)
	struct ncr53c9x_softc *sc;
{
	struct pcscp_softc *esc = (struct pcscp_softc *)sc;

	/* dma stop */
	/* XXX What should we do here ? */
	WRITE_DMAREG(esc, DMA_CMD,
	    DMACMD_ABORT | (esc->sc_datain ? DMACMD_DIR : 0));

	esc->sc_active = 0;
}

int
pcscp_dma_isactive(sc)
	struct ncr53c9x_softc *sc;
{
	struct pcscp_softc *esc = (struct pcscp_softc *)sc;

	/* XXX should check esc->sc_active? */
	if ((READ_DMAREG(esc, DMA_CMD) & DMACMD_CMD) != DMACMD_IDLE)
		return 1;
	return 0;
}
