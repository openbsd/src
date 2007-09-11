/*	$OpenBSD: isp_sbus.c,v 1.10 2007/09/11 13:39:34 gilles Exp $	*/
/* $NetBSD: isp_sbus.c,v 1.46 2001/09/26 20:53:14 eeh Exp $ */

/*
 * This driver, which is contained in NetBSD in the files:
 *
 *	sys/dev/ic/isp.c
 *	sys/dev/ic/isp_inline.h
 *	sys/dev/ic/isp_netbsd.c
 *	sys/dev/ic/isp_netbsd.h
 *	sys/dev/ic/isp_target.c
 *	sys/dev/ic/isp_target.h
 *	sys/dev/ic/isp_tpublic.h
 *	sys/dev/ic/ispmbox.h
 *	sys/dev/ic/ispreg.h
 *	sys/dev/ic/ispvar.h
 *	sys/microcode/isp/asm_sbus.h
 *	sys/microcode/isp/asm_1040.h
 *	sys/microcode/isp/asm_1080.h
 *	sys/microcode/isp/asm_12160.h
 *	sys/microcode/isp/asm_2100.h
 *	sys/microcode/isp/asm_2200.h
 *	sys/pci/isp_pci.c
 *	sys/sbus/isp_sbus.c
 *
 * Is being actively maintained by Matthew Jacob (mjacob@netbsd.org).
 * This driver also is shared source with FreeBSD, OpenBSD, Linux, Solaris,
 * Linux versions. This tends to be an interesting maintenance problem.
 *
 * Please coordinate with Matthew Jacob on changes you wish to make here.
 */
/*
 * SBus specific probe and attach routines for Qlogic ISP SCSI adapters.
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
#if	defined(ISP_COMPILE_FW) || defined(ISP_COMPILE_1000_FW)
#include <dev/microcode/isp/asm_sbus.h>
#endif
#include <dev/sbus/sbusvar.h>
#include <sys/reboot.h>

static int isp_sbus_intr(void *);
static int
isp_sbus_rd_isr(struct ispsoftc *, u_int16_t *, u_int16_t *, u_int16_t *);
static u_int16_t isp_sbus_rd_reg(struct ispsoftc *, int);
static void isp_sbus_wr_reg (struct ispsoftc *, int, u_int16_t);
static int isp_sbus_mbxdma(struct ispsoftc *);
static int isp_sbus_dmasetup(struct ispsoftc *, XS_T *, ispreq_t *, u_int16_t *,
    u_int16_t);
static void isp_sbus_dmateardown(struct ispsoftc *, XS_T *, u_int16_t);

#ifndef	ISP_1000_RISC_CODE
#define	ISP_1000_RISC_CODE	NULL
#endif

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
	(u_int16_t *) ISP_1000_RISC_CODE
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
		strcmp("QLGC,isp", sa->sa_name) == 0);
#ifdef DEBUG
	if (rv && oneshot) {
		oneshot = 0;
		printf("Qlogic ISP Driver, NetBSD (sbus) Platform Version "
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
#ifdef	SCSIDEBUG
	isp->isp_dblev |= ISP_LOGDEBUG1|ISP_LOGDEBUG2;
#endif
#ifdef	DEBUG
	isp->isp_dblev |= ISP_LOGDEBUG0|ISP_LOGCONFIG|ISP_LOGINFO;
#endif
#endif

	isp->isp_confopts = self->dv_cfdata->cf_flags;
	isp->isp_role = ISP_DEFAULT_ROLES;

	/*
	 * There's no tool on sparc to set NVRAM for ISPs, so ignore it.
	 */
	isp->isp_confopts |= ISP_CFG_NONVRAM;
	ISP_LOCK(isp);
	isp->isp_osinfo.no_mbox_ints = 1;
	isp_reset(isp);
	if (isp->isp_state != ISP_RESETSTATE) {
		ISP_UNLOCK(isp);
		return;
	}
	ENABLE_INTS(isp);
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
	u_int16_t isr, sema, mbox;
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
isp_sbus_rd_isr(struct ispsoftc *isp, u_int16_t *isrp,
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

static u_int16_t
isp_sbus_rd_reg(struct ispsoftc *isp, int regoff)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	int offset = sbc->sbus_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	return (bus_space_read_2(sbc->sbus_bustag, sbc->sbus_reg, offset));
}

static void
isp_sbus_wr_reg(struct ispsoftc *isp, int regoff, u_int16_t val)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	int offset = sbc->sbus_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	bus_space_write_2(sbc->sbus_bustag, sbc->sbus_reg, offset, val);
}

static int
isp_sbus_mbxdma(struct ispsoftc *isp)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	bus_dma_segment_t reqseg, rspseg;
	int reqrs, rsprs, i, progress;
	size_t n;
	bus_size_t len;

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
	 * Allocate and map the request and response queues
	 */
	progress = 0;
	len = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
	if (bus_dmamem_alloc(isp->isp_dmatag, len, 0, 0, &reqseg, 1, &reqrs,
	    BUS_DMA_NOWAIT)) {
		goto dmafail;
	}
	progress++;
	if (bus_dmamem_map(isp->isp_dmatag, &reqseg, reqrs, len,
	    (caddr_t *)&isp->isp_rquest, BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) {
		goto dmafail;
	}
	progress++;
	if (bus_dmamap_create(isp->isp_dmatag, len, 1, len, 0, BUS_DMA_NOWAIT,
	    &isp->isp_rqdmap) != 0) {
		goto dmafail;
	}
	progress++;
	if (bus_dmamap_load(isp->isp_dmatag, isp->isp_rqdmap,
	    isp->isp_rquest, len, NULL, BUS_DMA_NOWAIT) != 0) {
		goto dmafail;
	}
	progress++;
	isp->isp_rquest_dma = isp->isp_rqdmap->dm_segs[0].ds_addr;

	len = ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp));
	if (bus_dmamem_alloc(isp->isp_dmatag, len, 0, 0, &rspseg, 1, &rsprs,
	    BUS_DMA_NOWAIT)) {
		goto dmafail;
	}
	progress++;
	if (bus_dmamem_map(isp->isp_dmatag, &rspseg, rsprs, len,
	    (caddr_t *)&isp->isp_result, BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) {
		goto dmafail;
	}
	progress++;
	if (bus_dmamap_create(isp->isp_dmatag, len, 1, len, 0, BUS_DMA_NOWAIT,
	    &isp->isp_rsdmap) != 0) {
		goto dmafail;
	}
	progress++;
	if (bus_dmamap_load(isp->isp_dmatag, isp->isp_rsdmap,
	    isp->isp_result, len, NULL, BUS_DMA_NOWAIT) != 0) {
		goto dmafail;
	}
	isp->isp_result_dma = isp->isp_rsdmap->dm_segs[0].ds_addr;

	return (0);

dmafail:
	isp_prt(isp, ISP_LOGERR, "Mailbox DMA Setup Failure");

	if (progress >= 8) {
		bus_dmamap_unload(isp->isp_dmatag, isp->isp_rsdmap);
	}
	if (progress >= 7) {
		bus_dmamap_destroy(isp->isp_dmatag, isp->isp_rsdmap);
	}
	if (progress >= 6) {
		bus_dmamem_unmap(isp->isp_dmatag,
		    isp->isp_result, ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp)));
	}
	if (progress >= 5) {
		bus_dmamem_free(isp->isp_dmatag, &rspseg, rsprs);
	}

	if (progress >= 4) {
		bus_dmamap_unload(isp->isp_dmatag, isp->isp_rqdmap);
	}
	if (progress >= 3) {
		bus_dmamap_destroy(isp->isp_dmatag, isp->isp_rqdmap);
	}
	if (progress >= 2) {
		bus_dmamem_unmap(isp->isp_dmatag,
		    isp->isp_rquest, ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp)));
	}
	if (progress >= 1) {
		bus_dmamem_free(isp->isp_dmatag, &reqseg, reqrs);
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
    u_int16_t *nxtip, u_int16_t optr)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	bus_dmamap_t dmap;
	ispreq_t *qep;
	int cansleep = (xs->flags & SCSI_NOSLEEP) == 0;
	int in = (xs->flags & SCSI_DATA_IN) != 0;

	qep = (ispreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, isp->isp_reqidx);
	if (xs->datalen == 0) {
		rq->req_seg_count = 1;
		goto mbxsync;
	}

	dmap = sbc->sbus_dmamap[isp_handle_index(rq->req_handle)];
	if (dmap->dm_nsegs != 0) {
		panic("%s: dma map already allocated", isp->isp_name);
		/* NOTREACHED */
	}
	if (bus_dmamap_load(isp->isp_dmatag, dmap, xs->data, xs->datalen,
	    NULL, (cansleep ? BUS_DMA_WAITOK : BUS_DMA_NOWAIT) |
	    BUS_DMA_STREAMING) != 0) {
		XS_SETERR(xs, HBA_BOTCH);
		return (CMD_COMPLETE);
	}

	bus_dmamap_sync(isp->isp_dmatag, dmap, 0, xs->datalen,
	    in? BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

	if (in) {
		rq->req_flags |= REQFLAG_DATA_IN;
	} else {
		rq->req_flags |= REQFLAG_DATA_OUT;
	}

	if (XS_CDBLEN(xs) > 12) {
		u_int16_t onxti;
		ispcontreq_t local, *crq = &local, *cqe;

		onxti = *nxtip;
		cqe = (ispcontreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, onxti);
		*nxtip = ISP_NXT_QENTRY(onxti, RQUEST_QUEUE_LEN(isp));
		if (*nxtip == optr) {
			isp_prt(isp, ISP_LOGDEBUG0, "Request Queue Overflow++");
			bus_dmamap_unload(isp->isp_dmatag, dmap);
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_EAGAIN);
		}
		rq->req_seg_count = 2;
		MEMZERO((void *)crq, sizeof (*crq));
		crq->req_header.rqs_entry_count = 1;
		crq->req_header.rqs_entry_type = RQSTYPE_DATASEG;  
		crq->req_dataseg[0].ds_count = xs->datalen;
		crq->req_dataseg[0].ds_base = dmap->dm_segs[0].ds_addr;
		isp_put_cont_req(isp, crq, cqe);
		MEMORYBARRIER(isp, SYNC_REQUEST, onxti, QENTRY_LEN);
	} else {
		rq->req_seg_count = 1;
		rq->req_dataseg[0].ds_count = xs->datalen;
		rq->req_dataseg[0].ds_base = dmap->dm_segs[0].ds_addr;
	}

mbxsync:
	if (XS_CDBLEN(xs) > 12) {
		isp_put_extended_request(isp,
		    (ispextreq_t *)rq, (ispextreq_t *) qep);
	} else {
		isp_put_request(isp, rq, qep);
	}
	return (CMD_QUEUED);
}

static void
isp_sbus_dmateardown(struct ispsoftc *isp, XS_T *xs, u_int16_t handle)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	bus_dmamap_t dmap;

	dmap = sbc->sbus_dmamap[isp_handle_index(handle)];

	if (dmap->dm_nsegs == 0) {
		panic("%s: dma map not already allocated", isp->isp_name);
		/* NOTREACHED */
	}
	bus_dmamap_sync(isp->isp_dmatag, dmap, 0,
	    xs->datalen, (xs->flags & SCSI_DATA_IN)?
	    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(isp->isp_dmatag, dmap);
}
