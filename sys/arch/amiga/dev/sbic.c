/*	$NetBSD: sbic.c,v 1.21 1996/01/07 22:01:54 thorpej Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)scsi.c	7.5 (Berkeley) 5/4/91
 */

/*
 * AMIGA AMD 33C93 scsi adaptor driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h> /* For hz */
#include <sys/disklabel.h>
#include <sys/dkstat.h>
#include <sys/buf.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <machine/pmap.h>
#include <machine/cpu.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/custom.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/dmavar.h>
#include <amiga/dev/sbicreg.h>
#include <amiga/dev/sbicvar.h>

/* These are for bounce buffers */
#include <amiga/amiga/cc.h>
#include <amiga/dev/zbusvar.h>

#include <vm/pmap.h>

/* Since I can't find this in any other header files */
#define SCSI_PHASE(reg)	(reg&0x07)

/*
 * SCSI delays
 * In u-seconds, primarily for state changes on the SPC.
 */
#define	SBIC_CMD_WAIT	50000	/* wait per step of 'immediate' cmds */
#define	SBIC_DATA_WAIT	50000	/* wait per data in/out step */
#define	SBIC_INIT_WAIT	50000	/* wait per step (both) during init */

#define	b_cylin		b_resid
#define SBIC_WAIT(regs, until, timeo) sbicwait(regs, until, timeo, __LINE__)

extern u_int kvtop();

int  sbicicmd __P((struct sbic_softc *, int, int, void *, int, void *, int));
int  sbicgo __P((struct sbic_softc *, struct scsi_xfer *));
int  sbicdmaok __P((struct sbic_softc *, struct scsi_xfer *));
int  sbicwait __P((sbic_regmap_p, char, int , int));
int  sbiccheckdmap __P((void *, u_long, u_long));
int  sbicselectbus __P((struct sbic_softc *, sbic_regmap_p, u_char, u_char, u_char));
int  sbicxfstart __P((sbic_regmap_p, int, u_char, int));
int  sbicxfout __P((sbic_regmap_p regs, int, void *, int));
int  sbicfromscsiperiod __P((struct sbic_softc *, sbic_regmap_p, int));
int  sbictoscsiperiod __P((struct sbic_softc *, sbic_regmap_p, int));
int  sbicintr __P((struct sbic_softc *));
int  sbicpoll __P((struct sbic_softc *));
int  sbicnextstate __P((struct sbic_softc *, u_char, u_char));
int  sbicmsgin __P((struct sbic_softc *));
int  sbicxfin __P((sbic_regmap_p regs, int, void *));
int  sbicabort __P((struct sbic_softc *, sbic_regmap_p, char *));
void sbicxfdone __P((struct sbic_softc *, sbic_regmap_p, int));
void sbicerror __P((struct sbic_softc *, sbic_regmap_p, u_char));
void sbicstart __P((struct sbic_softc *));
void sbicreset __P((struct sbic_softc *));
void sbic_scsidone __P((struct sbic_acb *, int));
void sbic_sched __P((struct sbic_softc *));
void sbic_save_ptrs __P((struct sbic_softc *, sbic_regmap_p,int,int));
void sbic_load_ptrs __P((struct sbic_softc *, sbic_regmap_p,int,int));

/*
 * Synch xfer parameters, and timing conversions
 */
int sbic_min_period = SBIC_SYN_MIN_PERIOD;  /* in cycles = f(ICLK,FSn) */
int sbic_max_offset = SBIC_SYN_MAX_OFFSET;  /* pure number */

int sbic_cmd_wait = SBIC_CMD_WAIT;
int sbic_data_wait = SBIC_DATA_WAIT;
int sbic_init_wait = SBIC_INIT_WAIT;

/*
 * was broken before.. now if you want this you get it for all drives
 * on sbic controllers.
 */
u_char sbic_inhibit_sync[8];
int sbic_enable_reselect = 1;
int sbic_clock_override = 0;
int sbic_no_dma = 0;
int sbic_parallel_operations = 1;

#ifdef DEBUG
sbic_regmap_p debug_sbic_regs;
int	sbicdma_ops = 0;	/* total DMA operations */
int	sbicdma_bounces = 0;	/* number operations using bounce buffer */
int	sbicdma_hits = 0;	/* number of DMA chains that were contiguous */
int	sbicdma_misses = 0;	/* number of DMA chains that were not contiguous */
int     sbicdma_saves = 0;
#define QPRINTF(a) if (sbic_debug > 1) printf a
int	sbic_debug = 0;
int	sync_debug = 0;
int	sbic_dma_debug = 0;
int	reselect_debug = 0;
int	report_sense = 0;
int	data_pointer_debug = 0;
u_char	debug_asr, debug_csr, routine;
void sbictimeout __P((struct sbic_softc *dev));
void sbic_dump __P((struct sbic_softc *dev));

#define CSR_TRACE_SIZE 32
#if CSR_TRACE_SIZE
#define CSR_TRACE(w,c,a,x) do { \
	int s = splbio(); \
	csr_trace[csr_traceptr].whr = (w); csr_trace[csr_traceptr].csr = (c); \
	csr_trace[csr_traceptr].asr = (a); csr_trace[csr_traceptr].xtn = (x); \
	dma_cachectl(&csr_trace[csr_traceptr], sizeof(csr_trace[0])); \
	csr_traceptr = (csr_traceptr + 1) & (CSR_TRACE_SIZE - 1); \
/*	dma_cachectl(&csr_traceptr, sizeof(csr_traceptr));*/ \
	splx(s); \
} while (0)
int csr_traceptr;
int csr_tracesize = CSR_TRACE_SIZE;
struct {
	u_char whr;
	u_char csr;
	u_char asr;
	u_char xtn;
} csr_trace[CSR_TRACE_SIZE];
#else
#define CSR_TRACE
#endif

#define SBIC_TRACE_SIZE 0
#if SBIC_TRACE_SIZE
#define SBIC_TRACE(dev) do { \
	int s = splbio(); \
	sbic_trace[sbic_traceptr].sp = &s; \
	sbic_trace[sbic_traceptr].line = __LINE__; \
	sbic_trace[sbic_traceptr].sr = s; \
	sbic_trace[sbic_traceptr].csr = csr_traceptr; \
	dma_cachectl(&sbic_trace[sbic_traceptr], sizeof(sbic_trace[0])); \
	sbic_traceptr = (sbic_traceptr + 1) & (SBIC_TRACE_SIZE - 1); \
	dma_cachectl(&sbic_traceptr, sizeof(sbic_traceptr)); \
	if (dev) dma_cachectl(dev, sizeof(*dev)); \
	splx(s); \
} while (0)
int sbic_traceptr;
int sbic_tracesize = SBIC_TRACE_SIZE;
struct {
	void *sp;
	u_short line;
	u_short sr;
	int csr;
} sbic_trace[SBIC_TRACE_SIZE];
#else
#define SBIC_TRACE
#endif

#else
#define QPRINTF
#define CSR_TRACE
#define SBIC_TRACE
#endif

/*
 * default minphys routine for sbic based controllers
 */
void
sbic_minphys(bp)
	struct buf *bp;
{

	/*
	 * No max transfer at this level.
	 */
	minphys(bp);
}

/*
 * Save DMA pointers.  Take into account partial transfer. Shut down DMA.
 */
void
sbic_save_ptrs(dev, regs, target, lun)
	struct sbic_softc *dev;
	sbic_regmap_p regs;
	int target, lun;
{
	int count, asr, csr, s;
	unsigned long ptr;
	char *vptr;
	struct sbic_acb* acb;

	extern vm_offset_t vm_first_phys;

	SBIC_TRACE(dev);
	if( !dev->sc_cur ) return;
	if( !(dev->sc_flags & SBICF_INDMA) ) return; /* DMA not active */

	s = splbio();

	acb = dev->sc_nexus;
	count = -1;
	do {
		GET_SBIC_asr(regs, asr);
		if( asr & SBIC_ASR_DBR ) {
			printf("sbic_save_ptrs: asr %02x canceled!\n", asr);
			splx(s);
			SBIC_TRACE(dev);
			return;
		}
	} while( asr & (SBIC_ASR_BSY|SBIC_ASR_CIP) );

	/* Save important state */
	/* must be done before dmastop */
	acb->sc_dmacmd = dev->sc_dmacmd;
	SBIC_TC_GET(regs, count);

	/* Shut down DMA ====CAREFUL==== */
	dev->sc_dmastop(dev);
	dev->sc_flags &= ~SBICF_INDMA;
	SBIC_TC_PUT(regs, 0);

#ifdef DEBUG
	if(!count && sbic_debug) printf("%dcount0",target);
	if(data_pointer_debug == -1)
		printf("SBIC saving target %d data pointers from (%x,%x)%xASR:%02x",
		       target, dev->sc_cur->dc_addr, dev->sc_cur->dc_count,
		       acb->sc_dmacmd, asr);
#endif

	/* Fixup partial xfers */
	acb->sc_kv.dc_addr += (dev->sc_tcnt - count);
	acb->sc_kv.dc_count -= (dev->sc_tcnt - count);
	acb->sc_pa.dc_addr += (dev->sc_tcnt - count);
	acb->sc_pa.dc_count -= ((dev->sc_tcnt - count)>>1);

	acb->sc_tcnt = dev->sc_tcnt = count;
#ifdef DEBUG
	if(data_pointer_debug)
		printf(" at (%x,%x):%x\n",
		       dev->sc_cur->dc_addr, dev->sc_cur->dc_count,count);
	sbicdma_saves++;
#endif
	splx(s);
	SBIC_TRACE(dev);
}


/*
 * DOES NOT RESTART DMA!!!
 */
void sbic_load_ptrs(dev, regs, target, lun)
	struct sbic_softc *dev;
	sbic_regmap_p regs;
	int target, lun;
{
	int i, s, asr, count;
	char* vaddr, * paddr;
	struct sbic_acb *acb;

	SBIC_TRACE(dev);
	acb = dev->sc_nexus;
	if( !acb->sc_kv.dc_count ) {
		/* No data to xfer */
		SBIC_TRACE(dev);
		return;
	}

	s = splbio();

	dev->sc_last = dev->sc_cur = &acb->sc_pa;
	dev->sc_tcnt = acb->sc_tcnt;
	dev->sc_dmacmd = acb->sc_dmacmd;

#ifdef DEBUG
	sbicdma_ops++;
#endif
	if( !dev->sc_tcnt ) {
		/* sc_tcnt == 0 implies end of segment */

		/* do kvm to pa mappings */
		paddr = acb->sc_pa.dc_addr =
			(char *) kvtop(acb->sc_kv.dc_addr);

		vaddr = acb->sc_kv.dc_addr;
		count = acb->sc_kv.dc_count;
		for(count = (NBPG - ((int)vaddr & PGOFSET));
		    count < acb->sc_kv.dc_count
		    && (char*)kvtop(vaddr + count + 4) == paddr + count + 4;
		    count += NBPG);
		/* If it's all contiguous... */
		if(count > acb->sc_kv.dc_count ) {
			count = acb->sc_kv.dc_count;
#ifdef DEBUG
			sbicdma_hits++;
#endif
		} else {
#ifdef DEBUG
			sbicdma_misses++;
#endif
		}
		acb->sc_tcnt = count;
		acb->sc_pa.dc_count = count >> 1;

#ifdef DEBUG
		if(data_pointer_debug)
			printf("DMA recalc:kv(%x,%x)pa(%x,%x)\n",
			       acb->sc_kv.dc_addr,
			       acb->sc_kv.dc_count,
			       acb->sc_pa.dc_addr,
			       acb->sc_tcnt);
#endif
	}
	splx(s);
#ifdef DEBUG
	if(data_pointer_debug)
		printf("SBIC restoring target %d data pointers at (%x,%x)%x\n",
		       target, dev->sc_cur->dc_addr, dev->sc_cur->dc_count,
		       dev->sc_dmacmd);
#endif
	SBIC_TRACE(dev);
}

/*
 * used by specific sbic controller
 *
 * it appears that the higher level code does nothing with LUN's
 * so I will too.  I could plug it in, however so could they
 * in scsi_scsi_cmd().
 */
int
sbic_scsicmd(xs)
	struct scsi_xfer *xs;
{
	struct sbic_acb *acb;
	struct sbic_softc *dev;
	struct scsi_link *slp;
	int flags, s, stat;

	slp = xs->sc_link;
	dev = slp->adapter_softc;
	SBIC_TRACE(dev);
	flags = xs->flags;

	if (flags & SCSI_DATA_UIO)
		panic("sbic: scsi data uio requested");

	if (dev->sc_nexus && flags & SCSI_POLL)
		panic("sbic_scsicmd: busy");

	if (slp->target == slp->adapter_target)
		return ESCAPE_NOT_SUPPORTED;

	s = splbio();
	acb = dev->free_list.tqh_first;
	if (acb)
		TAILQ_REMOVE(&dev->free_list, acb, chain);
	splx(s);

	if (acb == NULL) {
#ifdef DEBUG
		printf("sbic_scsicmd: unable to queue request for target %d\n",
		    slp->target);
#ifdef DDB
		Debugger();
#endif
#endif
		xs->error = XS_DRIVER_STUFFUP;
		SBIC_TRACE(dev);
		return(TRY_AGAIN_LATER);
	}

	acb->flags = ACB_ACTIVE;
	if (flags & SCSI_DATA_IN)
		acb->flags |= ACB_DATAIN;
	acb->xs = xs;
	bcopy(xs->cmd, &acb->cmd, xs->cmdlen);
	acb->clen = xs->cmdlen;
	acb->sc_kv.dc_addr = xs->data;
	acb->sc_kv.dc_count = xs->datalen;
	acb->pa_addr = xs->data ? (char *)kvtop(xs->data) : 0;	/* XXXX check */

	if (flags & SCSI_POLL) {
		s = splbio();
		/*
		 * This has major side effects -- it locks up the machine
		 */

		dev->sc_flags |= SBICF_ICMD;
		do {
			while(dev->sc_nexus)
				sbicpoll(dev);
			dev->sc_nexus = acb;
			dev->sc_stat[0] = -1;
			dev->sc_xs = xs;
			dev->target = slp->target;
			dev->lun = slp->lun;
			stat = sbicicmd(dev, slp->target, slp->lun,
					&acb->cmd, acb->clen,
					acb->sc_kv.dc_addr, acb->sc_kv.dc_count);
		} while (dev->sc_nexus != acb);
		sbic_scsidone(acb, stat);

		splx(s);
		SBIC_TRACE(dev);
		return(COMPLETE);
	}

	s = splbio();
	TAILQ_INSERT_TAIL(&dev->ready_list, acb, chain);

	if (dev->sc_nexus) {
		splx(s);
		SBIC_TRACE(dev);
		return(SUCCESSFULLY_QUEUED);
	}

	/*
	 * nothing is active, try to start it now.
	 */
	sbic_sched(dev);
	splx(s);

	SBIC_TRACE(dev);
/* TODO:  add sbic_poll to do SCSI_POLL operations */
#if 0
	if (flags & SCSI_POLL)
		return(COMPLETE);
#endif
	return(SUCCESSFULLY_QUEUED);
}

/*
 * attempt to start the next available command
 */
void
sbic_sched(dev)
	struct sbic_softc *dev;
{
	struct scsi_xfer *xs;
	struct scsi_link *slp;
	struct sbic_acb *acb;
	int flags, /*phase,*/ stat, i;

	SBIC_TRACE(dev);
	if (dev->sc_nexus)
		return;			/* a command is current active */

	SBIC_TRACE(dev);
	for (acb = dev->ready_list.tqh_first; acb; acb = acb->chain.tqe_next) {
		slp = acb->xs->sc_link;
		i = slp->target;
		if (!(dev->sc_tinfo[i].lubusy & (1 << slp->lun))) {
			struct sbic_tinfo *ti = &dev->sc_tinfo[i];

			TAILQ_REMOVE(&dev->ready_list, acb, chain);
			dev->sc_nexus = acb;
			slp = acb->xs->sc_link;
			ti = &dev->sc_tinfo[slp->target];
			ti->lubusy |= (1 << slp->lun);
			acb->sc_pa.dc_addr = acb->pa_addr;	/* XXXX check */
			break;
		}
	}

	SBIC_TRACE(dev);
	if (acb == NULL)
		return;			/* did not find an available command */

	dev->sc_xs = xs = acb->xs;
	slp = xs->sc_link;
	flags = xs->flags;

	if (flags & SCSI_RESET)
		sbicreset(dev);

#ifdef DEBUG
	if( data_pointer_debug > 1 )
		printf("sbic_sched(%d,%d)\n",slp->target,slp->lun);
#endif
	dev->sc_stat[0] = -1;
	dev->target = slp->target;
	dev->lun = slp->lun;
	if ( flags & SCSI_POLL || ( !sbic_parallel_operations
				   && (/*phase == STATUS_PHASE ||*/
				       sbicdmaok(dev, xs) == 0) ) )
		stat = sbicicmd(dev, slp->target, slp->lun, &acb->cmd,
		    acb->clen, acb->sc_kv.dc_addr, acb->sc_kv.dc_count);
	else if (sbicgo(dev, xs) == 0) {
		SBIC_TRACE(dev);
		return;
	} else
		stat = dev->sc_stat[0];

	sbic_scsidone(acb, stat);
	SBIC_TRACE(dev);
}

void
sbic_scsidone(acb, stat)
	struct sbic_acb *acb;
	int stat;
{
	struct scsi_xfer *xs;
	struct scsi_link *slp;
	struct sbic_softc *dev;
	int s, dosched = 0;

	xs = acb->xs;
	slp = xs->sc_link;
	dev = slp->adapter_softc;
	SBIC_TRACE(dev);
#ifdef DIAGNOSTIC
	if (acb == NULL || xs == NULL) {
		printf("sbic_scsidone -- (%d,%d) no scsi_xfer\n",
		       dev->target, dev->lun);
#ifdef DDB
		Debugger();
#endif
		return;
	}
#endif
	/*
	 * XXX Support old-style instrumentation for now.
	 * IS THIS REALLY THE RIGHT PLACE FOR THIS?  --thorpej
	 */
	if (slp->device_softc &&
	    ((struct device *)(slp->device_softc))->dv_unit < dk_ndrive)
		++dk_xfer[((struct device *)(slp->device_softc))->dv_unit];
	/*
	 * is this right?
	 */
	xs->status = stat;

#ifdef DEBUG
	if( data_pointer_debug > 1 )
		printf("scsidone: (%d,%d)->(%d,%d)%02x\n",
		       slp->target, slp->lun,
		       dev->target,  dev->lun,  stat);
	if( xs->sc_link->target == dev->sc_link.adapter_target )
		panic("target == hostid");
#endif

	if (xs->error == XS_NOERROR && !(acb->flags & ACB_CHKSENSE)) {
		if (stat == SCSI_CHECK) {
			/* Schedule a REQUEST SENSE */
			struct scsi_sense *ss = (void *)&acb->cmd;
#ifdef DEBUG
			if (report_sense)
				printf("sbic_scsidone: autosense %02x targ %d lun %d",
				    acb->cmd.opcode, slp->target, slp->lun);
#endif
			bzero(ss, sizeof(*ss));
			ss->opcode = REQUEST_SENSE;
			ss->byte2 = slp->lun << 5;
			ss->length = sizeof(struct scsi_sense_data);
			acb->clen = sizeof(*ss);
			acb->sc_kv.dc_addr = (char *)&xs->sense;
			acb->sc_kv.dc_count = sizeof(struct scsi_sense_data);
			acb->pa_addr = (char *)kvtop(&xs->sense); /* XXX check */
			acb->flags = ACB_ACTIVE | ACB_CHKSENSE | ACB_DATAIN;
			TAILQ_INSERT_HEAD(&dev->ready_list, acb, chain);
			dev->sc_tinfo[slp->target].lubusy &=
			    ~(1 << slp->lun);
			dev->sc_tinfo[slp->target].senses++;
			if (dev->sc_nexus == acb) {
				dev->sc_nexus = NULL;
				dev->sc_xs = NULL;
				sbic_sched(dev);
			}
			SBIC_TRACE(dev);
			return;
		}
	}
	if (xs->error == XS_NOERROR && (acb->flags & ACB_CHKSENSE)) {
		xs->error = XS_SENSE;
#ifdef DEBUG
		if (report_sense)
			printf(" => %02x %02x\n", xs->sense.extended_flags,
			    xs->sense.extended_extra_bytes[3]);
#endif
	} else {
		xs->resid = 0;		/* XXXX */
	}
#if whataboutthisone
		case SCSI_BUSY:
			xs->error = XS_BUSY;
			break;
#endif
	xs->flags |= ITSDONE;

	/*
	 * Remove the ACB from whatever queue it's on.  We have to do a bit of
	 * a hack to figure out which queue it's on.  Note that it is *not*
	 * necessary to cdr down the ready queue, but we must cdr down the
	 * nexus queue and see if it's there, so we can mark the unit as no
	 * longer busy.  This code is sickening, but it works.
	 */
	if (acb == dev->sc_nexus) {
		dev->sc_nexus = NULL;
		dev->sc_xs = NULL;
		dev->sc_tinfo[slp->target].lubusy &= ~(1<<slp->lun);
		if (dev->ready_list.tqh_first)
			dosched = 1;	/* start next command */
	} else if (dev->ready_list.tqh_last == &acb->chain.tqe_next) {
		TAILQ_REMOVE(&dev->ready_list, acb, chain);
	} else {
		register struct sbic_acb *acb2;
		for (acb2 = dev->nexus_list.tqh_first; acb2;
		    acb2 = acb2->chain.tqe_next) {
			if (acb2 == acb) {
				TAILQ_REMOVE(&dev->nexus_list, acb, chain);
				dev->sc_tinfo[slp->target].lubusy
					&= ~(1<<slp->lun);
				break;
			}
		}
		if (acb2)
			;
		else if (acb->chain.tqe_next) {
			TAILQ_REMOVE(&dev->ready_list, acb, chain);
		} else {
			printf("%s: can't find matching acb\n",
			    dev->sc_dev.dv_xname);
#ifdef DDB
			Debugger();
#endif
		}
	}
	/* Put it on the free list. */
	acb->flags = ACB_FREE;
	TAILQ_INSERT_HEAD(&dev->free_list, acb, chain);

	dev->sc_tinfo[slp->target].cmds++;

	scsi_done(xs);

	if (dosched)
		sbic_sched(dev);
	SBIC_TRACE(dev);
}

int
sbicdmaok(dev, xs)
	struct sbic_softc *dev;
	struct scsi_xfer *xs;
{
	if (sbic_no_dma || xs->datalen & 0x1 || (u_int)xs->data & 0x3)
		return(0);
	/*
	 * controller supports dma to any addresses?
	 */
	else if ((dev->sc_flags & SBICF_BADDMA) == 0)
		return(1);
	/*
	 * this address is ok for dma?
	 */
	else if (sbiccheckdmap(xs->data, xs->datalen, dev->sc_dmamask) == 0)
		return(1);
	/*
	 * we have a bounce buffer?
	 */
	else if (dev->sc_tinfo[xs->sc_link->target].bounce)
		return(1);
	/*
	 * try to get one
	 */
	else if (dev->sc_tinfo[xs->sc_link->target].bounce
		 = (char *)alloc_z2mem(MAXPHYS)) {
		if (isztwomem(dev->sc_tinfo[xs->sc_link->target].bounce))
			printf("alloc ZII target %d bounce pa 0x%x\n",
			       xs->sc_link->target,
			       kvtop(dev->sc_tinfo[xs->sc_link->target].bounce));
		else if (dev->sc_tinfo[xs->sc_link->target].bounce)
			printf("alloc CHIP target %d bounce pa 0x%x\n",
			       xs->sc_link->target,
			       PREP_DMA_MEM(dev->sc_tinfo[xs->sc_link->target].bounce));
		return(1);
	}

	return(0);
}


int
sbicwait(regs, until, timeo, line)
	sbic_regmap_p regs;
	char until;
	int timeo;
	int line;
{
	u_char val;
	int csr;

	SBIC_TRACE((struct sbic_softc *)0);
	if (timeo == 0)
		timeo = 1000000;	/* some large value.. */

	GET_SBIC_asr(regs,val);
	while ((val & until) == 0) {
		if (timeo-- == 0) {
			GET_SBIC_csr(regs, csr);
			printf("sbicwait TIMEO @%d with asr=x%x csr=x%x\n",
			    line, val, csr);
#if defined(DDB) && defined(DEBUG)
			Debugger();
#endif
			return(val); /* Maybe I should abort */
			break;
		}
		DELAY(1);
		GET_SBIC_asr(regs,val);
	}
	SBIC_TRACE((struct sbic_softc *)0);
	return(val);
}

int
sbicabort(dev, regs, where)
	struct sbic_softc *dev;
	sbic_regmap_p regs;
	char *where;
{
	u_char csr, asr;

	GET_SBIC_asr(regs, asr);
	GET_SBIC_csr(regs, csr);

	printf ("%s: abort %s: csr = 0x%02x, asr = 0x%02x\n",
	    dev->sc_dev.dv_xname, where, csr, asr);


#if 0
	/* Clean up running command */
	if (dev->sc_nexus != NULL) {
		dev->sc_nexus->xs->error = XS_DRIVER_STUFFUP;
		sbic_scsidone(dev->sc_nexus, dev->sc_stat[0]);
	}
	while (acb = dev->nexus_list.tqh_first) {
		acb->xs->error = XS_DRIVER_STUFFUP;
		sbic_scsidone(acb, -1 /*acb->stat[0]*/);
	}
#endif

	/* Clean up chip itself */
	if (dev->sc_flags & SBICF_SELECTED) {
		while( asr & SBIC_ASR_DBR ) {
			/* sbic is jammed w/data. need to clear it */
			/* But we don't know what direction it needs to go */
			GET_SBIC_data(regs, asr);
			printf("%s: abort %s: clearing data buffer 0x%02x\n",
			       dev->sc_dev.dv_xname, where, asr);
			GET_SBIC_asr(regs, asr);
			if( asr & SBIC_ASR_DBR ) /* Not the read direction, then */
				SET_SBIC_data(regs, asr);
			GET_SBIC_asr(regs, asr);
		}
		WAIT_CIP(regs);
printf("%s: sbicabort - sending ABORT command\n", dev->sc_dev.dv_xname);
		SET_SBIC_cmd(regs, SBIC_CMD_ABORT);
		WAIT_CIP(regs);

		GET_SBIC_asr(regs, asr);
		if (asr & (SBIC_ASR_BSY|SBIC_ASR_LCI)) {
			/* ok, get more drastic.. */

printf("%s: sbicabort - asr %x, trying to reset\n", dev->sc_dev.dv_xname, asr);
			sbicreset(dev);
			dev->sc_flags &= ~SBICF_SELECTED;
			return -1;
		}
printf("%s: sbicabort - sending DISC command\n", dev->sc_dev.dv_xname);
		SET_SBIC_cmd(regs, SBIC_CMD_DISC);

		do {
			asr = SBIC_WAIT (regs, SBIC_ASR_INT, 0);
			GET_SBIC_csr (regs, csr);
			CSR_TRACE('a',csr,asr,0);
		} while ((csr != SBIC_CSR_DISC) && (csr != SBIC_CSR_DISC_1)
		    && (csr != SBIC_CSR_CMD_INVALID));

		/* lets just hope it worked.. */
		dev->sc_flags &= ~SBICF_SELECTED;
	}
	return -1;
}


/*
 * Initialize driver-private structures
 */

void
sbicinit(dev)
	struct sbic_softc *dev;
{
	sbic_regmap_p regs;
	u_int my_id, i, s;
	u_char csr;
	struct sbic_acb *acb;
	u_int inhibit_sync;

	extern u_long scsi_nosync;
	extern int shift_nosync;

	regs = dev->sc_sbicp;

	if ((dev->sc_flags & SBICF_ALIVE) == 0) {
		TAILQ_INIT(&dev->ready_list);
		TAILQ_INIT(&dev->nexus_list);
		TAILQ_INIT(&dev->free_list);
		dev->sc_nexus = NULL;
		dev->sc_xs = NULL;
		acb = dev->sc_acb;
		bzero(acb, sizeof(dev->sc_acb));
		for (i = 0; i < sizeof(dev->sc_acb) / sizeof(*acb); i++) {
			TAILQ_INSERT_TAIL(&dev->free_list, acb, chain);
			acb++;
		}
		bzero(dev->sc_tinfo, sizeof(dev->sc_tinfo));
#ifdef DEBUG
		/* make sure timeout is really not needed */
		timeout((void *)sbictimeout, dev, 30 * hz);
#endif

	} else panic("sbic: reinitializing driver!");

	dev->sc_flags |= SBICF_ALIVE;
	dev->sc_flags &= ~SBICF_SELECTED;

	/* initialize inhibit array */
	if (scsi_nosync) {
		inhibit_sync = (scsi_nosync >> shift_nosync) & 0xff;
		shift_nosync += 8;
#ifdef DEBUG
		if (inhibit_sync)
			printf("%s: Inhibiting synchronous transfer %02x\n",
				dev->sc_dev.dv_xname, inhibit_sync);
#endif
		for (i = 0; i < 8; ++i)
			if (inhibit_sync & (1 << i))
				sbic_inhibit_sync[i] = 1;
	}

	sbicreset(dev);
}

void
sbicreset(dev)
	struct sbic_softc *dev;
{
	sbic_regmap_p regs;
	u_int my_id, i, s;
	u_char csr;
	struct sbic_acb *acb;

	regs = dev->sc_sbicp;
#if 0
	if (dev->sc_flags & SBICF_ALIVE) {
		SET_SBIC_cmd(regs, SBIC_CMD_ABORT);
		WAIT_CIP(regs);
	}
#else
		SET_SBIC_cmd(regs, SBIC_CMD_ABORT);
		WAIT_CIP(regs);
#endif
	s = splbio();
	my_id = dev->sc_link.adapter_target & SBIC_ID_MASK;

	/* Enable advanced mode */
	my_id |= SBIC_ID_EAF /*| SBIC_ID_EHP*/ ;
	SET_SBIC_myid(regs, my_id);

	/*
	 * Disable interrupts (in dmainit) then reset the chip
	 */
	SET_SBIC_cmd(regs, SBIC_CMD_RESET);
	DELAY(25);
	SBIC_WAIT(regs, SBIC_ASR_INT, 0);
	GET_SBIC_csr(regs, csr);       /* clears interrupt also */

	if (dev->sc_clkfreq < 110)
		my_id |= SBIC_ID_FS_8_10;
	else if (dev->sc_clkfreq < 160)
		my_id |= SBIC_ID_FS_12_15;
	else if (dev->sc_clkfreq < 210)
		my_id |= SBIC_ID_FS_16_20;

	SET_SBIC_myid(regs, my_id);

	/*
	 * Set up various chip parameters
	 */
	SET_SBIC_control(regs, SBIC_CTL_EDI | SBIC_CTL_IDI /* | SBIC_CTL_HSP */
	    | SBIC_MACHINE_DMA_MODE);
	/*
	 * don't allow (re)selection (SBIC_RID_ES)
	 * until we can handle target mode!!
	 */
	SET_SBIC_rselid(regs, SBIC_RID_ER);
	SET_SBIC_syn(regs, 0);     /* asynch for now */

	/*
	 * anything else was zeroed by reset
	 */
	splx(s);

#if 0
	if ((dev->sc_flags & SBICF_ALIVE) == 0) {
		TAILQ_INIT(&dev->ready_list);
		TAILQ_INIT(&dev->nexus_list);
		TAILQ_INIT(&dev->free_list);
		dev->sc_nexus = NULL;
		dev->sc_xs = NULL;
		acb = dev->sc_acb;
		bzero(acb, sizeof(dev->sc_acb));
		for (i = 0; i < sizeof(dev->sc_acb) / sizeof(*acb); i++) {
			TAILQ_INSERT_TAIL(&dev->free_list, acb, chain);
			acb++;
		}
		bzero(dev->sc_tinfo, sizeof(dev->sc_tinfo));
	} else {
		if (dev->sc_nexus != NULL) {
			dev->sc_nexus->xs->error = XS_DRIVER_STUFFUP;
			sbic_scsidone(dev->sc_nexus, dev->sc_stat[0]);
		}
		while (acb = dev->nexus_list.tqh_first) {
			acb->xs->error = XS_DRIVER_STUFFUP;
			sbic_scsidone(acb, -1 /*acb->stat[0]*/);
		}
	}

	dev->sc_flags |= SBICF_ALIVE;
#endif
	dev->sc_flags &= ~SBICF_SELECTED;
}

void
sbicerror(dev, regs, csr)
	struct sbic_softc *dev;
	sbic_regmap_p regs;
	u_char csr;
{
	struct scsi_xfer *xs;

	xs = dev->sc_xs;

#ifdef DIAGNOSTIC
	if (xs == NULL)
		panic("sbicerror");
#endif
	if (xs->flags & SCSI_SILENT)
		return;

	printf("%s: ", dev->sc_dev.dv_xname);
	printf("csr == 0x%02x\n", csr);	/* XXX */
}

/*
 * select the bus, return when selected or error.
 */
int
sbicselectbus(dev, regs, target, lun, our_addr)
        struct sbic_softc *dev;
	sbic_regmap_p regs;
	u_char target, lun, our_addr;
{
	u_char asr, csr, id;

	SBIC_TRACE(dev);
	QPRINTF(("sbicselectbus %d\n", target));

	/*
	 * if we're already selected, return (XXXX panic maybe?)
	 */
	if (dev->sc_flags & SBICF_SELECTED) {
		SBIC_TRACE(dev);
		return(1);
	}

	/*
	 * issue select
	 */
	SBIC_TC_PUT(regs, 0);
	SET_SBIC_selid(regs, target);
	SET_SBIC_timeo(regs, SBIC_TIMEOUT(250,dev->sc_clkfreq));

	/*
	 * set sync or async
	 */
	if (dev->sc_sync[target].state == SYNC_DONE)
		SET_SBIC_syn(regs, SBIC_SYN (dev->sc_sync[target].offset,
		    dev->sc_sync[target].period));
	else
		SET_SBIC_syn(regs, SBIC_SYN (0, sbic_min_period));

	GET_SBIC_asr(regs, asr);
	if( asr & (SBIC_ASR_INT|SBIC_ASR_BSY) ) {
		/* This means we got ourselves reselected upon */
/*		printf("sbicselectbus: INT/BSY asr %02x\n", asr);*/
#ifdef DDB
/*		Debugger();*/
#endif
		SBIC_TRACE(dev);
		return 1;
	}

	SET_SBIC_cmd(regs, SBIC_CMD_SEL_ATN);

	/*
	 * wait for select (merged from seperate function may need
	 * cleanup)
	 */
	WAIT_CIP(regs);
	do {
		asr = SBIC_WAIT(regs, SBIC_ASR_INT | SBIC_ASR_LCI, 0);
		if (asr & SBIC_ASR_LCI) {
#ifdef DEBUG
			if (reselect_debug)
				printf("sbicselectbus: late LCI asr %02x\n", asr);
#endif
			SBIC_TRACE(dev);
			return 1;
		}
		GET_SBIC_csr (regs, csr);
		CSR_TRACE('s',csr,asr,target);
		QPRINTF(("%02x ", csr));
		if( csr == SBIC_CSR_RSLT_NI || csr == SBIC_CSR_RSLT_IFY) {
#ifdef DEBUG
			if(reselect_debug)
				printf("sbicselectbus: reselected asr %02x\n", asr);
#endif
			/* We need to handle this now so we don't lock up later */
			sbicnextstate(dev, csr, asr);
			SBIC_TRACE(dev);
			return 1;
		}
		if( csr == SBIC_CSR_SLT || csr == SBIC_CSR_SLT_ATN) {
			panic("sbicselectbus: target issued select!");
			return 1;
		}
	} while (csr != (SBIC_CSR_MIS_2|MESG_OUT_PHASE)
	    && csr != (SBIC_CSR_MIS_2|CMD_PHASE) && csr != SBIC_CSR_SEL_TIMEO);

	/* Enable (or not) reselection */
	if(!sbic_enable_reselect && dev->nexus_list.tqh_first == NULL)
		SET_SBIC_rselid (regs, 0);
	else
		SET_SBIC_rselid (regs, SBIC_RID_ER);

	if (csr == (SBIC_CSR_MIS_2|CMD_PHASE)) {
		dev->sc_flags |= SBICF_SELECTED;	/* device ignored ATN */
		GET_SBIC_selid(regs, id);
		dev->target = id;
		GET_SBIC_tlun(regs,dev->lun);
		if( dev->lun & SBIC_TLUN_VALID )
			dev->lun &= SBIC_TLUN_MASK;
		else
			dev->lun = lun;
	} else if (csr == (SBIC_CSR_MIS_2|MESG_OUT_PHASE)) {
		/*
		 * Send identify message
		 * (SCSI-2 requires an identify msg (?))
		 */
		GET_SBIC_selid(regs, id);
		dev->target = id;
		GET_SBIC_tlun(regs,dev->lun);
		if( dev->lun & SBIC_TLUN_VALID )
			dev->lun &= SBIC_TLUN_MASK;
		else
			dev->lun = lun;
		/*
		 * handle drives that don't want to be asked
		 * whether to go sync at all.
		 */
		if (sbic_inhibit_sync[id]
		    && dev->sc_sync[id].state == SYNC_START) {
#ifdef DEBUG
			if (sync_debug)
				printf("Forcing target %d asynchronous.\n", id);
#endif
			dev->sc_sync[id].offset = 0;
			dev->sc_sync[id].period = sbic_min_period;
			dev->sc_sync[id].state = SYNC_DONE;
		}


		if (dev->sc_sync[id].state != SYNC_START){
			if( dev->sc_xs->flags & SCSI_POLL
			   || (dev->sc_flags & SBICF_ICMD)
			   || !sbic_enable_reselect )
				SEND_BYTE (regs, MSG_IDENTIFY | lun);
			else
				SEND_BYTE (regs, MSG_IDENTIFY_DR | lun);
		} else {
			/*
			 * try to initiate a sync transfer.
			 * So compose the sync message we're going
			 * to send to the target
			 */

#ifdef DEBUG
			if (sync_debug)
				printf("Sending sync request to target %d ... ",
				    id);
#endif
			/*
			 * setup scsi message sync message request
			 */
			dev->sc_msg[0] = MSG_IDENTIFY | lun;
			dev->sc_msg[1] = MSG_EXT_MESSAGE;
			dev->sc_msg[2] = 3;
			dev->sc_msg[3] = MSG_SYNC_REQ;
			dev->sc_msg[4] = sbictoscsiperiod(dev, regs,
			    sbic_min_period);
			dev->sc_msg[5] = sbic_max_offset;

			if (sbicxfstart(regs, 6, MESG_OUT_PHASE, sbic_cmd_wait))
				sbicxfout(regs, 6, dev->sc_msg, MESG_OUT_PHASE);

			dev->sc_sync[id].state = SYNC_SENT;
#ifdef DEBUG
			if (sync_debug)
				printf ("sent\n");
#endif
		}

		asr = SBIC_WAIT (regs, SBIC_ASR_INT, 0);
		GET_SBIC_csr (regs, csr);
		CSR_TRACE('y',csr,asr,target);
		QPRINTF(("[%02x]", csr));
#ifdef DEBUG
		if (sync_debug && dev->sc_sync[id].state == SYNC_SENT)
			printf("csr-result of last msgout: 0x%x\n", csr);
#endif

		if (csr != SBIC_CSR_SEL_TIMEO)
			dev->sc_flags |= SBICF_SELECTED;
	}
	if (csr == SBIC_CSR_SEL_TIMEO)
		dev->sc_xs->error = XS_SELTIMEOUT;

	QPRINTF(("\n"));

	SBIC_TRACE(dev);
	return(csr == SBIC_CSR_SEL_TIMEO);
}

int
sbicxfstart(regs, len, phase, wait)
	sbic_regmap_p regs;
	int len, wait;
	u_char phase;
{
	u_char id;

	switch (phase) {
	case DATA_IN_PHASE:
	case MESG_IN_PHASE:
		GET_SBIC_selid (regs, id);
		id |= SBIC_SID_FROM_SCSI;
		SET_SBIC_selid (regs, id);
		SBIC_TC_PUT (regs, (unsigned)len);
		break;
	case DATA_OUT_PHASE:
	case MESG_OUT_PHASE:
	case CMD_PHASE:
		GET_SBIC_selid (regs, id);
		id &= ~SBIC_SID_FROM_SCSI;
		SET_SBIC_selid (regs, id);
		SBIC_TC_PUT (regs, (unsigned)len);
		break;
	default:
		SBIC_TC_PUT (regs, 0);
	}
	QPRINTF(("sbicxfstart %d, %d, %d\n", len, phase, wait));

	return(1);
}

int
sbicxfout(regs, len, bp, phase)
	sbic_regmap_p regs;
	int len;
	void *bp;
	int phase;
{
	u_char orig_csr, csr, asr, *buf;
	int wait;

	buf = bp;
	wait = sbic_data_wait;

	QPRINTF(("sbicxfout {%d} %02x %02x %02x %02x %02x "
	    "%02x %02x %02x %02x %02x\n", len, buf[0], buf[1], buf[2],
	    buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9]));

	GET_SBIC_csr (regs, orig_csr);
	CSR_TRACE('>',orig_csr,0,0);

	/*
	 * sigh.. WD-PROTO strikes again.. sending the command in one go
	 * causes the chip to lock up if talking to certain (misbehaving?)
	 * targets. Anyway, this procedure should work for all targets, but
	 * it's slightly slower due to the overhead
	 */
	WAIT_CIP (regs);
	SET_SBIC_cmd (regs, SBIC_CMD_XFER_INFO);
	for (;len > 0; len--) {
		GET_SBIC_asr (regs, asr);
		while ((asr & SBIC_ASR_DBR) == 0) {
			if ((asr & SBIC_ASR_INT) || --wait < 0) {
#ifdef DEBUG
				if (sbic_debug)
					printf("sbicxfout fail: l%d i%x w%d\n",
					    len, asr, wait);
#endif
				return (len);
			}
/*			DELAY(1);*/
			GET_SBIC_asr (regs, asr);
		}

		SET_SBIC_data (regs, *buf);
		buf++;
	}
	SBIC_TC_GET(regs, len);
	QPRINTF(("sbicxfout done %d bytes\n", len));
	/*
	 * this leaves with one csr to be read
	 */
	return(0);
}

/* returns # bytes left to read */
int
sbicxfin(regs, len, bp)
	sbic_regmap_p regs;
	int len;
	void *bp;
{
	int wait, read;
	u_char *obp, *buf;
	u_char orig_csr, csr, asr;

	wait = sbic_data_wait;
	obp = bp;
	buf = bp;

	GET_SBIC_csr (regs, orig_csr);
	CSR_TRACE('<',orig_csr,0,0);

	QPRINTF(("sbicxfin %d, csr=%02x\n", len, orig_csr));

	WAIT_CIP (regs);
	SET_SBIC_cmd (regs, SBIC_CMD_XFER_INFO);
	for (;len > 0; len--) {
		GET_SBIC_asr (regs, asr);
		if((asr & SBIC_ASR_PE)) {
#ifdef DEBUG
			printf("sbicxfin parity error: l%d i%x w%d\n",
			       len, asr, wait);
/*			return ((unsigned long)buf - (unsigned long)bp); */
#ifdef DDB
			Debugger();
#endif
#endif
		}
		while ((asr & SBIC_ASR_DBR) == 0) {
			if ((asr & SBIC_ASR_INT) || --wait < 0) {
#ifdef DEBUG
				if (sbic_debug) {
	QPRINTF(("sbicxfin fail:{%d} %02x %02x %02x %02x %02x %02x "
	    "%02x %02x %02x %02x\n", len, obp[0], obp[1], obp[2],
	    obp[3], obp[4], obp[5], obp[6], obp[7], obp[8], obp[9]));
					printf("sbicxfin fail: l%d i%x w%d\n",
					    len, asr, wait);
}
#endif
				return len;
			}

			if( ! asr & SBIC_ASR_BSY ) {
				GET_SBIC_csr(regs, csr);
				CSR_TRACE('<',csr,asr,len);
				QPRINTF(("[CSR%02xASR%02x]", csr, asr));
			}

/*			DELAY(1);*/
			GET_SBIC_asr (regs, asr);
		}

		GET_SBIC_data (regs, *buf);
/*		QPRINTF(("asr=%02x, csr=%02x, data=%02x\n", asr, csr, *buf));*/
		buf++;
	}

	QPRINTF(("sbicxfin {%d} %02x %02x %02x %02x %02x %02x "
	    "%02x %02x %02x %02x\n", len, obp[0], obp[1], obp[2],
	    obp[3], obp[4], obp[5], obp[6], obp[7], obp[8], obp[9]));

	/* this leaves with one csr to be read */
	return len;
}

/*
 * SCSI 'immediate' command:  issue a command to some SCSI device
 * and get back an 'immediate' response (i.e., do programmed xfer
 * to get the response data).  'cbuf' is a buffer containing a scsi
 * command of length clen bytes.  'buf' is a buffer of length 'len'
 * bytes for data.  The transfer direction is determined by the device
 * (i.e., by the scsi bus data xfer phase).  If 'len' is zero, the
 * command must supply no data.
 */
int
sbicicmd(dev, target, lun, cbuf, clen, buf, len)
	struct sbic_softc *dev;
	void *cbuf, *buf;
	int clen, len;
{
	sbic_regmap_p regs;
	u_char phase, csr, asr;
	int wait, newtarget, cmd_sent, parity_err;
	struct sbic_acb *acb;

	int discon;
	int i;

#define CSR_LOG_BUF_SIZE 0
#if CSR_LOG_BUF_SIZE
	int bufptr;
	int csrbuf[CSR_LOG_BUF_SIZE];
	bufptr=0;
#endif

	SBIC_TRACE(dev);
	regs = dev->sc_sbicp;
	acb = dev->sc_nexus;

	/* Make sure pointers are OK */
	dev->sc_last = dev->sc_cur = &acb->sc_pa;
	dev->sc_tcnt = acb->sc_tcnt = 0;
	acb->sc_pa.dc_count = 0; /* No DMA */
	acb->sc_kv.dc_addr = buf;
	acb->sc_kv.dc_count = len;

#ifdef DEBUG
	routine = 3;
	debug_sbic_regs = regs; /* store this to allow debug calls */
	if( data_pointer_debug > 1 )
		printf("sbicicmd(%d,%d):%d\n", target, lun,
		       acb->sc_kv.dc_count);
#endif

	/*
	 * set the sbic into non-DMA mode
	 */
	SET_SBIC_control(regs, SBIC_CTL_EDI | SBIC_CTL_IDI /*| SBIC_CTL_HSP*/);

	dev->sc_stat[0] = 0xff;
	dev->sc_msg[0] = 0xff;
	i = 1; /* pre-load */

	/* We're stealing the SCSI bus */
	dev->sc_flags |= SBICF_ICMD;

	do {
		/*
		 * select the SCSI bus (it's an error if bus isn't free)
		 */
		if (!( dev->sc_flags & SBICF_SELECTED )
		    && sbicselectbus(dev, regs, target, lun, dev->sc_scsiaddr)) {
			/*printf("sbicicmd trying to select busy bus!\n");*/
			dev->sc_flags &= ~SBICF_ICMD;
			return(-1);
		}

		/*
		 * Wait for a phase change (or error) then let the device sequence
		 * us through the various SCSI phases.
		 */

		wait = sbic_cmd_wait;

		asr = GET_SBIC_asr (regs, asr);
		GET_SBIC_csr (regs, csr);
		CSR_TRACE('I',csr,asr,target);
		QPRINTF((">ASR:%02xCSR:%02x<", asr, csr));

#if CSR_LOG_BUF_SIZE
		csrbuf[bufptr++] = csr;
#endif


		switch (csr) {
		case SBIC_CSR_S_XFERRED:
		case SBIC_CSR_DISC:
		case SBIC_CSR_DISC_1:
			dev->sc_flags &= ~SBICF_SELECTED;
			GET_SBIC_cmd_phase (regs, phase);
			if (phase == 0x60) {
				GET_SBIC_tlun (regs, dev->sc_stat[0]);
				i = 0; /* done */
/*				break; /* Bypass all the state gobldygook */
			} else {
#ifdef DEBUG
				if(reselect_debug>1)
					printf("sbicicmd: handling disconnect\n");
#endif
				i = SBIC_STATE_DISCONNECT;
			}
			break;

		case SBIC_CSR_XFERRED|CMD_PHASE:
		case SBIC_CSR_MIS|CMD_PHASE:
		case SBIC_CSR_MIS_1|CMD_PHASE:
		case SBIC_CSR_MIS_2|CMD_PHASE:
			if (sbicxfstart(regs, clen, CMD_PHASE, sbic_cmd_wait))
				if (sbicxfout(regs, clen,
					      cbuf, CMD_PHASE))
					i = sbicabort(dev, regs,"icmd sending cmd");
#if 0
			GET_SBIC_csr(regs, csr); /* Lets us reload tcount */
			WAIT_CIP(regs);
			GET_SBIC_asr(regs, asr);
			CSR_TRACE('I',csr,asr,target);
			if( asr & (SBIC_ASR_BSY|SBIC_ASR_LCI|SBIC_ASR_CIP) )
				printf("next: cmd sent asr %02x, csr %02x\n",
				       asr, csr);
#endif
			break;

#if 0
		case SBIC_CSR_XFERRED|DATA_OUT_PHASE:
		case SBIC_CSR_XFERRED|DATA_IN_PHASE:
		case SBIC_CSR_MIS|DATA_OUT_PHASE:
		case SBIC_CSR_MIS|DATA_IN_PHASE:
		case SBIC_CSR_MIS_1|DATA_OUT_PHASE:
		case SBIC_CSR_MIS_1|DATA_IN_PHASE:
		case SBIC_CSR_MIS_2|DATA_OUT_PHASE:
		case SBIC_CSR_MIS_2|DATA_IN_PHASE:
			if (acb->sc_kv.dc_count <= 0)
				i = sbicabort(dev, regs, "icmd out of data");
			else {
			  wait = sbic_data_wait;
			  if (sbicxfstart(regs,
					  acb->sc_kv.dc_count,
					  SBIC_PHASE(csr), wait))
			    if (csr & 0x01)
			      /* data in? */
			      i=sbicxfin(regs,
					 acb->sc_kv.dc_count,
					 acb->sc_kv.dc_addr);
			    else
			      i=sbicxfout(regs,
					  acb->sc_kv.dc_count,
					  acb->sc_kv.dc_addr,
					     SBIC_PHASE(csr));
			  acb->sc_kv.dc_addr +=
				  (acb->sc_kv.dc_count - i);
			  acb->sc_kv.dc_count = i;
			  i = 1;
			}
			break;

#endif
		case SBIC_CSR_XFERRED|STATUS_PHASE:
		case SBIC_CSR_MIS|STATUS_PHASE:
		case SBIC_CSR_MIS_1|STATUS_PHASE:
		case SBIC_CSR_MIS_2|STATUS_PHASE:
			/*
			 * the sbic does the status/cmd-complete reading ok,
			 * so do this with its hi-level commands.
			 */
#ifdef DEBUG
			if(sbic_debug)
				printf("SBICICMD status phase\n");
#endif
			SBIC_TC_PUT(regs, 0);
			SET_SBIC_cmd_phase(regs, 0x46);
			SET_SBIC_cmd(regs, SBIC_CMD_SEL_ATN_XFER);
			break;

#if THIS_IS_A_RESERVED_STATE
		case BUS_FREE_PHASE:		/* This is not legal */
			if( dev->sc_stat[0] != 0xff )
				goto out;
			break;
#endif

		default:
			i = sbicnextstate(dev, csr, asr);
		}

		/*
		 * make sure the last command was taken,
		 * ie. we're not hunting after an ignored command..
		 */
		GET_SBIC_asr(regs, asr);

		/* tapes may take a loooong time.. */
		while (asr & SBIC_ASR_BSY){
			if(asr & SBIC_ASR_DBR) {
				printf("sbicicmd: Waiting while sbic is jammed, CSR:%02x,ASR:%02x\n",
				       csr,asr);
#ifdef DDB
				Debugger();
#endif
				/* SBIC is jammed */
				/* DUNNO which direction */
				/* Try old direction */
				GET_SBIC_data(regs,i);
				GET_SBIC_asr(regs, asr);
				if( asr & SBIC_ASR_DBR) /* Wants us to write */
					SET_SBIC_data(regs,i);
			}
			GET_SBIC_asr(regs, asr);
		}

		/*
		 * wait for last command to complete
		 */
		if (asr & SBIC_ASR_LCI) {
			printf("sbicicmd: last command ignored\n");
		}
		else if( i == 1 ) /* Bsy */
			SBIC_WAIT (regs, SBIC_ASR_INT, wait);

		/*
		 * do it again
		 */
	} while ( i > 0 && dev->sc_stat[0] == 0xff);

	/* Sometimes we need to do an extra read of the CSR */
	GET_SBIC_csr(regs, csr);
	CSR_TRACE('I',csr,asr,0xff);

#if CSR_LOG_BUF_SIZE
	if(reselect_debug>1)
		for(i=0; i<bufptr; i++)
			printf("CSR:%02x", csrbuf[i]);
#endif

#ifdef DEBUG
	if(data_pointer_debug > 1)
		printf("sbicicmd done(%d,%d):%d =%d=\n",
		       dev->target, lun,
		       acb->sc_kv.dc_count,
		       dev->sc_stat[0]);
#endif

	QPRINTF(("=STS:%02x=", dev->sc_stat[0]));
	dev->sc_flags &= ~SBICF_ICMD;

	SBIC_TRACE(dev);
	return(dev->sc_stat[0]);
}

/*
 * Finish SCSI xfer command:  After the completion interrupt from
 * a read/write operation, sequence through the final phases in
 * programmed i/o.  This routine is a lot like sbicicmd except we
 * skip (and don't allow) the select, cmd out and data in/out phases.
 */
void
sbicxfdone(dev, regs, target)
	struct sbic_softc *dev;
	sbic_regmap_p regs;
	int target;
{
	u_char phase, asr, csr;
	int s;

	SBIC_TRACE(dev);
	QPRINTF(("{"));
	s = splbio();

	/*
	 * have the sbic complete on its own
	 */
	SBIC_TC_PUT(regs, 0);
	SET_SBIC_cmd_phase(regs, 0x46);
	SET_SBIC_cmd(regs, SBIC_CMD_SEL_ATN_XFER);

	do {
		asr = SBIC_WAIT (regs, SBIC_ASR_INT, 0);
		GET_SBIC_csr (regs, csr);
		CSR_TRACE('f',csr,asr,target);
		QPRINTF(("%02x:", csr));
	} while ((csr != SBIC_CSR_DISC) && (csr != SBIC_CSR_DISC_1)
	    && (csr != SBIC_CSR_S_XFERRED));

	dev->sc_flags &= ~SBICF_SELECTED;

	GET_SBIC_cmd_phase (regs, phase);
	QPRINTF(("}%02x", phase));
	if (phase == 0x60)
		GET_SBIC_tlun(regs, dev->sc_stat[0]);
	else
		sbicerror(dev, regs, csr);

	QPRINTF(("=STS:%02x=\n", dev->sc_stat[0]));
	splx(s);
	SBIC_TRACE(dev);
}

	/*
	 * No DMA chains
	 */

int
sbicgo(dev, xs)
	struct sbic_softc *dev;
	struct scsi_xfer *xs;
{
	int i, dmaflags, count, wait, usedma;
	u_char csr, asr, cmd, *addr;
	sbic_regmap_p regs;
	struct sbic_acb *acb;

	SBIC_TRACE(dev);
	dev->target = xs->sc_link->target;
	dev->lun = xs->sc_link->lun;
	acb = dev->sc_nexus;
	regs = dev->sc_sbicp;

	usedma = sbicdmaok(dev, xs);
#ifdef DEBUG
	routine = 1;
	debug_sbic_regs = regs; /* store this to allow debug calls */
	if( data_pointer_debug > 1 )
		printf("sbicgo(%d,%d)\n", dev->target, dev->lun);
#endif

	/*
	 * set the sbic into DMA mode
	 */
	if( usedma )
		SET_SBIC_control(regs, SBIC_CTL_EDI | SBIC_CTL_IDI |
				 SBIC_MACHINE_DMA_MODE);
	else
		SET_SBIC_control(regs, SBIC_CTL_EDI | SBIC_CTL_IDI);


	/*
	 * select the SCSI bus (it's an error if bus isn't free)
	 */
	if (sbicselectbus(dev, regs, dev->target, dev->lun,
	    dev->sc_scsiaddr)) {
/*		printf("sbicgo: Trying to select busy bus!\n"); */
		SBIC_TRACE(dev);
		return(0); /* Not done: needs to be rescheduled */
	}
	dev->sc_stat[0] = 0xff;

	/*
	 * Calculate DMA chains now
	 */

	dmaflags = 0;
	if (acb->flags & ACB_DATAIN)
		dmaflags |= DMAGO_READ;


	/*
	 * Deal w/bounce buffers.
	 */

	addr = acb->sc_kv.dc_addr;
	count = acb->sc_kv.dc_count;
	if (count && (char *)kvtop(addr) != acb->sc_pa.dc_addr)	{ /* XXXX check */
		printf("sbic: DMA buffer mapping changed %x->%x\n",
		    acb->sc_pa.dc_addr, kvtop(addr));
#ifdef DDB
		Debugger();
#endif
	}

#ifdef DEBUG
	++sbicdma_ops;			/* count total DMA operations */
#endif
	if (count && usedma && dev->sc_flags & SBICF_BADDMA &&
	    sbiccheckdmap(addr, count, dev->sc_dmamask)) {
		/*
		 * need to bounce the dma.
		 */
		if (dmaflags & DMAGO_READ) {
			acb->flags |= ACB_BBUF;
			acb->sc_dmausrbuf = addr;
			acb->sc_dmausrlen = count;
			acb->sc_usrbufpa = (u_char *)kvtop(addr);
			if(!dev->sc_tinfo[dev->target].bounce) {
				printf("sbicgo: HELP! no bounce allocated for %d\n",
				       dev->target);
				printf("xfer: (%x->%x,%x)\n", acb->sc_dmausrbuf,
				       acb->sc_usrbufpa, acb->sc_dmausrlen);
				dev->sc_tinfo[xs->sc_link->target].bounce
					= (char *)alloc_z2mem(MAXPHYS);
				if (isztwomem(dev->sc_tinfo[xs->sc_link->target].bounce))
					printf("alloc ZII target %d bounce pa 0x%x\n",
					       xs->sc_link->target,
					       kvtop(dev->sc_tinfo[xs->sc_link->target].bounce));
				else if (dev->sc_tinfo[xs->sc_link->target].bounce)
					printf("alloc CHIP target %d bounce pa 0x%x\n",
					       xs->sc_link->target,
					       PREP_DMA_MEM(dev->sc_tinfo[xs->sc_link->target].bounce));

				printf("Allocating %d bounce at %x\n",
				       dev->target,
				       kvtop(dev->sc_tinfo[dev->target].bounce));
			}
		} else {	/* write: copy to dma buffer */
#ifdef DEBUG
			if(data_pointer_debug)
			printf("sbicgo: copying %x bytes to target %d bounce %x\n",
			       count, dev->target,
			       kvtop(dev->sc_tinfo[dev->target].bounce));
#endif
			bcopy (addr, dev->sc_tinfo[dev->target].bounce, count);
		}
		addr = dev->sc_tinfo[dev->target].bounce;/* and use dma buffer */
		acb->sc_kv.dc_addr = addr;
#ifdef DEBUG
		++sbicdma_bounces;		/* count number of bounced */
#endif
	}

	/*
	 * Allocate the DMA chain
	 */

	/* Set start KVM addresses */
#if 0
	acb->sc_kv.dc_addr = addr;
	acb->sc_kv.dc_count = count;
#endif

	/* Mark end of segment */
	acb->sc_tcnt = dev->sc_tcnt = 0;
	acb->sc_pa.dc_count = 0;

	sbic_load_ptrs(dev, regs, dev->target, dev->lun);
	SBIC_TRACE(dev);
	/* Enable interrupts but don't do any DMA */
	dev->sc_enintr(dev);
	if (usedma) {
		dev->sc_tcnt = dev->sc_dmago(dev, acb->sc_pa.dc_addr,
		    acb->sc_pa.dc_count,
		    dmaflags);
#ifdef DEBUG
		dev->sc_dmatimo = dev->sc_tcnt ? 1 : 0;
#endif
        } else
		dev->sc_dmacmd = 0; /* Don't use DMA */
	dev->sc_flags |= SBICF_INDMA;
/*	SBIC_TC_PUT(regs, dev->sc_tcnt); /* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
	SBIC_TRACE(dev);
	sbic_save_ptrs(dev, regs, dev->target, dev->lun);

	/*
	 * push the data cache ( I think this won't work (EH))
	 */
#if defined(M68040)
	if (mmutype == MMU_68040 && usedma && count) {
		dma_cachectl(addr, count);
		if (((u_int)addr & 0xF) || (((u_int)addr + count) & 0xF))
			dev->sc_flags |= SBICF_DCFLUSH;
	}
#endif

	/*
	 * enintr() also enables interrupts for the sbic
	 */
#ifdef DEBUG
	if( data_pointer_debug > 1 )
		printf("sbicgo dmago:%d(%x:%x)\n",
		       dev->target,dev->sc_cur->dc_addr,dev->sc_tcnt);
	debug_asr = asr;
	debug_csr = csr;
#endif

	/*
	 * Lets cycle a while then let the interrupt handler take over
	 */

	asr = GET_SBIC_asr(regs, asr);
	do {
		GET_SBIC_csr(regs, csr);
		CSR_TRACE('g',csr,asr,dev->target);
#ifdef DEBUG
		debug_csr = csr;
		routine = 1;
#endif
		QPRINTF(("go[0x%x]", csr));

		i = sbicnextstate(dev, csr, asr);

		WAIT_CIP(regs);
		GET_SBIC_asr(regs, asr);
#ifdef DEBUG
		debug_asr = asr;
#endif
		if(asr & SBIC_ASR_LCI) printf("sbicgo: LCI asr:%02x csr:%02x\n",
					      asr,csr);
	} while( i == SBIC_STATE_RUNNING
		&& asr & (SBIC_ASR_INT|SBIC_ASR_LCI) );

	CSR_TRACE('g',csr,asr,i<<4);
	SBIC_TRACE(dev);
if (i == SBIC_STATE_DONE && dev->sc_stat[0] == 0xff) printf("sbicgo: done & stat = 0xff\n");
	if (i == SBIC_STATE_DONE && dev->sc_stat[0] != 0xff) {
/*	if( i == SBIC_STATE_DONE && dev->sc_stat[0] ) { */
		/* Did we really finish that fast? */
		return 1;
	}
	return 0;
}


int
sbicintr(dev)
	struct sbic_softc *dev;
{
	sbic_regmap_p regs;
	struct dma_chain *df, *dl;
	u_char asr, csr, *tmpaddr;
	struct sbic_acb *acb;
	int i, newtarget, newlun;
	unsigned tcnt;

	regs = dev->sc_sbicp;

	/*
	 * pending interrupt?
	 */
	GET_SBIC_asr (regs, asr);
	if ((asr & SBIC_ASR_INT) == 0)
		return(0);

	SBIC_TRACE(dev);
	do {
		GET_SBIC_csr(regs, csr);
		CSR_TRACE('i',csr,asr,dev->target);
#ifdef DEBUG
		debug_csr = csr;
		routine = 2;
#endif
		QPRINTF(("intr[0x%x]", csr));

		i = sbicnextstate(dev, csr, asr);

		WAIT_CIP(regs);
		GET_SBIC_asr(regs, asr);
#ifdef DEBUG
		debug_asr = asr;
#endif
#if 0
		if(asr & SBIC_ASR_LCI) printf("sbicintr: LCI asr:%02x csr:%02x\n",
					      asr,csr);
#endif
	} while(i == SBIC_STATE_RUNNING &&
		asr & (SBIC_ASR_INT|SBIC_ASR_LCI));
	CSR_TRACE('i',csr,asr,i<<4);
	SBIC_TRACE(dev);
	return(1);
}

/*
 * Run commands and wait for disconnect
 */
int
sbicpoll(dev)
	struct sbic_softc *dev;
{
	sbic_regmap_p regs;
	u_char asr, csr;
	struct sbic_pending* pendp;
	int i;
	unsigned tcnt;

	SBIC_TRACE(dev);
	regs = dev->sc_sbicp;

	do {
		GET_SBIC_asr (regs, asr);
#ifdef DEBUG
		debug_asr = asr;
#endif
		GET_SBIC_csr(regs, csr);
		CSR_TRACE('p',csr,asr,dev->target);
#ifdef DEBUG
		debug_csr = csr;
		routine = 2;
#endif
		QPRINTF(("poll[0x%x]", csr));

		i = sbicnextstate(dev, csr, asr);

		WAIT_CIP(regs);
		GET_SBIC_asr(regs, asr);
		/* tapes may take a loooong time.. */
		while (asr & SBIC_ASR_BSY){
			if(asr & SBIC_ASR_DBR) {
				printf("sbipoll: Waiting while sbic is jammed, CSR:%02x,ASR:%02x\n",
				       csr,asr);
#ifdef DDB
				Debugger();
#endif
				/* SBIC is jammed */
				/* DUNNO which direction */
				/* Try old direction */
				GET_SBIC_data(regs,i);
				GET_SBIC_asr(regs, asr);
				if( asr & SBIC_ASR_DBR) /* Wants us to write */
					SET_SBIC_data(regs,i);
			}
			GET_SBIC_asr(regs, asr);
		}

		if(asr & SBIC_ASR_LCI) printf("sbicpoll: LCI asr:%02x csr:%02x\n",
					      asr,csr);
		else if( i == 1 ) /* BSY */
			SBIC_WAIT(regs, SBIC_ASR_INT, sbic_cmd_wait);
	} while(i == SBIC_STATE_RUNNING);
	CSR_TRACE('p',csr,asr,i<<4);
	SBIC_TRACE(dev);
	return(1);
}

/*
 * Handle a single msgin
 */

int
sbicmsgin(dev)
	struct sbic_softc *dev;
{
	sbic_regmap_p regs;
	int recvlen;
	u_char asr, csr, *tmpaddr;

	regs = dev->sc_sbicp;

	dev->sc_msg[0] = 0xff;
	dev->sc_msg[1] = 0xff;

	GET_SBIC_asr(regs, asr);
#ifdef DEBUG
	if(reselect_debug>1)
		printf("sbicmsgin asr=%02x\n", asr);
#endif

	sbic_save_ptrs(dev, regs, dev->target, dev->lun);

	GET_SBIC_selid (regs, csr);
	SET_SBIC_selid (regs, csr | SBIC_SID_FROM_SCSI);

	SBIC_TC_PUT(regs, 0);
	tmpaddr = dev->sc_msg;
	recvlen = 1;
	do {
		while( recvlen-- ) {
			asr = GET_SBIC_asr(regs, asr);
			GET_SBIC_csr(regs, csr);
			QPRINTF(("sbicmsgin ready to go (csr,asr)=(%02x,%02x)\n",
				 csr, asr));

			RECV_BYTE(regs, *tmpaddr);
			CSR_TRACE('m',csr,asr,*tmpaddr);
#if 1
			/*
			 * get the command completion interrupt, or we
			 * can't send a new command (LCI)
			 */
			SBIC_WAIT(regs, SBIC_ASR_INT, 0);
			GET_SBIC_csr(regs, csr);
			CSR_TRACE('X',csr,asr,dev->target);
#else
			WAIT_CIP(regs);
			do {
				GET_SBIC_asr(regs, asr);
				csr = 0xff;
				GET_SBIC_csr(regs, csr);
				CSR_TRACE('X',csr,asr,dev->target);
				if( csr == 0xff )
					printf("sbicmsgin waiting: csr %02x asr %02x\n", csr, asr);
			} while( csr == 0xff );
#endif
#ifdef DEBUG
			if(reselect_debug>1)
				printf("sbicmsgin: got %02x csr %02x asr %02x\n",
				       *tmpaddr, csr, asr);
#endif
#if do_parity_check
			if( asr & SBIC_ASR_PE ) {
				printf ("Parity error");
				/* This code simply does not work. */
				WAIT_CIP(regs);
				SET_SBIC_cmd(regs, SBIC_CMD_SET_ATN);
				WAIT_CIP(regs);
				GET_SBIC_asr(regs, asr);
				WAIT_CIP(regs);
				SET_SBIC_cmd(regs, SBIC_CMD_CLR_ACK);
				WAIT_CIP(regs);
				if( !(asr & SBIC_ASR_LCI) )
					/* Target wants to send garbled msg*/
					continue;
				printf("--fixing\n");
				/* loop until a msgout phase occurs on target */
				while(csr & 0x07 != MESG_OUT_PHASE) {
					while( asr & SBIC_ASR_BSY &&
					      !(asr & SBIC_ASR_DBR|SBIC_ASR_INT) )
						GET_SBIC_asr(regs, asr);
					if( asr & SBIC_ASR_DBR )
						panic("msgin: jammed again!\n");
					GET_SBIC_csr(regs, csr);
					CSR_TRACE('e',csr,asr,dev->target);
					if( csr & 0x07 != MESG_OUT_PHASE ) {
						sbicnextstate(dev, csr, asr);
						sbic_save_ptrs(dev, regs,
							       dev->target,
							       dev->lun);
					}
				}
				/* Should be msg out by now */
				SEND_BYTE(regs, MSG_PARITY_ERROR);
			}
			else
#endif
				tmpaddr++;

			if(recvlen) {
				/* Clear ACK */
				WAIT_CIP(regs);
				GET_SBIC_asr(regs, asr);
				GET_SBIC_csr(regs, csr);
				CSR_TRACE('X',csr,asr,dev->target);
				QPRINTF(("sbicmsgin pre byte CLR_ACK (csr,asr)=(%02x,%02x)\n",
					 csr, asr));
				SET_SBIC_cmd(regs, SBIC_CMD_CLR_ACK);
				SBIC_WAIT(regs, SBIC_ASR_INT, 0);
			}

		};

		if(dev->sc_msg[0] == 0xff) {
			printf("sbicmsgin: sbic swallowed our message\n");
			break;
		}
#ifdef DEBUG
		if (sync_debug)
			printf("msgin done csr 0x%x asr 0x%x msg 0x%x\n",
			       csr, asr, dev->sc_msg[0]);
#endif
		/*
		 * test whether this is a reply to our sync
		 * request
		 */
		if (MSG_ISIDENTIFY(dev->sc_msg[0])) {
			QPRINTF(("IFFY"));
#if 0
			/* There is an implied load-ptrs here */
			sbic_load_ptrs(dev, regs, dev->target, dev->lun);
#endif
			/* Got IFFY msg -- ack it */
		} else if (dev->sc_msg[0] == MSG_REJECT
			   && dev->sc_sync[dev->target].state == SYNC_SENT) {
			QPRINTF(("REJECT of SYN"));
#ifdef DEBUG
			if (sync_debug)
				printf("target %d rejected sync, going async\n",
				       dev->target);
#endif
			dev->sc_sync[dev->target].period = sbic_min_period;
			dev->sc_sync[dev->target].offset = 0;
			dev->sc_sync[dev->target].state = SYNC_DONE;
			SET_SBIC_syn(regs,
				     SBIC_SYN(dev->sc_sync[dev->target].offset,
					      dev->sc_sync[dev->target].period));
		} else if ((dev->sc_msg[0] == MSG_REJECT)) {
			QPRINTF(("REJECT"));
			/*
			 * we'll never REJECt a REJECT message..
			 */
		} else if ((dev->sc_msg[0] == MSG_SAVE_DATA_PTR)) {
			QPRINTF(("MSG_SAVE_DATA_PTR"));
			/*
			 * don't reject this either.
			 */
		} else if ((dev->sc_msg[0] == MSG_DISCONNECT)) {
			QPRINTF(("DISCONNECT"));
#ifdef DEBUG
			if( reselect_debug>1 && dev->sc_msg[0] == MSG_DISCONNECT )
				printf("sbicmsgin: got disconnect msg %s\n",
				       (dev->sc_flags & SBICF_ICMD)?"rejecting":"");
#endif
			if( dev->sc_flags & SBICF_ICMD ) {
				/* We're in immediate mode. Prevent disconnects. */
				/* prepare to reject the message, NACK */
				SET_SBIC_cmd(regs, SBIC_CMD_SET_ATN);
				WAIT_CIP(regs);
			}
		} else if (dev->sc_msg[0] == MSG_CMD_COMPLETE ) {
			QPRINTF(("CMD_COMPLETE"));
			/* !! KLUDGE ALERT !! quite a few drives don't seem to
			 * really like the current way of sending the
			 * sync-handshake together with the ident-message, and
			 * they react by sending command-complete and
			 * disconnecting right after returning the valid sync
			 * handshake. So, all I can do is reselect the drive,
			 * and hope it won't disconnect again. I don't think
			 * this is valid behavior, but I can't help fixing a
			 * problem that apparently exists.
			 *
			 * Note: we should not get here on `normal' command
			 * completion, as that condition is handled by the
			 * high-level sel&xfer resume command used to walk
			 * thru status/cc-phase.
			 */

#ifdef DEBUG
			if (sync_debug)
				printf ("GOT MSG %d! target %d acting weird.."
					" waiting for disconnect...\n",
					dev->sc_msg[0], dev->target);
#endif
			/* Check to see if sbic is handling this */
			GET_SBIC_asr(regs, asr);
			if(asr & SBIC_ASR_BSY)
				return SBIC_STATE_RUNNING;

			/* Let's try this: Assume it works and set status to 00 */
			dev->sc_stat[0] = 0;
		} else if (dev->sc_msg[0] == MSG_EXT_MESSAGE
			   && tmpaddr == &dev->sc_msg[1]) {
			QPRINTF(("ExtMSG\n"));
			/* Read in whole extended message */
			SET_SBIC_cmd(regs, SBIC_CMD_CLR_ACK);
			SBIC_WAIT(regs, SBIC_ASR_INT, 0);
			GET_SBIC_asr(regs, asr);
			GET_SBIC_csr(regs, csr);
			QPRINTF(("CLR ACK asr %02x, csr %02x\n", asr, csr));
			RECV_BYTE(regs, *tmpaddr);
			CSR_TRACE('x',csr,asr,*tmpaddr);
			/* Wait for command completion IRQ */
			SBIC_WAIT(regs, SBIC_ASR_INT, 0);
			recvlen = *tmpaddr++;
			QPRINTF(("Recving ext msg, asr %02x csr %02x len %02x\n",
			       asr, csr, recvlen));
		} else if (dev->sc_msg[0] == MSG_EXT_MESSAGE && dev->sc_msg[1] == 3
			   && dev->sc_msg[2] == MSG_SYNC_REQ) {
			QPRINTF(("SYN"));
			dev->sc_sync[dev->target].period =
				sbicfromscsiperiod(dev,
						   regs, dev->sc_msg[3]);
			dev->sc_sync[dev->target].offset = dev->sc_msg[4];
			dev->sc_sync[dev->target].state = SYNC_DONE;
			SET_SBIC_syn(regs,
				     SBIC_SYN(dev->sc_sync[dev->target].offset,
					      dev->sc_sync[dev->target].period));
			printf("%s: target %d now synchronous,"
			       " period=%dns, offset=%d.\n",
			       dev->sc_dev.dv_xname, dev->target,
			       dev->sc_msg[3] * 4, dev->sc_msg[4]);
		} else {
#ifdef DEBUG
			if (sbic_debug || sync_debug)
				printf ("sbicmsgin: Rejecting message 0x%02x\n",
					dev->sc_msg[0]);
#endif
			/* prepare to reject the message, NACK */
			SET_SBIC_cmd(regs, SBIC_CMD_SET_ATN);
			WAIT_CIP(regs);
		}
		/* Clear ACK */
		WAIT_CIP(regs);
		GET_SBIC_asr(regs, asr);
		GET_SBIC_csr(regs, csr);
		CSR_TRACE('X',csr,asr,dev->target);
		QPRINTF(("sbicmsgin pre CLR_ACK (csr,asr)=(%02x,%02x)%d\n",
			 csr, asr, recvlen));
		SET_SBIC_cmd(regs, SBIC_CMD_CLR_ACK);
		SBIC_WAIT(regs, SBIC_ASR_INT, 0);
	}
#if 0
	while((csr == SBIC_CSR_MSGIN_W_ACK)
	      || (SBIC_PHASE(csr) == MESG_IN_PHASE));
#else
	while (recvlen>0);
#endif

	QPRINTF(("sbicmsgin finished: csr %02x, asr %02x\n",csr, asr));

	/* Should still have one CSR to read */
	return SBIC_STATE_RUNNING;
}


/*
 * sbicnextstate()
 * return:
 *		0  == done
 *		1  == working
 *		2  == disconnected
 *		-1 == error
 */
int
sbicnextstate(dev, csr, asr)
	struct sbic_softc *dev;
	u_char csr, asr;
{
	sbic_regmap_p regs;
	struct dma_chain *df, *dl;
	struct sbic_acb *acb;
	int i, newtarget, newlun, wait;
	unsigned tcnt;

	SBIC_TRACE(dev);
	regs = dev->sc_sbicp;
	acb = dev->sc_nexus;

	QPRINTF(("next[%02x,%02x]",asr,csr));

	switch (csr) {
	case SBIC_CSR_XFERRED|CMD_PHASE:
	case SBIC_CSR_MIS|CMD_PHASE:
	case SBIC_CSR_MIS_1|CMD_PHASE:
	case SBIC_CSR_MIS_2|CMD_PHASE:
		sbic_save_ptrs(dev, regs, dev->target, dev->lun);
		if (sbicxfstart(regs, acb->clen, CMD_PHASE, sbic_cmd_wait))
			if (sbicxfout(regs, acb->clen,
				      &acb->cmd, CMD_PHASE))
				goto abort;
		break;

	case SBIC_CSR_XFERRED|STATUS_PHASE:
	case SBIC_CSR_MIS|STATUS_PHASE:
	case SBIC_CSR_MIS_1|STATUS_PHASE:
	case SBIC_CSR_MIS_2|STATUS_PHASE:
		/*
		 * this should be the normal i/o completion case.
		 * get the status & cmd complete msg then let the
		 * device driver look at what happened.
		 */
		sbicxfdone(dev,regs,dev->target);
		/*
		 * check for overlapping cache line, flush if so
		 */
#ifdef M68040
		if (dev->sc_flags & SBICF_DCFLUSH) {
#if 0
			printf("sbic: 68040 DMA cache flush needs fixing? %x:%x\n",
			    dev->sc_xs->data, dev->sc_xs->datalen);
#endif
		}
#endif
#ifdef DEBUG
		if( data_pointer_debug > 1 )
			printf("next dmastop: %d(%x:%x)\n",
			       dev->target,dev->sc_cur->dc_addr,dev->sc_tcnt);
		dev->sc_dmatimo = 0;
#endif
		dev->sc_dmastop(dev); /* was dmafree */
		if (acb->flags & ACB_BBUF) {
			if ((u_char *)kvtop(acb->sc_dmausrbuf) != acb->sc_usrbufpa)
				printf("%s: WARNING - buffer mapping changed %x->%x\n",
				    dev->sc_dev.dv_xname, acb->sc_usrbufpa,
				    kvtop(acb->sc_dmausrbuf));
#ifdef DEBUG
			if(data_pointer_debug)
			printf("sbicgo:copying %x bytes from target %d bounce %x\n",
			       acb->sc_dmausrlen,
			       dev->target,
			       kvtop(dev->sc_tinfo[dev->target].bounce));
#endif
			bcopy(dev->sc_tinfo[dev->target].bounce,
			      acb->sc_dmausrbuf,
			      acb->sc_dmausrlen);
		}
		dev->sc_flags &= ~(SBICF_INDMA | SBICF_DCFLUSH);
		sbic_scsidone(acb, dev->sc_stat[0]);
		SBIC_TRACE(dev);
		return SBIC_STATE_DONE;

	case SBIC_CSR_XFERRED|DATA_OUT_PHASE:
	case SBIC_CSR_XFERRED|DATA_IN_PHASE:
	case SBIC_CSR_MIS|DATA_OUT_PHASE:
	case SBIC_CSR_MIS|DATA_IN_PHASE:
	case SBIC_CSR_MIS_1|DATA_OUT_PHASE:
	case SBIC_CSR_MIS_1|DATA_IN_PHASE:
	case SBIC_CSR_MIS_2|DATA_OUT_PHASE:
	case SBIC_CSR_MIS_2|DATA_IN_PHASE:
		if( dev->sc_xs->flags & SCSI_POLL || dev->sc_flags & SBICF_ICMD
		   || acb->sc_dmacmd == 0 ) {
			/* Do PIO */
			SET_SBIC_control(regs, SBIC_CTL_EDI | SBIC_CTL_IDI);
			if (acb->sc_kv.dc_count <= 0) {
				printf("sbicnextstate:xfer count %d asr%x csr%x\n",
				       acb->sc_kv.dc_count, asr, csr);
				goto abort;
			}
			wait = sbic_data_wait;
			if( sbicxfstart(regs,
					acb->sc_kv.dc_count,
					SBIC_PHASE(csr), wait))
				if( SBIC_PHASE(csr) == DATA_IN_PHASE )
					/* data in? */
					i=sbicxfin(regs,
						   acb->sc_kv.dc_count,
						   acb->sc_kv.dc_addr);
				else
					i=sbicxfout(regs,
						    acb->sc_kv.dc_count,
						    acb->sc_kv.dc_addr,
						    SBIC_PHASE(csr));
			acb->sc_kv.dc_addr +=
				(acb->sc_kv.dc_count - i);
			acb->sc_kv.dc_count = i;
		} else {
			if (acb->sc_kv.dc_count <= 0) {
				printf("sbicnextstate:xfer count %d asr%x csr%x\n",
				       acb->sc_kv.dc_count, asr, csr);
				goto abort;
			}
			/*
			 * do scatter-gather dma
			 * hacking the controller chip, ouch..
			 */
			SET_SBIC_control(regs, SBIC_CTL_EDI | SBIC_CTL_IDI |
					 SBIC_MACHINE_DMA_MODE);
			/*
			 * set next dma addr and dec count
			 */
#if 0
			SBIC_TC_GET(regs, tcnt);
			dev->sc_cur->dc_count -= ((dev->sc_tcnt - tcnt) >> 1);
			dev->sc_cur->dc_addr += (dev->sc_tcnt - tcnt);
			dev->sc_tcnt = acb->sc_tcnt = tcnt;
#else
			sbic_save_ptrs(dev, regs, dev->target, dev->lun);
			sbic_load_ptrs(dev, regs, dev->target, dev->lun);
#endif
#ifdef DEBUG
			if( data_pointer_debug > 1 )
				printf("next dmanext: %d(%x:%x)\n",
				       dev->target,dev->sc_cur->dc_addr,
				       dev->sc_tcnt);
			dev->sc_dmatimo = 1;
#endif
			dev->sc_tcnt = dev->sc_dmanext(dev);
			SBIC_TC_PUT(regs, (unsigned)dev->sc_tcnt);
			SET_SBIC_cmd(regs, SBIC_CMD_XFER_INFO);
			dev->sc_flags |= SBICF_INDMA;
		}
		break;

	case SBIC_CSR_XFERRED|MESG_IN_PHASE:
	case SBIC_CSR_MIS|MESG_IN_PHASE:
	case SBIC_CSR_MIS_1|MESG_IN_PHASE:
	case SBIC_CSR_MIS_2|MESG_IN_PHASE:
		SBIC_TRACE(dev);
		return sbicmsgin(dev);

	case SBIC_CSR_MSGIN_W_ACK:
		SET_SBIC_cmd(regs, SBIC_CMD_CLR_ACK); /* Dunno what I'm ACKing */
		printf("Acking unknown msgin CSR:%02x",csr);
		break;

	case SBIC_CSR_XFERRED|MESG_OUT_PHASE:
	case SBIC_CSR_MIS|MESG_OUT_PHASE:
	case SBIC_CSR_MIS_1|MESG_OUT_PHASE:
	case SBIC_CSR_MIS_2|MESG_OUT_PHASE:
#ifdef DEBUG
		if (sync_debug)
			printf ("sending REJECT msg to last msg.\n");
#endif

		sbic_save_ptrs(dev, regs, dev->target, dev->lun);
		/*
		 * should only get here on reject,
		 * since it's always US that
		 * initiate a sync transfer
		 */
		SEND_BYTE(regs, MSG_REJECT);
		WAIT_CIP(regs);
		if( asr & (SBIC_ASR_BSY|SBIC_ASR_LCI|SBIC_ASR_CIP) )
			printf("next: REJECT sent asr %02x\n", asr);
		SBIC_TRACE(dev);
		return SBIC_STATE_RUNNING;

	case SBIC_CSR_DISC:
	case SBIC_CSR_DISC_1:
		dev->sc_flags &= ~(SBICF_INDMA|SBICF_SELECTED);

		/* Try to schedule another target */
#ifdef DEBUG
		if(reselect_debug>1)
			printf("sbicnext target %d disconnected\n", dev->target);
#endif
		TAILQ_INSERT_HEAD(&dev->nexus_list, acb, chain);
		++dev->sc_tinfo[dev->target].dconns;
		dev->sc_nexus = NULL;
		dev->sc_xs = NULL;

		if( acb->xs->flags & SCSI_POLL
		   || (dev->sc_flags & SBICF_ICMD)
		   || !sbic_parallel_operations ) {
			SBIC_TRACE(dev);
			return SBIC_STATE_DISCONNECT;
		}
		sbic_sched(dev);
		SBIC_TRACE(dev);
		return SBIC_STATE_DISCONNECT;

	case SBIC_CSR_RSLT_NI:
	case SBIC_CSR_RSLT_IFY:
		GET_SBIC_rselid(regs, newtarget);
		/* check SBIC_RID_SIV? */
		newtarget &= SBIC_RID_MASK;
		if (csr == SBIC_CSR_RSLT_IFY) {
			/* Read IFY msg to avoid lockup */
			GET_SBIC_data(regs, newlun);
			WAIT_CIP(regs);
			newlun &= SBIC_TLUN_MASK;
			CSR_TRACE('r',csr,asr,newtarget);
		} else {
			/* Need to get IFY message */
			for (newlun = 256; newlun; --newlun) {
				GET_SBIC_asr(regs, asr);
				if (asr & SBIC_ASR_INT)
					break;
				delay(1);
			}
			newlun = 0;	/* XXXX */
			if ((asr & SBIC_ASR_INT) == 0) {
#ifdef DEBUG
				if (reselect_debug)
					printf("RSLT_NI - no IFFY message? asr %x\n", asr);
#endif
			} else {
				GET_SBIC_csr(regs,csr);
				CSR_TRACE('n',csr,asr,newtarget);
				if (csr == SBIC_CSR_MIS|MESG_IN_PHASE ||
				    csr == SBIC_CSR_MIS_1|MESG_IN_PHASE ||
				    csr == SBIC_CSR_MIS_2|MESG_IN_PHASE) {
					sbicmsgin(dev);
					newlun = dev->sc_msg[0] & 7;
				} else {
					printf("RSLT_NI - not MESG_IN_PHASE %x\n",
					    csr);
				}
			}
		}
#ifdef DEBUG
		if(reselect_debug>1 || (reselect_debug && csr==SBIC_CSR_RSLT_NI))
			printf("sbicnext: reselect %s from targ %d lun %d\n",
			    csr == SBIC_CSR_RSLT_NI ? "NI" : "IFY",
			    newtarget, newlun);
#endif
		if (dev->sc_nexus) {
#ifdef DEBUG
			if (reselect_debug > 1)
				printf("%s: reselect %s with active command\n",
				    dev->sc_dev.dv_xname,
				    csr == SBIC_CSR_RSLT_NI ? "NI" : "IFY");
#ifdef DDB
/*			Debugger();*/
#endif
#endif
			TAILQ_INSERT_HEAD(&dev->ready_list, dev->sc_nexus, chain);
			dev->sc_tinfo[dev->target].lubusy &= ~(1 << dev->lun);
			dev->sc_nexus = NULL;
			dev->sc_xs = NULL;
		}
		/* Reload sync values for this target */
		if (dev->sc_sync[newtarget].state == SYNC_DONE)
			SET_SBIC_syn(regs, SBIC_SYN (dev->sc_sync[newtarget].offset,
			    dev->sc_sync[newtarget].period));
		else
			SET_SBIC_syn(regs, SBIC_SYN (0, sbic_min_period));
		for (acb = dev->nexus_list.tqh_first; acb;
		    acb = acb->chain.tqe_next) {
			if (acb->xs->sc_link->target != newtarget ||
			    acb->xs->sc_link->lun != newlun)
				continue;
			TAILQ_REMOVE(&dev->nexus_list, acb, chain);
			dev->sc_nexus = acb;
			dev->sc_xs = acb->xs;
			dev->sc_flags |= SBICF_SELECTED;
			dev->target = newtarget;
			dev->lun = newlun;
			break;
		}
		if (acb == NULL) {
			printf("%s: reselect %s targ %d not in nexus_list %x\n",
			    dev->sc_dev.dv_xname,
			    csr == SBIC_CSR_RSLT_NI ? "NI" : "IFY", newtarget,
			    &dev->nexus_list.tqh_first);
			panic("bad reselect in sbic");
		}
		if (csr == SBIC_CSR_RSLT_IFY)
			SET_SBIC_cmd(regs, SBIC_CMD_CLR_ACK);
		break;

	default:
        abort:
		/*
		 * Something unexpected happened -- deal with it.
		 */
		printf("sbicnextstate: aborting csr %02x asr %02x\n", csr, asr);
#ifdef DDB
		Debugger();
#endif
#ifdef DEBUG
		if( data_pointer_debug > 1 )
			printf("next dmastop: %d(%x:%x)\n",
			       dev->target,dev->sc_cur->dc_addr,dev->sc_tcnt);
		dev->sc_dmatimo = 0;
#endif
		dev->sc_dmastop(dev);
		SET_SBIC_control(regs, SBIC_CTL_EDI | SBIC_CTL_IDI);
		sbicerror(dev, regs, csr);
		sbicabort(dev, regs, "next");
		if (dev->sc_flags & SBICF_INDMA) {
			/*
			 * check for overlapping cache line, flush if so
			 */
#ifdef M68040
			if (dev->sc_flags & SBICF_DCFLUSH) {
#if 0
				printf("sibc: 68040 DMA cache flush needs fixing? %x:%x\n",
				    dev->sc_xs->data, dev->sc_xs->datalen);
#endif
			}
#endif
			dev->sc_flags &=
				~(SBICF_INDMA | SBICF_DCFLUSH);
#ifdef DEBUG
			if( data_pointer_debug > 1 )
				printf("next dmastop: %d(%x:%x)\n",
				    dev->target,dev->sc_cur->dc_addr,dev->sc_tcnt);
			dev->sc_dmatimo = 0;
#endif
			dev->sc_dmastop(dev);
			sbic_scsidone(acb, -1);
		}
		SBIC_TRACE(dev);
                return SBIC_STATE_ERROR;
	}

	SBIC_TRACE(dev);
	return(SBIC_STATE_RUNNING);
}


/*
 * Check if DMA can not be used with specified buffer
 */

int
sbiccheckdmap(bp, len, mask)
	void *bp;
	u_long len, mask;
{
	u_char *buffer;
	u_long phy_buf;
	u_long phy_len;

	buffer = bp;

	if (len == 0)
		return(0);

	while (len) {
		phy_buf = kvtop(buffer);
		if (len < (phy_len = NBPG - ((int) buffer & PGOFSET)))
			phy_len = len;
		if (phy_buf & mask)
			return(1);
		buffer += phy_len;
		len -= phy_len;
	}
	return(0);
}

int
sbictoscsiperiod(dev, regs, a)
	struct sbic_softc *dev;
	sbic_regmap_p regs;
	int a;
{
	unsigned int fs;

	/*
	 * cycle = DIV / (2*CLK)
	 * DIV = FS+2
	 * best we can do is 200ns at 20Mhz, 2 cycles
	 */

	GET_SBIC_myid(regs,fs);
	fs = (fs >>6) + 2;		/* DIV */
	fs = (fs * 10000) / (dev->sc_clkfreq<<1);	/* Cycle, in ns */
	if (a < 2) a = 8;		/* map to Cycles */
	return ((fs*a)>>2);		/* in 4 ns units */
}

int
sbicfromscsiperiod(dev, regs, p)
	struct sbic_softc *dev;
	sbic_regmap_p regs;
	int p;
{
	register unsigned int fs, ret;

	/* Just the inverse of the above */

	GET_SBIC_myid(regs,fs);
	fs = (fs >>6) + 2;		/* DIV */
	fs = (fs * 10000) / (dev->sc_clkfreq<<1);   /* Cycle, in ns */

	ret = p << 2;			/* in ns units */
	ret = ret / fs;			/* in Cycles */
	if (ret < sbic_min_period)
		return(sbic_min_period);

	/* verify rounding */
	if (sbictoscsiperiod(dev, regs, ret) < p)
		ret++;
	return (ret >= 8) ? 0 : ret;
}

#ifdef DEBUG

void sbicdumpstate()
{
	u_char csr, asr;

	GET_SBIC_asr(debug_sbic_regs,asr);
	GET_SBIC_csr(debug_sbic_regs,csr);
	printf("%s: asr:csr(%02x:%02x)->(%02x:%02x)\n",
	       (routine==1)?"sbicgo":
	       (routine==2)?"sbicintr":
	       (routine==3)?"sbicicmd":
	       (routine==4)?"sbicnext":"unknown",
	       debug_asr, debug_csr, asr, csr);

}

void sbictimeout(dev)
	struct sbic_softc *dev;
{
	int s, asr;

	s = splbio();
	if (dev->sc_dmatimo) {
		if (dev->sc_dmatimo > 1) {
			printf("%s: dma timeout #%d\n",
			    dev->sc_dev.dv_xname, dev->sc_dmatimo - 1);
			GET_SBIC_asr(dev->sc_sbicp, asr);
			if( asr & SBIC_ASR_INT ) {
				/* We need to service a missed IRQ */
				printf("Servicing a missed int:(%02x,%02x)->(%02x,??)\n",
				    debug_asr, debug_csr, asr);
				sbicintr(dev);
			}
			sbicdumpstate();
		}
		dev->sc_dmatimo++;
	}
	splx(s);
	timeout((void *)sbictimeout, dev, 30 * hz);
}

void
sbic_dump_acb(acb)
	struct sbic_acb *acb;
{
	u_char *b = (u_char *) &acb->cmd;
	int i;

	printf("acb@%x ", acb);
	if (acb->xs == NULL) {
		printf("<unused>\n");
		return;
	}
	printf("(%d:%d) flags %2x clen %2d cmd ", acb->xs->sc_link->target,
	    acb->xs->sc_link->lun, acb->flags, acb->clen);
	for (i = acb->clen; i; --i)
		printf(" %02x", *b++);
	printf("\n");
	printf("  xs: %08x data %8x:%04x ", acb->xs, acb->xs->data,
	    acb->xs->datalen);
	printf("va %8x:%04x ", acb->sc_kv.dc_addr, acb->sc_kv.dc_count);
	printf("pa %8x:%04x tcnt %x\n", acb->sc_pa.dc_addr, acb->sc_pa.dc_count,
	    acb->sc_tcnt);
}

void
sbic_dump(dev)
	struct sbic_softc *dev;
{
	sbic_regmap_p regs;
	u_char csr, asr;
	struct sbic_acb *acb;
	int s;
	int i;

	s = splbio();
	regs = dev->sc_sbicp;
#if CSR_TRACE_SIZE
	printf("csr trace: ");
	i = csr_traceptr;
	do {
		printf("%c%02x%02x%02x ", csr_trace[i].whr,
		    csr_trace[i].csr, csr_trace[i].asr, csr_trace[i].xtn);
		switch(csr_trace[i].whr) {
		case 'g':
			printf("go "); break;
		case 's':
			printf("select "); break;
		case 'y':
			printf("select+ "); break;
		case 'i':
			printf("intr "); break;
		case 'f':
			printf("finish "); break;
		case '>':
			printf("out "); break;
		case '<':
			printf("in "); break;
		case 'm':
			printf("msgin "); break;
		case 'x':
			printf("msginx "); break;
		case 'X':
			printf("msginX "); break;
		case 'r':
			printf("reselect "); break;
		case 'I':
			printf("icmd "); break;
		case 'a':
			printf("abort "); break;
		default:
			printf("? ");
		}
		switch(csr_trace[i].csr) {
		case 0x11:
			printf("INITIATOR"); break;
		case 0x16:
			printf("S_XFERRED"); break;
		case 0x20:
			printf("MSGIN_ACK"); break;
		case 0x41:
			printf("DISC"); break;
		case 0x42:
			printf("SEL_TIMEO"); break;
		case 0x80:
			printf("RSLT_NI"); break;
		case 0x81:
			printf("RSLT_IFY"); break;
		case 0x85:
			printf("DISC_1"); break;
		case 0x18: case 0x19: case 0x1a:
		case 0x1b: case 0x1e: case 0x1f:
		case 0x28: case 0x29: case 0x2a:
		case 0x2b: case 0x2e: case 0x2f:
		case 0x48: case 0x49: case 0x4a:
		case 0x4b: case 0x4e: case 0x4f:
		case 0x88: case 0x89: case 0x8a:
		case 0x8b: case 0x8e: case 0x8f:
			switch(csr_trace[i].csr & 0xf0) {
			case 0x10:
				printf("DONE_"); break;
			case 0x20:
				printf("STOP_"); break;
			case 0x40:
				printf("ERR_"); break;
			case 0x80:
				printf("REQ_"); break;
			}
			switch(csr_trace[i].csr & 7) {
			case 0:
				printf("DATA_OUT"); break;
			case 1:
				printf("DATA_IN"); break;
			case 2:
				printf("CMD"); break;
			case 3:
				printf("STATUS"); break;
			case 6:
				printf("MSG_OUT"); break;
			case 7:
				printf("MSG_IN"); break;
			default:
				printf("invld phs");
			}
			break;
		default:    printf("****"); break;
		}
		if (csr_trace[i].asr & SBIC_ASR_INT)
			printf(" ASR_INT");
		if (csr_trace[i].asr & SBIC_ASR_LCI)
			printf(" ASR_LCI");
		if (csr_trace[i].asr & SBIC_ASR_BSY)
			printf(" ASR_BSY");
		if (csr_trace[i].asr & SBIC_ASR_CIP)
			printf(" ASR_CIP");
		printf("\n");
		i = (i + 1) & (CSR_TRACE_SIZE - 1);
	} while (i != csr_traceptr);
#endif
	GET_SBIC_asr(regs, asr);
	if ((asr & SBIC_ASR_INT) == 0)
		GET_SBIC_csr(regs, csr);
	else
		csr = 0;
	printf("%s@%x regs %x asr %x csr %x\n", dev->sc_dev.dv_xname,
	    dev, regs, asr, csr);
	if (acb = dev->free_list.tqh_first) {
		printf("Free list:\n");
		while (acb) {
			sbic_dump_acb(acb);
			acb = acb->chain.tqe_next;
		}
	}
	if (acb = dev->ready_list.tqh_first) {
		printf("Ready list:\n");
		while (acb) {
			sbic_dump_acb(acb);
			acb = acb->chain.tqe_next;
		}
	}
	if (acb = dev->nexus_list.tqh_first) {
		printf("Nexus list:\n");
		while (acb) {
			sbic_dump_acb(acb);
			acb = acb->chain.tqe_next;
		}
	}
	if (dev->sc_nexus) {
		printf("nexus:\n");
		sbic_dump_acb(dev->sc_nexus);
	}
	printf("sc_xs %x targ %d lun %d flags %x tcnt %x dmacmd %x mask %x\n",
	    dev->sc_xs, dev->target, dev->lun, dev->sc_flags, dev->sc_tcnt,
	    dev->sc_dmacmd, dev->sc_dmamask);
	for (i = 0; i < 8; ++i) {
		if (dev->sc_tinfo[i].cmds > 2) {
			printf("tgt %d: cmds %d disc %d senses %d lubusy %x\n",
			    i, dev->sc_tinfo[i].cmds,
			    dev->sc_tinfo[i].dconns,
			    dev->sc_tinfo[i].senses,
			    dev->sc_tinfo[i].lubusy);
		}
	}
	splx(s);
}

#endif
