/*	$OpenBSD: isp_sbus.c,v 1.4 1999/01/11 05:11:56 millert Exp $	*/
/*	$NetBSD: isp_sbus.c,v 1.8 1997/08/27 11:24:19 bouyer Exp $	*/

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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/device.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <machine/autoconf.h>
#include <sparc/cpu.h>
#include <sparc/sparc/cpuvar.h>
#include <sparc/dev/sbusvar.h>

#include <dev/ic/ispreg.h>
#include <dev/ic/ispvar.h>
#include <dev/microcode/isp/asm_sbus.h>
#include <machine/param.h>
#include <machine/vmparam.h>


static u_int16_t isp_sbus_rd_reg __P((struct ispsoftc *, int));
static void isp_sbus_wr_reg __P((struct ispsoftc *, int, u_int16_t));
static int isp_sbus_mbxdma __P((struct ispsoftc *));
static int isp_sbus_dmasetup __P((struct ispsoftc *, struct scsipi_xfer *,
	ispreq_t *, u_int8_t *, u_int8_t));
static void isp_sbus_dmateardown __P((struct ispsoftc *, struct scsipi_xfer *,
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
	0,
	0
};

struct isp_sbussoftc {
	struct ispsoftc sbus_isp;
	sdparam sbus_dev;
	struct intrhand sbus_ih;
	volatile u_char *sbus_reg;
	int sbus_node;
	int sbus_pri;
	vm_offset_t sbus_kdma_allocs[MAXISPREQUEST];
};


static int isp_match __P((struct device *, void *, void *));
static void isp_sbus_attach __P((struct device *, struct device *, void *));
struct cfattach isp_sbus_ca = {
        sizeof (struct isp_sbussoftc), isp_match, isp_sbus_attach
};

static int
isp_match(parent, vcf, aux)
        struct device *parent;
        void *vcf, *aux;
{
        struct cfdata *cf = vcf;
        struct confargs *ca = aux;
        register struct romaux *ra = &ca->ca_ra;

	if (strcmp(cf->cf_driver->cd_name, ra->ra_name) &&
	    strcmp("SUNW,isp", ra->ra_name) &&
	    strcmp("QLGC,isp", ra->ra_name)) {
		return (0);
	}
	if (ca->ca_bustype == BUS_SBUS) {
		if (!sbus_testdma((struct sbus_softc *)parent, ca))
			return (0);
		return (1);
	}
	ra->ra_len = NBPG;
	return (probeget(ra->ra_vaddr, 1) != -1);
}

static void    
isp_sbus_attach(parent, self, aux)
        struct device *parent, *self;
        void *aux;
{
	struct confargs *ca = aux;
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) self;

	if (ca->ca_ra.ra_nintr != 1) {
		printf(": expected 1 interrupt, got %d\n", ca->ca_ra.ra_nintr);
		return;
	}

	sbc->sbus_pri = ca->ca_ra.ra_intr[0].int_pri;
	printf(" pri %d\n", sbc->sbus_pri);

	if (ca->ca_ra.ra_vaddr) {
		sbc->sbus_reg = (volatile u_char *) ca->ca_ra.ra_vaddr;
	} else {
		sbc->sbus_reg = (volatile u_char *)
		    mapiodev(ca->ca_ra.ra_reg, 0, ca->ca_ra.ra_len);
	}
	sbc->sbus_node = ca->ca_ra.ra_node;

	sbc->sbus_isp.isp_mdvec = &mdvec;
	sbc->sbus_isp.isp_type = ISP_HA_SCSI_UNKNOWN;
	sbc->sbus_isp.isp_param = &sbc->sbus_dev;
	bzero(sbc->sbus_isp.isp_param, sizeof (sdparam));
	isp_reset(&sbc->sbus_isp);
	if (sbc->sbus_isp.isp_state != ISP_RESETSTATE) {
		return;
	}
	isp_init(&sbc->sbus_isp);
	if (sbc->sbus_isp.isp_state != ISP_INITSTATE) {
		isp_uninit(&sbc->sbus_isp);
		return;
	}
	sbc->sbus_ih.ih_fun = (void *) isp_intr;
	sbc->sbus_ih.ih_arg = sbc;
	intr_establish(sbc->sbus_pri, &sbc->sbus_ih);

	/*
	 * Do Generic attach now.
	 */
	isp_attach(&sbc->sbus_isp);
	if (sbc->sbus_isp.isp_state != ISP_RUNSTATE) {
		isp_uninit(&sbc->sbus_isp);
	}
}

#define  SBUS_BIU_REGS_OFF		0x00
#define	 SBUS_MBOX_REGS_OFF		0x80
#define	 SBUS_SXP_REGS_OFF		0x200
#define	 SBUS_RISC_REGS_OFF		0x400

static u_int16_t
isp_sbus_rd_reg(isp, regoff)
	struct ispsoftc *isp;
	int regoff;
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;

	int offset;
	if ((regoff & BIU_BLOCK) != 0) {
		offset = SBUS_BIU_REGS_OFF;
	} else if ((regoff & MBOX_BLOCK) != 0) {
		offset = SBUS_MBOX_REGS_OFF;
	} else if ((regoff & SXP_BLOCK) != 0) {
		offset = SBUS_SXP_REGS_OFF;
	} else {
		offset = SBUS_RISC_REGS_OFF;
	}
	regoff &= 0xff;
	offset += regoff;
	return (*((u_int16_t *) &sbc->sbus_reg[offset]));
}

static void
isp_sbus_wr_reg (isp, regoff, val)
	struct ispsoftc *isp;
	int regoff;
	u_int16_t val;
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	int offset;

	if ((regoff & BIU_BLOCK) != 0) {
		offset = SBUS_BIU_REGS_OFF;
	} else if ((regoff & MBOX_BLOCK) != 0) {
		offset = SBUS_MBOX_REGS_OFF;
	} else if ((regoff & SXP_BLOCK) != 0) {
		offset = SBUS_SXP_REGS_OFF;
	} else {
		offset = SBUS_RISC_REGS_OFF;
	}
	regoff &= 0xff;
	offset += regoff;
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
	len = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
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
	len = ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp));
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
		return (0);
	}

	if (rq->req_handle > RQUEST_QUEUE_LEN(isp) ||
	    rq->req_handle < 1) {
		panic("%s: bad handle (%d) in isp_sbus_dmasetup",
			isp->isp_name, rq->req_handle);
		/* NOTREACHED */
	}
	if (CPU_ISSUN4M) {
		kdvma = (vm_offset_t)
			kdvma_mapin((caddr_t)xs->data, xs->datalen, dosleep);
		if (kdvma == (vm_offset_t) 0) {
			return (1);
		}
	} else {
		kdvma = (vm_offset_t) xs->data;
	}

	if (sbc->sbus_kdma_allocs[rq->req_handle - 1] != (vm_offset_t) 0) {
		panic("%s: kdma handle already allocated", isp->isp_name);
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
	return (0);
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

	if (handle >= RQUEST_QUEUE_LEN(isp)) {
		panic("%s: bad handle (%d) in isp_sbus_dmateardown",
			isp->isp_name, handle);
		/* NOTREACHED */
	}
	if (sbc->sbus_kdma_allocs[handle] == (vm_offset_t) 0) {
		panic("%s: kdma handle not already allocated", isp->isp_name);
		/* NOTREACHED */
	}
	kdvma = sbc->sbus_kdma_allocs[handle];
	sbc->sbus_kdma_allocs[handle] = (vm_offset_t) 0;
	if (CPU_ISSUN4M) {
		dvma_mapout(kdvma, (vm_offset_t) xs->data, xs->datalen);
	}
}
