/*	$OpenBSD: isp_sbus.c,v 1.7 1999/03/25 22:58:37 mjacob Exp $	*/
/* release_03_25_99 */
/*
 * SBus specific probe and attach routines for Qlogic ISP SCSI adapters.
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
#include <dev/microcode/isp/asm_sbus.h>

static u_int16_t isp_sbus_rd_reg __P((struct ispsoftc *, int));
static void isp_sbus_wr_reg __P((struct ispsoftc *, int, u_int16_t));
static int isp_sbus_mbxdma __P((struct ispsoftc *));
static int isp_sbus_dmasetup __P((struct ispsoftc *, struct scsi_xfer *,
	ispreq_t *, u_int8_t *, u_int8_t));
static void isp_sbus_dmateardown __P((struct ispsoftc *, struct scsi_xfer *,
	u_int32_t));

static struct ispmdvec mdvec = {
	isp_sbus_rd_reg,
	isp_sbus_wr_reg,
	isp_sbus_mbxdma,
	isp_sbus_dmasetup,
	isp_sbus_dmateardown,
	NULL,
	NULL,
	NULL,
	ISP_RISC_CODE,
	ISP_CODE_LENGTH,
	ISP_CODE_ORG,
	ISP_CODE_VERSION,
	BIU_BURST_ENABLE,
	0
};

struct isp_sbussoftc {
	struct ispsoftc	sbus_isp;
	sdparam		sbus_dev;
	struct intrhand sbus_ih;
	volatile u_char *sbus_reg;
	int		sbus_node;
	int		sbus_pri;
	struct ispmdvec	sbus_mdvec;
	vm_offset_t	sbus_kdma_allocs[MAXISPREQUEST];
	int16_t		sbus_poff[_NREG_BLKS];
};


static int isp_match __P((struct device *, void *, void *));
static void isp_sbus_attach __P((struct device *, struct device *, void *));
struct cfattach isp_sbus_ca = {
	sizeof (struct isp_sbussoftc), isp_match, isp_sbus_attach
};

static int
isp_match(parent, cfarg, aux)
        struct device *parent;
	void *cfarg;
        void *aux;
{
	int rv;
	struct cfdata *cf = cfarg;
#ifdef DEBUG
	static int oneshot = 1;
#endif
	struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

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
		printf("Qlogic ISP Driver, NetBSD (sbus) Platform Version "
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
isp_sbus_attach(parent, self, aux)
        struct device *parent, *self;
        void *aux;
{
	int freq;
	struct confargs *ca = aux;
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) self;
	struct ispsoftc *isp = &sbc->sbus_isp;
	ISP_LOCKVAL_DECL;

	if (ca->ca_ra.ra_nintr != 1) {
		printf(": expected 1 interrupt, got %d\n", ca->ca_ra.ra_nintr);
		return;
	}

	printf("\n");

	sbc->sbus_pri = ca->ca_ra.ra_intr[0].int_pri;
	sbc->sbus_mdvec = mdvec;

	if (ca->ca_ra.ra_vaddr) {
		sbc->sbus_reg = (volatile u_char *) ca->ca_ra.ra_vaddr;
	} else {
		sbc->sbus_reg = (volatile u_char *)
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
		sbc->sbus_mdvec.dv_fwlen = 0;
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
	sbc->sbus_ih.ih_fun = (void *) isp_intr;
	sbc->sbus_ih.ih_arg = sbc;
	intr_establish(sbc->sbus_pri, &sbc->sbus_ih);

	ISP_LOCK(isp);
	isp_reset(isp);
	if (isp->isp_state != ISP_RESETSTATE) {
		ISP_UNLOCK(isp);
		return;
	}
	isp_init(isp);
	if (isp->isp_state != ISP_INITSTATE) {
		isp_uninit(isp);
		ISP_UNLOCK(isp);
		return;
	}
	/*
	 * do generic attach.
	 */
	isp_attach(isp);
	if (isp->isp_state != ISP_RUNSTATE) {
		isp_uninit(isp);
	}
	ISP_UNLOCK(isp);
}

static u_int16_t
isp_sbus_rd_reg(isp, regoff)
	struct ispsoftc *isp;
	int regoff;
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	int offset = sbc->sbus_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	return (*((u_int16_t *) &sbc->sbus_reg[offset]));
}

static void
isp_sbus_wr_reg (isp, regoff, val)
	struct ispsoftc *isp;
	int regoff;
	u_int16_t val;
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	int offset = sbc->sbus_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	*((u_int16_t *) &sbc->sbus_reg[offset]) = val;
}


static int
isp_sbus_mbxdma(isp)
	struct ispsoftc *isp;
{
	size_t len;

	/*
	 * NOTE: Since most Sun machines aren't I/O coherent,
	 * map the mailboxes through kdvma space to force them
	 * to be uncached.
	 */

	/*
	 * Allocate and map the request queue.
	 */
	len = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN);
	isp->isp_rquest = (volatile caddr_t)malloc(len, M_DEVBUF, M_NOWAIT);
	if (isp->isp_rquest == 0)
		return (1);
	isp->isp_rquest_dma = (u_int32_t)kdvma_mapin((caddr_t)isp->isp_rquest,
	    len, 0);
	if (isp->isp_rquest_dma == 0)
		return (1);

	/*
	 * Allocate and map the result queue.
	 */
	len = ISP_QUEUE_SIZE(RESULT_QUEUE_LEN);
	isp->isp_result = (volatile caddr_t)malloc(len, M_DEVBUF, M_NOWAIT);
	if (isp->isp_result == 0)
		return (1);
	isp->isp_result_dma = (u_int32_t)kdvma_mapin((caddr_t)isp->isp_result,
	    len, 0);
	if (isp->isp_result_dma == 0)
		return (1);

	return (0);
}

/*
 * TODO: If kdvma_mapin fails, try using multiple smaller chunks..
 */

static int
isp_sbus_dmasetup(isp, xs, rq, iptrp, optr)
	struct ispsoftc *isp;
	struct scsi_xfer *xs;
	ispreq_t *rq;
	u_int8_t *iptrp;
	u_int8_t optr;
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	vm_offset_t kdvma;
	int dosleep = (xs->flags & SCSI_NOSLEEP) != 0;

	if (xs->datalen == 0) {
		rq->req_seg_count = 1;
		return (CMD_QUEUED);
	}

	if (rq->req_handle > RQUEST_QUEUE_LEN ||
	    rq->req_handle < 1) {
		panic("%s: bad handle (%d) in isp_sbus_dmasetup\n",
			isp->isp_name, rq->req_handle);
		/* NOTREACHED */
	}
	if (CPU_ISSUN4M) {
		kdvma = (vm_offset_t)
			kdvma_mapin((caddr_t)xs->data, xs->datalen, dosleep);
		if (kdvma == (vm_offset_t) 0) {
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_COMPLETE);
		}
	} else {
		kdvma = (vm_offset_t) xs->data;
	}

	if (sbc->sbus_kdma_allocs[rq->req_handle - 1] != (vm_offset_t) 0) {
		panic("%s: kdma handle already allocated\n", isp->isp_name);
		/* NOTREACHED */
	}
	sbc->sbus_kdma_allocs[rq->req_handle - 1] = kdvma;
	if (xs->flags & SCSI_DATA_IN) {
		rq->req_flags |= REQFLAG_DATA_IN;
	} else {
		rq->req_flags |= REQFLAG_DATA_OUT;
	}
	rq->req_dataseg[0].ds_count = xs->datalen;
	rq->req_dataseg[0].ds_base =  (u_int32_t) kdvma;
	rq->req_seg_count = 1;
	return (CMD_QUEUED);
}

static void
isp_sbus_dmateardown(isp, xs, handle)
	struct ispsoftc *isp;
	struct scsi_xfer *xs;
	u_int32_t handle;
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	vm_offset_t kdvma;

	if (xs->flags & SCSI_DATA_IN) {
		cpuinfo.cache_flush(xs->data, xs->datalen - xs->resid);
	}
	if (handle >= RQUEST_QUEUE_LEN) {
		panic("%s: bad handle (%d) in isp_sbus_dmateardown\n",
			isp->isp_name, handle);
		/* NOTREACHED */
	}
	if (sbc->sbus_kdma_allocs[handle] == (vm_offset_t) 0) {
		panic("%s: kdma handle not already allocated\n", isp->isp_name);
		/* NOTREACHED */
	}
	kdvma = sbc->sbus_kdma_allocs[handle];
	sbc->sbus_kdma_allocs[handle] = (vm_offset_t) 0;
	if (CPU_ISSUN4M) {
		dvma_mapout(kdvma, (vm_offset_t) xs->data, xs->datalen);
	}
}
