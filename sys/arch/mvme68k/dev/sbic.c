/*	$OpenBSD: sbic.c,v 1.18 2007/05/28 22:17:21 pyr Exp $ */
/*	$NetBSD: sbic.c,v 1.2 1996/04/23 16:32:54 chuck Exp $	*/

/*
 * Changes Copyright (c) 1996 Steve Woodford
 * Original Copyright (c) 1994 Christian E. Hopps
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *  @(#)scsi.c  7.5 (Berkeley) 5/4/91
 */

/*
 * Steve Woodford (SCW), Apr, 1996
 * MVME147S WD33C93 Scsi Bus Interface Controller driver,
 *
 * Basically a de-loused and tidied up version of the Amiga AMD 33C93 driver.
 *
 * The original driver used features which required at least a WD33C93A
 * chip. The '147 has the original WD33C93 chip (no 'A' suffix).
 *
 * This version of the driver is pretty well generic, so should work with
 * any flavour of WD33C93 chip.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h> /* For hz */
#include <sys/disklabel.h>
#include <sys/dkstat.h>
#include <sys/buf.h>
#include <sys/queue.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <uvm/uvm_extern.h>
#include <mvme68k/dev/dmavar.h>
#include <mvme68k/dev/sbicreg.h>
#include <mvme68k/dev/sbicvar.h>
#include <machine/autoconf.h>
#include <mvme68k/dev/pccreg.h>


/*
 * Since I can't find this in any other header files
 */
#define SCSI_PHASE(reg) (reg&0x07)

/*
 * SCSI delays
 * In u-seconds, primarily for state changes on the SPC.
 */
#define SBIC_CMD_WAIT   50000   /* wait per step of 'immediate' cmds */
#define SBIC_DATA_WAIT  50000   /* wait per data in/out step */
#define SBIC_INIT_WAIT  50000   /* wait per step (both) during init */

/*
 * Convenience macro for waiting for a particular sbic event
 */
#define SBIC_WAIT(regs, until, timeo) sbicwait(regs, until, timeo, __LINE__)

int     sbicicmd(struct sbic_softc *, void *, int, void *, int);
int     sbicgo(struct sbic_softc *, struct scsi_xfer *);
int     sbicdmaok(struct sbic_softc *, struct scsi_xfer *);
int     sbicwait(sbic_regmap_p, u_char, int , int);
int     sbiccheckdmap(void *, u_long, u_long);
u_char  sbicselectbus(struct sbic_softc *);
int     sbicxfout(sbic_regmap_p, int, void *);
int     sbicxfin(sbic_regmap_p, int, void *);
int     sbicfromscsiperiod(struct sbic_softc *, int);
int     sbictoscsiperiod(struct sbic_softc *, int);
int     sbicintr(struct sbic_softc *);
int     sbicpoll(struct sbic_softc *);
int     sbicnextstate(struct sbic_softc *, u_char, u_char);
int     sbicmsgin(struct sbic_softc *);
int     sbicabort(struct sbic_softc *, char *);
void    sbicxfdone(struct sbic_softc *);
void    sbicerror(struct sbic_softc *,u_char);
void    sbicreset(struct sbic_softc *);
void    sbic_scsidone(struct sbic_acb *, int);
void    sbic_sched(struct sbic_softc *);
void    sbic_save_ptrs(struct sbic_softc *);
void    sbic_load_ptrs(struct sbic_softc *);
void    sbicinit(struct sbic_softc *);

/*
 * Synch xfer parameters, and timing conversions
 */
int     sbic_min_period = SBIC_SYN_MIN_PERIOD;  /* in cycles = f(ICLK,FSn) */
int     sbic_max_offset = SBIC_SYN_MAX_OFFSET;  /* pure number */
int     sbic_cmd_wait   = SBIC_CMD_WAIT;
int     sbic_data_wait  = SBIC_DATA_WAIT;
int     sbic_init_wait  = SBIC_INIT_WAIT;

/*
 * was broken before.. now if you want this you get it for all drives
 * on sbic controllers.
 */
u_char  sbic_inhibit_sync[8];
int     sbic_enable_reselect     = 1;   /* Allow Disconnect / Reselect */
int     sbic_no_dma              = 0;   /* Use PIO transfers instead of DMA */
int     sbic_parallel_operations = 1;   /* Allow command queues */

/*
 * Some useful stuff for debugging purposes
 */
#ifdef DEBUG
int     sbicdma_ops     = 0;    /* total DMA operations */
int     sbicdma_hits    = 0;    /* number of DMA chains that were contiguous */
int     sbicdma_misses  = 0;    /* number of DMA chains that were not contiguous */
int     sbicdma_saves   = 0;

#define QPRINTF(a) if (sbic_debug > 1) printf a

int     sbic_debug      = 0;    /* Debug all chip related things */
int     sync_debug      = 0;    /* Debug all Synchronous Scsi related things */
int     reselect_debug  = 0;    /* Debug all reselection related things */
int     report_sense    = 0;    /* Always print Sense information */
int     data_pointer_debug = 0; /* Debug Data Pointer related things */

void    sbictimeout(struct sbic_softc *dev);

#else
#define QPRINTF(a)  /* */
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
sbic_save_ptrs(dev)
    struct sbic_softc   *dev;
{
    sbic_regmap_p       regs;
    struct sbic_acb*    acb;
    int                 count,
                        asr,
                        s;

    /*
     * Only need to save pointers if DMA was active...
     */
    if ( dev->sc_cur == NULL || (dev->sc_flags & SBICF_INDMA) == 0 )
        return;

    regs = dev->sc_sbicp;

    s = splbio();

    /*
     * Wait until WD chip is idle
     */
    do {
        GET_SBIC_asr(regs, asr);
        if( asr & SBIC_ASR_DBR ) {
            printf("sbic_save_ptrs: asr %02x canceled!\n", asr);
            splx(s);
            return;
        }
    } while( asr & (SBIC_ASR_BSY|SBIC_ASR_CIP) );


    /*
     * Save important state.
     * must be done before dmastop
     */
    acb            = dev->sc_nexus;
    acb->sc_dmacmd = dev->sc_dmacmd;

    /*
     * Fetch the residual count
     */
    SBIC_TC_GET(regs, count);

    /*
     * Shut down DMA
     */
    dev->sc_dmastop(dev);

    /*
     * No longer in DMA
     */
    dev->sc_flags &= ~SBICF_INDMA;

    /*
     * Ensure the WD chip is back in polled I/O mode, with nothing to
     * transfer.
     */
    SBIC_TC_PUT(regs, 0);
    SET_SBIC_control(regs, SBIC_CTL_EDI | SBIC_CTL_IDI);

    /*
     * Update current count...
     */
    acb->sc_tcnt = count;

    /*
     * Work out how many bytes were actually transferred
     */
    count        = dev->sc_tcnt - count;
    dev->sc_tcnt = acb->sc_tcnt;

    /*
     * Fixup partial xfers
     */
    acb->sc_kv.dc_addr  += count;
    acb->sc_kv.dc_count -= count;
    acb->sc_pa.dc_addr  += count;
    acb->sc_pa.dc_count -= count >> 1;

#ifdef DEBUG
    if ( data_pointer_debug )
        printf("save at (%x,%x):%x\n",
               dev->sc_cur->dc_addr, dev->sc_cur->dc_count,count);
    sbicdma_saves++;
#endif

    splx(s);
}


/*
 * DOES NOT RESTART DMA!!!
 */
void
sbic_load_ptrs(dev)
    struct sbic_softc   *dev;
{
    struct sbic_acb *acb = dev->sc_nexus;
    int             s;

    if ( acb->sc_kv.dc_count == 0 ) {
        /*
         * No data to xfer
         */
        return;
    }

    s = splbio();

    /*
     * Reset the Scatter-Gather chain
     */
    dev->sc_last = dev->sc_cur = &acb->sc_pa;

    /*
     * Restore the Transfer Count and DMA specific data
     */
    dev->sc_tcnt   = acb->sc_tcnt;
    dev->sc_dmacmd = acb->sc_dmacmd;

#ifdef DEBUG
    sbicdma_ops++;
#endif

    /*
     * Need to fixup new segment?
     */
    if ( dev->sc_tcnt == 0 ) {
        /*
         * sc_tcnt == 0 implies end of segment
         */
        char    *vaddr, *paddr;
        int     count;

        /*
         * do kvm to pa mappings
         */
        vaddr = acb->sc_kv.dc_addr;
        paddr = acb->sc_pa.dc_addr = (char *)kvtop((vaddr_t)vaddr);

        for (count = (NBPG - ((int)vaddr & PGOFSET));
             count < acb->sc_kv.dc_count &&
                (char *)kvtop((vaddr_t)vaddr + count + 4) == paddr + count + 4;
             count += NBPG)
            ;   /* Do nothing */

        /*
         * If it's all contiguous...
         */
        if ( count > acb->sc_kv.dc_count ) {
            count = acb->sc_kv.dc_count;
#ifdef  DEBUG
            sbicdma_hits++;
#endif
        }
#ifdef  DEBUG
        else
            sbicdma_misses++;
#endif

        acb->sc_tcnt        = count;
        acb->sc_pa.dc_count = count >> 1;

#ifdef DEBUG
        if ( data_pointer_debug )
            printf("DMA recalc:kv(%x,%x)pa(%x,%x)\n", acb->sc_kv.dc_addr,
                                                      acb->sc_kv.dc_count,
                                                      acb->sc_pa.dc_addr,
                                                      acb->sc_tcnt);
#endif

    }

    splx(s);
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
    struct scsi_link    *slp = xs->sc_link;
    struct sbic_softc   *dev = slp->adapter_softc;
    struct sbic_acb     *acb;
    int                 flags = xs->flags,
                        s;

    if ( flags & SCSI_DATA_UIO )
        panic("sbic: scsi data uio requested");

    if ( dev->sc_nexus && (flags & SCSI_POLL) )
        panic("sbic_scsicmd: busy");

    if ( slp->target == slp->adapter_target )
        return ESCAPE_NOT_SUPPORTED;

    s = splbio();

    if ( (acb = TAILQ_FIRST(&dev->free_list)) != NULL )
        TAILQ_REMOVE(&dev->free_list, acb, chain);

    splx(s);

    if ( acb == NULL ) {
#ifdef DEBUG
        printf("sbic_scsicmd: unable to queue request for target %d\n",
            slp->target);
#ifdef DDB
        Debugger();
#endif
#endif
        return(TRY_AGAIN_LATER);
    }

    if ( flags & SCSI_DATA_IN )
        acb->flags = ACB_ACTIVE | ACB_DATAIN;
    else
        acb->flags = ACB_ACTIVE;

    acb->xs             = xs;
    acb->clen           = xs->cmdlen;
    acb->sc_kv.dc_addr  = xs->data;
    acb->sc_kv.dc_count = xs->datalen;
    acb->pa_addr        = xs->data ? (char *)kvtop((vaddr_t)xs->data) : 0;
    bcopy(xs->cmd, &acb->cmd, xs->cmdlen);

    if ( flags & SCSI_POLL ) {
        /*
         * This has major side effects -- it locks up the machine
         */
        int stat;

        s = splbio();

        dev->sc_flags |= SBICF_ICMD;

        do {
            /*
             * If we already had a nexus, while away the time until idle...
             * This is likely only to happen if a reselection occurs between
             * here and our earlier check for ICMD && sc_nexus (which would
             * have resulted in a panic() had it been true).
             */
            while ( dev->sc_nexus )
                sbicpoll(dev);

            /*
             * Fix up the new nexus
             */
            dev->sc_nexus   = acb;
            dev->sc_xs      = xs;
            dev->target     = slp->target;
            dev->lun        = slp->lun;

            stat = sbicicmd(dev, &acb->cmd, acb->clen,
                            acb->sc_kv.dc_addr, acb->sc_kv.dc_count);

        } while ( dev->sc_nexus != acb );

        sbic_scsidone(acb, stat);

        splx(s);

        return(COMPLETE);
    }

    s = splbio();
    TAILQ_INSERT_TAIL(&dev->ready_list, acb, chain);

    /*
     * If nothing is active, try to start it now.
     */
    if ( dev->sc_nexus == NULL )
        sbic_sched(dev);

    splx(s);

    return(SUCCESSFULLY_QUEUED);
}

/*
 * attempt to start the next available command
 */
void
sbic_sched(dev)
    struct sbic_softc *dev;
{
    struct scsi_xfer    *xs;
    struct scsi_link    *slp = NULL;    /* Gag the compiler */
    struct sbic_acb     *acb;
    int                 flags,
                        stat;

    /*
     * XXXSCW
     * I'll keep this test here, even though I can't see any obvious way
     * in which sbic_sched() could be called with sc_nexus non NULL
     */
    if ( dev->sc_nexus )
        return;         /* a command is current active */

    /*
     * Loop through the ready list looking for work to do...
     */
    TAILQ_FOREACH(acb, &dev->ready_list, chain) {
        int     i, j;

        slp = acb->xs->sc_link;
        i   = slp->target;
        j   = 1 << slp->lun;

        /*
         * We've found a potential command, but is the target/lun busy?
         */
        if ( (dev->sc_tinfo[i].lubusy & j) == 0 ) {
            /*
             * Nope, it's not busy, so we can use it.
             */
            dev->sc_tinfo[i].lubusy |= j;
            TAILQ_REMOVE(&dev->ready_list, acb, chain);
            dev->sc_nexus = acb;
            acb->sc_pa.dc_addr = acb->pa_addr;  /* XXXX check */
            break;
        }
    }

    if ( acb == NULL ) {
        QPRINTF(("sbicsched: no work\n"));
        return;         /* did not find an available command */
    }

#ifdef DEBUG
    if ( data_pointer_debug > 1 )
        printf("sbic_sched(%d,%d)\n", slp->target, slp->lun);
#endif

    dev->sc_xs = xs = acb->xs;
    flags      = xs->flags;

    if ( flags & SCSI_RESET )
        sbicreset(dev);

    dev->sc_stat[0] = -1;
    dev->target     = slp->target;
    dev->lun        = slp->lun;

    if ( flags & SCSI_POLL || (!sbic_parallel_operations &&
                              (sbicdmaok(dev, xs) == 0)) )
        stat = sbicicmd(dev, &acb->cmd, acb->clen,
                        acb->sc_kv.dc_addr, acb->sc_kv.dc_count);
    else
    if ( sbicgo(dev, xs) == 0 )
        return;
    else
        stat = dev->sc_stat[0];

    sbic_scsidone(acb, stat);
}

void
sbic_scsidone(acb, stat)
    struct sbic_acb *acb;
    int             stat;
{
    struct scsi_xfer    *xs  = acb->xs;
    struct scsi_link    *slp = xs->sc_link;
    struct sbic_softc   *dev = slp->adapter_softc;
    int                 dosched = 0;

#ifdef DIAGNOSTIC
    if ( acb == NULL || xs == NULL ) {
        printf("sbic_scsidone -- (%d,%d) no scsi_xfer\n", dev->target, dev->lun);
#ifdef DDB
        Debugger();
#endif
        return;
    }
#endif

    /*
     * is this right?
     */
    xs->status = stat;

#ifdef DEBUG
    if ( data_pointer_debug > 1 )
        printf("scsidone: (%d,%d)->(%d,%d)%02x\n", slp->target, slp->lun,
                                                   dev->target, dev->lun, stat);

    if ( xs->sc_link->target == dev->sc_link.adapter_target )
        panic("target == hostid");
#endif

    if ( xs->error == XS_NOERROR && (acb->flags & ACB_CHKSENSE) == 0 ) {

        if ( stat == SCSI_CHECK ) {
            /*
             * Schedule a REQUEST SENSE
             */
            struct scsi_sense *ss = (void *)&acb->cmd;

#ifdef DEBUG
            if ( report_sense )
                printf("sbic_scsidone: autosense %02x targ %d lun %d",
                        acb->cmd.opcode, slp->target, slp->lun);
#endif

            bzero(ss, sizeof(*ss));

            ss->opcode          = REQUEST_SENSE;
            ss->byte2           = slp->lun << 5;
            ss->length          = sizeof(struct scsi_sense_data);

            acb->clen           = sizeof(*ss);
            acb->sc_kv.dc_addr  = (char *)&xs->sense;
            acb->sc_kv.dc_count = sizeof(struct scsi_sense_data);
            acb->pa_addr        = (char *)kvtop((vaddr_t)&xs->sense); /* XXX check */
            acb->flags          = ACB_ACTIVE | ACB_CHKSENSE | ACB_DATAIN;

            TAILQ_INSERT_HEAD(&dev->ready_list, acb, chain);

            dev->sc_tinfo[slp->target].lubusy &= ~(1 << slp->lun);
            dev->sc_tinfo[slp->target].senses++;

            if ( dev->sc_nexus == acb ) {
                dev->sc_nexus = NULL;
                dev->sc_xs = NULL;
                sbic_sched(dev);
            }
            return;
        }
    }

    if ( xs->error == XS_NOERROR && (acb->flags & ACB_CHKSENSE) != 0 ) {

        xs->error = XS_SENSE;

#ifdef DEBUG
        if (report_sense)
            printf(" => %02x %02x\n", xs->sense.flags, 
			xs->sense.extra_bytes[3]);
#endif

    } else {
        xs->resid = 0;      /* XXXX */
    }

    xs->flags |= ITSDONE;

    /*
     * Remove the ACB from whatever queue it's on.  We have to do a bit of
     * a hack to figure out which queue it's on.  Note that it is *not*
     * necessary to cdr down the ready queue, but we must cdr down the
     * nexus queue and see if it's there, so we can mark the unit as no
     * longer busy.  This code is sickening, but it works.
     */
    if ( acb == dev->sc_nexus ) {

        dev->sc_nexus = NULL;
        dev->sc_xs    = NULL;

        dev->sc_tinfo[slp->target].lubusy &= ~(1 << slp->lun);

        if ( !TAILQ_EMPTY(&dev->ready_list) )
            dosched = 1;    /* start next command */

    } else
    if (TAILQ_LAST(&dev->ready_list) == TAILQ_NEXT(acb, chain)) {

        TAILQ_REMOVE(&dev->ready_list, acb, chain);

    } else {

        register struct sbic_acb *a;

	TAILQ_FOREACH(a, &dev->nexus_list, chain) {
            if ( a == acb ) {
                TAILQ_REMOVE(&dev->nexus_list, acb, chain);
                dev->sc_tinfo[slp->target].lubusy &= ~(1 << slp->lun);
                break;
            }
        }

        if ( a )
            ;
        else if ( TAILQ_NEXT(acb, chain) != NULL) {
            TAILQ_REMOVE(&dev->ready_list, acb, chain);
        } else {
            printf("%s: can't find matching acb\n", dev->sc_dev.dv_xname);
#ifdef DDB
            Debugger();
#endif
        }
    }

    /*
     * Put it on the free list.
     */
    acb->flags = ACB_FREE;
    TAILQ_INSERT_HEAD(&dev->free_list, acb, chain);

    dev->sc_tinfo[slp->target].cmds++;

    scsi_done(xs);

    if ( dosched )
        sbic_sched(dev);
}

int
sbicdmaok(dev, xs)
    struct sbic_softc   *dev;
    struct scsi_xfer    *xs;
{
    if ( sbic_no_dma || xs->datalen & 0x03 || (int)xs->data & 0x03)
        return(0);

    /*
     * controller supports dma to any addresses?
     */
    if ( (dev->sc_flags & SBICF_BADDMA) == 0 )
        return(1);

    /*
     * this address is ok for dma?
     */
    if ( sbiccheckdmap(xs->data, xs->datalen, dev->sc_dmamask) == 0 )
        return(1);

    return(0);
}

int
sbicwait(regs, until, timeo, line)
    sbic_regmap_p   regs;
    u_char          until;
    int             timeo;
    int             line;
{
    u_char  val;

    if ( timeo == 0 )
        timeo = 1000000;    /* some large value.. */

    GET_SBIC_asr(regs, val);

    while ( (val & until) == 0 ) {

        if ( timeo-- == 0 ) {
            int csr;
            GET_SBIC_csr(regs, csr);
            printf("sbicwait TIMEO @%d with asr=x%x csr=x%x\n", line, val, csr);
#if defined(DDB) && defined(DEBUG)
            Debugger();
#endif
            return(val); /* Maybe I should abort */
            break;
        }

        DELAY(1);
        GET_SBIC_asr(regs, val);
    }

    return(val);
}

int
sbicabort(dev, where)
    struct sbic_softc   *dev;
    char                *where;
{
    sbic_regmap_p   regs = dev->sc_sbicp;
    u_char          csr,
                    asr;

    GET_SBIC_asr(regs, asr);
    GET_SBIC_csr(regs, csr);

    printf ("%s: abort %s: csr = 0x%02x, asr = 0x%02x\n",
            dev->sc_dev.dv_xname, where, csr, asr);

    /*
     * Clean up chip itself
     */
    if ( dev->sc_flags & SBICF_SELECTED ) {

        while ( asr & SBIC_ASR_DBR ) {
            /*
             * sbic is jammed w/data. need to clear it
             * But we don't know what direction it needs to go
             */
            GET_SBIC_data(regs, asr);
            printf("%s: abort %s: clearing data buffer 0x%02x\n",
                   dev->sc_dev.dv_xname, where, asr);
            GET_SBIC_asr(regs, asr);
            if ( asr & SBIC_ASR_DBR ) /* Not the read direction, then */
                SET_SBIC_data(regs, asr);
            GET_SBIC_asr(regs, asr);
        }

        WAIT_CIP(regs);

        printf("%s: sbicabort - sending ABORT command\n", dev->sc_dev.dv_xname);
        SET_SBIC_cmd(regs, SBIC_CMD_ABORT);
        WAIT_CIP(regs);

        GET_SBIC_asr(regs, asr);

        if ( asr & (SBIC_ASR_BSY|SBIC_ASR_LCI) ) {
            /*
             * ok, get more drastic..
             */
            printf("%s: sbicabort - asr %x, trying to reset\n",
                    dev->sc_dev.dv_xname, asr);
            sbicreset(dev);
            dev->sc_flags &= ~SBICF_SELECTED;
            return SBIC_STATE_ERROR;
        }

        printf("%s: sbicabort - sending DISC command\n", dev->sc_dev.dv_xname);
        SET_SBIC_cmd(regs, SBIC_CMD_DISC);

        do {
            SBIC_WAIT (regs, SBIC_ASR_INT, 0);
            GET_SBIC_asr(regs, asr);
            GET_SBIC_csr (regs, csr);
            QPRINTF(("csr: 0x%02x, asr: 0x%02x\n", csr, asr));
        } while ( (csr != SBIC_CSR_DISC) && (csr != SBIC_CSR_DISC_1) &&
                  (csr != SBIC_CSR_CMD_INVALID) );

        /*
         * lets just hope it worked..
         */
        dev->sc_flags &= ~SBICF_SELECTED;
    }

    return SBIC_STATE_ERROR;
}


/*
 * Initialize driver-private structures
 */
void
sbicinit(dev)
    struct sbic_softc *dev;
{
    u_int   i;

    extern u_long   scsi_nosync;
    extern int      shift_nosync;

    if ( (dev->sc_flags & SBICF_ALIVE) == 0 ) {

        struct sbic_acb *acb;

        TAILQ_INIT(&dev->ready_list);
        TAILQ_INIT(&dev->nexus_list);
        TAILQ_INIT(&dev->free_list);

        dev->sc_nexus = NULL;
        dev->sc_xs    = NULL;

        acb = dev->sc_acb;
        bzero(acb, sizeof(dev->sc_acb));

        for (i = 0; i < sizeof(dev->sc_acb) / sizeof(*acb); i++) {
            TAILQ_INSERT_TAIL(&dev->free_list, acb, chain);
            acb++;
        }

        bzero(dev->sc_tinfo, sizeof(dev->sc_tinfo));

#ifdef DEBUG
        /*
         * make sure timeout is really not needed
         */
        timeout((void *)sbictimeout, dev, 30 * hz);
#endif

    } else
        panic("sbic: reinitializing driver!");

    dev->sc_flags |=  SBICF_ALIVE;
    dev->sc_flags &= ~SBICF_SELECTED;

    /*
     * initialize inhibit array
     */
    if ( scsi_nosync ) {

        u_int inhibit_sync = (scsi_nosync >> shift_nosync) & 0xff;

        shift_nosync += 8;

#ifdef DEBUG
        if ( inhibit_sync )
            printf("%s: Inhibiting synchronous transfer %02x\n",
                        dev->sc_dev.dv_xname, inhibit_sync);
#endif
        for (i = 0; i < 8; ++i) {
            if ( inhibit_sync & (1 << i) )
                sbic_inhibit_sync[i] = 1;
        }
    }

    sbicreset(dev);
}

void
sbicreset(dev)
    struct sbic_softc *dev;
{
    sbic_regmap_p   regs = dev->sc_sbicp;
    u_int           my_id,
                    s;
    u_char          csr;

    s = splbio();

    my_id = dev->sc_link.adapter_target & SBIC_ID_MASK;

    if (dev->sc_clkfreq < 110)
        my_id |= SBIC_ID_FS_8_10;
    else if (dev->sc_clkfreq < 160)
        my_id |= SBIC_ID_FS_12_15;
    else if (dev->sc_clkfreq < 210)
        my_id |= SBIC_ID_FS_16_20;

    SET_SBIC_myid(regs, my_id);

    /*
     * Reset the chip
     */
    SET_SBIC_cmd(regs, SBIC_CMD_RESET);
    DELAY(25);

    SBIC_WAIT(regs, SBIC_ASR_INT, 0);
    GET_SBIC_csr(regs, csr);       /* clears interrupt also */

    /*
     * Set up various chip parameters
     */
    SET_SBIC_control(regs, SBIC_CTL_EDI | SBIC_CTL_IDI);

    /*
     * don't allow Selection (SBIC_RID_ES)
     * until we can handle target mode!!
     */
    SET_SBIC_rselid(regs, SBIC_RID_ER);

    /*
     * Asynchronous for now
     */
    SET_SBIC_syn(regs, 0);

    /*
     * Anything else was zeroed by reset
     */
    splx(s);

    dev->sc_flags &= ~SBICF_SELECTED;
}

void
sbicerror(dev, csr)
    struct sbic_softc   *dev;
    u_char              csr;
{
    struct scsi_xfer    *xs  = dev->sc_xs;

#ifdef DIAGNOSTIC
    if ( xs == NULL )
        panic("sbicerror: dev->sc_xs == NULL");
#endif

    if ( xs->flags & SCSI_SILENT )
        return;

    printf("%s: csr == 0x%02x\n", dev->sc_dev.dv_xname, csr);
}

/*
 * select the bus, return when selected or error.
 *
 * Returns the current CSR following selection and optionally MSG out phase.
 * i.e. the returned CSR *should* indicate CMD phase...
 * If the return value is 0, some error happened.
 */
u_char
sbicselectbus(dev)
    struct sbic_softc   *dev;
{
    sbic_regmap_p   regs   = dev->sc_sbicp;
    u_char          target = dev->target,
                    lun    = dev->lun,
                    asr,
                    csr,
                    id;

    /*
     * if we're already selected, return (XXXX panic maybe?)
     */
    if ( dev->sc_flags & SBICF_SELECTED )
        return(0);

    QPRINTF(("sbicselectbus %d: ", target));

    /*
     * issue select
     */
    SET_SBIC_selid(regs, target);
    SET_SBIC_timeo(regs, SBIC_TIMEOUT(250, dev->sc_clkfreq));

    GET_SBIC_asr(regs, asr);

    if ( asr & (SBIC_ASR_INT|SBIC_ASR_BSY) ) {
        /*
         * This means we got ourselves reselected upon
         */
        QPRINTF(("WD busy (reselect?)\n"));
        return 0;
    }

    SET_SBIC_cmd(regs, SBIC_CMD_SEL_ATN);

    /*
     * wait for select (merged from separate function may need
     * cleanup)
     */
    WAIT_CIP(regs);

    do {

        asr = SBIC_WAIT(regs, SBIC_ASR_INT | SBIC_ASR_LCI, 0);

        if ( asr & SBIC_ASR_LCI ) {
            QPRINTF(("late LCI: asr %02x\n", asr));
            return 0;
        }

        /*
         * Clear interrupt
         */
        GET_SBIC_csr (regs, csr);

        QPRINTF(("%02x ", csr));

        /*
         * Reselected from under our feet?
         */
        if ( csr == SBIC_CSR_RSLT_NI || csr == SBIC_CSR_RSLT_IFY ) {
            QPRINTF(("got reselected, asr %02x\n", asr));
            /*
             * We need to handle this now so we don't lock up later
             */
            sbicnextstate(dev, csr, asr);

            return 0;
        }

        /*
         * Whoops!
         */
        if ( csr == SBIC_CSR_SLT || csr == SBIC_CSR_SLT_ATN ) {
            panic("sbicselectbus: target issued select!");
            return 0;
        }

    } while (csr != (SBIC_CSR_MIS_2 | MESG_OUT_PHASE) &&
             csr != (SBIC_CSR_MIS_2 | CMD_PHASE) &&
             csr != SBIC_CSR_SEL_TIMEO);

    /*
     * Anyone at home?
     */
    if ( csr == SBIC_CSR_SEL_TIMEO ) {
        dev->sc_xs->error = XS_SELTIMEOUT;
        QPRINTF(("Selection Timeout\n"));
        return 0;
    }

    QPRINTF(("Selection Complete\n"));

    /*
     * Assume we're now selected
     */
    GET_SBIC_selid(regs, id);
    dev->target    = id;
    dev->lun       = lun;
    dev->sc_flags |= SBICF_SELECTED;

    /*
     * Enable (or not) reselection
     * XXXSCW This is probably not necessary since we don't use use the
     * Select-and-Xfer-with-ATN command to initiate a selection...
     */
    if ( !sbic_enable_reselect && TAILQ_EMPTY(&dev->nexus_list))
        SET_SBIC_rselid (regs, 0);
    else
        SET_SBIC_rselid (regs, SBIC_RID_ER);

    /*
     * We only really need to do anything when the target goes to MSG out
     * If the device ignored ATN, it's probably old and brain-dead,
     * but we'll try to support it anyhow.
     * If it doesn't support message out, it definitely doesn't
     * support synchronous transfers, so no point in even asking...
     */
    if ( csr == (SBIC_CSR_MIS_2 | MESG_OUT_PHASE) ) {
        /*
         * Send identify message (SCSI-2 requires an identify msg)
         */
        if ( sbic_inhibit_sync[id] && dev->sc_sync[id].state == SYNC_START ) {
            /*
             * Handle drives that don't want to be asked
             * whether to go sync at all.
             */
            dev->sc_sync[id].offset = 0;
            dev->sc_sync[id].period = sbic_min_period;
            dev->sc_sync[id].state  = SYNC_DONE;
        }

        /*
         * Do we need to negotiate Synchronous Xfers for this target?
         */
        if ( dev->sc_sync[id].state != SYNC_START ) {
            /*
             * Nope, we've already negotiated.
             * Now see if we should allow the target to disconnect/reselect...
             */
            if ( dev->sc_xs->flags & SCSI_POLL || dev->sc_flags & SBICF_ICMD ||
                                                  !sbic_enable_reselect )
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
            if ( sync_debug )
                printf("\nSending sync request to target %d ... ", id);
#endif
            /*
             * setup scsi message sync message request
             */
            dev->sc_msg[0] = MSG_IDENTIFY | lun;
            dev->sc_msg[1] = MSG_EXT_MESSAGE;
            dev->sc_msg[2] = 3;
            dev->sc_msg[3] = MSG_SYNC_REQ;
            dev->sc_msg[4] = sbictoscsiperiod(dev, sbic_min_period);
            dev->sc_msg[5] = sbic_max_offset;

            sbicxfout(regs, 6, dev->sc_msg);

            dev->sc_sync[id].state = SYNC_SENT;
#ifdef DEBUG
            if ( sync_debug )
                printf ("sent\n");
#endif
        }

        /*
         * There's one interrupt still to come: the change to CMD phase...
         */
        SBIC_WAIT(regs, SBIC_ASR_INT , 0);
        GET_SBIC_csr(regs, csr);
    }

    /*
     * set sync or async
     */
    if ( dev->sc_sync[target].state == SYNC_DONE ) {
#ifdef  DEBUG
        if ( sync_debug )
            printf("select(%d): sync reg = 0x%02x\n", target,
                            SBIC_SYN(dev->sc_sync[target].offset,
                                     dev->sc_sync[target].period));
#endif
        SET_SBIC_syn(regs, SBIC_SYN(dev->sc_sync[target].offset,
                                    dev->sc_sync[target].period));
    } else {
#ifdef  DEBUG
        if ( sync_debug )
            printf("select(%d): sync reg = 0x%02x\n", target,
                            SBIC_SYN(0,sbic_min_period));
#endif
        SET_SBIC_syn(regs, SBIC_SYN(0, sbic_min_period));
    }

    return csr;
}

/*
 * Information Transfer *to* a Scsi Target.
 *
 * Note: Don't expect there to be an interrupt immediately after all
 * the data is transferred out. The WD spec sheet says that the Transfer-
 * Info command for non-MSG_IN phases only completes when the target
 * next asserts 'REQ'. That is, when the SCSI bus changes to a new state.
 *
 * This can have a nasty effect on commands which take a relatively long
 * time to complete, for example a START/STOP unit command may remain in
 * CMD phase until the disk has spun up. Only then will the target change
 * to STATUS phase. This is really only a problem for immediate commands
 * since we don't allow disconnection for them (yet).
 */
int
sbicxfout(regs, len, bp)
    sbic_regmap_p   regs;
    int             len;
    void            *bp;
{
    int     wait = sbic_data_wait;
    u_char  asr,
            *buf = bp;

    QPRINTF(("sbicxfout {%d} %02x %02x %02x %02x %02x "
        "%02x %02x %02x %02x %02x\n", len, buf[0], buf[1], buf[2],
        buf[3], buf[4], buf[5], buf[6], buf[7], buf[8], buf[9]));

    /*
     * sigh.. WD-PROTO strikes again.. sending the command in one go
     * causes the chip to lock up if talking to certain (misbehaving?)
     * targets. Anyway, this procedure should work for all targets, but
     * it's slightly slower due to the overhead
     */
    WAIT_CIP (regs);

    SBIC_TC_PUT (regs, 0);
    SET_SBIC_control(regs, SBIC_CTL_EDI | SBIC_CTL_IDI);
    SBIC_TC_PUT (regs, (unsigned)len);
    SET_SBIC_cmd (regs, SBIC_CMD_XFER_INFO);

    /*
     * Loop for each byte transferred
     */
    do {

        GET_SBIC_asr (regs, asr);

        if ( asr & SBIC_ASR_DBR ) {
            if ( len ) {
                SET_SBIC_data (regs, *buf);
                buf++;
                len--;
            } else {
                SET_SBIC_data (regs, 0);
            }
            wait = sbic_data_wait;
        }

    } while ( len && (asr & SBIC_ASR_INT) == 0 && wait-- > 0 );

#ifdef  DEBUG
    QPRINTF(("sbicxfout done: %d bytes remaining (wait:%d)\n", len, wait));
#endif

    /*
     * Normally, an interrupt will be pending when this routing returns.
     */
    return(len);
}

/*
 * Information Transfer *from* a Scsi Target
 * returns # bytes left to read
 */
int
sbicxfin(regs, len, bp)
    sbic_regmap_p   regs;
    int             len;
    void            *bp;
{
    int     wait = sbic_data_wait;
    u_char  *buf = bp;
    u_char  asr;
#ifdef  DEBUG
    u_char  *obp = bp;
#endif

    WAIT_CIP (regs);

    SET_SBIC_control(regs, SBIC_CTL_EDI | SBIC_CTL_IDI);
    SBIC_TC_PUT (regs, (unsigned)len);
    SET_SBIC_cmd (regs, SBIC_CMD_XFER_INFO);

    /*
     * Loop for each byte transferred
     */
    do {

        GET_SBIC_asr (regs, asr);

        if ( asr & SBIC_ASR_DBR ) {
            if ( len ) {
                GET_SBIC_data (regs, *buf);
                buf++;
                len--;
            } else {
                u_char foo;
                GET_SBIC_data (regs, foo);
            }
            wait = sbic_data_wait;
        }

    } while ( (asr & SBIC_ASR_INT) == 0 && wait-- > 0 );

    QPRINTF(("sbicxfin {%d} %02x %02x %02x %02x %02x %02x "
        "%02x %02x %02x %02x\n", len, obp[0], obp[1], obp[2],
        obp[3], obp[4], obp[5], obp[6], obp[7], obp[8], obp[9]));

    SBIC_TC_PUT (regs, 0);

    /*
     * this leaves with one csr to be read
     */
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
 *
 * Note that although this routine looks like it can handle disconnect/
 * reselect, the fact is that it can't. There is still some work to be
 * done to clean this lot up.
 */
int
sbicicmd(dev, cbuf, clen, buf, len)
    struct sbic_softc   *dev;
    void                *cbuf,
                        *buf;
    int                 clen,
                        len;
{
    sbic_regmap_p   regs = dev->sc_sbicp;
    struct sbic_acb *acb = dev->sc_nexus;
    u_char          csr,
                    asr;
    int             still_busy = SBIC_STATE_RUNNING;
#ifdef  DEBUG
    int             counter = 0;
#endif

    /*
     * Make sure pointers are OK
     */
    dev->sc_last = dev->sc_cur = &acb->sc_pa;
    dev->sc_tcnt = acb->sc_tcnt = 0;

    acb->sc_dmacmd      = 0;
    acb->sc_pa.dc_count = 0; /* No DMA */
    acb->sc_kv.dc_addr  = buf;
    acb->sc_kv.dc_count = len;

#ifdef  DEBUG
    if ( data_pointer_debug > 1 )
        printf("sbicicmd(%d,%d):%d\n", dev->target, dev->lun, acb->sc_kv.dc_count);
#endif

    /*
     * set the sbic into non-DMA mode
     */
    SET_SBIC_control(regs, SBIC_CTL_EDI | SBIC_CTL_IDI);

    dev->sc_stat[0] = 0xff;
    dev->sc_msg[0]  = 0xff;

    /*
     * We're stealing the SCSI bus
     */
    dev->sc_flags |= SBICF_ICMD;

    do {
        GET_SBIC_asr (regs, asr);

        /*
         * select the SCSI bus (it's an error if bus isn't free)
         */
        if ( (dev->sc_flags & SBICF_SELECTED) == 0 &&
             still_busy != SBIC_STATE_DISCONNECT ) {
            if ( (csr = sbicselectbus(dev)) == 0 ) {
                dev->sc_flags &= ~SBICF_ICMD;
                return(-1);
            }
        } else
        if ( (asr & (SBIC_ASR_BSY | SBIC_ASR_INT)) == SBIC_ASR_INT )
            GET_SBIC_csr(regs, csr);
        else
            csr = 0;

        if ( csr ) {

            QPRINTF((">ASR:0x%02x CSR:0x%02x< ", asr, csr));

            switch ( csr ) {

              case SBIC_CSR_S_XFERRED:
              case SBIC_CSR_DISC:
              case SBIC_CSR_DISC_1:
                {
                    u_char  phase;

                    dev->sc_flags &= ~SBICF_SELECTED;
                    GET_SBIC_cmd_phase (regs, phase);

                    if ( phase == 0x60 ) {
                        GET_SBIC_tlun (regs, dev->sc_stat[0]);
                        still_busy = SBIC_STATE_DONE; /* done */
                    } else {
#ifdef DEBUG
                        if ( reselect_debug > 1 )
                            printf("sbicicmd: handling disconnect\n");
#endif
                        still_busy = SBIC_STATE_DISCONNECT;
                    }
                }
                break;

              case SBIC_CSR_XFERRED | CMD_PHASE:
              case SBIC_CSR_MIS     | CMD_PHASE:
              case SBIC_CSR_MIS_1   | CMD_PHASE:
              case SBIC_CSR_MIS_2   | CMD_PHASE:
                {
                    if ( sbicxfout(regs, clen, cbuf) )
                        still_busy = sbicabort(dev, "icmd sending cmd");
                }
                break;

              case SBIC_CSR_XFERRED | STATUS_PHASE:
              case SBIC_CSR_MIS     | STATUS_PHASE:
              case SBIC_CSR_MIS_1   | STATUS_PHASE:
              case SBIC_CSR_MIS_2   | STATUS_PHASE:
                {
                    /*
                     * The sbic does the status/cmd-complete reading ok,
                     * so do this with its hi-level commands.
                     */
#ifdef DEBUG
                    if ( sbic_debug )
                        printf("SBICICMD status phase (bsy=%d)\n", still_busy);
#endif
                    SET_SBIC_cmd_phase(regs, 0x46);
                    SET_SBIC_cmd(regs, SBIC_CMD_SEL_ATN_XFER);
                }
                break;

              default:
                {
                    still_busy = sbicnextstate(dev, csr, asr);
                }
                break;
            }

            /*
             * make sure the last command was taken,
             * ie. we're not hunting after an ignored command..
             */
            GET_SBIC_asr(regs, asr);

            /*
             * tapes may take a loooong time..
             */
            while (asr & SBIC_ASR_BSY ) {

                if ( asr & SBIC_ASR_DBR ) {
                    int     i;

                    printf("sbicicmd: Waiting while sbic is jammed, CSR:%02x,ASR:%02x\n", csr,asr);
#ifdef DDB
                    Debugger();
#endif
                    /*
                     * SBIC is jammed
                     * DUNNO which direction
                     * Try old direction
                     */
                    GET_SBIC_data(regs, i);
                    GET_SBIC_asr(regs, asr);

                    if ( asr & SBIC_ASR_DBR ) /* Wants us to write */
                        SET_SBIC_data(regs, i);
                }

                GET_SBIC_asr(regs, asr);
            }
        }

        /*
         * wait for last command to complete
         */
        if ( asr & SBIC_ASR_LCI ) {
            printf("sbicicmd: last command ignored\n");
        }
        else
        if ( still_busy >= SBIC_STATE_RUNNING ) /* Bsy */
            SBIC_WAIT (regs, SBIC_ASR_INT, sbic_cmd_wait);

        /*
         * do it again
         */
    } while ( still_busy >= SBIC_STATE_RUNNING && dev->sc_stat[0] == 0xff );

    /*
     * Sometimes we need to do an extra read of the CSR
     */
    GET_SBIC_csr(regs, csr);

#ifdef DEBUG
    if ( data_pointer_debug > 1 )
        printf("sbicicmd done(%d,%d):%d =%d=\n", dev->target, dev->lun,
                                                 acb->sc_kv.dc_count,
                                                 dev->sc_stat[0]);
#endif

    dev->sc_flags &= ~SBICF_ICMD;

    return(dev->sc_stat[0]);
}

/*
 * Finish SCSI xfer command:  After the completion interrupt from
 * a read/write operation, sequence through the final phases in
 * programmed i/o.  This routine is a lot like sbicicmd except we
 * skip (and don't allow) the select, cmd out and data in/out phases.
 */
void
sbicxfdone(dev)
    struct sbic_softc   *dev;
{
    sbic_regmap_p   regs = dev->sc_sbicp;
    u_char          phase,
                    csr;
    int             s;

    QPRINTF(("{"));
    s = splbio();

    /*
     * have the sbic complete on its own
     */
    SBIC_TC_PUT(regs, 0);
    SET_SBIC_cmd_phase(regs, 0x46);
    SET_SBIC_cmd(regs, SBIC_CMD_SEL_ATN_XFER);

    do {

        SBIC_WAIT (regs, SBIC_ASR_INT, 0);
        GET_SBIC_csr (regs, csr);
        QPRINTF(("%02x:", csr));

    } while ( (csr != SBIC_CSR_DISC) && (csr != SBIC_CSR_DISC_1) &&
              (csr != SBIC_CSR_S_XFERRED));

    dev->sc_flags &= ~SBICF_SELECTED;

    GET_SBIC_cmd_phase (regs, phase);
    QPRINTF(("}%02x", phase));

    if ( phase == 0x60 )
        GET_SBIC_tlun(regs, dev->sc_stat[0]);
    else
        sbicerror(dev, csr);

    QPRINTF(("=STS:%02x=\n", dev->sc_stat[0]));

    splx(s);
}

/*
 * No DMA chains
 */
int
sbicgo(dev, xs)
    struct sbic_softc   *dev;
    struct scsi_xfer    *xs;
{
    struct sbic_acb *acb = dev->sc_nexus;
    sbic_regmap_p   regs = dev->sc_sbicp;
    int             i,
                    dmaflags,
                    count,
                    usedma;
    u_char          csr,
                    asr,
                    *addr;

    dev->target = xs->sc_link->target;
    dev->lun    = xs->sc_link->lun;

    usedma = sbicdmaok(dev, xs);

#ifdef DEBUG
    if ( data_pointer_debug > 1 )
        printf("sbicgo(%d,%d): usedma=%d\n", dev->target, dev->lun, usedma);
#endif

    /*
     * select the SCSI bus (it's an error if bus isn't free)
     */
    if ( (csr = sbicselectbus(dev)) == 0 )
        return(0); /* Not done: needs to be rescheduled */

    dev->sc_stat[0] = 0xff;

    /*
     * Calculate DMA chains now
     */
    if ( acb->flags & ACB_DATAIN )
        dmaflags = DMAGO_READ;
    else
        dmaflags = 0;

    addr  = acb->sc_kv.dc_addr;
    count = acb->sc_kv.dc_count;

    if ( count && ((char *)kvtop((vaddr_t)addr) != acb->sc_pa.dc_addr) ) {
        printf("sbic: DMA buffer mapping changed %p->%lx\n",
                acb->sc_pa.dc_addr, kvtop((vaddr_t)addr));
#ifdef DDB
        Debugger();
#endif
    }

#ifdef DEBUG
    ++sbicdma_ops;          /* count total DMA operations */
#endif

    /*
     * Allocate the DMA chain
     * Mark end of segment...
     */
    acb->sc_tcnt        = dev->sc_tcnt = 0;
    acb->sc_pa.dc_count = 0;

    sbic_load_ptrs(dev);

    /*
     * Enable interrupts but don't do any DMA
     * enintr() also enables interrupts for the sbic
     */
    dev->sc_enintr(dev);

    if ( usedma ) {
        dev->sc_tcnt = dev->sc_dmago(dev, acb->sc_pa.dc_addr,
                                          acb->sc_pa.dc_count, dmaflags);
#ifdef DEBUG
        dev->sc_dmatimo = dev->sc_tcnt ? 1 : 0;
#endif
    } else
        dev->sc_dmacmd = 0; /* Don't use DMA */

    acb->sc_dmacmd = dev->sc_dmacmd;

#ifdef DEBUG
    if ( data_pointer_debug > 1 ) {
        printf("sbicgo dmago:%d(%x:%x) dmacmd=0x%02x\n", dev->target,
                                           dev->sc_cur->dc_addr,
                                           dev->sc_tcnt,
                                           dev->sc_dmacmd);
    }
#endif

    /*
     * Lets cycle a while then let the interrupt handler take over.
     */
    GET_SBIC_asr(regs, asr);

    do {

        QPRINTF(("go "));

        /*
         * Handle the new phase
         */
        i = sbicnextstate(dev, csr, asr);
#if 0
        WAIT_CIP(regs);
#endif
        if ( i == SBIC_STATE_RUNNING ) {
            GET_SBIC_asr(regs, asr);

            if ( asr & SBIC_ASR_LCI )
                printf("sbicgo: LCI asr:%02x csr:%02x\n", asr, csr);

            if ( asr & SBIC_ASR_INT )
                GET_SBIC_csr(regs, csr);
        }

    } while ( i == SBIC_STATE_RUNNING && asr & (SBIC_ASR_INT|SBIC_ASR_LCI) );

    if ( i == SBIC_STATE_DONE ) {
        if ( dev->sc_stat[0] == 0xff )
#if 0
            printf("sbicgo: done & stat = 0xff\n");
#else
            ;
#endif
        else
            return 1;   /* Did we really finish that fast? */
    }

    return 0;
}


int
sbicintr(dev)
    struct sbic_softc   *dev;
{
    sbic_regmap_p       regs = dev->sc_sbicp;
    u_char              asr,
                        csr;
    int                 i;

    /*
     * pending interrupt?
     */
    GET_SBIC_asr (regs, asr);
    if ( (asr & SBIC_ASR_INT) == 0 )
        return(0);

    GET_SBIC_csr(regs, csr);

    do {

        QPRINTF(("intr[0x%x]", csr));

        i = sbicnextstate(dev, csr, asr);
#if 0
        WAIT_CIP(regs);
#endif
        if ( i == SBIC_STATE_RUNNING ) {
            GET_SBIC_asr(regs, asr);

            if ( asr & SBIC_ASR_LCI )
                printf("sbicgo: LCI asr:%02x csr:%02x\n", asr, csr);

            if ( asr & SBIC_ASR_INT )
                GET_SBIC_csr(regs, csr);
        }

    } while ( i == SBIC_STATE_RUNNING && asr & (SBIC_ASR_INT|SBIC_ASR_LCI) );

    QPRINTF(("intr done. state=%d, asr=0x%02x\n", i, asr));

    return(1);
}

/*
 * Run commands and wait for disconnect.
 * This is only ever called when a command is in progress, when we
 * want to busy wait for it to finish.
 */
int
sbicpoll(dev)
    struct sbic_softc   *dev;
{
    sbic_regmap_p       regs = dev->sc_sbicp;
    u_char              asr,
                        csr = 0;
    int                 i;

    /*
     * Wait for the next interrupt
     */
    SBIC_WAIT(regs, SBIC_ASR_INT, sbic_cmd_wait);

    do {
        GET_SBIC_asr (regs, asr);

        if ( asr & SBIC_ASR_INT )
            GET_SBIC_csr(regs, csr);

        QPRINTF(("poll[0x%x]", csr));

        /*
         * Handle it
         */
        i = sbicnextstate(dev, csr, asr);

        WAIT_CIP(regs);
        GET_SBIC_asr(regs, asr);

        /*
         * tapes may take a loooong time..
         */
        while ( asr & SBIC_ASR_BSY ) {
            u_char z = 0;

            if ( asr & SBIC_ASR_DBR ) {
                printf("sbipoll: Waiting while sbic is jammed, CSR:%02x,ASR:%02x\n", csr,asr);
#ifdef DDB
                Debugger();
#endif
                /*
                 * SBIC is jammed
                 * DUNNO which direction
                 * Try old direction
                 */
                GET_SBIC_data(regs, z);
                GET_SBIC_asr(regs, asr);

                if ( asr & SBIC_ASR_DBR ) /* Wants us to write */
                    SET_SBIC_data(regs, z);
            }

            GET_SBIC_asr(regs, asr);
        }

        if ( asr & SBIC_ASR_LCI )
            printf("sbicpoll: LCI asr:%02x csr:%02x\n", asr,csr);
        else
        if ( i == SBIC_STATE_RUNNING ) /* BSY */
            SBIC_WAIT(regs, SBIC_ASR_INT, sbic_cmd_wait);

    } while ( i == SBIC_STATE_RUNNING );

    return(1);
}

/*
 * Handle a single msgin
 */
int
sbicmsgin(dev)
    struct sbic_softc   *dev;
{
    sbic_regmap_p       regs = dev->sc_sbicp;
    int                 recvlen = 1;
    u_char              asr,
                        csr,
                        *tmpaddr,
                        *msgaddr;

    tmpaddr = msgaddr = dev->sc_msg;

    tmpaddr[0] = 0xff;
    tmpaddr[1] = 0xff;

    GET_SBIC_asr(regs, asr);

#ifdef DEBUG
    if ( reselect_debug > 1 )
        printf("sbicmsgin asr=%02x\n", asr);
#endif

    GET_SBIC_selid (regs, csr);
    SET_SBIC_selid (regs, csr | SBIC_SID_FROM_SCSI);

    SBIC_TC_PUT(regs, 0);
    SET_SBIC_control(regs, SBIC_CTL_EDI | SBIC_CTL_IDI);

    do {
        while( recvlen-- ) {

            /*
             * Fetch the next byte of the message
             */
            RECV_BYTE(regs, *tmpaddr);

            /*
             * get the command completion interrupt, or we
             * can't send a new command (LCI)
             */
            SBIC_WAIT(regs, SBIC_ASR_INT, 0);
            GET_SBIC_csr(regs, csr);

#ifdef DEBUG
            if ( reselect_debug > 1 )
                printf("sbicmsgin: got %02x csr %02x\n", *tmpaddr, csr);
#endif

            tmpaddr++;

            if ( recvlen ) {
                /*
                 * Clear ACK, and wait for the interrupt for the next byte
                 */
                SET_SBIC_cmd(regs, SBIC_CMD_CLR_ACK);
                SBIC_WAIT(regs, SBIC_ASR_INT, 0);
                GET_SBIC_csr(regs, csr);
            }
        }

        if ( msgaddr[0] == 0xff ) {
            printf("sbicmsgin: sbic swallowed our message\n");
            break;
        }

#ifdef DEBUG
        if ( sync_debug ) {
            GET_SBIC_asr(regs, asr);
            printf("msgin done csr 0x%x asr 0x%x msg 0x%x\n", csr, asr, msgaddr[0]);
        }
#endif
        /*
         * test whether this is a reply to our sync
         * request
         */
        if ( MSG_ISIDENTIFY(msgaddr[0]) ) {

            /*
             * Got IFFY msg -- ack it
             */
            QPRINTF(("IFFY"));

        } else
        if ( msgaddr[0] == MSG_REJECT &&
             dev->sc_sync[dev->target].state == SYNC_SENT) {

            /*
             * Target probably rejected our Sync negotiation.
             */
            QPRINTF(("REJECT of SYN"));

#ifdef DEBUG
            if ( sync_debug )
                printf("target %d rejected sync, going async\n", dev->target);
#endif

            dev->sc_sync[dev->target].period = sbic_min_period;
            dev->sc_sync[dev->target].offset = 0;
            dev->sc_sync[dev->target].state  = SYNC_DONE;
            SET_SBIC_syn(regs, SBIC_SYN(dev->sc_sync[dev->target].offset,
                                        dev->sc_sync[dev->target].period));

        } else
        if ( msgaddr[0] == MSG_REJECT ) {

            /*
             * we'll never REJECt a REJECT message..
             */
            QPRINTF(("REJECT"));

        } else
        if ( msgaddr[0] == MSG_SAVE_DATA_PTR ) {

            /*
             * don't reject this either.
             */
            QPRINTF(("MSG_SAVE_DATA_PTR"));

        } else
        if ( msgaddr[0] == MSG_RESTORE_PTR ) {

            /*
             * don't reject this either.
             */
            QPRINTF(("MSG_RESTORE_PTR"));

        } else
        if ( msgaddr[0] == MSG_DISCONNECT ) {

            /*
             * Target is disconnecting...
             */
            QPRINTF(("DISCONNECT"));

#ifdef DEBUG
            if ( reselect_debug > 1 && msgaddr[0] == MSG_DISCONNECT )
                printf("sbicmsgin: got disconnect msg %s\n",
                       (dev->sc_flags & SBICF_ICMD) ? "rejecting" : "");
#endif

            if ( dev->sc_flags & SBICF_ICMD ) {
                /*
                 * We're in immediate mode. Prevent disconnects.
                 * prepare to reject the message, NACK
                 */
                SET_SBIC_cmd(regs, SBIC_CMD_SET_ATN);
                WAIT_CIP(regs);
            }

        } else
        if ( msgaddr[0] == MSG_CMD_COMPLETE ) {

            /*
             * !! KLUDGE ALERT !! quite a few drives don't seem to
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
            QPRINTF(("CMD_COMPLETE"));

#ifdef DEBUG
            if ( sync_debug )
                printf ("GOT MSG %d! target %d acting weird.."
                        " waiting for disconnect...\n", msgaddr[0], dev->target);
#endif

            /*
             * Check to see if sbic is handling this
             */
            GET_SBIC_asr(regs, asr);

            /*
             * XXXSCW: I'm not convinced of this, we haven't negated ACK yet...
             */
            if ( asr & SBIC_ASR_BSY )
                return SBIC_STATE_RUNNING;

            /*
             * Let's try this: Assume it works and set status to 00
             */
            dev->sc_stat[0] = 0;

        } else
        if ( msgaddr[0] == MSG_EXT_MESSAGE && tmpaddr == &(msgaddr[1]) ) {

            /*
             * Target is sending us an extended message. We'll assume it's
             * the response to our Sync. negotiation.
             */
            QPRINTF(("ExtMSG\n"));

            /*
             * Read in whole extended message. First, negate ACK to accept
             * the MSG_EXT_MESSAGE byte...
             */
            SET_SBIC_cmd(regs, SBIC_CMD_CLR_ACK);

            /*
             * Wait for the interrupt for the next byte (length)
             */
            SBIC_WAIT(regs, SBIC_ASR_INT, 0);
            GET_SBIC_csr(regs, csr);

#ifdef  DEBUG
            QPRINTF(("CLR ACK csr %02x\n", csr));
#endif

            /*
             * Read the length byte
             */
            RECV_BYTE(regs, *tmpaddr);

            /*
             * Wait for command completion IRQ
             */
            SBIC_WAIT(regs, SBIC_ASR_INT, 0);
            GET_SBIC_csr(regs, csr);

            /*
             * Reload the loop counter
             */
            recvlen = *tmpaddr++;

            QPRINTF(("Recving ext msg, csr %02x len %02x\n", csr, recvlen));

        } else
        if ( msgaddr[0] == MSG_EXT_MESSAGE && msgaddr[1] == 3 &&
             msgaddr[2] == MSG_SYNC_REQ ) {

            /*
             * We've received the complete Extended Message Sync. Request...
             */
            QPRINTF(("SYN"));

            /*
             * Compute the required Transfer Period for the WD chip...
             */
            dev->sc_sync[dev->target].period = sbicfromscsiperiod(dev, msgaddr[3]);
            dev->sc_sync[dev->target].offset = msgaddr[4];
            dev->sc_sync[dev->target].state  = SYNC_DONE;

            /*
             * Put the WD chip in synchronous mode
             */
            SET_SBIC_syn(regs, SBIC_SYN(dev->sc_sync[dev->target].offset,
                                        dev->sc_sync[dev->target].period));
#ifdef  DEBUG
            if ( sync_debug )
                printf("msgin(%d): sync reg = 0x%02x\n", dev->target,
                                SBIC_SYN(dev->sc_sync[dev->target].offset,
                                         dev->sc_sync[dev->target].period));
#endif

            printf("%s: target %d now synchronous, period=%dns, offset=%d.\n",
                   dev->sc_dev.dv_xname, dev->target,
                   msgaddr[3] * 4, msgaddr[4]);

        } else {

            /*
             * We don't support whatever this message is...
             */
#ifdef DEBUG
            if ( sbic_debug || sync_debug )
                printf ("sbicmsgin: Rejecting message 0x%02x\n", msgaddr[0]);
#endif

            /*
             * prepare to reject the message, NACK
             */
            SET_SBIC_cmd(regs, SBIC_CMD_SET_ATN);
            WAIT_CIP(regs);
        }

        /*
         * Negate ACK to complete the transfer
         */
        SET_SBIC_cmd(regs, SBIC_CMD_CLR_ACK);

        /*
         * Wait for the interrupt for the next byte, or phase change.
         * Only read the CSR if we have more data to transfer.
         * XXXSCW: We should really verify that we're still in MSG IN phase
         * before blindly going back around this loop, but that would mean
         * we read the CSR... <sigh>
         */
        SBIC_WAIT(regs, SBIC_ASR_INT, 0);
        if ( recvlen > 0 )
            GET_SBIC_csr(regs, csr);

    } while ( recvlen > 0 );

    /*
     * Should still have one CSR to read
     */
    return SBIC_STATE_RUNNING;
}


/*
 * sbicnextstate()
 * return:
 *      SBIC_STATE_DONE        == done
 *      SBIC_STATE_RUNNING     == working
 *      SBIC_STATE_DISCONNECT  == disconnected
 *      SBIC_STATE_ERROR       == error
 */
int
sbicnextstate(dev, csr, asr)
    struct sbic_softc   *dev;
    u_char              csr,
                        asr;
{
    sbic_regmap_p       regs = dev->sc_sbicp;
    struct sbic_acb     *acb = dev->sc_nexus;

    QPRINTF(("next[%02x,%02x]: ",asr,csr));

    switch (csr) {

      case SBIC_CSR_XFERRED | CMD_PHASE:
      case SBIC_CSR_MIS     | CMD_PHASE:
      case SBIC_CSR_MIS_1   | CMD_PHASE:
      case SBIC_CSR_MIS_2   | CMD_PHASE:
        {
            if ( sbicxfout(regs, acb->clen, &acb->cmd) )
                goto abort;
        }
        break;

      case SBIC_CSR_XFERRED | STATUS_PHASE:
      case SBIC_CSR_MIS     | STATUS_PHASE:
      case SBIC_CSR_MIS_1   | STATUS_PHASE:
      case SBIC_CSR_MIS_2   | STATUS_PHASE:
        {
            SET_SBIC_control(regs, SBIC_CTL_EDI | SBIC_CTL_IDI);

            /*
             * this should be the normal i/o completion case.
             * get the status & cmd complete msg then let the
             * device driver look at what happened.
             */
            sbicxfdone(dev);

#ifdef DEBUG
            dev->sc_dmatimo = 0;
            if ( data_pointer_debug > 1 )
                printf("next dmastop: %d(%x:%x)\n", dev->target,
                                                    dev->sc_cur->dc_addr,
                                                    dev->sc_tcnt);
#endif
            /*
             * Stop the DMA chip
             */
            dev->sc_dmastop(dev);

            dev->sc_flags &= ~(SBICF_INDMA | SBICF_DCFLUSH);

            /*
             * Indicate to the upper layers that the command is done
             */
            sbic_scsidone(acb, dev->sc_stat[0]);

            return SBIC_STATE_DONE;
        }

      case SBIC_CSR_XFERRED | DATA_OUT_PHASE:
      case SBIC_CSR_XFERRED | DATA_IN_PHASE:
      case SBIC_CSR_MIS     | DATA_OUT_PHASE:
      case SBIC_CSR_MIS     | DATA_IN_PHASE:
      case SBIC_CSR_MIS_1   | DATA_OUT_PHASE:
      case SBIC_CSR_MIS_1   | DATA_IN_PHASE:
      case SBIC_CSR_MIS_2   | DATA_OUT_PHASE:
      case SBIC_CSR_MIS_2   | DATA_IN_PHASE:
        {
            /*
             * Verify that we expected to transfer data...
             */
            if ( acb->sc_kv.dc_count <= 0 ) {
                printf("next: DATA phase with xfer count == %d, asr:0x%02x csr:0x%02x\n",
                        acb->sc_kv.dc_count, asr, csr);
                goto abort;
            }

            /*
             * Should we transfer using PIO or DMA ?
             */
            if ( dev->sc_xs->flags & SCSI_POLL || dev->sc_flags & SBICF_ICMD ||
                 acb->sc_dmacmd == 0 ) {

                /*
                 * Do PIO transfer
                 */
                int     i;

#ifdef DEBUG
                if ( data_pointer_debug > 1 )
                    printf("next PIO: %d(%x:%x)\n", dev->target,
                                                    acb->sc_kv.dc_addr,
                                                    acb->sc_kv.dc_count);
#endif

                if ( SBIC_PHASE(csr) == DATA_IN_PHASE )
                    /*
                     * data in
                     */
                    i = sbicxfin(regs, acb->sc_kv.dc_count,
                                       acb->sc_kv.dc_addr);
                else
                    /*
                     * data out
                     */
                    i = sbicxfout(regs, acb->sc_kv.dc_count,
                                        acb->sc_kv.dc_addr);

                acb->sc_kv.dc_addr += (acb->sc_kv.dc_count - i);
                acb->sc_kv.dc_count = i;

                /*
                 * Update current count...
                 */
                acb->sc_tcnt = dev->sc_tcnt = i;

                dev->sc_flags &= ~SBICF_INDMA;

            } else {

                /*
                 * Do DMA transfer
                 * set next dma addr and dec count
                 */
                sbic_save_ptrs(dev);
                sbic_load_ptrs(dev);

                SET_SBIC_control(regs, SBIC_CTL_EDI | SBIC_CTL_IDI |
                                       SBIC_MACHINE_DMA_MODE);

#ifdef DEBUG
                dev->sc_dmatimo = 1;
                if ( data_pointer_debug > 1 )
                    printf("next DMA: %d(%x:%x)\n", dev->target,
                                                    dev->sc_cur->dc_addr,
                                                    dev->sc_tcnt);
#endif
                /*
                 * Start the DMA chip going
                 */
                dev->sc_tcnt = dev->sc_dmanext(dev);

                /*
                 * Tell the WD chip how much to transfer this time around
                 */
                SBIC_TC_PUT(regs, (unsigned)dev->sc_tcnt);

                /*
                 * Start the transfer
                 */
                SET_SBIC_cmd(regs, SBIC_CMD_XFER_INFO);

                /*
                 * Indicate that we're in DMA mode
                 */
                dev->sc_flags |= SBICF_INDMA;
            }
        }
        break;

      case SBIC_CSR_XFERRED | MESG_IN_PHASE:
      case SBIC_CSR_MIS     | MESG_IN_PHASE:
      case SBIC_CSR_MIS_1   | MESG_IN_PHASE:
      case SBIC_CSR_MIS_2   | MESG_IN_PHASE:
        {
            sbic_save_ptrs(dev);

            /*
             * Handle a single message in...
             */
            return sbicmsgin(dev);
        }

      case SBIC_CSR_MSGIN_W_ACK:
        {
            /*
             * We should never see this since it's handled in 'sbicmsgin()'
             * but just for the sake of paranoia...
             */
            SET_SBIC_cmd(regs, SBIC_CMD_CLR_ACK); /* Dunno what I'm ACKing */
            printf("Acking unknown msgin CSR:%02x",csr);
        }
        break;

      case SBIC_CSR_XFERRED | MESG_OUT_PHASE:
      case SBIC_CSR_MIS     | MESG_OUT_PHASE:
      case SBIC_CSR_MIS_1   | MESG_OUT_PHASE:
      case SBIC_CSR_MIS_2   | MESG_OUT_PHASE:
        {
            /*
             * We only ever handle a message out phase here for sending a
             * REJECT message.
             */
            sbic_save_ptrs(dev);

#ifdef DEBUG
            if (sync_debug)
                printf ("sending REJECT msg to last msg.\n");
#endif

            SEND_BYTE(regs, MSG_REJECT);
            WAIT_CIP(regs);
        }
        break;

      case SBIC_CSR_DISC:
      case SBIC_CSR_DISC_1:
        {
            /*
             * Try to schedule another target
             */
            sbic_save_ptrs(dev);

            dev->sc_flags &= ~SBICF_SELECTED;

#ifdef DEBUG
            if ( reselect_debug > 1 )
                printf("sbicnext target %d disconnected\n", dev->target);
#endif

            TAILQ_INSERT_HEAD(&dev->nexus_list, acb, chain);

            ++dev->sc_tinfo[dev->target].dconns;

            dev->sc_nexus = NULL;
            dev->sc_xs    = NULL;

            if ( acb->xs->flags & SCSI_POLL || dev->sc_flags & SBICF_ICMD ||
                                               !sbic_parallel_operations )
                return SBIC_STATE_DISCONNECT;

            QPRINTF(("sbicnext: calling sbic_sched\n"));

            sbic_sched(dev);

            QPRINTF(("sbicnext: sbic_sched returned\n"));

            return SBIC_STATE_DISCONNECT;
        }

      case SBIC_CSR_RSLT_NI:
      case SBIC_CSR_RSLT_IFY:
        {
            /*
             * A reselection.
             * Note that since we don't enable Advanced Features (assuming
             * the WD chip is at least the 'A' revision), we're only ever
             * likely to see the 'SBIC_CSR_RSLT_NI' status. But for the
             * hell of it, we'll handle it anyway, for all the extra code
             * it needs...
             */
            u_char  newtarget,
                    newlun;

            GET_SBIC_rselid(regs, newtarget);

            /*
             * check SBIC_RID_SIV?
             */
            newtarget &= SBIC_RID_MASK;

            if ( csr == SBIC_CSR_RSLT_IFY ) {

                /*
                 * Read Identify msg to avoid lockup
                 */
                GET_SBIC_data(regs, newlun);
                WAIT_CIP(regs);
                newlun &= SBIC_TLUN_MASK;

            } else {

                /*
                 * Need to read Identify message the hard way, assuming
                 * the target even sends us one...
                 */
                for (newlun = 255; newlun; --newlun) {
                    GET_SBIC_asr(regs, asr);
                    if (asr & SBIC_ASR_INT)
                        break;
                    delay(10);
                }

                /*
                 * If we didn't get an interrupt, somethink's up
                 */
                if ( (asr & SBIC_ASR_INT) == 0 ) {
                    printf("%s: Reselect without identify? asr %x\n",
                            dev->sc_dev.dv_xname, asr);
                    newlun = 0; /* XXXX */
                } else {
                    /*
                     * We got an interrupt, verify that it's a change to
                     * message in phase, and if so read the message.
                     */
                    GET_SBIC_csr(regs,csr);

                    if (csr == (SBIC_CSR_MIS   | MESG_IN_PHASE) ||
                        csr == (SBIC_CSR_MIS_1 | MESG_IN_PHASE) ||
                        csr == (SBIC_CSR_MIS_2 | MESG_IN_PHASE)) {
                        /*
                         * Yup, gone to message in. Fetch the target LUN
                         */
                        sbicmsgin(dev);
                        newlun = dev->sc_msg[0] & 0x07;

                    } else {
                        /*
                         * Whoops! Target didn't go to message in phase!!
                         */
                        printf("RSLT_NI - not MESG_IN_PHASE %x\n", csr);
                        newlun = 0; /* XXXSCW */
                    }
                }
            }

            /*
             * Ok, we have the identity of the reselecting target.
             */
#ifdef DEBUG
            if ( reselect_debug > 1 ||
                (reselect_debug && csr == SBIC_CSR_RSLT_NI) ) {
                printf("sbicnext: reselect %s from targ %d lun %d\n",
                        csr == SBIC_CSR_RSLT_NI ? "NI" : "IFY", newtarget, newlun);
            }
#endif

            if ( dev->sc_nexus ) {
                /*
                 * Whoops! We've been reselected with an command in progress!
                 * The best we can do is to put the current command back on the
                 * ready list and hope for the best.
                 */
#ifdef DEBUG
                if ( reselect_debug > 1 ) {
                    printf("%s: reselect %s with active command\n",
                        dev->sc_dev.dv_xname,
                        csr == SBIC_CSR_RSLT_NI ? "NI" : "IFY");
                }
#endif

                TAILQ_INSERT_HEAD(&dev->ready_list, dev->sc_nexus, chain);

                dev->sc_tinfo[dev->target].lubusy &= ~(1 << dev->lun);

                dev->sc_nexus = NULL;
                dev->sc_xs    = NULL;
            }

            /*
             * Reload sync values for this target
             */
            if ( dev->sc_sync[newtarget].state == SYNC_DONE )
                SET_SBIC_syn(regs, SBIC_SYN (dev->sc_sync[newtarget].offset,
                                             dev->sc_sync[newtarget].period));
            else
                SET_SBIC_syn(regs, SBIC_SYN (0, sbic_min_period));

            /*
             * Loop through the nexus list until we find the saved entry
             * for the reselecting target...
             */
	    TAILQ_FOREACH(acb, &dev->nexus_list, chain) {

                if ( acb->xs->sc_link->target == newtarget &&
                     acb->xs->sc_link->lun    == newlun) {
                    /*
                     * We've found the saved entry. Dequeue it, and 
                     * make it current again.
                     */
                    TAILQ_REMOVE(&dev->nexus_list, acb, chain);

                    dev->sc_nexus  = acb;
                    dev->sc_xs     = acb->xs;
                    dev->sc_flags |= SBICF_SELECTED;
                    dev->target    = newtarget;
                    dev->lun       = newlun;
                    break;
                }
            }

            if ( acb == NULL ) {
                printf("%s: reselect %s targ %d not in nexus_list %p\n",
                        dev->sc_dev.dv_xname,
                        csr == SBIC_CSR_RSLT_NI ? "NI" : "IFY", newtarget,
                        &TAILQ_FIRST(&dev->nexus_list));
                panic("bad reselect in sbic");
            }

            if ( csr == SBIC_CSR_RSLT_IFY )
                SET_SBIC_cmd(regs, SBIC_CMD_CLR_ACK);
        }
        break;

      default:
        abort:
        {
            /*
             * Something unexpected happened -- deal with it.
             */
            printf("next: aborting asr 0x%02x csr 0x%02x\n", asr, csr);

#ifdef DDB
            Debugger();
#endif

#ifdef DEBUG
            dev->sc_dmatimo = 0;
            if ( data_pointer_debug > 1 )
                printf("next dmastop: %d(%x:%x)\n", dev->target,
                                                    dev->sc_cur->dc_addr,
                                                    dev->sc_tcnt);
#endif

            dev->sc_dmastop(dev);
            SET_SBIC_control(regs, SBIC_CTL_EDI | SBIC_CTL_IDI);
            if ( dev->sc_xs ) sbicerror(dev, csr);
            sbicabort(dev, "next");

            if ( dev->sc_flags & SBICF_INDMA ) {
                dev->sc_flags &= ~(SBICF_INDMA | SBICF_DCFLUSH);

#ifdef DEBUG
                dev->sc_dmatimo = 0;
                if ( data_pointer_debug > 1 )
                    printf("next dmastop: %d(%x:%x)\n", dev->target,
                                                        dev->sc_cur->dc_addr,
                                                        dev->sc_tcnt);
#endif
                sbic_scsidone(acb, -1);
            }

            return SBIC_STATE_ERROR;
        }
    }

    return(SBIC_STATE_RUNNING);
}


/*
 * Check if DMA can not be used with specified buffer
 */
int
sbiccheckdmap(bp, len, mask)
    void    *bp;
    u_long  len,
            mask;
{
    u_char  *buffer;
    u_long  phy_buf;
    u_long  phy_len;

    buffer = bp;

    if ( len == 0 )
        return(1);

    while ( len ) {

        phy_buf = kvtop((vaddr_t)buffer);
        phy_len = NBPG - ((int) buffer & PGOFSET);

        if ( len < phy_len )
            phy_len = len;

        if ( phy_buf & mask )
            return(1);

        buffer += phy_len;
        len    -= phy_len;
    }

    return(0);
}

int
sbictoscsiperiod(dev, a)
    struct sbic_softc   *dev;
    int                 a;
{
    unsigned int fs;

    /*
     * cycle = DIV / (2 * CLK)
     * DIV = FS + 2
     * best we can do is 200ns at 20MHz, 2 cycles
     */

    GET_SBIC_myid(dev->sc_sbicp, fs);

    fs = (fs >> 6) + 2;         /* DIV */

    fs = (fs * 10000) / (dev->sc_clkfreq << 1); /* Cycle, in ns */

    if ( a < 2 )
        a = 8;                  /* map to Cycles */

    return ( (fs * a) >> 2 );   /* in 4 ns units */
}

int
sbicfromscsiperiod(dev, p)
    struct sbic_softc   *dev;
    int                 p;
{
    unsigned    fs,
                ret;

    /*
     * Just the inverse of the above
     */
    GET_SBIC_myid(dev->sc_sbicp, fs);

    fs = (fs >> 6) + 2;     /* DIV */

    fs = (fs * 10000) / (dev->sc_clkfreq << 1); /* Cycle, in ns */

    ret = p << 2;           /* in ns units */
    ret = ret / fs;         /* in Cycles */

    if ( ret < sbic_min_period )
        return(sbic_min_period);

    /*
     * verify rounding
     */
    if ( sbictoscsiperiod(dev, ret) < p )
        ret++;

    return( (ret >= 8) ? 0 : ret );
}

#ifdef DEBUG
void
sbictimeout(dev)
    struct sbic_softc   *dev;
{
    int     s,
            asr;

    s = splbio();

    if ( dev->sc_dmatimo ) {

        if ( dev->sc_dmatimo > 1 ) {

            printf("%s: dma timeout #%d\n", dev->sc_dev.dv_xname,
                                            dev->sc_dmatimo - 1);

            GET_SBIC_asr(dev->sc_sbicp, asr);

            if ( asr & SBIC_ASR_INT ) {
                /*
                 * We need to service a missed IRQ
                 */
                sbicintr(dev);
            } else {
                (void) sbicabort(dev, "timeout");
                splx(s);
                return;
            }
        }

        dev->sc_dmatimo++;
    }

    splx(s);

    timeout((void *)sbictimeout, dev, 30 * hz);
}
#endif
