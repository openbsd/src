/*	$OpenBSD: isp_pci.c,v 1.6 1999/03/25 22:58:44 mjacob Exp $	*/
/* release_03_25_99 */
/*
 * PCI specific probe and attach routines for Qlogic ISP SCSI adapters.
 *
 *---------------------------------------
 * Copyright (c) 1997, 1998, 1999 by Matthew Jacob
 * NASA/Ames Research Center
 * All rights reserved.
 *---------------------------------------
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
 *
 */

#include <dev/ic/isp_openbsd.h>
#include <dev/microcode/isp/asm_pci.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

static u_int16_t isp_pci_rd_reg __P((struct ispsoftc *, int));
static void isp_pci_wr_reg __P((struct ispsoftc *, int, u_int16_t));
#ifndef	ISP_DISABLE_1080_SUPPORT
static u_int16_t isp_pci_rd_reg_1080 __P((struct ispsoftc *, int));
static void isp_pci_wr_reg_1080 __P((struct ispsoftc *, int, u_int16_t));
#endif
static int isp_pci_mbxdma __P((struct ispsoftc *));
static int isp_pci_dmasetup __P((struct ispsoftc *, struct scsi_xfer *,
	ispreq_t *, u_int8_t *, u_int8_t));
static void
isp_pci_dmateardown __P((struct ispsoftc *, struct scsi_xfer *, u_int32_t));
static void isp_pci_reset1 __P((struct ispsoftc *));
static void isp_pci_dumpregs __P((struct ispsoftc *));
static int isp_pci_intr __P((void *));

#ifndef	ISP_DISABLE_1020_SUPPORT
static struct ispmdvec mdvec = {
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	NULL,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_RISC_CODE,
	ISP_CODE_LENGTH,
	ISP_CODE_ORG,
	ISP_CODE_VERSION,
	BIU_BURST_ENABLE|BIU_PCI_CONF1_FIFO_64,
	0
};
#endif

#ifndef	ISP_DISABLE_1080_SUPPORT
static struct ispmdvec mdvec_1080 = {
	isp_pci_rd_reg_1080,
	isp_pci_wr_reg_1080,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	NULL,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP1080_RISC_CODE,
	ISP1080_CODE_LENGTH,
	ISP1080_CODE_ORG,
	ISP1080_CODE_VERSION,
	BIU_BURST_ENABLE|BIU_PCI_CONF1_FIFO_64,
	0
};
#endif

#ifndef	ISP_DISABLE_2100_SUPPORT
static struct ispmdvec mdvec_2100 = {
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	NULL,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP2100_RISC_CODE,
	ISP2100_CODE_LENGTH,
	ISP2100_CODE_ORG,
	ISP2100_CODE_VERSION,
	0,				/* Irrelevant to the 2100 */
	0
};
#endif

#ifndef	PCI_VENDOR_QLOGIC
#define	PCI_VENDOR_QLOGIC	0x1077
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP1020
#define	PCI_PRODUCT_QLOGIC_ISP1020	0x1020
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP1080
#define	PCI_PRODUCT_QLOGIC_ISP1080	0x1080
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP1240
#define	PCI_PRODUCT_QLOGIC_ISP1240	0x1240
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2100
#define	PCI_PRODUCT_QLOGIC_ISP2100	0x2100
#endif

#define	PCI_QLOGIC_ISP	((PCI_PRODUCT_QLOGIC_ISP1020 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP1080	\
	((PCI_PRODUCT_QLOGIC_ISP1080 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP1240	\
	((PCI_PRODUCT_QLOGIC_ISP1240 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2100	\
	((PCI_PRODUCT_QLOGIC_ISP2100 << 16) | PCI_VENDOR_QLOGIC)

#define IO_MAP_REG	0x10
#define MEM_MAP_REG	0x14

#ifndef SCSI_ISP_PREFER_MEM_MAP
#ifdef  __alpha__
#define SCSI_ISP_PREFER_MEM_MAP 0
#else
#define SCSI_ISP_PREFER_MEM_MAP 1
#endif
#endif

#ifndef	BUS_DMA_COHERENT
#define	BUS_DMA_COHERENT	BUS_DMAMEM_NOSYNC
#endif


static int isp_pci_probe __P((struct device *, void *, void *));
static void isp_pci_attach __P((struct device *, struct device *, void *));

struct isp_pcisoftc {
	struct ispsoftc		pci_isp;
	pci_chipset_tag_t	pci_pc;
	pcitag_t		pci_tag;
	bus_space_tag_t		pci_st;
	bus_space_handle_t	pci_sh;
	bus_dma_tag_t		pci_dmat;
	bus_dmamap_t		pci_scratch_dmap;	/* for fcp only */
	bus_dmamap_t		pci_rquest_dmap;
	bus_dmamap_t		pci_result_dmap;
	bus_dmamap_t		pci_xfer_dmap[MAXISPREQUEST];
	void *			pci_ih;
	int16_t			pci_poff[_NREG_BLKS];
};

struct cfattach isp_pci_ca = {
	sizeof (struct isp_pcisoftc), isp_pci_probe, isp_pci_attach
};

static int
isp_pci_probe(parent, match, aux)
        struct device *parent;
        void *match;
	void *aux; 
{
        struct pci_attach_args *pa = aux;

        switch (pa->pa_id) {
#ifndef	ISP_DISABLE_1020_SUPPORT
	case PCI_QLOGIC_ISP:
		return (1);
#endif
#ifndef	ISP_DISABLE_1080_SUPPORT
	case PCI_QLOGIC_ISP1080:
#if	0
	case PCI_QLOGIC_ISP1240:	/* 1240 not ready yet */
		return (1);
#endif
#endif
#ifndef	ISP_DISABLE_2100_SUPPORT
	case PCI_QLOGIC_ISP2100:
		return (1);
#endif
	default:
		return (0);
	}
}


static void    
isp_pci_attach(parent, self, aux)
        struct device *parent, *self;
        void *aux;
{
#ifdef	DEBUG
	static char oneshot = 1;
#endif
	u_int32_t data;
	struct pci_attach_args *pa = aux;
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) self;
	struct ispsoftc *isp = &pcs->pci_isp;
	bus_space_tag_t st, iot, memt;
	bus_space_handle_t sh, ioh, memh;
	pci_intr_handle_t ih;
	const char *intrstr;
	int ioh_valid, memh_valid, i, s;
	bus_addr_t iobase, mbase;
	bus_size_t iosize, msize;

	ioh_valid = memh_valid = 0;

#if	SCSI_ISP_PREFER_MEM_MAP == 1
	if (pci_mem_find(pa->pa_pc, pa->pa_tag, MEM_MAP_REG, &mbase, &msize,
	    NULL)) {
		printf(": can't find mem space\n");
	} else if (bus_space_map(pa->pa_memt, mbase, msize, 0, &memh)) {
		printf(": can't map mem space\n");
	} else  {
		memt = pa->pa_memt;
		st = memt;
		sh = memh;
		memh_valid = 1;
	}
	if (memh_valid == 0) {
		if (pci_io_find(pa->pa_pc, pa->pa_tag, IO_MAP_REG, &iobase,
		    &iosize)) {
			printf(": can't find i/o space\n");
		} else if (bus_space_map(pa->pa_iot, iobase, iosize, 0, &ioh)) {
			printf(": can't map i/o space\n");
		} else {
			iot = pa->pa_iot;
			st = iot;
			sh = ioh;
			ioh_valid = 1;
		}
	}
#else
	if (pci_io_find(pa->pa_pc, pa->pa_tag, IO_MAP_REG, &iobase, &iosize)) {
		printf(": can't find i/o space\n");
	} else if (bus_space_map(pa->pa_iot, iobase, iosize, 0, &ioh)) {
		printf(": can't map i/o space\n");
	} else {
		iot = pa->pa_iot;
		st = iot;
		sh = ioh;
		ioh_valid = 1;
	}
	if (ioh_valid == 0) {
		if (pci_mem_find(pa->pa_pc, pa->pa_tag, MEM_MAP_REG, &mbase,
		    &msize, NULL)) {
			printf(": can't find mem space\n");
		} else if (bus_space_map(pa->pa_memt, mbase, msize, 0, &memh)) {
			printf(": can't map mem space\n");
		} else  {
			memt = pa->pa_memt;
			st = memt;
			sh = memh;
			memh_valid = 1;
		}
	}
#endif
	if (ioh_valid == 0 && memh_valid == 0) {
		printf(": unable to map device registers\n");
		return;
	}
	printf("\n");

	pcs->pci_st = st;
	pcs->pci_sh = sh;
	pcs->pci_dmat = pa->pa_dmat;
	pcs->pci_pc = pa->pa_pc;
	pcs->pci_tag = pa->pa_tag;
	pcs->pci_poff[BIU_BLOCK >> _BLK_REG_SHFT] = BIU_REGS_OFF;
	pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = PCI_MBOX_REGS_OFF;
	pcs->pci_poff[SXP_BLOCK >> _BLK_REG_SHFT] = PCI_SXP_REGS_OFF;
	pcs->pci_poff[RISC_BLOCK >> _BLK_REG_SHFT] = PCI_RISC_REGS_OFF;
	pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] = DMA_REGS_OFF;
#ifndef	ISP_DISABLE_1020_SUPPORT
	if (pa->pa_id == PCI_QLOGIC_ISP) {
		isp->isp_mdvec = &mdvec;
		isp->isp_type = ISP_HA_SCSI_UNKNOWN;
		isp->isp_param = malloc(sizeof (sdparam), M_DEVBUF, M_NOWAIT);
		if (isp->isp_param == NULL) {
			printf("%s: couldn't allocate sdparam table\n",
			       isp->isp_name);
			return;
		}
		bzero(isp->isp_param, sizeof (sdparam));
	}
#endif
#ifndef	ISP_DISABLE_1080_SUPPORT
	if (pa->pa_id == PCI_QLOGIC_ISP1080) {
		isp->isp_mdvec = &mdvec_1080;
		isp->isp_type = ISP_HA_SCSI_1080;
		isp->isp_param = malloc(sizeof (sdparam), M_DEVBUF, M_NOWAIT);
		if (isp->isp_param == NULL) {
			printf("%s: couldn't allocate sdparam table\n",
			       isp->isp_name);
			return;
		}
		bzero(isp->isp_param, sizeof (sdparam));
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    ISP1080_DMA_REGS_OFF;
	}
#endif
#ifndef	ISP_DISABLE_2100_SUPPORT
	if (pa->pa_id == PCI_QLOGIC_ISP2100) {
		isp->isp_mdvec = &mdvec_2100;
		isp->isp_type = ISP_HA_FC_2100;
		isp->isp_param = malloc(sizeof (fcparam), M_DEVBUF, M_NOWAIT);
		if (isp->isp_param == NULL) {
			printf("%s: couldn't allocate fcparam table\n",
			       isp->isp_name);
			return;
		}
		bzero(isp->isp_param, sizeof (fcparam));
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2100_OFF;
	}
#endif
	/*
	 * Make sure that command register set sanely.
	 */
	data = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	data |= PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_INVALIDATE_ENABLE;

	/*
	 * Not so sure about these- but I think it's important that they get
	 * enabled......
	 */
	data |= PCI_COMMAND_PARITY_ENABLE | PCI_COMMAND_SERR_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, data);

	/*
	 * Make sure that latency timer and cache line size is set sanely.
	 */
	data = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG);
	data &= ~(PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT);
	data &= ~(PCI_CACHELINE_MASK << PCI_CACHELINE_SHIFT);
	data |= (0x40 << PCI_LATTIMER_SHIFT);
	data |= (0x10 << PCI_CACHELINE_SHIFT);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG, data);

#ifdef DEBUG
	if (oneshot) {
		oneshot = 0;
		printf("Qlogic ISP Driver, OpenBSD (pci) Platform Version "
		    "%d.%d Core Version %d.%d\n",
		    ISP_PLATFORM_VERSION_MAJOR, ISP_PLATFORM_VERSION_MINOR,
		    ISP_CORE_VERSION_MAJOR, ISP_CORE_VERSION_MINOR);
	}
#endif

	if (pci_intr_map(pa->pa_pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf("%s: couldn't map interrupt\n", isp->isp_name);
		free(isp->isp_param, M_DEVBUF);
		return;
	}

	intrstr = pci_intr_string(pa->pa_pc, ih);
	if (intrstr == NULL)
		intrstr = "<I dunno>";
	pcs->pci_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, isp_pci_intr,
	    isp, isp->isp_name);
	if (pcs->pci_ih == NULL) {
		printf("%s: couldn't establish interrupt at %s\n",
			isp->isp_name, intrstr);
		free(isp->isp_param, M_DEVBUF);
		return;
	}
	printf("%s: interrupting at %s\n", isp->isp_name, intrstr);

	/*
	 * Create the DMA maps for the data transfers.
	 */
	for (i = 0; i < RQUEST_QUEUE_LEN; i++) {
		if (bus_dmamap_create(pcs->pci_dmat, MAXPHYS,
		    (MAXPHYS / NBPG) + 1, MAXPHYS, 0, BUS_DMA_NOWAIT,
		    &pcs->pci_xfer_dmap[i])) {
			printf("%s: can't create dma maps\n", isp->isp_name);
			free(isp->isp_param, M_DEVBUF);
			return;
		}
	}

	s = splhigh();
	isp_reset(isp);
	if (isp->isp_state != ISP_RESETSTATE) {
		free(isp->isp_param, M_DEVBUF);
		(void) splx(s);
		return;
	}
	isp_init(isp);
	if (isp->isp_state != ISP_INITSTATE) {
		isp_uninit(isp);
		free(isp->isp_param, M_DEVBUF);
		(void) splx(s);
		return;
	}
	/*
	 * Do Generic attach now.
	 */
	isp_attach(isp);
	if (isp->isp_state != ISP_RUNSTATE) {
		isp_uninit(isp);
		free(isp->isp_param, M_DEVBUF);
	}
	(void) splx(s);
}

static u_int16_t
isp_pci_rd_reg(isp, regoff)
	struct ispsoftc *isp;
	int regoff;
{
	u_int16_t rv;
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	int offset, oldconf = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oldconf = isp_pci_rd_reg(isp, BIU_CONF1);
		isp_pci_wr_reg(isp, BIU_CONF1, oldconf | BIU_PCI_CONF1_SXP);
	}
	offset = pcs->pci_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	rv = bus_space_read_2(pcs->pci_st, pcs->pci_sh, offset);
	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		isp_pci_wr_reg(isp, BIU_CONF1, oldconf);
	}
	return (rv);
}

static void
isp_pci_wr_reg(isp, regoff, val)
	struct ispsoftc *isp;
	int regoff;
	u_int16_t val;
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	int offset, oldconf = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oldconf = isp_pci_rd_reg(isp, BIU_CONF1);
		isp_pci_wr_reg(isp, BIU_CONF1, oldconf | BIU_PCI_CONF1_SXP);
	}
	offset = pcs->pci_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	bus_space_write_2(pcs->pci_st, pcs->pci_sh, offset, val);
	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		isp_pci_wr_reg(isp, BIU_CONF1, oldconf);
	}
}

#ifndef	ISP_DISABLE_1080_SUPPORT
static u_int16_t
isp_pci_rd_reg_1080(isp, regoff)
	struct ispsoftc *isp;
	int regoff;
{
	u_int16_t rv;
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	int offset, oc = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oc = isp_pci_rd_reg(isp, BIU_CONF1);
		isp_pci_wr_reg(isp, BIU_CONF1, oc | BIU_PCI1080_CONF1_SXP);
	} else if ((regoff & _BLK_REG_MASK) == DMA_BLOCK) {
		oc = isp_pci_rd_reg(isp, BIU_CONF1);
		isp_pci_wr_reg(isp, BIU_CONF1, oc | BIU_PCI1080_CONF1_DMA);
	}
	offset = pcs->pci_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	rv = bus_space_read_2(pcs->pci_st, pcs->pci_sh, offset);
	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK ||
	    ((regoff & _BLK_REG_MASK) == DMA_BLOCK)) {
		isp_pci_wr_reg(isp, BIU_CONF1, oc);
	}
	return (rv);
}

static void
isp_pci_wr_reg_1080(isp, regoff, val)
	struct ispsoftc *isp;
	int regoff;
	u_int16_t val;
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	int offset, oc = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oc = isp_pci_rd_reg(isp, BIU_CONF1);
		isp_pci_wr_reg(isp, BIU_CONF1, oc | BIU_PCI1080_CONF1_SXP);
	} else if ((regoff & _BLK_REG_MASK) == DMA_BLOCK) {
		oc = isp_pci_rd_reg(isp, BIU_CONF1);
		isp_pci_wr_reg(isp, BIU_CONF1, oc | BIU_PCI1080_CONF1_DMA);
	}
	offset = pcs->pci_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	bus_space_write_2(pcs->pci_st, pcs->pci_sh, offset, val);
	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK ||
	    ((regoff & _BLK_REG_MASK) == DMA_BLOCK)) {
		isp_pci_wr_reg(isp, BIU_CONF1, oc);
	}
}
#endif


static int
isp_pci_mbxdma(isp)
	struct ispsoftc *isp;
{
	struct isp_pcisoftc *pci = (struct isp_pcisoftc *)isp;
	bus_dma_segment_t seg;
	bus_size_t len;
	fcparam *fcp;
	int rseg;

	/*
	 * Allocate and map the request queue.
	 */
	len = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN);
	if (bus_dmamem_alloc(pci->pci_dmat, len, NBPG, 0, &seg, 1, &rseg,
	      BUS_DMA_NOWAIT) ||
	    bus_dmamem_map(pci->pci_dmat, &seg, rseg, len,
	      (caddr_t *)&isp->isp_rquest, BUS_DMA_NOWAIT|BUS_DMA_COHERENT))
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
	      (caddr_t *)&isp->isp_result, BUS_DMA_NOWAIT|BUS_DMA_COHERENT))
		return (1);
	if (bus_dmamap_create(pci->pci_dmat, len, 1, len, 0, BUS_DMA_NOWAIT,
	      &pci->pci_result_dmap) ||
	    bus_dmamap_load(pci->pci_dmat, pci->pci_result_dmap,
	      (caddr_t)isp->isp_result, len, NULL, BUS_DMA_NOWAIT))
		return (1);
	isp->isp_result_dma = pci->pci_result_dmap->dm_segs[0].ds_addr;

	if (isp->isp_type & ISP_HA_SCSI) {
		return (0);
	}

	fcp = isp->isp_param;
	len = ISP2100_SCRLEN;
	if (bus_dmamem_alloc(pci->pci_dmat, len, NBPG, 0, &seg, 1, &rseg,
		BUS_DMA_NOWAIT) ||
	    bus_dmamem_map(pci->pci_dmat, &seg, rseg, len,
	      (caddr_t *)&fcp->isp_scratch, BUS_DMA_NOWAIT|BUS_DMA_COHERENT))
		return (1);
	if (bus_dmamap_create(pci->pci_dmat, len, 1, len, 0, BUS_DMA_NOWAIT,
	      &pci->pci_scratch_dmap) ||
	    bus_dmamap_load(pci->pci_dmat, pci->pci_scratch_dmap,
	      (caddr_t)fcp->isp_scratch, len, NULL, BUS_DMA_NOWAIT))
		return (1);
	fcp->isp_scdma = pci->pci_scratch_dmap->dm_segs[0].ds_addr;
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
	bus_dmamap_t dmap = pci->pci_xfer_dmap[rq->req_handle - 1];
	ispcontreq_t *crq;
	int segcnt, seg, error, ovseg, seglim, drq;

	if (xs->datalen == 0) {
		rq->req_seg_count = 1;
		goto mbxsync;
	}

	if (rq->req_handle > RQUEST_QUEUE_LEN || rq->req_handle < 1) {
		panic("%s: bad handle (%d) in isp_pci_dmasetup\n",
		    isp->isp_name, rq->req_handle);
		/* NOTREACHED */
	}

	if (xs->flags & SCSI_DATA_IN) {
		drq = REQFLAG_DATA_IN;
	} else {
		drq = REQFLAG_DATA_OUT;
	}

	if (isp->isp_type & ISP_HA_FC) {
		seglim = ISP_RQDSEG_T2;
		((ispreqt2_t *)rq)->req_totalcnt = xs->datalen;
		((ispreqt2_t *)rq)->req_flags |= drq;
	} else {
		seglim = ISP_RQDSEG;
		rq->req_flags |= drq;
	}
	error = bus_dmamap_load(pci->pci_dmat, dmap, xs->data, xs->datalen,
	    NULL, xs->flags & SCSI_NOSLEEP ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
	if (error) {
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_COMPLETE);
	}

	segcnt = dmap->dm_nsegs;

	for (seg = 0, rq->req_seg_count = 0;
	     seg < segcnt && rq->req_seg_count < seglim;
	     seg++, rq->req_seg_count++) {
		if (isp->isp_type & ISP_HA_FC) {
			ispreqt2_t *rq2 = (ispreqt2_t *)rq;
			rq2->req_dataseg[rq2->req_seg_count].ds_count =
			    dmap->dm_segs[seg].ds_len;
			rq2->req_dataseg[rq2->req_seg_count].ds_base =
			    dmap->dm_segs[seg].ds_addr;
		} else {
			rq->req_dataseg[rq->req_seg_count].ds_count =
			    dmap->dm_segs[seg].ds_len;
			rq->req_dataseg[rq->req_seg_count].ds_base =
			    dmap->dm_segs[seg].ds_addr;
		}
	}

	if (seg == segcnt)
		goto dmasync;

	do {
		crq = (ispcontreq_t *)
			ISP_QUEUE_ENTRY(isp->isp_rquest, *iptrp);
		*iptrp = (*iptrp + 1) & (RQUEST_QUEUE_LEN - 1);
		if (*iptrp == optr) {
			printf("%s: Request Queue Overflow++\n",
			       isp->isp_name);
			bus_dmamap_unload(pci->pci_dmat, dmap);
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_COMPLETE);
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

dmasync:
	bus_dmamap_sync(pci->pci_dmat, dmap, (xs->flags & SCSI_DATA_IN) ?
	    BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

mbxsync:

	bus_dmamap_sync(pci->pci_dmat, pci->pci_rquest_dmap,
	    BUS_DMASYNC_PREWRITE);
	return (CMD_QUEUED);
}

static int
isp_pci_intr(arg)
	void *arg;
{
	struct isp_pcisoftc *pci = (struct isp_pcisoftc *)arg;
	bus_dmamap_sync(pci->pci_dmat, pci->pci_result_dmap,
	    BUS_DMASYNC_POSTREAD);
	return (isp_intr(arg));
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

static void
isp_pci_dumpregs(isp)
	struct ispsoftc *isp;
{
	struct isp_pcisoftc *pci = (struct isp_pcisoftc *)isp;
	printf("%s: PCI Status Command/Status=%x\n", pci->pci_isp.isp_name,
	    pci_conf_read(pci->pci_pc, pci->pci_tag, PCI_COMMAND_STATUS_REG));
}
