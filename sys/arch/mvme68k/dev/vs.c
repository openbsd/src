/*	$OpenBSD: vs.c,v 1.1 2000/01/25 04:18:17 smurph Exp $ */

/*
 * Copyright (c) 1999 Steve Murphree, Jr.
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
 */

/*
 * MVME328 scsi adaptor driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/dkstat.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <machine/autoconf.h>
#include <machine/param.h>

#define PAGESIZE 4096  /* should get this out of a header? XXX - smurph */
#if defined(MVME187) || defined(MVME188) || defined(MVME197)
   #include <mvme88k/dev/vsreg.h>
   #include <mvme88k/dev/vsvar.h>
   #include "machine/mmu.h"
   #define ROUND_PAGE m88k_round_page
   #define TRUNC_PAGE m88k_trunc_page
#else
   #include <mvme68k/dev/vsreg.h>
   #include <mvme68k/dev/vsvar.h>
   #define ROUND_PAGE m68k_round_page
   #define TRUNC_PAGE m68k_trunc_page
#endif /* defined(MVME187) || defined(MVME188) || defined(MVME197) */

int  vs_checkintr        __P((struct vs_softc *, struct scsi_xfer *, int *));
int  vs_chksense         __P((struct scsi_xfer *));
void vs_reset            __P((struct vs_softc *));
void vs_resync           __P((struct vs_softc *));
void vs_initialize       __P((struct vs_softc *));
int  vs_intr             __P((struct vs_softc *));
int  vs_poll             __P((struct vs_softc *, struct scsi_xfer *));
void vs_scsidone         __P((struct scsi_xfer *, int));
M328_CQE  * vs_getcqe    __P((struct vs_softc *));
M328_IOPB * vs_getiopb   __P((struct vs_softc *));

extern int cold;
extern u_int   kvtop();
/* 
 * 16 bit 's' memory functions.  MVME328 is a D16 board.
 * We must program with that in mind or else...
 * bcopy/bzero (the 'b' meaning byte) is implemented in 
 * 32 bit operations for speed, so thay are not really 
 * 'byte' operations at all!!  MVME1x7 can be set up to 
 * handle D32 -> D16 read/writes via VMEChip2 Address 
 * modifiers, however MVME188 can not.  These next two
 * functions insure 16 bit copy/zero operations.  The 
 * structures are all implemented with 16 bit or less
 * types.   smurph
 */

void
scopy(void *src, void *dst, unsigned short cnt) 
{ 
   register unsigned short volatile *x, *y, z; 

   z = cnt >> 1; 
   x = (unsigned short *) src; 
   y = (unsigned short *) dst; 

   while (z--) {
      *y++ = *x++; 
   }
}

void
szero(void *src, u_long cnt)
{
   register unsigned short *source;
   register unsigned short zero = 0;
   register unsigned short z; 

   source = (unsigned short *) src;
   z = cnt >> 1; 

   while (z--) {
      *source++ = zero;
   }
   return;
}





/*
 * default minphys routine for MVME328 based controllers
 */
void
vs_minphys(bp)
struct buf *bp;
{
   /*
    * No max transfer at this level.
    */
   minphys(bp);
}

int do_vspoll(sc, to)
struct vs_softc *sc;
int to;
{
   int i;
   if (to <= 0 ) to = 50000;
   /* use cmd_wait values? */
   i = 50000;
   /*spl0();*/
   while (!(CRSW & (M_CRSW_CRBV | M_CRSW_CC))) {
      if (--i <= 0) {
#ifdef DEBUG
         printf ("waiting: timeout %d crsw 0x%x\n", to, CRSW);
#endif
         i = 50000;
         --to;
         if (to <= 0) {
            /*splx(s);*/
            vs_reset(sc);
            vs_resync(sc);
            printf ("timed out: timeout %d crsw 0x%x\n", to, CRSW);
            return 1;
         }
      }
   }
   return 0;
}

int
vs_poll(sc, xs)
struct vs_softc *sc;
struct scsi_xfer *xs;
{
   M328_CIB *cib = (M328_CIB *)&sc->sc_vsreg->sh_CIB;
   M328_CQE *mc = (M328_CQE*)&sc->sc_vsreg->sh_MCE;
   M328_CRB *crb = (M328_CRB *)&sc->sc_vsreg->sh_CRB;
   M328_IOPB *riopb = (M328_IOPB *)&sc->sc_vsreg->sh_RET_IOPB;
   M328_IOPB *miopb = (M328_IOPB *)&sc->sc_vsreg->sh_MCE_IOPB;
   M328_MCSB *mcsb = (M328_MCSB *)&sc->sc_vsreg->sh_MCSB;
   M328_CQE *cqep;
   M328_IOPB *iopb;
   int i;
   int status;
   int s;
   int to;

   /*s = splbio();*/
   to = xs->timeout / 1000;
   for (;;) {
      if (do_vspoll(sc, to)) break;
      if (vs_checkintr(sc, xs, &status)) {
         vs_scsidone(xs, status);
      }
      if (CRSW & M_CRSW_ER)
         CRB_CLR_ER(CRSW);
      CRB_CLR_DONE(CRSW);
      if (xs->flags & ITSDONE) break;
   }
   return (COMPLETE);
}

void thaw_queue(sc, target)
struct vs_softc *sc;
u_int8_t target;
{
   u_short t;
   t = target << 8;
   t |= 0x0001;
   THAW_REG = t;
   /* loop until thawed */
   while (THAW_REG & 0x01);
}

void 
vs_scsidone (xs, stat)
struct scsi_xfer *xs;
int stat;                             
{
   struct scsi_link *slp = xs->sc_link;
   struct vs_softc *sc = slp->adapter_softc;
   M328_IOPB *riopb = (M328_IOPB *)&sc->sc_vsreg->sh_RET_IOPB;
   xs->status = stat;
   while (xs->status == SCSI_CHECK) {
      vs_chksense(xs);
      thaw_queue(sc, slp->target + 1);
   }
   xs->flags |= ITSDONE;
   /*sc->sc_tinfo[slp->target].cmds++;*/
   if (CRSW & M_CRSW_ER)
      CRB_CLR_ER(CRSW);
   CRB_CLR_DONE(CRSW);
   thaw_queue(sc, slp->target + 1);
   szero(riopb, sizeof(M328_IOPB));
   scsi_done(xs);
}

int
vs_scsicmd(xs)
struct scsi_xfer *xs;
{
   struct scsi_link *slp = xs->sc_link;
   struct vs_softc *sc = slp->adapter_softc;
   int flags, s, i;
   unsigned long buf, len;
   u_short iopb_len;
   M328_CQE *mc = (M328_CQE*)&sc->sc_vsreg->sh_MCE;
   M328_CRB *crb = (M328_CRB *)&sc->sc_vsreg->sh_CRB;
   M328_IOPB *riopb = (M328_IOPB *)&sc->sc_vsreg->sh_RET_IOPB;
   M328_IOPB *miopb = (M328_IOPB *)&sc->sc_vsreg->sh_MCE_IOPB;
   M328_MCSB *mcsb = (M328_MCSB *)&sc->sc_vsreg->sh_MCSB;
   M328_CQE *cqep;
   M328_IOPB *iopb;
   M328_CMD *m328_cmd;

   /* If the target doesn't exist, abort */
   if (!sc->sc_tinfo[slp->target].avail) {
      xs->error = XS_SELTIMEOUT;
      xs->status = -1;
      xs->flags |= ITSDONE;
      scsi_done(xs);
   }

   slp->quirks |= SDEV_NOLUNS;
   flags = xs->flags;
#ifdef SDEBUG
   printf("scsi_cmd() ");
   if (xs->cmd->opcode == 0) {
      printf("TEST_UNIT_READY ");
   } else if (xs->cmd->opcode == REQUEST_SENSE) {
      printf("REQUEST_SENSE   ");
   } else if (xs->cmd->opcode == INQUIRY) {
      printf("INQUIRY         ");
   } else if (xs->cmd->opcode == MODE_SELECT) {
      printf("MODE_SELECT     ");
   } else if (xs->cmd->opcode == MODE_SENSE) {
      printf("MODE_SENSE      ");
   } else if (xs->cmd->opcode == START_STOP) {
      printf("START_STOP      ");
   } else if (xs->cmd->opcode == RESERVE) {
      printf("RESERVE         ");
   } else if (xs->cmd->opcode == RELEASE) {
      printf("RELEASE         ");
   } else if (xs->cmd->opcode == PREVENT_ALLOW) {
      printf("PREVENT_ALLOW   ");
   } else if (xs->cmd->opcode == POSITION_TO_ELEMENT) {
      printf("POSITION_TO_EL  ");
   } else if (xs->cmd->opcode == CHANGE_DEFINITION) {
      printf("CHANGE_DEF      ");
   } else if (xs->cmd->opcode == MODE_SENSE_BIG) {
      printf("MODE_SENSE_BIG  ");
   } else if (xs->cmd->opcode == MODE_SELECT_BIG) {
      printf("MODE_SELECT_BIG ");
   } else if (xs->cmd->opcode == 0x25) {
      printf("READ_CAPACITY   ");
   } else if (xs->cmd->opcode == 0x08) {
      printf("READ_COMMAND    ");
   }
#endif 
   if (flags & SCSI_POLL) {
      cqep = mc;
      iopb = miopb;
   } else {
      cqep = vs_getcqe(sc);
      iopb = vs_getiopb(sc);
   }
   if (cqep == NULL) {
      xs->error = XS_DRIVER_STUFFUP;
      return (TRY_AGAIN_LATER);
   }

/*	s = splbio();*/
   iopb_len = sizeof(M328_short_IOPB) + xs->cmdlen;
   szero(iopb, sizeof(M328_IOPB));

   scopy(xs->cmd, &iopb->iopb_SCSI[0], xs->cmdlen);
   iopb->iopb_CMD = IOPB_SCSI;
#if 0
   LV(iopb->iopb_BUFF, kvtop(xs->data));
   LV(iopb->iopb_LENGTH, xs->datalen);
#endif   
   iopb->iopb_UNIT = slp->lun << 3;
   iopb->iopb_UNIT |= slp->target;
   iopb->iopb_NVCT = (u_char)sc->sc_nvec;
   iopb->iopb_EVCT = (u_char)sc->sc_evec;

   /*
    * Since the 88k's don't support cache snooping, we have
    * to flush the cache for a write and flush with inval for
    * a read, prior to starting the IO.
    */
   if (xs->flags & SCSI_DATA_IN) {  /* read */
#if defined(MVME187) || defined(MVME188) || defined(MVME197)
      dma_cachectl((vm_offset_t)xs->data, xs->datalen,
                   DMA_CACHE_SYNC_INVAL);
#endif      
      iopb->iopb_OPTION |= OPT_READ;
   } else {                         /* write */
#if defined(MVME187)
      dma_cachectl((vm_offset_t)xs->data, xs->datalen,
                   DMA_CACHE_SYNC);
#endif      
      iopb->iopb_OPTION |= OPT_WRITE;
   }

   if (flags & SCSI_POLL) {
      iopb->iopb_OPTION |= OPT_INTDIS;
      iopb->iopb_LEVEL = 0;
   } else {
      iopb->iopb_OPTION |= OPT_INTEN;
      iopb->iopb_LEVEL = sc->sc_ipl;
   }
   iopb->iopb_ADDR = ADDR_MOD;

   /*
    * Wait until we can use the command queue entry.
    * Should only have to wait if the master command 
    * queue entry is busy.
    */
   while (cqep->cqe_QECR & M_QECR_GO);

   cqep->cqe_IOPB_ADDR = OFF(iopb);
   cqep->cqe_IOPB_LENGTH = iopb_len;
   if (flags & SCSI_POLL) {
      cqep->cqe_WORK_QUEUE = slp->target + 1;
   } else {
      cqep->cqe_WORK_QUEUE = slp->target + 1;
   }
   
   MALLOC(m328_cmd, M328_CMD*, sizeof(M328_CMD), M_DEVBUF, M_WAITOK);
   
   m328_cmd->xs = xs;
   if (xs->datalen) {
      m328_cmd->top_sg_list = vs_build_memory_structure(xs, iopb);
   } else {
      m328_cmd->top_sg_list = (M328_SG)0;
   }

   LV(cqep->cqe_CTAG, m328_cmd);

   if (crb->crb_CRSW & M_CRSW_AQ) {
      cqep->cqe_QECR = M_QECR_AA;
   }
   VL(buf, iopb->iopb_BUFF);
   VL(len, iopb->iopb_LENGTH);
#ifdef SDEBUG
   printf("tgt %d lun %d buf %x len %d wqn %d ipl %d\n", slp->target, 
          slp->lun, buf, len, cqep->cqe_WORK_QUEUE, iopb->iopb_LEVEL);
#endif 
   cqep->cqe_QECR |= M_QECR_GO;

   if (flags & SCSI_POLL) {
      /* poll for the command to complete */
/*      splx(s);*/
      vs_poll(sc, xs);
      return (COMPLETE);
   }
/*	splx(s);*/
   return (SUCCESSFULLY_QUEUED);
}

int
vs_chksense(xs)
struct scsi_xfer *xs;
{
   int flags, s, i;
   struct scsi_link *slp = xs->sc_link;
   struct vs_softc *sc = slp->adapter_softc;
   struct scsi_sense *ss;
   M328_CQE *mc = (M328_CQE*)&sc->sc_vsreg->sh_MCE;
   M328_IOPB *riopb = (M328_IOPB *)&sc->sc_vsreg->sh_RET_IOPB;
   M328_IOPB *miopb = (M328_IOPB *)&sc->sc_vsreg->sh_MCE_IOPB;

   /* ack and clear the error */
   CRB_CLR_DONE(CRSW);
   CRB_CLR_ER(CRSW);
   xs->status = 0;

   szero(miopb, sizeof(M328_IOPB));
   /* This is a command, so point to it */
   ss = (void *)&miopb->iopb_SCSI[0];
   szero(ss, sizeof(*ss));
   ss->opcode = REQUEST_SENSE;
   ss->byte2 = slp->lun << 5;
   ss->length = sizeof(struct scsi_sense_data);

   miopb->iopb_CMD = IOPB_SCSI;
   miopb->iopb_OPTION = OPT_READ;
   miopb->iopb_NVCT = (u_char)sc->sc_nvec;
   miopb->iopb_EVCT = (u_char)sc->sc_evec;
   miopb->iopb_LEVEL = 0; /*sc->sc_ipl;*/
   miopb->iopb_ADDR = ADDR_MOD;
   LV(miopb->iopb_BUFF, kvtop(&xs->sense));
   LV(miopb->iopb_LENGTH, sizeof(struct scsi_sense_data));

   szero(mc, sizeof(M328_CQE));
   mc->cqe_IOPB_ADDR = OFF(miopb);
   mc->cqe_IOPB_LENGTH = sizeof(M328_short_IOPB) + sizeof(struct scsi_sense);
   mc->cqe_WORK_QUEUE = 0;
   mc->cqe_QECR = M_QECR_GO;
   /* poll for the command to complete */
   s = splbio();
   do_vspoll(sc, 0);
   /*
   if (xs->cmd->opcode != PREVENT_ALLOW) {
      xs->error = XS_SENSE;
   }
   */
   xs->status = riopb->iopb_STATUS >> 8;
#ifdef SDEBUG
   scsi_print_sense(xs, 2);
#endif   
   splx(s);
}

M328_CQE *
vs_getcqe(sc)
struct vs_softc *sc;
{
   M328_MCSB *mcsb = (M328_MCSB *)&sc->sc_vsreg->sh_MCSB;
   M328_CQE *cqep;

   cqep = (M328_CQE *)&sc->sc_vsreg->sh_CQE[mcsb->mcsb_QHDP];

   if (cqep->cqe_QECR & M_QECR_GO)
      return NULL; /* Hopefully, this will never happen */
   mcsb->mcsb_QHDP++;
   if (mcsb->mcsb_QHDP == NUM_CQE) mcsb->mcsb_QHDP = 0;
   szero(cqep, sizeof(M328_CQE));
   return cqep;
}

M328_IOPB *
vs_getiopb(sc)
struct vs_softc *sc;
{
   M328_MCSB *mcsb = (M328_MCSB *)&sc->sc_vsreg->sh_MCSB;
   M328_IOPB *iopb;
   int slot;

   if (mcsb->mcsb_QHDP == 0) {
      slot = NUM_CQE;
   } else {
      slot = mcsb->mcsb_QHDP - 1;
   }
   iopb = (M328_IOPB *)&sc->sc_vsreg->sh_IOPB[slot];
   szero(iopb, sizeof(M328_IOPB));
   return iopb;
}

void
vs_initialize(sc)
struct vs_softc *sc;
{
   M328_CIB *cib = (M328_CIB *)&sc->sc_vsreg->sh_CIB;
   M328_CQE *mc = (M328_CQE*)&sc->sc_vsreg->sh_MCE;
   M328_CRB *crb = (M328_CRB *)&sc->sc_vsreg->sh_CRB;
   M328_IOPB *riopb = (M328_IOPB *)&sc->sc_vsreg->sh_RET_IOPB;
   M328_MCSB *mcsb = (M328_MCSB *)&sc->sc_vsreg->sh_MCSB;
   M328_IOPB *iopb;
   M328_WQCF *wiopb = (M328_WQCF *)&sc->sc_vsreg->sh_MCE_IOPB;
   u_short i, crsw;
   int failed = 0;

   CRB_CLR_DONE(CRSW);
   szero(cib, sizeof(M328_CIB));
   mcsb->mcsb_QHDP = 0;
   sc->sc_qhp = 0;
   cib->cib_NCQE = 10;
   cib->cib_BURST = 0;
   cib->cib_NVECT = sc->sc_ipl << 8;
   cib->cib_NVECT |= sc->sc_nvec;
   cib->cib_EVECT = sc->sc_ipl << 8;
   cib->cib_EVECT |= sc->sc_evec;
   cib->cib_PID = 0x07;
   cib->cib_SID = 0x00;
   cib->cib_CRBO = OFF(crb);
   cib->cib_SELECT_msw = HI(SELECTION_TIMEOUT);
   cib->cib_SELECT_lsw = LO(SELECTION_TIMEOUT);
   cib->cib_WQ0TIMO_msw = HI(4);
   cib->cib_WQ0TIMO_lsw = LO(4);
   cib->cib_VMETIMO_msw = 0; /*HI(VME_BUS_TIMEOUT);*/
   cib->cib_VMETIMO_lsw = 0; /*LO(VME_BUS_TIMEOUT);*/
   cib->cib_SBRIV = sc->sc_ipl << 8;
   cib->cib_SBRIV |= sc->sc_evec;
   cib->cib_SOF0 = 0x15;
   cib->cib_SRATE0 = 100/4;
   cib->cib_SOF1 = 0x0;
   cib->cib_SRATE1 = 0x0;

   iopb = (M328_IOPB *)&sc->sc_vsreg->sh_MCE_IOPB;
   szero(iopb, sizeof(M328_IOPB));
   iopb->iopb_CMD = CNTR_INIT;
   iopb->iopb_OPTION = 0;
   iopb->iopb_NVCT = (u_char)sc->sc_nvec;
   iopb->iopb_EVCT = (u_char)sc->sc_evec;
   iopb->iopb_LEVEL = 0; /*sc->sc_ipl;*/
   iopb->iopb_ADDR = SHIO_MOD;
   LV(iopb->iopb_BUFF, OFF(cib));
   LV(iopb->iopb_LENGTH, sizeof(M328_CIB));

   szero(mc, sizeof(M328_CQE));
   mc->cqe_IOPB_ADDR = OFF(iopb);
   mc->cqe_IOPB_LENGTH = sizeof(M328_IOPB);
   mc->cqe_WORK_QUEUE = 0;
   mc->cqe_QECR = M_QECR_GO;
   /* poll for the command to complete */
   do_vspoll(sc, 0);
   CRB_CLR_DONE(CRSW);

   /* initialize work queues */
   for (i=1; i<8; i++) {
      szero(wiopb, sizeof(M328_IOPB));
      wiopb->wqcf_CMD = CNTR_INIT_WORKQ;
      wiopb->wqcf_OPTION = 0;
      wiopb->wqcf_NVCT = (u_char)sc->sc_nvec;
      wiopb->wqcf_EVCT = (u_char)sc->sc_evec;
      wiopb->wqcf_ILVL = 0; /*sc->sc_ipl;*/
      wiopb->wqcf_WORKQ = i;
      wiopb->wqcf_WOPT = (WQO_FOE | WQO_INIT);
      wiopb->wqcf_SLOTS = JAGUAR_MAX_Q_SIZ;
      LV(wiopb->wqcf_CMDTO, 2);

      szero(mc, sizeof(M328_CQE));
      mc->cqe_IOPB_ADDR = OFF(wiopb);
      mc->cqe_IOPB_LENGTH = sizeof(M328_IOPB);
      mc->cqe_WORK_QUEUE = 0;
      mc->cqe_QECR = M_QECR_GO;
      /* poll for the command to complete */
      do_vspoll(sc, 0);
      if (CRSW & M_CRSW_ER) {
         /*printf("\nerror: queue %d status = 0x%x\n", i, riopb->iopb_STATUS);*/
         /*failed = 1;*/
         CRB_CLR_ER(CRSW);
      }
      CRB_CLR_DONE(CRSW);
      delay(500);
   }
   /* start queue mode */
   CRSW = 0;
   mcsb->mcsb_MCR |= M_MCR_SQM;
   crsw = CRSW;
   do_vspoll(sc, 0);
   if (CRSW & M_CRSW_ER) {
      printf("error: status = 0x%x\n", riopb->iopb_STATUS);
      CRB_CLR_ER(CRSW);
   }
   CRB_CLR_DONE(CRSW);

   if (failed) {
      printf(": failed!\n");
      return;
   }
   /* reset SCSI bus */
   vs_reset(sc);
   /* sync all devices */
   vs_resync(sc);
   printf(": target %d\n", sc->sc_link.adapter_target);
}

void
vs_resync(sc)
struct vs_softc *sc;
{
   M328_CQE *mc = (M328_CQE*)&sc->sc_vsreg->sh_MCE;
   M328_IOPB *riopb = (M328_IOPB *)&sc->sc_vsreg->sh_RET_IOPB;
   M328_DRCF *devreset = (M328_DRCF *)&sc->sc_vsreg->sh_MCE_IOPB;  
   u_short i;

   for (i=0; i<7; i++) {
      szero(devreset, sizeof(M328_DRCF));
      devreset->drcf_CMD = CNTR_DEV_REINIT;
      devreset->drcf_OPTION = 0x00;       /* no interrupts yet... */
      devreset->drcf_NVCT = sc->sc_nvec;
      devreset->drcf_EVCT = sc->sc_evec;
      devreset->drcf_ILVL = 0;
      devreset->drcf_UNIT = i;

      szero(mc, sizeof(M328_CQE));
      mc->cqe_IOPB_ADDR = OFF(devreset);
      mc->cqe_IOPB_LENGTH = sizeof(M328_DRCF);
      mc->cqe_WORK_QUEUE = 0;
      mc->cqe_QECR = M_QECR_GO;
      /* poll for the command to complete */
      do_vspoll(sc, 0);
      if (riopb->iopb_STATUS) {
         sc->sc_tinfo[i].avail = 0;
      } else {
         sc->sc_tinfo[i].avail = 1;
      }
      if (CRSW & M_CRSW_ER) {
         CRB_CLR_ER(CRSW);
      }
      CRB_CLR_DONE(CRSW);
   }
}

void
vs_reset(sc)
struct vs_softc *sc;
{
   struct vsreg * rp;
   u_int s;
   u_char  i;
   struct iopb_reset* iopr;
   struct cqe *cqep;
   struct iopb_scsi *iopbs;
   struct scsi_sense *ss;
   M328_CIB *cib = (M328_CIB *)&sc->sc_vsreg->sh_CIB;
   M328_CQE *mc = (M328_CQE*)&sc->sc_vsreg->sh_MCE;
   M328_CRB *crb = (M328_CRB *)&sc->sc_vsreg->sh_CRB;
   M328_IOPB *riopb = (M328_IOPB *)&sc->sc_vsreg->sh_RET_IOPB;
   M328_MCSB *mcsb = (M328_MCSB *)&sc->sc_vsreg->sh_MCSB;
   M328_SRCF *reset = (M328_SRCF *)&sc->sc_vsreg->sh_MCE_IOPB;  
   M328_IOPB *iopb;

   szero(reset, sizeof(M328_SRCF));
   reset->srcf_CMD = IOPB_RESET;
   reset->srcf_OPTION = 0x00;       /* no interrupts yet... */
   reset->srcf_NVCT = sc->sc_nvec;
   reset->srcf_EVCT = sc->sc_evec;
   reset->srcf_ILVL = 0;
   reset->srcf_BUSID = 0;
   s = splbio();

   szero(mc, sizeof(M328_CQE));
   mc->cqe_IOPB_ADDR = OFF(reset);
   mc->cqe_IOPB_LENGTH = sizeof(M328_SRCF);
   mc->cqe_WORK_QUEUE = 0;
   mc->cqe_QECR = M_QECR_GO;
   /* poll for the command to complete */
   while (1) {
      do_vspoll(sc, 0);
      /* ack & clear scsi error condition cause by reset */
      if (CRSW & M_CRSW_ER) {
         CRB_CLR_ER(CRSW);
         CRB_CLR_DONE(CRSW);
         riopb->iopb_STATUS = 0;
         break;
      }
      CRB_CLR_DONE(CRSW);
   }
   /* thaw all work queues */
   thaw_queue(sc, 0xFF);
   splx (s);
}


/*
 * Process an interrupt from the MVME328
 * We'll generally update: xs->{flags,resid,error,sense,status} and
 * occasionally xs->retries.
 */

int
vs_checkintr(sc, xs, status)
struct   vs_softc *sc;
struct scsi_xfer *xs;
int   *status;
{
   struct vsreg * rp = sc->sc_vsreg;
   int   target = -1;
   int   lun = -1;
   M328_CRB *crb = (M328_CRB *)&sc->sc_vsreg->sh_CRB;
   M328_IOPB *riopb = (M328_IOPB *)&sc->sc_vsreg->sh_RET_IOPB;
   struct scsi_generic *cmd;
   u_long buf;
   u_long len;
   u_char error;

   target = xs->sc_link->target;
   lun = xs->sc_link->lun;
   cmd = (struct scsi_generic *)&riopb->iopb_SCSI[0];

   VL(buf, riopb->iopb_BUFF);
   VL(len, riopb->iopb_LENGTH);
   *status = riopb->iopb_STATUS >> 8;
   error = riopb->iopb_STATUS & 0xFF;

#ifdef SDEBUG
   printf("scsi_chk() ");

   if (xs->cmd->opcode == 0) {
      printf("TEST_UNIT_READY ");
   } else if (xs->cmd->opcode == REQUEST_SENSE) {
      printf("REQUEST_SENSE   ");
   } else if (xs->cmd->opcode == INQUIRY) {
      printf("INQUIRY         ");
   } else if (xs->cmd->opcode == MODE_SELECT) {
      printf("MODE_SELECT     ");
   } else if (xs->cmd->opcode == MODE_SENSE) {
      printf("MODE_SENSE      ");
   } else if (xs->cmd->opcode == START_STOP) {
      printf("START_STOP      ");
   } else if (xs->cmd->opcode == RESERVE) {
      printf("RESERVE         ");
   } else if (xs->cmd->opcode == RELEASE) {
      printf("RELEASE         ");
   } else if (xs->cmd->opcode == PREVENT_ALLOW) {
      printf("PREVENT_ALLOW   ");
   } else if (xs->cmd->opcode == POSITION_TO_ELEMENT) {
      printf("POSITION_TO_EL  ");
   } else if (xs->cmd->opcode == CHANGE_DEFINITION) {
      printf("CHANGE_DEF      ");
   } else if (xs->cmd->opcode == MODE_SENSE_BIG) {
      printf("MODE_SENSE_BIG  ");
   } else if (xs->cmd->opcode == MODE_SELECT_BIG) {
      printf("MODE_SELECT_BIG ");
   } else if (xs->cmd->opcode == 0x25) {
      printf("READ_CAPACITY   ");
   } else if (xs->cmd->opcode == 0x08) {
      printf("READ_COMMAND    ");
   }

   printf("tgt %d lun %d buf %x len %d status %x ", target, lun, buf, len, riopb->iopb_STATUS);

   if (CRSW & M_CRSW_EX) {
      printf("[ex]");
   }
   if (CRSW & M_CRSW_QMS) {
      printf("[qms]");
   }
   if (CRSW & M_CRSW_SC) {
      printf("[sc]");
   }
   if (CRSW & M_CRSW_SE) {
      printf("[se]");
   }
   if (CRSW & M_CRSW_AQ) {
      printf("[aq]");
   }
   if (CRSW & M_CRSW_ER) {
      printf("[er]");
   }
   printf("\n");
#endif   
   if (len != xs->datalen) {
      xs->resid = xs->datalen - len;
   } else {
      xs->resid = 0;
   }

   if (error == SCSI_SELECTION_TO) {
      xs->error = XS_SELTIMEOUT;
      xs->status = -1;
      *status = -1;
   }
   return 1;
}

int
vs_intr (sc)
register struct vs_softc *sc;
{
   M328_CRB *crb = (M328_CRB *)&sc->sc_vsreg->sh_CRB;
   struct scsi_xfer *xs;
   M328_CMD *m328_cmd;
   unsigned long loc;
   int status;
   int s;
   s = splbio();
   /* Got a valid interrupt on this device */

   VL(loc, crb->crb_CTAG);
#ifdef SDEBUG
   printf("Interrupt!!! ");
   printf("loc == 0x%x\n", loc);
#endif 
   /*
    * If this is a controller error, there won't be a m328_cmd
    * pointer in the CTAG feild.  Bad things happen if you try 
    * to point to address 0.  Controller error should be handeled
    * in vsdma.c  I'll change this soon - steve.
    */
   if (loc) {
      m328_cmd = (M328_CMD *)loc;
      xs = m328_cmd->xs;
      if (m328_cmd->top_sg_list) {
         vs_dealloc_scatter_gather(m328_cmd->top_sg_list);
         m328_cmd->top_sg_list = (M328_SG)0;
      }
      
      FREE(m328_cmd, M_DEVBUF); /* free the command tag */
      if (vs_checkintr (sc, xs, &status)) {
         vs_scsidone(xs, status);
      }
   }
   splx(s);
}

/*
 * Useful functions for scatter/gather list
 */

M328_SG
vs_alloc_scatter_gather(void)
{
   M328_SG sg;

   MALLOC(sg, M328_SG, sizeof(struct m328_sg), M_DEVBUF, M_WAITOK);
   assert ( sg );
   if ( !sg ) {
      panic ("Memory for scatter_gather_list not available");
   }
   bzero(sg, sizeof(struct m328_sg));

   return (sg);
}

void
vs_dealloc_scatter_gather(M328_SG sg)
{
   register int i;

   if (sg->level > 0) {
      for (i=0; sg->down[i] && i<MAX_SG_ELEMENTS; i++) {
         vs_dealloc_scatter_gather(sg->down[i]);
      }
   }
   FREE(sg, M_DEVBUF);
}

void
vs_link_sg_element(sg_list_element_t * element,
                               register vm_offset_t       phys_add,
                               register int         len)
{
   element->count.bytes = len;
   LV(element->address, phys_add);
   element->link = 0; /* FALSE */
   element->transfer_type = NORMAL_TYPE;
   element->memory_type = LONG_TRANSFER;
   element->address_modifier = 0xD;
}

void
vs_link_sg_list(sg_list_element_t * list,
                            register vm_offset_t    phys_add,
                            register int      elements)
{

   list->count.scatter.gather  = elements;
   LV(list->address, phys_add);
   list->link = 1;    /* TRUE */
   list->transfer_type = NORMAL_TYPE;
   list->memory_type = LONG_TRANSFER;
   list->address_modifier = 0xD;
}


M328_SG 
vs_build_memory_structure(xs, iopb)
struct scsi_xfer *xs;
M328_IOPB  *iopb;              /* the iopb */
{
   M328_SG   sg;
   vm_offset_t starting_point_virt, starting_point_phys, point_virt, 
      point1_phys, point2_phys, virt;
   unsigned len;
   int       level;

   sg = (M328_SG)0;   /* Hopefully we need no scatter/gather list */

   /*
    * We have the following things:
    *	virt			the virtuell address of the contiguous virtual memory block
    *	len			the lenght of the contiguous virtual memory block
    *	starting_point_virt	the virtual address of the contiguous *physical* memory block
    *	starting_point_phys	the *physical* address of the contiguous *physical* memory block
    *	point_virt		the pointer to the virtual memory we are checking at the moment
    *	point1_phys		the pointer to the *physical* memory we are checking at the moment
    *	point2_phys		the pointer to the *physical* memory we are checking at the moment
    */
   
   level = 0;
   virt = starting_point_virt = (vm_offset_t)xs->data;
   point1_phys = starting_point_phys = kvtop(xs->data);
   len = xs->datalen;
   /*
    * Check if we need scatter/gather
    */

   if (len > PAGESIZE) {
      for (level = 0, point_virt = ROUND_PAGE(starting_point_virt+1);
          /* if we do already scatter/gather we have to stay in the loop and jump */
          point_virt < virt + (vm_offset_t)len || sg ;
          point_virt += PAGESIZE) {                      /* out later */

         point2_phys = kvtop(point_virt);

         if ((point2_phys - TRUNC_PAGE(point1_phys) - PAGESIZE) ||                  /* physical memory is not contiguous */
             (point_virt - starting_point_virt >= MAX_SG_BLOCK_SIZE && sg)) {   /* we only can access (1<<16)-1 bytes in scatter/gather_mode */
            if (point_virt - starting_point_virt >= MAX_SG_BLOCK_SIZE) {           /* We were walking too far for one scatter/gather block ... */
               assert( MAX_SG_BLOCK_SIZE > PAGESIZE );
               point_virt = TRUNC_PAGE(starting_point_virt+MAX_SG_BLOCK_SIZE-1);    /* So go back to the beginning of the last matching page */
               /* and gererate the physadress of this location for the next time. */
               point2_phys = kvtop(point_virt);
            }

            if (!sg) {
               /* We allocate our fist scatter/gather list */
               sg = vs_alloc_scatter_gather();
            }
#if 1 /* broken firmware */

            if (sg->elements >= MAX_SG_ELEMENTS) {
               vs_dealloc_scatter_gather(sg);
               return (NULL);
            }

#else /* if the firmware will ever get fixed */
            while (sg->elements >= MAX_SG_ELEMENTS) {
               if (!sg->up) { /* If the list full in this layer ? */
                  sg->up = vs_alloc_scatter_gather();
                  sg->up->level = sg->level+1;
                  sg->up->down[0] = sg;
                  sg->up->elements = 1;
               }
               /* link this full list also in physical memory */
               vs_link_sg_list(&(sg->up->list[sg->up->elements-1]), 
                                           kvtop((vm_offset_t)sg->list),
                                           sg->elements);
               sg = sg->up;      /* Climb up */
            }
            while (sg->level) {  /* As long as we are not a the base level */
               register int i;

               i = sg->elements;
               /* We need a new element */
               sg->down[i] = vs_alloc_scatter_gather();  
               sg->down[i]->level = sg->level - 1;
               sg->down[i]->up = sg;
               sg->elements++;
               sg = sg->down[i]; /* Climb down */
            }
#endif /* 1 */

            if (point_virt < virt+(vm_offset_t)len) {
               /* linking element */
               vs_link_sg_element(&(sg->list[sg->elements]), 
                                  starting_point_phys, 
                                  point_virt-starting_point_virt);
               sg->elements++;
            } else {
               /* linking last element */
               vs_link_sg_element(&(sg->list[sg->elements]), 
                                  starting_point_phys, 
                                  (vm_offset_t)(virt+len)-starting_point_virt);
               sg->elements++;
               break;                         /* We have now collected all blocks */
            }
            starting_point_virt = point_virt;
            starting_point_phys = point2_phys;
         }
         point1_phys = point2_phys;
      }
   }

   /*
    * Climb up along the right side of the tree until we reach the top.
    */

   if (sg) {
      while (sg->up) {
         /* link this list also in physical memory */
         vs_link_sg_list(&(sg->up->list[sg->up->elements-1]), 
                         kvtop((vm_offset_t)sg->list),
                         sg->elements);
         sg = sg->up;                   /* Climb up */
      }

      iopb->iopb_OPTION |= M_OPT_SG;
      iopb->iopb_ADDR |= M_ADR_SG_LINK;
      LV(iopb->iopb_BUFF, kvtop((vm_offset_t)sg->list));
      LV(iopb->iopb_LENGTH, sg->elements);
      LV(iopb->iopb_SGTTL, len);
   } else { 
      /* no scatter/gather neccessary */
      LV(iopb->iopb_BUFF, starting_point_phys);
      LV(iopb->iopb_LENGTH, len);
   }
   return (sg);
}

