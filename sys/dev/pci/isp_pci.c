/*	$NetBSD: isp_pci.c,v 1.13 1997/06/08 06:34:52 thorpej Exp $	*/

/*
 * PCI specific probe and attach routines for Qlogic ISP SCSI adapters.
 *
 * Copyright (c) 1997 by Matthew Jacob 
 * NASA AMES Research Center
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/device.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>
#include <vm/vm.h>

#include <dev/ic/ispreg.h>
#include <dev/ic/ispvar.h>
#include <dev/ic/ispmbox.h>
#include <dev/microcode/isp/asm_pci.h>

static u_int16_t isp_pci_rd_reg __P((struct ispsoftc *, int));
static void isp_pci_wr_reg __P((struct ispsoftc *, int, u_int16_t));
static int isp_pci_mbxdma __P((struct ispsoftc *));
static int isp_pci_dmasetup __P((struct ispsoftc *, struct scsi_xfer *,
	ispreq_t *, u_int8_t *, u_int8_t));
static void isp_pci_dmateardown __P((struct ispsoftc *, struct scsi_xfer *,
	u_int32_t));

static void isp_pci_reset1 __P((struct ispsoftc *));

static struct ispmdvec mdvec = {
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	NULL,
	isp_pci_reset1,
	ISP_RISC_CODE,
	ISP_CODE_LENGTH,
	ISP_CODE_ORG,
	BIU_PCI_CONF1_FIFO_16 | BIU_BURST_ENABLE,
	60	/* MAGIC- all known PCI card implementations are 60MHz */
};

#define	PCI_QLOGIC_ISP	\
	((PCI_PRODUCT_QLOGIC_ISP1020 << 16) | PCI_VENDOR_QLOGIC)

#define IO_MAP_REG	0x10
#define MEM_MAP_REG	0x14


static int isp_pci_probe __P((struct device *, void *, void *));
static void isp_pci_attach __P((struct device *, struct device *, void *));

struct isp_pcisoftc {
	struct ispsoftc		pci_isp;
	bus_space_tag_t		pci_st;
	bus_space_handle_t	pci_sh;
	bus_dma_tag_t		pci_dmat;
	bus_dmamap_t		pci_rquest_dmap;
	bus_dmamap_t		pci_result_dmap;
	bus_dmamap_t		pci_xfer_dmap[RQUEST_QUEUE_LEN];
	void *			pci_ih;
};

struct cfattach isp_pci_ca = {
	sizeof (struct isp_pcisoftc), isp_pci_probe, isp_pci_attach
};

static int
isp_pci_probe(parent, match, aux)
        struct device *parent;
        void *match, *aux; 
{       
        struct pci_attach_args *pa = aux;

	if (pa->pa_id == PCI_QLOGIC_ISP) {
		return (1);
	} else {
		return (0);
	}
}


static void    
isp_pci_attach(parent, self, aux)
        struct device *parent, *self;
        void *aux;
{
	struct pci_attach_args *pa = aux;
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) self;
	bus_space_tag_t st, iot, memt;
	bus_space_handle_t sh, ioh, memh;
	pci_intr_handle_t ih;
	const char *intrstr;
	int ioh_valid, memh_valid;
	int i;
#ifdef __OpenBSD__
	bus_addr_t iobase;
	bus_size_t iosize;
#endif

#if 0
	ioh_valid = (pci_mapreg_map(pa, IO_MAP_REG,
	    PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, NULL, NULL) == 0);
#else
	if (pci_io_find(pa->pa_pc, pa->pa_tag, IO_MAP_REG, &iobase, &iosize)) {
		printf(": can't find i/o space\n");
		return;
	}

	if (bus_space_map(pa->pa_iot, iobase, iosize, 0, &ioh)) {
		printf(": can't map i/o space\n");
		return;
	}
	iot = pa->pa_iot;
	ioh_valid = 1;
#endif

#if 0
	memh_valid = (pci_mapreg_map(pa, MEM_MAP_REG,
	    PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &memt, &memh, NULL, NULL) == 0);
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
		printf(": unable to map device registers\n");
		return;
	}

	printf(": ");

	pcs->pci_st = st;
	pcs->pci_sh = sh;
	pcs->pci_dmat = pa->pa_dmat;
	pcs->pci_isp.isp_mdvec = &mdvec;
	isp_reset(&pcs->pci_isp);
	if (pcs->pci_isp.isp_state != ISP_RESETSTATE) {
		return;
	}
	isp_init(&pcs->pci_isp);
	if (pcs->pci_isp.isp_state != ISP_INITSTATE) {
		isp_uninit(&pcs->pci_isp);
		return;
	}

	if (pci_intr_map(pa->pa_pc, pa->pa_intrtag, pa->pa_intrpin,
			 pa->pa_intrline, &ih)) {
		printf(" couldn't map interrupt\n");
		isp_uninit(&pcs->pci_isp);
		return;
	}

	intrstr = pci_intr_string(pa->pa_pc, ih);
	if (intrstr == NULL)
		intrstr = "<I dunno>";
#ifndef __OpenBSD__
	pcs->pci_ih =
	  pci_intr_establish(pa->pa_pc, ih, IPL_BIO, isp_intr, &pcs->pci_isp);
#else
	pcs->pci_ih =
	  pci_intr_establish(pa->pa_pc, ih, IPL_BIO, isp_intr, &pcs->pci_isp,
	  pcs->pci_isp.isp_dev.dv_xname);
#endif
	if (pcs->pci_ih == NULL) {
		printf(" couldn't establish interrupt at %s\n");
		isp_uninit(&pcs->pci_isp);
		return;
	}
	printf("%s\n", intrstr);

	/*
	 * Create the DMA maps for the data transfers.
	 */
	for (i = 0; i < RQUEST_QUEUE_LEN; i++) {
		if (bus_dmamap_create(pcs->pci_dmat, MAXPHYS,
		    (MAXPHYS / NBPG) + 1, MAXPHYS, 0, BUS_DMA_NOWAIT,
		    &pcs->pci_xfer_dmap[i])) {
			printf("%s: can't create dma maps\n",
			    pcs->pci_isp.isp_name);
			isp_uninit(&pcs->pci_isp);
			return;
		}
	}

	/*
	 * Do Generic attach now.
	 */
	isp_attach(&pcs->pci_isp);
	if (pcs->pci_isp.isp_state != ISP_RUNSTATE) {
		isp_uninit(&pcs->pci_isp);
	}
}

#define  PCI_BIU_REGS_OFF		0x00
#define	 PCI_MBOX_REGS_OFF		0x70
#define	 PCI_SXP_REGS_OFF		0x80
#define	 PCI_RISC_REGS_OFF		0x80

static u_int16_t
isp_pci_rd_reg(isp, regoff)
	struct ispsoftc *isp;
	int regoff;
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	int offset;
	if ((regoff & BIU_BLOCK) != 0) {
		offset = PCI_BIU_REGS_OFF;
	} else if ((regoff & MBOX_BLOCK) != 0) {
		offset = PCI_MBOX_REGS_OFF;
	} else if ((regoff & SXP_BLOCK) != 0) {
		offset = PCI_SXP_REGS_OFF;
		/*
		 * XXX
		 */
		panic("SXP Registers not accessible yet!");
	} else {
		offset = PCI_RISC_REGS_OFF;
	}
	regoff &= 0xff;
	offset += regoff;
	return bus_space_read_2(pcs->pci_st, pcs->pci_sh, offset);
}

static void
isp_pci_wr_reg(isp, regoff, val)
	struct ispsoftc *isp;
	int regoff;
	u_int16_t val;
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	int offset;
	if ((regoff & BIU_BLOCK) != 0) {
		offset = PCI_BIU_REGS_OFF;
	} else if ((regoff & MBOX_BLOCK) != 0) {
		offset = PCI_MBOX_REGS_OFF;
	} else if ((regoff & SXP_BLOCK) != 0) {
		offset = PCI_SXP_REGS_OFF;
		/*
		 * XXX
		 */
		panic("SXP Registers not accessible yet!");
	} else {
		offset = PCI_RISC_REGS_OFF;
	}
	regoff &= 0xff;
	offset += regoff;
	bus_space_write_2(pcs->pci_st, pcs->pci_sh, offset, val);
}

static int
isp_pci_mbxdma(isp)
	struct ispsoftc *isp;
{
	struct isp_pcisoftc *pci = (struct isp_pcisoftc *)isp;
	bus_dma_segment_t seg;
	bus_size_t len;
	int rseg;

	/*
	 * Allocate and map the request queue.
	 */
	len = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN);
	if (bus_dmamem_alloc(pci->pci_dmat, len, NBPG, 0, &seg, 1, &rseg,
	      BUS_DMA_NOWAIT) ||
	    bus_dmamem_map(pci->pci_dmat, &seg, rseg, len,
	      (caddr_t *)&isp->isp_rquest, BUS_DMA_NOWAIT|BUS_DMAMEM_NOSYNC))
		return (1);
	if (bus_dmamap_create(pci->pci_dmat, len, 1, len, 0, BUS_DMA_NOWAIT,
	      &pci->pci_rquest_dmap) ||
	    bus_dmamap_load(pci->pci_dmat, pci->pci_rquest_dmap,
	      (caddr_t)isp->isp_rquest, len, NULL, BUS_DMA_NOWAIT))
		return (1);

	isp->isp_rquest_dma = pci->pci_rquest_dmap->dm_segs[0].ds_addr;

	/*
	 * Allocate and map the result queue.
	 */
	len = ISP_QUEUE_SIZE(RESULT_QUEUE_LEN);
	if (bus_dmamem_alloc(pci->pci_dmat, len, NBPG, 0, &seg, 1, &rseg,
	      BUS_DMA_NOWAIT) ||
	    bus_dmamem_map(pci->pci_dmat, &seg, rseg, len,
	      (caddr_t *)&isp->isp_result, BUS_DMA_NOWAIT|BUS_DMAMEM_NOSYNC))
		return (1);
	if (bus_dmamap_create(pci->pci_dmat, len, 1, len, 0, BUS_DMA_NOWAIT,
	      &pci->pci_result_dmap) ||
	    bus_dmamap_load(pci->pci_dmat, pci->pci_result_dmap,
	      (caddr_t)isp->isp_result, len, NULL, BUS_DMA_NOWAIT))
		return (1);

	isp->isp_result_dma = pci->pci_result_dmap->dm_segs[0].ds_addr;

	return (0);
}

static int
isp_pci_dmasetup(isp, xs, rq, iptrp, optr)
	struct ispsoftc *isp;
	struct scsi_xfer *xs;
	ispreq_t *rq;
	u_int8_t *iptrp;
	u_int8_t optr;
{
	struct isp_pcisoftc *pci = (struct isp_pcisoftc *)isp;
	bus_dmamap_t dmap = pci->pci_xfer_dmap[rq->req_handle];
	ispcontreq_t *crq;
	int segcnt, seg, error, ovseg;

	if (xs->datalen == 0) {
		rq->req_seg_count = 1;
		rq->req_flags |= REQFLAG_DATA_IN;
		return (0);
	}

	if (rq->req_handle >= RQUEST_QUEUE_LEN) {
		panic("%s: bad handle (%d) in isp_pci_dmasetup\n",
		    isp->isp_name, rq->req_handle);
		/* NOTREACHED */
	}

	if (xs->flags & SCSI_DATA_IN) {
		rq->req_flags |= REQFLAG_DATA_IN;
	} else {
		rq->req_flags |= REQFLAG_DATA_OUT;
	}

	error = bus_dmamap_load(pci->pci_dmat, dmap, xs->data, xs->datalen,
	    NULL, xs->flags & SCSI_NOSLEEP ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
	if (error)
		return (error);

	segcnt = dmap->dm_nsegs;

	for (seg = 0, rq->req_seg_count = 0;
	    seg < segcnt && rq->req_seg_count < ISP_RQDSEG;
	    seg++, rq->req_seg_count++) {
		rq->req_dataseg[rq->req_seg_count].ds_count =
		    dmap->dm_segs[seg].ds_len;
		rq->req_dataseg[rq->req_seg_count].ds_base =
		    dmap->dm_segs[seg].ds_addr;
	}

	if (seg == segcnt)
		goto mapsync;

	do {
		crq = (ispcontreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest,
		    *iptrp);
		*iptrp = (*iptrp + 1) & (RQUEST_QUEUE_LEN - 1);
		if (*iptrp == optr) {
			printf("%s: Request Queue Overflow++\n",
			       isp->isp_name);
			bus_dmamap_unload(pci->pci_dmat, dmap);
			return (EFBIG);
		}
		rq->req_header.rqs_entry_count++;
		bzero((void *)crq, sizeof (*crq));
		crq->req_header.rqs_entry_count = 1;
		crq->req_header.rqs_entry_type = RQSTYPE_DATASEG;

		for (ovseg = 0; seg < segcnt && ovseg < ISP_CDSEG;
		    rq->req_seg_count++, seg++, ovseg++) {
			crq->req_dataseg[ovseg].ds_count =
			    dmap->dm_segs[seg].ds_len;
			crq->req_dataseg[ovseg].ds_base =
			    dmap->dm_segs[seg].ds_addr;
		}
	} while (seg < segcnt);

 mapsync:
	bus_dmamap_sync(pci->pci_dmat, dmap, xs->flags & SCSI_DATA_IN ?
	    BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);
	return (0);
}

static void
isp_pci_dmateardown(isp, xs, handle)
	struct ispsoftc *isp;
	struct scsi_xfer *xs;
	u_int32_t handle;
{
	struct isp_pcisoftc *pci = (struct isp_pcisoftc *)isp;
	bus_dmamap_t dmap = pci->pci_xfer_dmap[handle];

	bus_dmamap_sync(pci->pci_dmat, dmap, xs->flags & SCSI_DATA_IN ?
	    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);

	bus_dmamap_unload(pci->pci_dmat, dmap);
}

static void
isp_pci_reset1(isp)
	struct ispsoftc *isp;
{
	/* Make sure the BIOS is disabled */
	isp_pci_wr_reg(isp, HCCR, PCI_HCCR_CMD_BIOS);
}
