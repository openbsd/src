/*	$OpenBSD: isp_sbus.c,v 1.23 2002/10/12 01:09:43 krw Exp $	*/
/*
 * SBus specific probe and attach routines for Qlogic ISP SCSI adapters.
 *
 * Copyright (c) 1997, 2000 by Matthew Jacob
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 *    documentation and/or other materials provided with the distribution.
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
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/param.h>
#include <machine/vmparam.h>
#include <sparc/sparc/cpuvar.h>

#include <dev/ic/isp_openbsd.h>
#if	defined(ISP_COMPILE_FW) || defined(ISP_COMPILE_1000_FW)
#include <dev/microcode/isp/asm_sbus.h>
#endif

#define ISP_SBUSIFY_ISPHDR(isp, hdrp)					\
	ISP_SWAP8((hdrp)->rqs_entry_count, (hdrp)->rqs_entry_type);	\
	ISP_SWAP8((hdrp)->rqs_flags, (hdrp)->rqs_seqno);

#define	ISP_SWIZZLE_REQUEST(a, b)				\
	ISP_SBUSIFY_ISPHDR(a, &(b)->req_header);		\
        ISP_SWAP8((b)->req_target, (b)->req_lun_trn)


static int
isp_sbus_rd_isr(struct ispsoftc *, u_int16_t *, u_int16_t *, u_int16_t *);
static u_int16_t isp_sbus_rd_reg(struct ispsoftc *, int);
static void isp_sbus_wr_reg(struct ispsoftc *, int, u_int16_t);
static int isp_sbus_mbxdma(struct ispsoftc *);
static int isp_sbus_dmasetup(struct ispsoftc *, struct scsi_xfer *,
	ispreq_t *, u_int16_t *, u_int16_t);
static void
isp_sbus_dmateardown(struct ispsoftc *, struct scsi_xfer *, u_int16_t);
static int isp_sbus_intr(void *);

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
	(u_int16_t *) ISP_1000_RISC_CODE,
	BIU_BURST_ENABLE|BIU_SBUS_CONF1_FIFO_32
};

struct isp_sbussoftc {
	struct ispsoftc	sbus_isp;
	sdparam		sbus_dev;
	struct intrhand sbus_ih;
	volatile u_int16_t *sbus_reg;
	int		sbus_node;
	int		sbus_pri;
	struct ispmdvec	sbus_mdvec;
	int16_t		sbus_poff[_NREG_BLKS];
	vaddr_t	 	*sbus_kdma_allocs;
};


static int isp_match(struct device *, void *, void *);
static void isp_sbus_attach(struct device *, struct device *, void *);
struct cfattach isp_sbus_ca = {
	sizeof (struct isp_sbussoftc), isp_match, isp_sbus_attach
};

static int
isp_match(struct device *parent, void *cfarg, void *aux)
{
	int rv;
	struct cfdata *cf = cfarg;
#ifdef DEBUG
	static int oneshot = 1;
#endif
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	rv = (strcmp(cf->cf_driver->cd_name, ra->ra_name) == 0 ||
		strcmp("PTI,ptisp", ra->ra_name) == 0 ||
		strcmp("ptisp", ra->ra_name) == 0 ||
		strcmp("SUNW,isp", ra->ra_name) == 0 ||
		strcmp("QLGC,isp", ra->ra_name) == 0);
	if (rv == 0)
		return (rv);
#ifdef DEBUG
	if (rv && oneshot) {
		oneshot = 0;
		printf("Qlogic ISP Driver, OpenBSD (sbus) Platform Version "
		    "%d.%d Core Version %d.%d\n",
		    ISP_PLATFORM_VERSION_MAJOR, ISP_PLATFORM_VERSION_MINOR,
		    ISP_CORE_VERSION_MAJOR, ISP_CORE_VERSION_MINOR);
	}
#endif
	if (ca->ca_bustype == BUS_SBUS)
		return (1);
	ra->ra_len = NBPG;
	return (probeget(ra->ra_vaddr, 1) != -1);
}

static void
isp_sbus_attach(struct device *parent, struct device *self, void *aux)
{
	int freq, storebp = 0;
	struct confargs *ca = aux;
	struct bootpath *bp;
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) self;
	struct ispsoftc *isp = &sbc->sbus_isp;

	if (ca->ca_ra.ra_nintr != 1) {
		printf(": expected 1 interrupt, got %d\n", ca->ca_ra.ra_nintr);
		return;
	}

	printf("\n");

	sbc->sbus_pri = ca->ca_ra.ra_intr[0].int_pri;
	sbc->sbus_mdvec = mdvec;

	if (ca->ca_ra.ra_vaddr) {
		sbc->sbus_reg = (volatile u_int16_t *) ca->ca_ra.ra_vaddr;
	} else {
		sbc->sbus_reg = (volatile u_int16_t *)
			mapiodev(ca->ca_ra.ra_reg, 0, ca->ca_ra.ra_len);
	}
	sbc->sbus_node = ca->ca_ra.ra_node;

	freq = getpropint(ca->ca_ra.ra_node, "clock-frequency", 0);
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

	if ((bp = ca->ca_ra.ra_bp) != NULL) {
		if (bp->val[0] == ca->ca_slot &&
		    bp->val[1] == ca->ca_offset) {
			if (strcmp("isp", bp->name) == 0 ||
			    strcmp("QLGC,isp", bp->name) == 0 ||
			    strcmp("PTI,isp", bp->name) == 0 ||
			    strcmp("ptisp", bp->name) == 0) {
				storebp = 1;
			}
		}
	}

	/*
	 * XXX: Now figure out what the proper burst sizes, etc., to use.
	 */
	sbc->sbus_mdvec.dv_conf1 |= BIU_SBUS_CONF1_FIFO_8;

	/*
	 * Some early versions of the PTI SBus adapter
	 * would fail in trying to download (via poking)
	 * FW. We give up on them.
	 */
	if (strcmp("PTI,ptisp", ca->ca_ra.ra_name) == 0 ||
	    strcmp("ptisp", ca->ca_ra.ra_name) == 0) {
		sbc->sbus_mdvec.dv_ispfw = NULL;
	}

	isp->isp_mdvec = &sbc->sbus_mdvec;
	isp->isp_bustype = ISP_BT_SBUS;
	isp->isp_type = ISP_HA_SCSI_UNKNOWN;
	isp->isp_param = &sbc->sbus_dev;
	bzero(isp->isp_param, sizeof (sdparam));

	sbc->sbus_poff[BIU_BLOCK >> _BLK_REG_SHFT] = BIU_REGS_OFF;
	sbc->sbus_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = SBUS_MBOX_REGS_OFF;
	sbc->sbus_poff[SXP_BLOCK >> _BLK_REG_SHFT] = SBUS_SXP_REGS_OFF;
	sbc->sbus_poff[RISC_BLOCK >> _BLK_REG_SHFT] = SBUS_RISC_REGS_OFF;
	sbc->sbus_poff[DMA_BLOCK >> _BLK_REG_SHFT] = DMA_REGS_OFF;

	/* Establish interrupt channel */
	sbc->sbus_ih.ih_fun = (void *) isp_sbus_intr;
	sbc->sbus_ih.ih_arg = sbc;
	intr_establish(sbc->sbus_pri, &sbc->sbus_ih, IPL_BIO);

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
	if (storebp) {
		/*
		 * We're the booting HBA.
		 *
		 * Override the bootpath name with our driver name
		 * so we will do the correct matching and and store
		 * the next component's boot path entry, also so a
		 * successful match will occur.
		 */
		bcopy("isp", bp->name, 4);
		bp++;
		bootpath_store(1, bp);
	}
	isp_attach(isp);
	if (isp->isp_state != ISP_RUNSTATE) {
		isp_uninit(isp);
	}
	if (storebp) {
		bootpath_store(1, NULL);
	}
	ISP_UNLOCK(isp);
}

#define	IspVirt2Off(a, x)	\
	(((struct isp_sbussoftc *)a)->sbus_poff[((x) & _BLK_REG_MASK) >> \
	_BLK_REG_SHFT] + ((x) & 0xff))

#define	BXR2(pcs, off)		(sbc->sbus_reg[off >> 1])

static int
isp_sbus_rd_isr(struct ispsoftc *isp, u_int16_t *isrp,
    u_int16_t *semap, u_int16_t *mbp)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	volatile u_int16_t isr, sema;

	isr = BXR2(pcs, IspVirt2Off(isp, BIU_ISR));
	sema = BXR2(pcs, IspVirt2Off(isp, BIU_SEMA));
	isp_prt(isp, ISP_LOGDEBUG3, "ISR 0x%x SEMA 0x%x", isr, sema);
	isr &= INT_PENDING_MASK(isp);
	sema &= BIU_SEMA_LOCK;
	if (isr == 0 && sema == 0) {
		return (0);
	}
	*isrp = isr;
	if ((*semap = sema) != 0) {
		*mbp = BXR2(pcs, IspVirt2Off(isp, OUTMAILBOX0));
	}
	return (1);
}

static u_int16_t
isp_sbus_rd_reg(struct ispsoftc *isp, int regoff)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	int offset = sbc->sbus_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	return ((u_int16_t) sbc->sbus_reg[offset >> 1]);
}

static void
isp_sbus_wr_reg(struct ispsoftc *isp, int regoff, u_int16_t val)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	int offset = sbc->sbus_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	sbc->sbus_reg[offset >> 1] = val;
}


static int
isp_sbus_mbxdma(struct ispsoftc *isp)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	size_t len;

	if (isp->isp_rquest_dma)	/* been here before? */
		return (0);

	/*
	 * NOTE: Since most Sun machines aren't I/O coherent,
	 * map the mailboxes through kdvma space to force them
	 * to be uncached.
	 */

	len = isp->isp_maxcmds * sizeof (XS_T);
	isp->isp_xflist = (XS_T **) malloc(len, M_DEVBUF, M_WAITOK);
	bzero(isp->isp_xflist, len);
	len = isp->isp_maxcmds * sizeof (vaddr_t);
	sbc->sbus_kdma_allocs = (vaddr_t *) malloc(len, M_DEVBUF, M_WAITOK);
	bzero(sbc->sbus_kdma_allocs, len);

	/*
	 * Allocate and map the request queue.
	 */
	len = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
	isp->isp_rquest = (volatile caddr_t)malloc(len, M_DEVBUF, M_NOWAIT);
	if (isp->isp_rquest == 0) {
		printf("%s: cannot allocate request queue\n", isp->isp_name);
		return (1);
	}
	isp->isp_rquest_dma = (u_int32_t)
	    kdvma_mapin((caddr_t)isp->isp_rquest, len, 0);
	if (isp->isp_rquest_dma == 0) {
		printf("%s: could not mapin request queue\n", isp->isp_name);
		return (1);
	}

	/*
	 * Allocate and map the result queue.
	 */
	len = ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp));
	isp->isp_result = (volatile caddr_t)malloc(len, M_DEVBUF, M_NOWAIT);
	if (isp->isp_result == 0) {
		printf("%s: cannot allocate result queue\n", isp->isp_name);
		return (1);
	}
	isp->isp_result_dma = (u_int32_t)
	    kdvma_mapin((caddr_t)isp->isp_result, len, 0);
	if (isp->isp_result_dma == 0) {
		printf("%s: could not mapin result queue\n", isp->isp_name);
		return (1);
	}
	return (0);
}

/*
 * TODO: If kdvma_mapin fails, try using multiple smaller chunks..
 */

static int
isp_sbus_dmasetup(struct ispsoftc *isp, struct scsi_xfer *xs, ispreq_t *rq,
    u_int16_t *iptrp, u_int16_t optr)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	ispreq_t *qe;
	ispcontreq_t *crq;
	vaddr_t kdvma;
	int dosleep = (xs->flags & SCSI_NOSLEEP) != 0;

	qe = (ispreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, isp->isp_reqidx);
	if (xs->datalen == 0) {
		rq->req_seg_count = 1;
		goto mbxsync;
	}
	if (CPU_ISSUN4M) {
		kdvma = (vaddr_t)
			kdvma_mapin((caddr_t)xs->data, xs->datalen, dosleep);
		if (kdvma == (vaddr_t) 0) {
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_COMPLETE);
		}
	} else {
		kdvma = (vaddr_t) xs->data;
	}

	if (sbc->sbus_kdma_allocs[isp_handle_index(rq->req_handle)] != 0) {
		panic("%s: kdma handle already allocated", isp->isp_name);
		/* NOTREACHED */
	}
	if (XS_CDBLEN(xs) > 12) {
		crq = (ispcontreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, *iptrp);
		*iptrp = ISP_NXT_QENTRY(*iptrp, RQUEST_QUEUE_LEN(isp));
		if (*iptrp == optr) {
			printf("%s: Request Queue Overflow++\n", isp->isp_name);
			if (CPU_ISSUN4M) {
				dvma_mapout(kdvma,
				    (vaddr_t) xs->data, xs->datalen);
			}
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_EAGAIN);
		}
	} else {
		crq = NULL;
	}
	sbc->sbus_kdma_allocs[isp_handle_index(rq->req_handle)] = kdvma;
	if (xs->flags & SCSI_DATA_IN) {
		rq->req_flags |= REQFLAG_DATA_IN;
	} else {
		rq->req_flags |= REQFLAG_DATA_OUT;
	}
	if (crq) {
		rq->req_seg_count = 2;
		rq->req_dataseg[0].ds_count = 0;
		rq->req_dataseg[0].ds_base =  0;
		bzero((void *)crq, sizeof (*crq));
		crq->req_header.rqs_entry_count = 1;
		crq->req_header.rqs_entry_type = RQSTYPE_DATASEG;  
		crq->req_dataseg[0].ds_count = xs->datalen;
		crq->req_dataseg[0].ds_base =  (u_int32_t) kdvma;
                ISP_SBUSIFY_ISPHDR(isp, &crq->req_header)
	} else {
		rq->req_dataseg[0].ds_count = xs->datalen;
		rq->req_dataseg[0].ds_base =  (u_int32_t) kdvma;
		rq->req_seg_count = 1;
	}

mbxsync:
	ISP_SWIZZLE_REQUEST(isp, rq);
	bcopy(rq, qe, sizeof (ispreq_t));
	return (CMD_QUEUED);
}

static void
isp_sbus_dmateardown(struct ispsoftc *isp, XS_T *xs, u_int16_t handle)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	vaddr_t kdvma;

	if (xs->flags & SCSI_DATA_IN) {
		cpuinfo.cache_flush(xs->data, xs->datalen - xs->resid);
	}
	if (sbc->sbus_kdma_allocs[isp_handle_index(handle)] == (vaddr_t) 0) {
		panic("%s: kdma handle not already allocated", isp->isp_name);
		/* NOTREACHED */
	}
	kdvma = sbc->sbus_kdma_allocs[isp_handle_index(handle)];
	sbc->sbus_kdma_allocs[isp_handle_index(handle)] = (vaddr_t) 0;
	if (CPU_ISSUN4M) {
		dvma_mapout(kdvma, (vaddr_t) xs->data, xs->datalen);
	}
}

static int
isp_sbus_intr(void *arg)
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
