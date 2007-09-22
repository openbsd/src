/*	$OpenBSD: isp_pci.c,v 1.40 2007/09/22 16:03:23 krw Exp $	*/
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

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#ifdef __sparc64__
#include <dev/ofw/openfirm.h>
#endif

static u_int16_t isp_pci_rd_reg(struct ispsoftc *, int);
static void isp_pci_wr_reg(struct ispsoftc *, int, u_int16_t);
#if !(defined(ISP_DISABLE_1080_SUPPORT) && defined(ISP_DISABLE_12160_SUPPORT))
static u_int16_t isp_pci_rd_reg_1080(struct ispsoftc *, int);
static void isp_pci_wr_reg_1080(struct ispsoftc *, int, u_int16_t);
#endif
static int
isp_pci_rd_isr(struct ispsoftc *, u_int16_t *, u_int16_t *, u_int16_t *);
#ifndef	ISP_DISABLE_2300_SUPPORT
static int
isp_pci_rd_isr_2300(struct ispsoftc *, u_int16_t *, u_int16_t *, u_int16_t *);
#endif
static int isp_pci_mbxdma(struct ispsoftc *);
static int isp_pci_dmasetup(struct ispsoftc *, struct scsi_xfer *,
	ispreq_t *, u_int16_t *, u_int16_t);
static void
isp_pci_dmateardown (struct ispsoftc *, struct scsi_xfer *, u_int16_t);
static void isp_pci_reset1 (struct ispsoftc *);
static void isp_pci_dumpregs (struct ispsoftc *, const char *);
static int isp_pci_intr (void *);

#ifdef	ISP_COMPILE_FW
#define	ISP_COMPILE_1040_FW	1
#define	ISP_COMPILE_1080_FW	1
#define	ISP_COMPILE_12160_FW	1
#define	ISP_COMPILE_2100_FW	1
#define	ISP_COMPILE_2200_FW	1
#define	ISP_COMPILE_2300_FW	1
#endif

#if	defined(ISP_DISABLE_1040_SUPPORT) || !defined(ISP_COMPILE_1040_FW)
#define	ISP_1040_RISC_CODE	NULL
#else
#define	ISP_1040_RISC_CODE	(u_int16_t *) isp_1040_risc_code
#include <dev/microcode/isp/asm_1040.h>
#endif

#if	defined(ISP_DISABLE_1080_SUPPORT) || !defined(ISP_COMPILE_1080_FW)
#define	ISP_1080_RISC_CODE	NULL
#else
#define	ISP_1080_RISC_CODE	(u_int16_t *) isp_1080_risc_code
#include <dev/microcode/isp/asm_1080.h>
#endif

#if	defined(ISP_DISABLE_12160_SUPPORT) || !defined(ISP_COMPILE_12160_FW)
#define	ISP_12160_RISC_CODE	(u_int16_t *) NULL
#else
#define	ISP_12160_RISC_CODE	(u_int16_t *) isp_12160_risc_code
#include <dev/microcode/isp/asm_12160.h>
#endif

#if	defined(ISP_DISABLE_2100_SUPPORT) || !defined(ISP_COMPILE_2100_FW)
#define	ISP_2100_RISC_CODE	NULL
#else
#define	ISP_2100_RISC_CODE	(u_int16_t *) isp_2100_risc_code
#include <dev/microcode/isp/asm_2100.h>
#endif

#if	defined(ISP_DISABLE_2200_SUPPORT) || !defined(ISP_COMPILE_2200_FW)
#define	ISP_2200_RISC_CODE	NULL
#else
#define	ISP_2200_RISC_CODE	(u_int16_t *) isp_2200_risc_code
#include <dev/microcode/isp/asm_2200.h>
#endif

#if	defined(ISP_DISABLE_2300_SUPPORT) || !defined(ISP_COMPILE_2300_FW)
#define	ISP_2300_RISC_CODE	NULL
#else
#define	ISP_2300_RISC_CODE	(u_int16_t *) isp_2300_risc_code
#include <dev/microcode/isp/asm_2300.h>
#endif

#ifndef	ISP_DISABLE_1020_SUPPORT
static struct ispmdvec mdvec = {
	isp_pci_rd_isr,
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	NULL,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_1040_RISC_CODE,
	BIU_BURST_ENABLE|BIU_PCI_CONF1_FIFO_64
};
#endif

#ifndef	ISP_DISABLE_1080_SUPPORT
static struct ispmdvec mdvec_1080 = {
	isp_pci_rd_isr,
	isp_pci_rd_reg_1080,
	isp_pci_wr_reg_1080,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	NULL,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_1080_RISC_CODE,
	BIU_BURST_ENABLE|BIU_PCI_CONF1_FIFO_64
};
#endif

#ifndef	ISP_DISABLE_12160_SUPPORT
static struct ispmdvec mdvec_12160 = {
	isp_pci_rd_isr,
	isp_pci_rd_reg_1080,
	isp_pci_wr_reg_1080,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	NULL,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_12160_RISC_CODE,
	BIU_BURST_ENABLE|BIU_PCI_CONF1_FIFO_64
};
#endif

#ifndef	ISP_DISABLE_2100_SUPPORT
static struct ispmdvec mdvec_2100 = {
	isp_pci_rd_isr,
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	NULL,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_2100_RISC_CODE
};
#endif

#ifndef	ISP_DISABLE_2200_SUPPORT
static struct ispmdvec mdvec_2200 = {
	isp_pci_rd_isr,
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	NULL,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_2200_RISC_CODE
};
#endif

#ifndef	ISP_DISABLE_2300_SUPPORT
static struct ispmdvec mdvec_2300 = {
	isp_pci_rd_isr_2300,
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	NULL,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_2300_RISC_CODE
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

#ifndef	PCI_PRODUCT_QLOGIC_ISP1280
#define	PCI_PRODUCT_QLOGIC_ISP1280	0x1280
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP10160
#define	PCI_PRODUCT_QLOGIC_ISP10160	0x1016
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP12160
#define	PCI_PRODUCT_QLOGIC_ISP12160	0x1216
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2100
#define	PCI_PRODUCT_QLOGIC_ISP2100	0x2100
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2200
#define	PCI_PRODUCT_QLOGIC_ISP2200	0x2200
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2300
#define	PCI_PRODUCT_QLOGIC_ISP2300	0x2300
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2312
#define	PCI_PRODUCT_QLOGIC_ISP2312	0x2312
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP6312
#define	PCI_PRODUCT_QLOGIC_ISP6312	0x6312
#endif

#define	PCI_QLOGIC_ISP	((PCI_PRODUCT_QLOGIC_ISP1020 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP1080	\
	((PCI_PRODUCT_QLOGIC_ISP1080 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP1240	\
	((PCI_PRODUCT_QLOGIC_ISP1240 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP1280	\
	((PCI_PRODUCT_QLOGIC_ISP1280 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP10160	\
	((PCI_PRODUCT_QLOGIC_ISP10160 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP12160	\
	((PCI_PRODUCT_QLOGIC_ISP12160 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2100	\
	((PCI_PRODUCT_QLOGIC_ISP2100 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2200	\
	((PCI_PRODUCT_QLOGIC_ISP2200 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2300	\
	((PCI_PRODUCT_QLOGIC_ISP2300 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2312	\
	((PCI_PRODUCT_QLOGIC_ISP2312 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP6312	\
	((PCI_PRODUCT_QLOGIC_ISP6312 << 16) | PCI_VENDOR_QLOGIC)

/*
 * Odd case for some AMI raid cards... We need to *not* attach to this.
 */
#define	AMI_RAID_SUBVENDOR_ID	0x101e


#define IO_MAP_REG	0x10
#define MEM_MAP_REG	0x14
#define	PCIR_ROMADDR	0x30

#define	PCI_DFLT_LTNCY	0x40
#define	PCI_DFLT_LNSZ	0x10

#ifndef SCSI_ISP_PREFER_MEM_MAP
#ifdef __alpha__
#define SCSI_ISP_PREFER_MEM_MAP 1
#else
#define SCSI_ISP_PREFER_MEM_MAP 0
#endif
#endif

#ifndef	BUS_DMA_COHERENT
#define	BUS_DMA_COHERENT	BUS_DMAMEM_NOSYNC
#endif

static int isp_pci_probe (struct device *, void *, void *);
static void isp_pci_attach (struct device *, struct device *, void *);

struct isp_pcisoftc {
	struct ispsoftc		pci_isp;
	pci_chipset_tag_t	pci_pc;
	pcitag_t		pci_tag;
	bus_space_tag_t		pci_st;
	bus_space_handle_t	pci_sh;
	bus_dmamap_t		*pci_xfer_dmap;
	void *			pci_ih;
	int16_t			pci_poff[_NREG_BLKS];
};

struct cfattach isp_pci_ca = {
	sizeof (struct isp_pcisoftc), isp_pci_probe, isp_pci_attach
};

#ifdef  DEBUG
const char vstring[] =
    "Qlogic ISP Driver, NetBSD (pci) Platform Version %d.%d Core Version %d.%d";
#endif

const struct pci_matchid ispdev[] = {
#ifndef	ISP_DISABLE_1020_SUPPORT
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP1020 },
#endif
#ifndef	ISP_DISABLE_1080_SUPPORT
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP1080 },
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP1240 },
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP1280 },
#endif
#ifndef	ISP_DISABLE_12160_SUPPORT
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP10160 },
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP12160 },
#endif
#ifndef	ISP_DISABLE_2100_SUPPORT
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP2100 },
#endif
#ifndef	ISP_DISABLE_2200_SUPPORT
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP2200 },
#endif
#ifndef	ISP_DISABLE_2300_SUPPORT
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP2300 },
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP2312 },
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP6312 },
#endif
	{ 0, 0 }
};

static int
isp_pci_probe(struct device *parent, void *match, void *aux)
{
        struct pci_attach_args *pa = aux;

#ifndef	ISP_DISABLE_12160_SUPPORT
	/*
	 * Sigh. Check for subvendor id match here. Too bad we
	 * can't give an exclude mask in matchbyid.
	 */
        if (pa->pa_id == PCI_QLOGIC_ISP12160) {
		pcireg_t subvid =
		    pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBVEND_0);
		if (PCI_VENDOR(subvid) == AMI_RAID_SUBVENDOR_ID) {
			return (0);
                }
	}
#endif
	return (pci_matchbyid(pa, ispdev, sizeof(ispdev)/sizeof(ispdev[0])));
}


static void    
isp_pci_attach(struct device *parent, struct device *self, void *aux)
{
#ifdef	DEBUG
	static char oneshot = 1;
#endif
	static const char nomem[] = ": no mem for sdparam table\n";
	u_int32_t data, rev, linesz = PCI_DFLT_LNSZ;
	struct pci_attach_args *pa = aux;
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) self;
	struct ispsoftc *isp = &pcs->pci_isp;
	bus_space_tag_t st, iot, memt;
	bus_space_handle_t sh, ioh, memh;
	pci_intr_handle_t ih;
	const char *intrstr;
	int ioh_valid, memh_valid;
	bus_size_t iosize, msize;
	u_int32_t confopts = 0;

	ioh_valid = memh_valid = 0;

#if	SCSI_ISP_PREFER_MEM_MAP == 1
	if (pci_mapreg_map(pa, MEM_MAP_REG, PCI_MAPREG_TYPE_MEM, 0,
	    &memt, &memh, NULL, &msize, 0)) {
		printf(": can't map mem space\n");
	} else  {
		st = memt;
		sh = memh;
		memh_valid = 1;
	}
	if (memh_valid == 0) {
		if (pci_mapreg_map(pa, IO_MAP_REG, PCI_MAPREG_TYPE_IO, 0,
		    &iot, &ioh, NULL, &iosize, 0)) {
		} else {
			st = iot;
			sh = ioh;
			ioh_valid = 1;
		}
	}
#else
	if (pci_mapreg_map(pa, IO_MAP_REG, PCI_MAPREG_TYPE_IO, 0,
	    &iot, &ioh, NULL, &iosize, 0)) {
		printf(": can't map i/o space\n");
	} else {
		st = iot;
		sh = ioh;
		ioh_valid = 1;
	}
	if (ioh_valid == 0) {
		if (pci_mapreg_map(pa, MEM_MAP_REG, PCI_MAPREG_TYPE_MEM, 0,
		    &memt, &memh, NULL, &msize, 0)) {
			printf(": can't map mem space\n");
		} else  {
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
#if 0
	printf("\n");
#endif

	pcs->pci_st = st;
	pcs->pci_sh = sh;
	pcs->pci_pc = pa->pa_pc;
	pcs->pci_tag = pa->pa_tag;
	pcs->pci_poff[BIU_BLOCK >> _BLK_REG_SHFT] = BIU_REGS_OFF;
	pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = PCI_MBOX_REGS_OFF;
	pcs->pci_poff[SXP_BLOCK >> _BLK_REG_SHFT] = PCI_SXP_REGS_OFF;
	pcs->pci_poff[RISC_BLOCK >> _BLK_REG_SHFT] = PCI_RISC_REGS_OFF;
	pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] = DMA_REGS_OFF;
	rev = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CLASS_REG) & 0xff;
#ifndef	ISP_DISABLE_1020_SUPPORT
	if (pa->pa_id == PCI_QLOGIC_ISP) {
		isp->isp_mdvec = &mdvec;
		isp->isp_type = ISP_HA_SCSI_UNKNOWN;
		isp->isp_param = malloc(sizeof(sdparam), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (isp->isp_param == NULL) {
			printf(nomem);
			return;
		}
	}
#endif
#ifndef	ISP_DISABLE_1080_SUPPORT
	if (pa->pa_id == PCI_QLOGIC_ISP1080) {
		isp->isp_mdvec = &mdvec_1080;
		isp->isp_type = ISP_HA_SCSI_1080;
		isp->isp_param = malloc(sizeof(sdparam), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (isp->isp_param == NULL) {
			printf(nomem);
			return;
		}
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    ISP1080_DMA_REGS_OFF;
	}
	if (pa->pa_id == PCI_QLOGIC_ISP1240) {
		isp->isp_mdvec = &mdvec_1080;
		isp->isp_type = ISP_HA_SCSI_1240;
		isp->isp_param = malloc(2 * sizeof(sdparam), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (isp->isp_param == NULL) {
			printf(nomem);
			return;
		}
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    ISP1080_DMA_REGS_OFF;
	}
	if (pa->pa_id == PCI_QLOGIC_ISP1280) {
		isp->isp_mdvec = &mdvec_1080;
		isp->isp_type = ISP_HA_SCSI_1280;
		isp->isp_param = malloc(2 * sizeof(sdparam),
		    M_DEVBUF, M_NOWAIT | M_ZERO);
		if (isp->isp_param == NULL) {
			printf(nomem);
			return;
		}
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    ISP1080_DMA_REGS_OFF;
	}
#endif
#ifndef	ISP_DISABLE_12160_SUPPORT
	if (pa->pa_id == PCI_QLOGIC_ISP10160) {
		isp->isp_mdvec = &mdvec_12160;
		isp->isp_type = ISP_HA_SCSI_10160;
		isp->isp_param = malloc(sizeof(sdparam), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (isp->isp_param == NULL) {
			printf(nomem);
			return;
		}
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    ISP1080_DMA_REGS_OFF;
	}
	if (pa->pa_id == PCI_QLOGIC_ISP12160) {
		isp->isp_mdvec = &mdvec_12160;
		isp->isp_type = ISP_HA_SCSI_12160;
		isp->isp_param = malloc(2 * sizeof(sdparam),
		    M_DEVBUF, M_NOWAIT | M_ZERO);
		if (isp->isp_param == NULL) {
			printf(nomem);
			return;
		}
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    ISP1080_DMA_REGS_OFF;
	}
#endif
#ifndef	ISP_DISABLE_2100_SUPPORT
	if (pa->pa_id == PCI_QLOGIC_ISP2100) {
		isp->isp_mdvec = &mdvec_2100;
		isp->isp_type = ISP_HA_FC_2100;
		isp->isp_param = malloc(sizeof(fcparam), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (isp->isp_param == NULL) {
			printf(nomem);
			return;
		}
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2100_OFF;
		if (rev < 3) {
			/*
			 * XXX: Need to get the actual revision
			 * XXX: number of the 2100 FB. At any rate,
			 * XXX: lower cache line size for early revision
			 * XXX; boards.
			 */
			linesz = 1;
		}
	}
#endif
#ifndef	ISP_DISABLE_2200_SUPPORT
	if (pa->pa_id == PCI_QLOGIC_ISP2200) {
		isp->isp_mdvec = &mdvec_2200;
		isp->isp_type = ISP_HA_FC_2200;
		isp->isp_param = malloc(sizeof(fcparam), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (isp->isp_param == NULL) {
			printf(nomem);
			return;
		}
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2100_OFF;
		data = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CLASS_REG);
#ifdef __sparc64__
		{
			char name[32];

			bzero(name, sizeof(name));
			OF_getprop(PCITAG_NODE(pa->pa_tag),
			    "name", name, sizeof(name));
			if (strcmp(name, "SUNW,qlc") == 0)
				confopts |= ISP_CFG_NONVRAM;
		}
#endif
	}
#endif
#ifndef	ISP_DISABLE_2300_SUPPORT
	if (pa->pa_id == PCI_QLOGIC_ISP2300 ||
	    pa->pa_id == PCI_QLOGIC_ISP2312 ||
	    pa->pa_id == PCI_QLOGIC_ISP6312) {
		isp->isp_mdvec = &mdvec_2300;
		if (pa->pa_id  == PCI_QLOGIC_ISP2300) {
			isp->isp_type = ISP_HA_FC_2300;
		} else {
			isp->isp_type = ISP_HA_FC_2312;
			isp->isp_port = pa->pa_function;
		}
		isp->isp_param = malloc(sizeof(fcparam), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (isp->isp_param == NULL) {
			printf(nomem);
			return;
		}
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2300_OFF;
		data = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CLASS_REG);
	}
#endif
	/*
	 * Set up logging levels.
	 */
#ifdef	ISP_LOGDEFAULT
	isp->isp_dblev = ISP_LOGDEFAULT;
#else
	isp->isp_dblev = ISP_LOGWARN|ISP_LOGERR;
#ifdef	SCSIDEBUG
	isp->isp_dblev |= ISP_LOGDEBUG1|ISP_LOGDEBUG2;
#endif
#ifdef	DEBUG
	isp->isp_dblev |= ISP_LOGDEBUG0|ISP_LOGCONFIG|ISP_LOGINFO;
#endif
#endif

#ifdef	DEBUG
	if (oneshot) {
		oneshot = 0;
		isp_prt(isp, ISP_LOGCONFIG, vstring,
		    ISP_PLATFORM_VERSION_MAJOR, ISP_PLATFORM_VERSION_MINOR,
		    ISP_CORE_VERSION_MAJOR, ISP_CORE_VERSION_MINOR);
	}
#endif

	isp->isp_dmatag = pa->pa_dmat;
	isp->isp_revision = rev;

	/*
	 * Make sure that command register set sanely.
	 */
	data = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	if (IS_2300(isp)) {	/* per QLogic errata */
		data &= ~PCI_COMMAND_PARITY_ENABLE;
	}
	if (IS_23XX(isp)) {
		isp->isp_touched = 1;
	}
	data |= PCI_COMMAND_INVALIDATE_ENABLE;

	/*
	 * Not so sure about these- but I think it's important that they get
	 * enabled......
	 */
	data |= PCI_COMMAND_PARITY_ENABLE | PCI_COMMAND_SERR_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, data);

	/*
	 * Make sure that the latency timer, cache line size,
	 * and ROM is disabled.
	 */
	data = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG);
	data &= ~(PCI_LATTIMER_MASK << PCI_LATTIMER_SHIFT);
	data &= ~(PCI_CACHELINE_MASK << PCI_CACHELINE_SHIFT);
	data |= (0x40 << PCI_LATTIMER_SHIFT);
	data |= (0x10 << PCI_CACHELINE_SHIFT);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG, data);

	data = pci_conf_read(pa->pa_pc, pa->pa_tag, PCIR_ROMADDR);
	data &= ~1;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCIR_ROMADDR, data);

	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		free(isp->isp_param, M_DEVBUF);
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	if (intrstr == NULL)
		intrstr = "<I dunno>";
	pcs->pci_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, isp_pci_intr,
	    isp, isp->isp_name);
	if (pcs->pci_ih == NULL) {
		printf(": couldn't establish interrupt at %s\n",
			intrstr);
		free(isp->isp_param, M_DEVBUF);
		return;
	}

	printf(": %s\n", intrstr);

	if (IS_FC(isp)) {
		DEFAULT_NODEWWN(isp) = 0x400000007F000003ULL;
		DEFAULT_PORTWWN(isp) = 0x400000007F000003ULL;
	}

	isp->isp_confopts = confopts | self->dv_cfdata->cf_flags;
	isp->isp_role = ISP_DEFAULT_ROLES;
	ISP_LOCK(isp);
	isp->isp_osinfo.no_mbox_ints = 1;
	isp_reset(isp);
	if (isp->isp_state != ISP_RESETSTATE) {
		ISP_UNLOCK(isp);
		free(isp->isp_param, M_DEVBUF);
		return;
	}
	ENABLE_INTS(isp);
	isp_init(isp);
	if (isp->isp_state != ISP_INITSTATE) {
		isp_uninit(isp);
		ISP_UNLOCK(isp);
		free(isp->isp_param, M_DEVBUF);
		return;
	}
	/*
	 * Do Generic attach now.
	 */
	isp_attach(isp);
	if (isp->isp_state != ISP_RUNSTATE) {
		isp_uninit(isp);
		ISP_UNLOCK(isp);
		free(isp->isp_param, M_DEVBUF);
	} else {
		ISP_UNLOCK(isp);
	}
}

#define	IspVirt2Off(a, x)	\
	(((struct isp_pcisoftc *)a)->pci_poff[((x) & _BLK_REG_MASK) >> \
	_BLK_REG_SHFT] + ((x) & 0xff))

#define	BXR2(pcs, off)		\
	bus_space_read_2(pcs->pci_st, pcs->pci_sh, off)
#define	BXW2(pcs, off, v)	\
	bus_space_write_2(pcs->pci_st, pcs->pci_sh, off, v)


static INLINE int
isp_pci_rd_debounced(struct ispsoftc *isp, int off, u_int16_t *rp)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	u_int16_t val0, val1;
	int i = 0;

	do {
		val0 = BXR2(pcs, IspVirt2Off(isp, off));
		val1 = BXR2(pcs, IspVirt2Off(isp, off));
	} while (val0 != val1 && ++i < 1000);
	if (val0 != val1) {
		return (1);
	}
	*rp = val0;
	return (0);
}

static int
isp_pci_rd_isr(struct ispsoftc *isp, u_int16_t *isrp,
    u_int16_t *semap, u_int16_t *mbp)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	u_int16_t isr, sema;

	if (IS_2100(isp)) {
		if (isp_pci_rd_debounced(isp, BIU_ISR, &isr)) {
		    return (0);
		}
		if (isp_pci_rd_debounced(isp, BIU_SEMA, &sema)) {
		    return (0);
		}
	} else {
		isr = BXR2(pcs, IspVirt2Off(isp, BIU_ISR));
		sema = BXR2(pcs, IspVirt2Off(isp, BIU_SEMA));
	}
	isp_prt(isp, ISP_LOGDEBUG3, "ISR 0x%x SEMA 0x%x", isr, sema);
	isr &= INT_PENDING_MASK(isp);
	sema &= BIU_SEMA_LOCK;
	if (isr == 0 && sema == 0) {
		return (0);
	}
	*isrp = isr;
	if ((*semap = sema) != 0) {
		if (IS_2100(isp)) {
			if (isp_pci_rd_debounced(isp, OUTMAILBOX0, mbp)) {
				return (0);
			}
		} else {
			*mbp = BXR2(pcs, IspVirt2Off(isp, OUTMAILBOX0));
		}
	}
	return (1);
}

#ifndef	ISP_DISABLE_2300_SUPPORT
static int
isp_pci_rd_isr_2300(struct ispsoftc *isp, u_int16_t *isrp,
    u_int16_t *semap, u_int16_t *mbox0p)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	u_int32_t r2hisr;

	if (!(BXR2(pcs, IspVirt2Off(isp, BIU_ISR)) & BIU2100_ISR_RISC_INT)) {
		*isrp = 0;
		return (0);
	}
	r2hisr = bus_space_read_4(pcs->pci_st, pcs->pci_sh,
	    IspVirt2Off(pcs, BIU_R2HSTSLO));
	isp_prt(isp, ISP_LOGDEBUG3, "RISC2HOST ISR 0x%x", r2hisr);
	if ((r2hisr & BIU_R2HST_INTR) == 0) {
		*isrp = 0;
		return (0);
	}
	switch (r2hisr & BIU_R2HST_ISTAT_MASK) {
	case ISPR2HST_ROM_MBX_OK:
	case ISPR2HST_ROM_MBX_FAIL:
	case ISPR2HST_MBX_OK:
	case ISPR2HST_MBX_FAIL:
	case ISPR2HST_ASYNC_EVENT:
		*isrp = r2hisr & 0xffff;
		*mbox0p = (r2hisr >> 16);
		*semap = 1;
		return (1);
	case ISPR2HST_RIO_16:
		*isrp = r2hisr & 0xffff;
		*mbox0p = ASYNC_RIO1;
		*semap = 1;
		return (1);
	case ISPR2HST_FPOST:
		*isrp = r2hisr & 0xffff;
		*mbox0p = ASYNC_CMD_CMPLT;
		*semap = 1;
		return (1);
	case ISPR2HST_FPOST_CTIO:
		*isrp = r2hisr & 0xffff;
		*mbox0p = ASYNC_CTIO_DONE;
		*semap = 1;
		return (1);
	case ISPR2HST_RSPQ_UPDATE:
		*isrp = r2hisr & 0xffff;
		*mbox0p = 0;
		*semap = 0;
		return (1);
	default:
		return (0);
	}
}
#endif

static u_int16_t
isp_pci_rd_reg(struct ispsoftc *isp, int regoff)
{
	u_int16_t rv;
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	int oldconf = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oldconf = BXR2(pcs, IspVirt2Off(isp, BIU_CONF1));
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1),
		    oldconf | BIU_PCI_CONF1_SXP);
	}
	rv = BXR2(pcs, IspVirt2Off(isp, regoff));
	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1), oldconf);
	}
	return (rv);
}

static void
isp_pci_wr_reg(struct ispsoftc *isp, int regoff, u_int16_t val)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	int oldconf = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oldconf = BXR2(pcs, IspVirt2Off(isp, BIU_CONF1));
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1),
		    oldconf | BIU_PCI_CONF1_SXP);
	}
	BXW2(pcs, IspVirt2Off(isp, regoff), val);
	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1), oldconf);
	}
}

#if !(defined(ISP_DISABLE_1080_SUPPORT) && defined(ISP_DISABLE_12160_SUPPORT))
static u_int16_t
isp_pci_rd_reg_1080(struct ispsoftc *isp, int regoff)
{
	u_int16_t rv, oc = 0;
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK ||
	    (regoff & _BLK_REG_MASK) == (SXP_BLOCK|SXP_BANK1_SELECT)) {
		u_int16_t tc;
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oc = BXR2(pcs, IspVirt2Off(isp, BIU_CONF1));
		tc = oc & ~BIU_PCI1080_CONF1_DMA;
		if (regoff & SXP_BANK1_SELECT)
			tc |= BIU_PCI1080_CONF1_SXP1;
		else
			tc |= BIU_PCI1080_CONF1_SXP0;
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1), tc);
	} else if ((regoff & _BLK_REG_MASK) == DMA_BLOCK) {
		oc = BXR2(pcs, IspVirt2Off(isp, BIU_CONF1));
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1), 
		    oc | BIU_PCI1080_CONF1_DMA);
	}
	rv = BXR2(pcs, IspVirt2Off(isp, regoff));
	if (oc) {
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1), oc);
	}
	return (rv);
}

static void
isp_pci_wr_reg_1080(struct ispsoftc *isp, int regoff, u_int16_t val)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	int oc = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK ||
	    (regoff & _BLK_REG_MASK) == (SXP_BLOCK|SXP_BANK1_SELECT)) {
		u_int16_t tc;
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oc = BXR2(pcs, IspVirt2Off(isp, BIU_CONF1));
		tc = oc & ~BIU_PCI1080_CONF1_DMA;
		if (regoff & SXP_BANK1_SELECT)
			tc |= BIU_PCI1080_CONF1_SXP1;
		else
			tc |= BIU_PCI1080_CONF1_SXP0;
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1), tc);
	} else if ((regoff & _BLK_REG_MASK) == DMA_BLOCK) {
		oc = BXR2(pcs, IspVirt2Off(isp, BIU_CONF1));
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1), 
		    oc | BIU_PCI1080_CONF1_DMA);
	}
	BXW2(pcs, IspVirt2Off(isp, regoff), val);
	if (oc) {
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1), oc);
	}
}
#endif

static int
isp_pci_mbxdma(struct ispsoftc *isp)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)isp;
	bus_dma_tag_t dmat = isp->isp_dmatag;
	bus_dma_segment_t sg;
	bus_size_t len;
	fcparam *fcp;
	int rs, i;

	if (isp->isp_rquest_dma)	/* been here before? */
		return (0);

	len = isp->isp_maxcmds * sizeof (XS_T *);
	isp->isp_xflist = malloc(len, M_DEVBUF, M_WAITOK | M_ZERO);
	if (isp->isp_xflist == NULL) {
		isp_prt(isp, ISP_LOGERR, "cannot malloc xflist array");
		return (1);
	}
	len = isp->isp_maxcmds * sizeof (bus_dmamap_t);
	pcs->pci_xfer_dmap = (bus_dmamap_t *) malloc(len, M_DEVBUF, M_WAITOK);
	if (pcs->pci_xfer_dmap == NULL) {
		free(isp->isp_xflist, M_DEVBUF);
		isp->isp_xflist = NULL;
		isp_prt(isp, ISP_LOGERR, "cannot malloc dma map array");
		return (1);
	}

	for (i = 0; i < isp->isp_maxcmds; i++) {
		if (bus_dmamap_create(dmat, MAXPHYS, (MAXPHYS / NBPG) + 1,
		    MAXPHYS, 0, BUS_DMA_NOWAIT, &pcs->pci_xfer_dmap[i])) {
			isp_prt(isp, ISP_LOGERR, "cannot create dma maps");
			break;
		}
	}

	if (i < isp->isp_maxcmds) {
		while (--i >= 0) {
			bus_dmamap_destroy(dmat, pcs->pci_xfer_dmap[i]);
		}
		free(isp->isp_xflist, M_DEVBUF);
		free(pcs->pci_xfer_dmap, M_DEVBUF);
		isp->isp_xflist = NULL;
		pcs->pci_xfer_dmap = NULL;
		return (1);
	}

	/*
	 * Allocate and map the request queue.
	 */
	len = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
	if (bus_dmamem_alloc(dmat, len, PAGE_SIZE, 0, &sg, 1, &rs,
			     BUS_DMA_NOWAIT) ||
	    bus_dmamem_map(isp->isp_dmatag, &sg, rs, len,
	    (caddr_t *)&isp->isp_rquest, BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) {
		goto dmafail;
	}

	if (bus_dmamap_create(dmat, len, 1, len, 0, BUS_DMA_NOWAIT,
	    &isp->isp_rqdmap) || bus_dmamap_load(dmat, isp->isp_rqdmap,
	    (caddr_t)isp->isp_rquest, len, NULL,
	    BUS_DMA_NOWAIT)) {
		goto dmafail;
	}
	isp->isp_rquest_dma = isp->isp_rqdmap->dm_segs[0].ds_addr;

	/*
	 * Allocate and map the result queue.
	 */
	len = ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp));
	if (bus_dmamem_alloc(dmat, len, PAGE_SIZE, 0, &sg, 1, &rs,
			     BUS_DMA_NOWAIT) ||
	    bus_dmamem_map(dmat, &sg, rs, len, (caddr_t *)&isp->isp_result,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) {
		goto dmafail;
	}
	if (bus_dmamap_create(dmat, len, 1, len, 0, BUS_DMA_NOWAIT,
	    &isp->isp_rsdmap) || bus_dmamap_load(isp->isp_dmatag,
	    isp->isp_rsdmap, (caddr_t)isp->isp_result, len, NULL,
	    BUS_DMA_NOWAIT)) {
		goto dmafail;
	}
	isp->isp_result_dma = isp->isp_rsdmap->dm_segs[0].ds_addr;

	if (IS_SCSI(isp)) {
		return (0);
	}

	fcp = isp->isp_param;
	len = ISP2100_SCRLEN;
	if (bus_dmamem_alloc(dmat, len, PAGE_SIZE, 0, &sg, 1, &rs,
			     BUS_DMA_NOWAIT) ||
	    bus_dmamem_map(dmat, &sg, rs, len, (caddr_t *)&fcp->isp_scratch,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) {
		goto dmafail;
	}
	if (bus_dmamap_create(dmat, len, 1, len, 0, BUS_DMA_NOWAIT,
	    &isp->isp_scdmap) || bus_dmamap_load(dmat,
	    isp->isp_scdmap, (caddr_t)fcp->isp_scratch, len, NULL,
	    BUS_DMA_NOWAIT)) {
		goto dmafail;
	}
	fcp->isp_scdma = isp->isp_scdmap->dm_segs[0].ds_addr;
	return (0);
dmafail:
	isp_prt(isp, ISP_LOGERR, "mailbox dma setup failure");
	for (i = 0; i < isp->isp_maxcmds; i++) {
		bus_dmamap_destroy(dmat, pcs->pci_xfer_dmap[i]);
	}
	free(isp->isp_xflist, M_DEVBUF);
	free(pcs->pci_xfer_dmap, M_DEVBUF);
	isp->isp_xflist = NULL;
	pcs->pci_xfer_dmap = NULL;
	return (1);
}

static int
isp_pci_dmasetup(struct ispsoftc *isp, XS_T *xs, ispreq_t *rq,
    u_int16_t *nxtip, u_int16_t optr)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)isp;
	bus_dmamap_t dmap;
	u_int16_t starti = isp->isp_reqidx, nxti = *nxtip;
	ispreq_t *qep;
	int segcnt, seg, error, ovseg, seglim, drq;

	qep = (ispreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, starti);
	dmap = pcs->pci_xfer_dmap[isp_handle_index(rq->req_handle)];
	if (xs->datalen == 0) {
		rq->req_seg_count = 1;
		goto mbxsync;
	}

	if (xs->flags & SCSI_DATA_IN) {
		drq = REQFLAG_DATA_IN;
	} else {
		drq = REQFLAG_DATA_OUT;
	}

	if (IS_FC(isp)) {
		seglim = ISP_RQDSEG_T2;
		((ispreqt2_t *)rq)->req_totalcnt = xs->datalen;
		((ispreqt2_t *)rq)->req_flags |= drq;
	} else {
		rq->req_flags |= drq;
		if (XS_CDBLEN(xs) > 12)
			seglim = 0;
		else
			seglim = ISP_RQDSEG;
	}
	error = bus_dmamap_load(isp->isp_dmatag, dmap, xs->data, xs->datalen,
	    NULL, (xs->flags & SCSI_NOSLEEP) ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
	if (error) {
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_COMPLETE);
	}

	segcnt = dmap->dm_nsegs;

	isp_prt(isp, ISP_LOGDEBUG2, "%d byte %s %p in %d segs",
	    xs->datalen, (xs->flags & SCSI_DATA_IN)? "read to" :
	    "write from", xs->data, segcnt);

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
		isp_prt(isp, ISP_LOGDEBUG2, "seg0.[%d]={0x%lx,%lu}",
		    rq->req_seg_count, (long) dmap->dm_segs[seg].ds_addr,
		    (unsigned long) dmap->dm_segs[seg].ds_len);
	}

	if (seg == segcnt) {
		goto dmasync;
	}

	do {
		u_int16_t onxti;
		ispcontreq_t *crq, *cqe, local;

		crq = &local;

		cqe = (ispcontreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, nxti);
		onxti = nxti;
		nxti = ISP_NXT_QENTRY(onxti, RQUEST_QUEUE_LEN(isp));
		if (nxti == optr) {
			isp_prt(isp, ISP_LOGDEBUG0, "Request Queue Overflow++");
			bus_dmamap_unload(isp->isp_dmatag, dmap);
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_EAGAIN);
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
			isp_prt(isp, ISP_LOGDEBUG2, "seg%d.[%d]={0x%lx,%lu}",
			    rq->req_header.rqs_entry_count - 1,
			    rq->req_seg_count, (long)dmap->dm_segs[seg].ds_addr,
			    (unsigned long) dmap->dm_segs[seg].ds_len);
		}
		isp_put_cont_req(isp, crq, cqe);
		MEMORYBARRIER(isp, SYNC_REQUEST, onxti, QENTRY_LEN);
	} while (seg < segcnt);

dmasync:
	bus_dmamap_sync(isp->isp_dmatag, dmap, 0, dmap->dm_mapsize,
	    (xs->flags & SCSI_DATA_IN) ?  BUS_DMASYNC_PREREAD :
	    BUS_DMASYNC_PREWRITE);

mbxsync:
	switch (rq->req_header.rqs_entry_type) {
	case RQSTYPE_REQUEST:
		isp_put_request(isp, rq, qep);
		break;
	case RQSTYPE_CMDONLY:
		isp_put_extended_request(isp, (ispextreq_t *)rq,
		    (ispextreq_t *)qep);
		break;
	case RQSTYPE_T2RQS:
		isp_put_request_t2(isp, (ispreqt2_t *) rq, (ispreqt2_t *) qep);
		break;
	}
	*nxtip = nxti;
	return (CMD_QUEUED);
}

static int
isp_pci_intr(void *arg)
{
	u_int16_t isr, sema, mbox;
	struct ispsoftc *isp = (struct ispsoftc *)arg;

	isp->isp_intcnt++;
	if (ISP_READ_ISR(isp, &isr, &sema, &mbox) == 0) {
		isp->isp_intbogus++;
		return (0);
	} else {
		isp->isp_osinfo.onintstack = 1;
		isp_intr(isp, isr, sema, mbox);
		isp->isp_osinfo.onintstack = 0;
		return (1);
	}
}

static void
isp_pci_dmateardown(struct ispsoftc *isp, XS_T *xs, u_int16_t handle)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)isp;
	bus_dmamap_t dmap = pcs->pci_xfer_dmap[isp_handle_index(handle)];
	bus_dmamap_sync(isp->isp_dmatag, dmap, 0, dmap->dm_mapsize,
	    (xs->flags & SCSI_DATA_IN)?
	    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(isp->isp_dmatag, dmap);
}

static void
isp_pci_reset1(struct ispsoftc *isp)
{
	/* Make sure the BIOS is disabled */
	isp_pci_wr_reg(isp, HCCR, PCI_HCCR_CMD_BIOS);
	if (isp->isp_osinfo.no_mbox_ints == 0) {
		ENABLE_INTS(isp);
	}
}

static void
isp_pci_dumpregs(struct ispsoftc *isp, const char *msg)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)isp;
	if (msg)
                isp_prt(isp, ISP_LOGERR, "%s", msg);
	isp_prt(isp, ISP_LOGERR, "PCI Status Command/Status=%x\n",
	    pci_conf_read(pcs->pci_pc, pcs->pci_tag, PCI_COMMAND_STATUS_REG));
}
