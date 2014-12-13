/*	$OpenBSD: isp_pci.c,v 1.62 2014/12/13 21:05:33 doug Exp $	*/
/* $FreeBSD: src/sys/dev/isp/isp_pci.c,v 1.148 2007/06/26 23:08:57 mjacob Exp $*/
/*-
 * Copyright (c) 1997-2006 by Matthew Jacob
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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
/*
 * PCI specific probe and attach routines for Qlogic ISP SCSI adapters.
 * FreeBSD Version.
 */

#include <dev/ic/isp_openbsd.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#ifdef __sparc64__
#include <dev/ofw/openfirm.h>
#endif
#ifdef __sgi__
#include <machine/autoconf.h>
#include <mips64/archtype.h>
#endif

#ifndef ISP_NOFIRMWARE
#define	ISP_FIRMWARE_1040
#define	ISP_FIRMWARE_1080
#define	ISP_FIRMWARE_12160
#define	ISP_FIRMWARE_2100
#define	ISP_FIRMWARE_2200
#define	ISP_FIRMWARE_2300
#endif

#if	defined(ISP_FIRMWARE_1040)
#define	ISP_1040_RISC_CODE	(u_int16_t *) isp_1040_risc_code
#include <dev/microcode/isp/asm_1040.h>
#else
#define	ISP_1040_RISC_CODE	NULL
#endif

#if	defined(ISP_FIRMWARE_1080)
#define	ISP_1080_RISC_CODE	(u_int16_t *) isp_1080_risc_code
#include <dev/microcode/isp/asm_1080.h>
#else
#define	ISP_1080_RISC_CODE	NULL
#endif

#if	defined(ISP_FIRMWARE_12160)
#define	ISP_12160_RISC_CODE	(u_int16_t *) isp_12160_risc_code
#include <dev/microcode/isp/asm_12160.h>
#else
#define	ISP_12160_RISC_CODE	NULL
#endif

#if	defined(ISP_FIRMWARE_2100)
extern const u_int16_t isp_2100_risc_code[];
#define	ISP_2100_RISC_CODE	(u_int16_t *) isp_2100_risc_code
#else
#define	ISP_2100_RISC_CODE	NULL
#endif

#if	defined(ISP_FIRMWARE_2200)
extern const u_int16_t isp_2200_risc_code[];
#define	ISP_2200_RISC_CODE	(u_int16_t *) isp_2200_risc_code
#else
#define	ISP_2200_RISC_CODE	NULL
#endif

#if	defined(ISP_FIRMWARE_2300)
extern const u_int16_t isp_2300_risc_code[];
#define	ISP_2300_RISC_CODE	(u_int16_t *) isp_2300_risc_code
#else
#define	ISP_2300_RISC_CODE	NULL
#endif

#if	defined(ISP_FIRMWARE_2400)
#define	ISP_2400_RISC_CODE	(u_int16_t *) isp_2400_risc_code
#include <dev/microcode/isp/asm_2400.h>
#else
#define	ISP_2400_RISC_CODE	NULL
#endif

uint32_t isp_pci_rd_reg(ispsoftc_t *, int);
void isp_pci_wr_reg(ispsoftc_t *, int, uint32_t);
uint32_t isp_pci_rd_reg_1080(ispsoftc_t *, int);
void isp_pci_wr_reg_1080(ispsoftc_t *, int, uint32_t);
uint32_t isp_pci_rd_reg_2400(ispsoftc_t *, int);
void isp_pci_wr_reg_2400(ispsoftc_t *, int, uint32_t);
int isp_pci_rd_isr(ispsoftc_t *, uint32_t *, uint16_t *, uint16_t *);
int isp_pci_rd_isr_2300(ispsoftc_t *, uint32_t *, uint16_t *, uint16_t *);
int isp_pci_rd_isr_2400(ispsoftc_t *, uint32_t *, uint16_t *, uint16_t *);
int isp_pci_mbxdma(ispsoftc_t *);
int isp_pci_dmasetup(ispsoftc_t *, XS_T *, ispreq_t *, uint32_t *, uint32_t);
void isp_pci_reset0(ispsoftc_t *);
void isp_pci_reset1(ispsoftc_t *);
void isp_pci_dumpregs(ispsoftc_t *, const char *);
 

int isp_pci_rd_debounced(struct ispsoftc *, int, u_int16_t *);
int isp_pci_intr (void *);
void isp_pci_dmateardown(struct ispsoftc *, XS_T *, u_int32_t);

static struct ispmdvec mdvec = {
	isp_pci_rd_isr,
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_1040_RISC_CODE,
	BIU_BURST_ENABLE|BIU_PCI_CONF1_FIFO_64
};

static struct ispmdvec mdvec_1080 = {
	isp_pci_rd_isr,
	isp_pci_rd_reg_1080,
	isp_pci_wr_reg_1080,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_1080_RISC_CODE,
	BIU_BURST_ENABLE|BIU_PCI_CONF1_FIFO_64
};

static struct ispmdvec mdvec_12160 = {
	isp_pci_rd_isr,
	isp_pci_rd_reg_1080,
	isp_pci_wr_reg_1080,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_12160_RISC_CODE,
	BIU_BURST_ENABLE|BIU_PCI_CONF1_FIFO_64
};

static struct ispmdvec mdvec_2100 = {
	isp_pci_rd_isr,
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_2100_RISC_CODE
};

static struct ispmdvec mdvec_2200 = {
	isp_pci_rd_isr,
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_2200_RISC_CODE
};

static struct ispmdvec mdvec_2300 = {
	isp_pci_rd_isr_2300,
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_pci_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_2300_RISC_CODE
};

#ifndef	PCIM_CMD_INVEN
#define	PCIM_CMD_INVEN			0x10
#endif
#ifndef	PCIM_CMD_BUSMASTEREN
#define	PCIM_CMD_BUSMASTEREN		0x0004
#endif
#ifndef	PCIM_CMD_PERRESPEN
#define	PCIM_CMD_PERRESPEN		0x0040
#endif
#ifndef	PCIM_CMD_SEREN
#define	PCIM_CMD_SEREN			0x0100
#endif
#ifndef	PCIM_CMD_INTX_DISABLE
#define	PCIM_CMD_INTX_DISABLE		0x0400
#endif

#ifndef	PCIR_COMMAND
#define	PCIR_COMMAND			0x04
#endif

#ifndef	PCIR_CACHELNSZ
#define	PCIR_CACHELNSZ			0x0c
#endif

#ifndef	PCIR_LATTIMER
#define	PCIR_LATTIMER			0x0d
#endif

#ifndef	PCIR_ROMADDR
#define	PCIR_ROMADDR			0x30
#endif

#ifndef	PCI_VENDOR_QLOGIC
#define	PCI_VENDOR_QLOGIC		0x1077
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP1020
#define	PCI_PRODUCT_QLOGIC_ISP1020	0x1020
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP1080
#define	PCI_PRODUCT_QLOGIC_ISP1080	0x1080
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP10160
#define	PCI_PRODUCT_QLOGIC_ISP10160	0x1016
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP12160
#define	PCI_PRODUCT_QLOGIC_ISP12160	0x1216
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP1240
#define	PCI_PRODUCT_QLOGIC_ISP1240	0x1240
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP1280
#define	PCI_PRODUCT_QLOGIC_ISP1280	0x1280
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

#ifndef	PCI_PRODUCT_QLOGIC_ISP2322
#define	PCI_PRODUCT_QLOGIC_ISP2322	0x2322
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP6312
#define	PCI_PRODUCT_QLOGIC_ISP6312	0x6312
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP6322
#define	PCI_PRODUCT_QLOGIC_ISP6322	0x6322
#endif


#define	PCI_QLOGIC_ISP1020	\
	((PCI_PRODUCT_QLOGIC_ISP1020 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP1080	\
	((PCI_PRODUCT_QLOGIC_ISP1080 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP10160	\
	((PCI_PRODUCT_QLOGIC_ISP10160 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP12160	\
	((PCI_PRODUCT_QLOGIC_ISP12160 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP1240	\
	((PCI_PRODUCT_QLOGIC_ISP1240 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP1280	\
	((PCI_PRODUCT_QLOGIC_ISP1280 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2100	\
	((PCI_PRODUCT_QLOGIC_ISP2100 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2200	\
	((PCI_PRODUCT_QLOGIC_ISP2200 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2300	\
	((PCI_PRODUCT_QLOGIC_ISP2300 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2312	\
	((PCI_PRODUCT_QLOGIC_ISP2312 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2322	\
	((PCI_PRODUCT_QLOGIC_ISP2322 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP6312	\
	((PCI_PRODUCT_QLOGIC_ISP6312 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP6322	\
	((PCI_PRODUCT_QLOGIC_ISP6322 << 16) | PCI_VENDOR_QLOGIC)

/*
 * Odd case for some AMI raid cards... We need to *not* attach to this.
 */
#define	AMI_RAID_SUBVENDOR_ID	0x101e

#define	IO_MAP_REG	0x10
#define	MEM_MAP_REG	0x14

#define	PCI_DFLT_LTNCY	0x40
#define	PCI_DFLT_LNSZ	0x10

#ifndef SCSI_ISP_PREFER_MEM_MAP
#ifdef __alpha__
#define SCSI_ISP_PREFER_MEM_MAP 1
#else
#define SCSI_ISP_PREFER_MEM_MAP 0
#endif
#endif

#ifdef  DEBUG
const char vstring[] =
    "QLogic ISP Driver, OpenBSD (pci) Platform Version %d.%d Core Version %d.%d";
#endif

const struct pci_matchid ispdev[] = {
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP1020 },
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP1080 },
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP1240 },
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP1280 },
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP10160 },
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP12160 },
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP2100 },
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP2200 },
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP2300 },
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP2312 },
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP2322 },
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP6312 },
	{ PCI_VENDOR_QLOGIC, PCI_PRODUCT_QLOGIC_ISP6322 },
	{ 0, 0 }
};
 
int isp_pci_probe (struct device *, void *, void *);
void isp_pci_attach (struct device *, struct device *, void *);

#define	ISP_PCD(isp)	((struct isp_pcisoftc *)isp)->pci_dev
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

int
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
	return (pci_matchbyid(pa, ispdev, nitems(ispdev)));
}


void    
isp_pci_attach(struct device *parent, struct device *self, void *aux)
{
#ifdef	DEBUG
	static char oneshot = 1;
#endif
	static const char nomem[] = ": no mem for sdparam table\n";
	u_int32_t data, rev, linesz = PCI_DFLT_LNSZ;
	struct pci_attach_args *pa = aux;
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)self;
	struct ispsoftc *isp = &pcs->pci_isp;
	bus_space_tag_t st, iot, memt;
	bus_space_handle_t sh, ioh, memh;
	pci_intr_handle_t ih;
	const char *intrstr;
	int ioh_valid, memh_valid;
	bus_size_t iosize, msize;
	u_int32_t confopts = 0;
#ifdef __sparc64__
	int node, iid;
	u_int64_t wwn;
#endif

	DEFAULT_IID(isp) = 7;
#ifdef __sparc64__
	/*
	 * Walk up the Open Firmware device tree until we find a
	 * "scsi-initiator-id" property.
	 */
	node = PCITAG_NODE(pa->pa_tag);
	while (node) {
		if (OF_getprop(node, "scsi-initiator-id",
		    &iid, sizeof(iid)) == sizeof(iid)) {
			DEFAULT_IID(isp) = iid;
			confopts |= ISP_CFG_OWNLOOPID;
			break;
		}

		node = OF_parent(node);
	}

	node = PCITAG_NODE(pa->pa_tag);
	if (OF_getprop(node, "node-wwn", &wwn, sizeof(wwn)) == sizeof(wwn)) {
		DEFAULT_NODEWWN(isp) = wwn;
		confopts |= ISP_CFG_OWNWWNN;
	}
	if (OF_getprop(node, "port-wwn", &wwn, sizeof(wwn)) == sizeof(wwn)) {
		DEFAULT_PORTWWN(isp) = wwn;
		confopts |= ISP_CFG_OWNWWPN;
	}
#endif
#ifdef __sgi__
	/*
	 * The on-board isp controllers found on Octane, Origin 200 and
	 * Origin 300 families use id #0.
	 * XXX We'll need to be able to tell apart onboard isp from
	 * XXX other isp...
	 */
	if (sys_config.system_type == SGI_OCTANE ||
	    sys_config.system_type == SGI_IP27 ||
	    sys_config.system_type == SGI_IP35)
		DEFAULT_IID(isp) = 0;
#endif

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

	pcs->pci_st = isp->isp_bus_tag = st;
	pcs->pci_sh = isp->isp_bus_handle = sh;
	pcs->pci_pc = pa->pa_pc;
	pcs->pci_tag = pa->pa_tag;
	pcs->pci_poff[BIU_BLOCK >> _BLK_REG_SHFT] = BIU_REGS_OFF;
	pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = PCI_MBOX_REGS_OFF;
	pcs->pci_poff[SXP_BLOCK >> _BLK_REG_SHFT] = PCI_SXP_REGS_OFF;
	pcs->pci_poff[RISC_BLOCK >> _BLK_REG_SHFT] = PCI_RISC_REGS_OFF;
	pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] = DMA_REGS_OFF;
	rev = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CLASS_REG) & 0xff;
	if (pa->pa_id == PCI_QLOGIC_ISP1020) {
		isp->isp_mdvec = &mdvec;
		isp->isp_type = ISP_HA_SCSI_UNKNOWN;
		isp->isp_param = malloc(sizeof(sdparam), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (isp->isp_param == NULL) {
			printf(nomem);
			return;
		}
	}
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
		isp->isp_param = mallocarray(2, sizeof(sdparam), M_DEVBUF,
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
		isp->isp_param = mallocarray(2, sizeof(sdparam),
		    M_DEVBUF, M_NOWAIT | M_ZERO);
		if (isp->isp_param == NULL) {
			printf(nomem);
			return;
		}
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    ISP1080_DMA_REGS_OFF;
	}
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
		isp->isp_param = mallocarray(2, sizeof(sdparam),
		    M_DEVBUF, M_NOWAIT | M_ZERO);
		if (isp->isp_param == NULL) {
			printf(nomem);
			return;
		}
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    ISP1080_DMA_REGS_OFF;
	}
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
	if (pa->pa_id == PCI_QLOGIC_ISP2300) {
		isp->isp_mdvec = &mdvec_2300;
		isp->isp_type = ISP_HA_FC_2300;
		isp->isp_param = malloc(sizeof(fcparam), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (isp->isp_param == NULL) {
			printf(nomem);
			return;
		}
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2300_OFF;
	}
	if (pa->pa_id == PCI_QLOGIC_ISP2312 ||
	    pa->pa_id == PCI_QLOGIC_ISP6312) {
		isp->isp_mdvec = &mdvec_2300;
		isp->isp_type = ISP_HA_FC_2312;
		isp->isp_port = pa->pa_function;
		isp->isp_param = malloc(sizeof(fcparam), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (isp->isp_param == NULL) {
			printf(nomem);
			return;
		}
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2300_OFF;
	}
	if (pa->pa_id == PCI_QLOGIC_ISP2322 ||
	    pa->pa_id == PCI_QLOGIC_ISP6322) {
		isp->isp_mdvec = &mdvec_2300;
		isp->isp_type = ISP_HA_FC_2322;
		isp->isp_port = pa->pa_function;
		isp->isp_param = malloc(sizeof(fcparam), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (isp->isp_param == NULL) {
			printf(nomem);
			return;
		}
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2300_OFF;
	}
	/*
	 * Set up logging levels.
	 */
#ifdef	ISP_LOGDEFAULT
	isp->isp_dblev = ISP_LOGDEFAULT;
#else
	isp->isp_dblev = ISP_LOGWARN|ISP_LOGERR;
#if 0
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
		free(isp->isp_param, M_DEVBUF, 0);
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
		free(isp->isp_param, M_DEVBUF, 0);
		return;
	}

	printf(": %s\n", intrstr);

	if (IS_FC(isp)) {
		if (DEFAULT_NODEWWN(isp) == 0)
			DEFAULT_NODEWWN(isp) = 0x400000007F000003ULL;
		if (DEFAULT_PORTWWN(isp) == 0)
			DEFAULT_PORTWWN(isp) = 0x400000007F000003ULL;
	}

	isp->isp_confopts = confopts | self->dv_cfdata->cf_flags;
	isp->isp_role = ISP_DEFAULT_ROLES;
	ISP_LOCK(isp);
	isp->isp_osinfo.no_mbox_ints = 1;
	isp_reset(isp);
	if (isp->isp_state != ISP_RESETSTATE) {
		ISP_UNLOCK(isp);
		free(isp->isp_param, M_DEVBUF, 0);
		return;
	}
	ISP_ENABLE_INTS(isp);
	isp_init(isp);
	if (isp->isp_state != ISP_INITSTATE) {
		isp_uninit(isp);
		ISP_UNLOCK(isp);
		free(isp->isp_param, M_DEVBUF, 0);
		return;
	}
	/*
	 * Do Generic attach now.
	 */
	isp_attach(isp);
	if (isp->isp_state != ISP_RUNSTATE) {
		isp_uninit(isp);
		ISP_UNLOCK(isp);
		free(isp->isp_param, M_DEVBUF, 0);
	} else {
		ISP_UNLOCK(isp);
	}
}

#define	IspVirt2Off(a, x)	\
	(((struct isp_pcisoftc *)a)->pci_poff[((x) & _BLK_REG_MASK) >> \
	_BLK_REG_SHFT] + ((x) & 0xfff))

#define	BXR2(pcs, off)		\
	bus_space_read_2(pcs->pci_st, pcs->pci_sh, off)
#define	BXW2(pcs, off, v)	\
	bus_space_write_2(pcs->pci_st, pcs->pci_sh, off, v)
#define	BXR4(pcs, off)		\
	bus_space_read_4(pcs->pci_st, pcs->pci_sh, off)
#define	BXW4(pcs, off, v)	\
	bus_space_write_4(pcs->pci_st, pcs->pci_sh, off, v)

int
isp_pci_rd_debounced(struct ispsoftc *isp, int off, u_int16_t *rp)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)isp;
	u_int32_t val0, val1;
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

int
isp_pci_rd_isr(struct ispsoftc *isp, u_int32_t *isrp,
    u_int16_t *semap, u_int16_t *mbp)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)isp;
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

int
isp_pci_rd_isr_2300(struct ispsoftc *isp, u_int32_t *isrp,
    u_int16_t *semap, u_int16_t *mbox0p)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)isp;
	u_int32_t hccr;
	u_int32_t r2hisr;

	if (!(BXR2(pcs, IspVirt2Off(isp, BIU_ISR)) & BIU2100_ISR_RISC_INT)) {
		*isrp = 0;
		return (0);
	}
	r2hisr = BXR4(pcs, IspVirt2Off(isp, BIU_R2HSTSLO));
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
		hccr = ISP_READ(isp, HCCR);
		if (hccr & HCCR_PAUSE) {
			ISP_WRITE(isp, HCCR, HCCR_RESET);
			isp_prt(isp, ISP_LOGERR,
			    "RISC paused at interrupt (%x->%x)", hccr,
			    ISP_READ(isp, HCCR));
			ISP_WRITE(isp, BIU_ICR, 0);
		} else {
			isp_prt(isp, ISP_LOGERR, "unknown interrupt 0x%x\n",
			    r2hisr);
		}
		return (0);
	}
}

u_int32_t
isp_pci_rd_reg(struct ispsoftc *isp, int regoff)
{
	u_int16_t rv;
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)isp;
	int oldconf = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oldconf = BXR2(pcs, IspVirt2Off(isp, BIU_CONF1));
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1),
		    oldconf | BIU_PCI_CONF1_SXP);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2);
	}
	rv = BXR2(pcs, IspVirt2Off(isp, regoff));
	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1), oldconf);
	}
	MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2);
	return (rv);
}

void
isp_pci_wr_reg(struct ispsoftc *isp, int regoff, u_int32_t val)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)isp;
	int oldconf = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oldconf = BXR2(pcs, IspVirt2Off(isp, BIU_CONF1));
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1),
		    oldconf | BIU_PCI_CONF1_SXP);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2);
	}
	BXW2(pcs, IspVirt2Off(isp, regoff), val);
	MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, regoff), 2);
	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1), oldconf);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2);
	}

}

u_int32_t
isp_pci_rd_reg_1080(struct ispsoftc *isp, int regoff)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)isp;
	u_int32_t rv, oc = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK ||
	    (regoff & _BLK_REG_MASK) == (SXP_BLOCK|SXP_BANK1_SELECT)) {
		u_int32_t tc;
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
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2);
	} else if ((regoff & _BLK_REG_MASK) == DMA_BLOCK) {
		oc = BXR2(pcs, IspVirt2Off(isp, BIU_CONF1));
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1), 
		    oc | BIU_PCI1080_CONF1_DMA);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2);
	}
	rv = BXR2(pcs, IspVirt2Off(isp, regoff));
	if (oc) {
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1), oc);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2);
	}
	return (rv);
}

void
isp_pci_wr_reg_1080(struct ispsoftc *isp, int regoff, u_int32_t val)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)isp;
	int oc = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK ||
	    (regoff & _BLK_REG_MASK) == (SXP_BLOCK|SXP_BANK1_SELECT)) {
		u_int32_t tc;
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
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2);
	} else if ((regoff & _BLK_REG_MASK) == DMA_BLOCK) {
		oc = BXR2(pcs, IspVirt2Off(isp, BIU_CONF1));
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1), 
		    oc | BIU_PCI1080_CONF1_DMA);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2);
	}
	BXW2(pcs, IspVirt2Off(isp, regoff), val);
	MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, regoff), 2);
	if (oc) {
		BXW2(pcs, IspVirt2Off(isp, BIU_CONF1), oc);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2);
	}
}

struct imush {
	ispsoftc_t *isp;
	int error;
};


int
isp_pci_mbxdma(struct ispsoftc *isp)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)isp;
	bus_dma_tag_t dmat = isp->isp_dmatag;
	bus_dma_segment_t sg;
	bus_addr_t addr;
	bus_size_t len;
	caddr_t base;
	int rs, i;

	if (isp->isp_rquest_dma)	/* been here before? */
		return (0);

	isp->isp_xflist = mallocarray(isp->isp_maxcmds, sizeof(XS_T *),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (isp->isp_xflist == NULL) {
		isp_prt(isp, ISP_LOGERR, "cannot malloc xflist array");
		return (1);
	}
	len = isp->isp_maxcmds * sizeof(XS_T *);

	pcs->pci_xfer_dmap = mallocarray(isp->isp_maxcmds, sizeof(bus_dmamap_t),
	    M_DEVBUF, M_NOWAIT);
	if (pcs->pci_xfer_dmap == NULL) {
		free(isp->isp_xflist, M_DEVBUF, 0);
		isp->isp_xflist = NULL;
		isp_prt(isp, ISP_LOGERR, "cannot malloc dma map array");
		return (1);
	}
	len = isp->isp_maxcmds * sizeof(bus_dmamap_t);

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
		free(isp->isp_xflist, M_DEVBUF, 0);
		free(pcs->pci_xfer_dmap, M_DEVBUF, 0);
		isp->isp_xflist = NULL;
		pcs->pci_xfer_dmap = NULL;
		return (1);
	}

	/*
	 * Allocate and map the request, result queues, plus FC scratch area.
	 */
	len = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
	len += ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp));
	if (IS_FC(isp)) {
		len += ISP2100_SCRLEN;
	}

	if (bus_dmamem_alloc(dmat, len, PAGE_SIZE, 0, &sg, 1, &rs,
	    BUS_DMA_NOWAIT))
		goto dmafail;

	if (bus_dmamem_map(isp->isp_dmatag, &sg, rs, len, &base,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT))
		goto dmafree;

	if (bus_dmamap_create(dmat, len, 1, len, 0, BUS_DMA_NOWAIT,
	    &isp->isp_cdmap))
		goto dmaunmap;

	if (bus_dmamap_load(dmat, isp->isp_cdmap, base, len, NULL,
	    BUS_DMA_NOWAIT))
		goto dmadestroy;

	addr = isp->isp_cdmap->dm_segs[0].ds_addr;
	isp->isp_rquest_dma = addr;
	addr += ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
	isp->isp_result_dma = addr;

	if (IS_FC(isp)) {
		addr += ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp));
		FCPARAM(isp)->isp_scdma = addr;

	}

	isp->isp_rquest = base;
	base += ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
	isp->isp_result = base;
	if (IS_FC(isp)) {
		base += ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp));
		FCPARAM(isp)->isp_scratch = base;
	}
	return (0);

dmadestroy:
	bus_dmamap_destroy(dmat, isp->isp_cdmap);
dmaunmap:
	bus_dmamem_unmap(dmat, base, len);
dmafree:
	bus_dmamem_free(dmat, &sg, rs);
dmafail:
	isp_prt(isp, ISP_LOGERR, "mailbox dma setup failure");
	for (i = 0; i < isp->isp_maxcmds; i++) {
		bus_dmamap_destroy(dmat, pcs->pci_xfer_dmap[i]);
	}
	free(isp->isp_xflist, M_DEVBUF, 0);
	free(pcs->pci_xfer_dmap, M_DEVBUF, 0);
	isp->isp_xflist = NULL;
	pcs->pci_xfer_dmap = NULL;
	return (1);
}

int
isp_pci_dmasetup(struct ispsoftc *isp, XS_T *xs, ispreq_t *rq,
    u_int32_t *nxtip, u_int32_t optr)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)isp;
	bus_dmamap_t dmap;
	u_int16_t nxti = *nxtip;
	ispreq_t *qep;
	int segcnt, seg, error, ovseg, seglim, drq;

	qep = (ispreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, isp->isp_reqidx);
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
		if (FCPARAM(isp)->isp_2klogin) {
			isp_put_request_t2e(isp,
			    (ispreqt2e_t *) rq, (ispreqt2e_t *) qep);
		} else {
			isp_put_request_t2(isp,
			    (ispreqt2_t *) rq, (ispreqt2_t *) qep);
		}
		break;
	case RQSTYPE_T3RQS:
		if (FCPARAM(isp)->isp_2klogin) {
			isp_put_request_t3e(isp,
			    (ispreqt3e_t *) rq, (ispreqt3e_t *) qep);
			break;
		}
		/* FALLTHROUGH */
	case RQSTYPE_A64:
		isp_put_request_t3(isp, (ispreqt3_t *) rq, (ispreqt3_t *) qep);
		break;
	}
	*nxtip = nxti;
	return (CMD_QUEUED);
}

int
isp_pci_intr(void *arg)
{
	u_int32_t isr;
	u_int16_t sema, mbox;
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

void
isp_pci_dmateardown(struct ispsoftc *isp, XS_T *xs, u_int32_t handle)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)isp;
	bus_dmamap_t dmap = pcs->pci_xfer_dmap[isp_handle_index(handle)];
	bus_dmamap_sync(isp->isp_dmatag, dmap, 0, dmap->dm_mapsize,
	    (xs->flags & SCSI_DATA_IN)?
	    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(isp->isp_dmatag, dmap);
}

void
isp_pci_reset0(ispsoftc_t *isp)
{
	ISP_DISABLE_INTS(isp);
}

void
isp_pci_reset1(struct ispsoftc *isp)
{
	/* Make sure the BIOS is disabled */
	isp_pci_wr_reg(isp, HCCR, PCI_HCCR_CMD_BIOS);
	/* and enable interrupts */
	ISP_ENABLE_INTS(isp);
}

void
isp_pci_dumpregs(struct ispsoftc *isp, const char *msg)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)isp;
	if (msg)
                isp_prt(isp, ISP_LOGERR, "%s", msg);
	if (IS_SCSI(isp))
		isp_prt(isp, ISP_LOGERR, "    biu_conf1=%x",
		    ISP_READ(isp, BIU_CONF1));
	else
		isp_prt(isp, ISP_LOGERR, "    biu_csr=%x",
		    ISP_READ(isp, BIU2100_CSR));
	isp_prt(isp, ISP_LOGERR, " biu_icr=%x biu_isr=%x biu_sema=%x ",
	    ISP_READ(isp, BIU_ICR), ISP_READ(isp, BIU_ISR),
	    ISP_READ(isp, BIU_SEMA));
	isp_prt(isp, ISP_LOGERR, "risc_hccr=%x\n", ISP_READ(isp, HCCR));
	isp_prt(isp, ISP_LOGERR, "PCI Status Command/Status=%x\n",
	    pci_conf_read(pcs->pci_pc, pcs->pci_tag, PCI_COMMAND_STATUS_REG));
}
