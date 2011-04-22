/*	$OpenBSD: isp_sbus.c,v 1.18 2011/04/22 23:19:55 deraadt Exp $	*/
/* $NetBSD: isp_sbus.c,v 1.46 2001/09/26 20:53:14 eeh Exp $ */
/*
 * SBus specific probe and attach routines for QLogic ISP SCSI adapters.
 *
 * Copyright (c) 1997, 2001 by Matthew Jacob
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/autoconf.h>

#include <dev/ic/isp_openbsd.h>

#ifndef ISP_NOFIRMWARE
#define ISP_FIRMWARE_1000
#define ISP_FIRMWARE_2200
#endif

#if	defined(ISP_FIRMWARE_1000)
#include <dev/microcode/isp/asm_sbus.h>
#else
#define	ISP_1000_RISC_CODE	NULL
#endif

#if	defined(ISP_FIRMWARE_2200)
#define	ISP_2200_RISC_CODE	(u_int16_t *) isp_2200_risc_code
#include <dev/microcode/isp/asm_2200.h>
#else
#define	ISP_2200_RISC_CODE	NULL
#endif

#include <dev/sbus/sbusvar.h>

static int isp_sbus_intr(void *);
static int
isp_sbus_rd_isr(struct ispsoftc *, u_int32_t *, u_int16_t *, u_int16_t *);
static int
isp_sbus_rd_isr_2200(struct ispsoftc *, u_int32_t *, u_int16_t *, u_int16_t *);
static u_int32_t isp_sbus_rd_reg(struct ispsoftc *, int);
static void isp_sbus_wr_reg (struct ispsoftc *, int, u_int32_t);
static u_int32_t isp_sbus_rd_reg_2200(struct ispsoftc *, int);
static void isp_sbus_wr_reg_2200(struct ispsoftc *, int, u_int32_t);
static int isp_sbus_mbxdma(struct ispsoftc *);
static int isp_sbus_dmasetup(struct ispsoftc *, XS_T *, ispreq_t *, u_int32_t *,
    u_int32_t);
static void isp_sbus_dmateardown(struct ispsoftc *, XS_T *, u_int32_t);

static struct ispmdvec mdvec = {
	isp_sbus_rd_isr,
	isp_sbus_rd_reg,
	isp_sbus_wr_reg,
	isp_sbus_mbxdma,
	isp_sbus_dmasetup,
	isp_sbus_dmateardown,
	NULL,
	NULL,
	NULL,
	ISP_1000_RISC_CODE
};

static struct ispmdvec mdvec_2200 = {
	isp_sbus_rd_isr_2200,
	isp_sbus_rd_reg_2200,
	isp_sbus_wr_reg_2200,
	isp_sbus_mbxdma,
	isp_sbus_dmasetup,
	isp_sbus_dmateardown,
	NULL,
	NULL,
	NULL,
	ISP_2200_RISC_CODE
};

struct isp_sbussoftc {
	struct ispsoftc	sbus_isp;
	sdparam		sbus_dev;
	bus_space_tag_t	sbus_bustag;
	bus_space_handle_t sbus_reg;
	int		sbus_node;
	int		sbus_pri;
	struct ispmdvec	sbus_mdvec;
	bus_dmamap_t	*sbus_dmamap;
	int16_t		sbus_poff[_NREG_BLKS];
};


static int isp_match(struct device *, void *, void *);
static void isp_sbus_attach(struct device *, struct device *, void *);
struct cfattach isp_sbus_ca = {
	sizeof (struct isp_sbussoftc), isp_match, isp_sbus_attach
};

static int
isp_match(struct device *parent, void *vcf, void *aux)
{
	struct cfdata *cf = vcf;
	int rv;
#ifdef DEBUG
	static int oneshot = 1;
#endif
	struct sbus_attach_args *sa = aux;

	rv = (strcmp(cf->cf_driver->cd_name, sa->sa_name) == 0 ||
		strcmp("PTI,ptisp", sa->sa_name) == 0 ||
		strcmp("ptisp", sa->sa_name) == 0 ||
		strcmp("SUNW,isp", sa->sa_name) == 0 ||
		strcmp("SUNW,qlc", sa->sa_name) == 0 ||
		strcmp("QLGC,isp", sa->sa_name) == 0 ||
		strcmp("QLGC,qla", sa->sa_name) == 0);
#ifdef DEBUG
	if (rv && oneshot) {
		oneshot = 0;
		printf("QLogic ISP Driver, OpenBSD (sbus) Platform Version "
		    "%d.%d Core Version %d.%d\n",
		    ISP_PLATFORM_VERSION_MAJOR, ISP_PLATFORM_VERSION_MINOR,
		    ISP_CORE_VERSION_MAJOR, ISP_CORE_VERSION_MINOR);
	}
#endif
	return (rv);
}


static void
isp_sbus_attach(struct device *parent, struct device *self, void *aux)
{
	int freq, ispburst, sbusburst;
	struct sbus_attach_args *sa = aux;
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) self;
	struct ispsoftc *isp = &sbc->sbus_isp;

	printf(": %s\n", sa->sa_name);

	sbc->sbus_bustag = sa->sa_bustag;
	if (sa->sa_nintr != 0)
		sbc->sbus_pri = sa->sa_pri;
	sbc->sbus_mdvec = mdvec;

	if (sa->sa_npromvaddrs != 0) {
		if (bus_space_map(sa->sa_bustag, sa->sa_promvaddrs[0],
		    sa->sa_size,
		    BUS_SPACE_MAP_PROMADDRESS | BUS_SPACE_MAP_LINEAR,
		    &sbc->sbus_reg) == 0) {
			printf("%s: cannot map registers\n", self->dv_xname);
			return;
		}
	} else {
		if (sbus_bus_map(sa->sa_bustag, sa->sa_slot, sa->sa_offset,
				 sa->sa_size, BUS_SPACE_MAP_LINEAR, 0,
				 &sbc->sbus_reg) != 0) {
			printf("%s: cannot map registers\n", self->dv_xname);
			return;
		}
	}
	sbc->sbus_node = sa->sa_node;

	freq = getpropint(sa->sa_node, "clock-frequency", 0);
	if (freq) {
		/*
		 * Convert from HZ to MHz, rounding up.
		 */
		freq = (freq + 500000)/1000000;
#if	0
		printf("%s: %d MHz\n", self->dv_xname, freq);
#endif
	}
	sbc->sbus_mdvec.dv_clock = freq;

	DEFAULT_IID(isp) = getpropint(sa->sa_node, "scsi-initiator-id", 7);

	/*
	 * Now figure out what the proper burst sizes, etc., to use.
	 * Unfortunately, there is no ddi_dma_burstsizes here which
	 * walks up the tree finding the limiting burst size node (if
	 * any).
	 */
	sbusburst = ((struct sbus_softc *)parent)->sc_burst;
	if (sbusburst == 0)
		sbusburst = SBUS_BURST_32 - 1;
	ispburst = getpropint(sa->sa_node, "burst-sizes", -1);
	if (ispburst == -1) {
		ispburst = sbusburst;
	}
	ispburst &= sbusburst;
	ispburst &= ~(1 << 7);
	ispburst &= ~(1 << 6);
	sbc->sbus_mdvec.dv_conf1 =  0;
	if (ispburst & (1 << 5)) {
		sbc->sbus_mdvec.dv_conf1 = BIU_SBUS_CONF1_FIFO_32;
	} else if (ispburst & (1 << 4)) {
		sbc->sbus_mdvec.dv_conf1 = BIU_SBUS_CONF1_FIFO_16;
	} else if (ispburst & (1 << 3)) {
		sbc->sbus_mdvec.dv_conf1 =
		    BIU_SBUS_CONF1_BURST8 | BIU_SBUS_CONF1_FIFO_8;
	}
	if (sbc->sbus_mdvec.dv_conf1) {
		sbc->sbus_mdvec.dv_conf1 |= BIU_BURST_ENABLE;
	}

	/*
	 * Some early versions of the PTI SBus adapter
	 * would fail in trying to download (via poking)
	 * FW. We give up on them.
	 */
	if (strcmp("PTI,ptisp", sa->sa_name) == 0 ||
	    strcmp("ptisp", sa->sa_name) == 0) {
		sbc->sbus_mdvec.dv_ispfw = NULL;
	}

	isp->isp_mdvec = &sbc->sbus_mdvec;
	isp->isp_bustype = ISP_BT_SBUS;
	isp->isp_type = ISP_HA_SCSI_UNKNOWN;
	isp->isp_param = &sbc->sbus_dev;
	isp->isp_dmatag = sa->sa_dmatag;
	MEMZERO(isp->isp_param, sizeof (sdparam));

	sbc->sbus_poff[BIU_BLOCK >> _BLK_REG_SHFT] = BIU_REGS_OFF;
	sbc->sbus_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = SBUS_MBOX_REGS_OFF;
	sbc->sbus_poff[SXP_BLOCK >> _BLK_REG_SHFT] = SBUS_SXP_REGS_OFF;
	sbc->sbus_poff[RISC_BLOCK >> _BLK_REG_SHFT] = SBUS_RISC_REGS_OFF;
	sbc->sbus_poff[DMA_BLOCK >> _BLK_REG_SHFT] = DMA_REGS_OFF;

	if (strcmp("SUNW,qlc", sa->sa_name) == 0 ||
	    strcmp("QLGC,qla", sa->sa_name) == 0) {
		isp->isp_mdvec = &mdvec_2200;
		isp->isp_bustype = ISP_BT_PCI;
		isp->isp_type = ISP_HA_FC_2200;
		isp->isp_param = malloc(sizeof(fcparam), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (isp->isp_param == NULL) {
			printf("%s: no mem for sdparam table\n",
			    self->dv_xname);
			return;
		}
		sbc->sbus_poff[BIU_BLOCK >> _BLK_REG_SHFT] =
		    0x100 + BIU_REGS_OFF;
		sbc->sbus_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    0x100 + PCI_MBOX_REGS2100_OFF;
		sbc->sbus_poff[SXP_BLOCK >> _BLK_REG_SHFT] =
		    0x100 + PCI_SXP_REGS_OFF;
		sbc->sbus_poff[RISC_BLOCK >> _BLK_REG_SHFT] =
		    0x100 + PCI_RISC_REGS_OFF;
		sbc->sbus_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    0x100 + DMA_REGS_OFF;
	}

	/* Establish interrupt channel */
	bus_intr_establish(sbc->sbus_bustag, sbc->sbus_pri, IPL_BIO, 0,
	    isp_sbus_intr, sbc, self->dv_xname);

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

	isp->isp_confopts = self->dv_cfdata->cf_flags;
	isp->isp_role = ISP_DEFAULT_ROLES;

	/*
	 * There's no tool on sparc to set NVRAM for ISPs, so ignore
	 * it if we don't need to read WWNs from it.
	 */
	if (IS_SCSI(isp))
	    isp->isp_confopts |= ISP_CFG_NONVRAM;

	ISP_LOCK(isp);
	isp->isp_osinfo.no_mbox_ints = 1;
	isp_reset(isp);
	if (isp->isp_state != ISP_RESETSTATE) {
		ISP_UNLOCK(isp);
		return;
	}
	ISP_ENABLE_INTS(isp);
	isp_init(isp);
	if (isp->isp_state != ISP_INITSTATE) {
		isp_uninit(isp);
		ISP_UNLOCK(isp);
		return;
	}

	/*
	 * do generic attach.
	 */
	ISP_UNLOCK(isp);
	isp_attach(isp);
	if (isp->isp_state != ISP_RUNSTATE) {
		ISP_LOCK(isp);
		isp_uninit(isp);
		ISP_UNLOCK(isp);
	}
}

static int
isp_sbus_intr(void *arg)
{
	u_int32_t isr;
	u_int16_t sema, mbox;
	struct ispsoftc *isp = arg;

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

#define	IspVirt2Off(a, x)	\
	(((struct isp_sbussoftc *)a)->sbus_poff[((x) & _BLK_REG_MASK) >> \
	_BLK_REG_SHFT] + ((x) & 0xff))

#define	BXR2(sbc, off)		\
	bus_space_read_2(sbc->sbus_bustag, sbc->sbus_reg, off)

static int
isp_sbus_rd_isr(struct ispsoftc *isp, u_int32_t *isrp,
    u_int16_t *semap, u_int16_t *mbp)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	u_int16_t isr, sema;

	isr = BXR2(sbc, IspVirt2Off(isp, BIU_ISR));
	sema = BXR2(sbc, IspVirt2Off(isp, BIU_SEMA));
	isp_prt(isp, ISP_LOGDEBUG3, "ISR 0x%x SEMA 0x%x", isr, sema);
	isr &= INT_PENDING_MASK(isp);
	sema &= BIU_SEMA_LOCK;
	if (isr == 0 && sema == 0) {
		return (0);
	}
	*isrp = isr;
	if ((*semap = sema) != 0) {
		*mbp = BXR2(sbc, IspVirt2Off(isp, OUTMAILBOX0));
	}
	return (1);
}

static int
isp_sbus_rd_isr_2200(struct ispsoftc *isp, u_int32_t *isrp,
    u_int16_t *semap, u_int16_t *mbp)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	u_int16_t isr, sema;

	isr = letoh16(BXR2(sbc, IspVirt2Off(isp, BIU_ISR)));
	sema = letoh16(BXR2(sbc, IspVirt2Off(isp, BIU_SEMA)));
	isp_prt(isp, ISP_LOGDEBUG3, "ISR 0x%x SEMA 0x%x", isr, sema);
	isr &= INT_PENDING_MASK(isp);
	sema &= BIU_SEMA_LOCK;
	if (isr == 0 && sema == 0) {
		return (0);
	}
	*isrp = isr;
	if ((*semap = sema) != 0) {
		*mbp = letoh16(BXR2(sbc, IspVirt2Off(isp, OUTMAILBOX0)));
	}
	return (1);
}

static u_int32_t
isp_sbus_rd_reg(struct ispsoftc *isp, int regoff)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	int offset = sbc->sbus_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	return (bus_space_read_2(sbc->sbus_bustag, sbc->sbus_reg, offset));
}

static void
isp_sbus_wr_reg(struct ispsoftc *isp, int regoff, u_int32_t val)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	int offset = sbc->sbus_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	bus_space_write_2(sbc->sbus_bustag, sbc->sbus_reg, offset, val);
}

static u_int32_t
isp_sbus_rd_reg_2200(struct ispsoftc *isp, int regoff)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	int offset = sbc->sbus_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	return (letoh16(bus_space_read_2(sbc->sbus_bustag, sbc->sbus_reg, offset)));
}

static void
isp_sbus_wr_reg_2200(struct ispsoftc *isp, int regoff, u_int32_t val)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	int offset = sbc->sbus_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	bus_space_write_2(sbc->sbus_bustag, sbc->sbus_reg, offset, htole16(val));
}

static int
isp_sbus_mbxdma(struct ispsoftc *isp)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	bus_dma_segment_t seg;
	bus_addr_t addr;
	bus_size_t len;
	caddr_t base;
	size_t n;
	int rs, i, progress;

	if (isp->isp_rquest_dma)
		return (0);

	n = isp->isp_maxcmds * sizeof (XS_T *);
	isp->isp_xflist = (XS_T **) malloc(n, M_DEVBUF, M_WAITOK | M_ZERO);
	if (isp->isp_xflist == NULL) {
		isp_prt(isp, ISP_LOGERR, "cannot alloc xflist array");
		return (1);
	}

	n = sizeof (bus_dmamap_t) * isp->isp_maxcmds;
	sbc->sbus_dmamap = (bus_dmamap_t *) malloc(n, M_DEVBUF, M_WAITOK);
	if (sbc->sbus_dmamap == NULL) {
		free(isp->isp_xflist, M_DEVBUF);
		isp->isp_xflist = NULL;
		isp_prt(isp, ISP_LOGERR, "cannot alloc dmamap array");
		return (1);
	}
	for (i = 0; i < isp->isp_maxcmds; i++) {
		/* Allocate a DMA handle */
		if (bus_dmamap_create(isp->isp_dmatag, MAXPHYS, 1, MAXPHYS, 0,
		    BUS_DMA_NOWAIT, &sbc->sbus_dmamap[i]) != 0) {
			isp_prt(isp, ISP_LOGERR, "cmd DMA maps create error");
			break;
		}
	}
	if (i < isp->isp_maxcmds) {
		while (--i >= 0) {
			bus_dmamap_destroy(isp->isp_dmatag,
			    sbc->sbus_dmamap[i]);
		}
		free(isp->isp_xflist, M_DEVBUF);
		free(sbc->sbus_dmamap, M_DEVBUF);
		isp->isp_xflist = NULL;
		sbc->sbus_dmamap = NULL;
		return (1);
	}

	/*
	 * Allocate and map the request, result queues, plus FC scratch area.
	 */
	progress = 0;
	len = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
	len += ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp));
	if (IS_FC(isp)) {
		len += ISP2100_SCRLEN;
	}
	if (bus_dmamem_alloc(isp->isp_dmatag, len, 0, 0, &seg, 1, &rs,
	    BUS_DMA_NOWAIT)) {
		goto dmafail;
	}
	progress++;
	if (bus_dmamem_map(isp->isp_dmatag, &seg, rs, len,
	    &base, BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) {
		goto dmafail;
	}
	progress++;
	if (bus_dmamap_create(isp->isp_dmatag, len, 1, len, 0, BUS_DMA_NOWAIT,
	    &isp->isp_cdmap) != 0) {
		goto dmafail;
	}
	progress++;
	if (bus_dmamap_load(isp->isp_dmatag, isp->isp_cdmap,
	    base, len, NULL, BUS_DMA_NOWAIT) != 0) {
		goto dmafail;
	}
	progress++;
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

dmafail:
	isp_prt(isp, ISP_LOGERR, "Mailbox DMA Setup Failure");

	if (progress >= 4) {
		bus_dmamap_unload(isp->isp_dmatag, isp->isp_cdmap);
	}
	if (progress >= 3) {
		bus_dmamap_destroy(isp->isp_dmatag, isp->isp_cdmap);
	}
	if (progress >= 2) {
		bus_dmamem_unmap(isp->isp_dmatag, isp->isp_rquest, len);
	}
	if (progress >= 1) {
		bus_dmamem_free(isp->isp_dmatag, &seg, rs);
	}

	for (i = 0; i < isp->isp_maxcmds; i++) {
		bus_dmamap_destroy(isp->isp_dmatag, sbc->sbus_dmamap[i]);
	}
	free(sbc->sbus_dmamap, M_DEVBUF);
	free(isp->isp_xflist, M_DEVBUF);
	isp->isp_xflist = NULL;
	sbc->sbus_dmamap = NULL;
	return (1);
}

/*
 * Map a DMA request.
 * We're guaranteed that rq->req_handle is a value from 1 to isp->isp_maxcmds.
 */

static int
isp_sbus_dmasetup(struct ispsoftc *isp, XS_T *xs, ispreq_t *rq,
    u_int32_t *nxtip, u_int32_t optr)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	bus_dmamap_t dmap;
	u_int16_t nxti = *nxtip;
	ispreq_t *qep;
	int segcnt, seg, error, ovseg, seglim, drq;

	qep = (ispreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, isp->isp_reqidx);
	dmap = sbc->sbus_dmamap[isp_handle_index(rq->req_handle)];
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
	    NULL, (xs->flags & SCSI_NOSLEEP) ? BUS_DMA_NOWAIT :
	    BUS_DMA_WAITOK | BUS_DMA_STREAMING);
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
	case RQSTYPE_T7RQS:
		isp_put_request_t7(isp, (ispreqt7_t *) rq, (ispreqt7_t *) qep);
		break;
	}
	*nxtip = nxti;
	return (CMD_QUEUED);
}

static void
isp_sbus_dmateardown(struct ispsoftc *isp, XS_T *xs, u_int32_t handle)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *)isp;
	bus_dmamap_t dmap = sbc->sbus_dmamap[isp_handle_index(handle)];
	bus_dmamap_sync(isp->isp_dmatag, dmap, 0, dmap->dm_mapsize,
	    (xs->flags & SCSI_DATA_IN)?
	    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(isp->isp_dmatag, dmap);
}
